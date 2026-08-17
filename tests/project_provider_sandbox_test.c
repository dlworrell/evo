#define _POSIX_C_SOURCE 200809L

#include "internal/project_provider_sandbox.h"

#include <errno.h>
#include <stdbool.h>
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
        (void)fprintf(stderr, "project sandbox test failure: %s\n", message);
        failures += 1;
    }
}

static evo_project_sandbox_limits_t limits(void)
{
    return (evo_project_sandbox_limits_t){
        .cpu_time_ms = 2000U,
        .address_space_bytes = 268435456U,
        .descendant_process_count = 8U,
        .storage_bytes = 1048576U,
        .output_bytes = 65536U,
        .wall_timeout_ms = 2000U,
        .network_access = false,
    };
}

static evo_project_sandbox_status_t run_command(
    const char *workspace,
    size_t argument_count,
    const char *const *arguments,
    evo_project_sandbox_limits_t command_limits,
    evo_project_sandbox_result_t *result)
{
    static const char *const environment[] = {"LANG=C", "LC_ALL=C"};
    const evo_project_sandbox_command_t command = {
        .schema_version = EVO_PROJECT_SANDBOX_SCHEMA_VERSION,
        .workspace_path = workspace,
        .working_directory = ".",
        .argument_count = argument_count,
        .arguments = arguments,
        .environment_count = sizeof(environment) / sizeof(environment[0]),
        .environment = environment,
        .limits = command_limits,
    };

    return evo_project_sandbox_run(&command, result);
}

int main(void)
{
#if !defined(__linux__)
    (void)printf("project sandbox provider is Linux-only\n");
    return 77;
#else
    char template_path[] = "/tmp/evo-provider-sandbox-XXXXXX";
    char *workspace = mkdtemp(template_path);
    char host_tmp_path[256];
    const char *literal_arguments[] = {
        "/usr/bin/printf", "%s", "literal;$(touch /tmp/should-not-run)"};
    const char *tmp_arguments[] = {
        "/usr/bin/touch", "/tmp/evo-provider-private-tmp-marker"};
    const char *timeout_arguments[] = {"/usr/bin/sleep", "2"};
    const char *output_arguments[] = {"/usr/bin/yes", "bounded"};
    const char *storage_arguments[] = {
        "/usr/bin/dd", "if=/dev/zero", "of=large.bin", "bs=4096", "count=1024"};
    evo_project_sandbox_result_t result = {0};
    evo_project_sandbox_limits_t command_limits;
    int written;

    if (!evo_project_sandbox_available()) {
        (void)printf("project sandbox provider unavailable\n");
        return 77;
    }
    if (workspace == NULL) {
        (void)fprintf(stderr, "unable to create sandbox workspace: %s\n", strerror(errno));
        return 1;
    }

    command_limits = limits();
    check(
        run_command(
            workspace,
            sizeof(literal_arguments) / sizeof(literal_arguments[0]),
            literal_arguments,
            command_limits,
            &result) == EVO_PROJECT_SANDBOX_SUCCESS,
        "literal argv execution status");
    check(result.completed && result.exit_code == 0, "literal argv execution");
    check(
        result.stdout_text != NULL &&
            strcmp(result.stdout_text, "literal;$(touch /tmp/should-not-run)") == 0,
        "shell metacharacters remain literal");
    check(result.filesystem_isolation_enforced, "filesystem isolation evidence");
    check(result.network_isolation_enforced, "network isolation evidence");
    check(result.descendant_cleanup_enforced, "cleanup evidence");
    evo_project_sandbox_result_destroy(&result);

    written = snprintf(
        host_tmp_path,
        sizeof(host_tmp_path),
        "/tmp/evo-provider-private-tmp-marker");
    check(written > 0 && (size_t)written < sizeof(host_tmp_path), "host tmp path");
    (void)unlink(host_tmp_path);
    check(
        run_command(
            workspace,
            sizeof(tmp_arguments) / sizeof(tmp_arguments[0]),
            tmp_arguments,
            command_limits,
            &result) == EVO_PROJECT_SANDBOX_SUCCESS,
        "private tmp execution status");
    check(result.completed && result.exit_code == 0, "private tmp command");
    check(access(host_tmp_path, F_OK) != 0, "sandbox tmp does not escape to host");
    evo_project_sandbox_result_destroy(&result);

    command_limits = limits();
    command_limits.wall_timeout_ms = 50U;
    check(
        run_command(
            workspace,
            sizeof(timeout_arguments) / sizeof(timeout_arguments[0]),
            timeout_arguments,
            command_limits,
            &result) == EVO_PROJECT_SANDBOX_SUCCESS,
        "timeout execution status");
    check(result.timed_out, "wall timeout enforced");
    check(
        result.exhausted_resource == EVO_PROJECT_SANDBOX_RESOURCE_WALL_TIME,
        "timeout classification");
    check(result.descendant_cleanup_enforced, "timeout cleanup");
    evo_project_sandbox_result_destroy(&result);

    command_limits = limits();
    command_limits.output_bytes = 1024U;
    check(
        run_command(
            workspace,
            sizeof(output_arguments) / sizeof(output_arguments[0]),
            output_arguments,
            command_limits,
            &result) == EVO_PROJECT_SANDBOX_SUCCESS,
        "output-limit execution status");
    check(result.resource_exhausted, "output limit exhausted");
    check(
        result.exhausted_resource == EVO_PROJECT_SANDBOX_RESOURCE_OUTPUT,
        "output limit classification");
    check(result.descendant_cleanup_enforced, "output cleanup");
    evo_project_sandbox_result_destroy(&result);

    command_limits = limits();
    command_limits.storage_bytes = 65536U;
    check(
        run_command(
            workspace,
            sizeof(storage_arguments) / sizeof(storage_arguments[0]),
            storage_arguments,
            command_limits,
            &result) == EVO_PROJECT_SANDBOX_SUCCESS,
        "storage-limit execution status");
    check(result.resource_exhausted, "storage limit exhausted");
    check(
        result.exhausted_resource == EVO_PROJECT_SANDBOX_RESOURCE_STORAGE,
        "storage limit classification");
    check(result.descendant_cleanup_enforced, "storage cleanup");
    evo_project_sandbox_result_destroy(&result);

    (void)unlink("/tmp/evo-provider-private-tmp-marker");
    {
        char large_path[512];
        const int large_written = snprintf(
            large_path, sizeof(large_path), "%s/large.bin", workspace);

        if (large_written > 0 && (size_t)large_written < sizeof(large_path)) {
            (void)unlink(large_path);
        }
    }
    check(rmdir(workspace) == 0, "workspace cleanup");

    if (failures != 0) {
        (void)fprintf(stderr, "%d project sandbox tests failed\n", failures);
        return 1;
    }
    (void)printf("project sandbox provider tests passed\n");
    return 0;
#endif
}
