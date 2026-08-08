#include "internal/bounded_run.h"

#include "internal/adaptive_mutation.h"
#include "internal/child_evaluation.h"
#include "internal/child_pair.h"
#include "internal/child_single.h"
#include "internal/checkpoint.h"
#include "internal/crossover.h"
#include "internal/diversity.h"
#include "internal/elite.h"
#include "internal/fitness.h"
#include "internal/observer.h"
#include "internal/mutation.h"
#include "internal/selection.h"
#include "internal/secure_erasure.h"
#include "internal/statistics.h"
#include "internal/stopping.h"

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
    bool singleton_operator_policy_is_valid = false;

    if (evo_elite_validate_config(config) != EVO_SUCCESS ||
        evo_selection_validate_config(config) != EVO_SUCCESS ||
        !evo_crossover_operator_is_valid(config->crossover_operator) ||
        !evo_mutation_operator_is_valid(config->mutation_operator) ||
        problem->genome_size == 0 || config->population_size == 0 ||
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

    singleton_operator_policy_is_valid =
        evo_selection_validate_active_config(config) == EVO_SUCCESS &&
        evo_mutation_validate_config(problem, config) == EVO_SUCCESS;

    if (config->population_size == 1) {
        if (!config->elite_count_enabled || config->elite_count == 1) {
            return true;
        }
        return singleton_operator_policy_is_valid;
    }

    return singleton_operator_policy_is_valid &&
           evo_crossover_validate_config(problem, config) == EVO_SUCCESS;
}

evo_status_t evo_bounded_run_validate_config(
    const evo_problem_t *problem,
    const evo_config_t *config)
{
    if (problem == NULL || config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    {
        const evo_status_t stopping_status =
            evo_stopping_validate_config(config);

        if (stopping_status != EVO_SUCCESS) {
            return stopping_status;
        }
    }

    {
        const evo_status_t diversity_status =
            evo_diversity_validate_config(problem, config);

        if (diversity_status != EVO_SUCCESS) {
            return diversity_status;
        }
    }

    if (config->generation_limit == 0) {
        return EVO_SUCCESS;
    }

    if (evo_adaptive_mutation_is_applicable(config)) {
        const evo_status_t adaptive_mutation_status =
            evo_adaptive_mutation_validate_config(config);

        if (adaptive_mutation_status != EVO_SUCCESS) {
            return adaptive_mutation_status;
        }
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
               right->fitness_comparison_policy_version &&
           left->diversity_policy_version ==
               right->diversity_policy_version &&
           left->diversity_metric_version ==
               right->diversity_metric_version &&
           left->diversity_pair_count == right->diversity_pair_count &&
           left->diversity_work_units == right->diversity_work_units &&
           left->diversity == right->diversity &&
           left->diversity_uses_domain_distance ==
               right->diversity_uses_domain_distance &&
           left->adaptive_mutation_policy_version ==
               right->adaptive_mutation_policy_version &&
           left->mutation_rate_prior == right->mutation_rate_prior &&
           left->mutation_rate_effective ==
               right->mutation_rate_effective &&
           left->adaptive_mutation_min_rate ==
               right->adaptive_mutation_min_rate &&
           left->adaptive_mutation_max_rate ==
               right->adaptive_mutation_max_rate &&
           left->adaptive_mutation_step ==
               right->adaptive_mutation_step &&
           left->adaptive_mutation_diversity_threshold ==
               right->adaptive_mutation_diversity_threshold &&
           left->adaptive_mutation_stagnant_generations ==
               right->adaptive_mutation_stagnant_generations &&
           left->mutation_adaptation_reason ==
               right->mutation_adaptation_reason &&
           left->adaptive_mutation_enabled ==
               right->adaptive_mutation_enabled &&
           left->adaptive_mutation_low_diversity ==
               right->adaptive_mutation_low_diversity &&
           left->adaptive_mutation_global_best_improved ==
               right->adaptive_mutation_global_best_improved &&
           left->adaptive_mutation_clamped_to_min ==
               right->adaptive_mutation_clamped_to_min &&
           left->adaptive_mutation_clamped_to_max ==
               right->adaptive_mutation_clamped_to_max &&
           left->adaptive_mutation_reset_on_improvement ==
               right->adaptive_mutation_reset_on_improvement;
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
    evo_adaptive_mutation_state_t expected_adaptive_state = {0};
    evo_generation_statistics_t expected_statistics = {0};
    const evo_candidate_evaluation_t *evaluation = NULL;
    const void *parent_best = NULL;
    size_t best_index = 0;
    size_t valid_count = 0;

    if (config->generation_limit == 0 ||
        best_result->best_genome == NULL ||
        best_result->best_genome_size != problem->genome_size ||
        best_result->secure_erasure_enabled !=
            config->secure_erasure_enabled ||
        !evo_secure_erasure_metadata_is_valid(
            best_result->secure_erasure_enabled,
            best_result->secure_erasure_policy_version,
            best_result->secure_erasure_backend) ||
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

    if (evo_generation_statistics_record(config,
                                         parents,
                                         UINT64_C(0),
                                         &expected_statistics) !=
        EVO_SUCCESS) {
        return false;
    }
    if (evo_adaptive_mutation_is_applicable(config) &&
        evo_adaptive_mutation_initialize(config,
                                         &expected_statistics,
                                         &expected_adaptive_state) !=
            EVO_SUCCESS) {
        return false;
    }
    if (
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
    evo_child_single_evidence_t single_evidence = {0};
    evo_elite_evidence_t elite_evidence = {0};
    size_t requested_elite_count = 0;
    size_t effective_elite_count = 0;
    size_t offspring_count = 0;
    size_t complete_pair_count = 0;
    evo_status_t status = EVO_SUCCESS;

    status = evo_elite_policy_counts(config,
                                     parents->valid_count,
                                     &requested_elite_count,
                                     &effective_elite_count,
                                     &offspring_count);
    if (status != EVO_SUCCESS) {
        return status;
    }
    (void)requested_elite_count;
    (void)effective_elite_count;
    complete_pair_count = offspring_count / 2;

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

    if (offspring_count % 2 != 0) {
        status = evo_child_single_produce(problem,
                                          config,
                                          context,
                                          parents,
                                          source_generation,
                                          children,
                                          &single_evidence);
        if (status != EVO_SUCCESS) {
            return status;
        }
    }

    return evo_elite_population_complete(problem,
                                         config,
                                         parents,
                                         source_generation,
                                         children,
                                         &elite_evidence);
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

evo_status_t evo_run_state_initialize(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *population,
    const evo_result_t *result,
    evo_run_state_t *state)
{
    evo_bounded_run_evidence_t validation_evidence = {0};
    evo_generation_statistics_t expected_statistics = {0};
    const evo_candidate_evaluation_t *evaluation = NULL;
    const void *genome = NULL;
    evo_run_state_t candidate = {0};
    size_t best_index = 0;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || population == NULL ||
        result == NULL || state == NULL || state->initialized ||
        byte_ranges_overlap(population,
                            sizeof(*population),
                            state,
                            sizeof(*state)) ||
        byte_ranges_overlap(result,
                            sizeof(*result),
                            state,
                            sizeof(*state))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (config->generation_limit != 0) {
        if (!initial_run_state_is_valid(problem,
                                        config,
                                        population,
                                        result,
                                        &validation_evidence)) {
            return EVO_ERROR_STATE;
        }
    } else if (!population->initialized ||
               population->source_generation != 0 ||
               population->genomes == NULL ||
               population->evaluations == NULL ||
               population->population_size != config->population_size ||
               population->genome_size != problem->genome_size ||
               population->valid_count == 0 || !population->has_best ||
               !evo_population_best_index(population, &best_index) ||
               result->best_genome == NULL ||
               result->best_genome_size != problem->genome_size ||
               result->generations_completed != 0 ||
               result->random_seed != config->random_seed ||
               result->termination_reason != EVO_TERMINATION_NONE ||
               evo_generation_statistics_record(config,
                                                population,
                                                UINT64_C(0),
                                                &expected_statistics) !=
                   EVO_SUCCESS ||
               !generation_statistics_equal(&expected_statistics,
                                            &result->generation_statistics)) {
        return EVO_ERROR_STATE;
    } else {
        genome = evo_population_genome_const(population, best_index);
        evaluation = evo_population_evaluation_const(population,
                                                     best_index);
        if (genome == NULL || evaluation == NULL ||
            !fitness_equal(&evaluation->fitness,
                           &result->best_fitness) ||
            !bytes_equal(genome,
                         result->best_genome,
                         problem->genome_size)) {
            return EVO_ERROR_STATE;
        }
    }

    candidate.version = EVO_RUN_STATE_VERSION;
    candidate.best_population_index = population->best_index;
    candidate.adaptive_mutation_applicable =
        evo_adaptive_mutation_is_applicable(config);
    if (candidate.adaptive_mutation_applicable) {
        status = evo_adaptive_mutation_restore_initial(
            config,
            &result->generation_statistics,
            &candidate.adaptive_mutation);
        if (status != EVO_SUCCESS) {
            return EVO_ERROR_STATE;
        }
    }
    status = evo_stopping_state_initialize(config,
                                           result,
                                           &candidate.stopping);
    if (status != EVO_SUCCESS) {
        return EVO_ERROR_STATE;
    }
    candidate.initialized = true;
    *state = candidate;
    return EVO_SUCCESS;
}

static bool continuation_state_is_valid(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    const evo_result_t *best_result,
    const evo_run_state_t *state)
{
    size_t valid_count = 0;

    return state->version == EVO_RUN_STATE_VERSION && state->initialized &&
           state->termination_reason == EVO_TERMINATION_NONE &&
           state->current_generation < config->generation_limit &&
           state->current_generation == best_result->generations_completed &&
           state->current_generation ==
               best_result->generation_statistics.generation_index &&
           state->best_generation <= state->current_generation &&
           state->best_population_index < config->population_size &&
           state->adaptive_mutation_applicable ==
               evo_adaptive_mutation_is_applicable(config) &&
           state->stopping.initialized &&
           isfinite(state->stopping.significant_best_total) &&
           best_result->best_genome != NULL &&
           best_result->best_genome_size == problem->genome_size &&
           best_result->random_seed == config->random_seed &&
           best_result->termination_reason == EVO_TERMINATION_NONE &&
           evo_fitness_evidence_is_valid(&best_result->best_fitness) &&
           ((state->current_generation == 0 && parents->initialized &&
             parents->source_generation == 0) ||
            (state->current_generation != 0 && !parents->initialized &&
             parents->source_generation == state->current_generation - 1)) &&
           evo_population_validate_completed(config,
                                             parents,
                                             &valid_count) &&
           valid_count == parents->valid_count && parents->has_best;
}

evo_status_t evo_bounded_run_continue(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *parents,
    evo_result_t *best_result,
    evo_run_state_t *state,
    evo_bounded_run_evidence_t *evidence)
{
    evo_bounded_run_evidence_t candidate = {0};
    evo_population_t children = {0};
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || parents == NULL ||
        best_result == NULL || state == NULL || evidence == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    status = evo_bounded_run_validate_config(problem, config);
    if (status != EVO_SUCCESS) {
        return status;
    }
    if (!continuation_state_is_valid(problem,
                                     config,
                                     parents,
                                     best_result,
                                     state)) {
        return EVO_ERROR_STATE;
    }

    candidate.population_size = config->population_size;
    candidate.requested_transitions = config->generation_limit;
    candidate.completed_transitions =
        (size_t)state->current_generation;
    candidate.final_generation = state->current_generation;
    candidate.final_valid_count = parents->valid_count;
    candidate.final_best_index = parents->best_index;
    candidate.final_has_best = parents->has_best;
    candidate.best_generation = state->best_generation;
    candidate.best_population_index = state->best_population_index;
    candidate.operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;
    candidate.selection_policy_version =
        EVO_SELECTION_POLICY_VERSION;
    candidate.selection_policy = config->selection_policy;
    candidate.byte_operator_policy_version =
        EVO_BYTE_OPERATOR_POLICY_VERSION;
    candidate.crossover_operator = config->crossover_operator;
    candidate.mutation_operator = config->mutation_operator;
    candidate.adaptive_mutation_policy_version =
        best_result->generation_statistics
            .adaptive_mutation_policy_version;
    candidate.effective_mutation_rate =
        best_result->generation_statistics.mutation_rate_effective;
    candidate.adaptive_mutation_stagnant_generations =
        best_result->generation_statistics
            .adaptive_mutation_stagnant_generations;
    candidate.mutation_adaptation_reason =
        best_result->generation_statistics.mutation_adaptation_reason;
    candidate.elite_policy_version = EVO_ELITE_POLICY_VERSION;
    candidate.fitness_comparison_policy_version =
        EVO_FITNESS_COMPARISON_POLICY_VERSION;
    candidate.child_evaluation_policy_version =
        EVO_CHILD_EVALUATION_POLICY_VERSION;
    candidate.generation_advancement_policy_version =
        EVO_GENERATION_ADVANCEMENT_POLICY_VERSION;
    candidate.diversity_policy_version = EVO_DIVERSITY_POLICY_VERSION;
    candidate.diversity_metric_version =
        parents->diversity_metric_version;
    candidate.stopping_policy_version = EVO_STOPPING_POLICY_VERSION;
    candidate.policy_version = EVO_BOUNDED_RUN_POLICY_VERSION;

    candidate.significant_best_total =
        state->stopping.significant_best_total;
    candidate.stagnant_generations =
        state->stopping.stagnant_generations;

    for (size_t transition = (size_t)state->current_generation;
         transition < config->generation_limit;
         ++transition) {
        evo_child_evaluation_evidence_t evaluation_evidence = {0};
        evo_generation_advancement_evidence_t advancement_evidence = {0};
        evo_generation_statistics_t generation_statistics = {0};
        evo_config_t transition_config = *config;
        const evo_candidate_evaluation_t *improved_evaluation = NULL;
        const void *improved_genome = NULL;
        const uint64_t source_generation = (uint64_t)transition;
        evo_termination_reason_t natural_reason =
            EVO_TERMINATION_NONE;
        evo_termination_reason_t termination_reason =
            EVO_TERMINATION_NONE;
        bool has_improvement = false;

        transition_config.mutation_rate =
            state->adaptive_mutation_applicable
                ? state->adaptive_mutation.effective_rate
                : 0.0;

        status = evo_child_population_create(problem,
                                             &transition_config,
                                             parents,
                                             &children);
        if (status != EVO_SUCCESS) {
            break;
        }

        status = produce_child_population(problem,
                                          &transition_config,
                                          context,
                                          parents,
                                          source_generation,
                                          &children);
        if (status != EVO_SUCCESS) {
            break;
        }

        status = evo_child_population_evaluate(problem,
                                               &transition_config,
                                               context,
                                               source_generation,
                                               &children,
                                               &evaluation_evidence);
        if (status != EVO_SUCCESS) {
            break;
        }

        status = evo_generation_statistics_record(
            &transition_config,
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
                                                   &transition_config,
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
        candidate.final_elite_count = advancement_evidence.elite_count;
        candidate.final_elite_source_valid_count =
            advancement_evidence.elite_source_valid_count;
        candidate.final_has_best = advancement_evidence.has_best;
        candidate.odd_child_policy_version =
            advancement_evidence.odd_child_policy_version;
        candidate.selection_policy_version =
            advancement_evidence.selection_policy_version;
        candidate.selection_policy =
            advancement_evidence.selection_policy;
        candidate.byte_operator_policy_version =
            advancement_evidence.byte_operator_policy_version;
        candidate.crossover_operator =
            advancement_evidence.crossover_operator;
        candidate.mutation_operator =
            advancement_evidence.mutation_operator;
        candidate.final_mutation_rate_used =
            advancement_evidence.mutation_rate_used;
        candidate.elite_policy_version =
            advancement_evidence.elite_policy_version;
        candidate.singleton_child_policy_version =
            advancement_evidence.singleton_child_policy_version;
        candidate.elite_count_explicit =
            advancement_evidence.elite_count_explicit;
        best_result->generations_completed =
            candidate.completed_transitions;
        state->current_generation = candidate.final_generation;

        if (has_improvement) {
            copy_genome(improved_genome,
                        best_result->best_genome,
                        problem->genome_size);
            best_result->best_fitness = improved_evaluation->fitness;
            candidate.best_generation = candidate.final_generation;
            candidate.best_population_index =
                candidate.final_best_index;
            state->best_generation = candidate.best_generation;
            state->best_population_index =
                candidate.best_population_index;
        }

        if (state->adaptive_mutation_applicable) {
            status = evo_adaptive_mutation_commit(
                config,
                has_improvement,
                &generation_statistics,
                &state->adaptive_mutation);
        }
        if (status != EVO_SUCCESS ||
            advancement_evidence.mutation_rate_used !=
                transition_config.mutation_rate ||
            !evo_adaptive_mutation_statistics_are_valid(
                config,
                &generation_statistics)) {
            status = EVO_ERROR_STATE;
            break;
        }
        best_result->generation_statistics = generation_statistics;
        candidate.adaptive_mutation_policy_version =
            generation_statistics.adaptive_mutation_policy_version;
        candidate.effective_mutation_rate =
            generation_statistics.mutation_rate_effective;
        candidate.adaptive_mutation_stagnant_generations =
            generation_statistics.adaptive_mutation_stagnant_generations;
        candidate.mutation_adaptation_reason =
            generation_statistics.mutation_adaptation_reason;

        status = evo_stopping_classify_committed(
            config,
            best_result,
            !candidate.final_has_best,
            candidate.completed_transitions == candidate.requested_transitions,
            &state->stopping,
            &natural_reason);
        if (status != EVO_SUCCESS) {
            status = EVO_ERROR_STATE;
            break;
        }
        candidate.significant_best_total =
            state->stopping.significant_best_total;
        candidate.stagnant_generations =
            state->stopping.stagnant_generations;
        termination_reason = evo_generation_callbacks_notify(problem,
                                                             config,
                                                             best_result,
                                                             natural_reason);
        state->termination_reason = termination_reason;
        if (termination_reason != EVO_TERMINATION_NONE) {
            candidate.termination_reason = termination_reason;
            candidate.stopped_all_invalid =
                termination_reason == EVO_TERMINATION_ALL_INVALID;
            candidate.stopped_converged =
                termination_reason == EVO_TERMINATION_CONVERGED;
            candidate.stopped_stagnated =
                termination_reason == EVO_TERMINATION_STAGNATED;
            candidate.stopped_application_requested =
                termination_reason ==
                EVO_TERMINATION_APPLICATION_REQUESTED;
        }

        status = evo_checkpoint_emit(problem,
                                     config,
                                     parents,
                                     best_result,
                                     state);
        if (status != EVO_SUCCESS) {
            break;
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

evo_status_t evo_bounded_run_advance(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *parents,
    evo_result_t *best_result,
    evo_bounded_run_evidence_t *evidence)
{
    evo_run_state_t state = {0};
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || parents == NULL ||
        best_result == NULL || evidence == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    status = evo_run_state_initialize(problem,
                                      config,
                                      parents,
                                      best_result,
                                      &state);
    if (status != EVO_SUCCESS) {
        return status;
    }
    return evo_bounded_run_continue(problem,
                                    config,
                                    context,
                                    parents,
                                    best_result,
                                    &state,
                                    evidence);
}
