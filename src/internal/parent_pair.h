#ifndef CATALYST_EVO_INTERNAL_PARENT_PAIR_H
#define CATALYST_EVO_INTERNAL_PARENT_PAIR_H

#include "internal/population_storage.h"
#include "internal/rng.h"

typedef struct evo_parent_pair {
    size_t parent_a_index;
    size_t parent_b_index;
    size_t child_a_index;
    size_t child_b_index;
    size_t pair_index;
    uint64_t source_generation;
    uint32_t seed_schedule_version;
    uint32_t selection_policy_version;
    evo_selection_policy_t selection_policy;
} evo_parent_pair_t;

/*
 * Plan one complete pair of child slots without writing child storage.
 *
 * The planner derives a selection-domain stream from the master seed, source
 * generation, and pair ordinal, then performs two configured policy draws.
 * Only the complete pairs in the resolved non-elite prefix are in scope. A
 * possible singleton and the stable elite suffix remain unassigned. Rejection
 * preserves the parent population and output object.
 */
evo_status_t evo_parent_pair_plan(
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    size_t pair_index,
    evo_parent_pair_t *pair);

#endif
