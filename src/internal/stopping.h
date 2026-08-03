#ifndef CATALYST_EVO_INTERNAL_STOPPING_H
#define CATALYST_EVO_INTERNAL_STOPPING_H

#include "catalyst/evo/evo.h"

typedef struct evo_stopping_state {
    double significant_best_total;
    size_t stagnant_generations;
    bool initialized;
} evo_stopping_state_t;

/* Validate canonical disabled controls and every enabled numeric boundary. */
evo_status_t evo_stopping_validate_config(const evo_config_t *config);

/*
 * Classify committed generation zero. Fitness-target and diversity-floor
 * evidence precede a coincident generation limit.
 */
evo_status_t evo_stopping_classify_initial(
    const evo_config_t *config,
    const evo_result_t *result,
    bool generation_limit_reached,
    evo_termination_reason_t *reason);

/* Establish the significant-improvement reference from committed generation zero. */
evo_status_t evo_stopping_state_initialize(
    const evo_config_t *config,
    const evo_result_t *result,
    evo_stopping_state_t *state);

/*
 * Classify one committed child. Extinction precedes target, diversity, and
 * patience evidence; the generation limit follows every enabled policy.
 */
evo_status_t evo_stopping_classify_committed(
    const evo_config_t *config,
    const evo_result_t *result,
    bool all_invalid,
    bool generation_limit_reached,
    evo_stopping_state_t *state,
    evo_termination_reason_t *reason);

#endif
