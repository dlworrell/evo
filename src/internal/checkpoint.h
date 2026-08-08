#ifndef CATALYST_EVO_INTERNAL_CHECKPOINT_H
#define CATALYST_EVO_INTERNAL_CHECKPOINT_H

#include "internal/population_storage.h"
#include "internal/run_state.h"

/* Validate checkpoint delivery resources before any run callback. */
evo_status_t evo_checkpoint_validate_config(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_result_t *result);

/* Serialize and synchronously deliver one committed snapshot when enabled. */
evo_status_t evo_checkpoint_emit(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *population,
    const evo_result_t *result,
    const evo_run_state_t *state);

/* Restore owners and continuation state without invoking callbacks or RNG. */
evo_status_t evo_checkpoint_restore(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const void *checkpoint,
    size_t checkpoint_size,
    evo_population_t *population,
    evo_result_t *result,
    evo_run_state_t *state);

#endif
