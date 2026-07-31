#include "internal/crossover.h"

#include <math.h>

static void clone_genome(const void *parent,
                         void *child,
                         size_t genome_size)
{
    const unsigned char *source = parent;
    unsigned char *destination = child;

    for (size_t offset = 0; offset < genome_size; ++offset) {
        destination[offset] = source[offset];
    }
}

evo_status_t evo_crossover_pair(const evo_problem_t *problem,
                                const evo_config_t *config,
                                void *context,
                                evo_rng_t *rng,
                                const void *parent_a,
                                const void *parent_b,
                                void *child_a,
                                void *child_b)
{
    bool crossover_selected = false;

    if (problem == NULL || config == NULL || rng == NULL ||
        parent_a == NULL || parent_b == NULL || child_a == NULL ||
        child_b == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (problem->genome_size == 0 || config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        !isfinite(config->crossover_rate) ||
        config->crossover_rate < 0.0 ||
        config->crossover_rate > 1.0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!rng->seeded) {
        return EVO_ERROR_STATE;
    }

    if (child_a == child_b || child_a == parent_a ||
        child_a == parent_b || child_b == parent_a ||
        child_b == parent_b) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!evo_rng_probability_event(
            rng, config->crossover_rate, &crossover_selected)) {
        return EVO_ERROR_STATE;
    }

    if (crossover_selected && problem->crossover != NULL) {
        problem->crossover(
            parent_a, parent_b, child_a, child_b, context);
    } else {
        clone_genome(parent_a, child_a, problem->genome_size);
        clone_genome(parent_b, child_b, problem->genome_size);
    }

    return EVO_SUCCESS;
}
