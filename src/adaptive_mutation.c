#include "internal/adaptive_mutation.h"

#include <math.h>
#include <stdint.h>

static bool byte_ranges_overlap(const void *left,
                                size_t left_size,
                                const void *right,
                                size_t right_size)
{
    const uintmax_t left_start = (uintmax_t)(uintptr_t)left;
    const uintmax_t right_start = (uintmax_t)(uintptr_t)right;
    uintmax_t left_end = 0;
    uintmax_t right_end = 0;

    if (left == NULL || right == NULL || left_size == 0 || right_size == 0 ||
        (uintmax_t)left_size > UINTMAX_MAX - left_start ||
        (uintmax_t)right_size > UINTMAX_MAX - right_start) {
        return true;
    }

    left_end = left_start + (uintmax_t)left_size;
    right_end = right_start + (uintmax_t)right_size;
    return left_start < right_end && right_start < left_end;
}

static bool writable_objects_are_independent(
    const evo_config_t *config,
    const evo_generation_statistics_t *statistics,
    const evo_adaptive_mutation_state_t *state)
{
    return !byte_ranges_overlap(config,
                                sizeof(*config),
                                statistics,
                                sizeof(*statistics)) &&
           !byte_ranges_overlap(config,
                                sizeof(*config),
                                state,
                                sizeof(*state)) &&
           !byte_ranges_overlap(statistics,
                                sizeof(*statistics),
                                state,
                                sizeof(*state));
}

static bool disabled_payload_is_canonical(const evo_config_t *config)
{
    return config->adaptive_mutation_min_rate == 0.0 &&
           config->adaptive_mutation_max_rate == 0.0 &&
           config->adaptive_mutation_step == 0.0 &&
           config->adaptive_mutation_diversity_threshold == 0.0 &&
           !config->adaptive_mutation_reset_on_improvement;
}

bool evo_adaptive_mutation_is_applicable(const evo_config_t *config)
{
    if (config == NULL || config->generation_limit == 0) {
        return false;
    }

    return config->population_size != 1 ||
           (config->elite_count_enabled && config->elite_count == 0);
}

evo_status_t evo_adaptive_mutation_validate_config(
    const evo_config_t *config)
{
    if (config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!isfinite(config->mutation_rate) || config->mutation_rate < 0.0 ||
        config->mutation_rate > 1.0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!config->adaptive_mutation_enabled) {
        return disabled_payload_is_canonical(config)
                   ? EVO_SUCCESS
                   : EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!isfinite(config->adaptive_mutation_min_rate) ||
        !isfinite(config->adaptive_mutation_max_rate) ||
        !isfinite(config->adaptive_mutation_step) ||
        !isfinite(config->adaptive_mutation_diversity_threshold) ||
        config->adaptive_mutation_min_rate < 0.0 ||
        config->adaptive_mutation_min_rate > 1.0 ||
        config->adaptive_mutation_max_rate < 0.0 ||
        config->adaptive_mutation_max_rate > 1.0 ||
        config->adaptive_mutation_min_rate >
            config->adaptive_mutation_max_rate ||
        config->adaptive_mutation_step <= 0.0 ||
        config->adaptive_mutation_step > 1.0 ||
        config->adaptive_mutation_diversity_threshold < 0.0 ||
        config->adaptive_mutation_diversity_threshold > 1.0) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    return EVO_SUCCESS;
}

static bool reason_is_valid(evo_mutation_adaptation_reason_t reason)
{
    switch (reason) {
    case EVO_MUTATION_ADAPTATION_NOT_APPLICABLE:
    case EVO_MUTATION_ADAPTATION_DISABLED:
    case EVO_MUTATION_ADAPTATION_INITIAL:
    case EVO_MUTATION_ADAPTATION_LOW_DIVERSITY:
    case EVO_MUTATION_ADAPTATION_STAGNATION:
    case EVO_MUTATION_ADAPTATION_STAGNATION_LOW_DIVERSITY:
    case EVO_MUTATION_ADAPTATION_IMPROVEMENT_RESET:
    case EVO_MUTATION_ADAPTATION_IMPROVEMENT_HOLD:
        return true;
    default:
        return false;
    }
}

static bool adaptation_projection_is_zero(
    const evo_generation_statistics_t *statistics)
{
    return statistics->adaptive_mutation_policy_version == UINT32_C(0) &&
           statistics->mutation_rate_prior == 0.0 &&
           statistics->mutation_rate_effective == 0.0 &&
           statistics->adaptive_mutation_min_rate == 0.0 &&
           statistics->adaptive_mutation_max_rate == 0.0 &&
           statistics->adaptive_mutation_step == 0.0 &&
           statistics->adaptive_mutation_diversity_threshold == 0.0 &&
           statistics->adaptive_mutation_stagnant_generations == 0 &&
           statistics->mutation_adaptation_reason ==
               EVO_MUTATION_ADAPTATION_NOT_APPLICABLE &&
           !statistics->adaptive_mutation_enabled &&
           !statistics->adaptive_mutation_low_diversity &&
           !statistics->adaptive_mutation_global_best_improved &&
           !statistics->adaptive_mutation_clamped_to_min &&
           !statistics->adaptive_mutation_clamped_to_max &&
           !statistics->adaptive_mutation_reset_on_improvement;
}

static bool base_statistics_are_valid(
    const evo_generation_statistics_t *statistics)
{
    return statistics != NULL &&
           statistics->version == EVO_GENERATION_STATISTICS_VERSION &&
           statistics->population_size != 0 &&
           statistics->valid_count <= statistics->population_size &&
           statistics->invalid_count ==
               statistics->population_size - statistics->valid_count &&
           statistics->has_best == (statistics->valid_count != 0) &&
           statistics->fitness_comparison_policy_version ==
               EVO_FITNESS_COMPARISON_POLICY_VERSION &&
           statistics->diversity_policy_version ==
               EVO_DIVERSITY_POLICY_VERSION &&
           statistics->diversity_metric_version != UINT32_C(0) &&
           isfinite(statistics->diversity) &&
           statistics->diversity >= 0.0 && statistics->diversity <= 1.0;
}

static double clamp_initial_rate(const evo_config_t *config,
                                 bool *clamped_to_min,
                                 bool *clamped_to_max)
{
    if (config->mutation_rate < config->adaptive_mutation_min_rate) {
        *clamped_to_min = true;
        return config->adaptive_mutation_min_rate;
    }
    if (config->mutation_rate > config->adaptive_mutation_max_rate) {
        *clamped_to_max = true;
        return config->adaptive_mutation_max_rate;
    }
    return config->mutation_rate;
}

static double increase_rate(const evo_config_t *config,
                            double current,
                            bool *clamped_to_max)
{
    const double remaining =
        config->adaptive_mutation_max_rate - current;

    if (config->adaptive_mutation_step > remaining) {
        *clamped_to_max = true;
        return config->adaptive_mutation_max_rate;
    }
    return current + config->adaptive_mutation_step;
}

static void write_projection(
    const evo_config_t *config,
    double prior_rate,
    const evo_adaptive_mutation_state_t *state,
    evo_mutation_adaptation_reason_t reason,
    bool low_diversity,
    bool global_best_improved,
    bool clamped_to_min,
    bool clamped_to_max,
    evo_generation_statistics_t *statistics)
{
    statistics->adaptive_mutation_policy_version =
        EVO_MUTATION_ADAPTATION_POLICY_VERSION;
    statistics->mutation_rate_prior = prior_rate;
    statistics->mutation_rate_effective = state->effective_rate;
    statistics->adaptive_mutation_min_rate =
        config->adaptive_mutation_min_rate;
    statistics->adaptive_mutation_max_rate =
        config->adaptive_mutation_max_rate;
    statistics->adaptive_mutation_step = config->adaptive_mutation_step;
    statistics->adaptive_mutation_diversity_threshold =
        config->adaptive_mutation_diversity_threshold;
    statistics->adaptive_mutation_stagnant_generations =
        state->stagnant_generations;
    statistics->mutation_adaptation_reason = reason;
    statistics->adaptive_mutation_enabled =
        config->adaptive_mutation_enabled;
    statistics->adaptive_mutation_low_diversity = low_diversity;
    statistics->adaptive_mutation_global_best_improved =
        global_best_improved;
    statistics->adaptive_mutation_clamped_to_min = clamped_to_min;
    statistics->adaptive_mutation_clamped_to_max = clamped_to_max;
    statistics->adaptive_mutation_reset_on_improvement =
        config->adaptive_mutation_reset_on_improvement;
}

evo_status_t evo_adaptive_mutation_initialize(
    const evo_config_t *config,
    evo_generation_statistics_t *statistics,
    evo_adaptive_mutation_state_t *state)
{
    evo_adaptive_mutation_state_t candidate_state = {0};
    evo_generation_statistics_t candidate_statistics = {0};
    evo_mutation_adaptation_reason_t reason =
        EVO_MUTATION_ADAPTATION_DISABLED;
    bool clamped_to_min = false;
    bool clamped_to_max = false;
    bool low_diversity = false;

    if (config == NULL || statistics == NULL || state == NULL ||
        !writable_objects_are_independent(config, statistics, state) ||
        !evo_adaptive_mutation_is_applicable(config) || state->initialized ||
        state->effective_rate != 0.0 || state->stagnant_generations != 0 ||
        evo_adaptive_mutation_validate_config(config) != EVO_SUCCESS ||
        !base_statistics_are_valid(statistics) ||
        statistics->generation_index != UINT64_C(0) ||
        !statistics->has_best || !adaptation_projection_is_zero(statistics)) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    candidate_statistics = *statistics;
    candidate_state.initialized = true;
    candidate_state.effective_rate = config->mutation_rate;

    if (config->adaptive_mutation_enabled) {
        candidate_state.effective_rate =
            clamp_initial_rate(config,
                               &clamped_to_min,
                               &clamped_to_max);
        low_diversity =
            statistics->diversity <=
            config->adaptive_mutation_diversity_threshold;
        reason = EVO_MUTATION_ADAPTATION_INITIAL;
        if (low_diversity) {
            candidate_state.effective_rate =
                increase_rate(config,
                              candidate_state.effective_rate,
                              &clamped_to_max);
            reason = EVO_MUTATION_ADAPTATION_LOW_DIVERSITY;
        }
    }

    write_projection(config,
                     config->mutation_rate,
                     &candidate_state,
                     reason,
                     low_diversity,
                     false,
                     clamped_to_min,
                     clamped_to_max,
                     &candidate_statistics);
    *statistics = candidate_statistics;
    *state = candidate_state;
    return EVO_SUCCESS;
}

bool evo_adaptive_mutation_statistics_are_valid(
    const evo_config_t *config,
    const evo_generation_statistics_t *statistics)
{
    evo_mutation_adaptation_reason_t expected_reason =
        EVO_MUTATION_ADAPTATION_NOT_APPLICABLE;
    evo_mutation_adaptation_reason_t reason =
        EVO_MUTATION_ADAPTATION_NOT_APPLICABLE;
    double expected_rate = 0.0;
    bool expected_clamped_to_min = false;
    bool expected_clamped_to_max = false;
    bool low_diversity = false;

    if (config == NULL || !base_statistics_are_valid(statistics)) {
        return false;
    }

    if (!evo_adaptive_mutation_is_applicable(config)) {
        return adaptation_projection_is_zero(statistics);
    }

    if (evo_adaptive_mutation_validate_config(config) != EVO_SUCCESS ||
        statistics->adaptive_mutation_policy_version !=
            EVO_MUTATION_ADAPTATION_POLICY_VERSION ||
        !isfinite(statistics->mutation_rate_prior) ||
        statistics->mutation_rate_prior < 0.0 ||
        statistics->mutation_rate_prior > 1.0 ||
        !isfinite(statistics->mutation_rate_effective) ||
        statistics->mutation_rate_effective < 0.0 ||
        statistics->mutation_rate_effective > 1.0 ||
        statistics->adaptive_mutation_enabled !=
            config->adaptive_mutation_enabled ||
        statistics->adaptive_mutation_min_rate !=
            config->adaptive_mutation_min_rate ||
        statistics->adaptive_mutation_max_rate !=
            config->adaptive_mutation_max_rate ||
        statistics->adaptive_mutation_step !=
            config->adaptive_mutation_step ||
        statistics->adaptive_mutation_diversity_threshold !=
            config->adaptive_mutation_diversity_threshold ||
        statistics->adaptive_mutation_reset_on_improvement !=
            config->adaptive_mutation_reset_on_improvement) {
        return false;
    }

    reason = statistics->mutation_adaptation_reason;
    if (!reason_is_valid(reason) ||
        reason == EVO_MUTATION_ADAPTATION_NOT_APPLICABLE) {
        return false;
    }

    if (!config->adaptive_mutation_enabled) {
        return reason == EVO_MUTATION_ADAPTATION_DISABLED &&
               statistics->mutation_rate_prior == config->mutation_rate &&
               statistics->mutation_rate_effective == config->mutation_rate &&
               statistics->adaptive_mutation_stagnant_generations == 0 &&
               !statistics->adaptive_mutation_low_diversity &&
               !statistics->adaptive_mutation_clamped_to_min &&
               !statistics->adaptive_mutation_clamped_to_max &&
               (statistics->generation_index != UINT64_C(0) ||
                !statistics->adaptive_mutation_global_best_improved);
    }

    low_diversity =
        statistics->diversity <=
        config->adaptive_mutation_diversity_threshold;
    if (statistics->mutation_rate_effective <
            config->adaptive_mutation_min_rate ||
        statistics->mutation_rate_effective >
            config->adaptive_mutation_max_rate ||
        statistics->adaptive_mutation_low_diversity !=
            low_diversity) {
        return false;
    }

    if (statistics->generation_index == UINT64_C(0)) {
        expected_rate =
            clamp_initial_rate(config,
                               &expected_clamped_to_min,
                               &expected_clamped_to_max);
        expected_reason = EVO_MUTATION_ADAPTATION_INITIAL;
        if (low_diversity) {
            expected_rate = increase_rate(config,
                                          expected_rate,
                                          &expected_clamped_to_max);
            expected_reason = EVO_MUTATION_ADAPTATION_LOW_DIVERSITY;
        }
        return statistics->mutation_rate_prior == config->mutation_rate &&
               statistics->mutation_rate_effective == expected_rate &&
               statistics->adaptive_mutation_stagnant_generations == 0 &&
               reason == expected_reason &&
               !statistics->adaptive_mutation_global_best_improved &&
               statistics->adaptive_mutation_clamped_to_min ==
                   expected_clamped_to_min &&
               statistics->adaptive_mutation_clamped_to_max ==
                   expected_clamped_to_max;
    }

    if (statistics->mutation_rate_prior <
            config->adaptive_mutation_min_rate ||
        statistics->mutation_rate_prior >
            config->adaptive_mutation_max_rate) {
        return false;
    }

    expected_rate = statistics->mutation_rate_prior;
    if (statistics->adaptive_mutation_global_best_improved) {
        if (statistics->adaptive_mutation_stagnant_generations != 0) {
            return false;
        }
        if (config->adaptive_mutation_reset_on_improvement) {
            expected_rate = config->adaptive_mutation_min_rate;
            expected_reason =
                EVO_MUTATION_ADAPTATION_IMPROVEMENT_RESET;
        } else if (low_diversity) {
            expected_rate = increase_rate(config,
                                          expected_rate,
                                          &expected_clamped_to_max);
            expected_reason = EVO_MUTATION_ADAPTATION_LOW_DIVERSITY;
        } else {
            expected_reason = EVO_MUTATION_ADAPTATION_IMPROVEMENT_HOLD;
        }
    } else {
        if (statistics->adaptive_mutation_stagnant_generations == 0) {
            return false;
        }
        expected_rate = increase_rate(config,
                                      expected_rate,
                                      &expected_clamped_to_max);
        expected_reason =
            low_diversity
                ? EVO_MUTATION_ADAPTATION_STAGNATION_LOW_DIVERSITY
                : EVO_MUTATION_ADAPTATION_STAGNATION;
    }

    return statistics->mutation_rate_effective == expected_rate &&
           reason == expected_reason &&
           !statistics->adaptive_mutation_clamped_to_min &&
           statistics->adaptive_mutation_clamped_to_max ==
               expected_clamped_to_max;
}

evo_status_t evo_adaptive_mutation_restore_initial(
    const evo_config_t *config,
    const evo_generation_statistics_t *statistics,
    evo_adaptive_mutation_state_t *state)
{
    if (config == NULL || statistics == NULL || state == NULL ||
        !writable_objects_are_independent(config, statistics, state) ||
        !evo_adaptive_mutation_is_applicable(config) || state->initialized ||
        state->effective_rate != 0.0 || state->stagnant_generations != 0 ||
        statistics->generation_index != UINT64_C(0) ||
        !evo_adaptive_mutation_statistics_are_valid(config, statistics)) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    state->effective_rate = statistics->mutation_rate_effective;
    state->stagnant_generations =
        statistics->adaptive_mutation_stagnant_generations;
    state->initialized = true;
    return EVO_SUCCESS;
}

evo_status_t evo_adaptive_mutation_commit(
    const evo_config_t *config,
    bool global_best_improved,
    evo_generation_statistics_t *statistics,
    evo_adaptive_mutation_state_t *state)
{
    evo_adaptive_mutation_state_t candidate_state = {0};
    evo_generation_statistics_t candidate_statistics = {0};
    evo_mutation_adaptation_reason_t reason =
        EVO_MUTATION_ADAPTATION_DISABLED;
    const double prior_rate = state == NULL ? 0.0 : state->effective_rate;
    bool clamped_to_max = false;
    bool low_diversity = false;

    if (config == NULL || statistics == NULL || state == NULL ||
        !writable_objects_are_independent(config, statistics, state) ||
        !evo_adaptive_mutation_is_applicable(config) ||
        !state->initialized ||
        !isfinite(state->effective_rate) || state->effective_rate < 0.0 ||
        state->effective_rate > 1.0 ||
        evo_adaptive_mutation_validate_config(config) != EVO_SUCCESS ||
        !base_statistics_are_valid(statistics) ||
        statistics->generation_index == UINT64_C(0) ||
        !adaptation_projection_is_zero(statistics)) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    candidate_state = *state;
    candidate_statistics = *statistics;

    if (!config->adaptive_mutation_enabled) {
        candidate_state.effective_rate = config->mutation_rate;
        candidate_state.stagnant_generations = 0;
    } else {
        low_diversity =
            statistics->diversity <=
            config->adaptive_mutation_diversity_threshold;

        if (global_best_improved) {
            candidate_state.stagnant_generations = 0;
            if (config->adaptive_mutation_reset_on_improvement) {
                candidate_state.effective_rate =
                    config->adaptive_mutation_min_rate;
                reason = EVO_MUTATION_ADAPTATION_IMPROVEMENT_RESET;
            } else if (low_diversity) {
                candidate_state.effective_rate =
                    increase_rate(config,
                                  candidate_state.effective_rate,
                                  &clamped_to_max);
                reason = EVO_MUTATION_ADAPTATION_LOW_DIVERSITY;
            } else {
                reason = EVO_MUTATION_ADAPTATION_IMPROVEMENT_HOLD;
            }
        } else {
            if (candidate_state.stagnant_generations != SIZE_MAX) {
                ++candidate_state.stagnant_generations;
            }
            candidate_state.effective_rate =
                increase_rate(config,
                              candidate_state.effective_rate,
                              &clamped_to_max);
            reason = low_diversity
                         ? EVO_MUTATION_ADAPTATION_STAGNATION_LOW_DIVERSITY
                         : EVO_MUTATION_ADAPTATION_STAGNATION;
        }
    }

    write_projection(config,
                     prior_rate,
                     &candidate_state,
                     reason,
                     low_diversity,
                     global_best_improved,
                     false,
                     clamped_to_max,
                     &candidate_statistics);
    *statistics = candidate_statistics;
    *state = candidate_state;
    return EVO_SUCCESS;
}
