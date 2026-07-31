#include "internal/population_storage.h"

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

evo_status_t evo_population_create(const evo_problem_t *problem,
                                   const evo_config_t *config,
                                   evo_population_t *population)
{
    size_t storage_bytes = 0;

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

    if (problem->genome_size == 0 || config->population_size == 0 ||
        config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        config->max_population_bytes == 0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!checked_size_multiply(config->population_size,
                               problem->genome_size,
                               &storage_bytes) ||
        storage_bytes > config->max_population_bytes) {
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
