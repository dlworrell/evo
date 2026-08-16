#include "internal/project_measurement_internal.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <math.h>
#include <string.h>

static bool evo_measurement_text_valid(const char *value, size_t maximum_bytes)
{
    size_t index;

    if (value == NULL || maximum_bytes == 0U) {
        return false;
    }
    for (index = 0U; index < maximum_bytes; index += 1U) {
        if (value[index] == '\0') {
            return index > 0U;
        }
    }
    return false;
}

static bool evo_measurement_weight_valid(double value)
{
    return isfinite(value) && value >= 0.0;
}

static void evo_measurement_fingerprint_double(
    evo_project_fingerprint_t *fingerprint,
    double value)
{
    char text[48];
    const int written = evo_project_format(text, sizeof(text), "%.17g", value);

    if (written > 0 && (size_t)written < sizeof(text)) {
        evo_project_fingerprint_string(fingerprint, text);
    } else {
        evo_project_fingerprint_string(fingerprint, "invalid-double");
    }
}

static bool evo_measurement_workload_valid(
    const evo_project_measurement_config_t *config,
    const evo_project_measurement_workload_policy_t *policy)
{
    if (!evo_measurement_text_valid(
            policy->workload_id, config->limits.max_string_bytes) ||
        policy->repetition_count == 0U ||
        policy->minimum_included_repetitions == 0U ||
        policy->minimum_included_repetitions > policy->repetition_count ||
        (policy->order != EVO_PROJECT_MEASUREMENT_ALTERNATE_BASELINE_FIRST &&
         policy->order != EVO_PROJECT_MEASUREMENT_ALTERNATE_CANDIDATE_FIRST) ||
        (policy->outlier_policy != EVO_PROJECT_MEASUREMENT_OUTLIER_NONE &&
         policy->outlier_policy !=
             EVO_PROJECT_MEASUREMENT_OUTLIER_ABSOLUTE_MEDIAN) ||
        policy->max_runtime_range_ppm > EVO_PROJECT_MEASUREMENT_PPM_SCALE ||
        policy->comparison_tolerance_ppm > EVO_PROJECT_MEASUREMENT_PPM_SCALE ||
        policy->minimum_improvement_ppm > EVO_PROJECT_MEASUREMENT_PPM_SCALE ||
        policy->timeout_ms == 0U ||
        policy->timeout_ms > config->limits.max_timeout_ms ||
        !evo_measurement_weight_valid(policy->workload_weight) ||
        policy->workload_weight == 0.0 ||
        !evo_measurement_weight_valid(policy->peak_memory_mix_weight) ||
        !evo_measurement_weight_valid(policy->binary_size_mix_weight) ||
        policy->peak_memory_mix_weight + policy->binary_size_mix_weight <= 0.0) {
        return false;
    }
    if (policy->outlier_policy == EVO_PROJECT_MEASUREMENT_OUTLIER_ABSOLUTE_MEDIAN &&
        policy->outlier_deviation_ns == 0U) {
        return false;
    }
    return true;
}

static bool evo_measurement_weights_valid(
    const evo_project_measurement_fitness_weights_t *weights)
{
    const double total = weights->correctness + weights->performance +
                         weights->memory_use + weights->reliability +
                         weights->maintainability + weights->constraint_penalty;

    return evo_measurement_weight_valid(weights->correctness) &&
           evo_measurement_weight_valid(weights->performance) &&
           evo_measurement_weight_valid(weights->memory_use) &&
           evo_measurement_weight_valid(weights->reliability) &&
           evo_measurement_weight_valid(weights->maintainability) &&
           evo_measurement_weight_valid(weights->constraint_penalty) &&
           isfinite(total) && total > 0.0;
}

bool evo_measurement_config_valid(const evo_project_measurement_config_t *config)
{
    size_t index;
    size_t sample_count = 0U;

    if (config == NULL || config->assurance == NULL ||
        config->assurance->private_owner == NULL ||
        config->assurance->schema_version != EVO_PROJECT_ASSURANCE_SCHEMA_VERSION ||
        !config->assurance->performance_eligible ||
        !config->assurance->projection_complete ||
        config->assurance->probabilistic_authority ||
        !evo_measurement_text_valid(
            config->assurance->candidate_fingerprint,
            config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->assurance->assurance_fingerprint,
            config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->baseline_identity, config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->policy_id, config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->measurement_provider_identity,
            config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->output_path, config->limits.max_string_bytes) ||
        config->workload_count == 0U ||
        config->workload_count > config->limits.max_workloads ||
        config->workloads == NULL || config->provider == NULL ||
        config->limits.max_string_bytes == 0U ||
        config->limits.max_workloads == 0U ||
        config->limits.max_samples == 0U ||
        config->limits.max_evidence_bytes == 0U ||
        config->limits.max_timeout_ms == 0U ||
        !evo_measurement_weights_valid(&config->fitness_weights)) {
        return false;
    }

    if (!evo_measurement_text_valid(
            config->condition.hardware_identity, config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->condition.operating_system_identity,
            config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->condition.compiler_identity, config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->condition.linker_identity, config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->condition.environment_identity,
            config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->condition.dataset_identity, config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->condition.baseline_binary_identity,
            config->limits.max_string_bytes) ||
        !evo_measurement_text_valid(
            config->condition.candidate_binary_identity,
            config->limits.max_string_bytes)) {
        return false;
    }

    for (index = 0U; index < config->workload_count; index += 1U) {
        const evo_project_measurement_workload_policy_t *policy =
            &config->workloads[index];
        size_t pair_count;
        size_t workload_samples;
        size_t previous;

        if (!evo_measurement_workload_valid(config, policy)) {
            return false;
        }
        for (previous = 0U; previous < index; previous += 1U) {
            if (strcmp(
                    config->workloads[previous].workload_id,
                    policy->workload_id) == 0) {
                return false;
            }
        }
        if (policy->warmup_count > SIZE_MAX - policy->repetition_count) {
            return false;
        }
        pair_count = policy->warmup_count + policy->repetition_count;
        if (pair_count > SIZE_MAX / 2U) {
            return false;
        }
        workload_samples = pair_count * 2U;
        if (sample_count > SIZE_MAX - workload_samples) {
            return false;
        }
        sample_count += workload_samples;
    }
    return sample_count <= config->limits.max_samples;
}

char *evo_measurement_duplicate(const char *value)
{
    size_t size;
    char *copy;

    if (value == NULL) {
        return NULL;
    }
    size = strlen(value) + 1U;
    copy = evo_project_allocate_zeroed(size, sizeof(*copy));
    if (copy != NULL) {
        (void)memcpy(copy, value, size);
    }
    return copy;
}

uint64_t evo_measurement_condition_fingerprint(
    const evo_project_measurement_config_t *config)
{
    evo_project_fingerprint_t fingerprint;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(&fingerprint, "evo-project-measurement-condition-v1");
    evo_project_fingerprint_string(&fingerprint, config->baseline_identity);
    evo_project_fingerprint_string(
        &fingerprint, config->assurance->candidate_fingerprint);
    evo_project_fingerprint_string(&fingerprint, config->condition.hardware_identity);
    evo_project_fingerprint_string(
        &fingerprint, config->condition.operating_system_identity);
    evo_project_fingerprint_string(&fingerprint, config->condition.compiler_identity);
    evo_project_fingerprint_string(&fingerprint, config->condition.linker_identity);
    evo_project_fingerprint_string(
        &fingerprint, config->condition.environment_identity);
    evo_project_fingerprint_string(&fingerprint, config->condition.dataset_identity);
    evo_project_fingerprint_string(
        &fingerprint, config->condition.baseline_binary_identity);
    evo_project_fingerprint_string(
        &fingerprint, config->condition.candidate_binary_identity);
    return fingerprint.value;
}

uint64_t evo_measurement_policy_fingerprint(
    const evo_project_measurement_config_t *config)
{
    evo_project_fingerprint_t fingerprint;
    size_t index;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(&fingerprint, "evo-project-measurement-policy-v1");
    evo_project_fingerprint_string(&fingerprint, config->policy_id);
    evo_project_fingerprint_string(
        &fingerprint, config->measurement_provider_identity);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)config->workload_count);
    for (index = 0U; index < config->workload_count; index += 1U) {
        const evo_project_measurement_workload_policy_t *policy =
            &config->workloads[index];

        evo_project_fingerprint_string(&fingerprint, policy->workload_id);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)policy->warmup_count);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)policy->repetition_count);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)policy->minimum_included_repetitions);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)policy->order);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)policy->outlier_policy);
        evo_project_fingerprint_u64(
            &fingerprint, policy->outlier_deviation_ns);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)policy->max_runtime_range_ppm);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)policy->comparison_tolerance_ppm);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)policy->minimum_improvement_ppm);
        evo_project_fingerprint_u64(&fingerprint, policy->timeout_ms);
        evo_measurement_fingerprint_double(&fingerprint, policy->workload_weight);
        evo_measurement_fingerprint_double(
            &fingerprint, policy->peak_memory_mix_weight);
        evo_measurement_fingerprint_double(
            &fingerprint, policy->binary_size_mix_weight);
    }
    evo_measurement_fingerprint_double(
        &fingerprint, config->fitness_weights.correctness);
    evo_measurement_fingerprint_double(
        &fingerprint, config->fitness_weights.performance);
    evo_measurement_fingerprint_double(
        &fingerprint, config->fitness_weights.memory_use);
    evo_measurement_fingerprint_double(
        &fingerprint, config->fitness_weights.reliability);
    evo_measurement_fingerprint_double(
        &fingerprint, config->fitness_weights.maintainability);
    evo_measurement_fingerprint_double(
        &fingerprint, config->fitness_weights.constraint_penalty);
    return fingerprint.value;
}

const char *evo_project_measurement_status_name(
    evo_project_measurement_status_t status)
{
    switch (status) {
    case EVO_PROJECT_MEASUREMENT_SUCCESS:
        return "success";
    case EVO_PROJECT_MEASUREMENT_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_MEASUREMENT_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_MEASUREMENT_ERROR_ASSURANCE_INELIGIBLE:
        return "assurance-ineligible";
    case EVO_PROJECT_MEASUREMENT_ERROR_POLICY_INVALID:
        return "policy-invalid";
    case EVO_PROJECT_MEASUREMENT_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_MEASUREMENT_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_MEASUREMENT_ERROR_PROVIDER:
        return "provider";
    case EVO_PROJECT_MEASUREMENT_ERROR_OUTPUT_EXISTS:
        return "output-exists";
    case EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE:
        return "evidence";
    case EVO_PROJECT_MEASUREMENT_ERROR_STATE:
        return "state";
    default:
        return "unknown";
    }
}

const char *evo_project_measurement_subject_name(
    evo_project_measurement_subject_t subject)
{
    switch (subject) {
    case EVO_PROJECT_MEASUREMENT_BASELINE:
        return "baseline";
    case EVO_PROJECT_MEASUREMENT_CANDIDATE:
        return "candidate";
    default:
        return "unknown";
    }
}

const char *evo_project_measurement_phase_name(
    evo_project_measurement_phase_t phase)
{
    switch (phase) {
    case EVO_PROJECT_MEASUREMENT_WARMUP:
        return "warmup";
    case EVO_PROJECT_MEASUREMENT_RECORDED:
        return "recorded";
    default:
        return "unknown";
    }
}

const char *evo_project_measurement_comparison_name(
    evo_project_measurement_comparison_t comparison)
{
    switch (comparison) {
    case EVO_PROJECT_MEASUREMENT_INCOMPLETE:
        return "incomplete";
    case EVO_PROJECT_MEASUREMENT_UNSTABLE:
        return "unstable";
    case EVO_PROJECT_MEASUREMENT_SLOWER:
        return "slower";
    case EVO_PROJECT_MEASUREMENT_EQUAL:
        return "equal";
    case EVO_PROJECT_MEASUREMENT_FASTER:
        return "faster";
    default:
        return "unknown";
    }
}
