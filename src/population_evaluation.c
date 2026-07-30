#include "internal/population_storage.h"
#include "internal/rng.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static bool evaluation_storage_size(size_t population_size, size_t *bytes)
{
    if (bytes == NULL || population_size == 0 ||
        population_size > SIZE_MAX / sizeof(evo_candidate_evaluation_t)) {
        return false;
    }

    *bytes = population_size * sizeof(evo_candidate_evaluation_t);
    return true;
}

static bool population_ready_for_evaluation(const evo_problem_t *problem,
                                            const evo_config_t *config,
                                            const evo_population_t *population)
{
    size_t expected_storage_bytes = 0;

    if (population->genomes == NULL || population->evaluations != NULL ||
        !population->initialized || population->evaluated ||
        population->evaluation_bytes != 0 || population->valid_count != 0 ||
        population->best_index != 0 || population->has_best ||
        population->population_size == 0 || population->genome_size == 0 ||
        population->storage_bytes == 0 ||
        population->population_size != config->population_size ||
        population->genome_size != problem->genome_size ||
        population->initialization_seed != config->random_seed ||
        population->rng_algorithm_version != EVO_RNG_ALGORITHM_VERSION ||
        config->max_genome_bytes < population->genome_size ||
        config->max_population_bytes < population->storage_bytes ||
        population->population_size >
            SIZE_MAX / population->genome_size) {
        return false;
    }

    expected_storage_bytes =
        population->population_size * population->genome_size;
    return expected_storage_bytes == population->storage_bytes;
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

static evo_status_t discard_provisional_evaluations(
    evo_candidate_evaluation_t *evaluations,
    evo_status_t status)
{
    free(evaluations);
    return status;
}

evo_status_t evo_population_evaluate(const evo_problem_t *problem,
                                     const evo_config_t *config,
                                     void *context,
                                     evo_population_t *population)
{
    evo_candidate_evaluation_t *evaluations = NULL;
    size_t evaluation_bytes = 0;
    size_t valid_count = 0;
    size_t best_index = 0;
    bool has_best = false;

    if (problem == NULL || config == NULL || population == NULL ||
        problem->evaluate == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!population_ready_for_evaluation(problem, config, population)) {
        return EVO_ERROR_STATE;
    }

    if (config->max_evaluation_bytes == 0 ||
        !evaluation_storage_size(population->population_size,
                                 &evaluation_bytes) ||
        evaluation_bytes > config->max_evaluation_bytes) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    evaluations = calloc(1, evaluation_bytes);
    if (evaluations == NULL) {
        return EVO_ERROR_OUT_OF_MEMORY;
    }

    for (size_t index = 0; index < population->population_size; ++index) {
        const void *genome =
            evo_population_genome_const(population, index);

        if (genome == NULL) {
            return discard_provisional_evaluations(evaluations,
                                                   EVO_ERROR_STATE);
        }

        evaluations[index].valid =
            problem->is_valid == NULL ||
            problem->is_valid(genome, context);
        if (evaluations[index].valid) {
            ++valid_count;
        }
    }

    for (size_t index = 0; index < population->population_size; ++index) {
        const void *genome = NULL;

        if (!evaluations[index].valid) {
            continue;
        }

        genome = evo_population_genome_const(population, index);
        if (genome == NULL) {
            return discard_provisional_evaluations(evaluations,
                                                   EVO_ERROR_STATE);
        }

        evaluations[index].fitness = problem->evaluate(genome, context);
        if (!fitness_is_finite(&evaluations[index].fitness)) {
            return discard_provisional_evaluations(
                evaluations, EVO_ERROR_EVALUATION);
        }
        evaluations[index].evaluated = true;

        if (!has_best ||
            evaluations[index].fitness.total >
                evaluations[best_index].fitness.total) {
            best_index = index;
            has_best = true;
        }
    }

    population->evaluations = evaluations;
    population->evaluation_bytes = evaluation_bytes;
    population->valid_count = valid_count;
    population->best_index = best_index;
    population->has_best = has_best;
    population->evaluated = true;
    return EVO_SUCCESS;
}

const evo_candidate_evaluation_t *
evo_population_evaluation_const(const evo_population_t *population,
                                size_t index)
{
    if (population == NULL || population->evaluations == NULL ||
        !population->evaluated || index >= population->population_size) {
        return NULL;
    }

    return &population->evaluations[index];
}

bool evo_population_best_index(const evo_population_t *population,
                               size_t *best_index)
{
    if (population == NULL || best_index == NULL ||
        population->evaluations == NULL || !population->evaluated ||
        !population->has_best ||
        population->best_index >= population->population_size) {
        return false;
    }

    *best_index = population->best_index;
    return true;
}
