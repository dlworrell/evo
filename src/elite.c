#include "internal/elite.h"

#include "internal/child_tail.h"
#include "internal/crossover.h"
#include "internal/fitness.h"
#include "internal/mutation.h"
#include "internal/rng.h"
#include "internal/selection.h"

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

bool evo_elite_completion_objects_are_independent(
    const evo_population_t *parents,
    const evo_population_t *children,
    const void *evidence,
    size_t evidence_size)
{
    if (parents == NULL || children == NULL || evidence == NULL ||
        evidence_size == 0) {
        return false;
    }

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
                                evidence_size,
                                parents->genomes,
                                parents->storage_bytes) &&
           !byte_ranges_overlap(evidence,
                                evidence_size,
                                parents->evaluations,
                                parents->evaluation_bytes) &&
           !byte_ranges_overlap(evidence,
                                evidence_size,
                                children->genomes,
                                children->storage_bytes);
}

evo_status_t evo_elite_validate_config(const evo_config_t *config)
{
    if (config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (config->population_size == 0 ||
        (!config->elite_count_enabled && config->elite_count != 0) ||
        (config->elite_count_enabled &&
         config->elite_count > config->population_size)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    return EVO_SUCCESS;
}

evo_status_t evo_elite_policy_counts(const evo_config_t *config,
                                     size_t source_valid_count,
                                     size_t *requested_count,
                                     size_t *effective_count,
                                     size_t *offspring_count)
{
    size_t requested = 0;
    size_t effective = 0;
    evo_status_t status = EVO_SUCCESS;

    if (requested_count == NULL || effective_count == NULL ||
        offspring_count == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = evo_elite_validate_config(config);
    if (status != EVO_SUCCESS) {
        return status;
    }

    if (source_valid_count == 0) {
        return EVO_ERROR_NO_VALID_CANDIDATE;
    }
    if (source_valid_count > config->population_size) {
        return EVO_ERROR_STATE;
    }

    requested = config->elite_count_enabled
                    ? config->elite_count
                    : config->population_size % 2;
    effective = requested < source_valid_count
                    ? requested
                    : source_valid_count;

    *requested_count = requested;
    *effective_count = effective;
    *offspring_count = config->population_size - effective;
    return EVO_SUCCESS;
}

static bool candidate_view(const evo_population_t *parents,
                           size_t index,
                           evo_fitness_candidate_view_t *view)
{
    const evo_candidate_evaluation_t *evaluation = NULL;

    if (parents == NULL || view == NULL ||
        index >= parents->population_size) {
        return false;
    }

    evaluation = evo_population_evaluation_const(parents, index);
    if (evaluation == NULL) {
        return false;
    }

    *view = (evo_fitness_candidate_view_t){
        .fitness = &evaluation->fitness,
        .generation = UINT64_C(0),
        .population_index = index,
        .hard_valid = evaluation->valid,
        .evaluated = evaluation->evaluated,
    };
    return evo_fitness_candidate_is_rankable(view);
}

static bool next_ranked_parent(const evo_population_t *parents,
                               bool has_previous,
                               size_t previous_index,
                               size_t *next_index)
{
    evo_fitness_candidate_view_t previous_view = {0};
    evo_fitness_candidate_view_t winner_view = {0};
    size_t winner_index = 0;
    bool has_winner = false;

    if (parents == NULL || next_index == NULL ||
        (has_previous &&
         !candidate_view(parents, previous_index, &previous_view))) {
        return false;
    }

    for (size_t index = 0; index < parents->population_size; ++index) {
        evo_fitness_candidate_view_t candidate = {0};
        evo_fitness_order_t order = EVO_FITNESS_ORDER_EQUAL;

        if (!parents->evaluations[index].valid) {
            continue;
        }
        if (!candidate_view(parents, index, &candidate)) {
            return false;
        }

        if (has_previous) {
            if (!evo_fitness_compare_candidates(&candidate,
                                                &previous_view,
                                                &order)) {
                return false;
            }
            if (order != EVO_FITNESS_ORDER_RIGHT) {
                continue;
            }
        }

        if (!has_winner) {
            winner_index = index;
            winner_view = candidate;
            has_winner = true;
            continue;
        }

        if (!evo_fitness_compare_candidates(&candidate,
                                            &winner_view,
                                            &order)) {
            return false;
        }
        if (order == EVO_FITNESS_ORDER_LEFT) {
            winner_index = index;
            winner_view = candidate;
        }
    }

    if (!has_winner) {
        return false;
    }

    *next_index = winner_index;
    return true;
}

static bool ranked_elites_are_resolvable(
    const evo_population_t *parents,
    evo_population_t *children,
    size_t offspring_count,
    size_t effective_count,
    size_t *best_parent_index,
    size_t *worst_elite_parent_index)
{
    size_t previous_index = 0;
    bool has_previous = false;

    if (best_parent_index == NULL || worst_elite_parent_index == NULL) {
        return false;
    }

    for (size_t rank = 0; rank < effective_count; ++rank) {
        size_t parent_index = 0;

        if (!next_ranked_parent(parents,
                                has_previous,
                                previous_index,
                                &parent_index) ||
            evo_population_genome_const(parents, parent_index) == NULL ||
            evo_population_genome(children,
                                  offspring_count + rank) == NULL) {
            return false;
        }
        if (rank == 0) {
            *best_parent_index = parent_index;
        }
        previous_index = parent_index;
        has_previous = true;
    }

    if (has_previous) {
        *worst_elite_parent_index = previous_index;
    }
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

static void clone_ranked_elites(const evo_population_t *parents,
                                evo_population_t *children,
                                size_t offspring_count,
                                size_t effective_count)
{
    size_t previous_index = 0;
    bool has_previous = false;

    for (size_t rank = 0; rank < effective_count; ++rank) {
        const void *parent = NULL;
        void *child = NULL;
        size_t parent_index = 0;
        bool resolved = false;

        resolved = next_ranked_parent(parents,
                                      has_previous,
                                      previous_index,
                                      &parent_index);
        (void)resolved;

        parent = evo_population_genome_const(parents, parent_index);
        child = evo_population_genome(children, offspring_count + rank);

        clone_genome(parent, child, children->genome_size);
        previous_index = parent_index;
        has_previous = true;
    }
}

evo_status_t evo_elite_population_complete(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    evo_population_t *children,
    evo_elite_evidence_t *evidence)
{
    evo_elite_evidence_t candidate = {0};
    size_t expected_storage_bytes = 0;
    size_t valid_count = 0;
    size_t requested_count = 0;
    size_t effective_count = 0;
    size_t offspring_count = 0;
    uint32_t expected_singleton_policy_version = 0;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || parents == NULL ||
        children == NULL || evidence == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = evo_elite_validate_config(config);
    if (status != EVO_SUCCESS) {
        return status;
    }
    status = evo_selection_validate_config(config);
    if (status != EVO_SUCCESS) {
        return status;
    }
    if (!evo_crossover_operator_is_valid(config->crossover_operator) ||
        !evo_mutation_operator_is_valid(config->mutation_operator) ||
        problem->genome_size == 0 || config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        config->max_child_population_bytes == 0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!evo_elite_completion_objects_are_independent(parents,
                                                      children,
                                                      evidence,
                                                      sizeof(*evidence))) {
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

    if (offspring_count % 2 != 0) {
        expected_singleton_policy_version =
            EVO_SINGLETON_CHILD_POLICY_VERSION;
    }

    if (children->genomes == NULL || children->evaluations != NULL ||
        children->population_size != config->population_size ||
        children->population_size != parents->population_size ||
        children->genome_size != problem->genome_size ||
        children->genome_size != parents->genome_size ||
        children->evaluation_bytes != 0 || children->valid_count != 0 ||
        children->best_index != 0 ||
        children->produced_count != offspring_count ||
        children->elite_count != 0 ||
        children->elite_source_valid_count != 0 ||
        children->initialization_seed != 0 ||
        children->rng_algorithm_version != 0 ||
        (children->selection_policy_version == 0 &&
         children->selection_policy != EVO_SELECTION_TOURNAMENT) ||
        (children->byte_operator_policy_version == 0 &&
         (children->crossover_operator != EVO_CROSSOVER_CONSUMER ||
          children->mutation_operator != EVO_MUTATION_CONSUMER)) ||
        (children->byte_operator_policy_version == 0 &&
         children->mutation_rate_used != 0.0) ||
        children->odd_child_policy_version != 0 ||
        children->elite_policy_version != 0 ||
        children->singleton_child_policy_version !=
            expected_singleton_policy_version ||
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
        return EVO_ERROR_STATE;
    }

    if ((offspring_count == 0 &&
         (children->source_generation != 0 ||
          children->operator_seed_schedule_version != 0 ||
          children->selection_policy_version != 0 ||
          children->selection_policy != EVO_SELECTION_TOURNAMENT ||
          children->byte_operator_policy_version != 0 ||
          children->crossover_operator != EVO_CROSSOVER_CONSUMER ||
          children->mutation_operator != EVO_MUTATION_CONSUMER ||
          children->mutation_rate_used != 0.0)) ||
        (offspring_count != 0 &&
         (children->source_generation != source_generation ||
          children->operator_seed_schedule_version !=
              EVO_OPERATOR_SEED_SCHEDULE_VERSION ||
          children->selection_policy_version !=
              EVO_SELECTION_POLICY_VERSION ||
          children->selection_policy != config->selection_policy ||
          children->byte_operator_policy_version !=
              EVO_BYTE_OPERATOR_POLICY_VERSION ||
          children->crossover_operator != config->crossover_operator ||
          children->mutation_operator != config->mutation_operator ||
          children->mutation_rate_used != config->mutation_rate))) {
        return EVO_ERROR_STATE;
    }

    if (!ranked_elites_are_resolvable(parents,
                                      children,
                                      offspring_count,
                                      effective_count,
                                      &candidate.best_parent_index,
                                      &candidate.worst_elite_parent_index)) {
        return EVO_ERROR_STATE;
    }

    /* The dry ranking pass above leaves no fallible copy condition. */
    clone_ranked_elites(parents,
                        children,
                        offspring_count,
                        effective_count);

    children->produced_count = children->population_size;
    children->elite_count = effective_count;
    children->elite_source_valid_count = valid_count;
    children->source_generation = source_generation;
    children->operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;
    children->selection_policy_version =
        EVO_SELECTION_POLICY_VERSION;
    children->selection_policy = config->selection_policy;
    children->byte_operator_policy_version =
        EVO_BYTE_OPERATOR_POLICY_VERSION;
    children->crossover_operator = config->crossover_operator;
    children->mutation_operator = config->mutation_operator;
    children->mutation_rate_used = config->mutation_rate;
    children->odd_child_policy_version =
        !config->elite_count_enabled &&
                config->population_size % 2 != 0
            ? EVO_ODD_CHILD_POLICY_VERSION
            : 0;
    children->elite_policy_version = EVO_ELITE_POLICY_VERSION;
    children->elite_count_explicit = config->elite_count_enabled;

    candidate.requested_count = requested_count;
    candidate.effective_count = effective_count;
    candidate.source_valid_count = valid_count;
    candidate.offspring_count = offspring_count;
    candidate.source_generation = source_generation;
    candidate.operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;
    candidate.selection_policy_version =
        EVO_SELECTION_POLICY_VERSION;
    candidate.selection_policy = config->selection_policy;
    candidate.byte_operator_policy_version =
        EVO_BYTE_OPERATOR_POLICY_VERSION;
    candidate.crossover_operator = config->crossover_operator;
    candidate.mutation_operator = config->mutation_operator;
    candidate.mutation_rate_used = config->mutation_rate;
    candidate.odd_child_policy_version =
        children->odd_child_policy_version;
    candidate.elite_policy_version = EVO_ELITE_POLICY_VERSION;
    candidate.singleton_child_policy_version =
        children->singleton_child_policy_version;
    candidate.elite_count_explicit = config->elite_count_enabled;
    candidate.complete = true;
    *evidence = candidate;
    return EVO_SUCCESS;
}
