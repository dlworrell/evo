#include "internal/statistics.h"

#include <math.h>
#include <stdint.h>

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

static bool statistics_target_is_independent(
    const evo_population_t *population,
    const evo_generation_statistics_t *statistics)
{
    if (byte_ranges_overlap(population,
                            sizeof(*population),
                            statistics,
                            sizeof(*statistics))) {
        return false;
    }

    if (population->genomes != NULL && population->storage_bytes != 0 &&
        byte_ranges_overlap(population->genomes,
                            population->storage_bytes,
                            statistics,
                            sizeof(*statistics))) {
        return false;
    }

    return population->evaluations == NULL ||
           population->evaluation_bytes == 0 ||
           !byte_ranges_overlap(population->evaluations,
                                population->evaluation_bytes,
                                statistics,
                                sizeof(*statistics));
}

static bool population_matches_generation(
    const evo_population_t *population,
    uint64_t generation_index)
{
    if (population->initialized) {
        return generation_index == UINT64_C(0) &&
               population->source_generation == UINT64_C(0);
    }

    return population->source_generation != UINT64_MAX &&
           generation_index ==
               population->source_generation + UINT64_C(1);
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

static bool add_fitness(evo_fitness_t *sums,
                        const evo_fitness_t *fitness)
{
    if (!fitness_is_finite(fitness)) {
        return false;
    }

    sums->correctness += fitness->correctness;
    sums->performance += fitness->performance;
    sums->memory_use += fitness->memory_use;
    sums->reliability += fitness->reliability;
    sums->maintainability += fitness->maintainability;
    sums->constraint_penalty += fitness->constraint_penalty;
    sums->total += fitness->total;
    return fitness_is_finite(sums);
}

evo_status_t evo_generation_statistics_record(
    const evo_population_t *population,
    uint64_t generation_index,
    evo_generation_statistics_t *statistics)
{
    evo_generation_statistics_t candidate = {0};
    size_t expected_evaluation_bytes = 0;
    size_t valid_count = 0;

    if (population == NULL || statistics == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!statistics_target_is_independent(population, statistics)) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (population->genomes == NULL || population->evaluations == NULL ||
        population->population_size == 0 || !population->evaluated ||
        population->valid_count > population->population_size ||
        !population_matches_generation(population, generation_index) ||
        !checked_size_multiply(population->population_size,
                               sizeof(evo_candidate_evaluation_t),
                               &expected_evaluation_bytes) ||
        expected_evaluation_bytes != population->evaluation_bytes) {
        return EVO_ERROR_STATE;
    }

    candidate.version = EVO_GENERATION_STATISTICS_VERSION;
    candidate.generation_index = generation_index;
    candidate.population_size = population->population_size;

    for (size_t index = 0; index < population->population_size; ++index) {
        const evo_candidate_evaluation_t *evaluation =
            &population->evaluations[index];

        if (!evaluation->valid) {
            if (evaluation->evaluated) {
                return EVO_ERROR_STATE;
            }
            continue;
        }

        if (!evaluation->evaluated) {
            return EVO_ERROR_STATE;
        }

        if (!add_fitness(&candidate.fitness_sums,
                         &evaluation->fitness)) {
            return EVO_ERROR_EVALUATION;
        }
        ++valid_count;
    }

    if (valid_count != population->valid_count ||
        population->has_best != (valid_count != 0)) {
        return EVO_ERROR_STATE;
    }

    candidate.valid_count = valid_count;
    candidate.invalid_count =
        candidate.population_size - candidate.valid_count;
    candidate.has_best = population->has_best;

    if (candidate.has_best) {
        const evo_candidate_evaluation_t *best = NULL;

        if (population->best_index >= population->population_size) {
            return EVO_ERROR_STATE;
        }

        best = &population->evaluations[population->best_index];
        if (!best->valid || !best->evaluated ||
            !fitness_is_finite(&best->fitness)) {
            return EVO_ERROR_STATE;
        }

        candidate.best_index = population->best_index;
        candidate.best_fitness = best->fitness;
    } else if (population->best_index != 0) {
        return EVO_ERROR_STATE;
    }

    *statistics = candidate;
    return EVO_SUCCESS;
}
