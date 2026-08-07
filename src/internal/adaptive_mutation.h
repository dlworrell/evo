#ifndef CATALYST_EVO_INTERNAL_ADAPTIVE_MUTATION_H
#define CATALYST_EVO_INTERNAL_ADAPTIVE_MUTATION_H

#include "catalyst/evo/evo.h"

typedef struct evo_adaptive_mutation_state {
    double effective_rate;
    size_t stagnant_generations;
    bool initialized;
} evo_adaptive_mutation_state_t;

/* Return whether the configured run can dispatch at least one mutation path. */
bool evo_adaptive_mutation_is_applicable(const evo_config_t *config);

/* Validate canonical disabled payloads and every enabled numeric boundary. */
evo_status_t evo_adaptive_mutation_validate_config(
    const evo_config_t *config);

/*
 * Project the first positive-limit decision from committed generation zero.
 * The output statistics and state are committed atomically and no RNG or
 * callback is used.
 */
evo_status_t evo_adaptive_mutation_initialize(
    const evo_config_t *config,
    evo_generation_statistics_t *statistics,
    evo_adaptive_mutation_state_t *state);

/* Reconstruct the private state from a validated generation-zero projection. */
evo_status_t evo_adaptive_mutation_restore_initial(
    const evo_config_t *config,
    const evo_generation_statistics_t *statistics,
    evo_adaptive_mutation_state_t *state);

/*
 * Project the next rate after one later generation commits. The prior rate is
 * the rate that produced this generation. A strict global-best improvement is
 * supplied by the bounded-run comparison authority.
 */
evo_status_t evo_adaptive_mutation_commit(
    const evo_config_t *config,
    bool global_best_improved,
    evo_generation_statistics_t *statistics,
    evo_adaptive_mutation_state_t *state);

/* Validate a committed public audit projection without changing it. */
bool evo_adaptive_mutation_statistics_are_valid(
    const evo_config_t *config,
    const evo_generation_statistics_t *statistics);

#endif
