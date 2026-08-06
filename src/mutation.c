#include "internal/mutation.h"

#include <math.h>

bool evo_mutation_operator_is_valid(
    evo_mutation_operator_t mutation_operator)
{
    switch (mutation_operator) {
    case EVO_MUTATION_CONSUMER:
    case EVO_MUTATION_BYTE_XOR:
        return true;
    default:
        return false;
    }
}

evo_status_t evo_mutation_validate_config(const evo_problem_t *problem,
                                          const evo_config_t *config)
{
    if (problem == NULL || config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (problem->genome_size == 0 || config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        !isfinite(config->mutation_rate) ||
        config->mutation_rate < 0.0 ||
        config->mutation_rate > 1.0 ||
        !evo_mutation_operator_is_valid(config->mutation_operator)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    return EVO_SUCCESS;
}

static bool mutate_byte_xor(evo_rng_t *rng,
                            unsigned char *genome,
                            size_t genome_size)
{
    size_t byte_index = 0;
    size_t mask_index = 0;

    if (!evo_rng_uniform_index(rng, genome_size, &byte_index) ||
        !evo_rng_uniform_index(rng, 255, &mask_index)) {
        return false;
    }

    genome[byte_index] ^=
        (unsigned char)(mask_index + 1);
    return true;
}

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

    {
        const evo_status_t validation_status =
            evo_mutation_validate_config(problem, config);

        if (validation_status != EVO_SUCCESS) {
            return validation_status;
        }
    }

    if (!rng->seeded) {
        return EVO_ERROR_STATE;
    }

    if (!evo_rng_probability_event(
            rng, config->mutation_rate, &mutation_selected)) {
        return EVO_ERROR_STATE;
    }

    if (!mutation_selected) {
        return EVO_SUCCESS;
    }

    switch (config->mutation_operator) {
    case EVO_MUTATION_CONSUMER:
        if (problem->mutate != NULL) {
            problem->mutate(genome, config->mutation_rate, context);
        }
        break;
    case EVO_MUTATION_BYTE_XOR:
        if (!mutate_byte_xor(rng, genome, problem->genome_size)) {
            return EVO_ERROR_STATE;
        }
        break;
    default:
        return EVO_ERROR_STATE;
    }

    return EVO_SUCCESS;
}
