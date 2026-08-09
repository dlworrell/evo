#include "internal/project_analysis.h"

#include "internal/project_analysis_evidence.h"
#include "internal/project_analysis_model.h"
#include "internal/project_analysis_owner.h"
#include "internal/project_baseline_owner.h"
#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_runtime.h"
#include "internal/project_snapshot.h"

static bool evo_project_analysis_limits_valid(
    const evo_project_analysis_limits_t *limits)
{
    return limits->max_string_bytes > 0U &&
           limits->max_path_bytes > 0U &&
           limits->max_translation_units > 0U &&
           limits->max_source_locations > 0U &&
           limits->max_declarations > 0U && limits->max_calls > 0U &&
           limits->max_control_flows > 0U &&
           limits->max_data_flows > 0U &&
           limits->max_optimization_records > 0U &&
           limits->max_runtime_records > 0U &&
           limits->max_opportunities > 0U &&
           limits->max_evidence_bytes > 0U;
}

static bool evo_project_analysis_profile_valid(
    const evo_project_analysis_config_t *config)
{
    if (config->runtime_profile_state ==
        EVO_PROJECT_RUNTIME_NOT_CONFIGURED) {
        return config->runtime_profile_identity == NULL;
    }
    if (config->runtime_profile_state == EVO_PROJECT_RUNTIME_UNAVAILABLE ||
        config->runtime_profile_state == EVO_PROJECT_RUNTIME_AVAILABLE) {
        return evo_project_json_text_valid(
            config->runtime_profile_identity,
            config->limits.max_string_bytes,
            false);
    }
    return false;
}

static bool evo_project_analysis_config_valid(
    const evo_project_analysis_config_t *config,
    const evo_project_analysis_t *analysis)
{
    const evo_project_baseline_owner_t *baseline_owner;

    if (config == NULL || analysis == NULL || config->baseline == NULL ||
        config->output_path == NULL || config->provider == NULL ||
        !evo_project_analysis_limits_valid(&config->limits)) {
        return false;
    }
    baseline_owner = config->baseline->private_owner;
    if (config->baseline->schema_version !=
            EVO_PROJECT_BASELINE_SCHEMA_VERSION ||
        baseline_owner == NULL || !baseline_owner->committed ||
        !evo_project_json_text_valid(
            config->provider_identity,
            config->limits.max_string_bytes,
            false) ||
        !evo_project_json_text_valid(
            config->clang_identity,
            config->limits.max_string_bytes,
            false) ||
        !evo_project_json_text_valid(
            config->llvm_identity,
            config->limits.max_string_bytes,
            false) ||
        !evo_project_json_text_valid(
            config->target_identity,
            config->limits.max_string_bytes,
            false) ||
        !evo_project_json_text_valid(
            config->flags_identity,
            config->limits.max_string_bytes,
            false) ||
        !evo_project_analysis_profile_valid(config)) {
        return false;
    }
    return (const void *)config != (const void *)analysis &&
           (const void *)config->baseline != (const void *)analysis &&
           (const void *)baseline_owner != (const void *)analysis &&
           config->provider_context != config &&
           config->provider_context != analysis &&
           config->provider_context != config->baseline &&
           config->provider_context != baseline_owner;
}

static void evo_project_analysis_make_request(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    evo_project_analysis_request_t *request)
{
    request->schema_version = EVO_PROJECT_ANALYSIS_SCHEMA_VERSION;
    request->baseline_fingerprint = config->baseline->baseline_fingerprint;
    request->snapshot_path = baseline_owner->snapshot_path;
    request->compilation_unit_count = baseline_owner->compilation_unit_count;
    request->compilation_units = baseline_owner->compilation_units;
    request->provider_identity = config->provider_identity;
    request->clang_identity = config->clang_identity;
    request->llvm_identity = config->llvm_identity;
    request->target_identity = config->target_identity;
    request->flags_identity = config->flags_identity;
    request->runtime_profile_state = config->runtime_profile_state;
    request->runtime_profile_identity = config->runtime_profile_identity;
    request->limits = config->limits;
    if (request->limits.max_evidence_bytes >
        baseline_owner->manifest.budget.max_evidence_bytes) {
        request->limits.max_evidence_bytes =
            baseline_owner->manifest.budget.max_evidence_bytes;
    }
    request->timeout_ms = baseline_owner->manifest.budget.command_timeout_ms;
    request->max_memory_bytes =
        baseline_owner->manifest.budget.max_memory_bytes;
    request->max_processes = baseline_owner->manifest.budget.max_processes;
    request->max_storage_bytes =
        baseline_owner->manifest.budget.max_storage_bytes;
    request->max_output_bytes =
        baseline_owner->manifest.budget.max_command_output_bytes;
    request->network_access = false;
}

static evo_project_analysis_status_t evo_project_analysis_map_snapshot_status(
    evo_project_status_t status)
{
    switch (status) {
    case EVO_PROJECT_SUCCESS:
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    case EVO_PROJECT_ERROR_OUT_OF_MEMORY:
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    case EVO_PROJECT_ERROR_RESOURCE_LIMIT:
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    case EVO_PROJECT_ERROR_INVALID_ARGUMENT:
    case EVO_PROJECT_ERROR_RESULT_ACTIVE:
    case EVO_PROJECT_ERROR_MANIFEST_IO:
    case EVO_PROJECT_ERROR_MANIFEST_INVALID:
    case EVO_PROJECT_ERROR_PATH_INVALID:
    case EVO_PROJECT_ERROR_SOURCE_IO:
    case EVO_PROJECT_ERROR_SOURCE_CHANGED:
    case EVO_PROJECT_ERROR_OUTPUT_EXISTS:
    case EVO_PROJECT_ERROR_EXECUTION_PROVIDER:
    case EVO_PROJECT_ERROR_EVIDENCE_IO:
    case EVO_PROJECT_ERROR_STATE:
    default:
        return EVO_PROJECT_ANALYSIS_ERROR_BASELINE_CHANGED;
    }
}

static void evo_project_analysis_publish(
    evo_project_analysis_owner_t *owner,
    evo_project_analysis_t *analysis)
{
    analysis->schema_version = EVO_PROJECT_ANALYSIS_SCHEMA_VERSION;
    analysis->baseline_fingerprint = owner->baseline_fingerprint;
    evo_project_fingerprint_format(
        owner->analysis_fingerprint, analysis->analysis_fingerprint);
    analysis->provider_identity = owner->provider_identity;
    analysis->clang_identity = owner->clang_identity;
    analysis->llvm_identity = owner->llvm_identity;
    analysis->target_identity = owner->target_identity;
    analysis->flags_identity = owner->flags_identity;
    analysis->runtime_profile_state = owner->runtime_profile_state;
    analysis->runtime_profile_identity = owner->runtime_profile_identity;
    analysis->output_path = owner->output_path;
    analysis->translation_unit_count = owner->translation_unit_count;
    analysis->translation_units =
        (const char *const *)owner->translation_units;
    analysis->source_location_count = owner->source_location_count;
    analysis->source_locations = owner->source_locations;
    analysis->declaration_count = owner->declaration_count;
    analysis->declarations = owner->declarations;
    analysis->call_count = owner->call_count;
    analysis->calls = owner->calls;
    analysis->control_flow_count = owner->control_flow_count;
    analysis->control_flows = owner->control_flows;
    analysis->data_flow_count = owner->data_flow_count;
    analysis->data_flows = owner->data_flows;
    analysis->optimization_record_count = owner->optimization_record_count;
    analysis->optimization_records = owner->optimization_records;
    analysis->runtime_record_count = owner->runtime_record_count;
    analysis->runtime_records = owner->runtime_records;
    analysis->opportunity_count = owner->opportunity_count;
    analysis->opportunities = owner->opportunities;
    analysis->projection_complete = true;
    analysis->probabilistic_authority = false;
    analysis->private_owner = owner;
}

evo_project_analysis_status_t evo_project_analyze(
    const evo_project_analysis_config_t *config,
    evo_project_analysis_t *analysis)
{
    const evo_project_baseline_owner_t *baseline_owner;
    evo_project_analysis_owner_t *owner;
    evo_project_analysis_provider_result_t provider_result = {0};
    evo_project_analysis_request_t request = {0};
    evo_project_analysis_status_t status;
    evo_project_status_t snapshot_status;

    if (!evo_project_analysis_config_valid(config, analysis)) {
        return EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT;
    }
    if (analysis->private_owner != NULL || analysis->schema_version != 0U) {
        return EVO_PROJECT_ANALYSIS_ERROR_RESULT_ACTIVE;
    }
    if (config->baseline->state != EVO_PROJECT_BASELINE_ELIGIBLE) {
        return EVO_PROJECT_ANALYSIS_ERROR_BASELINE_INELIGIBLE;
    }
    baseline_owner = config->baseline->private_owner;
    if (baseline_owner->compilation_unit_count >
        config->limits.max_translation_units) {
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    status = evo_project_analysis_evidence_preflight(config, baseline_owner);
    if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return status;
    }
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    evo_project_analysis_make_request(config, baseline_owner, &request);
    status = config->provider(
        &request, config->provider_context, &provider_result);
    if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = status == EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY ||
                         status == EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT
                     ? status
                     : EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    } else if (provider_result.schema_version !=
                   EVO_PROJECT_ANALYSIS_SCHEMA_VERSION ||
               !provider_result.completed) {
        status = EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    } else {
        status = evo_project_analysis_model_build(
            config, baseline_owner, &provider_result, owner);
    }
    snapshot_status = evo_project_snapshot_verify_baseline(baseline_owner);
    if (snapshot_status != EVO_PROJECT_SUCCESS) {
        status = evo_project_analysis_map_snapshot_status(snapshot_status);
    }
    if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        goto fail;
    }
    status = evo_project_analysis_evidence_commit(
        config, baseline_owner, owner);
    if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        goto fail;
    }
    evo_project_analysis_publish(owner, analysis);
    return EVO_PROJECT_ANALYSIS_SUCCESS;

fail:
    evo_project_analysis_evidence_discard(owner);
    evo_project_analysis_model_destroy(owner);
    evo_project_release(owner);
    return status;
}

void evo_project_analysis_destroy(evo_project_analysis_t *analysis)
{
    evo_project_analysis_owner_t *owner;

    if (analysis == NULL) {
        return;
    }
    owner = analysis->private_owner;
    if (owner != NULL) {
        evo_project_analysis_evidence_discard(owner);
        evo_project_analysis_model_destroy(owner);
        evo_project_release(owner);
    }
    *analysis = (evo_project_analysis_t){0};
}

const char *evo_project_analysis_status_name(
    evo_project_analysis_status_t status)
{
    switch (status) {
    case EVO_PROJECT_ANALYSIS_SUCCESS:
        return "success";
    case EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_ANALYSIS_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_ANALYSIS_ERROR_BASELINE_INELIGIBLE:
        return "baseline-ineligible";
    case EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_ANALYSIS_ERROR_PROVIDER:
        return "provider";
    case EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE:
        return "inconsistent-evidence";
    case EVO_PROJECT_ANALYSIS_ERROR_BASELINE_CHANGED:
        return "baseline-changed";
    case EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID:
        return "path-invalid";
    case EVO_PROJECT_ANALYSIS_ERROR_OUTPUT_EXISTS:
        return "output-exists";
    case EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO:
        return "evidence-io";
    case EVO_PROJECT_ANALYSIS_ERROR_UNSUPPORTED_EVIDENCE:
        return "unsupported-evidence";
    case EVO_PROJECT_ANALYSIS_ERROR_STATE:
    default:
        return "state";
    }
}

const char *evo_project_runtime_profile_state_name(
    evo_project_runtime_profile_state_t state)
{
    switch (state) {
    case EVO_PROJECT_RUNTIME_NOT_CONFIGURED:
        return "not-configured";
    case EVO_PROJECT_RUNTIME_UNAVAILABLE:
        return "unavailable";
    case EVO_PROJECT_RUNTIME_AVAILABLE:
        return "available";
    default:
        return "invalid";
    }
}
