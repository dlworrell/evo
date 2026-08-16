#include "internal/project_orchestration_internal.h"

#include "internal/project_runtime.h"

#include <stdint.h>
#include <string.h>

static bool evo_orchestration_text_valid(
    const char *text,
    size_t maximum,
    bool allow_slash)
{
    size_t index = 0U;

    if (text == NULL || maximum == 0U) {
        return false;
    }
    while (index <= maximum && text[index] != '\0') {
        const unsigned char value = (unsigned char)text[index];

        if (value < 0x20U || value == 0x7fU ||
            (!allow_slash && (value == (unsigned char)'/' ||
                              value == (unsigned char)'\\'))) {
            return false;
        }
        index += 1U;
    }
    return index > 0U && index <= maximum;
}

static char *evo_orchestration_duplicate(
    const char *text,
    size_t maximum)
{
    size_t length = 0U;
    char *copy;
    size_t index;

    if (!evo_orchestration_text_valid(text, maximum, true)) {
        return NULL;
    }
    while (text[length] != '\0') {
        length += 1U;
    }
    if (length == SIZE_MAX) {
        return NULL;
    }
    copy = evo_project_allocate_zeroed(length + 1U, sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }
    for (index = 0U; index < length; index += 1U) {
        copy[index] = text[index];
    }
    copy[length] = '\0';
    return copy;
}

static bool evo_orchestration_limits_valid(
    const evo_project_orchestration_limits_t *limits)
{
    return limits->max_string_bytes > 0U &&
           limits->max_candidates > 0U &&
           limits->max_external_workers > 0U &&
           limits->max_poll_rounds > 0U &&
           limits->max_cpu_time_ms > 0U &&
           limits->max_address_space_bytes > 0U &&
           limits->max_descendant_process_count > 0U &&
           limits->max_storage_bytes > 0U &&
           limits->max_output_bytes > 0U &&
           limits->max_wall_timeout_ms > 0U &&
           limits->max_workspace_bytes > 0U &&
           limits->max_total_bytes > 0U;
}

static bool evo_orchestration_resources_valid(
    const evo_project_orchestration_resource_policy_t *resources,
    const evo_project_orchestration_limits_t *limits)
{
    return resources->schema_version ==
               EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION &&
           resources->external_worker_count > 0U &&
           resources->external_worker_count <=
               limits->max_external_workers &&
           resources->cpu_time_ms > 0U &&
           resources->cpu_time_ms <= limits->max_cpu_time_ms &&
           resources->address_space_bytes > 0U &&
           resources->address_space_bytes <=
               limits->max_address_space_bytes &&
           resources->descendant_process_count > 0U &&
           resources->descendant_process_count <=
               limits->max_descendant_process_count &&
           resources->storage_bytes > 0U &&
           resources->storage_bytes <= limits->max_storage_bytes &&
           resources->output_bytes > 0U &&
           resources->output_bytes <= limits->max_output_bytes &&
           resources->wall_timeout_ms > 0U &&
           resources->wall_timeout_ms <= limits->max_wall_timeout_ms &&
           resources->workspace_bytes > 0U &&
           resources->workspace_bytes <= limits->max_workspace_bytes &&
           resources->require_descendant_cleanup;
}

static bool evo_orchestration_candidates_valid(
    const evo_project_orchestration_config_t *config)
{
    size_t index;
    size_t generation;

    if (config->candidate_count == 0U ||
        config->candidate_count > config->limits.max_candidates ||
        config->candidates == NULL) {
        return false;
    }
    generation = config->candidates[0].generation;
    for (index = 0U; index < config->candidate_count; index += 1U) {
        const evo_project_orchestration_candidate_request_t *candidate =
            &config->candidates[index];

        if (candidate->schema_version !=
                EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION ||
            candidate->generation != generation ||
            candidate->population_index != index ||
            !evo_orchestration_text_valid(
                candidate->recipe_fingerprint,
                config->limits.max_string_bytes,
                false) ||
            !evo_orchestration_text_valid(
                candidate->workspace_identity,
                config->limits.max_string_bytes,
                false)) {
            return false;
        }
    }
    return true;
}

static bool evo_orchestration_allocation_budget_valid(
    const evo_project_orchestration_config_t *config)
{
    size_t job_bytes;
    size_t runtime_bytes;
    size_t owner_bytes = sizeof(evo_project_orchestration_owner_t);

    if (config->candidate_count >
            SIZE_MAX / sizeof(evo_project_orchestration_job_record_t) ||
        config->candidate_count >
            SIZE_MAX / sizeof(evo_project_orchestration_runtime_job_t)) {
        return false;
    }
    job_bytes = config->candidate_count *
                sizeof(evo_project_orchestration_job_record_t);
    runtime_bytes = config->candidate_count *
                    sizeof(evo_project_orchestration_runtime_job_t);
    if (job_bytes > SIZE_MAX - owner_bytes) {
        return false;
    }
    owner_bytes += job_bytes;
    if (runtime_bytes > SIZE_MAX - owner_bytes) {
        return false;
    }
    owner_bytes += runtime_bytes;
    return owner_bytes <= config->limits.max_total_bytes;
}

bool evo_project_orchestration_config_valid(
    const evo_project_orchestration_config_t *config,
    const evo_project_orchestration_t *orchestration)
{
    if (config == NULL || orchestration == NULL ||
        !evo_orchestration_limits_valid(&config->limits) ||
        !evo_orchestration_resources_valid(
            &config->resources, &config->limits) ||
        !evo_orchestration_candidates_valid(config) ||
        !evo_orchestration_allocation_budget_valid(config) ||
        !evo_orchestration_text_valid(
            config->policy_identity,
            config->limits.max_string_bytes,
            false) ||
        !evo_orchestration_text_valid(
            config->provider.identity,
            config->limits.max_string_bytes,
            false) ||
        config->provider.start == NULL || config->provider.poll == NULL ||
        config->provider.cancel == NULL || config->provider.join == NULL) {
        return false;
    }
    return (const void *)config != (const void *)orchestration &&
           (const void *)config->candidates !=
               (const void *)orchestration &&
           (const void *)config->policy_identity !=
               (const void *)orchestration &&
           (const void *)config->provider.identity !=
               (const void *)orchestration;
}

bool evo_project_orchestration_allocate_owner(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t **owner_out)
{
    evo_project_orchestration_owner_t *owner;

    if (config == NULL || owner_out == NULL || *owner_out != NULL) {
        return false;
    }
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        return false;
    }
    owner->policy_identity = evo_orchestration_duplicate(
        config->policy_identity, config->limits.max_string_bytes);
    owner->provider_identity = evo_orchestration_duplicate(
        config->provider.identity, config->limits.max_string_bytes);
    owner->jobs = evo_project_allocate_zeroed(
        config->candidate_count, sizeof(*owner->jobs));
    owner->runtime_jobs = evo_project_allocate_zeroed(
        config->candidate_count, sizeof(*owner->runtime_jobs));
    if (owner->policy_identity == NULL || owner->provider_identity == NULL ||
        owner->jobs == NULL || owner->runtime_jobs == NULL) {
        evo_project_orchestration_owner_destroy(owner);
        return false;
    }
    owner->job_count = config->candidate_count;
    owner->view.schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    owner->view.policy_identity = owner->policy_identity;
    owner->view.provider_identity = owner->provider_identity;
    owner->view.generation = config->candidates[0].generation;
    owner->view.candidate_count = config->candidate_count;
    owner->view.external_worker_count =
        config->resources.external_worker_count;
    owner->view.job_count = config->candidate_count;
    owner->view.jobs = owner->jobs;
    owner->view.first_hard_failure_index = 0U;
    owner->view.projection_complete = true;
    owner->view.probabilistic_authority = false;
    *owner_out = owner;
    return true;
}

void evo_project_orchestration_owner_destroy(
    evo_project_orchestration_owner_t *owner)
{
    if (owner == NULL) {
        return;
    }
    evo_project_release(owner->policy_identity);
    evo_project_release(owner->provider_identity);
    evo_project_release(owner->jobs);
    evo_project_release(owner->runtime_jobs);
    evo_project_release(owner);
}

bool evo_project_orchestration_terminal_is_hard_failure(
    evo_project_orchestration_terminal_reason_t reason)
{
    return reason != EVO_PROJECT_ORCHESTRATION_TERMINAL_NONE &&
           reason != EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS &&
           reason !=
               EVO_PROJECT_ORCHESTRATION_TERMINAL_CANDIDATE_REJECTED;
}

bool evo_project_orchestration_capabilities_satisfy(
    const evo_project_orchestration_resource_policy_t *resources,
    const evo_project_orchestration_provider_capabilities_t *capabilities)
{
    if (resources == NULL || capabilities == NULL ||
        capabilities->schema_version !=
            EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION) {
        return false;
    }
    return capabilities->cpu_limit_enforced &&
           capabilities->address_space_limit_enforced &&
           capabilities->process_limit_enforced &&
           capabilities->storage_limit_enforced &&
           capabilities->output_limit_enforced &&
           capabilities->timeout_enforced &&
           (!resources->require_filesystem_isolation ||
            capabilities->filesystem_isolation_enforced) &&
           (!resources->require_network_isolation ||
            capabilities->network_isolation_enforced) &&
           (!resources->require_descendant_cleanup ||
            capabilities->descendant_cleanup_enforced);
}

const char *evo_project_orchestration_status_name(
    evo_project_orchestration_status_t status)
{
    switch (status) {
    case EVO_PROJECT_ORCHESTRATION_SUCCESS:
        return "success";
    case EVO_PROJECT_ORCHESTRATION_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_ORCHESTRATION_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_ORCHESTRATION_ERROR_POLICY_INVALID:
        return "policy-invalid";
    case EVO_PROJECT_ORCHESTRATION_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_ORCHESTRATION_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER:
        return "provider";
    case EVO_PROJECT_ORCHESTRATION_ERROR_CLEANUP:
        return "cleanup";
    case EVO_PROJECT_ORCHESTRATION_ERROR_EVIDENCE:
        return "evidence";
    case EVO_PROJECT_ORCHESTRATION_ERROR_STATE:
    default:
        return "state";
    }
}

const char *evo_project_orchestration_job_state_name(
    evo_project_orchestration_job_state_t state)
{
    switch (state) {
    case EVO_PROJECT_ORCHESTRATION_JOB_UNASSIGNED:
        return "unassigned";
    case EVO_PROJECT_ORCHESTRATION_JOB_ADMITTED:
        return "admitted";
    case EVO_PROJECT_ORCHESTRATION_JOB_STARTED:
        return "started";
    case EVO_PROJECT_ORCHESTRATION_JOB_CANCEL_REQUESTED:
        return "cancel-requested";
    case EVO_PROJECT_ORCHESTRATION_JOB_TERMINAL:
        return "terminal";
    case EVO_PROJECT_ORCHESTRATION_JOB_JOINED:
        return "joined";
    case EVO_PROJECT_ORCHESTRATION_JOB_STAGED:
        return "staged";
    case EVO_PROJECT_ORCHESTRATION_JOB_COMMITTED:
        return "committed";
    default:
        return "unknown";
    }
}

const char *evo_project_orchestration_terminal_reason_name(
    evo_project_orchestration_terminal_reason_t reason)
{
    switch (reason) {
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_NONE:
        return "none";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS:
        return "success";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_CANDIDATE_REJECTED:
        return "candidate-rejected";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_START_FAILED:
        return "start-failed";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_TIMEOUT:
        return "timeout";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_SIGNAL:
        return "signal";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_CPU_LIMIT:
        return "cpu-limit";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_MEMORY_LIMIT:
        return "memory-limit";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_PROCESS_LIMIT:
        return "process-limit";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_STORAGE_LIMIT:
        return "storage-limit";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_OUTPUT_LIMIT:
        return "output-limit";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED:
        return "canceled";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_CAPABILITY_UNAVAILABLE:
        return "capability-unavailable";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL:
        return "provider-protocol";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_JOIN_FAILED:
        return "join-failed";
    case EVO_PROJECT_ORCHESTRATION_TERMINAL_CLEANUP_FAILED:
        return "cleanup-failed";
    default:
        return "unknown";
    }
}
