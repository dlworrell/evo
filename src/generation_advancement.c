#include "internal/generation_advancement.h"

#include <stdint.h>

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

static bool range_overlaps_population_owners(
    const void *range,
    size_t range_size,
    const evo_population_t *population)
{
    return (population->genomes != NULL &&
            population->storage_bytes != 0 &&
            byte_ranges_overlap(range,
                                range_size,
                                population->genomes,
                                population->storage_bytes)) ||
           (population->evaluations != NULL &&
            population->evaluation_bytes != 0 &&
            byte_ranges_overlap(range,
                                range_size,
                                population->evaluations,
                                population->evaluation_bytes));
}

static bool transition_objects_are_independent(
    const evo_population_t *parents,
    const evo_population_t *children,
    const evo_generation_advancement_evidence_t *evidence,
    size_t evidence_size)
{
    return !byte_ranges_overlap(parents,
                                sizeof(*parents),
                                children,
                                sizeof(*children)) &&
           !byte_ranges_overlap(parents,
                                sizeof(*parents),
                                evidence,
                                evidence_size) &&
           !byte_ranges_overlap(children,
                                sizeof(*children),
                                evidence,
                                evidence_size) &&
           !range_overlaps_population_owners(parents,
                                             sizeof(*parents),
                                             parents) &&
           !range_overlaps_population_owners(parents,
                                             sizeof(*parents),
                                             children) &&
           !range_overlaps_population_owners(children,
                                             sizeof(*children),
                                             parents) &&
           !range_overlaps_population_owners(children,
                                             sizeof(*children),
                                             children) &&
           !range_overlaps_population_owners(evidence,
                                             evidence_size,
                                             parents) &&
           !range_overlaps_population_owners(evidence,
                                             evidence_size,
                                             children);
}

static bool population_owned_ranges_are_independent(
    const evo_population_t *parents,
    const evo_population_t *children)
{
    if (byte_ranges_overlap(parents->genomes,
                            parents->storage_bytes,
                            parents->evaluations,
                            parents->evaluation_bytes) ||
        byte_ranges_overlap(children->genomes,
                            children->storage_bytes,
                            children->evaluations,
                            children->evaluation_bytes) ||
        byte_ranges_overlap(parents->genomes,
                            parents->storage_bytes,
                            children->genomes,
                            children->storage_bytes) ||
        byte_ranges_overlap(parents->genomes,
                            parents->storage_bytes,
                            children->evaluations,
                            children->evaluation_bytes) ||
        byte_ranges_overlap(parents->evaluations,
                            parents->evaluation_bytes,
                            children->genomes,
                            children->storage_bytes) ||
        byte_ranges_overlap(parents->evaluations,
                            parents->evaluation_bytes,
                            children->evaluations,
                            children->evaluation_bytes)) {
        return false;
    }

    return true;
}

static bool population_matches_generation(
    const evo_population_t *population,
    uint64_t generation)
{
    if (population->initialized) {
        return generation == UINT64_C(0);
    }

    return generation != UINT64_C(0) &&
           population->source_generation == generation - UINT64_C(1);
}

evo_status_t evo_population_advance_generation(
    const evo_problem_t *problem,
    const evo_config_t *config,
    uint64_t current_generation,
    evo_population_t *parents,
    evo_population_t *evaluated_children,
    evo_generation_advancement_evidence_t *evidence)
{
    evo_generation_advancement_evidence_t candidate = {0};
    evo_population_t previous_parents = {0};
    size_t parent_valid_count = 0;
    size_t child_valid_count = 0;

    if (problem == NULL || config == NULL || parents == NULL ||
        evaluated_children == NULL || evidence == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!transition_objects_are_independent(parents,
                                            evaluated_children,
                                            evidence,
                                            sizeof(*evidence))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (current_generation == UINT64_MAX) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (problem->genome_size == 0 || config->population_size == 0 ||
        config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!population_owned_ranges_are_independent(parents,
                                                 evaluated_children) ||
        parents->genome_size != problem->genome_size ||
        evaluated_children->genome_size != problem->genome_size ||
        !evo_population_validate_completed(config,
                                           parents,
                                           &parent_valid_count) ||
        !evo_population_validate_completed(config,
                                           evaluated_children,
                                           &child_valid_count) ||
        !population_matches_generation(parents, current_generation) ||
        evaluated_children->initialized ||
        evaluated_children->source_generation != current_generation) {
        return EVO_ERROR_STATE;
    }

    candidate.population_size = evaluated_children->population_size;
    candidate.valid_count = child_valid_count;
    candidate.best_index = evaluated_children->best_index;
    candidate.elite_count = evaluated_children->elite_count;
    candidate.elite_source_valid_count =
        evaluated_children->elite_source_valid_count;
    candidate.previous_generation = current_generation;
    candidate.completed_generation = current_generation + UINT64_C(1);
    candidate.operator_seed_schedule_version =
        evaluated_children->operator_seed_schedule_version;
    candidate.selection_policy_version =
        evaluated_children->selection_policy_version;
    candidate.selection_policy = evaluated_children->selection_policy;
    candidate.byte_operator_policy_version =
        evaluated_children->byte_operator_policy_version;
    candidate.crossover_operator =
        evaluated_children->crossover_operator;
    candidate.mutation_operator = evaluated_children->mutation_operator;
    candidate.odd_child_policy_version =
        evaluated_children->odd_child_policy_version;
    candidate.elite_policy_version =
        evaluated_children->elite_policy_version;
    candidate.singleton_child_policy_version =
        evaluated_children->singleton_child_policy_version;
    candidate.fitness_comparison_policy_version =
        evaluated_children->fitness_comparison_policy_version;
    candidate.diversity_policy_version =
        evaluated_children->diversity_policy_version;
    candidate.diversity_metric_version =
        evaluated_children->diversity_metric_version;
    candidate.policy_version =
        EVO_GENERATION_ADVANCEMENT_POLICY_VERSION;
    candidate.has_best = evaluated_children->has_best;
    candidate.elite_count_explicit =
        evaluated_children->elite_count_explicit;
    candidate.complete = true;

    /*
     * No fallible operation remains. Transfer the child owners first, empty
     * the former child handle, and only then release the inaccessible prior
     * parent owners.
     */
    previous_parents = *parents;
    *parents = *evaluated_children;
    *evaluated_children = (evo_population_t){0};
    evo_population_destroy(&previous_parents);
    *evidence = candidate;
    return EVO_SUCCESS;
}
