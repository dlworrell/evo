#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "internal/project_baseline_owner.h"
#include "internal/project_candidate.h"
#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_recipe_owner.h"
#include "internal/project_runtime.h"
#include "internal/project_transformation_owner.h"

#include <dirent.h>
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

#define TEST_FILE_COUNT 2U
#define TEST_RECORD_COUNT 3U

static int test_failures = 0;

static const char test_a_before[] =
    "int adjust(int total, int ready) {\n"
    "    total = total + ready;\n"
    "    return total;\n"
    "}\n";

static const char test_a_after[] =
    "int adjust(int total, int ready) {\n"
    "    total += ready;\n"
    "    return total;\n"
    "}\n";

static const char test_b_before[] =
    "unsigned scale(unsigned value) {\n"
    "    return value * 8U;\n"
    "}\n";

static const char test_b_after[] =
    "unsigned scale(unsigned value) {\n"
    "    return (value << 3);\n"
    "}\n";

typedef struct test_fixture {
    char root[256];
    char snapshot_path[320];
    char a_path[384];
    char sub_path[384];
    char b_path[448];
    char *permitted_roots[TEST_FILE_COUNT];
    evo_project_file_record_t files[TEST_FILE_COUNT];
    evo_project_baseline_owner_t baseline_owner;
    evo_project_baseline_t baseline;
    evo_project_recipe_record_t records[TEST_RECORD_COUNT];
    evo_project_recipe_owner_t recipe_owner;
    evo_project_recipe_t recipe;
    evo_project_transformation_application_owner_t application_owners[TEST_RECORD_COUNT];
    evo_project_transformation_application_t applications[TEST_RECORD_COUNT];
} test_fixture_t;

static void test_check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project candidate test failure: %s\n", message);
        test_failures += 1;
    }
}

static bool test_write_all(int file_fd, const unsigned char *bytes, size_t size)
{
    size_t position = 0U;

    while (position < size) {
        const ssize_t count = write(file_fd, bytes + position, size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        position += (size_t)count;
    }
    return true;
}

static bool test_write_file(const char *path, const char *text)
{
    const size_t size = strlen(text);
    int file_fd = open(
        path,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    bool success = false;

    if (file_fd >= 0 &&
        test_write_all(file_fd, (const unsigned char *)text, size) &&
        fsync(file_fd) == 0 && fchmod(file_fd, (mode_t)0444) == 0 &&
        close(file_fd) == 0) {
        file_fd = -1;
        success = true;
    }
    if (file_fd >= 0) {
        (void)close(file_fd);
    }
    return success;
}

static bool test_read_file(const char *path, unsigned char **bytes, size_t *size)
{
    struct stat metadata;
    size_t position = 0U;
    int file_fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);

    if (file_fd < 0 || fstat(file_fd, &metadata) != 0 ||
        !S_ISREG(metadata.st_mode) || metadata.st_size < 0 ||
        (uintmax_t)metadata.st_size > (uintmax_t)SIZE_MAX) {
        if (file_fd >= 0) {
            (void)close(file_fd);
        }
        return false;
    }
    *size = (size_t)metadata.st_size;
    if (*size == SIZE_MAX) {
        (void)close(file_fd);
        return false;
    }
    *bytes = evo_project_allocate_zeroed(*size + 1U, sizeof(**bytes));
    if (*bytes == NULL) {
        (void)close(file_fd);
        return false;
    }
    while (position < *size) {
        const ssize_t count = read(file_fd, *bytes + position, *size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            evo_project_release(*bytes);
            *bytes = NULL;
            (void)close(file_fd);
            return false;
        }
        position += (size_t)count;
    }
    if (close(file_fd) != 0) {
        evo_project_release(*bytes);
        *bytes = NULL;
        return false;
    }
    (*bytes)[*size] = '\0';
    return true;
}

static bool test_file_equals(const char *path, const char *expected)
{
    unsigned char *bytes = NULL;
    size_t size = 0U;
    const size_t expected_size = strlen(expected);
    size_t index;
    bool equal = test_read_file(path, &bytes, &size) && size == expected_size;

    if (equal) {
        for (index = 0U; index < size; index += 1U) {
            if (bytes[index] != (unsigned char)expected[index]) {
                equal = false;
                break;
            }
        }
    }
    evo_project_release(bytes);
    return equal;
}

static uint64_t test_fingerprint_value(const void *bytes, size_t size)
{
    evo_project_fingerprint_t fingerprint;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(&fingerprint, bytes, size);
    return fingerprint.value;
}

static void test_fingerprint_text(
    const void *bytes,
    size_t size,
    char output[EVO_PROJECT_FINGERPRINT_TEXT_SIZE])
{
    evo_project_fingerprint_format(test_fingerprint_value(bytes, size), output);
}

static size_t test_find_text(const char *source, const char *needle)
{
    const char *found = strstr(source, needle);

    return found == NULL ? SIZE_MAX : (size_t)(found - source);
}

static bool test_json_valid(const char *text, size_t size)
{
    const size_t capacity = 2048U;
    evo_project_json_token_t *tokens = evo_project_allocate_zeroed(
        capacity, sizeof(*tokens));
    size_t count = 0U;
    bool valid = false;

    if (tokens != NULL) {
        valid = evo_project_json_parse(
                    text, size, tokens, capacity, 24U, &count) ==
                    EVO_PROJECT_JSON_SUCCESS &&
                count > 0U && tokens[0].type == EVO_PROJECT_JSON_OBJECT &&
                evo_project_json_next(tokens, count, 0U) == count;
    }
    evo_project_release(tokens);
    return valid;
}

static bool test_remove_directory_contents(int directory_fd)
{
    DIR *directory;
    struct dirent *entry;
    int iteration_fd;

    if (fchmod(directory_fd, (mode_t)0700) != 0) {
        return false;
    }
    iteration_fd = dup(directory_fd);
    if (iteration_fd < 0) {
        return false;
    }
    directory = fdopendir(iteration_fd);
    if (directory == NULL) {
        (void)close(iteration_fd);
        return false;
    }
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        struct stat metadata;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            errno = 0;
            continue;
        }
        if (fstatat(directory_fd, entry->d_name, &metadata, AT_SYMLINK_NOFOLLOW) != 0) {
            (void)closedir(directory);
            return false;
        }
        if (S_ISDIR(metadata.st_mode)) {
            const int child_fd = openat(
                directory_fd,
                entry->d_name,
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
                0);
            bool success;

            if (child_fd < 0) {
                (void)closedir(directory);
                return false;
            }
            success = test_remove_directory_contents(child_fd);
            (void)close(child_fd);
            if (!success || unlinkat(directory_fd, entry->d_name, AT_REMOVEDIR) != 0) {
                (void)closedir(directory);
                return false;
            }
        } else if (unlinkat(directory_fd, entry->d_name, 0) != 0) {
            (void)closedir(directory);
            return false;
        }
        errno = 0;
    }
    return errno == 0 && closedir(directory) == 0;
}

static void test_remove_tree(const char *path)
{
    int directory_fd = open(
        path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);

    if (directory_fd < 0) {
        return;
    }
    (void)test_remove_directory_contents(directory_fd);
    (void)close(directory_fd);
    (void)rmdir(path);
}

static evo_project_candidate_limits_t test_limits(void)
{
    evo_project_candidate_limits_t limits = {0};

    limits.max_string_bytes = 256U;
    limits.max_path_bytes = 512U;
    limits.max_files = 16U;
    limits.max_file_bytes = 16384U;
    limits.max_total_file_bytes = 65536U;
    limits.max_edits = 16U;
    limits.max_patch_bytes = 65536U;
    limits.max_evidence_bytes = 65536U;
    return limits;
}

static void test_prepare_record(
    evo_project_recipe_record_t *record,
    const char *identity,
    const char *location_identity,
    const char *file,
    const char *transformation_identity)
{
    record->identity = identity;
    record->target.location_identity = location_identity;
    record->target.file = file;
    record->transformation_identity = transformation_identity;
    record->transformation_version = 1U;
}

static void test_prepare_application_common(
    test_fixture_t *fixture,
    size_t index,
    const char *application_identity)
{
    evo_project_transformation_application_t *application =
        &fixture->applications[index];
    evo_project_transformation_application_owner_t *owner =
        &fixture->application_owners[index];
    const evo_project_recipe_record_t *record = &fixture->records[index];

    application->schema_version =
        EVO_PROJECT_TRANSFORMATION_APPLICATION_SCHEMA_VERSION;
    application->baseline_fingerprint = fixture->baseline.baseline_fingerprint;
    application->recipe_fingerprint = fixture->recipe.recipe_fingerprint;
    application->record_identity = record->identity;
    application->transformation_identity = record->transformation_identity;
    application->transformation_version = record->transformation_version;
    application->target = record->target;
    application->projection_complete = true;
    application->probabilistic_authority = false;
    application->snapshot_modified = false;
    application->candidate_materialized = false;
    owner->application_fingerprint = test_fingerprint_value(
        application_identity, strlen(application_identity));
    evo_project_fingerprint_format(
        owner->application_fingerprint, application->application_fingerprint);
    application->private_owner = owner;
}

static bool test_prepare_edit(
    evo_project_transformation_application_t *application,
    const char *source,
    const char *before,
    const char *replacement)
{
    const size_t start = test_find_text(source, before);
    const size_t before_size = strlen(before);
    const size_t replacement_size = strlen(replacement);

    if (start == SIZE_MAX) {
        return false;
    }
    application->disposition = EVO_PROJECT_TRANSFORMATION_EDIT;
    application->edit.before_start = start;
    application->edit.before_end = start + before_size;
    application->edit.before_size = before_size;
    application->edit.before_text = before;
    test_fingerprint_text(
        before, before_size, application->edit.before_fingerprint);
    application->edit.after_start = start;
    application->edit.after_end = start + replacement_size;
    application->edit.replacement_size = replacement_size;
    application->edit.replacement_text = replacement;
    test_fingerprint_text(
        replacement,
        replacement_size,
        application->edit.replacement_fingerprint);
    return true;
}

static bool test_fixture_prepare(test_fixture_t *fixture)
{
    char temporary_template[] = "/tmp/evo-project-candidate-XXXXXX";
    char *root = mkdtemp(temporary_template);
    char *canonical_root;
    int written;

    if (root == NULL) {
        return false;
    }
    canonical_root = realpath(root, NULL);
    if (canonical_root == NULL) {
        return false;
    }
    written = evo_project_format(
        fixture->root, sizeof(fixture->root), "%s", canonical_root);
    free(canonical_root);
    if (written <= 0 || (size_t)written >= sizeof(fixture->root)) {
        return false;
    }
    written = evo_project_format(
        fixture->snapshot_path,
        sizeof(fixture->snapshot_path),
        "%s/snapshot",
        fixture->root);
    if (written <= 0 || (size_t)written >= sizeof(fixture->snapshot_path) ||
        mkdir(fixture->snapshot_path, 0700) != 0) {
        return false;
    }
    written = evo_project_format(
        fixture->a_path,
        sizeof(fixture->a_path),
        "%s/a.c",
        fixture->snapshot_path);
    if (written <= 0 || (size_t)written >= sizeof(fixture->a_path) ||
        !test_write_file(fixture->a_path, test_a_before)) {
        return false;
    }
    written = evo_project_format(
        fixture->sub_path,
        sizeof(fixture->sub_path),
        "%s/sub",
        fixture->snapshot_path);
    if (written <= 0 || (size_t)written >= sizeof(fixture->sub_path) ||
        mkdir(fixture->sub_path, 0700) != 0) {
        return false;
    }
    written = evo_project_format(
        fixture->b_path,
        sizeof(fixture->b_path),
        "%s/b.c",
        fixture->sub_path);
    if (written <= 0 || (size_t)written >= sizeof(fixture->b_path) ||
        !test_write_file(fixture->b_path, test_b_before) ||
        chmod(fixture->sub_path, 0500) != 0 ||
        chmod(fixture->snapshot_path, 0500) != 0) {
        return false;
    }

    fixture->permitted_roots[0] = "a.c";
    fixture->permitted_roots[1] = "sub";
    fixture->files[0].path = "a.c";
    fixture->files[0].size = strlen(test_a_before);
    fixture->files[0].source_mode = 0644U;
    fixture->files[0].content_fingerprint = test_fingerprint_value(
        test_a_before, strlen(test_a_before));
    fixture->files[1].path = "sub/b.c";
    fixture->files[1].size = strlen(test_b_before);
    fixture->files[1].source_mode = 0644U;
    fixture->files[1].content_fingerprint = test_fingerprint_value(
        test_b_before, strlen(test_b_before));

    fixture->baseline_owner.manifest.permitted_roots = fixture->permitted_roots;
    fixture->baseline_owner.manifest.permitted_root_count = TEST_FILE_COUNT;
    fixture->baseline_owner.manifest.budget.max_files = 16U;
    fixture->baseline_owner.manifest.budget.max_file_bytes = 16384U;
    fixture->baseline_owner.manifest.budget.max_total_bytes = 65536U;
    fixture->baseline_owner.manifest.budget.max_path_bytes = 512U;
    fixture->baseline_owner.manifest.budget.max_evidence_bytes = 65536U;
    fixture->baseline_owner.snapshot_path = fixture->snapshot_path;
    fixture->baseline_owner.files = fixture->files;
    fixture->baseline_owner.file_count = TEST_FILE_COUNT;
    fixture->baseline_owner.total_file_bytes =
        fixture->files[0].size + fixture->files[1].size;
    fixture->baseline_owner.baseline_fingerprint =
        UINT64_C(0x1020304050607080);
    fixture->baseline_owner.state = EVO_PROJECT_BASELINE_ELIGIBLE;
    fixture->baseline_owner.committed = true;

    fixture->baseline.schema_version = EVO_PROJECT_BASELINE_SCHEMA_VERSION;
    fixture->baseline.state = EVO_PROJECT_BASELINE_ELIGIBLE;
    evo_project_fingerprint_format(
        fixture->baseline_owner.baseline_fingerprint,
        fixture->baseline.baseline_fingerprint);
    fixture->baseline.file_count = TEST_FILE_COUNT;
    fixture->baseline.total_file_bytes = fixture->baseline_owner.total_file_bytes;
    fixture->baseline.files = fixture->files;
    fixture->baseline.projection_complete = true;
    fixture->baseline.probabilistic_authority = false;
    fixture->baseline.private_owner = &fixture->baseline_owner;

    test_prepare_record(
        &fixture->records[0],
        "record-assignment",
        "location-assignment",
        "a.c",
        "catalyst.evo.c.assignment-to-compound");
    test_prepare_record(
        &fixture->records[1],
        "record-shift",
        "location-shift",
        "sub/b.c",
        "catalyst.evo.c.unsigned-multiply-to-shift");
    test_prepare_record(
        &fixture->records[2],
        "record-no-change",
        "location-no-change",
        "a.c",
        "catalyst.evo.c.double-negation-condition");

    fixture->recipe_owner.records = fixture->records;
    fixture->recipe_owner.record_count = TEST_RECORD_COUNT;
    fixture->recipe_owner.recipe_fingerprint =
        UINT64_C(0x8877665544332211);
    fixture->recipe.schema_version = EVO_PROJECT_RECIPE_SCHEMA_VERSION;
    fixture->recipe.baseline_fingerprint = fixture->baseline.baseline_fingerprint;
    fixture->recipe.records = fixture->records;
    fixture->recipe.record_count = TEST_RECORD_COUNT;
    evo_project_fingerprint_format(
        fixture->recipe_owner.recipe_fingerprint,
        fixture->recipe.recipe_fingerprint);
    fixture->recipe.projection_complete = true;
    fixture->recipe.probabilistic_authority = false;
    fixture->recipe.raw_source_bytes = false;
    fixture->recipe.private_owner = &fixture->recipe_owner;

    test_prepare_application_common(fixture, 0U, "application-assignment");
    test_prepare_application_common(fixture, 1U, "application-shift");
    test_prepare_application_common(fixture, 2U, "application-no-change");
    if (!test_prepare_edit(
            &fixture->applications[0],
            test_a_before,
            "total = total + ready",
            "total += ready") ||
        !test_prepare_edit(
            &fixture->applications[1],
            test_b_before,
            "value * 8U",
            "(value << 3)")) {
        return false;
    }
    fixture->applications[2].disposition =
        EVO_PROJECT_TRANSFORMATION_ALREADY_SATISFIED;
    return true;
}

static void test_fixture_destroy(test_fixture_t *fixture)
{
    if (fixture->root[0] != '\0') {
        test_remove_tree(fixture->root);
    }
    *fixture = (test_fixture_t){0};
}

static bool test_make_output_path(
    const test_fixture_t *fixture,
    const char *name,
    char *path,
    size_t path_size)
{
    const int written = evo_project_format(
        path, path_size, "%s/%s", fixture->root, name);

    return written > 0 && (size_t)written < path_size;
}

static evo_project_candidate_config_t test_config(
    test_fixture_t *fixture,
    const char *output_path,
    evo_project_candidate_workspace_policy_t policy)
{
    evo_project_candidate_config_t config = {0};

    config.baseline = &fixture->baseline;
    config.recipe = &fixture->recipe;
    config.application_count = TEST_RECORD_COUNT;
    config.applications = fixture->applications;
    config.output_path = output_path;
    config.workspace_policy = policy;
    config.limits = test_limits();
    return config;
}

static void test_success_and_replay(test_fixture_t *fixture)
{
    char output_one[384];
    char output_two[384];
    char candidate_a[448];
    char candidate_b[512];
    char patch_path[448];
    evo_project_candidate_t first = {0};
    evo_project_candidate_t second = {0};
    evo_project_candidate_config_t config;
    evo_project_candidate_status_t status;

    test_check(
        test_make_output_path(
            fixture, "candidate-one", output_one, sizeof(output_one)) &&
            test_make_output_path(
                fixture, "candidate-two", output_two, sizeof(output_two)),
        "form replay output paths");
    config = test_config(
        fixture, output_one, EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN);
    status = evo_project_candidate_materialize(&config, &first);
    test_check(status == EVO_PROJECT_CANDIDATE_SUCCESS, "first materialization succeeds");
    if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
        return;
    }
    test_check(first.changed_file_count == 2U, "multi-file change inventory");
    test_check(first.file_count == TEST_FILE_COUNT, "candidate file count");
    test_check(first.workspace_path != NULL, "retained workspace path published");
    test_check(first.patch_size > 0U, "normalized patch emitted");
    test_check(
        test_json_valid(first.canonical_json, first.canonical_json_size),
        "candidate JSON parses");
    test_check(!first.source_modified, "source modification remains false");
    test_check(!first.snapshot_modified, "snapshot modification remains false");
    test_check(first.projection_complete, "projection complete");
    test_check(!first.probabilistic_authority, "exact authority only");

    (void)evo_project_format(
        candidate_a, sizeof(candidate_a), "%s/candidate/a.c", output_one);
    (void)evo_project_format(
        candidate_b, sizeof(candidate_b), "%s/candidate/sub/b.c", output_one);
    (void)evo_project_format(
        patch_path, sizeof(patch_path), "%s/candidate.patch", output_one);
    test_check(test_file_equals(candidate_a, test_a_after), "first file transformed");
    test_check(test_file_equals(candidate_b, test_b_after), "second file transformed");
    test_check(test_file_equals(fixture->a_path, test_a_before), "baseline a.c unchanged");
    test_check(test_file_equals(fixture->b_path, test_b_before), "baseline b.c unchanged");
    test_check(access(patch_path, R_OK) == 0, "published patch is readable");

    config = test_config(
        fixture, output_two, EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN);
    status = evo_project_candidate_materialize(&config, &second);
    test_check(status == EVO_PROJECT_CANDIDATE_SUCCESS, "replay materialization succeeds");
    if (status == EVO_PROJECT_CANDIDATE_SUCCESS) {
        test_check(
            strcmp(first.candidate_fingerprint, second.candidate_fingerprint) == 0,
            "replay candidate identity is byte-stable");
        test_check(first.patch_size == second.patch_size, "replay patch size stable");
        test_check(
            first.patch_size == second.patch_size &&
                strcmp(first.patch, second.patch) == 0,
            "replay patch bytes stable");
    }
    evo_project_candidate_destroy(&second);
    evo_project_candidate_destroy(&first);
}

static void test_discard_policy(test_fixture_t *fixture)
{
    char output[384];
    char workspace[448];
    char patch[448];
    evo_project_candidate_t candidate = {0};
    evo_project_candidate_config_t config;
    evo_project_candidate_status_t status;

    test_check(
        test_make_output_path(
            fixture, "candidate-discard", output, sizeof(output)),
        "form discard output path");
    config = test_config(
        fixture, output, EVO_PROJECT_CANDIDATE_WORKSPACE_DISCARD);
    status = evo_project_candidate_materialize(&config, &candidate);
    test_check(status == EVO_PROJECT_CANDIDATE_SUCCESS, "discard materialization succeeds");
    if (status == EVO_PROJECT_CANDIDATE_SUCCESS) {
        (void)evo_project_format(
            workspace, sizeof(workspace), "%s/candidate", output);
        (void)evo_project_format(
            patch, sizeof(patch), "%s/candidate.patch", output);
        test_check(candidate.workspace_path == NULL, "discard has no workspace path");
        test_check(access(workspace, F_OK) != 0, "discard removes candidate tree");
        test_check(access(patch, R_OK) == 0, "discard retains patch evidence");
    }
    evo_project_candidate_destroy(&candidate);
}

static void test_conflict_failure(test_fixture_t *fixture)
{
    char output[384];
    evo_project_candidate_t candidate = {0};
    evo_project_candidate_config_t config;
    evo_project_candidate_status_t status;
    evo_project_recipe_target_t saved_target = fixture->records[2].target;
    evo_project_transformation_application_t saved_application =
        fixture->applications[2];

    fixture->records[2].target.file = "a.c";
    fixture->records[2].target.location_identity = "location-overlap";
    fixture->applications[2].target = fixture->records[2].target;
    fixture->applications[2].disposition = EVO_PROJECT_TRANSFORMATION_EDIT;
    test_check(
        test_prepare_edit(
            &fixture->applications[2],
            test_a_before,
            "total + ready",
            "total - ready"),
        "prepare overlapping edit");
    test_check(
        test_make_output_path(
            fixture, "candidate-conflict", output, sizeof(output)),
        "form conflict output path");
    config = test_config(
        fixture, output, EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN);
    status = evo_project_candidate_materialize(&config, &candidate);
    test_check(status == EVO_PROJECT_CANDIDATE_ERROR_CONFLICT, "overlap fails closed");
    test_check(candidate.private_owner == NULL, "conflict publishes no candidate");
    test_check(access(output, F_OK) != 0, "conflict leaves no partial output");
    evo_project_candidate_destroy(&candidate);
    fixture->records[2].target = saved_target;
    fixture->applications[2] = saved_application;
}

static void test_path_escape_failure(test_fixture_t *fixture)
{
    char output[384];
    evo_project_candidate_t candidate = {0};
    evo_project_candidate_config_t config;
    evo_project_candidate_status_t status;
    const char *saved_record_file = fixture->records[2].target.file;
    const char *saved_application_file = fixture->applications[2].target.file;

    fixture->records[2].target.file = "../outside.c";
    fixture->applications[2].target.file = "../outside.c";
    test_check(
        test_make_output_path(
            fixture, "candidate-escape", output, sizeof(output)),
        "form path escape output path");
    config = test_config(
        fixture, output, EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN);
    status = evo_project_candidate_materialize(&config, &candidate);
    test_check(
        status == EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID,
        "path traversal fails closed");
    test_check(access(output, F_OK) != 0, "path failure creates no output");
    evo_project_candidate_destroy(&candidate);
    fixture->records[2].target.file = saved_record_file;
    fixture->applications[2].target.file = saved_application_file;
}

static void test_resource_failure(test_fixture_t *fixture)
{
    char output[384];
    evo_project_candidate_t candidate = {0};
    evo_project_candidate_config_t config;
    evo_project_candidate_status_t status;

    test_check(
        test_make_output_path(
            fixture, "candidate-resource", output, sizeof(output)),
        "form resource output path");
    config = test_config(
        fixture, output, EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN);
    config.limits.max_patch_bytes = 32U;
    status = evo_project_candidate_materialize(&config, &candidate);
    test_check(
        status == EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT,
        "patch resource exhaustion fails closed");
    test_check(access(output, F_OK) != 0, "resource failure creates no output");
    evo_project_candidate_destroy(&candidate);
}

static void test_output_inside_snapshot_failure(test_fixture_t *fixture)
{
    char output[448];
    evo_project_candidate_t candidate = {0};
    evo_project_candidate_config_t config;
    evo_project_candidate_status_t status;

    (void)evo_project_format(
        output,
        sizeof(output),
        "%s/forbidden-candidate",
        fixture->snapshot_path);
    config = test_config(
        fixture, output, EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN);
    status = evo_project_candidate_materialize(&config, &candidate);
    test_check(
        status == EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID,
        "snapshot-contained output rejected before write");
    test_check(access(output, F_OK) != 0, "snapshot output remains absent");
    evo_project_candidate_destroy(&candidate);
}

int main(void)
{
    test_fixture_t fixture = {0};

    test_check(test_fixture_prepare(&fixture), "fixture preparation");
    if (test_failures == 0) {
        test_success_and_replay(&fixture);
        test_discard_policy(&fixture);
        test_conflict_failure(&fixture);
        test_path_escape_failure(&fixture);
        test_resource_failure(&fixture);
        test_output_inside_snapshot_failure(&fixture);
    }
    test_fixture_destroy(&fixture);
    if (test_failures != 0) {
        (void)fprintf(
            stderr,
            "project candidate test: %d failure(s)\n",
            test_failures);
        return EXIT_FAILURE;
    }
    (void)fprintf(stdout, "project candidate test: all checks passed\n");
    return EXIT_SUCCESS;
}
