#include "internal/selection.h"

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

    if (config == NULL || population == NULL || rng == NULL ||
        selected_index == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (config->population_size == 0 ||
        config->tournament_size == 0 ||
        config->tournament_size > config->population_size) {
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

        const bool lower_index = candidate_index < winner_index;
        const double candidate_total =
            population->evaluations[candidate_index].fitness.total;
        bool candidate_wins = !has_winner;

        if (has_winner) {
            double winner_total = 0.0;

            if (winner_index >= population->population_size) {
                return EVO_ERROR_STATE;
            }

            winner_total =
                population->evaluations[winner_index].fitness.total;
            candidate_wins =
                candidate_total > winner_total ||
                (lower_index && candidate_total == winner_total);
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
