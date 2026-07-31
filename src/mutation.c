#include "internal/mutation.h"

#include <math.h>

evo_status_t evo_mutate_genome(const evo_problem_t *problem,
                               const evo_config_t *config,
                               void *context,
                               evo_rng_t *rng,
                               void *genome)
{
    bool mutation_selected = false;

    if (problem == NULL || config == NULL || rng == NULL ||
        genome == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (problem->genome_size == 0 || config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        !isfinite(config->mutation_rate) ||
        config->mutation_rate < 0.0 ||
        config->mutation_rate > 1.0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!rng->seeded) {
        return EVO_ERROR_STATE;
    }

    if (!evo_rng_probability_event(
            rng, config->mutation_rate, &mutation_selected)) {
        return EVO_ERROR_STATE;
    }

    if (mutation_selected && problem->mutate != NULL) {
        problem->mutate(genome, config->mutation_rate, context);
    }

    return EVO_SUCCESS;
}
