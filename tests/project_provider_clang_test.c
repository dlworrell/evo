#define _XOPEN_SOURCE 700

#include "internal/project_provider.h"
#include "internal/project_provider_clang.h"
#include "internal/project_provider_sandbox.h"
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

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project clang provider test failure: %s\n", message);
        failures += 1;
    }
}

static bool write_text(const char *path, const char *text)
{
    int descriptor;
    size_t size = strlen(text);
    size_t position = 0U;

    descriptor = open(
        path,
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        (mode_t)0600);
    if (descriptor < 0) {
        return false;
    }
    while (position < size) {
        const ssize_t written = write(descriptor, text + position, size - position);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            (void)close(descriptor);
            return false;
        }
        position += (size_t)written;
    }
    return close(descriptor) == 0;
}

static evo_project_analysis_limits_t analysis_limits(void)
{
    return (evo_project_analysis_limits_t){
        .max_string_bytes = 1024U,
        .max_path_bytes = 1024U,
        .max_translation_units = 4U,
        .max_source_locations = 32U,
        .max_declarations = 32U,
        .max_calls = 32U,
        .max_control_flows = 32U,
        .max_data_flows = 32U,
        .max_optimization_records = 32U,
        .max_runtime_records = 32U,
        .max_opportunities = 32U,
        .max_evidence_bytes = 1048576U,
    };
}

static const evo_project_declaration_record_t *find_declaration(
    const evo_project_analysis_provider_result_t *result,
    const char *name)
{
    size_t index;

    for (index = 0U; index < result->declaration_count; index += 1U) {
        if (strcmp(result->declarations[index].name, name) == 0) {
            return &result->declarations[index];
        }
    }
    return NULL;
}

int main(void)
{
#if !defined(__linux__)
    (void)printf("real Clang provider fixture is Linux-only for the v1 sandbox\n");
    return 77;
#else
    static const char source[] =
        "static int helper(int value)\n"
        "{\n"
        "    return value + 1;\n"
        "}\n"
        "\n"
        "int main(void)\n"
        "{\n"
        "    return helper(4) == 5 ? 0 : 1;\n"
        "}\n";
    static const char *const compile_arguments[] = {
        "cc", "-std=c17", "-c", "main.c"};
    static const char *const unsafe_arguments[] = {
        "cc", "@response.rsp", "-c", "main.c"};
    char template_path[] = "/tmp/evo-clang-provider-XXXXXX";
    char source_path[512];
    char first_identity[1024];
    char first_location[1024];
    char *workspace;
    evo_project_compilation_record_t unit = {
        .directory = ".",
        .file = "main.c",
        .output = NULL,
        .command_form = EVO_PROJECT_COMPILE_COMMAND_ARGUMENTS,
        .argument_count = sizeof(compile_arguments) / sizeof(compile_arguments[0]),
        .arguments = compile_arguments,
        .command = NULL,
    };
    evo_project_analysis_request_t request = {
        .schema_version = EVO_PROJECT_ANALYSIS_SCHEMA_VERSION,
        .baseline_fingerprint = "evo-fnv1a64:0000000000000000",
        .snapshot_path = NULL,
        .compilation_unit_count = 1U,
        .compilation_units = &unit,
        .provider_identity = EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID,
        .clang_identity = "clang-system-v1",
        .llvm_identity = "llvm-system-v1",
        .target_identity = "portable-posix-c17",
        .flags_identity = "provider-fixture-flags-v1",
        .runtime_profile_state = EVO_PROJECT_RUNTIME_NOT_CONFIGURED,
        .runtime_profile_identity = NULL,
        .limits = {0},
        .timeout_ms = 10000U,
        .max_memory_bytes = 536870912U,
        .max_processes = 8U,
        .max_storage_bytes = 1048576U,
        .max_output_bytes = 1048576U,
        .network_access = false,
    };
    evo_project_clang_analysis_context_t context = {0};
    evo_project_analysis_provider_result_t result = {0};
    evo_project_analysis_status_t status;
    const evo_project_declaration_record_t *helper;
    const evo_project_declaration_record_t *main_function;
    int path_written;

    if (!evo_project_sandbox_available() ||
        !evo_project_provider_available(
            evo_project_provider_find(EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID))) {
        (void)printf("real Clang/Bubblewrap provider unavailable\n");
        return 77;
    }
    workspace = mkdtemp(template_path);
    if (workspace == NULL) {
        (void)fprintf(stderr, "mkdtemp failed: %s\n", strerror(errno));
        return 1;
    }
    path_written = evo_project_format(
        source_path, sizeof(source_path), "%s/main.c", workspace);
    if (path_written <= 0 || (size_t)path_written >= sizeof(source_path) ||
        !write_text(source_path, source)) {
        (void)fprintf(stderr, "unable to create Clang fixture\n");
        (void)rmdir(workspace);
        return 1;
    }
    request.snapshot_path = workspace;
    request.limits = analysis_limits();

    status = evo_project_clang_analysis_provider(&request, &context, &result);
    check(status == EVO_PROJECT_ANALYSIS_SUCCESS, "real provider succeeds");
    check(result.completed, "provider completes");
    check(result.source_location_count >= 2U, "source locations emitted");
    check(result.declaration_count >= 2U, "declarations emitted");
    helper = find_declaration(&result, "helper");
    main_function = find_declaration(&result, "main");
    check(helper != NULL, "helper declaration found");
    check(main_function != NULL, "main declaration found");
    if (helper != NULL) {
        check(helper->definition, "helper is a definition");
        check(
            helper->linkage == EVO_PROJECT_LINKAGE_INTERNAL,
            "static helper has internal linkage");
    }
    if (main_function != NULL) {
        check(main_function->definition, "main is a definition");
        check(
            main_function->linkage == EVO_PROJECT_LINKAGE_EXTERNAL,
            "main has external linkage");
        check(
            strstr(main_function->identity, "0x") == NULL,
            "runtime Clang pointer identity excluded");
        (void)evo_project_format(
            first_identity, sizeof(first_identity), "%s", main_function->identity);
        (void)evo_project_format(
            first_location,
            sizeof(first_location),
            "%s",
            main_function->location_identity);
    } else {
        first_identity[0] = '\0';
        first_location[0] = '\0';
    }

    result = (evo_project_analysis_provider_result_t){0};
    status = evo_project_clang_analysis_provider(&request, &context, &result);
    check(status == EVO_PROJECT_ANALYSIS_SUCCESS, "repeated provider succeeds");
    main_function = find_declaration(&result, "main");
    check(main_function != NULL, "main found on replay");
    if (main_function != NULL) {
        check(
            strcmp(first_identity, main_function->identity) == 0,
            "declaration identity stable across replay");
        check(
            strcmp(first_location, main_function->location_identity) == 0,
            "location identity stable across replay");
    }

    unit.arguments = unsafe_arguments;
    unit.argument_count = sizeof(unsafe_arguments) / sizeof(unsafe_arguments[0]);
    result = (evo_project_analysis_provider_result_t){0};
    status = evo_project_clang_analysis_provider(&request, &context, &result);
    check(
        status == EVO_PROJECT_ANALYSIS_ERROR_UNSUPPORTED_EVIDENCE,
        "response-file compiler arguments fail closed");

    unit.command_form = EVO_PROJECT_COMPILE_COMMAND_SHELL;
    unit.arguments = NULL;
    unit.argument_count = 0U;
    unit.command = "cc -std=c17 -c main.c";
    status = evo_project_clang_analysis_provider(&request, &context, &result);
    check(
        status == EVO_PROJECT_ANALYSIS_ERROR_UNSUPPORTED_EVIDENCE,
        "shell-form compilation record fails closed");

    evo_project_clang_analysis_context_destroy(&context);
    check(unlink(source_path) == 0, "fixture source cleanup");
    check(rmdir(workspace) == 0, "fixture workspace cleanup");

    if (failures != 0) {
        (void)fprintf(stderr, "%d Clang provider tests failed\n", failures);
        return 1;
    }
    (void)printf("real Clang analysis provider tests passed\n");
    return 0;
#endif
}
