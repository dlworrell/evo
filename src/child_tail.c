#include "internal/child_tail.h"

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

static bool child_tail_state_is_valid(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    const evo_population_t *children)
{
    size_t expected_storage_bytes = 0;

    if (parents == children || children->genomes == NULL ||
        byte_ranges_overlap(parents->genomes,
                            parents->storage_bytes,
                            children->genomes,
                            children->storage_bytes) ||
        children->evaluations != NULL ||
        children->population_size != config->population_size ||
        children->population_size != parents->population_size ||
        children->genome_size != problem->genome_size ||
        children->genome_size != parents->genome_size ||
        children->evaluation_bytes != 0 ||
        children->valid_count != 0 || children->best_index != 0 ||
        children->initialization_seed != 0 ||
        children->rng_algorithm_version != 0 ||
        children->odd_child_policy_version != 0 ||
        children->initialized || children->has_best ||
        children->evaluated ||
        !checked_size_multiply(children->population_size,
                               children->genome_size,
                               &expected_storage_bytes) ||
        expected_storage_bytes != children->storage_bytes ||
        children->storage_bytes > config->max_child_population_bytes ||
        children->produced_count != children->population_size - 1) {
        return false;
    }

    if (children->produced_count == 0) {
        return children->source_generation == 0 &&
               children->operator_seed_schedule_version == 0;
    }

    return children->source_generation == source_generation &&
           children->operator_seed_schedule_version ==
               EVO_OPERATOR_SEED_SCHEDULE_VERSION;
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

evo_status_t evo_child_tail_produce(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    evo_population_t *children,
    evo_child_tail_evidence_t *evidence)
{
    evo_child_tail_evidence_t candidate = {0};
    const void *parent = NULL;
    void *child = NULL;
    size_t valid_count = 0;

    if (problem == NULL || config == NULL || parents == NULL ||
        children == NULL || evidence == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (problem->genome_size == 0 || config->population_size == 0 ||
        config->population_size % 2 == 0 ||
        config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        config->max_child_population_bytes == 0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (parents->genome_size != problem->genome_size ||
        !evo_population_validate_completed(
            config, parents, &valid_count)) {
        return EVO_ERROR_STATE;
    }

    if (valid_count == 0 || !parents->has_best) {
        return EVO_ERROR_NO_VALID_CANDIDATE;
    }

    if (!child_tail_state_is_valid(problem,
                                   config,
                                   parents,
                                   source_generation,
                                   children)) {
        return EVO_ERROR_STATE;
    }

    candidate.parent_index = parents->best_index;
    candidate.child_index = children->population_size - 1;
    parent = evo_population_genome_const(parents,
                                         candidate.parent_index);
    child = evo_population_genome(children, candidate.child_index);

    if (parent == NULL || child == NULL || parent == child) {
        return EVO_ERROR_STATE;
    }

    clone_genome(parent, child, children->genome_size);

    children->produced_count = children->population_size;
    children->source_generation = source_generation;
    children->operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;
    children->odd_child_policy_version =
        EVO_ODD_CHILD_POLICY_VERSION;

    candidate.produced_count = children->produced_count;
    candidate.source_generation = source_generation;
    candidate.operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;
    candidate.policy_version = EVO_ODD_CHILD_POLICY_VERSION;
    candidate.complete = true;
    *evidence = candidate;
    return EVO_SUCCESS;
}
