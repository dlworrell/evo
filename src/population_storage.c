#include "internal/population_storage.h"
#include "internal/child_tail.h"
#include "internal/fitness.h"
#include "internal/rng.h"

#include <stdint.h>
#include <stdlib.h>

static bool checked_size_multiply(size_t left, size_t right, size_t *product)
{
    if (product == NULL) {
        return false;
    }

    if (left != 0 && right > SIZE_MAX / left) {
        return false;
    }

    *product = left * right;
    return true;
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

static bool completed_population_provenance_is_valid(
    const evo_config_t *config,
    const evo_population_t *population)
{
    uint32_t expected_odd_child_policy_version = 0;

    if (population->initialized) {
        return population->initialization_seed == config->random_seed &&
               population->rng_algorithm_version ==
                   EVO_RNG_ALGORITHM_VERSION &&
               population->produced_count == 0 &&
               population->source_generation == 0 &&
               population->operator_seed_schedule_version == 0 &&
               population->odd_child_policy_version == 0 &&
               population->storage_bytes <=
                   config->max_population_bytes;
    }

    if (population->population_size % 2 != 0) {
        expected_odd_child_policy_version =
            EVO_ODD_CHILD_POLICY_VERSION;
    }

    return population->initialization_seed == 0 &&
           population->rng_algorithm_version == 0 &&
           population->produced_count == population->population_size &&
           population->operator_seed_schedule_version ==
               EVO_OPERATOR_SEED_SCHEDULE_VERSION &&
           population->odd_child_policy_version ==
               expected_odd_child_policy_version &&
           population->storage_bytes <=
               config->max_child_population_bytes;
}

bool evo_population_validate_completed(
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
        population->genome_size == 0 || !population->evaluated ||
        population->population_size != config->population_size ||
        !completed_population_provenance_is_valid(config, population) ||
        !checked_size_multiply(population->population_size,
                               population->genome_size,
                               &expected_storage_bytes) ||
        expected_storage_bytes != population->storage_bytes ||
        population->genome_size > config->max_genome_bytes ||
        !checked_size_multiply(
            population->population_size,
            sizeof(evo_candidate_evaluation_t),
            &expected_evaluation_bytes) ||
        expected_evaluation_bytes != population->evaluation_bytes ||
        population->evaluation_bytes > config->max_evaluation_bytes ||
        population->fitness_comparison_policy_version !=
            EVO_FITNESS_COMPARISON_POLICY_VERSION) {
        return false;
    }

    for (size_t index = 0;
         index < population->population_size;
         ++index) {
        const evo_candidate_evaluation_t *evaluation =
            &population->evaluations[index];
        const evo_fitness_candidate_view_t candidate_view = {
            .fitness = &evaluation->fitness,
            .generation = UINT64_C(0),
            .population_index = index,
            .hard_valid = evaluation->valid,
            .evaluated = evaluation->evaluated,
        };

        if (!evaluation->valid) {
            if (evaluation->evaluated ||
                !fitness_is_zero(&evaluation->fitness)) {
                return false;
            }
            continue;
        }

        if (!evo_fitness_candidate_is_rankable(&candidate_view)) {
            return false;
        }

        ++valid_count;
        if (!has_best) {
            best_index = index;
            has_best = true;
        } else {
            const evo_candidate_evaluation_t *best_evaluation =
                &population->evaluations[best_index];
            const evo_fitness_candidate_view_t best_view = {
                .fitness = &best_evaluation->fitness,
                .generation = UINT64_C(0),
                .population_index = best_index,
                .hard_valid = best_evaluation->valid,
                .evaluated = best_evaluation->evaluated,
            };
            evo_fitness_order_t order = EVO_FITNESS_ORDER_EQUAL;

            if (!evo_fitness_compare_candidates(&candidate_view,
                                                &best_view,
                                                &order)) {
                return false;
            }
            if (order == EVO_FITNESS_ORDER_LEFT) {
                best_index = index;
            }
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

static bool population_genome_offset(const evo_population_t *population,
                                     size_t index,
                                     size_t *offset)
{
    size_t calculated_offset = 0;

    if (population == NULL || population->genomes == NULL || offset == NULL ||
        index >= population->population_size ||
        !checked_size_multiply(index,
                               population->genome_size,
                               &calculated_offset)) {
        return false;
    }

    if (calculated_offset > population->storage_bytes ||
        population->genome_size >
            population->storage_bytes - calculated_offset) {
        return false;
    }

    *offset = calculated_offset;
    return true;
}

static evo_status_t population_allocate(const evo_problem_t *problem,
                                        const evo_config_t *config,
                                        size_t storage_budget,
                                        evo_population_t *population)
{
    size_t storage_bytes = 0;

    if (problem->genome_size == 0 || config->population_size == 0 ||
        config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        storage_budget == 0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!checked_size_multiply(config->population_size,
                               problem->genome_size,
                               &storage_bytes) ||
        storage_bytes > storage_budget) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    population->genomes = calloc(1, storage_bytes);
    if (population->genomes == NULL) {
        return EVO_ERROR_OUT_OF_MEMORY;
    }

    population->population_size = config->population_size;
    population->genome_size = problem->genome_size;
    population->storage_bytes = storage_bytes;
    return EVO_SUCCESS;
}

evo_status_t evo_population_create(const evo_problem_t *problem,
                                   const evo_config_t *config,
                                   evo_population_t *population)
{
    if (population == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (population->genomes != NULL || population->evaluations != NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    *population = (evo_population_t){0};

    if (problem == NULL || config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    return population_allocate(problem,
                               config,
                               config->max_population_bytes,
                               population);
}

evo_status_t evo_child_population_create(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    evo_population_t *children)
{
    size_t valid_count = 0;

    if (children == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (children == parents || children->genomes != NULL ||
        children->evaluations != NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    *children = (evo_population_t){0};

    if (problem == NULL || config == NULL || parents == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (parents->genome_size != problem->genome_size ||
        !evo_population_validate_completed(
            config, parents, &valid_count)) {
        return EVO_ERROR_STATE;
    }

    return population_allocate(problem,
                               config,
                               config->max_child_population_bytes,
                               children);
}

void *evo_population_genome(evo_population_t *population, size_t index)
{
    size_t offset = 0;

    if (!population_genome_offset(population, index, &offset)) {
        return NULL;
    }

    return population->genomes + offset;
}

const void *evo_population_genome_const(const evo_population_t *population,
                                        size_t index)
{
    size_t offset = 0;

    if (!population_genome_offset(population, index, &offset)) {
        return NULL;
    }

    return population->genomes + offset;
}

void evo_population_destroy(evo_population_t *population)
{
    if (population == NULL) {
        return;
    }

    free(population->evaluations);
    free(population->genomes);
    *population = (evo_population_t){0};
}
