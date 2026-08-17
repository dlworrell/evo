#ifndef CATALYST_EVO_INTERNAL_POPULATION_EVALUATION_H
#define CATALYST_EVO_INTERNAL_POPULATION_EVALUATION_H

#include "internal/batch_evaluation.h"
#include "internal/population_storage.h"

struct evo_evaluation_thread_backend;

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

/*
 * Private product seam: one bounded batch evaluator may classify and evaluate
 * the complete population into provisional records. The core validates and
 * commits those records in stable candidate order. Public evo_run semantics do
 * not use this path.
 */
evo_status_t evo_population_evaluate_ready_with_batch_evaluator(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    uint64_t generation,
    evo_population_t *population,
    const evo_population_batch_evaluator_t *batch_evaluator);

/* Test seam for deterministic worker-start failure injection. */
evo_status_t evo_population_evaluate_ready_with_worker_backend(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *population,
    const struct evo_evaluation_thread_backend *backend);

/* Initial-population wrapper for the private product batch seam. */
evo_status_t evo_population_evaluate_with_batch_evaluator(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    uint64_t generation,
    evo_population_t *population,
    const evo_population_batch_evaluator_t *batch_evaluator);

/* Allocate and attach empty local-backend evaluation owners for restore. */
evo_status_t evo_population_restore_evaluations_allocate(
    const evo_config_t *config,
    evo_population_t *population);

/* Allocate a detached zeroed evaluation reserve for one reusable child. */
evo_status_t evo_population_reusable_evaluations_allocate(
    const evo_config_t *config,
    evo_population_t *population);

#endif
