#ifndef CATALYST_EVO_INTERNAL_EVALUATION_WORKERS_H
#define CATALYST_EVO_INTERNAL_EVALUATION_WORKERS_H

#include "internal/population_storage.h"

#include <pthread.h>

typedef struct evo_evaluation_thread_backend {
    int (*create)(pthread_t *thread,
                  void *(*entry)(void *),
                  void *argument,
                  void *context);
    int (*join)(pthread_t thread, void **result, void *context);
    void *context;
} evo_evaluation_thread_backend_t;

/* Validate the complete serial or worker evaluation configuration. */
evo_status_t evo_evaluation_workers_validate_config(
    const evo_problem_t *problem,
    const evo_config_t *config);

/*
 * Run policy-version-1 worker evaluation after lifecycle preflight and
 * evaluation-record construction. Validity remains serial and worker results
 * are committed in ascending candidate order only after all waves succeed.
 */
evo_status_t evo_evaluation_workers_run(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    const evo_population_t *population,
    evo_candidate_evaluation_t *evaluations,
    size_t *valid_count,
    size_t *best_index,
    bool *has_best,
    const evo_evaluation_thread_backend_t *backend);

#endif
