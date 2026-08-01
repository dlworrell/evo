#include "internal/child_evaluation.h"

#include "internal/child_tail.h"
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
    uint32_t expected_odd_child_policy_version = 0;

    if (children->genomes == NULL || children->evaluations != NULL ||
        children->population_size == 0 || children->genome_size == 0 ||
        children->storage_bytes == 0 ||
        children->population_size != config->population_size ||
        children->genome_size != problem->genome_size ||
        children->evaluation_bytes != 0 || children->valid_count != 0 ||
        children->best_index != 0 || children->initialization_seed != 0 ||
        children->rng_algorithm_version != 0 ||
        children->produced_count != children->population_size ||
        children->source_generation != source_generation ||
        children->operator_seed_schedule_version !=
            EVO_OPERATOR_SEED_SCHEDULE_VERSION ||
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

    if (children->population_size % 2 != 0) {
        expected_odd_child_policy_version =
            EVO_ODD_CHILD_POLICY_VERSION;
    }

    return children->odd_child_policy_version ==
           expected_odd_child_policy_version;
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
    candidate.source_generation = children->source_generation;
    candidate.operator_seed_schedule_version =
        children->operator_seed_schedule_version;
    candidate.odd_child_policy_version =
        children->odd_child_policy_version;
    candidate.policy_version = EVO_CHILD_EVALUATION_POLICY_VERSION;
    candidate.has_best = children->has_best;
    candidate.complete = true;
    *evidence = candidate;
    return EVO_SUCCESS;
}
