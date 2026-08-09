#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700

#include "internal/project_analysis.h"
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

enum test_provider_mode {
    TEST_PROVIDER_VALID = 0,
    TEST_PROVIDER_ERROR = 1,
    TEST_PROVIDER_SCHEMA = 2,
    TEST_PROVIDER_MISSING_REFERENCE = 3,
    TEST_PROVIDER_DUPLICATE_IDENTITY = 4,
    TEST_PROVIDER_GENERATED_SOURCE = 5,
    TEST_PROVIDER_MUTATE_SNAPSHOT = 6,
    TEST_PROVIDER_MUTATE_SNAPSHOT_DIRECTORY = 7,
    TEST_PROVIDER_INCOMPLETE = 8,
    TEST_PROVIDER_MALFORMED_ARRAY = 9,
    TEST_PROVIDER_ZERO_RUNTIME = 10,
    TEST_PROVIDER_INVALID_ENUM = 11,
    TEST_PROVIDER_NULL_REFERENCE = 12,
    TEST_PROVIDER_MUTATE_SNAPSHOT_AND_ERROR = 13,
    TEST_PROVIDER_OVERFLOW_COUNT = 14
};

typedef struct test_provider_context {
    enum test_provider_mode mode;
    bool reverse;
    size_t provider_calls;
    bool request_valid;
    evo_project_source_location_record_t source_locations[7];
    evo_project_declaration_record_t declarations[4];
    evo_project_call_record_t call_records[1];
    evo_project_control_flow_record_t control_flows[2];
    evo_project_data_flow_record_t data_flows[1];
    evo_project_optimization_record_t optimization_records[2];
    evo_project_runtime_record_t runtime_records[2];
} test_provider_context_t;

static int test_failures = 0;

static const evo_project_source_location_record_t test_source_locations[] = {
    {"loc:test:call",
     "tests/value_test.c",
     5U,
     12U,
     5U,
     28U,
     EVO_PROJECT_LOCATION_SPELLING,
     NULL},
    {"loc:header:guard",
     "include/value.h",
     1U,
     1U,
     1U,
     37U,
     EVO_PROJECT_LOCATION_SPELLING,
     NULL},
    {"loc:source:return",
     "src/value.c",
     5U,
     5U,
     5U,
     28U,
     EVO_PROJECT_LOCATION_SPELLING,
     NULL},
    {"loc:source:macro-expansion",
     "src/value.c",
     1U,
     1U,
     1U,
     18U,
     EVO_PROJECT_LOCATION_MACRO_EXPANSION,
     "loc:header:guard"},
    {"loc:header:declaration",
     "include/value.h",
     4U,
     1U,
     4U,
     29U,
     EVO_PROJECT_LOCATION_SPELLING,
     NULL},
    {"loc:test:main",
     "tests/value_test.c",
     3U,
     1U,
     6U,
     2U,
     EVO_PROJECT_LOCATION_SPELLING,
     NULL},
    {"loc:source:function",
     "src/value.c",
     3U,
     1U,
     6U,
     2U,
     EVO_PROJECT_LOCATION_SPELLING,
     NULL}};

static const evo_project_declaration_record_t test_declarations[] = {
    {"decl:main:def",
     "main",
     "tests/value_test.c",
     "loc:test:main",
     EVO_PROJECT_DECLARATION_FUNCTION,
     EVO_PROJECT_LINKAGE_EXTERNAL,
     true},
    {"decl:fixture-value:header",
     "fixture_value",
     "src/value.c",
     "loc:header:declaration",
     EVO_PROJECT_DECLARATION_FUNCTION,
     EVO_PROJECT_LINKAGE_EXTERNAL,
     false},
    {"decl:input:param",
     "input",
     "src/value.c",
     "loc:source:function",
     EVO_PROJECT_DECLARATION_VARIABLE,
     EVO_PROJECT_LINKAGE_NONE,
     true},
    {"decl:fixture-value:def",
     "fixture_value",
     "src/value.c",
     "loc:source:function",
     EVO_PROJECT_DECLARATION_FUNCTION,
     EVO_PROJECT_LINKAGE_EXTERNAL,
     true}};

static const evo_project_call_record_t test_calls[] = {
    {"call:main:fixture-value",
     "decl:main:def",
     "decl:fixture-value:def",
     "loc:test:call",
     EVO_PROJECT_CALL_DIRECT}};

static const evo_project_control_flow_record_t test_control_flows[] = {
    {"cfg:fixture-value:entry-return",
     "decl:fixture-value:def",
     "block:entry",
     "block:return",
     "loc:source:function",
     EVO_PROJECT_CONTROL_FALLTHROUGH},
    {"cfg:fixture-value:return-exit",
     "decl:fixture-value:def",
     "block:return",
     "block:exit",
     "loc:source:return",
     EVO_PROJECT_CONTROL_RETURN}};

static const evo_project_data_flow_record_t test_data_flows[] = {
    {"df:fixture-value:input-read",
     "decl:fixture-value:def",
     "decl:input:param",
     "loc:source:return",
     EVO_PROJECT_DATA_READ}};

static const evo_project_optimization_record_t test_optimization_records[] = {
    {"opt:inline:main-call",
     "inline",
     "decl:main:def",
     "loc:test:call",
     "callee was not inlined",
     EVO_PROJECT_OPTIMIZATION_MISSED},
    {"opt:reassociate:return",
     "reassociate",
     "decl:fixture-value:def",
     "loc:source:return",
     "integer expression retained",
     EVO_PROJECT_OPTIMIZATION_MISSED}};

static const evo_project_runtime_record_t test_runtime_records[] = {
    {"runtime:fixture-value:return",
     "small-value-loop-v1",
     "decl:fixture-value:def",
     "loc:source:return",
     EVO_PROJECT_RUNTIME_SAMPLE_COUNT,
     UINT64_C(100)},
    {"runtime:main:call",
     "small-value-loop-v1",
     "decl:main:def",
     "loc:test:call",
     EVO_PROJECT_RUNTIME_SAMPLE_COUNT,
     UINT64_C(900)}};

static const char *const test_project_files[] = {
    "CMakeLists.txt",
    "compile_commands.json",
    "bad_compile_commands.json",
    "include/value.h",
    "src/value.c",
    "tests/value_test.c",
    "tests/value_benchmark.c"};

static void test_check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project analysis test failure: %s\n", message);
        test_failures += 1;
    }
}

static bool test_path(
    char *output,
    size_t output_size,
    const char *left,
    const char *right)
{
    const int written = evo_project_format(
        output, output_size, "%s/%s", left, right);

    return written > 0 && (size_t)written < output_size;
}

static char *test_read_file(const char *path, size_t *byte_count)
{
    struct stat metadata;
    int file_fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    size_t size;
    size_t position = 0U;
    char *bytes;

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
        const ssize_t count = read(file_fd, bytes + position, size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            evo_project_release(bytes);
            (void)close(file_fd);
            return NULL;
        }
        position += (size_t)count;
    }
    if (close(file_fd) != 0) {
        evo_project_release(bytes);
        return NULL;
    }
    bytes[size] = '\0';
    *byte_count = size;
    return bytes;
}

static bool test_write_all(int file_fd, const char *bytes, size_t byte_count)
{
    size_t position = 0U;

    while (position < byte_count) {
        const ssize_t count =
            write(file_fd, bytes + position, byte_count - position);

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

static bool test_prepare_project_fixture(
    const char *temporary_root,
    const char *name,
    char *project_path,
    size_t project_path_size)
{
    char source_root[1024];
    char include_path[1024];
    char source_path[1024];
    char tests_path[1024];
    char source_file[1024];
    char destination_file[1024];
    size_t index;
    const int written = evo_project_format(
        project_path,
        project_path_size,
        "%s/%s-source",
        temporary_root,
        name);

    if (written <= 0 || (size_t)written >= project_path_size ||
        !test_path(
            source_root,
            sizeof(source_root),
            EVO_TEST_SOURCE_DIR "/tests/fixtures/project-ingestion/cmake",
            "project") ||
        !test_path(
            include_path,
            sizeof(include_path),
            project_path,
            "include") ||
        !test_path(
            source_path, sizeof(source_path), project_path, "src") ||
        !test_path(tests_path, sizeof(tests_path), project_path, "tests") ||
        mkdir(project_path, 0700) != 0 || mkdir(include_path, 0700) != 0 ||
        mkdir(source_path, 0700) != 0 || mkdir(tests_path, 0700) != 0) {
        return false;
    }
    for (index = 0U;
         index < sizeof(test_project_files) / sizeof(test_project_files[0]);
         index += 1U) {
        if (!test_path(
                source_file,
                sizeof(source_file),
                source_root,
                test_project_files[index]) ||
            !test_path(
                destination_file,
                sizeof(destination_file),
                project_path,
                test_project_files[index]) ||
            !test_copy_file_with_mode(
                source_file, destination_file, (mode_t)0644)) {
            return false;
        }
    }
    return true;
}

static bool test_files_equal(const char *left_path, const char *right_path)
{
    size_t left_size = 0U;
    size_t right_size = 0U;
    char *left = test_read_file(left_path, &left_size);
    char *right = test_read_file(right_path, &right_size);
    bool equal = left != NULL && right != NULL && left_size == right_size;
    size_t index;

    if (equal) {
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
    size_t size = 0U;
    char *bytes = test_read_file(path, &size);
    const bool found = bytes != NULL && size > 0U &&
                       strstr(bytes, needle) != NULL;

    evo_project_release(bytes);
    return found;
}

static bool test_path_mode(
    const char *path,
    unsigned int expected_mode,
    bool directory)
{
    struct stat metadata;

    return lstat(path, &metadata) == 0 &&
           (directory ? S_ISDIR(metadata.st_mode)
                      : S_ISREG(metadata.st_mode)) &&
           (unsigned int)(metadata.st_mode & (mode_t)07777) == expected_mode;
}

static evo_project_ingest_limits_t test_ingest_limits(void)
{
    evo_project_ingest_limits_t limits = {0};

    limits.max_manifest_bytes = 65536U;
    limits.max_json_tokens = 4096U;
    limits.max_json_depth = 16U;
    limits.max_string_bytes = 1024U;
    limits.max_path_bytes = 1024U;
    limits.max_files = 64U;
    limits.max_file_bytes = 131072U;
    limits.max_total_bytes = 1048576U;
    limits.max_compilation_database_bytes = 131072U;
    limits.max_permitted_roots = 16U;
    limits.max_dependencies = 16U;
    limits.max_toolchains = 16U;
    limits.max_environment_entries = 16U;
    limits.max_targets = 16U;
    limits.max_workloads = 16U;
    limits.max_constraints = 16U;
    limits.max_command_args = 32U;
    limits.max_command_bytes = 4096U;
    limits.max_command_output_bytes = 65536U;
    limits.max_evidence_bytes = 1048576U;
    limits.max_command_timeout_ms = UINT64_C(60000);
    limits.max_memory_bytes = UINT64_C(536870912);
    limits.max_processes = 16U;
    limits.max_storage_bytes = UINT64_C(2097152);
    return limits;
}

static evo_project_analysis_limits_t test_analysis_limits(void)
{
    evo_project_analysis_limits_t limits = {0};

    limits.max_string_bytes = 1024U;
    limits.max_path_bytes = 1024U;
    limits.max_translation_units = 16U;
    limits.max_source_locations = 64U;
    limits.max_declarations = 64U;
    limits.max_calls = 64U;
    limits.max_control_flows = 64U;
    limits.max_data_flows = 64U;
    limits.max_optimization_records = 64U;
    limits.max_runtime_records = 64U;
    limits.max_opportunities = 64U;
    limits.max_evidence_bytes = 1048576U;
    return limits;
}

static evo_project_status_t test_command_runner(
    const evo_project_command_view_t *command,
    const char *workspace_path,
    void *context,
    evo_project_command_outcome_t *outcome)
{
    size_t *calls = context;

    if (command == NULL || workspace_path == NULL || outcome == NULL ||
        calls == NULL) {
        return EVO_PROJECT_ERROR_EXECUTION_PROVIDER;
    }
    *calls += 1U;
    outcome->schema_version = EVO_PROJECT_BASELINE_SCHEMA_VERSION;
    outcome->completed = true;
    outcome->timed_out = false;
    outcome->exit_code = 0;
    outcome->output_bytes = 8U;
    outcome->output_fingerprint = UINT64_C(0x1234) + (uint64_t)command->stage;
    return EVO_PROJECT_SUCCESS;
}

static bool test_capture_baseline(
    const char *temporary_root,
    const char *name,
    evo_project_baseline_t *baseline)
{
    char manifest_path[1024];
    char project_path[1024];
    char output_path[1024];
    evo_project_capture_config_t config = {0};
    size_t calls = 0U;

    if (!test_path(
            manifest_path,
            sizeof(manifest_path),
            EVO_TEST_SOURCE_DIR "/tests/fixtures/project-ingestion/cmake",
            "manifest.json") ||
        !test_prepare_project_fixture(
            temporary_root, name, project_path, sizeof(project_path)) ||
        !test_path(
            output_path,
            sizeof(output_path),
            temporary_root,
            name)) {
        return false;
    }
    config.manifest_path = manifest_path;
    config.authorized_project_root = project_path;
    config.output_path = output_path;
    config.execution_provider_identity = "analysis-test-baseline-runner-v1";
    config.limits = test_ingest_limits();
    config.command_runner = test_command_runner;
    config.command_runner_context = &calls;
    return evo_project_capture_baseline(&config, baseline) ==
               EVO_PROJECT_SUCCESS &&
           baseline->state == EVO_PROJECT_BASELINE_ELIGIBLE && calls == 4U;
}

static void test_copy_provider_records(test_provider_context_t *context)
{
    size_t index;

#define TEST_COPY_RECORDS(destination, source)                            \
    do {                                                                  \
        const size_t record_count = sizeof(source) / sizeof((source)[0]); \
        for (index = 0U; index < record_count; index += 1U) {             \
            const size_t source_index =                                   \
                context->reverse ? record_count - 1U - index : index;     \
            (destination)[index] = (source)[source_index];                \
        }                                                                 \
    } while (false)
    TEST_COPY_RECORDS(context->source_locations, test_source_locations);
    TEST_COPY_RECORDS(context->declarations, test_declarations);
    TEST_COPY_RECORDS(context->call_records, test_calls);
    TEST_COPY_RECORDS(context->control_flows, test_control_flows);
    TEST_COPY_RECORDS(context->data_flows, test_data_flows);
    TEST_COPY_RECORDS(
        context->optimization_records, test_optimization_records);
    TEST_COPY_RECORDS(context->runtime_records, test_runtime_records);
#undef TEST_COPY_RECORDS
}

static bool test_mutate_snapshot(const char *snapshot_path)
{
    char source_path[1024];
    int file_fd;
    const char replacement = 'X';

    if (!test_path(
            source_path,
            sizeof(source_path),
            snapshot_path,
            "src/value.c") ||
        chmod(source_path, 0600) != 0) {
        return false;
    }
    file_fd = open(source_path, O_WRONLY | O_NOFOLLOW | O_CLOEXEC);
    if (file_fd < 0) {
        return false;
    }
    if (write(file_fd, &replacement, 1U) != 1 || fsync(file_fd) != 0) {
        (void)close(file_fd);
        return false;
    }
    return close(file_fd) == 0;
}

static evo_project_analysis_status_t test_analysis_provider(
    const evo_project_analysis_request_t *request,
    void *context_value,
    evo_project_analysis_provider_result_t *result)
{
    test_provider_context_t *context = context_value;

    if (request == NULL || context == NULL || result == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    }
    context->provider_calls += 1U;
    context->request_valid =
        request->schema_version == EVO_PROJECT_ANALYSIS_SCHEMA_VERSION &&
        request->baseline_fingerprint != NULL &&
        request->snapshot_path != NULL &&
        request->compilation_unit_count == 2U &&
        request->compilation_units != NULL &&
        strcmp(request->provider_identity, "clang-llvm-test-provider-v1") == 0 &&
        strcmp(request->clang_identity, "clang-18.1.8") == 0 &&
        strcmp(request->llvm_identity, "llvm-18.1.8") == 0 &&
        strcmp(request->target_identity, "portable-posix-c17") == 0 &&
        strcmp(request->flags_identity, "fixture-c17-flags-v1") == 0 &&
        request->timeout_ms == UINT64_C(30000) &&
        request->max_memory_bytes == UINT64_C(268435456) &&
        request->max_processes == 8U &&
        request->max_storage_bytes == UINT64_C(1048576) &&
        request->max_output_bytes == 16384U &&
        request->limits.max_evidence_bytes == 262144U &&
        !request->network_access;
    if (context->mode == TEST_PROVIDER_ERROR) {
        return EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    }
    if (context->mode == TEST_PROVIDER_MUTATE_SNAPSHOT_AND_ERROR) {
        (void)test_mutate_snapshot(request->snapshot_path);
        return EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    }
    test_copy_provider_records(context);
    if (context->mode == TEST_PROVIDER_MISSING_REFERENCE) {
        context->call_records[0].callee_identity = "decl:missing";
    } else if (context->mode == TEST_PROVIDER_DUPLICATE_IDENTITY) {
        context->source_locations[1].identity =
            context->source_locations[0].identity;
    } else if (context->mode == TEST_PROVIDER_GENERATED_SOURCE) {
        context->source_locations[0].kind = EVO_PROJECT_LOCATION_GENERATED;
    } else if (context->mode == TEST_PROVIDER_ZERO_RUNTIME) {
        context->runtime_records[0].value = 0U;
    } else if (context->mode == TEST_PROVIDER_INVALID_ENUM) {
        context->optimization_records[0].disposition =
            (evo_project_optimization_disposition_t)99;
    } else if (context->mode == TEST_PROVIDER_NULL_REFERENCE) {
        context->call_records[0].callee_identity = NULL;
    } else if (context->mode == TEST_PROVIDER_MUTATE_SNAPSHOT &&
               !test_mutate_snapshot(request->snapshot_path)) {
        return EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    } else if (context->mode == TEST_PROVIDER_MUTATE_SNAPSHOT_DIRECTORY &&
               chmod(request->snapshot_path, 0700) != 0) {
        return EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    }
    result->schema_version =
        context->mode == TEST_PROVIDER_SCHEMA
            ? EVO_PROJECT_ANALYSIS_SCHEMA_VERSION + 1U
            : EVO_PROJECT_ANALYSIS_SCHEMA_VERSION;
    result->completed = context->mode != TEST_PROVIDER_INCOMPLETE;
    result->source_location_count =
        sizeof(context->source_locations) /
        sizeof(context->source_locations[0]);
    result->source_locations = context->source_locations;
    if (context->mode == TEST_PROVIDER_OVERFLOW_COUNT) {
        result->source_location_count = SIZE_MAX;
    }
    result->declaration_count =
        sizeof(context->declarations) / sizeof(context->declarations[0]);
    result->declarations = context->declarations;
    result->call_count =
        sizeof(context->call_records) / sizeof(context->call_records[0]);
    result->calls = context->call_records;
    if (context->mode == TEST_PROVIDER_MALFORMED_ARRAY) {
        result->calls = NULL;
    }
    result->control_flow_count =
        sizeof(context->control_flows) / sizeof(context->control_flows[0]);
    result->control_flows = context->control_flows;
    result->data_flow_count =
        sizeof(context->data_flows) / sizeof(context->data_flows[0]);
    result->data_flows = context->data_flows;
    result->optimization_record_count =
        sizeof(context->optimization_records) /
        sizeof(context->optimization_records[0]);
    result->optimization_records = context->optimization_records;
    if (request->runtime_profile_state == EVO_PROJECT_RUNTIME_AVAILABLE) {
        result->runtime_record_count =
            sizeof(context->runtime_records) /
            sizeof(context->runtime_records[0]);
        result->runtime_records = context->runtime_records;
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static evo_project_analysis_config_t test_analysis_config(
    const evo_project_baseline_t *baseline,
    const char *output_path,
    test_provider_context_t *context)
{
    evo_project_analysis_config_t config = {0};

    config.baseline = baseline;
    config.output_path = output_path;
    config.provider_identity = "clang-llvm-test-provider-v1";
    config.clang_identity = "clang-18.1.8";
    config.llvm_identity = "llvm-18.1.8";
    config.target_identity = "portable-posix-c17";
    config.flags_identity = "fixture-c17-flags-v1";
    config.runtime_profile_state = EVO_PROJECT_RUNTIME_AVAILABLE;
    config.runtime_profile_identity = "fixture-sample-profile-v1";
    config.limits = test_analysis_limits();
    config.provider = test_analysis_provider;
    config.provider_context = context;
    return config;
}

static void test_success_and_replay(
    const char *temporary_root,
    evo_project_baseline_t *baseline)
{
    char first_output[1024];
    char second_output[1024];
    char first_json[1024];
    char second_json[1024];
    char first_markdown[1024];
    char golden_json[1024];
    evo_project_analysis_t first = {0};
    evo_project_analysis_t second = {0};
    test_provider_context_t first_context = {0};
    test_provider_context_t second_context = {0};
    evo_project_analysis_config_t first_config;
    evo_project_analysis_config_t second_config;
    evo_project_analysis_status_t status;

    test_check(
        test_path(
            first_output,
            sizeof(first_output),
            temporary_root,
            "analysis-first") &&
            test_path(
                second_output,
                sizeof(second_output),
                temporary_root,
                "analysis-second") &&
            test_path(
                first_json,
                sizeof(first_json),
                first_output,
                "analysis.json") &&
            test_path(
                second_json,
                sizeof(second_json),
                second_output,
                "analysis.json") &&
            test_path(
                first_markdown,
                sizeof(first_markdown),
                first_output,
                "analysis.md") &&
            test_path(
                golden_json,
                sizeof(golden_json),
                EVO_TEST_SOURCE_DIR "/tests/fixtures/project-analysis",
                "golden-v1.json"),
        "success paths");
    first_config = test_analysis_config(
        baseline, first_output, &first_context);
    status = evo_project_analyze(&first_config, &first);
    if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        (void)fprintf(
            stderr,
            "first analysis status: %s (%u)\n",
            evo_project_analysis_status_name(status),
            (unsigned int)status);
    }
    test_check(status == EVO_PROJECT_ANALYSIS_SUCCESS, "first analysis succeeds");
    test_check(
        first_context.provider_calls == 1U, "first provider called once");
    test_check(first_context.request_valid, "provider request complete");
    test_check(
        first.translation_unit_count == 2U &&
            strcmp(first.translation_units[0], "src/value.c") == 0 &&
            strcmp(first.translation_units[1], "tests/value_test.c") == 0,
        "translation units complete");
    test_check(first.source_location_count == 7U, "source locations complete");
    test_check(first.declaration_count == 4U, "declarations complete");
    test_check(first.call_count == 1U, "calls complete");
    test_check(first.control_flow_count == 2U, "control flow complete");
    test_check(first.data_flow_count == 1U, "data flow complete");
    test_check(
        first.optimization_record_count == 2U,
        "compiler records complete");
    test_check(first.runtime_record_count == 2U, "runtime records complete");
    test_check(first.opportunity_count == 2U, "opportunities complete");
    test_check(
        first.opportunities != NULL &&
            first.opportunities[0].rank == 1U &&
            strcmp(
                first.opportunities[0].location_identity,
                "loc:test:call") == 0 &&
            first.opportunities[0].runtime_sample_count == UINT64_C(900) &&
            first.opportunities[1].rank == 2U &&
            strcmp(
                first.opportunities[1].location_identity,
                "loc:source:return") == 0,
        "opportunity rank policy");
    test_check(
        first.projection_complete && !first.probabilistic_authority,
        "human-readable abstraction declaration");
    test_check(
        test_file_contains(
            first_json,
            "\"runtime_profile\":{\"state\":\"available\"") &&
            test_file_contains(
                first_json,
                "\"translation_units\":[\"src/value.c\","
                "\"tests/value_test.c\"]") &&
            test_file_contains(
                first_json,
                "\"runtime_sample_count\":900") &&
            test_file_contains(
                first_json,
                "\"source_modified\":false") &&
            test_file_contains(
                first_markdown,
                "absent or unavailable profile is not zero runtime cost"),
        "canonical evidence content");
    status = evo_project_analyze(&first_config, &first);
    test_check(
        status == EVO_PROJECT_ANALYSIS_ERROR_RESULT_ACTIVE,
        "active result rejected");

    second_context.reverse = true;
    second_config = test_analysis_config(
        baseline, second_output, &second_context);
    status = evo_project_analyze(&second_config, &second);
    test_check(status == EVO_PROJECT_ANALYSIS_SUCCESS, "reordered replay succeeds");
    test_check(
        strcmp(first.analysis_fingerprint, second.analysis_fingerprint) == 0,
        "reordered provider has stable identity");
    test_check(test_files_equal(first_json, second_json), "replay JSON exact");
    test_check(
        test_files_equal(first_json, golden_json),
        "canonical analysis JSON golden");
    test_check(
        test_path_mode(first_output, 0500U, true) &&
            test_path_mode(first_json, 0400U, false) &&
            test_path_mode(first_markdown, 0400U, false),
        "completed analysis output is read-only");
    test_check(
        strcmp(
            first.analysis_fingerprint,
            "fnv1a64-v1:2cc6038835197dba") == 0,
        "analysis fingerprint golden vector");
    evo_project_analysis_destroy(&second);
    evo_project_analysis_destroy(&first);
}

static void test_unavailable_profile(
    const char *temporary_root,
    evo_project_baseline_t *baseline)
{
    char output_path[1024];
    char json_path[1024];
    evo_project_analysis_t analysis = {0};
    test_provider_context_t context = {0};
    evo_project_analysis_config_t config;
    evo_project_analysis_status_t status;

    test_check(
        test_path(
            output_path,
            sizeof(output_path),
            temporary_root,
            "analysis-unavailable") &&
            test_path(
                json_path,
                sizeof(json_path),
                output_path,
                "analysis.json"),
        "unavailable paths");
    config = test_analysis_config(baseline, output_path, &context);
    config.runtime_profile_state = EVO_PROJECT_RUNTIME_UNAVAILABLE;
    config.runtime_profile_identity = "fixture-profile-unavailable-v1";
    status = evo_project_analyze(&config, &analysis);
    test_check(status == EVO_PROJECT_ANALYSIS_SUCCESS, "unavailable profile accepted");
    test_check(analysis.runtime_record_count == 0U, "no invented runtime record");
    test_check(
        analysis.opportunity_count == 2U &&
            !analysis.opportunities[0].runtime_evidence_present &&
            !analysis.opportunities[1].runtime_evidence_present,
        "missing runtime remains absent");
    test_check(
        test_file_contains(
            json_path,
            "\"runtime_profile\":{\"state\":\"unavailable\"") &&
            test_file_contains(json_path, "\"runtime_sample_count\":null"),
        "unavailable profile projected explicitly");
    evo_project_analysis_destroy(&analysis);

    analysis = (evo_project_analysis_t){0};
    context = (test_provider_context_t){0};
    test_check(
        test_path(
            output_path,
            sizeof(output_path),
            temporary_root,
            "analysis-not-configured") &&
            test_path(
                json_path,
                sizeof(json_path),
                output_path,
                "analysis.json"),
        "not-configured paths");
    config = test_analysis_config(baseline, output_path, &context);
    config.runtime_profile_state = EVO_PROJECT_RUNTIME_NOT_CONFIGURED;
    config.runtime_profile_identity = NULL;
    status = evo_project_analyze(&config, &analysis);
    test_check(
        status == EVO_PROJECT_ANALYSIS_SUCCESS,
        "not-configured profile accepted");
    test_check(
        analysis.runtime_record_count == 0U,
        "not-configured profile invents no runtime record");
    test_check(
        test_file_contains(
            json_path,
            "\"runtime_profile\":{\"state\":\"not-configured\","
            "\"identity\":null}"),
        "not-configured profile projected explicitly");
    evo_project_analysis_destroy(&analysis);
}

static void test_preflight_and_provider_failures(
    const char *temporary_root,
    evo_project_baseline_t *baseline)
{
    static const enum test_provider_mode modes[] = {
        TEST_PROVIDER_ERROR,
        TEST_PROVIDER_SCHEMA,
        TEST_PROVIDER_MISSING_REFERENCE,
        TEST_PROVIDER_DUPLICATE_IDENTITY,
        TEST_PROVIDER_GENERATED_SOURCE,
        TEST_PROVIDER_INCOMPLETE,
        TEST_PROVIDER_MALFORMED_ARRAY,
        TEST_PROVIDER_ZERO_RUNTIME,
        TEST_PROVIDER_INVALID_ENUM,
        TEST_PROVIDER_NULL_REFERENCE,
        TEST_PROVIDER_OVERFLOW_COUNT};
    static const evo_project_analysis_status_t expected_statuses[] = {
        EVO_PROJECT_ANALYSIS_ERROR_PROVIDER,
        EVO_PROJECT_ANALYSIS_ERROR_PROVIDER,
        EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE,
        EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE,
        EVO_PROJECT_ANALYSIS_ERROR_UNSUPPORTED_EVIDENCE,
        EVO_PROJECT_ANALYSIS_ERROR_PROVIDER,
        EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE,
        EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE,
        EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE,
        EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE,
        EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT};
    size_t index;

    for (index = 0U; index < sizeof(modes) / sizeof(modes[0]); index += 1U) {
        char output_path[1024];
        char output_name[64];
        evo_project_analysis_t analysis = {0};
        test_provider_context_t context = {0};
        evo_project_analysis_config_t config;
        evo_project_analysis_status_t status;
        const int written = evo_project_format(
            output_name,
            sizeof(output_name),
            "analysis-failure-%u",
            (unsigned int)index);

        test_check(
            written > 0 && (size_t)written < sizeof(output_name) &&
                test_path(
                    output_path,
                    sizeof(output_path),
                    temporary_root,
                    output_name),
            "failure path");
        context.mode = modes[index];
        config = test_analysis_config(baseline, output_path, &context);
        if (context.mode == TEST_PROVIDER_OVERFLOW_COUNT) {
            config.limits.max_source_locations = SIZE_MAX;
        }
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == expected_statuses[index],
            "bad provider rejected with actionable status");
        test_check(
            context.provider_calls == 1U,
            "bad provider called exactly once");
        test_check(access(output_path, F_OK) != 0, "failed output absent");
        test_check(analysis.private_owner == NULL, "failed result reset");
    }

    {
        char output_path[1024];
        evo_project_analysis_t analysis = {0};
        test_provider_context_t context = {0};
        evo_project_analysis_config_t config;
        size_t calls_before;
        evo_project_analysis_status_t status;

        test_check(
            test_path(
                output_path,
                sizeof(output_path),
                temporary_root,
                "analysis-preflight"),
            "preflight path");
        config = test_analysis_config(baseline, output_path, &context);
        config.limits.max_source_locations = 0U;
        calls_before = context.provider_calls;
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT &&
                context.provider_calls == calls_before,
            "invalid limits reject before provider");
        config = test_analysis_config(baseline, output_path, &context);
        config.provider_context = &analysis;
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT &&
                context.provider_calls == calls_before,
            "result alias rejects before provider");
        config = test_analysis_config(baseline, output_path, &context);
        config.provider_context = &config;
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT &&
                context.provider_calls == calls_before,
            "config context alias rejects before provider");
        config = test_analysis_config(baseline, output_path, &context);
        status = evo_project_analyze(
            &config, (evo_project_analysis_t *)(void *)&config);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT &&
                context.provider_calls == calls_before,
            "config result alias rejects before provider");
        config = test_analysis_config(baseline, output_path, &context);
        status = evo_project_analyze(
            &config, (evo_project_analysis_t *)(void *)baseline);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT &&
                context.provider_calls == calls_before,
            "baseline result alias rejects before provider");
        config = test_analysis_config(baseline, output_path, &context);
        config.runtime_profile_state = EVO_PROJECT_RUNTIME_UNAVAILABLE;
        config.runtime_profile_identity = NULL;
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT &&
                context.provider_calls == calls_before,
            "malformed profile rejects before provider");
    }

    {
        char nested_output[1024];
        evo_project_analysis_t analysis = {0};
        test_provider_context_t context = {0};
        evo_project_analysis_config_t config;
        evo_project_analysis_status_t status;

        test_check(
            test_path(
                nested_output,
                sizeof(nested_output),
                baseline->output_path,
                "forbidden-analysis"),
            "nested output path");
        config = test_analysis_config(baseline, nested_output, &context);
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID &&
                context.provider_calls == 0U,
            "baseline output overlap rejects before provider");
    }

    {
        char output_path[1024];
        evo_project_baseline_t ineligible = *baseline;
        evo_project_analysis_t analysis = {0};
        test_provider_context_t context = {0};
        evo_project_analysis_config_t config;
        evo_project_analysis_status_t status;

        test_check(
            test_path(
                output_path,
                sizeof(output_path),
                temporary_root,
                "analysis-ineligible"),
            "ineligible output path");
        ineligible.state = EVO_PROJECT_BASELINE_BUILD_FAILED;
        config = test_analysis_config(&ineligible, output_path, &context);
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_BASELINE_INELIGIBLE &&
                context.provider_calls == 0U,
            "ineligible baseline rejects before provider");
    }

    {
        char output_path[1024];
        evo_project_analysis_t analysis = {0};
        test_provider_context_t context = {0};
        evo_project_analysis_config_t config;
        evo_project_analysis_status_t status;

        test_check(
            test_path(
                output_path,
                sizeof(output_path),
                temporary_root,
                "analysis-existing") &&
                mkdir(output_path, 0700) == 0,
            "pre-existing output fixture");
        config = test_analysis_config(baseline, output_path, &context);
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_OUTPUT_EXISTS &&
                context.provider_calls == 0U,
            "pre-existing output rejects before provider");
    }

    {
        char output_path[1024];
        evo_project_analysis_t analysis = {0};
        test_provider_context_t context = {0};
        evo_project_analysis_config_t config;
        evo_project_analysis_status_t status;

        test_check(
            test_path(
                output_path,
                sizeof(output_path),
                temporary_root,
                "analysis-unit-limit"),
            "unit-limit output path");
        config = test_analysis_config(baseline, output_path, &context);
        config.limits.max_translation_units = 1U;
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT &&
                context.provider_calls == 0U,
            "translation-unit limit rejects before provider");

        config = test_analysis_config(baseline, output_path, &context);
        config.limits.max_evidence_bytes = 1U;
        status = evo_project_analyze(&config, &analysis);
        test_check(
            status == EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT &&
                context.provider_calls == 1U &&
                access(output_path, F_OK) != 0,
            "evidence limit publishes no output");
    }
}

static void test_snapshot_mutation_detection(const char *temporary_root)
{
    char output_path[1024];
    evo_project_baseline_t baseline = {0};
    evo_project_analysis_t analysis = {0};
    test_provider_context_t context = {0};
    evo_project_analysis_config_t config;
    evo_project_analysis_status_t status;

    test_check(
        test_capture_baseline(
            temporary_root, "mutation-baseline", &baseline),
        "mutation baseline capture");
    test_check(
        test_path(
            output_path,
            sizeof(output_path),
            temporary_root,
            "analysis-mutated-snapshot"),
        "mutation output path");
    context.mode = TEST_PROVIDER_MUTATE_SNAPSHOT;
    config = test_analysis_config(&baseline, output_path, &context);
    status = evo_project_analyze(&config, &analysis);
    test_check(
        status == EVO_PROJECT_ANALYSIS_ERROR_BASELINE_CHANGED,
        "snapshot mutation fails closed");
    test_check(access(output_path, F_OK) != 0, "mutated analysis output absent");
    evo_project_analysis_destroy(&analysis);
    evo_project_baseline_destroy(&baseline);

    baseline = (evo_project_baseline_t){0};
    analysis = (evo_project_analysis_t){0};
    context = (test_provider_context_t){0};
    test_check(
        test_capture_baseline(
            temporary_root, "error-mutation-baseline", &baseline),
        "error mutation baseline capture");
    test_check(
        test_path(
            output_path,
            sizeof(output_path),
            temporary_root,
            "analysis-error-mutated-snapshot"),
        "error mutation output path");
    context.mode = TEST_PROVIDER_MUTATE_SNAPSHOT_AND_ERROR;
    config = test_analysis_config(&baseline, output_path, &context);
    status = evo_project_analyze(&config, &analysis);
    test_check(
        status == EVO_PROJECT_ANALYSIS_ERROR_BASELINE_CHANGED,
        "snapshot mutation overrides provider error");
    test_check(
        access(output_path, F_OK) != 0,
        "error-mutated analysis output absent");
    evo_project_analysis_destroy(&analysis);
    evo_project_baseline_destroy(&baseline);

    baseline = (evo_project_baseline_t){0};
    analysis = (evo_project_analysis_t){0};
    context = (test_provider_context_t){0};
    test_check(
        test_capture_baseline(
            temporary_root, "directory-mutation-baseline", &baseline),
        "directory mutation baseline capture");
    test_check(
        test_path(
            output_path,
            sizeof(output_path),
            temporary_root,
            "analysis-mutated-snapshot-directory"),
        "directory mutation output path");
    context.mode = TEST_PROVIDER_MUTATE_SNAPSHOT_DIRECTORY;
    config = test_analysis_config(&baseline, output_path, &context);
    status = evo_project_analyze(&config, &analysis);
    test_check(
        status == EVO_PROJECT_ANALYSIS_ERROR_BASELINE_CHANGED,
        "snapshot directory mutation fails closed");
    test_check(
        access(output_path, F_OK) != 0,
        "directory-mutated analysis output absent");
    evo_project_analysis_destroy(&analysis);
    evo_project_baseline_destroy(&baseline);
}

int main(void)
{
    char temporary_template[] = "/tmp/evo-project-analysis-XXXXXX";
    char *temporary_root = mkdtemp(temporary_template);
    evo_project_baseline_t baseline = {0};

    test_check(temporary_root != NULL, "create temporary root");
    if (temporary_root == NULL) {
        return EXIT_FAILURE;
    }
    test_check(
        test_capture_baseline(temporary_root, "baseline", &baseline),
        "capture eligible baseline");
    if (baseline.private_owner != NULL) {
        test_success_and_replay(temporary_root, &baseline);
        test_unavailable_profile(temporary_root, &baseline);
        test_preflight_and_provider_failures(temporary_root, &baseline);
    }
    evo_project_baseline_destroy(&baseline);
    test_snapshot_mutation_detection(temporary_root);
    return test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
