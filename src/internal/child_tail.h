#ifndef CATALYST_EVO_INTERNAL_CHILD_TAIL_H
#define CATALYST_EVO_INTERNAL_CHILD_TAIL_H

#include "internal/child_pair.h"

#define EVO_ODD_CHILD_POLICY_VERSION UINT32_C(1)

typedef struct evo_child_tail_evidence {
    size_t parent_index;
    size_t child_index;
    size_t produced_count;
    uint64_t source_generation;
    uint32_t operator_seed_schedule_version;
    uint32_t policy_version;
    bool complete;
} evo_child_tail_evidence_t;

/*
 * Complete the one trailing slot of an odd child population.
 *
 * Policy version 1 clones the completed parent's stable best valid genome.
 * Every fallible condition and bounded view is resolved before the byte copy.
 * No RNG state is consumed and no consumer callback is invoked. Rejection
 * preserves the parent, child, and output evidence.
 */
evo_status_t evo_child_tail_produce(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    evo_population_t *children,
    evo_child_tail_evidence_t *evidence);

#endif
