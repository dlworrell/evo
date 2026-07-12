#include "catalyst/evo/evo.h"

#include <stdlib.h>

int evo_run(const evo_problem_t *problem,
            const evo_config_t *config,
            void *context,
            evo_result_t *result)
{
    (void)context;

    if (problem == NULL || config == NULL || result == NULL ||
        problem->genome_size == 0 || config->population_size == 0) {
        return -1;
    }

    result->best_genome = calloc(1, problem->genome_size);
    if (result->best_genome == NULL) {
        return -2;
    }

    result->generations_completed = 0;
    result->random_seed = config->random_seed;
    return 0;
}

void evo_result_destroy(evo_result_t *result)
{
    if (result == NULL) {
        return;
    }

    free(result->best_genome);
    result->best_genome = NULL;
}
