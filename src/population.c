#include "catalyst/evo/evo.h"

#include <stdlib.h>

evo_status_t evo_run(const evo_problem_t *problem, const evo_config_t *config, void *context, evo_result_t *result)
{
    (void)context;

    if (result == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (result->best_genome != NULL) {
        return EVO_ERROR_RESULT_ACTIVE;
    }

    *result = (evo_result_t){0};

    if (problem == NULL || config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (problem->genome_size == 0 || config->population_size == 0 || config->max_genome_bytes == 0 || problem->genome_size > config->max_genome_bytes) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    result->best_genome = calloc(1, problem->genome_size);
    if (result->best_genome == NULL) {
        *result = (evo_result_t){0};
        return EVO_ERROR_OUT_OF_MEMORY;
    }

    result->best_fitness = (evo_fitness_t){0};
    result->generations_completed = 0;
    result->random_seed = config->random_seed;
    return EVO_SUCCESS;
}

void evo_result_destroy(evo_result_t *result)
{
    if (result == NULL) {
        return;
    }

    free(result->best_genome);
    *result = (evo_result_t){0};
}
