#include "internal/project_command.h"

#include <string.h>

#define EVO_PROJECT_COMMAND_EXECUTION_CAPABILITIES \
    (EVO_PROJECT_COMMAND_CAPABILITY_DIRECT_ARGV | \
     EVO_PROJECT_COMMAND_CAPABILITY_CPU_LIMIT | \
     EVO_PROJECT_COMMAND_CAPABILITY_ADDRESS_SPACE_LIMIT | \
     EVO_PROJECT_COMMAND_CAPABILITY_PROCESS_LIMIT | \
     EVO_PROJECT_COMMAND_CAPABILITY_STORAGE_LIMIT | \
     EVO_PROJECT_COMMAND_CAPABILITY_OUTPUT_LIMIT | \
     EVO_PROJECT_COMMAND_CAPABILITY_WALL_TIMEOUT | \
     EVO_PROJECT_COMMAND_CAPABILITY_FILESYSTEM_ISOLATION | \
     EVO_PROJECT_COMMAND_CAPABILITY_NETWORK_ISOLATION | \
     EVO_PROJECT_COMMAND_CAPABILITY_DESCENDANT_CLEANUP | \
     EVO_PROJECT_COMMAND_CAPABILITY_MEASUREMENT)

static const evo_project_command_descriptor_t evo_project_commands[] = {
    {EVO_PROJECT_COMMAND_SCHEMA_VERSION,
     EVO_PROJECT_COMMAND_ANALYZE,
     "analyze",
     "catalyst.evo.command.analyze.v1",
     "analyze --manifest <file> --input <project> --output <directory>",
     true,
     true,
     false,
     false},
    {EVO_PROJECT_COMMAND_SCHEMA_VERSION,
     EVO_PROJECT_COMMAND_EVOLVE,
     "evolve",
     "catalyst.evo.command.evolve.v1",
     "evolve --manifest <file> --input <project> --evidence <analysis> --output <directory> [--checkpoint <file>]",
     true,
     true,
     true,
     true},
    {EVO_PROJECT_COMMAND_SCHEMA_VERSION,
     EVO_PROJECT_COMMAND_REPLAY,
     "replay",
     "catalyst.evo.command.replay.v1",
     "replay --manifest <file> --input <project> --evidence <recorded> --output <directory> [--checkpoint <file>]",
     true,
     true,
     true,
     true},
    {EVO_PROJECT_COMMAND_SCHEMA_VERSION,
     EVO_PROJECT_COMMAND_REPORT,
     "report",
     "catalyst.evo.command.report.v1",
     "report --evidence <recorded> --output <path>",
     false,
     false,
     true,
     false}};

static const evo_project_command_provider_requirement_t evo_project_provider_requirements[] = {
    {EVO_PROJECT_COMMAND_PROVIDER_CLANG_ANALYSIS_ID,
     EVO_PROJECT_COMMAND_PROVIDER_IMPLEMENTATION_VERSION,
     EVO_PROJECT_COMMAND_CAPABILITY_CLANG_AST |
         EVO_PROJECT_COMMAND_CAPABILITY_COMPILATION_DATABASE |
         EVO_PROJECT_COMMAND_CAPABILITY_DIRECT_ARGV |
         EVO_PROJECT_COMMAND_CAPABILITY_FILESYSTEM_ISOLATION |
         EVO_PROJECT_COMMAND_CAPABILITY_NETWORK_ISOLATION |
         EVO_PROJECT_COMMAND_CAPABILITY_DESCENDANT_CLEANUP,
     false},
    {EVO_PROJECT_COMMAND_PROVIDER_CLANG_AST_ID,
     EVO_PROJECT_COMMAND_PROVIDER_IMPLEMENTATION_VERSION,
     EVO_PROJECT_COMMAND_CAPABILITY_CLANG_AST |
         EVO_PROJECT_COMMAND_CAPABILITY_DIRECT_ARGV |
         EVO_PROJECT_COMMAND_CAPABILITY_FILESYSTEM_ISOLATION |
         EVO_PROJECT_COMMAND_CAPABILITY_NETWORK_ISOLATION |
         EVO_PROJECT_COMMAND_CAPABILITY_DESCENDANT_CLEANUP,
     false},
    {EVO_PROJECT_COMMAND_PROVIDER_LINUX_BWRAP_ID,
     EVO_PROJECT_COMMAND_PROVIDER_IMPLEMENTATION_VERSION,
     EVO_PROJECT_COMMAND_EXECUTION_CAPABILITIES,
     false},
    {EVO_PROJECT_COMMAND_PROVIDER_LOCAL_EVALUATION_ID,
     EVO_PROJECT_COMMAND_PROVIDER_IMPLEMENTATION_VERSION,
     EVO_PROJECT_COMMAND_EXECUTION_CAPABILITIES |
         EVO_PROJECT_COMMAND_CAPABILITY_CLANG_AST |
         EVO_PROJECT_COMMAND_CAPABILITY_COMPILATION_DATABASE |
         EVO_PROJECT_COMMAND_CAPABILITY_ASYNC_START_POLL_CANCEL_JOIN,
     false}};

static bool evo_project_command_operation_valid(
    evo_project_command_operation_t operation)
{
    return operation >= EVO_PROJECT_COMMAND_ANALYZE &&
           operation < EVO_PROJECT_COMMAND_COUNT;
}

static bool evo_project_command_path_valid(const char *path)
{
    size_t index;

    if (path == NULL || path[0] == '\0') {
        return false;
    }
    for (index = 0U; path[index] != '\0'; index += 1U) {
        if (path[index] == '\n' || path[index] == '\r') {
            return false;
        }
    }
    return true;
}

static bool evo_project_command_manifest_valid(
    const evo_project_manifest_t *manifest)
{
    return manifest != NULL && manifest->schema != NULL &&
           manifest->manifest_id != NULL && manifest->source_identity != NULL &&
           manifest->build_frontend != NULL && manifest->language != NULL &&
           manifest->budget.max_path_bytes > 0U &&
           manifest->budget.max_evidence_bytes > 0U &&
           manifest->budget.command_timeout_ms > 0U &&
           manifest->budget.max_processes > 0U;
}

static bool evo_project_command_provider_required(
    evo_project_command_operation_t operation,
    evo_project_command_provider_slot_t slot)
{
    switch (operation) {
    case EVO_PROJECT_COMMAND_ANALYZE:
        return slot == EVO_PROJECT_COMMAND_PROVIDER_ANALYSIS ||
               slot == EVO_PROJECT_COMMAND_PROVIDER_EXECUTION;
    case EVO_PROJECT_COMMAND_EVOLVE:
    case EVO_PROJECT_COMMAND_REPLAY:
        return true;
    case EVO_PROJECT_COMMAND_REPORT:
        return false;
    default:
        return false;
    }
}

static evo_project_command_status_t evo_project_command_validate_provider(
    const evo_project_command_provider_requirement_t *requirement,
    const evo_project_command_provider_selection_t *selection)
{
    if (requirement == NULL || selection == NULL) {
        return EVO_PROJECT_COMMAND_ERROR_INVALID_ARGUMENT;
    }
    if (selection->identity == NULL ||
        strcmp(selection->identity, requirement->identity) != 0) {
        return EVO_PROJECT_COMMAND_ERROR_PROVIDER_IDENTITY;
    }
    if (selection->implementation_version != requirement->implementation_version) {
        return EVO_PROJECT_COMMAND_ERROR_PROVIDER_VERSION;
    }
    if (!selection->available) {
        return EVO_PROJECT_COMMAND_ERROR_PROVIDER_UNAVAILABLE;
    }
    if ((selection->capabilities & requirement->required_capabilities) !=
        requirement->required_capabilities) {
        return EVO_PROJECT_COMMAND_ERROR_PROVIDER_CAPABILITY;
    }
    return EVO_PROJECT_COMMAND_SUCCESS;
}

size_t evo_project_command_registry_count(void)
{
    return sizeof(evo_project_commands) / sizeof(evo_project_commands[0]);
}

const evo_project_command_descriptor_t *evo_project_command_registry_at(
    size_t index)
{
    if (index >= evo_project_command_registry_count()) {
        return NULL;
    }
    return &evo_project_commands[index];
}

const evo_project_command_descriptor_t *evo_project_command_find(
    const char *name)
{
    size_t index;

    if (name == NULL) {
        return NULL;
    }
    for (index = 0U; index < evo_project_command_registry_count(); index += 1U) {
        if (strcmp(evo_project_commands[index].name, name) == 0) {
            return &evo_project_commands[index];
        }
    }
    return NULL;
}

const evo_project_command_provider_requirement_t *evo_project_command_provider_requirement(
    evo_project_command_provider_slot_t slot)
{
    if (slot < EVO_PROJECT_COMMAND_PROVIDER_ANALYSIS ||
        slot >= EVO_PROJECT_COMMAND_PROVIDER_COUNT) {
        return NULL;
    }
    return &evo_project_provider_requirements[slot];
}

evo_project_command_status_t evo_project_command_plan_build(
    const evo_project_command_request_t *request,
    evo_project_command_plan_t *plan)
{
    const evo_project_command_descriptor_t *descriptor;
    size_t slot;

    if (request == NULL || plan == NULL) {
        return EVO_PROJECT_COMMAND_ERROR_INVALID_ARGUMENT;
    }
    *plan = (evo_project_command_plan_t){0};

    if (request->schema_version != EVO_PROJECT_COMMAND_SCHEMA_VERSION) {
        return EVO_PROJECT_COMMAND_ERROR_SCHEMA;
    }
    if (!evo_project_command_operation_valid(request->operation)) {
        return EVO_PROJECT_COMMAND_ERROR_OPERATION;
    }
    descriptor = &evo_project_commands[request->operation];

    if (!evo_project_command_path_valid(request->output_path)) {
        return EVO_PROJECT_COMMAND_ERROR_PATH;
    }
    if (descriptor->requires_manifest) {
        if (!evo_project_command_path_valid(request->manifest_path)) {
            return EVO_PROJECT_COMMAND_ERROR_PATH;
        }
        if (!evo_project_command_manifest_valid(request->manifest)) {
            return EVO_PROJECT_COMMAND_ERROR_MANIFEST;
        }
    }
    if (descriptor->requires_input &&
        !evo_project_command_path_valid(request->input_path)) {
        return EVO_PROJECT_COMMAND_ERROR_PATH;
    }
    if (descriptor->requires_evidence &&
        !evo_project_command_path_valid(request->evidence_path)) {
        return EVO_PROJECT_COMMAND_ERROR_PATH;
    }
    if (request->overwrite_output) {
        return EVO_PROJECT_COMMAND_ERROR_OUTPUT_POLICY;
    }
    if (request->resume) {
        if (!descriptor->allows_checkpoint ||
            !evo_project_command_path_valid(request->checkpoint_path)) {
            return EVO_PROJECT_COMMAND_ERROR_CHECKPOINT;
        }
    } else if (request->checkpoint_path != NULL &&
               !descriptor->allows_checkpoint) {
        return EVO_PROJECT_COMMAND_ERROR_CHECKPOINT;
    }
    if (request->operation == EVO_PROJECT_COMMAND_REPLAY &&
        (!request->replay_identity_complete || !request->external_inputs_declared)) {
        return EVO_PROJECT_COMMAND_ERROR_REPLAY_IDENTITY;
    }

    plan->schema_version = EVO_PROJECT_COMMAND_SCHEMA_VERSION;
    plan->interface_version = EVO_PROJECT_COMMAND_INTERFACE_VERSION;
    plan->operation = request->operation;
    plan->name = descriptor->name;
    plan->request_schema = descriptor->request_schema;
    plan->manifest_path = request->manifest_path;
    plan->input_path = request->input_path;
    plan->output_path = request->output_path;
    plan->evidence_path = request->evidence_path;
    plan->checkpoint_path = request->checkpoint_path;
    plan->resume = request->resume;

    for (slot = 0U; slot < EVO_PROJECT_COMMAND_PROVIDER_COUNT; slot += 1U) {
        evo_project_command_provider_requirement_t requirement =
            evo_project_provider_requirements[slot];
        const bool required = evo_project_command_provider_required(
            request->operation,
            (evo_project_command_provider_slot_t)slot);

        requirement.required = required;
        plan->providers[slot] = requirement;
        if (required) {
            evo_project_command_status_t provider_status;

            plan->required_provider_count += 1U;
            if (request->provider_policy_identity == NULL ||
                strcmp(request->provider_policy_identity,
                       EVO_PROJECT_COMMAND_PROVIDER_POLICY_ID) != 0) {
                return EVO_PROJECT_COMMAND_ERROR_PROVIDER_POLICY;
            }
            provider_status = evo_project_command_validate_provider(
                &requirement,
                &request->providers[slot]);
            if (provider_status != EVO_PROJECT_COMMAND_SUCCESS) {
                return provider_status;
            }
        }
    }

    plan->provider_policy_identity =
        plan->required_provider_count > 0U
            ? EVO_PROJECT_COMMAND_PROVIDER_POLICY_ID
            : NULL;
    plan->execution_permitted = plan->required_provider_count > 0U;
    plan->input_repository_read_only = true;
    plan->output_atomic = true;
    plan->existing_output_rejected = true;
    plan->network_access_implicit = false;
    plan->repository_mutation_permitted = false;
    plan->machine_evidence_authoritative = true;
    plan->human_summary_is_projection = true;
    plan->stdout_machine_output_only = true;
    plan->stderr_diagnostics_only = true;
    return EVO_PROJECT_COMMAND_SUCCESS;
}

evo_project_command_exit_status_t evo_project_command_exit_for_terminal(
    evo_project_command_operation_t operation,
    evo_project_command_terminal_class_t terminal_class)
{
    if (!evo_project_command_operation_valid(operation)) {
        return EVO_PROJECT_COMMAND_EXIT_INTERNAL;
    }
    switch (terminal_class) {
    case EVO_PROJECT_COMMAND_TERMINAL_SUCCESS:
        return EVO_PROJECT_COMMAND_EXIT_SUCCESS;
    case EVO_PROJECT_COMMAND_TERMINAL_INVALID_INPUT:
        return EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
    case EVO_PROJECT_COMMAND_TERMINAL_INGESTION_FAILURE:
        return EVO_PROJECT_COMMAND_EXIT_INGESTION;
    case EVO_PROJECT_COMMAND_TERMINAL_BASELINE_BUILD_FAILURE:
        return EVO_PROJECT_COMMAND_EXIT_BASELINE_BUILD;
    case EVO_PROJECT_COMMAND_TERMINAL_CORRECTNESS_FAILURE:
        return EVO_PROJECT_COMMAND_EXIT_CORRECTNESS;
    case EVO_PROJECT_COMMAND_TERMINAL_BENCHMARK_INELIGIBLE:
        return EVO_PROJECT_COMMAND_EXIT_BENCHMARK_INELIGIBLE;
    case EVO_PROJECT_COMMAND_TERMINAL_ANALYSIS_FAILURE:
        return EVO_PROJECT_COMMAND_EXIT_ANALYSIS;
    case EVO_PROJECT_COMMAND_TERMINAL_MATERIALIZATION_FAILURE:
        return EVO_PROJECT_COMMAND_EXIT_MATERIALIZATION;
    case EVO_PROJECT_COMMAND_TERMINAL_CANDIDATE_FAILURE:
        return EVO_PROJECT_COMMAND_EXIT_CANDIDATE;
    case EVO_PROJECT_COMMAND_TERMINAL_NO_ELIGIBLE_CHAMPION:
        return EVO_PROJECT_COMMAND_EXIT_NO_ELIGIBLE_CHAMPION;
    case EVO_PROJECT_COMMAND_TERMINAL_REPLAY_MISMATCH:
        return EVO_PROJECT_COMMAND_EXIT_REPLAY_MISMATCH;
    case EVO_PROJECT_COMMAND_TERMINAL_RESOURCE_FAILURE:
        return EVO_PROJECT_COMMAND_EXIT_RESOURCE;
    case EVO_PROJECT_COMMAND_TERMINAL_INTERRUPTED:
        return EVO_PROJECT_COMMAND_EXIT_INTERRUPTED;
    case EVO_PROJECT_COMMAND_TERMINAL_INTERNAL_FAILURE:
    default:
        return EVO_PROJECT_COMMAND_EXIT_INTERNAL;
    }
}

const char *evo_project_command_operation_name(
    evo_project_command_operation_t operation)
{
    if (!evo_project_command_operation_valid(operation)) {
        return "unknown";
    }
    return evo_project_commands[operation].name;
}

const char *evo_project_command_status_name(
    evo_project_command_status_t status)
{
    switch (status) {
    case EVO_PROJECT_COMMAND_SUCCESS:
        return "success";
    case EVO_PROJECT_COMMAND_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_COMMAND_ERROR_SCHEMA:
        return "schema-mismatch";
    case EVO_PROJECT_COMMAND_ERROR_OPERATION:
        return "invalid-operation";
    case EVO_PROJECT_COMMAND_ERROR_PATH:
        return "invalid-path";
    case EVO_PROJECT_COMMAND_ERROR_MANIFEST:
        return "invalid-manifest";
    case EVO_PROJECT_COMMAND_ERROR_PROVIDER_POLICY:
        return "provider-policy-mismatch";
    case EVO_PROJECT_COMMAND_ERROR_PROVIDER_IDENTITY:
        return "provider-identity-mismatch";
    case EVO_PROJECT_COMMAND_ERROR_PROVIDER_VERSION:
        return "provider-version-mismatch";
    case EVO_PROJECT_COMMAND_ERROR_PROVIDER_UNAVAILABLE:
        return "provider-unavailable";
    case EVO_PROJECT_COMMAND_ERROR_PROVIDER_CAPABILITY:
        return "provider-capability-mismatch";
    case EVO_PROJECT_COMMAND_ERROR_REPLAY_IDENTITY:
        return "replay-identity-incomplete";
    case EVO_PROJECT_COMMAND_ERROR_CHECKPOINT:
        return "checkpoint-policy-invalid";
    case EVO_PROJECT_COMMAND_ERROR_OUTPUT_POLICY:
        return "output-policy-invalid";
    default:
        return "unknown";
    }
}

const char *evo_project_command_exit_status_name(
    evo_project_command_exit_status_t status)
{
    switch (status) {
    case EVO_PROJECT_COMMAND_EXIT_SUCCESS:
        return "success";
    case EVO_PROJECT_COMMAND_EXIT_INTERNAL:
        return "internal-failure";
    case EVO_PROJECT_COMMAND_EXIT_USAGE:
        return "usage";
    case EVO_PROJECT_COMMAND_EXIT_CONFIGURATION:
        return "configuration";
    case EVO_PROJECT_COMMAND_EXIT_INGESTION:
        return "ingestion-failed";
    case EVO_PROJECT_COMMAND_EXIT_BASELINE_BUILD:
        return "baseline-build-failed";
    case EVO_PROJECT_COMMAND_EXIT_CORRECTNESS:
        return "correctness-failed";
    case EVO_PROJECT_COMMAND_EXIT_BENCHMARK_INELIGIBLE:
        return "benchmark-ineligible";
    case EVO_PROJECT_COMMAND_EXIT_ANALYSIS:
        return "analysis-failed";
    case EVO_PROJECT_COMMAND_EXIT_MATERIALIZATION:
        return "materialization-failed";
    case EVO_PROJECT_COMMAND_EXIT_CANDIDATE:
        return "candidate-failed";
    case EVO_PROJECT_COMMAND_EXIT_NO_ELIGIBLE_CHAMPION:
        return "no-eligible-champion";
    case EVO_PROJECT_COMMAND_EXIT_REPLAY_MISMATCH:
        return "replay-mismatch";
    case EVO_PROJECT_COMMAND_EXIT_RESOURCE:
        return "resource-failure";
    case EVO_PROJECT_COMMAND_EXIT_INTERRUPTED:
        return "interrupted";
    default:
        return "unknown";
    }
}
