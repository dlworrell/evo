#ifndef CATALYST_EVO_INTERNAL_CROSSOVER_H
#define CATALYST_EVO_INTERNAL_CROSSOVER_H

#include "catalyst/evo/evo.h"
#include "internal/rng.h"

/*
 * Resolve one crossover decision and produce two children from bounded genome
 * views. The parent views are read-only. Child views must be distinct,
 * non-overlapping, and must not overlap either parent. Invalid input preserves
 * the RNG and child outputs.
 *
 * A selected event invokes the consumer callback exactly once when present.
 * Otherwise each parent is cloned into its corresponding child. The callback
 * has no failure channel and must fully initialize both children.
 */
evo_status_t evo_crossover_pair(const evo_problem_t *problem,
                                const evo_config_t *config,
                                void *context,
                                evo_rng_t *rng,
                                const void *parent_a,
                                const void *parent_b,
                                void *child_a,
                                void *child_b);

#endif
