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
} evo_parent_pair_t;

/*
 * Plan one complete pair of child slots without writing child storage.
 *
 * The planner derives a selection-domain stream from the master seed, source
 * generation, and pair ordinal, then performs two tournaments with
 * replacement. Only floor(population_size / 2) complete pairs are in scope;
 * an odd trailing child remains unassigned. Rejection preserves the parent
 * population and output object.
 */
evo_status_t evo_parent_pair_plan(
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    size_t pair_index,
    evo_parent_pair_t *pair);

#endif
