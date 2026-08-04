#include "internal/crossover.h"

#include <math.h>
#include <stdint.h>

bool evo_crossover_operator_is_valid(
    evo_crossover_operator_t crossover_operator)
{
    switch (crossover_operator) {
    case EVO_CROSSOVER_CONSUMER:
    case EVO_CROSSOVER_BYTE_ONE_POINT:
    case EVO_CROSSOVER_BYTE_TWO_POINT:
    case EVO_CROSSOVER_BYTE_UNIFORM:
        return true;
    default:
        return false;
    }
}

evo_status_t evo_crossover_validate_config(const evo_problem_t *problem,
                                           const evo_config_t *config)
{
    if (problem == NULL || config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (problem->genome_size == 0 || config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        !isfinite(config->crossover_rate) ||
        config->crossover_rate < 0.0 ||
        config->crossover_rate > 1.0 ||
        !evo_crossover_operator_is_valid(config->crossover_operator) ||
        (config->crossover_operator == EVO_CROSSOVER_BYTE_TWO_POINT &&
         problem->genome_size == SIZE_MAX)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    return EVO_SUCCESS;
}

static bool byte_ranges_overlap(const void *left,
                                const void *right,
                                size_t size)
{
    const uintmax_t left_start = (uintmax_t)(uintptr_t)left;
    const uintmax_t right_start = (uintmax_t)(uintptr_t)right;
    uintmax_t left_end = 0;
    uintmax_t right_end = 0;

    if (left == NULL || right == NULL || size == 0 ||
        (uintmax_t)size > UINTMAX_MAX - left_start ||
        (uintmax_t)size > UINTMAX_MAX - right_start) {
        return true;
    }

    left_end = left_start + (uintmax_t)size;
    right_end = right_start + (uintmax_t)size;
    return left_start < right_end && right_start < left_end;
}

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

static void write_crossed_byte(const unsigned char *parent_a,
                               const unsigned char *parent_b,
                               unsigned char *child_a,
                               unsigned char *child_b,
                               size_t offset,
                               bool swap)
{
    child_a[offset] = swap ? parent_b[offset] : parent_a[offset];
    child_b[offset] = swap ? parent_a[offset] : parent_b[offset];
}

static bool crossover_one_point(evo_rng_t *rng,
                                const unsigned char *parent_a,
                                const unsigned char *parent_b,
                                unsigned char *child_a,
                                unsigned char *child_b,
                                size_t genome_size)
{
    size_t cut = 0;

    if (genome_size == 1) {
        clone_genome(parent_a, child_a, genome_size);
        clone_genome(parent_b, child_b, genome_size);
        return true;
    }

    if (!evo_rng_uniform_index(rng, genome_size - 1, &cut)) {
        return false;
    }
    ++cut;

    for (size_t offset = 0; offset < genome_size; ++offset) {
        write_crossed_byte(parent_a,
                           parent_b,
                           child_a,
                           child_b,
                           offset,
                           offset >= cut);
    }
    return true;
}

static bool crossover_two_point(evo_rng_t *rng,
                                const unsigned char *parent_a,
                                const unsigned char *parent_b,
                                unsigned char *child_a,
                                unsigned char *child_b,
                                size_t genome_size)
{
    size_t first = 0;
    size_t second_rank = 0;
    size_t second = 0;
    size_t lower = 0;
    size_t upper = 0;

    if (!evo_rng_uniform_index(rng, genome_size + 1, &first) ||
        !evo_rng_uniform_index(rng, genome_size, &second_rank)) {
        return false;
    }

    second = second_rank >= first ? second_rank + 1 : second_rank;
    lower = first < second ? first : second;
    upper = first < second ? second : first;

    for (size_t offset = 0; offset < genome_size; ++offset) {
        write_crossed_byte(parent_a,
                           parent_b,
                           child_a,
                           child_b,
                           offset,
                           offset >= lower && offset < upper);
    }
    return true;
}

static bool crossover_uniform(evo_rng_t *rng,
                              const unsigned char *parent_a,
                              const unsigned char *parent_b,
                              unsigned char *child_a,
                              unsigned char *child_b,
                              size_t genome_size)
{
    uint32_t mask = 0;
    unsigned int available_bits = 0;

    for (size_t offset = 0; offset < genome_size; ++offset) {
        if (available_bits == 0) {
            if (!evo_rng_next_u32(rng, &mask)) {
                return false;
            }
            available_bits = 32u;
        }

        write_crossed_byte(parent_a,
                           parent_b,
                           child_a,
                           child_b,
                           offset,
                           (mask & UINT32_C(1)) != 0);
        mask >>= 1u;
        --available_bits;
    }
    return true;
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

    {
        const evo_status_t validation_status =
            evo_crossover_validate_config(problem, config);

        if (validation_status != EVO_SUCCESS) {
            return validation_status;
        }
    }

    if (!rng->seeded) {
        return EVO_ERROR_STATE;
    }

    if (byte_ranges_overlap(child_a,
                            child_b,
                            problem->genome_size) ||
        byte_ranges_overlap(child_a,
                            parent_a,
                            problem->genome_size) ||
        byte_ranges_overlap(child_a,
                            parent_b,
                            problem->genome_size) ||
        byte_ranges_overlap(child_b,
                            parent_a,
                            problem->genome_size) ||
        byte_ranges_overlap(child_b,
                            parent_b,
                            problem->genome_size)) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!evo_rng_probability_event(
            rng, config->crossover_rate, &crossover_selected)) {
        return EVO_ERROR_STATE;
    }

    if (!crossover_selected) {
        clone_genome(parent_a, child_a, problem->genome_size);
        clone_genome(parent_b, child_b, problem->genome_size);
        return EVO_SUCCESS;
    }

    switch (config->crossover_operator) {
    case EVO_CROSSOVER_CONSUMER:
        if (problem->crossover != NULL) {
            problem->crossover(
                parent_a, parent_b, child_a, child_b, context);
        } else {
            clone_genome(parent_a, child_a, problem->genome_size);
            clone_genome(parent_b, child_b, problem->genome_size);
        }
        break;
    case EVO_CROSSOVER_BYTE_ONE_POINT:
        if (!crossover_one_point(rng,
                                 parent_a,
                                 parent_b,
                                 child_a,
                                 child_b,
                                 problem->genome_size)) {
            return EVO_ERROR_STATE;
        }
        break;
    case EVO_CROSSOVER_BYTE_TWO_POINT:
        if (!crossover_two_point(rng,
                                 parent_a,
                                 parent_b,
                                 child_a,
                                 child_b,
                                 problem->genome_size)) {
            return EVO_ERROR_STATE;
        }
        break;
    case EVO_CROSSOVER_BYTE_UNIFORM:
        if (!crossover_uniform(rng,
                               parent_a,
                               parent_b,
                               child_a,
                               child_b,
                               problem->genome_size)) {
            return EVO_ERROR_STATE;
        }
        break;
    default:
        return EVO_ERROR_STATE;
    }

    return EVO_SUCCESS;
}
