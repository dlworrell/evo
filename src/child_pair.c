#include "internal/child_pair.h"

#include "internal/crossover.h"
#include "internal/mutation.h"

#include <math.h>
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

static bool operator_policy_is_valid(const evo_problem_t *problem,
                                     const evo_config_t *config)
{
    return problem->genome_size != 0 &&
           config->population_size != 0 &&
           config->tournament_size != 0 &&
           config->tournament_size <= config->population_size &&
           config->max_genome_bytes != 0 &&
           problem->genome_size <= config->max_genome_bytes &&
           config->max_child_population_bytes != 0 &&
           isfinite(config->crossover_rate) &&
           config->crossover_rate >= 0.0 &&
           config->crossover_rate <= 1.0 &&
           isfinite(config->mutation_rate) &&
           config->mutation_rate >= 0.0 &&
           config->mutation_rate <= 1.0;
}

static bool child_progress_is_valid(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    size_t pair_index,
    const evo_population_t *children)
{
    size_t expected_storage_bytes = 0;
    size_t expected_produced_count = 0;
    size_t complete_pair_count = 0;

    if (parents == children || children->genomes == NULL ||
        children->genomes == parents->genomes ||
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
        children->storage_bytes > config->max_child_population_bytes) {
        return false;
    }

    complete_pair_count = children->population_size / 2;
    if (pair_index >= complete_pair_count) {
        return false;
    }

    expected_produced_count = pair_index * 2;
    if (children->produced_count != expected_produced_count) {
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

static bool size_index_to_u64(size_t index, uint64_t *converted)
{
    const uint64_t candidate = (uint64_t)index;

    if (converted == NULL || (size_t)candidate != index) {
        return false;
    }

    *converted = candidate;
    return true;
}

evo_status_t evo_child_pair_produce(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    const evo_population_t *parents,
    uint64_t source_generation,
    size_t pair_index,
    evo_population_t *children,
    evo_child_pair_evidence_t *evidence)
{
    evo_child_pair_evidence_t candidate = {0};
    evo_rng_t crossover_rng = {0};
    evo_rng_t mutation_a_rng = {0};
    evo_rng_t mutation_b_rng = {0};
    const void *parent_a = NULL;
    const void *parent_b = NULL;
    void *child_a = NULL;
    void *child_b = NULL;
    uint64_t pair_stream_index = 0;
    uint64_t child_a_stream_index = 0;
    uint64_t child_b_stream_index = 0;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || parents == NULL ||
        children == NULL || evidence == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!operator_policy_is_valid(problem, config)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!child_progress_is_valid(problem,
                                 config,
                                 parents,
                                 source_generation,
                                 pair_index,
                                 children)) {
        return EVO_ERROR_STATE;
    }

    status = evo_parent_pair_plan(config,
                                  parents,
                                  source_generation,
                                  pair_index,
                                  &candidate.plan);
    if (status != EVO_SUCCESS) {
        return status;
    }

    if (!size_index_to_u64(candidate.plan.pair_index,
                           &pair_stream_index) ||
        !size_index_to_u64(candidate.plan.child_a_index,
                           &child_a_stream_index) ||
        !size_index_to_u64(candidate.plan.child_b_index,
                           &child_b_stream_index) ||
        !evo_rng_derive_operator_stream(
            &crossover_rng,
            config->random_seed,
            source_generation,
            pair_stream_index,
            EVO_OPERATOR_RNG_DOMAIN_CROSSOVER) ||
        !evo_rng_derive_operator_stream(
            &mutation_a_rng,
            config->random_seed,
            source_generation,
            child_a_stream_index,
            EVO_OPERATOR_RNG_DOMAIN_MUTATION) ||
        !evo_rng_derive_operator_stream(
            &mutation_b_rng,
            config->random_seed,
            source_generation,
            child_b_stream_index,
            EVO_OPERATOR_RNG_DOMAIN_MUTATION)) {
        return EVO_ERROR_STATE;
    }

    parent_a = evo_population_genome_const(
        parents, candidate.plan.parent_a_index);
    parent_b = evo_population_genome_const(
        parents, candidate.plan.parent_b_index);
    child_a = evo_population_genome(
        children, candidate.plan.child_a_index);
    child_b = evo_population_genome(
        children, candidate.plan.child_b_index);

    if (parent_a == NULL || parent_b == NULL || child_a == NULL ||
        child_b == NULL || child_a == child_b || child_a == parent_a ||
        child_a == parent_b || child_b == parent_a ||
        child_b == parent_b) {
        return EVO_ERROR_STATE;
    }

    /*
     * Every expected fallible condition is now resolved. These dispatchers
     * cannot reject the validated views and seeded streams under their
     * contracts; consumer callback side effects remain non-transactional.
     */
    status = evo_crossover_pair(problem,
                                config,
                                context,
                                &crossover_rng,
                                parent_a,
                                parent_b,
                                child_a,
                                child_b);
    if (status != EVO_SUCCESS) {
        return EVO_ERROR_STATE;
    }

    status = evo_mutate_genome(problem,
                               config,
                               context,
                               &mutation_a_rng,
                               child_a);
    if (status != EVO_SUCCESS) {
        return EVO_ERROR_STATE;
    }

    status = evo_mutate_genome(problem,
                               config,
                               context,
                               &mutation_b_rng,
                               child_b);
    if (status != EVO_SUCCESS) {
        return EVO_ERROR_STATE;
    }

    children->produced_count = candidate.plan.child_b_index + 1;
    children->source_generation = source_generation;
    children->operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;

    candidate.produced_count = children->produced_count;
    candidate.rng_algorithm_version = EVO_RNG_ALGORITHM_VERSION;
    candidate.complete = true;
    *evidence = candidate;
    return EVO_SUCCESS;
}
