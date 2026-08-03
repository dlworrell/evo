#include "internal/fitness.h"

#include <math.h>

bool evo_fitness_evidence_is_valid(const evo_fitness_t *fitness)
{
    return fitness != NULL &&
           isfinite(fitness->correctness) &&
           isfinite(fitness->performance) &&
           isfinite(fitness->memory_use) &&
           isfinite(fitness->reliability) &&
           isfinite(fitness->maintainability) &&
           isfinite(fitness->constraint_penalty) &&
           fitness->constraint_penalty >= 0.0 &&
           isfinite(fitness->total);
}

bool evo_fitness_candidate_is_rankable(
    const evo_fitness_candidate_view_t *candidate)
{
    return candidate != NULL && candidate->hard_valid &&
           candidate->evaluated &&
           evo_fitness_evidence_is_valid(candidate->fitness);
}

bool evo_fitness_compare_candidates(
    const evo_fitness_candidate_view_t *left,
    const evo_fitness_candidate_view_t *right,
    evo_fitness_order_t *order)
{
    evo_fitness_order_t candidate_order = EVO_FITNESS_ORDER_EQUAL;

    if (order == NULL || !evo_fitness_candidate_is_rankable(left) ||
        !evo_fitness_candidate_is_rankable(right)) {
        return false;
    }

    if (left->fitness->total > right->fitness->total) {
        candidate_order = EVO_FITNESS_ORDER_LEFT;
    } else if (left->fitness->total < right->fitness->total) {
        candidate_order = EVO_FITNESS_ORDER_RIGHT;
    } else if (left->generation < right->generation) {
        candidate_order = EVO_FITNESS_ORDER_LEFT;
    } else if (left->generation > right->generation) {
        candidate_order = EVO_FITNESS_ORDER_RIGHT;
    } else if (left->population_index < right->population_index) {
        candidate_order = EVO_FITNESS_ORDER_LEFT;
    } else if (left->population_index > right->population_index) {
        candidate_order = EVO_FITNESS_ORDER_RIGHT;
    }

    *order = candidate_order;
    return true;
}
