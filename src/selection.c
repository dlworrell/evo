#include "internal/selection.h"

#include "internal/fitness.h"

#include <stdint.h>

static bool checked_size_add(size_t left,
                             size_t right,
                             size_t *sum)
{
    if (sum == NULL || right > SIZE_MAX - left) {
        return false;
    }

    *sum = left + right;
    return true;
}

static bool checked_size_multiply(size_t left,
                                  size_t right,
                                  size_t *product)
{
    if (product == NULL || (left != 0 && right > SIZE_MAX / left)) {
        return false;
    }

    *product = left * right;
    return true;
}

static bool rank_weight_total(size_t candidate_count,
                              size_t base_weight,
                              size_t step_weight,
                              size_t *total_weight)
{
    size_t triangular_left = candidate_count;
    size_t triangular_right = 0;
    size_t triangular = 0;
    size_t base_total = 0;
    size_t step_total = 0;

    if (candidate_count == 0 || base_weight == 0 ||
        total_weight == NULL) {
        return false;
    }

    triangular_right = candidate_count - 1;
    if (triangular_left % 2 == 0) {
        triangular_left /= 2;
    } else {
        triangular_right /= 2;
    }

    return checked_size_multiply(triangular_left,
                                 triangular_right,
                                 &triangular) &&
           checked_size_multiply(candidate_count,
                                 base_weight,
                                 &base_total) &&
           checked_size_multiply(triangular,
                                 step_weight,
                                 &step_total) &&
           checked_size_add(base_total, step_total, total_weight);
}

static bool rank_weight_at(size_t candidate_count,
                           size_t rank,
                           size_t base_weight,
                           size_t step_weight,
                           size_t *weight)
{
    size_t step_count = 0;
    size_t ranked_step = 0;

    if (candidate_count == 0 || rank >= candidate_count ||
        base_weight == 0 || weight == NULL) {
        return false;
    }

    step_count = candidate_count - 1 - rank;
    return checked_size_multiply(step_count,
                                 step_weight,
                                 &ranked_step) &&
           checked_size_add(base_weight, ranked_step, weight);
}

evo_status_t evo_selection_validate_config(const evo_config_t *config)
{
    size_t total_weight = 0;

    if (config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (config->population_size == 0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    switch (config->selection_policy) {
    case EVO_SELECTION_TOURNAMENT:
        if (config->tournament_size > config->population_size ||
            config->rank_base_weight != 0 ||
            config->rank_step_weight != 0) {
            return EVO_ERROR_RESOURCE_LIMIT;
        }
        return EVO_SUCCESS;
    case EVO_SELECTION_RANK:
        if (config->tournament_size != 0 ||
            !rank_weight_total(config->population_size,
                               config->rank_base_weight,
                               config->rank_step_weight,
                               &total_weight)) {
            return EVO_ERROR_RESOURCE_LIMIT;
        }
        return EVO_SUCCESS;
    default:
        return EVO_ERROR_RESOURCE_LIMIT;
    }
}

evo_status_t evo_selection_validate_active_config(
    const evo_config_t *config)
{
    evo_status_t status = evo_selection_validate_config(config);

    if (status != EVO_SUCCESS) {
        return status;
    }
    if (config->selection_policy == EVO_SELECTION_TOURNAMENT &&
        config->tournament_size == 0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    return EVO_SUCCESS;
}

static bool valid_index_from_ordinal(
    const evo_population_t *population,
    size_t ordinal,
    size_t *population_index)
{
    size_t current_ordinal = 0;

    for (size_t index = 0;
         index < population->population_size;
         ++index) {
        if (!population->evaluations[index].valid) {
            continue;
        }

        if (current_ordinal == ordinal) {
            *population_index = index;
            return true;
        }
        ++current_ordinal;
    }

    return false;
}

evo_status_t evo_population_select_tournament(
    const evo_config_t *config,
    const evo_population_t *population,
    evo_rng_t *rng,
    size_t *selected_index)
{
    size_t valid_count = 0;
    size_t winner_index = 0;
    bool has_winner = false;
    evo_status_t status = EVO_SUCCESS;

    if (config == NULL || population == NULL || rng == NULL ||
        selected_index == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = evo_selection_validate_active_config(config);
    if (status != EVO_SUCCESS) {
        return status;
    }
    if (config->selection_policy != EVO_SELECTION_TOURNAMENT) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!evo_population_validate_completed(
            config, population, &valid_count)) {
        return EVO_ERROR_STATE;
    }

    if (valid_count == 0) {
        return EVO_ERROR_NO_VALID_CANDIDATE;
    }

    if (!rng->seeded) {
        return EVO_ERROR_STATE;
    }

    for (size_t draw = 0;
         draw < config->tournament_size;
         ++draw) {
        size_t valid_ordinal = 0;
        size_t candidate_index = 0;

        if (!evo_rng_uniform_index(
                rng, valid_count, &valid_ordinal) ||
            !valid_index_from_ordinal(
                population, valid_ordinal, &candidate_index) ||
            candidate_index >= population->population_size) {
            return EVO_ERROR_STATE;
        }

        bool candidate_wins = !has_winner;

        if (has_winner) {
            const evo_candidate_evaluation_t *candidate_evaluation =
                &population->evaluations[candidate_index];
            const evo_candidate_evaluation_t *winner_evaluation = NULL;
            evo_fitness_candidate_view_t candidate_view = {0};
            evo_fitness_candidate_view_t winner_view = {0};
            evo_fitness_order_t order = EVO_FITNESS_ORDER_EQUAL;

            if (winner_index >= population->population_size) {
                return EVO_ERROR_STATE;
            }

            winner_evaluation = &population->evaluations[winner_index];
            candidate_view = (evo_fitness_candidate_view_t){
                .fitness = &candidate_evaluation->fitness,
                .generation = UINT64_C(0),
                .population_index = candidate_index,
                .hard_valid = candidate_evaluation->valid,
                .evaluated = candidate_evaluation->evaluated,
            };
            winner_view = (evo_fitness_candidate_view_t){
                .fitness = &winner_evaluation->fitness,
                .generation = UINT64_C(0),
                .population_index = winner_index,
                .hard_valid = winner_evaluation->valid,
                .evaluated = winner_evaluation->evaluated,
            };
            if (!evo_fitness_compare_candidates(&candidate_view,
                                                &winner_view,
                                                &order)) {
                return EVO_ERROR_STATE;
            }
            candidate_wins = order == EVO_FITNESS_ORDER_LEFT;
        }

        if (candidate_wins) {
            winner_index = candidate_index;
            has_winner = true;
        }
    }

    if (!has_winner) {
        return EVO_ERROR_STATE;
    }

    *selected_index = winner_index;
    return EVO_SUCCESS;
}

static bool candidate_rank(const evo_population_t *population,
                           size_t candidate_index,
                           size_t *rank)
{
    const evo_candidate_evaluation_t *candidate_evaluation = NULL;
    evo_fitness_candidate_view_t candidate_view = {0};
    size_t candidate_rank = 0;

    if (population == NULL || rank == NULL ||
        candidate_index >= population->population_size ||
        !population->evaluations[candidate_index].valid) {
        return false;
    }

    candidate_evaluation = &population->evaluations[candidate_index];
    candidate_view = (evo_fitness_candidate_view_t){
        .fitness = &candidate_evaluation->fitness,
        .generation = UINT64_C(0),
        .population_index = candidate_index,
        .hard_valid = candidate_evaluation->valid,
        .evaluated = candidate_evaluation->evaluated,
    };
    if (!evo_fitness_candidate_is_rankable(&candidate_view)) {
        return false;
    }

    for (size_t other_index = 0;
         other_index < population->population_size;
         ++other_index) {
        const evo_candidate_evaluation_t *other_evaluation = NULL;
        evo_fitness_candidate_view_t other_view = {0};
        evo_fitness_order_t order = EVO_FITNESS_ORDER_EQUAL;

        if (other_index == candidate_index ||
            !population->evaluations[other_index].valid) {
            continue;
        }

        other_evaluation = &population->evaluations[other_index];
        other_view = (evo_fitness_candidate_view_t){
            .fitness = &other_evaluation->fitness,
            .generation = UINT64_C(0),
            .population_index = other_index,
            .hard_valid = other_evaluation->valid,
            .evaluated = other_evaluation->evaluated,
        };
        if (!evo_fitness_compare_candidates(&other_view,
                                            &candidate_view,
                                            &order)) {
            return false;
        }
        if (order == EVO_FITNESS_ORDER_LEFT) {
            ++candidate_rank;
        }
    }

    *rank = candidate_rank;
    return true;
}

static bool rank_distribution_is_resolvable(
    const evo_config_t *config,
    const evo_population_t *population,
    size_t valid_count,
    size_t expected_total_weight)
{
    size_t observed_valid_count = 0;
    size_t observed_total_weight = 0;

    for (size_t index = 0;
         index < population->population_size;
         ++index) {
        size_t rank = 0;
        size_t weight = 0;

        if (!population->evaluations[index].valid) {
            continue;
        }
        if (!candidate_rank(population, index, &rank) ||
            rank >= valid_count ||
            !rank_weight_at(valid_count,
                            rank,
                            config->rank_base_weight,
                            config->rank_step_weight,
                            &weight) ||
            !checked_size_add(observed_total_weight,
                              weight,
                              &observed_total_weight)) {
            return false;
        }
        ++observed_valid_count;
    }

    return observed_valid_count == valid_count &&
           observed_total_weight == expected_total_weight;
}

static evo_status_t population_select_rank(
    const evo_config_t *config,
    const evo_population_t *population,
    evo_rng_t *rng,
    size_t *selected_index)
{
    size_t valid_count = 0;
    size_t total_weight = 0;
    size_t ticket = 0;

    if (!evo_population_validate_completed(
            config, population, &valid_count)) {
        return EVO_ERROR_STATE;
    }
    if (valid_count == 0) {
        return EVO_ERROR_NO_VALID_CANDIDATE;
    }
    if (!rng->seeded) {
        return EVO_ERROR_STATE;
    }
    if (!rank_weight_total(valid_count,
                           config->rank_base_weight,
                           config->rank_step_weight,
                           &total_weight) ||
        !rank_distribution_is_resolvable(config,
                                         population,
                                         valid_count,
                                         total_weight)) {
        return EVO_ERROR_STATE;
    }
    if (!evo_rng_uniform_index(rng, total_weight, &ticket)) {
        return EVO_ERROR_STATE;
    }

    for (size_t index = 0;
         index < population->population_size;
         ++index) {
        size_t rank = 0;
        size_t weight = 0;

        if (!population->evaluations[index].valid) {
            continue;
        }
        if (!candidate_rank(population, index, &rank) ||
            !rank_weight_at(valid_count,
                            rank,
                            config->rank_base_weight,
                            config->rank_step_weight,
                            &weight)) {
            return EVO_ERROR_STATE;
        }
        if (ticket < weight) {
            *selected_index = index;
            return EVO_SUCCESS;
        }
        ticket -= weight;
    }

    return EVO_ERROR_STATE;
}

evo_status_t evo_population_select(const evo_config_t *config,
                                   const evo_population_t *population,
                                   evo_rng_t *rng,
                                   size_t *selected_index)
{
    evo_status_t status = EVO_SUCCESS;

    if (config == NULL || population == NULL || rng == NULL ||
        selected_index == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = evo_selection_validate_active_config(config);
    if (status != EVO_SUCCESS) {
        return status;
    }

    switch (config->selection_policy) {
    case EVO_SELECTION_TOURNAMENT:
        return evo_population_select_tournament(config,
                                                population,
                                                rng,
                                                selected_index);
    case EVO_SELECTION_RANK:
        return population_select_rank(config,
                                      population,
                                      rng,
                                      selected_index);
    default:
        return EVO_ERROR_RESOURCE_LIMIT;
    }
}
