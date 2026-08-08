#ifndef CATALYST_EVO_INTERNAL_RUN_STATE_H
#define CATALYST_EVO_INTERNAL_RUN_STATE_H

#include "internal/adaptive_mutation.h"
#include "internal/stopping.h"

#define EVO_RUN_STATE_VERSION UINT32_C(1)

/*
 * Complete constant-space continuation authority for one committed run.
 * Checkpoints persist every field; resume never reconstructs hidden history.
 */
typedef struct evo_run_state {
    uint32_t version;
    uint64_t current_generation;
    uint64_t best_generation;
    size_t best_population_index;
    evo_adaptive_mutation_state_t adaptive_mutation;
    evo_stopping_state_t stopping;
    evo_termination_reason_t termination_reason;
    bool adaptive_mutation_applicable;
    bool initialized;
} evo_run_state_t;

#endif
