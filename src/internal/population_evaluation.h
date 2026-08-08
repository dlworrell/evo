#ifndef CATALYST_EVO_INTERNAL_POPULATION_EVALUATION_H
#define CATALYST_EVO_INTERNAL_POPULATION_EVALUATION_H

#include "internal/population_storage.h"

/*
 * Evaluate a population whose lifecycle-specific preflight has completed.
 *
 * Evaluation records remain provisional until every valid candidate returns
 * finite fitness. The operation commits the complete record set atomically
 * with respect to library-owned population state. Consumer callback side
 * effects cannot be rolled back.
 */
evo_status_t evo_population_evaluate_ready(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *population);

/* Allocate and attach empty local-backend evaluation owners for restore. */
evo_status_t evo_population_restore_evaluations_allocate(
    const evo_config_t *config,
    evo_population_t *population);

/* Allocate a detached zeroed evaluation reserve for one reusable child. */
evo_status_t evo_population_reusable_evaluations_allocate(
    const evo_config_t *config,
    evo_population_t *population);

#endif
