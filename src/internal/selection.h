#ifndef CATALYST_EVO_INTERNAL_SELECTION_H
#define CATALYST_EVO_INTERNAL_SELECTION_H

#include "internal/population_storage.h"
#include "internal/rng.h"

/*
 * Select one evaluated candidate by deterministic tournament.
 *
 * Samples are drawn with replacement from valid candidates only. Higher
 * fitness.total wins; an exact tie selects the lower population index.
 * Validation failures preserve the output, population, and RNG stream.
 */
evo_status_t evo_population_select_tournament(
    const evo_config_t *config,
    const evo_population_t *population,
    evo_rng_t *rng,
    size_t *selected_index);

#endif
