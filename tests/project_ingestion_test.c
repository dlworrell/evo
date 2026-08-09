#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "internal/project_ingestion.h"
#include "internal/project_compilation_database.h"
#include "internal/project_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef EVO_TEST_SOURCE_DIR
#define EVO_TEST_SOURCE_DIR "."
#endif

typedef struct test_runner_context {
    evo_project_command_stage_t fail_stage;
    evo_project_command_stage_t timeout_stage;
    size_t calls;
    bool touch_workspace;
    bool invalid_outcome;
    const char *source_mutation_path;
    bool source_mutated;
} test_runner_context_t;

static int test_failures = 0;

static void test_check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project ingestion test failure: %s\n", message);
        test_failures += 1;
    }
}

static bool test_path(
    char *output,
    size_t output_size,
    const char *left,
    const char *right)
{
    const int written = evo_project_format(output, output_size, "%s/%s", left, right);

    return written > 0 && (size_t)written < output_size;
}

static bool test_write_all(int file_fd, const char *bytes, size_t byte_count)
{
    size_t position = 0U;

    while (position < byte_count) {
        const ssize_t written = write(file_fd, bytes + position, byte_count - position);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        position += (size_t)written;
    }
    return true;
}

static bool test_write_text_file(const char *path, const char *text)
{
    const int file_fd = open(
        path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    bool success;

    if (file_fd < 0) {
        return false;
    }
    success = test_write_all(file_fd, text, strlen(text));
    if (success && fsync(file_fd) != 0) {
        success = false;
    }
    if (close(file_fd) != 0) {
        success = false;
    }
    return success;
}

static char *test_read_file(const char *path, size_t *byte_count)
{
    struct stat metadata;
    char *bytes;
    int file_fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    size_t size;
    size_t position = 0U;

    if (file_fd < 0 || fstat(file_fd, &metadata) != 0 ||
        !S_ISREG(metadata.st_mode) || metadata.st_size < 0) {
        if (file_fd >= 0) {
            (void)close(file_fd);
        }
        return NULL;
    }
    size = (size_t)metadata.st_size;
    if (size == SIZE_MAX) {
        (void)close(file_fd);
        return NULL;
    }
    bytes = evo_project_allocate_zeroed(size + 1U, sizeof(*bytes));
    if (bytes == NULL) {
        (void)close(file_fd);
        return NULL;
    }
    while (position < size) {
        const ssize_t read_count = read(file_fd, bytes + position, size - position);

        if (read_count < 0 && errno == EINTR) {
            continue;
        }
        if (read_count <= 0) {
            evo_project_release(bytes);
            (void)close(file_fd);
            return NULL;
        }
        position += (size_t)read_count;
    }
    if (close(file_fd) != 0) {
        evo_project_release(bytes);
        return NULL;
    }
    bytes[size] = '\0';
    *byte_count = size;
    return bytes;
}

static bool test_copy_file_with_mode(
    const char *source_path,
    const char *destination_path,
    mode_t mode)
{
    size_t byte_count = 0U;
    char *bytes = test_read_file(source_path, &byte_count);
    int file_fd;
    bool success;

    if (bytes == NULL) {
        return false;
    }
    file_fd = open(
        destination_path,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        mode);
    if (file_fd < 0) {
        evo_project_release(bytes);
        return false;
    }
    success = fchmod(file_fd, mode) == 0 &&
              test_write_all(file_fd, bytes, byte_count) &&
              fsync(file_fd) == 0;
    if (close(file_fd) != 0) {
        success = false;
    }
    evo_project_release(bytes);
    return success;
}

static bool test_files_equal(const char *left_path, const char *right_path)
{
    size_t left_size = 0U;
    size_t right_size = 0U;
    char *left = test_read_file(left_path, &left_size);
    char *right = test_read_file(right_path, &right_size);
    bool equal = false;
    size_t index;

    if (left != NULL && right != NULL && left_size == right_size) {
        equal = true;
        for (index = 0U; index < left_size; index += 1U) {
            if (left[index] != right[index]) {
                equal = false;
                break;
            }
        }
    }
    evo_project_release(left);
    evo_project_release(right);
    return equal;
}

static bool test_file_contains(const char *path, const char *needle)
{
    size_t byte_count = 0U;
    char *bytes = test_read_file(path, &byte_count);
    const bool contains = bytes != NULL && byte_count > 0U &&
                          strstr(bytes, needle) != NULL;

    evo_project_release(bytes);
    return contains;
}

static bool test_write_replaced_file(
    const char *source_path,
    const char *destination_path,
    const char *needle,
    const char *replacement,
    bool replace_all)
{
    size_t source_size = 0U;
    char *source = test_read_file(source_path, &source_size);
    const size_t needle_size = strlen(needle);
    const size_t replacement_size = strlen(replacement);
    size_t occurrence_count = 0U;
    size_t source_position = 0U;
    size_t output_size;
    char *output;
    size_t output_position = 0U;
    bool success;

    if (source == NULL || needle_size == 0U) {
        evo_project_release(source);
        return false;
    }
    while (source_position <= source_size) {
        const char *match = strstr(source + source_position, needle);

        if (match == NULL) {
            break;
        }
        occurrence_count += 1U;
        source_position = (size_t)(match - source) + needle_size;
        if (!replace_all) {
            break;
        }
    }
    if (occurrence_count == 0U) {
        evo_project_release(source);
        return false;
    }
    if (replacement_size >= needle_size) {
        const size_t growth = replacement_size - needle_size;

        if (growth > 0U && occurrence_count > (SIZE_MAX - source_size) / growth) {
            evo_project_release(source);
            return false;
        }
        output_size = source_size + (occurrence_count * growth);
    } else {
        output_size = source_size -
                      (occurrence_count * (needle_size - replacement_size));
    }
    if (output_size == SIZE_MAX) {
        evo_project_release(source);
        return false;
    }
    output = evo_project_allocate_zeroed(output_size + 1U, sizeof(*output));
    if (output == NULL) {
        evo_project_release(source);
        return false;
    }
    source_position = 0U;
    occurrence_count = 0U;
    while (source_position < source_size) {
        const char *match = strstr(source + source_position, needle);
        size_t copy_end;
        size_t index;

        if (match == NULL || (!replace_all && occurrence_count > 0U)) {
            copy_end = source_size;
        } else {
            copy_end = (size_t)(match - source);
        }
        for (index = source_position; index < copy_end; index += 1U) {
            output[output_position] = source[index];
            output_position += 1U;
        }
        if (copy_end == source_size) {
            break;
        }
        for (index = 0U; index < replacement_size; index += 1U) {
            output[output_position] = replacement[index];
            output_position += 1U;
        }
        occurrence_count += 1U;
        source_position = copy_end + needle_size;
    }
    output[output_position] = '\0';
    success = output_position == output_size &&
              test_write_text_file(destination_path, output);
    evo_project_release(output);
    evo_project_release(source);
    return success;
}

static evo_project_status_t test_command_runner(
    const evo_project_command_view_t *command,
    const char *workspace_path,
    void *context_value,
    evo_project_command_outcome_t *outcome)
{
    test_runner_context_t *context = context_value;
    char touch_path[1024];
    int touch_fd;
    const char touch[] = "derived workspace only\n";

    if (command == NULL || workspace_path == NULL || context == NULL ||
        outcome == NULL || command->argument_count == 0U ||
        command->arguments == NULL || command->network_access ||
        command->timeout_ms == 0U || command->max_memory_bytes == 0U ||
        command->max_processes == 0U || command->max_storage_bytes == 0U ||
        command->max_output_bytes == 0U) {
        return EVO_PROJECT_ERROR_EXECUTION_PROVIDER;
    }
    context->calls += 1U;
    if (context->source_mutation_path != NULL && !context->source_mutated &&
        command->stage == EVO_PROJECT_COMMAND_CONFIGURE) {
        const char replacement = 'Z';
        int source_fd = open(
            context->source_mutation_path,
            O_WRONLY | O_NOFOLLOW | O_CLOEXEC);
        bool mutation_ok = source_fd >= 0;

        if (mutation_ok &&
            (!test_write_all(source_fd, &replacement, 1U) ||
             fsync(source_fd) != 0)) {
            mutation_ok = false;
        }
        if (source_fd >= 0 && close(source_fd) != 0) {
            mutation_ok = false;
        }
        if (!mutation_ok) {
            return EVO_PROJECT_ERROR_EXECUTION_PROVIDER;
        }
        context->source_mutated = true;
    }
    if (context->touch_workspace &&
        command->stage == EVO_PROJECT_COMMAND_CONFIGURE) {
        if (!test_path(
                touch_path,
                sizeof(touch_path),
                workspace_path,
                ".provider-touch")) {
            return EVO_PROJECT_ERROR_EXECUTION_PROVIDER;
        }
        touch_fd = open(
            touch_path,
            O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
            0600);
        if (touch_fd < 0) {
            return EVO_PROJECT_ERROR_EXECUTION_PROVIDER;
        }
        {
            bool touch_ok =
                test_write_all(touch_fd, touch, sizeof(touch) - 1U);

            if (close(touch_fd) != 0) {
                touch_ok = false;
            }
            if (!touch_ok) {
                return EVO_PROJECT_ERROR_EXECUTION_PROVIDER;
            }
        }
    }
    outcome->schema_version = EVO_PROJECT_BASELINE_SCHEMA_VERSION;
    outcome->completed = true;
    outcome->timed_out = command->stage == context->timeout_stage;
    outcome->exit_code =
        command->stage == context->fail_stage || outcome->timed_out ? 7 : 0;
    outcome->output_bytes = 16U + (size_t)command->stage;
    outcome->output_fingerprint =
        UINT64_C(0xabc00000) + (uint64_t)command->stage;
    if (context->invalid_outcome) {
        outcome->completed = false;
    }
    return EVO_PROJECT_SUCCESS;
}

static evo_project_ingest_limits_t test_limits(void)
{
    evo_project_ingest_limits_t limits;

    limits.max_manifest_bytes = 131072U;
    limits.max_json_tokens = 2048U;
    limits.max_json_depth = 16U;
    limits.max_string_bytes = 1024U;
    limits.max_path_bytes = 1024U;
    limits.max_files = 128U;
    limits.max_file_bytes = 131072U;
    limits.max_total_bytes = 1048576U;
    limits.max_compilation_database_bytes = 131072U;
    limits.max_permitted_roots = 32U;
    limits.max_dependencies = 32U;
    limits.max_toolchains = 32U;
    limits.max_environment_entries = 32U;
    limits.max_targets = 32U;
    limits.max_workloads = 32U;
    limits.max_constraints = 32U;
    limits.max_command_args = 32U;
    limits.max_command_bytes = 8192U;
    limits.max_command_output_bytes = 65536U;
    limits.max_evidence_bytes = 524288U;
    limits.max_command_timeout_ms = 60000U;
    limits.max_memory_bytes = UINT64_C(536870912);
    limits.max_processes = 16U;
    limits.max_storage_bytes = UINT64_C(2097152);
    return limits;
}

static evo_project_capture_config_t test_config(
    const char *manifest,
    const char *project,
    const char *output,
    test_runner_context_t *runner)
{
    evo_project_capture_config_t config;

    config.manifest_path = manifest;
    config.authorized_project_root = project;
    config.output_path = output;
    config.execution_provider_identity = "evo-test-execution-provider-v1";
    config.limits = test_limits();
    config.command_runner = test_command_runner;
    config.command_runner_context = runner;
    return config;
}

static void test_expected_file_order(
    const evo_project_baseline_t *baseline,
    const char *const *expected,
    size_t expected_count)
{
    size_t index;

    test_check(baseline->file_count == expected_count, "file count");
    if (baseline->file_count != expected_count) {
        return;
    }
    for (index = 0U; index < expected_count; index += 1U) {
        test_check(
            strcmp(baseline->files[index].path, expected[index]) == 0,
            "stable UTF-8 path order");
        test_check(
            baseline->files[index].content_fingerprint != 0U,
            "file diagnostic fingerprint");
    }
}

static void test_expected_compilation_units(
    const evo_project_baseline_t *baseline)
{
    static const char *const expected_files[] = {
        "src/value.c", "tests/value_test.c"};
    size_t index;

    test_check(
        baseline->compilation_unit_count == 2U,
        "normalized compilation-unit count");
    if (baseline->compilation_unit_count != 2U) {
        return;
    }
    for (index = 0U; index < 2U; index += 1U) {
        test_check(
            strcmp(
                baseline->compilation_units[index].file,
                expected_files[index]) == 0,
            "stable compilation-unit source order");
        test_check(
            strcmp(baseline->compilation_units[index].directory, ".") == 0,
            "normalized compilation-unit directory");
        test_check(
            baseline->compilation_units[index].output == NULL,
            "absent compilation-unit output");
        test_check(
            baseline->compilation_units[index].command_form ==
                EVO_PROJECT_COMPILE_COMMAND_ARGUMENTS,
            "explicit compilation-unit argv form");
        test_check(
            baseline->compilation_units[index].argument_count > 0U,
            "nonempty compilation-unit argv");
    }
    test_check(
        strcmp(
            baseline->normalized_build_fingerprint,
            "fnv1a64-v1:68c5c9e07e0f8c2c") == 0,
        "normalized build golden fingerprint");
}

static void test_compilation_database_canonicalization(
    const evo_project_baseline_t *baseline,
    const char *project_root)
{
    static const char relative_database[] =
        "[{\"directory\":\".\",\"file\":\"tests/value_test.c\","
        "\"command\":\"cc tests/value_test.c src/value.c -o value_test\"},"
        "{\"directory\":\".\",\"file\":\"src/value.c\","
        "\"output\":\"build/value.o\","
        "\"arguments\":[\"cc\",\"-c\",\"src/value.c\"]}]";
    static const char malformed_database[] =
        "[{\"directory\":\".\",\"file\":\"src/value.c\","
        "\"arguments\":[\"cc\"],\"command\":\"cc\"}]";
    char absolute_database[4096];
    const int written = evo_project_format(
        absolute_database,
        sizeof(absolute_database),
        "[ { \"file\": \"%s/src/value.c\", \"directory\": \"%s\", "
        "\"arguments\": [\"cc\", \"-c\", \"src/value.c\"], "
        "\"output\": \"build/value.o\" }, "
        "{ \"command\": \"cc tests/value_test.c src/value.c -o value_test\", "
        "\"file\": \"%s/tests/value_test.c\", \"directory\": \"%s\" } ]",
        project_root,
        project_root,
        project_root,
        project_root);
    evo_project_compilation_record_t *relative_records = NULL;
    evo_project_compilation_record_t *absolute_records = NULL;
    evo_project_compilation_record_t *malformed_records = NULL;
    size_t relative_count = 0U;
    size_t absolute_count = 0U;
    size_t malformed_count = 0U;
    uint64_t relative_fingerprint = 0U;
    uint64_t absolute_fingerprint = 0U;
    uint64_t malformed_fingerprint = 0U;
    evo_project_status_t status;
    evo_project_ingest_limits_t limits = test_limits();

    test_check(
        written > 0 && (size_t)written < sizeof(absolute_database),
        "absolute compilation-database fixture");
    if (written <= 0 || (size_t)written >= sizeof(absolute_database)) {
        return;
    }
    status = evo_project_compilation_database_parse(
        relative_database,
        sizeof(relative_database) - 1U,
        project_root,
        &limits,
        baseline->files,
        baseline->file_count,
        &relative_records,
        &relative_count,
        &relative_fingerprint);
    test_check(status == EVO_PROJECT_SUCCESS, "relative compilation database");
    status = evo_project_compilation_database_parse(
        absolute_database,
        (size_t)written,
        project_root,
        &limits,
        baseline->files,
        baseline->file_count,
        &absolute_records,
        &absolute_count,
        &absolute_fingerprint);
    test_check(status == EVO_PROJECT_SUCCESS, "absolute compilation database");
    test_check(
        relative_count == absolute_count &&
            relative_fingerprint == absolute_fingerprint,
        "equivalent databases have one normalized identity");
    if (relative_count == 2U && absolute_count == 2U) {
        test_check(
            strcmp(relative_records[0].file, absolute_records[0].file) == 0 &&
                strcmp(relative_records[1].file, absolute_records[1].file) == 0,
            "equivalent databases have one ordered description");
    }
    status = evo_project_compilation_database_parse(
        malformed_database,
        sizeof(malformed_database) - 1U,
        project_root,
        &limits,
        baseline->files,
        baseline->file_count,
        &malformed_records,
        &malformed_count,
        &malformed_fingerprint);
    test_check(
        status == EVO_PROJECT_ERROR_MANIFEST_INVALID,
        "ambiguous compilation command rejected");
    evo_project_compilation_database_destroy(
        absolute_records, absolute_count);
    evo_project_compilation_database_destroy(
        malformed_records, malformed_count);
    evo_project_compilation_database_destroy(
        relative_records, relative_count);
}

static bool test_stage_replay_fixture(
    const char *temporary_root,
    const char *fixture_name,
    const char *alternate_manifest,
    const char *const *expected_files,
    size_t expected_file_count,
    char *fixture_root,
    size_t fixture_root_size,
    char *project_root,
    size_t project_root_size,
    char *manifest_path,
    size_t manifest_path_size,
    char *alternate_path,
    size_t alternate_path_size)
{
    static const char *const directories[] = {"include", "src", "tests"};
    char source_fixture_root[1024];
    char source_project_root[1024];
    char source_path[1024];
    char destination_path[1024];
    char staged_name[128];
    size_t index;

    if (evo_project_format(
            staged_name,
            sizeof(staged_name),
            "%s-staged-fixture",
            fixture_name) <= 0 ||
        !test_path(
            source_fixture_root,
            sizeof(source_fixture_root),
            EVO_TEST_SOURCE_DIR "/tests/fixtures/project-ingestion",
            fixture_name) ||
        !test_path(
            source_project_root,
            sizeof(source_project_root),
            source_fixture_root,
            "project") ||
        !test_path(
            fixture_root,
            fixture_root_size,
            temporary_root,
            staged_name) ||
        mkdir(fixture_root, 0700) != 0 ||
        !test_path(
            project_root,
            project_root_size,
            fixture_root,
            "project") ||
        mkdir(project_root, 0700) != 0) {
        return false;
    }
    for (index = 0U;
         index < sizeof(directories) / sizeof(directories[0]);
         index += 1U) {
        if (!test_path(
                destination_path,
                sizeof(destination_path),
                project_root,
                directories[index]) ||
            mkdir(destination_path, 0700) != 0) {
            return false;
        }
    }
    if (!test_path(
            source_path,
            sizeof(source_path),
            source_fixture_root,
            "manifest.json") ||
        !test_path(
            manifest_path,
            manifest_path_size,
            fixture_root,
            "manifest.json") ||
        !test_copy_file_with_mode(source_path, manifest_path, 0644)) {
        return false;
    }
    if (strcmp(alternate_manifest, "manifest.json") == 0) {
        if (!test_path(
                alternate_path,
                alternate_path_size,
                fixture_root,
                "manifest.json")) {
            return false;
        }
    } else {
        if (!test_path(
                source_path,
                sizeof(source_path),
                source_fixture_root,
                alternate_manifest) ||
            !test_path(
                alternate_path,
                alternate_path_size,
                fixture_root,
                alternate_manifest) ||
            !test_copy_file_with_mode(source_path, alternate_path, 0644)) {
            return false;
        }
    }
    for (index = 0U; index < expected_file_count; index += 1U) {
        if (!test_path(
                source_path,
                sizeof(source_path),
                source_project_root,
                expected_files[index]) ||
            !test_path(
                destination_path,
                sizeof(destination_path),
                project_root,
                expected_files[index]) ||
            !test_copy_file_with_mode(source_path, destination_path, 0644)) {
            return false;
        }
    }
    return true;
}

static void test_successful_replay(
    const char *temporary_root,
    const char *fixture_name,
    const char *alternate_manifest,
    const char *const *expected_files,
    size_t expected_file_count,
    const char *expected_frontend,
    const char *expected_manifest_fingerprint,
    const char *expected_baseline_fingerprint)
{
    char fixture_root[1024];
    char project_root[1024];
    char manifest_path[1024];
    char alternate_path[1024];
    char output_one[1024];
    char output_two[1024];
    char output_three[1024];
    char evidence_one[1024];
    char evidence_markdown[1024];
    char evidence_two[1024];
    char evidence_three[1024];
    char touch_path[1024];
    char source_file[1024];
    char snapshot_root[1024];
    char snapshot_file[1024];
    char output_name[128];
    struct stat metadata;
    size_t file_index;
    test_runner_context_t runner_one = {0};
    test_runner_context_t runner_two = {0};
    test_runner_context_t runner_three = {0};
    evo_project_baseline_t baseline_one = {0};
    evo_project_baseline_t baseline_two = {0};
    evo_project_baseline_t baseline_three = {0};
    evo_project_capture_config_t config;
    evo_project_status_t status;
    bool paths_ready;

    paths_ready = test_stage_replay_fixture(
                      temporary_root,
                      fixture_name,
                      alternate_manifest,
                      expected_files,
                      expected_file_count,
                      fixture_root,
                      sizeof(fixture_root),
                      project_root,
                      sizeof(project_root),
                      manifest_path,
                      sizeof(manifest_path),
                      alternate_path,
                      sizeof(alternate_path)) &&
                  evo_project_format(
                      output_name,
                      sizeof(output_name),
                      "%s-replay-one",
                      fixture_name) > 0 &&
                  test_path(
                      output_one,
                      sizeof(output_one),
                      temporary_root,
                      output_name) &&
                  evo_project_format(
                      output_name,
                      sizeof(output_name),
                      "%s-replay-two",
                      fixture_name) > 0 &&
                  test_path(
                      output_two,
                      sizeof(output_two),
                      temporary_root,
                      output_name) &&
                  evo_project_format(
                      output_name,
                      sizeof(output_name),
                      "%s-replay-three",
                      fixture_name) > 0 &&
                  test_path(
                      output_three,
                      sizeof(output_three),
                      temporary_root,
                      output_name);
    test_check(paths_ready, "fixture paths");
    if (!paths_ready) {
        return;
    }
    runner_one.touch_workspace = true;
    config = test_config(manifest_path, project_root, output_one, &runner_one);
    status = evo_project_capture_baseline(&config, &baseline_one);
    if (status != EVO_PROJECT_SUCCESS) {
        (void)fprintf(
            stderr,
            "first capture status for %s: %s\n",
            fixture_name,
            evo_project_status_name(status));
    }
    test_check(status == EVO_PROJECT_SUCCESS, "first capture succeeds");
    if (status != EVO_PROJECT_SUCCESS) {
        return;
    }
    test_check(
        baseline_one.schema_version == EVO_PROJECT_BASELINE_SCHEMA_VERSION,
        "baseline schema version");
    test_check(
        baseline_one.state == EVO_PROJECT_BASELINE_ELIGIBLE,
        "eligible baseline state");
    test_check(
        strcmp(baseline_one.build_frontend, expected_frontend) == 0,
        "frontend evidence");
    test_check(runner_one.calls == 4U, "all baseline gates run");
    test_check(baseline_one.command_count == 4U, "complete command trace");
    test_check(baseline_one.projection_complete, "projection complete");
    test_check(
        !baseline_one.probabilistic_authority,
        "no probabilistic authority");
    test_check(
        strncmp(baseline_one.manifest_fingerprint, "fnv1a64-v1:", 11U) == 0 &&
            strncmp(baseline_one.baseline_fingerprint, "fnv1a64-v1:", 11U) == 0,
        "versioned diagnostic fingerprints");
    test_check(
        strcmp(
            baseline_one.manifest_fingerprint,
            expected_manifest_fingerprint) == 0,
        "manifest golden fingerprint");
    test_check(
        strcmp(
            baseline_one.baseline_fingerprint,
            expected_baseline_fingerprint) == 0,
        "baseline golden fingerprint");
    test_expected_file_order(
        &baseline_one, expected_files, expected_file_count);
    for (file_index = 0U; file_index < expected_file_count;
         file_index += 1U) {
        test_check(
            test_path(
                source_file,
                sizeof(source_file),
                project_root,
                expected_files[file_index]) &&
                test_path(
                    snapshot_root,
                    sizeof(snapshot_root),
                    output_one,
                    "snapshot") &&
                test_path(
                    snapshot_file,
                    sizeof(snapshot_file),
                    snapshot_root,
                    expected_files[file_index]) &&
                test_files_equal(source_file, snapshot_file),
            "snapshot file is byte-identical to source");
        test_check(
            stat(snapshot_file, &metadata) == 0 &&
                (metadata.st_mode & (mode_t)0222) == 0,
            "snapshot file is read-only");
    }
    test_expected_compilation_units(&baseline_one);
    test_compilation_database_canonicalization(&baseline_one, project_root);
    test_check(
        test_path(evidence_one, sizeof(evidence_one), output_one, "baseline.json") &&
            access(evidence_one, R_OK) == 0,
        "canonical JSON evidence retained");
    test_check(
        test_file_contains(evidence_one, "\"generated_sources\":[]") &&
            test_file_contains(evidence_one, "\"benchmark_required\":true") &&
            test_file_contains(evidence_one, "\"compilation_units\":["),
        "canonical JSON exposes complete audit registries");
    test_check(
        test_path(
            evidence_markdown,
            sizeof(evidence_markdown),
            output_one,
            "baseline.md") &&
            test_file_contains(
                evidence_markdown, "Generated-source policy: `reject`") &&
            test_file_contains(evidence_markdown, "| Stage | Invocation |") &&
            test_file_contains(evidence_markdown, "### Dependencies") &&
            test_file_contains(evidence_markdown, "### Toolchains") &&
            test_file_contains(evidence_markdown, "### Environment") &&
            test_file_contains(evidence_markdown, "### Search") &&
            test_file_contains(evidence_markdown, "### Resource Budgets") &&
            test_file_contains(evidence_markdown, "### Artifact Policy"),
        "human-readable evidence projects complete policy and gate input");
    test_check(
        test_path(touch_path, sizeof(touch_path), output_one, "snapshot/.provider-touch") &&
            access(touch_path, F_OK) != 0,
        "derived workspace output excluded from snapshot");
    test_check(
        test_path(touch_path, sizeof(touch_path), output_one, ".evo-incomplete-v1") &&
            access(touch_path, F_OK) != 0,
        "completion marker removed");
    test_check(
        test_path(touch_path, sizeof(touch_path), output_one, "snapshot") &&
            stat(touch_path, &metadata) == 0 &&
            (metadata.st_mode & (mode_t)0222) == 0,
        "snapshot directory is read-only");

    config = test_config(manifest_path, project_root, output_two, &runner_two);
    status = evo_project_capture_baseline(&config, &baseline_two);
    test_check(status == EVO_PROJECT_SUCCESS, "second capture succeeds");
    if (status == EVO_PROJECT_SUCCESS) {
        test_check(
            strcmp(
                baseline_one.manifest_fingerprint,
                baseline_two.manifest_fingerprint) == 0,
            "manifest replay identity");
        test_check(
            strcmp(
                baseline_one.baseline_fingerprint,
                baseline_two.baseline_fingerprint) == 0,
            "baseline replay identity");
        test_check(
            test_path(evidence_two, sizeof(evidence_two), output_two, "baseline.json") &&
                test_files_equal(evidence_one, evidence_two),
            "byte-identical replay evidence");
    }

    if (strcmp(alternate_manifest, "manifest.json") != 0) {
        config = test_config(
            alternate_path, project_root, output_three, &runner_three);
        status = evo_project_capture_baseline(&config, &baseline_three);
        test_check(status == EVO_PROJECT_SUCCESS, "reordered manifest succeeds");
        if (status == EVO_PROJECT_SUCCESS) {
            test_check(
                strcmp(
                    baseline_one.manifest_fingerprint,
                    baseline_three.manifest_fingerprint) == 0,
                "semantic manifest canonicalization");
            test_check(
                strcmp(
                    baseline_one.baseline_fingerprint,
                    baseline_three.baseline_fingerprint) == 0,
                "semantic baseline canonicalization");
            test_check(
                test_path(
                    evidence_three,
                    sizeof(evidence_three),
                    output_three,
                    "baseline.json") &&
                    test_files_equal(evidence_one, evidence_three),
                "reordered manifest yields exact evidence");
        }
    }

    config = test_config(manifest_path, project_root, output_one, &runner_one);
    status = evo_project_capture_baseline(&config, &baseline_one);
    test_check(
        status == EVO_PROJECT_ERROR_RESULT_ACTIVE,
        "active result rejected before output work");
    evo_project_baseline_destroy(&baseline_three);
    evo_project_baseline_destroy(&baseline_two);
    evo_project_baseline_destroy(&baseline_one);
}

static void test_gate_failure(
    const char *temporary_root,
    evo_project_command_stage_t fail_stage,
    bool timed_out,
    evo_project_baseline_state_t expected_state,
    size_t expected_calls,
    const char *output_name)
{
    char fixture_root[1024];
    char project_root[1024];
    char manifest_path[1024];
    char output_path[1024];
    test_runner_context_t runner = {0};
    evo_project_baseline_t baseline = {0};
    evo_project_capture_config_t config;
    evo_project_status_t status;

    test_check(
        test_path(
            fixture_root,
            sizeof(fixture_root),
            EVO_TEST_SOURCE_DIR "/tests/fixtures/project-ingestion",
            "cmake") &&
            test_path(project_root, sizeof(project_root), fixture_root, "project") &&
            test_path(manifest_path, sizeof(manifest_path), fixture_root, "manifest.json") &&
            test_path(output_path, sizeof(output_path), temporary_root, output_name),
        "gate failure paths");
    if (timed_out) {
        runner.timeout_stage = fail_stage;
    } else {
        runner.fail_stage = fail_stage;
    }
    config = test_config(manifest_path, project_root, output_path, &runner);
    status = evo_project_capture_baseline(&config, &baseline);
    test_check(status == EVO_PROJECT_SUCCESS, "gate failure is retained evidence");
    if (status == EVO_PROJECT_SUCCESS) {
        test_check(baseline.state == expected_state, "classified baseline failure");
        test_check(runner.calls == expected_calls, "later gates suppressed");
        test_check(
            baseline.commands[expected_calls - 1U].disposition ==
                (timed_out ? EVO_PROJECT_COMMAND_TIMED_OUT
                           : EVO_PROJECT_COMMAND_FAILED),
            "failed gate disposition");
        if (expected_calls < EVO_PROJECT_COMMAND_COUNT) {
            test_check(
                baseline.commands[expected_calls].disposition ==
                    EVO_PROJECT_COMMAND_NOT_RUN,
                "suppressed gate evidence");
        }
    }
    evo_project_baseline_destroy(&baseline);
}

static void test_optional_benchmark(const char *temporary_root)
{
    char fixture_root[1024];
    char project_root[1024];
    char manifest_path[1024];
    char optional_manifest[1024];
    char output_path[1024];
    char json_path[1024];
    char markdown_path[1024];
    test_runner_context_t runner = {0};
    evo_project_baseline_t baseline = {0};
    evo_project_capture_config_t config;
    evo_project_status_t status;

    test_check(
        test_path(
            fixture_root,
            sizeof(fixture_root),
            EVO_TEST_SOURCE_DIR "/tests/fixtures/project-ingestion",
            "cmake") &&
            test_path(project_root, sizeof(project_root), fixture_root, "project") &&
            test_path(manifest_path, sizeof(manifest_path), fixture_root, "manifest.json") &&
            test_path(
                optional_manifest,
                sizeof(optional_manifest),
                temporary_root,
                "optional-benchmark.json") &&
            test_path(
                output_path,
                sizeof(output_path),
                temporary_root,
                "optional-benchmark-output"),
        "optional benchmark paths");
    test_check(
        test_write_replaced_file(
            manifest_path,
            optional_manifest,
            "\"benchmark_required\": true",
            "\"benchmark_required\": false",
            false),
        "write optional benchmark manifest");
    config = test_config(
        optional_manifest, project_root, output_path, &runner);
    status = evo_project_capture_baseline(&config, &baseline);
    test_check(status == EVO_PROJECT_SUCCESS, "optional benchmark capture succeeds");
    if (status == EVO_PROJECT_SUCCESS) {
        test_check(
            baseline.state == EVO_PROJECT_BASELINE_ELIGIBLE,
            "optional benchmark remains eligible");
        test_check(runner.calls == 3U, "optional benchmark gate is skipped");
        test_check(
            baseline.commands[3].disposition == EVO_PROJECT_COMMAND_NOT_RUN,
            "optional benchmark has explicit not-run evidence");
        test_check(
            test_path(json_path, sizeof(json_path), output_path, "baseline.json") &&
                test_file_contains(json_path, "\"benchmark_required\":false"),
            "JSON retains optional benchmark policy");
        test_check(
            test_path(
                markdown_path,
                sizeof(markdown_path),
                output_path,
                "baseline.md") &&
                test_file_contains(
                    markdown_path, "Benchmark required: no"),
            "Markdown explains optional benchmark policy");
    }
    evo_project_baseline_destroy(&baseline);
}

static void test_rejected_before_commands(
    const char *temporary_root,
    const char *project_root,
    const char *manifest_path,
    const char *output_name,
    evo_project_ingest_limits_t limits,
    evo_project_status_t expected_status,
    const char *message)
{
    char output_path[1024];
    test_runner_context_t runner = {0};
    evo_project_baseline_t baseline = {0};
    evo_project_capture_config_t config;
    evo_project_status_t status;

    test_check(
        test_path(
            output_path,
            sizeof(output_path),
            temporary_root,
            output_name),
        "rejection output path");
    config = test_config(manifest_path, project_root, output_path, &runner);
    config.limits = limits;
    status = evo_project_capture_baseline(&config, &baseline);
    if (status != expected_status) {
        (void)fprintf(
            stderr,
            "project ingestion rejection mismatch (%s): expected %s, got %s\n",
            message,
            evo_project_status_name(expected_status),
            evo_project_status_name(status));
    }
    test_check(status == expected_status, message);
    test_check(runner.calls == 0U, "rejected input invokes no command");
    test_check(access(output_path, F_OK) != 0, "rejected output is absent");
    test_check(
        baseline.private_owner == NULL && baseline.schema_version == 0U,
        "rejected result remains empty");
    evo_project_baseline_destroy(&baseline);
}

static void test_preflight_failures(const char *temporary_root)
{
    char fixture_root[1024];
    char project_root[1024];
    char manifest_path[1024];
    char malformed_path[1024];
    char missing_path[1024];
    char bad_database_path[1024];
    char out_of_root_path[1024];
    char overlapping_path[1024];
    char duplicate_path[1024];
    char output_path[1024];
    char inside_output[1024];
    test_runner_context_t runner = {0};
    evo_project_baseline_t baseline = {0};
    evo_project_capture_config_t config;
    evo_project_status_t status;
    evo_project_ingest_limits_t limits;

    test_check(
        test_path(
            fixture_root,
            sizeof(fixture_root),
            EVO_TEST_SOURCE_DIR "/tests/fixtures/project-ingestion",
            "cmake") &&
            test_path(project_root, sizeof(project_root), fixture_root, "project") &&
            test_path(manifest_path, sizeof(manifest_path), fixture_root, "manifest.json") &&
            test_path(malformed_path, sizeof(malformed_path), temporary_root, "malformed.json") &&
            test_path(missing_path, sizeof(missing_path), temporary_root, "missing.json") &&
            test_path(bad_database_path, sizeof(bad_database_path), temporary_root, "bad-database.json") &&
            test_path(out_of_root_path, sizeof(out_of_root_path), temporary_root, "out-of-root.json") &&
            test_path(overlapping_path, sizeof(overlapping_path), temporary_root, "overlapping.json") &&
            test_path(duplicate_path, sizeof(duplicate_path), temporary_root, "duplicate.json") &&
            test_path(output_path, sizeof(output_path), temporary_root, "malformed-output") &&
            test_path(inside_output, sizeof(inside_output), project_root, "forbidden-output"),
        "preflight paths");
    test_check(
        test_write_text_file(malformed_path, "{\"schema\":true}"),
        "write malformed manifest");
    config = test_config(malformed_path, project_root, output_path, &runner);
    status = evo_project_capture_baseline(&config, &baseline);
    test_check(
        status == EVO_PROJECT_ERROR_MANIFEST_INVALID,
        "malformed manifest rejected");
    test_check(runner.calls == 0U, "malformed manifest invokes no command");
    test_check(access(output_path, F_OK) != 0, "malformed output absent");

    test_check(
        test_write_replaced_file(
            manifest_path,
            missing_path,
            "compile_commands.json",
            "missing_commands.json",
            true),
        "write missing-input manifest");
    test_check(
        test_write_replaced_file(
            manifest_path,
            bad_database_path,
            "compile_commands.json",
            "bad_compile_commands.json",
            true),
        "write ambiguous-database manifest");
    test_check(
        test_write_replaced_file(
            manifest_path,
            out_of_root_path,
            "\"compilation_database\": \"compile_commands.json\"",
            "\"compilation_database\": \"../compile_commands.json\"",
            false),
        "write out-of-root manifest");
    test_check(
        test_write_replaced_file(
            manifest_path,
            overlapping_path,
            "\"CMakeLists.txt\"",
            "\"include/value.h\"",
            false),
        "write overlapping-root manifest");
    test_check(
        test_write_replaced_file(
            manifest_path,
            duplicate_path,
            "\"manifest_id\": \"fixture-cmake-v1\",",
            "\"manifest_id\": \"fixture-cmake-v1\",\n  \"manifest_id\": \"duplicate\",",
            false),
        "write duplicate-field manifest");

    limits = test_limits();
    test_rejected_before_commands(
        temporary_root,
        project_root,
        missing_path,
        "missing-input-output",
        limits,
        EVO_PROJECT_ERROR_PATH_INVALID,
        "missing declared input rejected");
    test_rejected_before_commands(
        temporary_root,
        project_root,
        bad_database_path,
        "ambiguous-database-output",
        limits,
        EVO_PROJECT_ERROR_MANIFEST_INVALID,
        "ambiguous compilation database rejected");
    test_rejected_before_commands(
        temporary_root,
        project_root,
        out_of_root_path,
        "out-of-root-output",
        limits,
        EVO_PROJECT_ERROR_PATH_INVALID,
        "out-of-root input rejected");
    test_rejected_before_commands(
        temporary_root,
        project_root,
        overlapping_path,
        "overlapping-output",
        limits,
        EVO_PROJECT_ERROR_MANIFEST_INVALID,
        "ambiguous overlapping roots rejected");
    test_rejected_before_commands(
        temporary_root,
        project_root,
        duplicate_path,
        "duplicate-output",
        limits,
        EVO_PROJECT_ERROR_MANIFEST_INVALID,
        "duplicate manifest field rejected");

    config = test_config(manifest_path, project_root, inside_output, &runner);
    status = evo_project_capture_baseline(&config, &baseline);
    test_check(
        status == EVO_PROJECT_ERROR_PATH_INVALID,
        "output inside input root rejected");
    test_check(runner.calls == 0U, "inside output invokes no command");
    test_check(access(inside_output, F_OK) != 0, "input root remains unchanged");

    config = test_config(manifest_path, project_root, output_path, &runner);
    config.limits.max_manifest_bytes = 16U;
    status = evo_project_capture_baseline(&config, &baseline);
    test_check(
        status == EVO_PROJECT_ERROR_RESOURCE_LIMIT,
        "manifest byte limit enforced");
    test_check(runner.calls == 0U, "resource preflight invokes no command");
    limits = test_limits();
    limits.max_files = 4U;
    test_rejected_before_commands(
        temporary_root,
        project_root,
        manifest_path,
        "file-count-output",
        limits,
        EVO_PROJECT_ERROR_RESOURCE_LIMIT,
        "file-count budget enforced");
    limits = test_limits();
    limits.max_file_bytes = 64U;
    test_rejected_before_commands(
        temporary_root,
        project_root,
        manifest_path,
        "file-size-output",
        limits,
        EVO_PROJECT_ERROR_RESOURCE_LIMIT,
        "file-size budget enforced");
    limits = test_limits();
    limits.max_compilation_database_bytes = 128U;
    test_rejected_before_commands(
        temporary_root,
        project_root,
        manifest_path,
        "database-size-output",
        limits,
        EVO_PROJECT_ERROR_RESOURCE_LIMIT,
        "compilation-database budget enforced");
    evo_project_baseline_destroy(&baseline);
}

static void test_integrity_and_provider_failures(const char *temporary_root)
{
    static const char compilation_database[] =
        "[{\"directory\":\".\",\"file\":\"unit.c\","
        "\"arguments\":[\"cc\",\"-std=c17\",\"-c\",\"unit.c\"]}]";
    char fixture_root[1024];
    char fixture_manifest[1024];
    char project_root[1024];
    char manifest_path[1024];
    char symlink_manifest[1024];
    char source_path[1024];
    char database_path[1024];
    char link_path[1024];
    char output_path[1024];
    test_runner_context_t runner = {0};
    evo_project_baseline_t baseline = {0};
    evo_project_capture_config_t config;
    evo_project_status_t status;

    test_check(
        test_path(
            fixture_root,
            sizeof(fixture_root),
            EVO_TEST_SOURCE_DIR "/tests/fixtures/project-ingestion",
            "cmake") &&
            test_path(
                fixture_manifest,
                sizeof(fixture_manifest),
                fixture_root,
                "manifest.json") &&
            test_path(
                project_root,
                sizeof(project_root),
                temporary_root,
                "dynamic-project") &&
            test_path(
                manifest_path,
                sizeof(manifest_path),
                temporary_root,
                "dynamic-manifest.json") &&
            test_path(
                symlink_manifest,
                sizeof(symlink_manifest),
                temporary_root,
                "symlink-manifest.json") &&
            test_path(
                source_path,
                sizeof(source_path),
                project_root,
                "unit.c") &&
            test_path(
                database_path,
                sizeof(database_path),
                project_root,
                "compile_commands.json") &&
            test_path(link_path, sizeof(link_path), project_root, "linked.c"),
        "integrity fixture paths");
    test_check(mkdir(project_root, 0700) == 0, "create dynamic project");
    test_check(
        test_write_text_file(source_path, "int unit_value(void) { return 7; }\n"),
        "write dynamic source");
    test_check(
        test_write_text_file(database_path, compilation_database),
        "write dynamic compilation database");
    test_check(
        symlink("unit.c", link_path) == 0,
        "create adversarial source symlink");
    test_check(
        test_write_replaced_file(
            fixture_manifest,
            manifest_path,
            "\"CMakeLists.txt\",\n      \"compile_commands.json\",\n      \"include\",\n      \"src\",\n      \"tests\"",
            "\"compile_commands.json\",\n      \"unit.c\"",
            false),
        "write dynamic manifest");
    test_check(
        test_write_replaced_file(
            manifest_path,
            symlink_manifest,
            "\"unit.c\"",
            "\"linked.c\"",
            true),
        "write symlink manifest");

    test_rejected_before_commands(
        temporary_root,
        project_root,
        symlink_manifest,
        "symlink-output",
        test_limits(),
        EVO_PROJECT_ERROR_PATH_INVALID,
        "source symlink rejected");

    test_check(
        test_path(
            output_path,
            sizeof(output_path),
            temporary_root,
            "source-change-output"),
        "source-change output path");
    runner.source_mutation_path = source_path;
    config = test_config(manifest_path, project_root, output_path, &runner);
    status = evo_project_capture_baseline(&config, &baseline);
    test_check(
        status == EVO_PROJECT_ERROR_SOURCE_CHANGED,
        "concurrent source mutation rejected");
    test_check(runner.source_mutated, "source mutation exercised");
    test_check(runner.calls == 4U, "source checked after baseline gates");
    test_check(access(output_path, F_OK) != 0, "changed-source output absent");
    evo_project_baseline_destroy(&baseline);

    runner = (test_runner_context_t){0};
    runner.invalid_outcome = true;
    test_check(
        test_path(
            output_path,
            sizeof(output_path),
            temporary_root,
            "provider-output"),
        "provider output path");
    config = test_config(
        fixture_manifest,
        EVO_TEST_SOURCE_DIR "/tests/fixtures/project-ingestion/cmake/project",
        output_path,
        &runner);
    status = evo_project_capture_baseline(&config, &baseline);
    test_check(
        status == EVO_PROJECT_ERROR_EXECUTION_PROVIDER,
        "malformed provider outcome rejected");
    test_check(runner.calls == 1U, "provider failure stops later gates");
    test_check(access(output_path, F_OK) != 0, "provider-failure output absent");
    evo_project_baseline_destroy(&baseline);
}

int main(void)
{
    static const char *const cmake_files[] = {
        "CMakeLists.txt",
        "compile_commands.json",
        "include/value.h",
        "src/value.c",
        "tests/value_benchmark.c",
        "tests/value_test.c"};
    static const char *const autotools_files[] = {
        "Makefile.am",
        "compile_commands.json",
        "configure.ac",
        "include/value.h",
        "src/value.c",
        "tests/value_benchmark.c",
        "tests/value_test.c"};
    char temporary_template[] = "/tmp/evo-project-ingestion-XXXXXX";
    char *temporary_root = mkdtemp(temporary_template);

    test_check(temporary_root != NULL, "create temporary root");
    if (temporary_root == NULL) {
        return EXIT_FAILURE;
    }
    test_successful_replay(
        temporary_root,
        "cmake",
        "manifest-reordered.json",
        cmake_files,
        sizeof(cmake_files) / sizeof(cmake_files[0]),
        "cmake",
        "fnv1a64-v1:8eb75ed14a510f89",
        "fnv1a64-v1:cf7551a9cd56b691");
    test_successful_replay(
        temporary_root,
        "autotools",
        "manifest.json",
        autotools_files,
        sizeof(autotools_files) / sizeof(autotools_files[0]),
        "autotools",
        "fnv1a64-v1:ec52a4f523da9734",
        "fnv1a64-v1:300add83f452b5a5");
    test_gate_failure(
        temporary_root,
        EVO_PROJECT_COMMAND_CONFIGURE,
        false,
        EVO_PROJECT_BASELINE_BUILD_FAILED,
        1U,
        "configure-failed");
    test_gate_failure(
        temporary_root,
        EVO_PROJECT_COMMAND_COMPILE,
        true,
        EVO_PROJECT_BASELINE_BUILD_FAILED,
        2U,
        "compile-timeout");
    test_gate_failure(
        temporary_root,
        EVO_PROJECT_COMMAND_CORRECTNESS,
        false,
        EVO_PROJECT_BASELINE_CORRECTNESS_FAILED,
        3U,
        "correctness-failed");
    test_gate_failure(
        temporary_root,
        EVO_PROJECT_COMMAND_BENCHMARK,
        false,
        EVO_PROJECT_BASELINE_BENCHMARK_INELIGIBLE,
        4U,
        "benchmark-ineligible");
    test_optional_benchmark(temporary_root);
    test_preflight_failures(temporary_root);
    test_integrity_and_provider_failures(temporary_root);

    return test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
