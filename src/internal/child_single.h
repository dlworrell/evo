#ifndef CATALYST_EVO_INTERNAL_CHILD_SINGLE_H
#define CATALYST_EVO_INTERNAL_CHILD_SINGLE_H

#include "internal/elite.h"
#include "internal/selection.h"

typedef struct evo_child_single_evidence {
    size_t parent_index;
    size_t child_index;
    size_t produced_count;
    size_t selection_stream_index;
    uint64_t source_generation;
    uint32_t rng_algorithm_version;
    uint32_t operator_seed_schedule_version;
    uint32_t selection_policy_version;
    evo_selection_policy_t selection_policy;
    uint32_t byte_operator_policy_version;
    evo_crossover_operator_t crossover_operator;
    evo_mutation_operator_t mutation_operator;
    uint32_t policy_version;
    bool complete;
} evo_child_single_evidence_t;

/*
 * Produce the one ordinary singleton required when the non-elite prefix is
 * odd. A dedicated selection stream chooses one valid parent, its bytes are
 * cloned, and the standard child-indexed mutation stream is applied. Every
 * library precondition is resolved before the copy or consumer callback.
 */
evo_status_t evo_child_single_produce(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    const evo_population_t *parents,
    uint64_t source_generation,
    evo_population_t *children,
    evo_child_single_evidence_t *evidence);

#endif
