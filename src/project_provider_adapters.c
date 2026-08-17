#define _POSIX_C_SOURCE 200809L

#include "internal/project_provider_adapters.h"

#include "internal/project_fingerprint.h"

#include <string.h>
#include <sys/stat.h>

static void evo_provider_combined_fingerprint(
    const evo_project_sandbox_result_t *sandbox,
    uint64_t *fingerprint_out)
{
    evo_project_fingerprint_t fingerprint;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)sandbox->stdout_bytes);
    evo_project_fingerprint_u64(&fingerprint, sandbox->stdout_fingerprint);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)sandbox->stderr_bytes);
    evo_project_fingerprint_u64(&fingerprint, sandbox->stderr_fingerprint);
    *fingerprint_out = fingerprint.value;
}

static evo_project_sandbox_limits_t evo_provider_command_limits(
    const evo_project_command_view_t *command)
{
    return (evo_project_sandbox_limits_t){
        .cpu_time_ms = command->timeout_ms,
        .address_space_bytes = command->max_memory_bytes,
        .descendant_process_count = command->max_processes,
        .storage_bytes = command->max_storage_bytes,
        .output_bytes = command->max_output_bytes,
        .wall_timeout_ms = command->timeout_ms,
        .network_access = command->network_access,
    };
}

evo_project_status_t evo_project_sandbox_command_runner(
    const evo_project_command_view_t *command,
    const char *workspace_path,
    void *opaque,
    evo_project_command_outcome_t *outcome)
{
    const evo_project_sandbox_command_context_t *context = opaque;
    evo_project_sandbox_result_t sandbox = {0};
    evo_project_sandbox_command_t sandbox_command;
    evo_project_sandbox_status_t status;

    if (command == NULL || workspace_path == NULL || outcome == NULL ||
        context == NULL) {
        return EVO_PROJECT_ERROR_EXECUTION_PROVIDER;
    }
    sandbox_command = (evo_project_sandbox_command_t){
        .schema_version = EVO_PROJECT_SANDBOX_SCHEMA_VERSION,
        .workspace_path = workspace_path,
        .working_directory = ".",
        .argument_count = command->argument_count,
        .arguments = command->arguments,
        .environment_count = context->environment_count,
        .environment = context->environment,
        .limits = evo_provider_command_limits(command),
    };
    status = evo_project_sandbox_run(&sandbox_command, &sandbox);
    if (status == EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    if (status != EVO_PROJECT_SANDBOX_SUCCESS) {
        return EVO_PROJECT_ERROR_EXECUTION_PROVIDER;
    }
    *outcome = (evo_project_command_outcome_t){0};
    outcome->schema_version = EVO_PROJECT_BASELINE_SCHEMA_VERSION;
    outcome->completed = true;
    outcome->timed_out = sandbox.timed_out;
    outcome->exit_code =
        sandbox.completed ? sandbox.exit_code : (sandbox.signaled ? 128 + sandbox.signal_number : -1);
    if (sandbox.stdout_bytes <= SIZE_MAX - sandbox.stderr_bytes) {
        outcome->output_bytes = sandbox.stdout_bytes + sandbox.stderr_bytes;
    } else {
        evo_project_sandbox_result_destroy(&sandbox);
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    evo_provider_combined_fingerprint(&sandbox, &outcome->output_fingerprint);
    evo_project_sandbox_result_destroy(&sandbox);
    return EVO_PROJECT_SUCCESS;
}

static uint64_t evo_provider_text_fingerprint(const char *identity)
{
    evo_project_fingerprint_t fingerprint;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, identity == NULL ? "unknown-toolchain" : identity);
    return fingerprint.value;
}

static size_t evo_provider_diagnostic_bytes(
    const char *diagnostic,
    size_t available,
    size_t maximum)
{
    size_t count = available < maximum ? available : maximum;

    while (count > 0U && diagnostic[count - 1U] == '\0') {
        count -= 1U;
    }
    return count;
}

evo_project_assurance_status_t evo_project_sandbox_assurance_runner(
    const evo_project_assurance_gate_view_t *gate,
    const char *candidate_workspace_path,
    void *opaque,
    evo_project_assurance_gate_outcome_t *outcome)
{
    evo_project_sandbox_assurance_context_t *context = opaque;
    evo_project_sandbox_command_t command;
    evo_project_sandbox_status_t status;
    size_t diagnostic_bytes;

    if (gate == NULL || candidate_workspace_path == NULL || context == NULL ||
        outcome == NULL || context->toolchain_identity == NULL ||
        context->max_diagnostic_bytes == 0U || !gate->workspace_only_filesystem ||
        gate->shell_interpretation) {
        return EVO_PROJECT_ASSURANCE_ERROR_EXECUTION_PROVIDER;
    }
    evo_project_sandbox_result_destroy(&context->last_result);
    command = (evo_project_sandbox_command_t){
        .schema_version = EVO_PROJECT_SANDBOX_SCHEMA_VERSION,
        .workspace_path = candidate_workspace_path,
        .working_directory = gate->working_directory,
        .argument_count = gate->argument_count,
        .arguments = gate->arguments,
        .environment_count = gate->environment_count,
        .environment = gate->environment,
        .limits = {
            .cpu_time_ms = gate->timeout_ms,
            .address_space_bytes = gate->max_memory_bytes,
            .descendant_process_count = gate->max_processes,
            .storage_bytes = gate->max_storage_bytes,
            .output_bytes = gate->max_output_bytes,
            .wall_timeout_ms = gate->timeout_ms,
            .network_access = gate->network_access,
        },
    };
    status = evo_project_sandbox_run(&command, &context->last_result);
    if (status == EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY) {
        return EVO_PROJECT_ASSURANCE_ERROR_OUT_OF_MEMORY;
    }
    if (status != EVO_PROJECT_SANDBOX_SUCCESS) {
        return EVO_PROJECT_ASSURANCE_ERROR_EXECUTION_PROVIDER;
    }
    diagnostic_bytes = evo_provider_diagnostic_bytes(
        context->last_result.stderr_text == NULL ? "" : context->last_result.stderr_text,
        context->last_result.stderr_bytes,
        context->max_diagnostic_bytes);
    *outcome = (evo_project_assurance_gate_outcome_t){0};
    outcome->schema_version = EVO_PROJECT_ASSURANCE_SCHEMA_VERSION;
    outcome->completed = true;
    outcome->available = true;
    outcome->timed_out = context->last_result.timed_out;
    outcome->signaled = context->last_result.signaled;
    outcome->resource_exhausted =
        context->last_result.resource_exhausted && !context->last_result.timed_out &&
        !context->last_result.signaled;
    outcome->filesystem_policy_enforced =
        context->last_result.filesystem_isolation_enforced;
    outcome->network_policy_enforced =
        gate->network_access || context->last_result.network_isolation_enforced;
    outcome->process_group_clean =
        context->last_result.descendant_cleanup_enforced;
    outcome->source_modified = false;
    outcome->snapshot_modified = false;
    outcome->exit_code = context->last_result.completed
                             ? context->last_result.exit_code
                             : -1;
    outcome->signal_number = context->last_result.signaled
                                 ? context->last_result.signal_number
                                 : 0;
    outcome->stdout_bytes = context->last_result.stdout_bytes;
    outcome->stdout_fingerprint = context->last_result.stdout_fingerprint;
    outcome->stderr_bytes = context->last_result.stderr_bytes;
    outcome->stderr_fingerprint = context->last_result.stderr_fingerprint;
    outcome->toolchain_fingerprint =
        evo_provider_text_fingerprint(context->toolchain_identity);
    outcome->diagnostic_excerpt = diagnostic_bytes == 0U
                                      ? NULL
                                      : context->last_result.stderr_text;
    outcome->diagnostic_excerpt_bytes = diagnostic_bytes;
    return EVO_PROJECT_ASSURANCE_SUCCESS;
}

void evo_project_sandbox_assurance_context_destroy(
    evo_project_sandbox_assurance_context_t *context)
{
    if (context == NULL) {
        return;
    }
    evo_project_sandbox_result_destroy(&context->last_result);
}

static const evo_project_measurement_command_t *evo_provider_workload(
    const evo_project_sandbox_measurement_context_t *context,
    const char *workload_id)
{
    size_t index;

    if (context == NULL || workload_id == NULL ||
        (context->workload_count > 0U && context->workloads == NULL)) {
        return NULL;
    }
    for (index = 0U; index < context->workload_count; index += 1U) {
        if (context->workloads[index].workload_id != NULL &&
            strcmp(context->workloads[index].workload_id, workload_id) == 0) {
            return &context->workloads[index];
        }
    }
    return NULL;
}

static bool evo_provider_binary_size(
    const char *workspace,
    const char *binary_path,
    uint64_t *size_out)
{
    char path[4096];
    struct stat metadata;
    int written;

    if (workspace == NULL || binary_path == NULL || size_out == NULL ||
        binary_path[0] == '\0') {
        return false;
    }
    if (binary_path[0] == '/') {
        written = snprintf(path, sizeof(path), "%s", binary_path);
    } else {
        written = snprintf(path, sizeof(path), "%s/%s", workspace, binary_path);
    }
    if (written <= 0 || (size_t)written >= sizeof(path) ||
        stat(path, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        metadata.st_size <= 0) {
        return false;
    }
    *size_out = (uint64_t)metadata.st_size;
    return true;
}

evo_project_measurement_status_t evo_project_sandbox_measurement_provider(
    const evo_project_measurement_request_t *request,
    void *opaque,
    evo_project_measurement_outcome_t *outcome)
{
    const evo_project_sandbox_measurement_context_t *context = opaque;
    const evo_project_measurement_command_t *workload;
    const char *workspace;
    const char *binary_path;
    const char *const *arguments;
    size_t argument_count;
    evo_project_sandbox_command_t command;
    evo_project_sandbox_result_t sandbox = {0};
    evo_project_sandbox_status_t status;
    uint64_t binary_size = 0U;

    if (request == NULL || outcome == NULL ||
        request->schema_version != EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION) {
        return EVO_PROJECT_MEASUREMENT_ERROR_PROVIDER;
    }
    workload = evo_provider_workload(context, request->workload_id);
    if (workload == NULL || request->timeout_ms == 0U ||
        workload->max_memory_bytes == 0U || workload->max_processes == 0U ||
        workload->max_storage_bytes == 0U || workload->max_output_bytes == 0U ||
        workload->reliability_ppm > EVO_PROJECT_MEASUREMENT_PPM_SCALE ||
        workload->maintainability_ppm > EVO_PROJECT_MEASUREMENT_PPM_SCALE) {
        return EVO_PROJECT_MEASUREMENT_ERROR_PROVIDER;
    }
    if (request->subject == EVO_PROJECT_MEASUREMENT_BASELINE) {
        workspace = workload->baseline_workspace_path;
        binary_path = workload->baseline_binary_path;
        arguments = workload->baseline_arguments;
        argument_count = workload->baseline_argument_count;
    } else if (request->subject == EVO_PROJECT_MEASUREMENT_CANDIDATE) {
        workspace = workload->candidate_workspace_path;
        binary_path = workload->candidate_binary_path;
        arguments = workload->candidate_arguments;
        argument_count = workload->candidate_argument_count;
    } else {
        return EVO_PROJECT_MEASUREMENT_ERROR_PROVIDER;
    }
    if (workspace == NULL || arguments == NULL || argument_count == 0U ||
        !evo_provider_binary_size(workspace, binary_path, &binary_size)) {
        return EVO_PROJECT_MEASUREMENT_ERROR_PROVIDER;
    }
    command = (evo_project_sandbox_command_t){
        .schema_version = EVO_PROJECT_SANDBOX_SCHEMA_VERSION,
        .workspace_path = workspace,
        .working_directory = workload->working_directory,
        .argument_count = argument_count,
        .arguments = arguments,
        .environment_count = workload->environment_count,
        .environment = workload->environment,
        .limits = {
            .cpu_time_ms = request->timeout_ms,
            .address_space_bytes = workload->max_memory_bytes,
            .descendant_process_count = workload->max_processes,
            .storage_bytes = workload->max_storage_bytes,
            .output_bytes = workload->max_output_bytes,
            .wall_timeout_ms = request->timeout_ms,
            .network_access = workload->network_access,
        },
    };
    status = evo_project_sandbox_run(&command, &sandbox);
    if (status == EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY) {
        return EVO_PROJECT_MEASUREMENT_ERROR_OUT_OF_MEMORY;
    }
    if (status != EVO_PROJECT_SANDBOX_SUCCESS) {
        return EVO_PROJECT_MEASUREMENT_ERROR_PROVIDER;
    }
    *outcome = (evo_project_measurement_outcome_t){0};
    outcome->schema_version = EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION;
    outcome->condition_fingerprint = request->expected_condition_fingerprint;
    outcome->runtime_ns = sandbox.elapsed_ns;
    outcome->peak_memory_bytes = 0U;
    outcome->binary_size_bytes = binary_size;
    outcome->reliability_ppm = workload->reliability_ppm;
    outcome->maintainability_ppm = workload->maintainability_ppm;
    if (sandbox.completed && sandbox.exit_code == 0 && !sandbox.resource_exhausted &&
        !sandbox.signaled && sandbox.elapsed_ns > 0U) {
        outcome->completed = true;
    } else if (sandbox.timed_out) {
        outcome->timed_out = true;
    } else {
        outcome->failed = true;
    }
    evo_project_sandbox_result_destroy(&sandbox);
    return EVO_PROJECT_MEASUREMENT_SUCCESS;
}
