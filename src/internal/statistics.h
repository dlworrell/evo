#ifndef CATALYST_EVO_INTERNAL_STATISTICS_H
#define CATALYST_EVO_INTERNAL_STATISTICS_H

#include "internal/population_storage.h"

/*
 * Summarize one completed population without allocating, consuming RNG state,
 * invoking callbacks, or changing population or winner evidence.
 *
 * Valid evaluated candidates are traversed once in ascending population
 * index. Invalid fitness payloads are never read. The caller-owned output is
 * committed only after the complete record and every component sum are proven
 * finite and structurally consistent. Stored diversity evidence is validated
 * and copied without invoking a distance callback.
 */
evo_status_t evo_generation_statistics_record(
    const evo_config_t *config,
    const evo_population_t *population,
    uint64_t generation_index,
    evo_generation_statistics_t *statistics);

#endif
