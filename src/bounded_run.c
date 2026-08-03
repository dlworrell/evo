#include "internal/bounded_run.h"

#include "internal/child_evaluation.h"
#include "internal/child_pair.h"
#include "internal/child_tail.h"
#include "internal/fitness.h"
#include "internal/observer.h"
#include "internal/statistics.h"

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

static bool transition_configuration_is_valid(
    const evo_problem_t *problem,
    const evo_config_t *config)
{
    size_t child_storage_bytes = 0;

    if (problem->genome_size == 0 || config->population_size == 0 ||
        config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        config->max_child_population_bytes == 0 ||
        !checked_size_multiply(config->population_size,
                               problem->genome_size,
                               &child_storage_bytes) ||
        child_storage_bytes > config->max_child_population_bytes) {
        return false;
    }

#if SIZE_MAX > UINT64_MAX
    if (config->generation_limit > (size_t)UINT64_MAX ||
        config->population_size - 1 > (size_t)UINT64_MAX) {
        return false;
    }
#endif

    if (config->population_size == 1) {
        return true;
    }

    return config->tournament_size != 0 &&
           config->tournament_size <= config->population_size &&
           isfinite(config->crossover_rate) &&
           config->crossover_rate >= 0.0 &&
           config->crossover_rate <= 1.0 &&
           isfinite(config->mutation_rate) &&
           config->mutation_rate >= 0.0 &&
           config->mutation_rate <= 1.0;
}

evo_status_t evo_bounded_run_validate_config(
    const evo_problem_t *problem,
    const evo_config_t *config)
{
    if (problem == NULL || config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (config->generation_limit == 0) {
        return EVO_SUCCESS;
    }

    if (!transition_configuration_is_valid(problem, config)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    return EVO_SUCCESS;
}

static bool fitness_equal(const evo_fitness_t *left,
                          const evo_fitness_t *right)
{
    return left->correctness == right->correctness &&
           left->performance == right->performance &&
           left->memory_use == right->memory_use &&
           left->reliability == right->reliability &&
           left->maintainability == right->maintainability &&
           left->constraint_penalty == right->constraint_penalty &&
           left->total == right->total;
}

static bool generation_statistics_equal(
    const evo_generation_statistics_t *left,
    const evo_generation_statistics_t *right)
{
    return left->version == right->version &&
           left->generation_index == right->generation_index &&
           left->population_size == right->population_size &&
           left->valid_count == right->valid_count &&
           left->invalid_count == right->invalid_count &&
           left->best_index == right->best_index &&
           fitness_equal(&left->best_fitness, &right->best_fitness) &&
           fitness_equal(&left->fitness_sums, &right->fitness_sums) &&
           left->has_best == right->has_best &&
           left->fitness_comparison_policy_version ==
               right->fitness_comparison_policy_version;
}

static bool bytes_equal(const void *left,
                        const void *right,
                        size_t size)
{
    const unsigned char *left_bytes = left;
    const unsigned char *right_bytes = right;

    for (size_t offset = 0; offset < size; ++offset) {
        if (left_bytes[offset] != right_bytes[offset]) {
            return false;
        }
    }

    return true;
}

static bool initial_run_state_is_valid(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    const evo_result_t *best_result,
    const evo_bounded_run_evidence_t *evidence)
{
    evo_generation_statistics_t expected_statistics = {0};
    const evo_candidate_evaluation_t *evaluation = NULL;
    const void *parent_best = NULL;
    size_t best_index = 0;
    size_t valid_count = 0;

    if (config->generation_limit == 0 ||
        best_result->best_genome == NULL ||
        best_result->generations_completed != 0 ||
        best_result->random_seed != config->random_seed ||
        best_result->termination_reason != EVO_TERMINATION_NONE ||
        byte_ranges_overlap(parents,
                            sizeof(*parents),
                            best_result,
                            sizeof(*best_result)) ||
        byte_ranges_overlap(parents,
                            sizeof(*parents),
                            evidence,
                            sizeof(*evidence)) ||
        byte_ranges_overlap(best_result,
                            sizeof(*best_result),
                            evidence,
                            sizeof(*evidence)) ||
        byte_ranges_overlap(parents,
                            sizeof(*parents),
                            best_result->best_genome,
                            problem->genome_size) ||
        byte_ranges_overlap(best_result,
                            sizeof(*best_result),
                            best_result->best_genome,
                            problem->genome_size) ||
        byte_ranges_overlap(evidence,
                            sizeof(*evidence),
                            best_result->best_genome,
                            problem->genome_size) ||
        byte_ranges_overlap(parents,
                            sizeof(*parents),
                            parents->genomes,
                            parents->storage_bytes) ||
        byte_ranges_overlap(parents,
                            sizeof(*parents),
                            parents->evaluations,
                            parents->evaluation_bytes) ||
        byte_ranges_overlap(parents->genomes,
                            parents->storage_bytes,
                            parents->evaluations,
                            parents->evaluation_bytes) ||
        byte_ranges_overlap(best_result,
                            sizeof(*best_result),
                            parents->genomes,
                            parents->storage_bytes) ||
        byte_ranges_overlap(best_result,
                            sizeof(*best_result),
                            parents->evaluations,
                            parents->evaluation_bytes) ||
        byte_ranges_overlap(best_result->best_genome,
                            problem->genome_size,
                            parents->genomes,
                            parents->storage_bytes) ||
        byte_ranges_overlap(best_result->best_genome,
                            problem->genome_size,
                            parents->evaluations,
                            parents->evaluation_bytes) ||
        byte_ranges_overlap(evidence,
                            sizeof(*evidence),
                            parents->genomes,
                            parents->storage_bytes) ||
        byte_ranges_overlap(evidence,
                            sizeof(*evidence),
                            parents->evaluations,
                            parents->evaluation_bytes) ||
        !parents->initialized || parents->source_generation != 0 ||
        parents->genome_size != problem->genome_size ||
        !evo_population_validate_completed(config,
                                           parents,
                                           &valid_count) ||
        valid_count == 0 || !parents->has_best ||
        !evo_population_best_index(parents, &best_index)) {
        return false;
    }

    if (evo_generation_statistics_record(parents,
                                         UINT64_C(0),
                                         &expected_statistics) !=
            EVO_SUCCESS ||
        !generation_statistics_equal(
            &best_result->generation_statistics,
            &expected_statistics)) {
        return false;
    }

    parent_best = evo_population_genome_const(parents, best_index);
    evaluation = evo_population_evaluation_const(parents, best_index);
    return parent_best != NULL && evaluation != NULL &&
           evaluation->valid && evaluation->evaluated &&
           fitness_equal(&best_result->best_fitness,
                         &evaluation->fitness) &&
           bytes_equal(best_result->best_genome,
                       parent_best,
                       problem->genome_size);
}

static void copy_genome(const void *source,
                        void *destination,
                        size_t genome_size)
{
    const unsigned char *source_bytes = source;
    unsigned char *destination_bytes = destination;

    for (size_t offset = 0; offset < genome_size; ++offset) {
        destination_bytes[offset] = source_bytes[offset];
    }
}

static evo_status_t produce_child_population(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    const evo_population_t *parents,
    uint64_t source_generation,
    evo_population_t *children)
{
    evo_child_pair_evidence_t pair_evidence = {0};
    evo_child_tail_evidence_t tail_evidence = {0};
    const size_t complete_pair_count = config->population_size / 2;
    evo_status_t status = EVO_SUCCESS;

    for (size_t pair_index = 0;
         pair_index < complete_pair_count;
         ++pair_index) {
        status = evo_child_pair_produce(problem,
                                        config,
                                        context,
                                        parents,
                                        source_generation,
                                        pair_index,
                                        children,
                                        &pair_evidence);
        if (status != EVO_SUCCESS) {
            return status;
        }
    }

    if (config->population_size % 2 != 0) {
        status = evo_child_tail_produce(problem,
                                        config,
                                        parents,
                                        source_generation,
                                        children,
                                        &tail_evidence);
    }

    return status;
}

static evo_status_t resolve_strict_improvement(
    const evo_population_t *children,
    const evo_result_t *best_result,
    uint64_t child_generation,
    uint64_t best_generation,
    size_t best_population_index,
    const void **genome,
    const evo_candidate_evaluation_t **evaluation,
    bool *has_improvement)
{
    evo_fitness_candidate_view_t candidate_view = {0};
    evo_fitness_candidate_view_t incumbent_view = {0};
    evo_fitness_order_t order = EVO_FITNESS_ORDER_EQUAL;
    size_t best_index = 0;

    if (children == NULL || best_result == NULL || genome == NULL ||
        evaluation == NULL || has_improvement == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    *genome = NULL;
    *evaluation = NULL;
    *has_improvement = false;

    if (!children->has_best || children->valid_count == 0) {
        return EVO_SUCCESS;
    }

    if (!evo_population_best_index(children, &best_index)) {
        return EVO_ERROR_STATE;
    }

    *genome = evo_population_genome_const(children, best_index);
    *evaluation = evo_population_evaluation_const(children, best_index);
    if (*genome == NULL || *evaluation == NULL) {
        return EVO_ERROR_STATE;
    }

    candidate_view = (evo_fitness_candidate_view_t){
        .fitness = &(*evaluation)->fitness,
        .generation = child_generation,
        .population_index = best_index,
        .hard_valid = (*evaluation)->valid,
        .evaluated = (*evaluation)->evaluated,
    };
    incumbent_view = (evo_fitness_candidate_view_t){
        .fitness = &best_result->best_fitness,
        .generation = best_generation,
        .population_index = best_population_index,
        .hard_valid = true,
        .evaluated = true,
    };
    if (!evo_fitness_compare_candidates(&candidate_view,
                                        &incumbent_view,
                                        &order)) {
        return EVO_ERROR_STATE;
    }

    *has_improvement = order == EVO_FITNESS_ORDER_LEFT;
    return EVO_SUCCESS;
}

evo_status_t evo_bounded_run_advance(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *parents,
    evo_result_t *best_result,
    evo_bounded_run_evidence_t *evidence)
{
    evo_bounded_run_evidence_t candidate = {0};
    evo_population_t children = {0};
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || parents == NULL ||
        best_result == NULL || evidence == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = evo_bounded_run_validate_config(problem, config);
    if (status != EVO_SUCCESS) {
        return status;
    }

    if (!initial_run_state_is_valid(problem,
                                    config,
                                    parents,
                                    best_result,
                                    evidence)) {
        return EVO_ERROR_STATE;
    }

    candidate.population_size = config->population_size;
    candidate.requested_transitions = config->generation_limit;
    candidate.best_generation = 0;
    candidate.best_population_index = parents->best_index;
    candidate.operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;
    candidate.fitness_comparison_policy_version =
        EVO_FITNESS_COMPARISON_POLICY_VERSION;
    candidate.child_evaluation_policy_version =
        EVO_CHILD_EVALUATION_POLICY_VERSION;
    candidate.generation_advancement_policy_version =
        EVO_GENERATION_ADVANCEMENT_POLICY_VERSION;
    candidate.policy_version = EVO_BOUNDED_RUN_POLICY_VERSION;

    for (size_t transition = 0;
         transition < config->generation_limit;
         ++transition) {
        evo_child_evaluation_evidence_t evaluation_evidence = {0};
        evo_generation_advancement_evidence_t advancement_evidence = {0};
        evo_generation_statistics_t generation_statistics = {0};
        const evo_candidate_evaluation_t *improved_evaluation = NULL;
        const void *improved_genome = NULL;
        const uint64_t source_generation = (uint64_t)transition;
        evo_termination_reason_t natural_reason =
            EVO_TERMINATION_NONE;
        evo_termination_reason_t termination_reason =
            EVO_TERMINATION_NONE;
        bool has_improvement = false;

        status = evo_child_population_create(problem,
                                             config,
                                             parents,
                                             &children);
        if (status != EVO_SUCCESS) {
            break;
        }

        status = produce_child_population(problem,
                                          config,
                                          context,
                                          parents,
                                          source_generation,
                                          &children);
        if (status != EVO_SUCCESS) {
            break;
        }

        status = evo_child_population_evaluate(problem,
                                               config,
                                               context,
                                               source_generation,
                                               &children,
                                               &evaluation_evidence);
        if (status != EVO_SUCCESS) {
            break;
        }

        status = evo_generation_statistics_record(
            &children,
            source_generation + UINT64_C(1),
            &generation_statistics);
        if (status != EVO_SUCCESS) {
            break;
        }

        status = resolve_strict_improvement(
            &children,
            best_result,
            source_generation + UINT64_C(1),
            candidate.best_generation,
            candidate.best_population_index,
            &improved_genome,
            &improved_evaluation,
            &has_improvement);
        if (status != EVO_SUCCESS) {
            break;
        }

        status = evo_population_advance_generation(problem,
                                                   config,
                                                   source_generation,
                                                   parents,
                                                   &children,
                                                   &advancement_evidence);
        if (status != EVO_SUCCESS) {
            break;
        }

        candidate.completed_transitions = transition + 1;
        candidate.final_generation =
            advancement_evidence.completed_generation;
        candidate.final_valid_count = advancement_evidence.valid_count;
        candidate.final_best_index = advancement_evidence.best_index;
        candidate.final_has_best = advancement_evidence.has_best;
        candidate.odd_child_policy_version =
            advancement_evidence.odd_child_policy_version;
        best_result->generations_completed =
            candidate.completed_transitions;
        best_result->generation_statistics = generation_statistics;

        if (has_improvement) {
            copy_genome(improved_genome,
                        best_result->best_genome,
                        problem->genome_size);
            best_result->best_fitness = improved_evaluation->fitness;
            candidate.best_generation = candidate.final_generation;
            candidate.best_population_index =
                candidate.final_best_index;
        }

        if (!candidate.final_has_best) {
            candidate.stopped_all_invalid = true;
        }

        if (candidate.stopped_all_invalid) {
            natural_reason = EVO_TERMINATION_ALL_INVALID;
        } else if (candidate.completed_transitions ==
                   candidate.requested_transitions) {
            natural_reason = EVO_TERMINATION_GENERATION_LIMIT;
        }
        termination_reason = evo_generation_callbacks_notify(problem,
                                                             config,
                                                             best_result,
                                                             natural_reason);
        if (termination_reason != EVO_TERMINATION_NONE) {
            candidate.termination_reason = termination_reason;
            candidate.stopped_application_requested =
                termination_reason ==
                EVO_TERMINATION_APPLICATION_REQUESTED;
        }

        if (termination_reason != EVO_TERMINATION_NONE) {
            break;
        }
    }

    evo_population_destroy(&children);
    if (status != EVO_SUCCESS) {
        return status;
    }

    candidate.complete = true;
    *evidence = candidate;
    return EVO_SUCCESS;
}
