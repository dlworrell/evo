#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "internal/project_measurement_internal.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int evo_measurement_u64_compare(const void *left, const void *right)
{
    const uint64_t a = *(const uint64_t *)left;
    const uint64_t b = *(const uint64_t *)right;

    return a < b ? -1 : (a > b ? 1 : 0);
}

static uint64_t evo_measurement_median(uint64_t *values, size_t count)
{
    qsort(values, count, sizeof(*values), evo_measurement_u64_compare);
    if ((count % 2U) != 0U) {
        return values[count / 2U];
    }
    {
        const uint64_t left = values[(count / 2U) - 1U];
        const uint64_t right = values[count / 2U];
        return (left / 2U) + (right / 2U) +
               (((left % 2U) + (right % 2U)) / 2U);
    }
}

static uint32_t evo_measurement_range_ppm(
    uint64_t minimum,
    uint64_t maximum,
    uint64_t median)
{
    long double ratio;

    if (median == 0U || maximum < minimum) {
        return EVO_PROJECT_MEASUREMENT_PPM_SCALE;
    }
    ratio = ((long double)(maximum - minimum) *
             (long double)EVO_PROJECT_MEASUREMENT_PPM_SCALE) /
            (long double)median;
    if (ratio >= (long double)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)(ratio + 0.5L);
}

static double evo_measurement_relative_improvement(
    uint64_t baseline,
    uint64_t candidate)
{
    if (baseline == 0U) {
        return 0.0;
    }
    return ((double)baseline - (double)candidate) / (double)baseline;
}

static bool evo_measurement_set_exclusion(
    evo_project_measurement_owner_t *owner,
    size_t index,
    const char *reason)
{
    if (owner->sample_exclusion_reasons[index] != NULL) {
        evo_project_release(owner->sample_exclusion_reasons[index]);
    }
    owner->sample_exclusion_reasons[index] = evo_measurement_duplicate(reason);
    if (owner->sample_exclusion_reasons[index] == NULL) {
        return false;
    }
    owner->samples[index].excluded = true;
    owner->samples[index].exclusion_reason =
        owner->sample_exclusion_reasons[index];
    return true;
}

bool evo_measurement_record_sample(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner,
    const evo_project_measurement_workload_policy_t *policy,
    evo_project_measurement_subject_t subject,
    evo_project_measurement_phase_t phase,
    size_t pair_index,
    size_t sequence_index,
    const evo_project_measurement_outcome_t *outcome)
{
    size_t index;
    evo_project_measurement_sample_t *sample;

    (void)config;
    if (owner->view.sample_count >= owner->sample_capacity) {
        return false;
    }
    index = owner->view.sample_count;
    sample = &owner->samples[index];
    owner->sample_workload_ids[index] =
        evo_measurement_duplicate(policy->workload_id);
    if (owner->sample_workload_ids[index] == NULL) {
        return false;
    }
    sample->workload_id = owner->sample_workload_ids[index];
    sample->subject = subject;
    sample->phase = phase;
    sample->pair_index = pair_index;
    sample->sequence_index = sequence_index;
    sample->completed = outcome->completed;
    sample->timed_out = outcome->timed_out;
    sample->failed = outcome->failed;
    sample->condition_fingerprint = outcome->condition_fingerprint;
    sample->runtime_ns = outcome->runtime_ns;
    sample->peak_memory_bytes = outcome->peak_memory_bytes;
    sample->binary_size_bytes = outcome->binary_size_bytes;
    sample->reliability_ppm = outcome->reliability_ppm;
    sample->maintainability_ppm = outcome->maintainability_ppm;
    if (!outcome->completed) {
        if (!evo_measurement_set_exclusion(owner, index, "incomplete-provider-sample")) {
            return false;
        }
    }
    owner->view.sample_count += 1U;
    return true;
}

static bool evo_measurement_sample_matches(
    const evo_project_measurement_sample_t *sample,
    const char *workload_id,
    evo_project_measurement_subject_t subject)
{
    return sample->phase == EVO_PROJECT_MEASUREMENT_RECORDED &&
           sample->subject == subject && strcmp(sample->workload_id, workload_id) == 0;
}

static size_t evo_measurement_collect_runtime(
    const evo_project_measurement_owner_t *owner,
    const char *workload_id,
    evo_project_measurement_subject_t subject,
    bool include_excluded,
    uint64_t *values)
{
    size_t count = 0U;
    size_t index;

    for (index = 0U; index < owner->view.sample_count; index += 1U) {
        const evo_project_measurement_sample_t *sample = &owner->samples[index];

        if (evo_measurement_sample_matches(sample, workload_id, subject) &&
            sample->completed && !sample->timed_out && !sample->failed &&
            (include_excluded || !sample->excluded)) {
            values[count] = sample->runtime_ns;
            count += 1U;
        }
    }
    return count;
}

static bool evo_measurement_apply_outliers(
    const evo_project_measurement_workload_policy_t *policy,
    evo_project_measurement_owner_t *owner,
    evo_project_measurement_subject_t subject,
    uint64_t *scratch)
{
    const size_t count = evo_measurement_collect_runtime(
        owner, policy->workload_id, subject, true, scratch);
    uint64_t median;
    size_t index;

    if (policy->outlier_policy == EVO_PROJECT_MEASUREMENT_OUTLIER_NONE ||
        count == 0U) {
        return true;
    }
    median = evo_measurement_median(scratch, count);
    for (index = 0U; index < owner->view.sample_count; index += 1U) {
        evo_project_measurement_sample_t *sample = &owner->samples[index];
        uint64_t deviation;

        if (!evo_measurement_sample_matches(
                sample, policy->workload_id, subject) ||
            !sample->completed || sample->timed_out || sample->failed) {
            continue;
        }
        deviation = sample->runtime_ns >= median ? sample->runtime_ns - median
                                                 : median - sample->runtime_ns;
        if (deviation > policy->outlier_deviation_ns &&
            !evo_measurement_set_exclusion(
                owner, index, "runtime-median-deviation")) {
            return false;
        }
    }
    return true;
}

static bool evo_measurement_aggregate_subject(
    const evo_project_measurement_owner_t *owner,
    const evo_project_measurement_workload_policy_t *policy,
    evo_project_measurement_subject_t subject,
    evo_project_measurement_aggregate_t *aggregate)
{
    const size_t capacity = policy->repetition_count;
    uint64_t *runtime = evo_project_allocate_zeroed(capacity, sizeof(*runtime));
    uint64_t *memory = evo_project_allocate_zeroed(capacity, sizeof(*memory));
    uint64_t *binary = evo_project_allocate_zeroed(capacity, sizeof(*binary));
    uint64_t *reliability =
        evo_project_allocate_zeroed(capacity, sizeof(*reliability));
    uint64_t *maintainability =
        evo_project_allocate_zeroed(capacity, sizeof(*maintainability));
    size_t count = 0U;
    size_t excluded = 0U;
    size_t index;

    if (runtime == NULL || memory == NULL || binary == NULL || reliability == NULL ||
        maintainability == NULL) {
        evo_project_release(runtime);
        evo_project_release(memory);
        evo_project_release(binary);
        evo_project_release(reliability);
        evo_project_release(maintainability);
        return false;
    }

    for (index = 0U; index < owner->view.sample_count; index += 1U) {
        const evo_project_measurement_sample_t *sample = &owner->samples[index];

        if (!evo_measurement_sample_matches(sample, policy->workload_id, subject)) {
            continue;
        }
        if (sample->excluded || !sample->completed || sample->timed_out ||
            sample->failed) {
            excluded += 1U;
            continue;
        }
        if (count >= capacity) {
            evo_project_release(runtime);
            evo_project_release(memory);
            evo_project_release(binary);
            evo_project_release(reliability);
            evo_project_release(maintainability);
            return false;
        }
        runtime[count] = sample->runtime_ns;
        memory[count] = sample->peak_memory_bytes;
        binary[count] = sample->binary_size_bytes;
        reliability[count] = sample->reliability_ppm;
        maintainability[count] = sample->maintainability_ppm;
        count += 1U;
    }

    aggregate->included_count = count;
    aggregate->excluded_count = excluded;
    if (count > 0U) {
        uint64_t runtime_min;
        uint64_t runtime_max;

        aggregate->runtime_ns = evo_measurement_median(runtime, count);
        runtime_min = runtime[0];
        runtime_max = runtime[count - 1U];
        aggregate->runtime_min_ns = runtime_min;
        aggregate->runtime_max_ns = runtime_max;
        aggregate->runtime_range_ppm = evo_measurement_range_ppm(
            runtime_min, runtime_max, aggregate->runtime_ns);
        aggregate->peak_memory_bytes = evo_measurement_median(memory, count);
        aggregate->binary_size_bytes = evo_measurement_median(binary, count);
        aggregate->reliability_ppm =
            (uint32_t)evo_measurement_median(reliability, count);
        aggregate->maintainability_ppm =
            (uint32_t)evo_measurement_median(maintainability, count);
    }

    evo_project_release(runtime);
    evo_project_release(memory);
    evo_project_release(binary);
    evo_project_release(reliability);
    evo_project_release(maintainability);
    return true;
}

static bool evo_measurement_workload_has_incomplete_sample(
    const evo_project_measurement_owner_t *owner,
    const char *workload_id)
{
    size_t index;

    for (index = 0U; index < owner->view.sample_count; index += 1U) {
        const evo_project_measurement_sample_t *sample = &owner->samples[index];

        if (sample->phase == EVO_PROJECT_MEASUREMENT_RECORDED &&
            strcmp(sample->workload_id, workload_id) == 0 &&
            (!sample->completed || sample->timed_out || sample->failed)) {
            return true;
        }
    }
    return false;
}

static evo_project_measurement_comparison_t evo_measurement_compare_runtime(
    const evo_project_measurement_workload_policy_t *policy,
    uint64_t baseline,
    uint64_t candidate)
{
    long double change_ppm;

    if (baseline == 0U) {
        return EVO_PROJECT_MEASUREMENT_INCOMPLETE;
    }
    change_ppm = ((long double)baseline - (long double)candidate) *
                 (long double)EVO_PROJECT_MEASUREMENT_PPM_SCALE /
                 (long double)baseline;
    if (change_ppm < -(long double)policy->comparison_tolerance_ppm) {
        return EVO_PROJECT_MEASUREMENT_SLOWER;
    }
    if (change_ppm > (long double)policy->comparison_tolerance_ppm &&
        change_ppm >= (long double)policy->minimum_improvement_ppm) {
        return EVO_PROJECT_MEASUREMENT_FASTER;
    }
    return EVO_PROJECT_MEASUREMENT_EQUAL;
}

static double evo_measurement_memory_improvement(
    const evo_project_measurement_workload_policy_t *policy,
    const evo_project_measurement_aggregate_t *baseline,
    const evo_project_measurement_aggregate_t *candidate)
{
    const double memory = evo_measurement_relative_improvement(
        baseline->peak_memory_bytes, candidate->peak_memory_bytes);
    const double binary = evo_measurement_relative_improvement(
        baseline->binary_size_bytes, candidate->binary_size_bytes);
    const double total = policy->peak_memory_mix_weight +
                         policy->binary_size_mix_weight;

    return ((memory * policy->peak_memory_mix_weight) +
            (binary * policy->binary_size_mix_weight)) /
           total;
}

static void evo_measurement_compute_fitness(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner)
{
    double workload_weight_total = 0.0;
    double performance_total = 0.0;
    double memory_total = 0.0;
    double reliability_total = 0.0;
    double maintainability_total = 0.0;
    size_t index;

    for (index = 0U; index < config->workload_count; index += 1U) {
        const double weight = config->workloads[index].workload_weight;
        const evo_project_measurement_workload_result_t *result =
            &owner->workloads[index];

        workload_weight_total += weight;
        performance_total += weight * result->runtime_improvement;
        memory_total += weight * result->memory_improvement;
        reliability_total += weight * result->reliability_improvement;
        maintainability_total += weight * result->maintainability_improvement;
    }

    owner->view.fitness.correctness = 1.0;
    owner->view.fitness.performance = performance_total / workload_weight_total;
    owner->view.fitness.memory_use = memory_total / workload_weight_total;
    owner->view.fitness.reliability = reliability_total / workload_weight_total;
    owner->view.fitness.maintainability =
        maintainability_total / workload_weight_total;
    owner->view.fitness.constraint_penalty = 0.0;
    owner->view.fitness.total =
        owner->view.fitness.correctness * config->fitness_weights.correctness +
        owner->view.fitness.performance * config->fitness_weights.performance +
        owner->view.fitness.memory_use * config->fitness_weights.memory_use +
        owner->view.fitness.reliability * config->fitness_weights.reliability +
        owner->view.fitness.maintainability *
            config->fitness_weights.maintainability -
        owner->view.fitness.constraint_penalty *
            config->fitness_weights.constraint_penalty;
    owner->view.fitness_available = isfinite(owner->view.fitness.total);
}

bool evo_measurement_finalize_workloads(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner)
{
    size_t max_repetitions = 0U;
    uint64_t *scratch;
    bool any_incomplete = false;
    bool any_unstable = false;
    bool any_slower = false;
    bool any_faster = false;
    size_t index;

    for (index = 0U; index < config->workload_count; index += 1U) {
        if (config->workloads[index].repetition_count > max_repetitions) {
            max_repetitions = config->workloads[index].repetition_count;
        }
    }
    scratch = evo_project_allocate_zeroed(max_repetitions, sizeof(*scratch));
    if (scratch == NULL) {
        return false;
    }

    for (index = 0U; index < config->workload_count; index += 1U) {
        const evo_project_measurement_workload_policy_t *policy =
            &config->workloads[index];
        evo_project_measurement_workload_result_t *result = &owner->workloads[index];

        if (!evo_measurement_apply_outliers(
                policy, owner, EVO_PROJECT_MEASUREMENT_BASELINE, scratch) ||
            !evo_measurement_apply_outliers(
                policy, owner, EVO_PROJECT_MEASUREMENT_CANDIDATE, scratch) ||
            !evo_measurement_aggregate_subject(
                owner, policy, EVO_PROJECT_MEASUREMENT_BASELINE, &result->baseline) ||
            !evo_measurement_aggregate_subject(
                owner, policy, EVO_PROJECT_MEASUREMENT_CANDIDATE, &result->candidate)) {
            evo_project_release(scratch);
            return false;
        }

        result->complete =
            !evo_measurement_workload_has_incomplete_sample(owner, policy->workload_id) &&
            result->baseline.included_count >= policy->minimum_included_repetitions &&
            result->candidate.included_count >= policy->minimum_included_repetitions;
        result->stable = result->complete &&
                         result->baseline.runtime_range_ppm <=
                             policy->max_runtime_range_ppm &&
                         result->candidate.runtime_range_ppm <=
                             policy->max_runtime_range_ppm;

        if (!result->complete) {
            result->comparison = EVO_PROJECT_MEASUREMENT_INCOMPLETE;
            any_incomplete = true;
            continue;
        }
        if (!result->stable) {
            result->comparison = EVO_PROJECT_MEASUREMENT_UNSTABLE;
            any_unstable = true;
            continue;
        }

        result->comparison = evo_measurement_compare_runtime(
            policy, result->baseline.runtime_ns, result->candidate.runtime_ns);
        result->runtime_improvement = evo_measurement_relative_improvement(
            result->baseline.runtime_ns, result->candidate.runtime_ns);
        result->memory_improvement = evo_measurement_memory_improvement(
            policy, &result->baseline, &result->candidate);
        result->reliability_improvement =
            ((double)result->candidate.reliability_ppm -
             (double)result->baseline.reliability_ppm) /
            (double)EVO_PROJECT_MEASUREMENT_PPM_SCALE;
        result->maintainability_improvement =
            ((double)result->candidate.maintainability_ppm -
             (double)result->baseline.maintainability_ppm) /
            (double)EVO_PROJECT_MEASUREMENT_PPM_SCALE;
        any_slower = any_slower ||
                     result->comparison == EVO_PROJECT_MEASUREMENT_SLOWER;
        any_faster = any_faster ||
                     result->comparison == EVO_PROJECT_MEASUREMENT_FASTER;
    }

    evo_project_release(scratch);
    if (any_incomplete) {
        owner->view.overall_comparison = EVO_PROJECT_MEASUREMENT_INCOMPLETE;
    } else if (any_unstable) {
        owner->view.overall_comparison = EVO_PROJECT_MEASUREMENT_UNSTABLE;
    } else if (any_slower) {
        owner->view.overall_comparison = EVO_PROJECT_MEASUREMENT_SLOWER;
    } else if (any_faster) {
        owner->view.overall_comparison = EVO_PROJECT_MEASUREMENT_FASTER;
    } else {
        owner->view.overall_comparison = EVO_PROJECT_MEASUREMENT_EQUAL;
    }

    if (!any_incomplete && !any_unstable) {
        evo_measurement_compute_fitness(config, owner);
    }
    return true;
}

static bool evo_measurement_append_bool(
    evo_candidate_buffer_t *buffer,
    bool value)
{
    return evo_candidate_buffer_append_text(buffer, value ? "true" : "false");
}

static bool evo_measurement_append_double(
    evo_candidate_buffer_t *buffer,
    double value)
{
    char text[64];
    const int written = evo_project_format(text, sizeof(text), "%.17g", value);

    return written > 0 && (size_t)written < sizeof(text) &&
           evo_candidate_buffer_append_text(buffer, text);
}

static bool evo_measurement_append_u32(
    evo_candidate_buffer_t *buffer,
    uint32_t value)
{
    return evo_candidate_buffer_append_u64(buffer, (uint64_t)value);
}

static bool evo_measurement_append_json_field(
    evo_candidate_buffer_t *buffer,
    const char *name,
    const char *value,
    bool comma)
{
    return evo_candidate_buffer_append_json_string(buffer, name) &&
           evo_candidate_buffer_append_text(buffer, ":") &&
           evo_candidate_buffer_append_json_string(buffer, value) &&
           (!comma || evo_candidate_buffer_append_text(buffer, ","));
}

static bool evo_measurement_append_aggregate_json(
    evo_candidate_buffer_t *buffer,
    const evo_project_measurement_aggregate_t *aggregate)
{
    return evo_candidate_buffer_append_text(buffer, "{") &&
           evo_candidate_buffer_append_text(buffer, "\"runtime_ns\":") &&
           evo_candidate_buffer_append_u64(buffer, aggregate->runtime_ns) &&
           evo_candidate_buffer_append_text(buffer, ",\"peak_memory_bytes\":") &&
           evo_candidate_buffer_append_u64(buffer, aggregate->peak_memory_bytes) &&
           evo_candidate_buffer_append_text(buffer, ",\"binary_size_bytes\":") &&
           evo_candidate_buffer_append_u64(buffer, aggregate->binary_size_bytes) &&
           evo_candidate_buffer_append_text(buffer, ",\"reliability_ppm\":") &&
           evo_measurement_append_u32(buffer, aggregate->reliability_ppm) &&
           evo_candidate_buffer_append_text(buffer, ",\"maintainability_ppm\":") &&
           evo_measurement_append_u32(buffer, aggregate->maintainability_ppm) &&
           evo_candidate_buffer_append_text(buffer, ",\"runtime_min_ns\":") &&
           evo_candidate_buffer_append_u64(buffer, aggregate->runtime_min_ns) &&
           evo_candidate_buffer_append_text(buffer, ",\"runtime_max_ns\":") &&
           evo_candidate_buffer_append_u64(buffer, aggregate->runtime_max_ns) &&
           evo_candidate_buffer_append_text(buffer, ",\"runtime_range_ppm\":") &&
           evo_measurement_append_u32(buffer, aggregate->runtime_range_ppm) &&
           evo_candidate_buffer_append_text(buffer, ",\"included_count\":") &&
           evo_candidate_buffer_append_size(buffer, aggregate->included_count) &&
           evo_candidate_buffer_append_text(buffer, ",\"excluded_count\":") &&
           evo_candidate_buffer_append_size(buffer, aggregate->excluded_count) &&
           evo_candidate_buffer_append_text(buffer, "}");
}

static bool evo_measurement_build_json(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner,
    evo_candidate_buffer_t *json)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(json, "{") ||
        !evo_candidate_buffer_append_text(json, "\"schema\":\"catalyst.evo-project-measurement.v1\",") ||
        !evo_measurement_append_json_field(
            json, "candidate_fingerprint", owner->view.candidate_fingerprint, true) ||
        !evo_measurement_append_json_field(
            json, "assurance_fingerprint", owner->view.assurance_fingerprint, true) ||
        !evo_measurement_append_json_field(
            json, "baseline_identity", owner->view.baseline_identity, true) ||
        !evo_measurement_append_json_field(json, "policy_id", owner->view.policy_id, true) ||
        !evo_measurement_append_json_field(
            json, "policy_fingerprint", owner->view.policy_fingerprint, true) ||
        !evo_measurement_append_json_field(
            json,
            "measurement_provider_identity",
            owner->view.measurement_provider_identity,
            true) ||
        !evo_measurement_append_json_field(
            json, "condition_fingerprint", owner->view.condition_fingerprint, true) ||
        !evo_candidate_buffer_append_text(json, "\"condition\":{") ||
        !evo_measurement_append_json_field(
            json, "hardware_identity", config->condition.hardware_identity, true) ||
        !evo_measurement_append_json_field(
            json,
            "operating_system_identity",
            config->condition.operating_system_identity,
            true) ||
        !evo_measurement_append_json_field(
            json, "compiler_identity", config->condition.compiler_identity, true) ||
        !evo_measurement_append_json_field(
            json, "linker_identity", config->condition.linker_identity, true) ||
        !evo_measurement_append_json_field(
            json, "environment_identity", config->condition.environment_identity, true) ||
        !evo_measurement_append_json_field(
            json, "dataset_identity", config->condition.dataset_identity, true) ||
        !evo_measurement_append_json_field(
            json,
            "baseline_binary_identity",
            config->condition.baseline_binary_identity,
            true) ||
        !evo_measurement_append_json_field(
            json,
            "candidate_binary_identity",
            config->condition.candidate_binary_identity,
            false) ||
        !evo_candidate_buffer_append_text(json, "},\"workloads\":[")) {
        return false;
    }

    for (index = 0U; index < owner->view.workload_count; index += 1U) {
        const evo_project_measurement_workload_result_t *result =
            &owner->workloads[index];
        const evo_project_measurement_workload_policy_t *policy =
            &config->workloads[index];

        if (index > 0U && !evo_candidate_buffer_append_text(json, ",")) {
            return false;
        }
        if (!evo_candidate_buffer_append_text(json, "{") ||
            !evo_measurement_append_json_field(
                json, "workload_id", result->workload_id, true) ||
            !evo_candidate_buffer_append_text(json, "\"warmup_count\":") ||
            !evo_candidate_buffer_append_size(json, policy->warmup_count) ||
            !evo_candidate_buffer_append_text(json, ",\"repetition_count\":") ||
            !evo_candidate_buffer_append_size(json, policy->repetition_count) ||
            !evo_candidate_buffer_append_text(
                json, ",\"minimum_included_repetitions\":") ||
            !evo_candidate_buffer_append_size(
                json, policy->minimum_included_repetitions) ||
            !evo_candidate_buffer_append_text(json, ",\"max_runtime_range_ppm\":") ||
            !evo_measurement_append_u32(json, policy->max_runtime_range_ppm) ||
            !evo_candidate_buffer_append_text(
                json, ",\"comparison_tolerance_ppm\":") ||
            !evo_measurement_append_u32(json, policy->comparison_tolerance_ppm) ||
            !evo_candidate_buffer_append_text(
                json, ",\"minimum_improvement_ppm\":") ||
            !evo_measurement_append_u32(json, policy->minimum_improvement_ppm) ||
            !evo_candidate_buffer_append_text(json, ",\"baseline\":") ||
            !evo_measurement_append_aggregate_json(json, &result->baseline) ||
            !evo_candidate_buffer_append_text(json, ",\"candidate\":") ||
            !evo_measurement_append_aggregate_json(json, &result->candidate) ||
            !evo_candidate_buffer_append_text(json, ",\"comparison\":") ||
            !evo_candidate_buffer_append_json_string(
                json, evo_project_measurement_comparison_name(result->comparison)) ||
            !evo_candidate_buffer_append_text(json, ",\"complete\":") ||
            !evo_measurement_append_bool(json, result->complete) ||
            !evo_candidate_buffer_append_text(json, ",\"stable\":") ||
            !evo_measurement_append_bool(json, result->stable) ||
            !evo_candidate_buffer_append_text(json, ",\"runtime_improvement\":") ||
            !evo_measurement_append_double(json, result->runtime_improvement) ||
            !evo_candidate_buffer_append_text(json, ",\"memory_improvement\":") ||
            !evo_measurement_append_double(json, result->memory_improvement) ||
            !evo_candidate_buffer_append_text(
                json, ",\"reliability_improvement\":") ||
            !evo_measurement_append_double(json, result->reliability_improvement) ||
            !evo_candidate_buffer_append_text(
                json, ",\"maintainability_improvement\":") ||
            !evo_measurement_append_double(json, result->maintainability_improvement) ||
            !evo_candidate_buffer_append_text(json, "}")) {
            return false;
        }
    }

    if (!evo_candidate_buffer_append_text(json, "],\"samples\":[")) {
        return false;
    }
    for (index = 0U; index < owner->view.sample_count; index += 1U) {
        const evo_project_measurement_sample_t *sample = &owner->samples[index];
        char fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];

        evo_project_fingerprint_format(sample->condition_fingerprint, fingerprint);
        if (index > 0U && !evo_candidate_buffer_append_text(json, ",")) {
            return false;
        }
        if (!evo_candidate_buffer_append_text(json, "{") ||
            !evo_measurement_append_json_field(
                json, "workload_id", sample->workload_id, true) ||
            !evo_candidate_buffer_append_text(json, "\"subject\":") ||
            !evo_candidate_buffer_append_json_string(
                json, evo_project_measurement_subject_name(sample->subject)) ||
            !evo_candidate_buffer_append_text(json, ",\"phase\":") ||
            !evo_candidate_buffer_append_json_string(
                json, evo_project_measurement_phase_name(sample->phase)) ||
            !evo_candidate_buffer_append_text(json, ",\"pair_index\":") ||
            !evo_candidate_buffer_append_size(json, sample->pair_index) ||
            !evo_candidate_buffer_append_text(json, ",\"sequence_index\":") ||
            !evo_candidate_buffer_append_size(json, sample->sequence_index) ||
            !evo_candidate_buffer_append_text(json, ",\"completed\":") ||
            !evo_measurement_append_bool(json, sample->completed) ||
            !evo_candidate_buffer_append_text(json, ",\"timed_out\":") ||
            !evo_measurement_append_bool(json, sample->timed_out) ||
            !evo_candidate_buffer_append_text(json, ",\"failed\":") ||
            !evo_measurement_append_bool(json, sample->failed) ||
            !evo_candidate_buffer_append_text(json, ",\"excluded\":") ||
            !evo_measurement_append_bool(json, sample->excluded) ||
            !evo_candidate_buffer_append_text(json, ",\"exclusion_reason\":") ||
            !evo_candidate_buffer_append_json_string(
                json,
                sample->exclusion_reason == NULL ? "" : sample->exclusion_reason) ||
            !evo_candidate_buffer_append_text(json, ",\"condition_fingerprint\":") ||
            !evo_candidate_buffer_append_json_string(json, fingerprint) ||
            !evo_candidate_buffer_append_text(json, ",\"runtime_ns\":") ||
            !evo_candidate_buffer_append_u64(json, sample->runtime_ns) ||
            !evo_candidate_buffer_append_text(json, ",\"peak_memory_bytes\":") ||
            !evo_candidate_buffer_append_u64(json, sample->peak_memory_bytes) ||
            !evo_candidate_buffer_append_text(json, ",\"binary_size_bytes\":") ||
            !evo_candidate_buffer_append_u64(json, sample->binary_size_bytes) ||
            !evo_candidate_buffer_append_text(json, ",\"reliability_ppm\":") ||
            !evo_measurement_append_u32(json, sample->reliability_ppm) ||
            !evo_candidate_buffer_append_text(json, ",\"maintainability_ppm\":") ||
            !evo_measurement_append_u32(json, sample->maintainability_ppm) ||
            !evo_candidate_buffer_append_text(json, "}")) {
            return false;
        }
    }

    if (!evo_candidate_buffer_append_text(json, "],\"overall_comparison\":") ||
        !evo_candidate_buffer_append_json_string(
            json,
            evo_project_measurement_comparison_name(owner->view.overall_comparison)) ||
        !evo_candidate_buffer_append_text(json, ",\"fitness_available\":") ||
        !evo_measurement_append_bool(json, owner->view.fitness_available) ||
        !evo_candidate_buffer_append_text(json, ",\"correctness_preserved\":true,") ||
        !evo_candidate_buffer_append_text(json, "\"fitness\":{") ||
        !evo_candidate_buffer_append_text(json, "\"correctness\":") ||
        !evo_measurement_append_double(json, owner->view.fitness.correctness) ||
        !evo_candidate_buffer_append_text(json, ",\"performance\":") ||
        !evo_measurement_append_double(json, owner->view.fitness.performance) ||
        !evo_candidate_buffer_append_text(json, ",\"memory_use\":") ||
        !evo_measurement_append_double(json, owner->view.fitness.memory_use) ||
        !evo_candidate_buffer_append_text(json, ",\"reliability\":") ||
        !evo_measurement_append_double(json, owner->view.fitness.reliability) ||
        !evo_candidate_buffer_append_text(json, ",\"maintainability\":") ||
        !evo_measurement_append_double(json, owner->view.fitness.maintainability) ||
        !evo_candidate_buffer_append_text(json, ",\"constraint_penalty\":") ||
        !evo_measurement_append_double(json, owner->view.fitness.constraint_penalty) ||
        !evo_candidate_buffer_append_text(json, ",\"total\":") ||
        !evo_measurement_append_double(json, owner->view.fitness.total) ||
        !evo_candidate_buffer_append_text(json, "},\"fitness_weights\":{") ||
        !evo_candidate_buffer_append_text(json, "\"correctness\":") ||
        !evo_measurement_append_double(json, config->fitness_weights.correctness) ||
        !evo_candidate_buffer_append_text(json, ",\"performance\":") ||
        !evo_measurement_append_double(json, config->fitness_weights.performance) ||
        !evo_candidate_buffer_append_text(json, ",\"memory_use\":") ||
        !evo_measurement_append_double(json, config->fitness_weights.memory_use) ||
        !evo_candidate_buffer_append_text(json, ",\"reliability\":") ||
        !evo_measurement_append_double(json, config->fitness_weights.reliability) ||
        !evo_candidate_buffer_append_text(json, ",\"maintainability\":") ||
        !evo_measurement_append_double(json, config->fitness_weights.maintainability) ||
        !evo_candidate_buffer_append_text(json, ",\"constraint_penalty\":") ||
        !evo_measurement_append_double(json, config->fitness_weights.constraint_penalty) ||
        !evo_candidate_buffer_append_text(json, "},\"projection_complete\":true,") ||
        !evo_candidate_buffer_append_text(json, "\"probabilistic_authority\":false,") ||
        !evo_measurement_append_json_field(
            json,
            "measurement_fingerprint",
            owner->view.measurement_fingerprint,
            false) ||
        !evo_candidate_buffer_append_text(json, "}\n")) {
        return false;
    }
    return true;
}

static bool evo_measurement_build_markdown(
    const evo_project_measurement_config_t *config,
    const evo_project_measurement_owner_t *owner,
    evo_candidate_buffer_t *markdown)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(
            markdown, "# EVO Candidate Measurement and Fitness\n\n") ||
        !evo_candidate_buffer_append_text(markdown, "- Candidate: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.candidate_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Assurance: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.assurance_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Baseline: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.baseline_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Policy: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.policy_id) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Condition: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.condition_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Overall comparison: **") ||
        !evo_candidate_buffer_append_text(
            markdown,
            evo_project_measurement_comparison_name(owner->view.overall_comparison)) ||
        !evo_candidate_buffer_append_text(markdown, "**\n- Fitness available: **") ||
        !evo_candidate_buffer_append_text(
            markdown, owner->view.fitness_available ? "yes" : "no") ||
        !evo_candidate_buffer_append_text(
            markdown,
            "**\n\nCorrectness authority is unchanged from candidate assurance; performance evidence cannot alter it.\n\n") ||
        !evo_candidate_buffer_append_text(
            markdown,
            "## Workloads\n\n| Workload | Comparison | Baseline ns | Candidate ns | Baseline range ppm | Candidate range ppm | Included B/C |\n|---|---|---:|---:|---:|---:|---:|\n")) {
        return false;
    }

    for (index = 0U; index < owner->view.workload_count; index += 1U) {
        const evo_project_measurement_workload_result_t *result =
            &owner->workloads[index];
        char row[256];
        const int written = evo_project_format(
            row,
            sizeof(row),
            "| %s | %s | %llu | %llu | %u | %u | %zu/%zu |\n",
            result->workload_id,
            evo_project_measurement_comparison_name(result->comparison),
            (unsigned long long)result->baseline.runtime_ns,
            (unsigned long long)result->candidate.runtime_ns,
            result->baseline.runtime_range_ppm,
            result->candidate.runtime_range_ppm,
            result->baseline.included_count,
            result->candidate.included_count);

        if (written <= 0 || (size_t)written >= sizeof(row) ||
            !evo_candidate_buffer_append_text(markdown, row)) {
            return false;
        }
    }

    if (!evo_candidate_buffer_append_text(
            markdown,
            "\n## Raw sample trace\n\n| Seq | Workload | Phase | Subject | Runtime ns | Memory bytes | Binary bytes | Excluded |\n|---:|---|---|---|---:|---:|---:|---|\n")) {
        return false;
    }
    for (index = 0U; index < owner->view.sample_count; index += 1U) {
        const evo_project_measurement_sample_t *sample = &owner->samples[index];
        char row[320];
        const int written = evo_project_format(
            row,
            sizeof(row),
            "| %zu | %s | %s | %s | %llu | %llu | %llu | %s |\n",
            sample->sequence_index,
            sample->workload_id,
            evo_project_measurement_phase_name(sample->phase),
            evo_project_measurement_subject_name(sample->subject),
            (unsigned long long)sample->runtime_ns,
            (unsigned long long)sample->peak_memory_bytes,
            (unsigned long long)sample->binary_size_bytes,
            sample->excluded
                ? (sample->exclusion_reason == NULL ? "yes" : sample->exclusion_reason)
                : "no");

        if (written <= 0 || (size_t)written >= sizeof(row) ||
            !evo_candidate_buffer_append_text(markdown, row)) {
            return false;
        }
    }

    if (!evo_candidate_buffer_append_text(
            markdown,
            "\n## Fitness\n\nThe scalar total is derived only from the recorded EVO fitness components and caller-declared weights. No default consumer objective is supplied.\n\n") ||
        !evo_candidate_buffer_append_text(
            markdown,
            "A later selected result may be described only as the **best verified candidate found within the recorded bounded search contract**; this evidence does not claim a globally optimal program.\n")) {
        return false;
    }
    (void)config;
    return true;
}

bool evo_measurement_build_evidence(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner)
{
    evo_candidate_buffer_t json = {0};
    evo_candidate_buffer_t markdown = {0};

    if (!evo_candidate_buffer_open(&json, 4096U) ||
        !evo_candidate_buffer_open(&markdown, 4096U)) {
        evo_candidate_buffer_close(&json);
        evo_candidate_buffer_close(&markdown);
        return false;
    }
    if (!evo_measurement_build_json(config, owner, &json) ||
        !evo_measurement_build_markdown(config, owner, &markdown)) {
        evo_candidate_buffer_close(&json);
        evo_candidate_buffer_close(&markdown);
        return false;
    }
    owner->canonical_json = json.bytes;
    owner->audit_markdown = markdown.bytes;
    owner->view.canonical_json = owner->canonical_json;
    owner->view.canonical_json_size = json.size;
    owner->view.audit_markdown = owner->audit_markdown;
    owner->view.audit_markdown_size = markdown.size;
    return true;
}

static evo_project_measurement_status_t evo_measurement_write_file(
    int output_fd,
    const char *name,
    const char *bytes,
    size_t byte_count)
{
    int file_fd = openat(
        output_fd,
        name,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    evo_project_candidate_status_t write_status;

    if (file_fd < 0) {
        return EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE;
    }
    write_status = evo_candidate_write_all(
        file_fd, (const unsigned char *)bytes, byte_count);
    if (write_status != EVO_PROJECT_CANDIDATE_SUCCESS || fsync(file_fd) != 0) {
        (void)close(file_fd);
        return EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE;
    }
    if (close(file_fd) != 0) {
        return EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE;
    }
    return EVO_PROJECT_MEASUREMENT_SUCCESS;
}

evo_project_measurement_status_t evo_measurement_publish_evidence(
    const evo_project_measurement_owner_t *owner)
{
    const char marker[] = "incomplete\n";
    int output_fd = -1;
    int marker_fd = -1;
    evo_project_measurement_status_t status = EVO_PROJECT_MEASUREMENT_SUCCESS;

    if (mkdir(owner->output_path, 0700) != 0) {
        return errno == EEXIST ? EVO_PROJECT_MEASUREMENT_ERROR_OUTPUT_EXISTS
                               : EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE;
    }
    output_fd = open(
        owner->output_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (output_fd < 0) {
        status = EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE;
        goto finish;
    }
    marker_fd = openat(
        output_fd,
        ".evo-measurement-incomplete-v1",
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    if (marker_fd < 0 ||
        evo_candidate_write_all(
            marker_fd,
            (const unsigned char *)marker,
            sizeof(marker) - 1U) != EVO_PROJECT_CANDIDATE_SUCCESS ||
        fsync(marker_fd) != 0) {
        status = EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE;
        goto finish;
    }
    if (close(marker_fd) != 0) {
        marker_fd = -1;
        status = EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE;
        goto finish;
    }
    marker_fd = -1;
    status = evo_measurement_write_file(
        output_fd,
        "measurement.json",
        owner->canonical_json,
        owner->view.canonical_json_size);
    if (status == EVO_PROJECT_MEASUREMENT_SUCCESS) {
        status = evo_measurement_write_file(
            output_fd,
            "measurement.md",
            owner->audit_markdown,
            owner->view.audit_markdown_size);
    }
    if (status == EVO_PROJECT_MEASUREMENT_SUCCESS &&
        (fsync(output_fd) != 0 ||
         unlinkat(output_fd, ".evo-measurement-incomplete-v1", 0) != 0 ||
         fsync(output_fd) != 0)) {
        status = EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE;
    }

finish:
    if (marker_fd >= 0) {
        (void)close(marker_fd);
    }
    if (output_fd >= 0) {
        (void)close(output_fd);
    }
    if (status != EVO_PROJECT_MEASUREMENT_SUCCESS) {
        (void)evo_candidate_remove_tree(owner->output_path);
    }
    return status;
}
