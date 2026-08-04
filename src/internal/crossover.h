#ifndef CATALYST_EVO_INTERNAL_CROSSOVER_H
#define CATALYST_EVO_INTERNAL_CROSSOVER_H

#include "catalyst/evo/evo.h"
#include "internal/rng.h"

/* Return whether an explicit crossover enum value is defined by policy 1. */
bool evo_crossover_operator_is_valid(
    evo_crossover_operator_t crossover_operator);

/* Validate the explicit crossover enum, rate, and bounded byte policy. */
evo_status_t evo_crossover_validate_config(const evo_problem_t *problem,
                                           const evo_config_t *config);

/*
 * Resolve one crossover decision and produce two children from bounded genome
 * views. The parent views are read-only. Child views must be distinct,
 * non-overlapping, and must not overlap either parent. Invalid input preserves
 * the RNG and child outputs.
 *
 * Consumer mode invokes the callback exactly once for a selected event when it
 * is present and otherwise clones corresponding parents. Explicit byte modes
 * use the same stream after the one-word gate and never invoke the callback.
 * Every successful path completely initializes both child spans.
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
