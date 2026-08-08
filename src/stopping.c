#include "internal/stopping.h"

#include "internal/adaptive_mutation.h"
#include "internal/fitness.h"
#include "internal/secure_erasure.h"

#include <math.h>
#include <stdint.h>

static bool disabled_controls_are_canonical(const evo_config_t *config)
{
    return (config->fitness_target_enabled || config->fitness_target == 0.0) &&
           (config->stagnation_enabled ||
            (config->improvement_tolerance == 0.0 &&
             config->stagnation_patience == 0)) &&
           (config->diversity_floor_enabled ||
            config->diversity_floor == 0.0);
}

evo_status_t evo_stopping_validate_config(const evo_config_t *config)
{
    if (config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!disabled_controls_are_canonical(config) ||
        (config->fitness_target_enabled &&
         !isfinite(config->fitness_target)) ||
        (config->stagnation_enabled &&
         (!isfinite(config->improvement_tolerance) ||
          config->improvement_tolerance < 0.0 ||
          config->stagnation_patience == 0)) ||
        (config->diversity_floor_enabled &&
         (!isfinite(config->diversity_floor) ||
          config->diversity_floor < 0.0 ||
          config->diversity_floor > 1.0))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    return EVO_SUCCESS;
}

static bool committed_result_is_valid(const evo_config_t *config,
                                      const evo_result_t *result,
                                      bool current_has_best)
{
    const evo_generation_statistics_t *statistics = NULL;

    if (result == NULL || result->best_genome == NULL ||
        result->best_genome_size == 0 ||
        result->best_genome_size > config->max_genome_bytes ||
        result->secure_erasure_enabled !=
            config->secure_erasure_enabled ||
        !evo_secure_erasure_metadata_is_valid(
            result->secure_erasure_enabled,
            result->secure_erasure_policy_version,
            result->secure_erasure_backend) ||
        result->termination_reason != EVO_TERMINATION_NONE ||
        !evo_fitness_evidence_is_valid(&result->best_fitness)) {
        return false;
    }

    statistics = &result->generation_statistics;
#if SIZE_MAX > UINT64_MAX
    if (result->generations_completed > (size_t)UINT64_MAX) {
        return false;
    }
#endif
    return statistics->version == EVO_GENERATION_STATISTICS_VERSION &&
           statistics->generation_index ==
               (uint64_t)result->generations_completed &&
           statistics->population_size != 0 &&
           statistics->valid_count <= statistics->population_size &&
           statistics->invalid_count ==
               statistics->population_size - statistics->valid_count &&
           statistics->has_best == current_has_best &&
           (current_has_best
                ? statistics->valid_count != 0
                : statistics->valid_count == 0) &&
           statistics->fitness_comparison_policy_version ==
               EVO_FITNESS_COMPARISON_POLICY_VERSION &&
           statistics->diversity_policy_version ==
               EVO_DIVERSITY_POLICY_VERSION &&
           statistics->diversity_metric_version != UINT32_C(0) &&
           isfinite(statistics->diversity) &&
           statistics->diversity >= 0.0 &&
           statistics->diversity <= 1.0 &&
           evo_adaptive_mutation_statistics_are_valid(config, statistics);
}

static evo_termination_reason_t classify_policy_evidence(
    const evo_config_t *config,
    const evo_result_t *result,
    const evo_stopping_state_t *state)
{
    if (config->fitness_target_enabled &&
        result->best_fitness.total >= config->fitness_target) {
        return EVO_TERMINATION_CONVERGED;
    }

    if (config->diversity_floor_enabled &&
        result->generation_statistics.diversity <=
            config->diversity_floor) {
        return EVO_TERMINATION_STAGNATED;
    }

    if (config->stagnation_enabled && state != NULL &&
        state->stagnant_generations >= config->stagnation_patience) {
        return EVO_TERMINATION_STAGNATED;
    }

    return EVO_TERMINATION_NONE;
}

evo_status_t evo_stopping_classify_initial(
    const evo_config_t *config,
    const evo_result_t *result,
    bool generation_limit_reached,
    evo_termination_reason_t *reason)
{
    evo_termination_reason_t candidate = EVO_TERMINATION_NONE;

    if (reason == NULL || evo_stopping_validate_config(config) != EVO_SUCCESS ||
        !committed_result_is_valid(config, result, true) ||
        result->generations_completed != 0) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    candidate = classify_policy_evidence(config, result, NULL);
    if (candidate == EVO_TERMINATION_NONE && generation_limit_reached) {
        candidate = EVO_TERMINATION_GENERATION_LIMIT;
    }
    *reason = candidate;
    return EVO_SUCCESS;
}

evo_status_t evo_stopping_state_initialize(
    const evo_config_t *config,
    const evo_result_t *result,
    evo_stopping_state_t *state)
{
    if (state == NULL || state->initialized ||
        state->significant_best_total != 0.0 ||
        state->stagnant_generations != 0 ||
        evo_stopping_validate_config(config) != EVO_SUCCESS ||
        !committed_result_is_valid(config, result, true) ||
        result->generations_completed != 0) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    state->significant_best_total = result->best_fitness.total;
    state->initialized = true;
    return EVO_SUCCESS;
}

static void update_patience(const evo_config_t *config,
                            const evo_result_t *result,
                            evo_stopping_state_t *state)
{
    double threshold = 0.0;

    if (!config->stagnation_enabled) {
        return;
    }

    threshold = state->significant_best_total +
                config->improvement_tolerance;
    if (result->best_fitness.total > threshold) {
        state->significant_best_total = result->best_fitness.total;
        state->stagnant_generations = 0;
    } else if (state->stagnant_generations <
               config->stagnation_patience) {
        ++state->stagnant_generations;
    }
}

evo_status_t evo_stopping_classify_committed(
    const evo_config_t *config,
    const evo_result_t *result,
    bool all_invalid,
    bool generation_limit_reached,
    evo_stopping_state_t *state,
    evo_termination_reason_t *reason)
{
    evo_termination_reason_t candidate = EVO_TERMINATION_NONE;

    if (reason == NULL || state == NULL || !state->initialized ||
        !isfinite(state->significant_best_total) ||
        evo_stopping_validate_config(config) != EVO_SUCCESS ||
        !committed_result_is_valid(config, result, !all_invalid) ||
        result->generations_completed == 0) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (all_invalid) {
        *reason = EVO_TERMINATION_ALL_INVALID;
        return EVO_SUCCESS;
    }

    update_patience(config, result, state);
    candidate = classify_policy_evidence(config, result, state);
    if (candidate == EVO_TERMINATION_NONE && generation_limit_reached) {
        candidate = EVO_TERMINATION_GENERATION_LIMIT;
    }
    *reason = candidate;
    return EVO_SUCCESS;
}
