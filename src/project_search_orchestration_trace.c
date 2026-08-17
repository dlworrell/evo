#include "internal/project_search_orchestration_trace.h"

#include "internal/project_runtime.h"

#include <stdint.h>
#include <string.h>

typedef struct evo_project_search_orchestration_trace_owner {
    char *policy_identity;
    char *provider_identity;
    evo_project_search_orchestration_batch_record_t *batches;
    evo_project_orchestration_job_record_t *jobs;
    size_t batch_capacity;
    size_t job_capacity;
    size_t batch_count;
    size_t job_count;
} evo_project_search_orchestration_trace_owner_t;

static char *trace_duplicate(const char *value, size_t maximum)
{
    size_t length = 0U;
    char *copy;
    size_t index;

    if (value == NULL || maximum == 0U) {
        return NULL;
    }
    while (length <= maximum && value[length] != '\0') {
        if ((unsigned char)value[length] < 0x20U ||
            (unsigned char)value[length] == 0x7fU) {
            return NULL;
        }
        length += 1U;
    }
    if (length == 0U || length > maximum || length == SIZE_MAX) {
        return NULL;
    }
    copy = evo_project_allocate_zeroed(length + 1U, sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }
    for (index = 0U; index < length; index += 1U) {
        copy[index] = value[index];
    }
    copy[length] = '\0';
    return copy;
}

static bool trace_checked_multiply(size_t left, size_t right, size_t *product)
{
    if (product == NULL || (left != 0U && right > SIZE_MAX / left)) {
        return false;
    }
    *product = left * right;
    return true;
}

static void trace_rebind_job(evo_project_orchestration_job_record_t *job)
{
    if (job->candidate_fingerprint[0] != '\0') {
        job->evaluation.candidate_fingerprint = job->candidate_fingerprint;
    } else {
        job->evaluation.candidate_fingerprint = NULL;
    }
    if (job->assurance_fingerprint[0] != '\0') {
        job->evaluation.assurance_fingerprint = job->assurance_fingerprint;
    } else {
        job->evaluation.assurance_fingerprint = NULL;
    }
    if (job->measurement_fingerprint[0] != '\0') {
        job->evaluation.measurement_fingerprint = job->measurement_fingerprint;
    } else {
        job->evaluation.measurement_fingerprint = NULL;
    }
}

bool evo_project_search_orchestration_trace_owner_create(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *policy,
    evo_project_search_orchestration_trace_owner_t **owner_out)
{
    evo_project_search_orchestration_trace_owner_t *owner;
    size_t batch_capacity;
    size_t job_capacity;

    if (config == NULL || policy == NULL || owner_out == NULL ||
        *owner_out != NULL || config->population_size == 0U ||
        config->generation_limit == SIZE_MAX) {
        return false;
    }
    batch_capacity = config->generation_limit + 1U;
    if (!trace_checked_multiply(
            batch_capacity, config->population_size, &job_capacity)) {
        return false;
    }
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        return false;
    }
    owner->policy_identity = trace_duplicate(
        policy->identity, policy->limits.max_string_bytes);
    owner->provider_identity = trace_duplicate(
        policy->provider.identity, policy->limits.max_string_bytes);
    owner->batches = evo_project_allocate_zeroed(
        batch_capacity, sizeof(*owner->batches));
    owner->jobs = evo_project_allocate_zeroed(
        job_capacity, sizeof(*owner->jobs));
    if (owner->policy_identity == NULL || owner->provider_identity == NULL ||
        owner->batches == NULL || owner->jobs == NULL) {
        evo_project_search_orchestration_trace_owner_destroy(owner);
        return false;
    }
    owner->batch_capacity = batch_capacity;
    owner->job_capacity = job_capacity;
    *owner_out = owner;
    return true;
}

bool evo_project_search_orchestration_trace_append(
    evo_project_search_orchestration_trace_owner_t *owner,
    const evo_project_orchestration_t *batch)
{
    evo_project_search_orchestration_batch_record_t *record;
    size_t index;

    if (owner == NULL || batch == NULL ||
        batch->schema_version != EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION ||
        !batch->projection_complete || batch->probabilistic_authority ||
        batch->job_count != batch->candidate_count ||
        batch->job_count > owner->job_capacity - owner->job_count ||
        owner->batch_count >= owner->batch_capacity ||
        (batch->job_count > 0U && batch->jobs == NULL)) {
        return false;
    }
    record = &owner->batches[owner->batch_count];
    *record = (evo_project_search_orchestration_batch_record_t){0};
    record->generation = batch->generation;
    record->external_worker_count = batch->external_worker_count;
    record->completion_count = batch->completion_count;
    record->committed_count = batch->committed_count;
    record->first_hard_failure_index = batch->first_hard_failure_index;
    record->has_hard_failure = batch->has_hard_failure;
    record->generation_committed = batch->generation_committed;
    record->cleanup_complete = batch->cleanup_complete;
    record->job_count = batch->job_count;
    record->jobs = owner->jobs + owner->job_count;
    for (index = 0U; index < batch->job_count; index += 1U) {
        owner->jobs[owner->job_count + index] = batch->jobs[index];
        trace_rebind_job(&owner->jobs[owner->job_count + index]);
    }
    owner->job_count += batch->job_count;
    owner->batch_count += 1U;
    return true;
}

void evo_project_search_orchestration_trace_publish(
    evo_project_search_orchestration_trace_owner_t *owner,
    bool run_complete,
    evo_project_search_orchestration_trace_t *trace)
{
    if (owner == NULL || trace == NULL) {
        return;
    }
    *trace = (evo_project_search_orchestration_trace_t){0};
    trace->schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    trace->policy_identity = owner->policy_identity;
    trace->provider_identity = owner->provider_identity;
    trace->batch_count = owner->batch_count;
    trace->batches = owner->batches;
    trace->job_count = owner->job_count;
    trace->run_complete = run_complete;
    trace->projection_complete = true;
    trace->probabilistic_authority = false;
    trace->private_owner = owner;
}

void evo_project_search_orchestration_trace_owner_destroy(
    evo_project_search_orchestration_trace_owner_t *owner)
{
    if (owner == NULL) {
        return;
    }
    evo_project_release(owner->policy_identity);
    evo_project_release(owner->provider_identity);
    evo_project_release(owner->batches);
    evo_project_release(owner->jobs);
    evo_project_release(owner);
}

void evo_project_search_orchestration_trace_destroy(
    evo_project_search_orchestration_trace_t *trace)
{
    evo_project_search_orchestration_trace_owner_t *owner;

    if (trace == NULL) {
        return;
    }
    owner = trace->private_owner;
    if (owner != NULL) {
        evo_project_search_orchestration_trace_owner_destroy(owner);
    }
    *trace = (evo_project_search_orchestration_trace_t){0};
}
