#include "internal/selection.h"

#include <math.h>
#include <stdint.h>

static bool checked_size_multiply(size_t left,
                                  size_t right,
                                  size_t *product)
{
    if (product == NULL ||
        (left != 0 && right > SIZE_MAX / left)) {
        return false;
    }

    *product = left * right;
    return true;
}

static bool fitness_is_finite(const evo_fitness_t *fitness)
{
    return isfinite(fitness->correctness) &&
           isfinite(fitness->performance) &&
           isfinite(fitness->memory_use) &&
           isfinite(fitness->reliability) &&
           isfinite(fitness->maintainability) &&
           isfinite(fitness->constraint_penalty) &&
           isfinite(fitness->total);
}

static bool fitness_is_zero(const evo_fitness_t *fitness)
{
    return fitness->correctness == 0.0 &&
           fitness->performance == 0.0 &&
           fitness->memory_use == 0.0 &&
           fitness->reliability == 0.0 &&
           fitness->maintainability == 0.0 &&
           fitness->constraint_penalty == 0.0 &&
           fitness->total == 0.0;
}

static bool completed_population_is_consistent(
    const evo_config_t *config,
    const evo_population_t *population,
    size_t *validated_valid_count)
{
    size_t expected_storage_bytes = 0;
    size_t expected_evaluation_bytes = 0;
    size_t valid_count = 0;
    size_t best_index = 0;
    bool has_best = false;

    if (config == NULL || population == NULL ||
        validated_valid_count == NULL ||
        population->genomes == NULL ||
        population->evaluations == NULL ||
        population->population_size == 0 ||
        population->genome_size == 0 ||
        !population->initialized ||
        !population->evaluated ||
        population->population_size != config->population_size ||
        population->initialization_seed != config->random_seed ||
        population->rng_algorithm_version != EVO_RNG_ALGORITHM_VERSION ||
        !checked_size_multiply(population->population_size,
                               population->genome_size,
                               &expected_storage_bytes) ||
        expected_storage_bytes != population->storage_bytes ||
        population->genome_size > config->max_genome_bytes ||
        population->storage_bytes > config->max_population_bytes ||
        !checked_size_multiply(
            population->population_size,
            sizeof(evo_candidate_evaluation_t),
            &expected_evaluation_bytes) ||
        expected_evaluation_bytes != population->evaluation_bytes ||
        population->evaluation_bytes > config->max_evaluation_bytes) {
        return false;
    }

    for (size_t index = 0;
         index < population->population_size;
         ++index) {
        const evo_candidate_evaluation_t *evaluation =
            &population->evaluations[index];

        if (!evaluation->valid) {
            if (evaluation->evaluated ||
                !fitness_is_zero(&evaluation->fitness)) {
                return false;
            }
            continue;
        }

        if (!evaluation->evaluated ||
            !fitness_is_finite(&evaluation->fitness)) {
            return false;
        }

        ++valid_count;
        if (!has_best ||
            evaluation->fitness.total >
                population->evaluations[best_index].fitness.total) {
            best_index = index;
            has_best = true;
        }
    }

    if (valid_count != population->valid_count ||
        has_best != population->has_best ||
        (!has_best && population->best_index != 0) ||
        (has_best && population->best_index != best_index)) {
        return false;
    }

    *validated_valid_count = valid_count;
    return true;
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

    if (config == NULL || population == NULL || rng == NULL ||
        selected_index == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (config->population_size == 0 ||
        config->tournament_size == 0 ||
        config->tournament_size > config->population_size) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!completed_population_is_consistent(
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
                population, valid_ordinal, &candidate_index)) {
            return EVO_ERROR_STATE;
        }

        if (!has_winner ||
            population->evaluations[candidate_index].fitness.total >
                population->evaluations[winner_index].fitness.total ||
            (population->evaluations[candidate_index].fitness.total ==
                 population->evaluations[winner_index].fitness.total &&
             candidate_index < winner_index)) {
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
