#include "internal/child_tail.h"

#include "internal/elite.h"

evo_status_t evo_child_tail_produce(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    evo_population_t *children,
    evo_child_tail_evidence_t *evidence)
{
    evo_child_tail_evidence_t candidate = {0};
    evo_elite_evidence_t elite_evidence = {0};
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || parents == NULL ||
        children == NULL || evidence == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (config->population_size == 0 ||
        config->population_size % 2 == 0 ||
        config->elite_count_enabled || config->elite_count != 0) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!evo_elite_completion_objects_are_independent(parents,
                                                      children,
                                                      evidence,
                                                      sizeof(*evidence))) {
        return EVO_ERROR_STATE;
    }

    status = evo_elite_population_complete(problem,
                                           config,
                                           parents,
                                           source_generation,
                                           children,
                                           &elite_evidence);
    if (status != EVO_SUCCESS) {
        return status;
    }

    candidate.parent_index = elite_evidence.best_parent_index;
    candidate.child_index = config->population_size - 1;
    candidate.produced_count = children->produced_count;
    candidate.source_generation = source_generation;
    candidate.operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;
    candidate.selection_policy_version =
        elite_evidence.selection_policy_version;
    candidate.selection_policy = elite_evidence.selection_policy;
    candidate.policy_version = EVO_ODD_CHILD_POLICY_VERSION;
    candidate.complete = true;
    *evidence = candidate;
    return EVO_SUCCESS;
}
