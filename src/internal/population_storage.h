#ifndef CATALYST_EVO_INTERNAL_POPULATION_STORAGE_H
#define CATALYST_EVO_INTERNAL_POPULATION_STORAGE_H

#include "catalyst/evo/evo.h"

typedef struct evo_population {
    unsigned char *genomes;
    size_t population_size;
    size_t genome_size;
    size_t storage_bytes;
} evo_population_t;

/*
 * Construct zero-initialized contiguous genome storage.
 *
 * The population object must be zero-initialized before first use. An active
 * object is rejected without modification. Every other failure leaves the
 * object empty.
 */
evo_status_t evo_population_create(const evo_problem_t *problem,
                                   const evo_config_t *config,
                                   evo_population_t *population);

/*
 * Return a bounded non-owning view of one genome, or NULL when the population
 * is inactive or the index is outside the constructed population.
 */
void *evo_population_genome(evo_population_t *population, size_t index);
const void *evo_population_genome_const(const evo_population_t *population,
                                        size_t index);

/*
 * Release the population storage and reset the complete object to zero.
 * This operation is null-safe and repeatable for initialized objects.
 * It releases ordinary genome storage without securely erasing its bytes.
 */
void evo_population_destroy(evo_population_t *population);

#endif
