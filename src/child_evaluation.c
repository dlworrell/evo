#include "internal/child_evaluation.h"

#include "internal/child_tail.h"
#include "internal/elite.h"
#include "internal/rng.h"

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

static bool produced_child_ready_for_evaluation(
    const evo_problem_t *problem,
    const evo_config_t *config,
    uint64_t source_generation,
    const evo_population_t *children)
{
    size_t expected_storage_bytes = 0;
    size_t expected_requested_elite_count = 0;
    size_t expected_elite_count = 0;
    size_t expected_offspring_count = 0;
    uint32_t expected_odd_child_policy_version = 0;
    uint32_t expected_singleton_child_policy_version = 0;

    if (children->genomes == NULL ||
        !evo_population_secure_erasure_is_valid(config, children) ||
        !evo_population_recycling_child_is_valid(config, children) ||
        children->population_size == 0 || children->genome_size == 0 ||
        children->storage_bytes == 0 ||
        children->population_size != config->population_size ||
        children->genome_size != problem->genome_size ||
        children->valid_count != 0 ||
        children->best_index != 0 || children->initialization_seed != 0 ||
        children->rng_algorithm_version != 0 ||
        children->produced_count != children->population_size ||
        children->source_generation != source_generation ||
        children->operator_seed_schedule_version !=
            EVO_OPERATOR_SEED_SCHEDULE_VERSION ||
        children->selection_policy_version !=
            EVO_SELECTION_POLICY_VERSION ||
        children->selection_policy != config->selection_policy ||
        children->byte_operator_policy_version !=
            EVO_BYTE_OPERATOR_POLICY_VERSION ||
        children->crossover_operator != config->crossover_operator ||
        children->mutation_operator != config->mutation_operator ||
        children->mutation_rate_used != config->mutation_rate ||
        children->fitness_comparison_policy_version != 0 ||
        children->diversity_policy_version != 0 ||
        children->diversity_metric_version != 0 ||
        children->diversity_pair_count != 0 ||
        children->diversity_work_units != 0 ||
        children->diversity != 0.0 ||
        children->diversity_uses_domain_distance ||
        children->initialized || children->has_best ||
        children->evaluated ||
        config->max_genome_bytes < children->genome_size ||
        config->max_child_population_bytes < children->storage_bytes ||
        !checked_size_multiply(children->population_size,
                               children->genome_size,
                               &expected_storage_bytes) ||
        expected_storage_bytes != children->storage_bytes) {
        return false;
    }

    if (evo_elite_policy_counts(
            config,
            children->elite_source_valid_count,
            &expected_requested_elite_count,
            &expected_elite_count,
            &expected_offspring_count) != EVO_SUCCESS) {
        return false;
    }
    (void)expected_requested_elite_count;

    if (!config->elite_count_enabled &&
        children->population_size % 2 != 0) {
        expected_odd_child_policy_version =
            EVO_ODD_CHILD_POLICY_VERSION;
    }
    if (expected_offspring_count % 2 != 0) {
        expected_singleton_child_policy_version =
            EVO_SINGLETON_CHILD_POLICY_VERSION;
    }

    return children->elite_count == expected_elite_count &&
           children->elite_policy_version ==
               EVO_ELITE_POLICY_VERSION &&
           children->singleton_child_policy_version ==
               expected_singleton_child_policy_version &&
           children->odd_child_policy_version ==
               expected_odd_child_policy_version &&
           children->elite_count_explicit ==
               config->elite_count_enabled;
}

evo_status_t evo_child_population_evaluate(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    uint64_t source_generation,
    evo_population_t *children,
    evo_child_evaluation_evidence_t *evidence)
{
    evo_child_evaluation_evidence_t candidate = {0};
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || children == NULL ||
        evidence == NULL || problem->evaluate == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (problem->genome_size == 0 || config->population_size == 0 ||
        config->max_genome_bytes == 0 ||
        problem->genome_size > config->max_genome_bytes ||
        config->max_child_population_bytes == 0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!produced_child_ready_for_evaluation(problem,
                                             config,
                                             source_generation,
                                             children)) {
        return EVO_ERROR_STATE;
    }

    status = evo_population_evaluate_ready(problem,
                                           config,
                                           context,
                                           children);
    if (status != EVO_SUCCESS) {
        return status;
    }

    candidate.population_size = children->population_size;
    candidate.evaluation_bytes = children->evaluation_bytes;
    candidate.valid_count = children->valid_count;
    candidate.best_index = children->best_index;
    candidate.elite_count = children->elite_count;
    candidate.elite_source_valid_count =
        children->elite_source_valid_count;
    candidate.source_generation = children->source_generation;
    candidate.operator_seed_schedule_version =
        children->operator_seed_schedule_version;
    candidate.selection_policy_version =
        children->selection_policy_version;
    candidate.selection_policy = children->selection_policy;
    candidate.byte_operator_policy_version =
        children->byte_operator_policy_version;
    candidate.crossover_operator = children->crossover_operator;
    candidate.mutation_operator = children->mutation_operator;
    candidate.mutation_rate_used = children->mutation_rate_used;
    candidate.odd_child_policy_version =
        children->odd_child_policy_version;
    candidate.elite_policy_version = children->elite_policy_version;
    candidate.singleton_child_policy_version =
        children->singleton_child_policy_version;
    candidate.fitness_comparison_policy_version =
        children->fitness_comparison_policy_version;
    candidate.diversity_policy_version =
        children->diversity_policy_version;
    candidate.diversity_metric_version =
        children->diversity_metric_version;
    candidate.population_recycling_policy_version =
        children->population_recycling_policy_version;
    candidate.storage_owner_identity =
        children->storage_owner_identity;
    candidate.policy_version = EVO_CHILD_EVALUATION_POLICY_VERSION;
    candidate.has_best = children->has_best;
    candidate.elite_count_explicit = children->elite_count_explicit;
    candidate.population_recycling_enabled =
        children->population_recycling_enabled;
    candidate.complete = true;
    *evidence = candidate;
    return EVO_SUCCESS;
}
