#ifndef CATALYST_EVO_INTERNAL_DIVERSITY_H
#define CATALYST_EVO_INTERNAL_DIVERSITY_H

#include "internal/population_storage.h"

/*
 * Validate callback/version coupling and the all-valid worst-case work bound.
 * The check invokes no callback and consumes no RNG state.
 */
evo_status_t evo_diversity_validate_config(
    const evo_problem_t *problem,
    const evo_config_t *config);

/*
 * Measure one fully evaluated population using fixed lexicographic unordered
 * pairs. Invalid candidates are excluded. Evidence is committed only after
 * every distance is valid and all work completes.
 */
evo_status_t evo_population_measure_diversity(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *population);

/* Validate stored diversity metadata without invoking a consumer callback. */
bool evo_population_diversity_evidence_is_valid(
    const evo_config_t *config,
    const evo_population_t *population);

#endif
