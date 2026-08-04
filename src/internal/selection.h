#ifndef CATALYST_EVO_INTERNAL_SELECTION_H
#define CATALYST_EVO_INTERNAL_SELECTION_H

#include "internal/population_storage.h"
#include "internal/rng.h"

/*
 * Validate the version-1 selection configuration without requiring that a
 * selection draw will occur. Tournament size zero is accepted only for an
 * otherwise canonical tournament policy whose selection path is unused.
 */
evo_status_t evo_selection_validate_config(const evo_config_t *config);

/* Validate a configuration for an operation that must select one parent. */
evo_status_t evo_selection_validate_active_config(
    const evo_config_t *config);

/* Dispatch one parent selection through the configured version-1 policy. */
evo_status_t evo_population_select(const evo_config_t *config,
                                   const evo_population_t *population,
                                   evo_rng_t *rng,
                                   size_t *selected_index);

/*
 * Select one evaluated candidate by deterministic tournament.
 *
 * Samples are drawn with replacement from hard-valid evaluated candidates
 * only. Fitness-comparison policy version 1 maximizes fitness.total; an exact
 * tie selects the lower population index. Validation failures preserve the
 * output, population, and RNG stream.
 */
evo_status_t evo_population_select_tournament(
    const evo_config_t *config,
    const evo_population_t *population,
    evo_rng_t *rng,
    size_t *selected_index);

#endif
