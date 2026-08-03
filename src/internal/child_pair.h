#ifndef CATALYST_EVO_INTERNAL_CHILD_PAIR_H
#define CATALYST_EVO_INTERNAL_CHILD_PAIR_H

#include "internal/parent_pair.h"

typedef struct evo_child_pair_evidence {
    evo_parent_pair_t plan;
    size_t produced_count;
    uint32_t rng_algorithm_version;
    bool complete;
} evo_child_pair_evidence_t;

/*
 * Produce the next complete pair in the non-elite prefix of an independently
 * owned child slab.
 *
 * Complete pairs are committed in ascending pair order. The operation plans
 * two parents, derives pair-local crossover and child-local mutation streams,
 * preflights every fallible library condition, then dispatches crossover and
 * both mutations. Parent state is always read-only. Rejection before callback
 * dispatch preserves child bytes, child metadata, and output evidence.
 *
 * Consumer callbacks have no failure channel. Once the preflight completes,
 * the dispatch suffix contains no expected library rejection and callback
 * side effects cannot be rolled back.
 */
evo_status_t evo_child_pair_produce(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    const evo_population_t *parents,
    uint64_t source_generation,
    size_t pair_index,
    evo_population_t *children,
    evo_child_pair_evidence_t *evidence);

#endif
