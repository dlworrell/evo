#include "internal/project_measurement_internal.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <string.h>

static size_t evo_measurement_sample_capacity(
    const evo_project_measurement_config_t *config)
{
    size_t total = 0U;
    size_t index;

    for (index = 0U; index < config->workload_count; index += 1U) {
        total += 2U * (config->workloads[index].warmup_count +
                       config->workloads[index].repetition_count);
    }
    return total;
}

bool evo_measurement_prepare_owner(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner)
{
    size_t index;

    owner->candidate_fingerprint =
        evo_measurement_duplicate(config->assurance->candidate_fingerprint);
    owner->assurance_fingerprint =
        evo_measurement_duplicate(config->assurance->assurance_fingerprint);
    owner->baseline_identity = evo_measurement_duplicate(config->baseline_identity);
    owner->policy_id = evo_measurement_duplicate(config->policy_id);
    owner->measurement_provider_identity =
        evo_measurement_duplicate(config->measurement_provider_identity);
    owner->output_path = evo_measurement_duplicate(config->output_path);
    owner->sample_capacity = evo_measurement_sample_capacity(config);
    owner->workloads = evo_project_allocate_zeroed(
        config->workload_count, sizeof(*owner->workloads));
    owner->workload_ids = evo_project_allocate_zeroed(
        config->workload_count, sizeof(*owner->workload_ids));
    owner->samples = evo_project_allocate_zeroed(
        owner->sample_capacity, sizeof(*owner->samples));
    owner->sample_workload_ids = evo_project_allocate_zeroed(
        owner->sample_capacity, sizeof(*owner->sample_workload_ids));
    owner->sample_exclusion_reasons = evo_project_allocate_zeroed(
        owner->sample_capacity, sizeof(*owner->sample_exclusion_reasons));

    if (owner->candidate_fingerprint == NULL ||
        owner->assurance_fingerprint == NULL || owner->baseline_identity == NULL ||
        owner->policy_id == NULL || owner->measurement_provider_identity == NULL ||
        owner->output_path == NULL || owner->workloads == NULL ||
        owner->workload_ids == NULL || owner->samples == NULL ||
        owner->sample_workload_ids == NULL ||
        owner->sample_exclusion_reasons == NULL) {
        return false;
    }

    for (index = 0U; index < config->workload_count; index += 1U) {
        owner->workload_ids[index] =
            evo_measurement_duplicate(config->workloads[index].workload_id);
        if (owner->workload_ids[index] == NULL) {
            return false;
        }
        owner->workloads[index].workload_id = owner->workload_ids[index];
        owner->workloads[index].comparison = EVO_PROJECT_MEASUREMENT_INCOMPLETE;
    }

    owner->policy_fingerprint_value = evo_measurement_policy_fingerprint(config);
    owner->condition_fingerprint_value =
        evo_measurement_condition_fingerprint(config);

    owner->view.schema_version = EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION;
    owner->view.candidate_fingerprint = owner->candidate_fingerprint;
    owner->view.assurance_fingerprint = owner->assurance_fingerprint;
    owner->view.baseline_identity = owner->baseline_identity;
    owner->view.policy_id = owner->policy_id;
    owner->view.measurement_provider_identity =
        owner->measurement_provider_identity;
    evo_project_fingerprint_format(
        owner->policy_fingerprint_value, owner->view.policy_fingerprint);
    evo_project_fingerprint_format(
        owner->condition_fingerprint_value, owner->view.condition_fingerprint);
    owner->view.workload_count = config->workload_count;
    owner->view.workloads = owner->workloads;
    owner->view.sample_count = 0U;
    owner->view.samples = owner->samples;
    owner->view.overall_comparison = EVO_PROJECT_MEASUREMENT_INCOMPLETE;
    owner->view.fitness_weights = config->fitness_weights;
    owner->view.fitness_available = false;
    owner->view.correctness_preserved = true;
    owner->view.projection_complete = true;
    owner->view.probabilistic_authority = false;
    owner->view.output_path = owner->output_path;
    owner->view.private_owner = owner;
    return true;
}

void evo_measurement_release_owner(evo_project_measurement_owner_t *owner)
{
    size_t index;

    if (owner == NULL) {
        return;
    }
    if (owner->workload_ids != NULL) {
        for (index = 0U; index < owner->view.workload_count; index += 1U) {
            evo_project_release(owner->workload_ids[index]);
        }
    }
    if (owner->sample_workload_ids != NULL) {
        for (index = 0U; index < owner->sample_capacity; index += 1U) {
            evo_project_release(owner->sample_workload_ids[index]);
        }
    }
    if (owner->sample_exclusion_reasons != NULL) {
        for (index = 0U; index < owner->sample_capacity; index += 1U) {
            evo_project_release(owner->sample_exclusion_reasons[index]);
        }
    }
    evo_project_release(owner->candidate_fingerprint);
    evo_project_release(owner->assurance_fingerprint);
    evo_project_release(owner->baseline_identity);
    evo_project_release(owner->policy_id);
    evo_project_release(owner->measurement_provider_identity);
    evo_project_release(owner->output_path);
    evo_project_release(owner->workloads);
    evo_project_release(owner->workload_ids);
    evo_project_release(owner->samples);
    evo_project_release(owner->sample_workload_ids);
    evo_project_release(owner->sample_exclusion_reasons);
    evo_project_release(owner->canonical_json);
    evo_project_release(owner->audit_markdown);
    evo_project_release(owner);
}

static bool evo_measurement_outcome_valid(
    const evo_project_measurement_outcome_t *outcome)
{
    if (outcome->schema_version != EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION ||
        outcome->reliability_ppm > EVO_PROJECT_MEASUREMENT_PPM_SCALE ||
        outcome->maintainability_ppm > EVO_PROJECT_MEASUREMENT_PPM_SCALE ||
        (outcome->timed_out && outcome->failed)) {
        return false;
    }
    if (!outcome->completed) {
        return outcome->timed_out || outcome->failed;
    }
    return !outcome->timed_out && !outcome->failed && outcome->runtime_ns > 0U &&
           outcome->binary_size_bytes > 0U;
}

static evo_project_measurement_status_t evo_measurement_run_one(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner,
    const evo_project_measurement_workload_policy_t *policy,
    evo_project_measurement_subject_t subject,
    evo_project_measurement_phase_t phase,
    size_t pair_index,
    size_t sequence_index)
{
    evo_project_measurement_request_t request = {0};
    evo_project_measurement_outcome_t outcome = {0};
    evo_project_measurement_status_t status;

    request.schema_version = EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION;
    request.workload_id = policy->workload_id;
    request.subject = subject;
    request.phase = phase;
    request.pair_index = pair_index;
    request.sequence_index = sequence_index;
    request.timeout_ms = policy->timeout_ms;
    request.expected_condition_fingerprint = owner->condition_fingerprint_value;

    outcome.schema_version = EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION;
    status = config->provider(&request, config->provider_context, &outcome);
    if (status != EVO_PROJECT_MEASUREMENT_SUCCESS ||
        !evo_measurement_outcome_valid(&outcome)) {
        return status == EVO_PROJECT_MEASUREMENT_ERROR_OUT_OF_MEMORY
                   ? status
                   : EVO_PROJECT_MEASUREMENT_ERROR_PROVIDER;
    }
    if (!evo_measurement_record_sample(
            config,
            owner,
            policy,
            subject,
            phase,
            pair_index,
            sequence_index,
            &outcome)) {
        return EVO_PROJECT_MEASUREMENT_ERROR_OUT_OF_MEMORY;
    }
    return EVO_PROJECT_MEASUREMENT_SUCCESS;
}

static evo_project_measurement_subject_t evo_measurement_first_subject(
    evo_project_measurement_order_t order,
    size_t pair_index)
{
    const bool even = (pair_index % 2U) == 0U;

    if (order == EVO_PROJECT_MEASUREMENT_ALTERNATE_BASELINE_FIRST) {
        return even ? EVO_PROJECT_MEASUREMENT_BASELINE
                    : EVO_PROJECT_MEASUREMENT_CANDIDATE;
    }
    return even ? EVO_PROJECT_MEASUREMENT_CANDIDATE
                : EVO_PROJECT_MEASUREMENT_BASELINE;
}

static evo_project_measurement_subject_t evo_measurement_other_subject(
    evo_project_measurement_subject_t subject)
{
    return subject == EVO_PROJECT_MEASUREMENT_BASELINE
               ? EVO_PROJECT_MEASUREMENT_CANDIDATE
               : EVO_PROJECT_MEASUREMENT_BASELINE;
}

static evo_project_measurement_status_t evo_measurement_run_phase(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner,
    const evo_project_measurement_workload_policy_t *policy,
    evo_project_measurement_phase_t phase,
    size_t pair_count,
    size_t *sequence_index)
{
    size_t pair_index;

    for (pair_index = 0U; pair_index < pair_count; pair_index += 1U) {
        const evo_project_measurement_subject_t first =
            evo_measurement_first_subject(policy->order, pair_index);
        const evo_project_measurement_subject_t second =
            evo_measurement_other_subject(first);
        evo_project_measurement_status_t status = evo_measurement_run_one(
            config,
            owner,
            policy,
            first,
            phase,
            pair_index,
            *sequence_index);

        *sequence_index += 1U;
        if (status != EVO_PROJECT_MEASUREMENT_SUCCESS) {
            return status;
        }
        status = evo_measurement_run_one(
            config,
            owner,
            policy,
            second,
            phase,
            pair_index,
            *sequence_index);
        *sequence_index += 1U;
        if (status != EVO_PROJECT_MEASUREMENT_SUCCESS) {
            return status;
        }
    }
    return EVO_PROJECT_MEASUREMENT_SUCCESS;
}

static evo_project_measurement_status_t evo_measurement_execute(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner)
{
    size_t workload_index;
    size_t sequence_index = 0U;

    for (workload_index = 0U; workload_index < config->workload_count;
         workload_index += 1U) {
        const evo_project_measurement_workload_policy_t *policy =
            &config->workloads[workload_index];
        evo_project_measurement_status_t status = evo_measurement_run_phase(
            config,
            owner,
            policy,
            EVO_PROJECT_MEASUREMENT_WARMUP,
            policy->warmup_count,
            &sequence_index);

        if (status != EVO_PROJECT_MEASUREMENT_SUCCESS) {
            return status;
        }
        status = evo_measurement_run_phase(
            config,
            owner,
            policy,
            EVO_PROJECT_MEASUREMENT_RECORDED,
            policy->repetition_count,
            &sequence_index);
        if (status != EVO_PROJECT_MEASUREMENT_SUCCESS) {
            return status;
        }
    }
    return EVO_PROJECT_MEASUREMENT_SUCCESS;
}

static void evo_measurement_result_fingerprint_double(
    evo_project_fingerprint_t *fingerprint,
    double value)
{
    char text[48];
    const int written =
        evo_project_format(text, sizeof(text), "%.17g", value);

    if (written > 0 && (size_t)written < sizeof(text)) {
        evo_project_fingerprint_string(fingerprint, text);
    } else {
        evo_project_fingerprint_string(fingerprint, "invalid-double");
    }
}

uint64_t evo_measurement_result_fingerprint(
    const evo_project_measurement_owner_t *owner)
{
    evo_project_fingerprint_t fingerprint;
    size_t index;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(&fingerprint, "evo-project-measurement-result-v1");
    evo_project_fingerprint_string(&fingerprint, owner->candidate_fingerprint);
    evo_project_fingerprint_string(&fingerprint, owner->assurance_fingerprint);
    evo_project_fingerprint_string(&fingerprint, owner->baseline_identity);
    evo_project_fingerprint_u64(&fingerprint, owner->policy_fingerprint_value);
    evo_project_fingerprint_u64(&fingerprint, owner->condition_fingerprint_value);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)owner->view.sample_count);
    for (index = 0U; index < owner->view.sample_count; index += 1U) {
        const evo_project_measurement_sample_t *sample = &owner->samples[index];

        evo_project_fingerprint_string(&fingerprint, sample->workload_id);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)sample->subject);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)sample->phase);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)sample->pair_index);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)sample->sequence_index);
        evo_project_fingerprint_u64(&fingerprint, sample->completed ? 1U : 0U);
        evo_project_fingerprint_u64(&fingerprint, sample->timed_out ? 1U : 0U);
        evo_project_fingerprint_u64(&fingerprint, sample->failed ? 1U : 0U);
        evo_project_fingerprint_u64(&fingerprint, sample->excluded ? 1U : 0U);
        evo_project_fingerprint_string(
            &fingerprint,
            sample->exclusion_reason == NULL ? "" : sample->exclusion_reason);
        evo_project_fingerprint_u64(&fingerprint, sample->condition_fingerprint);
        evo_project_fingerprint_u64(&fingerprint, sample->runtime_ns);
        evo_project_fingerprint_u64(&fingerprint, sample->peak_memory_bytes);
        evo_project_fingerprint_u64(&fingerprint, sample->binary_size_bytes);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)sample->reliability_ppm);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)sample->maintainability_ppm);
    }
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)owner->view.overall_comparison);
    evo_measurement_result_fingerprint_double(
        &fingerprint, owner->view.fitness.correctness);
    evo_measurement_result_fingerprint_double(
        &fingerprint, owner->view.fitness.performance);
    evo_measurement_result_fingerprint_double(
        &fingerprint, owner->view.fitness.memory_use);
    evo_measurement_result_fingerprint_double(
        &fingerprint, owner->view.fitness.reliability);
    evo_measurement_result_fingerprint_double(
        &fingerprint, owner->view.fitness.maintainability);
    evo_measurement_result_fingerprint_double(
        &fingerprint, owner->view.fitness.constraint_penalty);
    evo_measurement_result_fingerprint_double(
        &fingerprint, owner->view.fitness.total);
    evo_project_fingerprint_u64(
        &fingerprint, owner->view.fitness_available ? 1U : 0U);
    return fingerprint.value;
}

evo_project_measurement_status_t evo_project_candidate_measure(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_t *measurement)
{
    evo_project_measurement_owner_t *owner;
    evo_project_measurement_status_t status;

    if (measurement == NULL || config == NULL) {
        return EVO_PROJECT_MEASUREMENT_ERROR_INVALID_ARGUMENT;
    }
    if (measurement->private_owner != NULL) {
        return EVO_PROJECT_MEASUREMENT_ERROR_RESULT_ACTIVE;
    }
    if (config->assurance == NULL || !config->assurance->performance_eligible) {
        return EVO_PROJECT_MEASUREMENT_ERROR_ASSURANCE_INELIGIBLE;
    }
    if (!evo_measurement_config_valid(config)) {
        return EVO_PROJECT_MEASUREMENT_ERROR_POLICY_INVALID;
    }

    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        return EVO_PROJECT_MEASUREMENT_ERROR_OUT_OF_MEMORY;
    }
    if (!evo_measurement_prepare_owner(config, owner)) {
        evo_measurement_release_owner(owner);
        return EVO_PROJECT_MEASUREMENT_ERROR_OUT_OF_MEMORY;
    }

    status = evo_measurement_execute(config, owner);
    if (status == EVO_PROJECT_MEASUREMENT_SUCCESS &&
        !evo_measurement_finalize_workloads(config, owner)) {
        status = EVO_PROJECT_MEASUREMENT_ERROR_OUT_OF_MEMORY;
    }
    if (status == EVO_PROJECT_MEASUREMENT_SUCCESS) {
        owner->measurement_fingerprint_value =
            evo_measurement_result_fingerprint(owner);
        evo_project_fingerprint_format(
            owner->measurement_fingerprint_value,
            owner->view.measurement_fingerprint);
        if (!evo_measurement_build_evidence(config, owner)) {
            status = EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE;
        }
    }
    if (status == EVO_PROJECT_MEASUREMENT_SUCCESS &&
        (owner->view.canonical_json_size > config->limits.max_evidence_bytes ||
         owner->view.audit_markdown_size > config->limits.max_evidence_bytes)) {
        status = EVO_PROJECT_MEASUREMENT_ERROR_RESOURCE_LIMIT;
    }
    if (status == EVO_PROJECT_MEASUREMENT_SUCCESS) {
        status = evo_measurement_publish_evidence(owner);
    }
    if (status != EVO_PROJECT_MEASUREMENT_SUCCESS) {
        evo_measurement_release_owner(owner);
        return status;
    }

    *measurement = owner->view;
    measurement->private_owner = owner;
    return EVO_PROJECT_MEASUREMENT_SUCCESS;
}

void evo_project_measurement_destroy(evo_project_measurement_t *measurement)
{
    evo_project_measurement_owner_t *owner;

    if (measurement == NULL || measurement->private_owner == NULL) {
        return;
    }
    owner = measurement->private_owner;
    *measurement = (evo_project_measurement_t){0};
    evo_measurement_release_owner(owner);
}
