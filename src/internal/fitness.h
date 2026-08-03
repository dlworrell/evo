#ifndef CATALYST_EVO_INTERNAL_FITNESS_H
#define CATALYST_EVO_INTERNAL_FITNESS_H

#include "catalyst/evo/evo.h"

typedef struct evo_fitness_candidate_view {
    const evo_fitness_t *fitness;
    uint64_t generation;
    size_t population_index;
    bool hard_valid;
    bool evaluated;
} evo_fitness_candidate_view_t;

typedef enum evo_fitness_order {
    EVO_FITNESS_ORDER_RIGHT = -1,
    EVO_FITNESS_ORDER_EQUAL = 0,
    EVO_FITNESS_ORDER_LEFT = 1
} evo_fitness_order_t;

/*
 * Validate policy-version-1 fitness evidence. Every component must be finite,
 * and constraint_penalty must be a non-negative magnitude. Both signed zeros
 * are accepted as zero.
 */
bool evo_fitness_evidence_is_valid(const evo_fitness_t *fitness);

/*
 * Return whether one candidate is eligible for ranking under the hard gate.
 * Hard-invalid or unevaluated records are never rankable.
 */
bool evo_fitness_candidate_is_rankable(
    const evo_fitness_candidate_view_t *candidate);

/*
 * Compare two rankable candidates under policy version 1. Higher total wins;
 * an exact total tie prefers the earlier generation and then the lower
 * population index. The output is committed only after both views validate.
 */
bool evo_fitness_compare_candidates(
    const evo_fitness_candidate_view_t *left,
    const evo_fitness_candidate_view_t *right,
    evo_fitness_order_t *order);

#endif
