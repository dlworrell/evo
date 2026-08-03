#include "internal/child_single.h"

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

static bool production_objects_are_independent(
    const evo_population_t *parents,
    const evo_population_t *children,
    const evo_child_single_evidence_t *evidence)
{
    return !byte_ranges_overlap(parents,
                                sizeof(*parents),
                                children,
                                sizeof(*children)) &&
           !byte_ranges_overlap(parents,
                                sizeof(*parents),
                                evidence,
                                sizeof(*evidence)) &&
           !byte_ranges_overlap(children,
                                sizeof(*children),
                                evidence,
                                sizeof(*evidence)) &&
           !byte_ranges_overlap(parents->genomes,
                                parents->storage_bytes,
                                parents->evaluations,
                                parents->evaluation_bytes) &&
           !byte_ranges_overlap(parents->genomes,
                                parents->storage_bytes,
                                children->genomes,
                                children->storage_bytes) &&
           !byte_ranges_overlap(parents->evaluations,
                                parents->evaluation_bytes,
                                children->genomes,
                                children->storage_bytes) &&
           !byte_ranges_overlap(parents,
                                sizeof(*parents),
                                parents->genomes,
                                parents->storage_bytes) &&
           !byte_ranges_overlap(parents,
                                sizeof(*parents),
                                parents->evaluations,
                                parents->evaluation_bytes) &&
           !byte_ranges_overlap(parents,
                                sizeof(*parents),
                                children->genomes,
                                children->storage_bytes) &&
           !byte_ranges_overlap(children,
                                sizeof(*children),
                                parents->genomes,
                                parents->storage_bytes) &&
           !byte_ranges_overlap(children,
                                sizeof(*children),
                                parents->evaluations,
                                parents->evaluation_bytes) &&
           !byte_ranges_overlap(children,
                                sizeof(*children),
                                children->genomes,
                                children->storage_bytes) &&
           !byte_ranges_overlap(evidence,
                                sizeof(*evidence),
                                parents->genomes,
                                parents->storage_bytes) &&
           !byte_ranges_overlap(evidence,
                                sizeof(*evidence),
                                parents->evaluations,
                                parents->evaluation_bytes) &&
           !byte_ranges_overlap(evidence,
                                sizeof(*evidence),
                                children->genomes,
                                children->storage_bytes);
}

static bool child_state_is_valid(const evo_problem_t *problem,
                                 const evo_config_t *config,
                                 const evo_population_t *parents,
                                 uint64_t source_generation,
                                 size_t child_index,
                                 const evo_population_t *children,
                                 const evo_child_single_evidence_t *evidence)
{
    size_t expected_storage_bytes = 0;

    if (!production_objects_are_independent(parents, children, evidence) ||
        children->genomes == NULL || children->evaluations != NULL ||
        children->population_size != config->population_size ||
        children->population_size != parents->population_size ||
        children->genome_size != problem->genome_size ||
        children->genome_size != parents->genome_size ||
        children->evaluation_bytes != 0 || children->valid_count != 0 ||
        children->best_index != 0 ||
        children->produced_count != child_index ||
        children->elite_count != 0 ||
        children->elite_source_valid_count != 0 ||
        children->initialization_seed != 0 ||
        children->rng_algorithm_version != 0 ||
        children->odd_child_policy_version != 0 ||
        children->elite_policy_version != 0 ||
        children->singleton_child_policy_version != 0 ||
        children->fitness_comparison_policy_version != 0 ||
        children->diversity_policy_version != 0 ||
        children->diversity_metric_version != 0 ||
        children->diversity_pair_count != 0 ||
        children->diversity_work_units != 0 ||
        children->diversity != 0.0 || children->initialized ||
        children->has_best || children->evaluated ||
        children->elite_count_explicit ||
        children->diversity_uses_domain_distance ||
        !checked_size_multiply(children->population_size,
                               children->genome_size,
                               &expected_storage_bytes) ||
        expected_storage_bytes != children->storage_bytes ||
        children->storage_bytes > config->max_child_population_bytes) {
        return false;
    }

    if (child_index == 0) {
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

evo_status_t evo_child_single_produce(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    const evo_population_t *parents,
    uint64_t source_generation,
    evo_population_t *children,
    evo_child_single_evidence_t *evidence)
{
    evo_child_single_evidence_t candidate = {0};
    evo_rng_t selection_rng = {0};
    evo_rng_t mutation_rng = {0};
    const void *parent = NULL;
    void *child = NULL;
    size_t valid_count = 0;
    size_t requested_count = 0;
    size_t effective_count = 0;
    size_t offspring_count = 0;
    uint64_t selection_stream_index = 0;
    uint64_t mutation_stream_index = 0;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || parents == NULL ||
        children == NULL || evidence == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (problem->genome_size == 0 || config->population_size == 0 ||
        config->tournament_size == 0 ||
        config->tournament_size > config->population_size ||
        config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        config->max_child_population_bytes == 0 ||
        !isfinite(config->mutation_rate) ||
        config->mutation_rate < 0.0 || config->mutation_rate > 1.0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!production_objects_are_independent(parents,
                                            children,
                                            evidence)) {
        return EVO_ERROR_STATE;
    }

    if (parents->genome_size != problem->genome_size ||
        !evo_population_validate_completed(config, parents, &valid_count)) {
        return EVO_ERROR_STATE;
    }

    status = evo_elite_policy_counts(config,
                                     valid_count,
                                     &requested_count,
                                     &effective_count,
                                     &offspring_count);
    if (status != EVO_SUCCESS) {
        return status;
    }
    (void)requested_count;
    (void)effective_count;

    if (offspring_count == 0 || offspring_count % 2 == 0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    candidate.child_index = offspring_count - 1;
    candidate.selection_stream_index = offspring_count / 2;
    if (!child_state_is_valid(problem,
                              config,
                              parents,
                              source_generation,
                              candidate.child_index,
                              children,
                              evidence) ||
        !size_index_to_u64(candidate.selection_stream_index,
                           &selection_stream_index) ||
        !size_index_to_u64(candidate.child_index,
                           &mutation_stream_index) ||
        !evo_rng_derive_operator_stream(
            &selection_rng,
            config->random_seed,
            source_generation,
            selection_stream_index,
            EVO_OPERATOR_RNG_DOMAIN_SELECTION) ||
        !evo_rng_derive_operator_stream(
            &mutation_rng,
            config->random_seed,
            source_generation,
            mutation_stream_index,
            EVO_OPERATOR_RNG_DOMAIN_MUTATION)) {
        return EVO_ERROR_STATE;
    }

    status = evo_population_select_tournament(config,
                                              parents,
                                              &selection_rng,
                                              &candidate.parent_index);
    if (status != EVO_SUCCESS) {
        return status;
    }

    parent = evo_population_genome_const(parents,
                                         candidate.parent_index);
    child = evo_population_genome(children, candidate.child_index);
    if (parent == NULL || child == NULL || parent == child) {
        return EVO_ERROR_STATE;
    }

    clone_genome(parent, child, children->genome_size);
    status = evo_mutate_genome(problem,
                               config,
                               context,
                               &mutation_rng,
                               child);
    if (status != EVO_SUCCESS) {
        return EVO_ERROR_STATE;
    }

    children->produced_count = offspring_count;
    children->source_generation = source_generation;
    children->operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;
    children->singleton_child_policy_version =
        EVO_SINGLETON_CHILD_POLICY_VERSION;

    candidate.produced_count = children->produced_count;
    candidate.source_generation = source_generation;
    candidate.rng_algorithm_version = EVO_RNG_ALGORITHM_VERSION;
    candidate.operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;
    candidate.policy_version = EVO_SINGLETON_CHILD_POLICY_VERSION;
    candidate.complete = true;
    *evidence = candidate;
    return EVO_SUCCESS;
}
