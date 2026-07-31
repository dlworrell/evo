#ifndef CATALYST_EVO_INTERNAL_MUTATION_H
#define CATALYST_EVO_INTERNAL_MUTATION_H

#include "catalyst/evo/evo.h"
#include "internal/rng.h"

/*
 * Resolve one fixed-rate mutation decision over a bounded writable genome.
 * Invalid input preserves the RNG and genome. Every valid attempt consumes
 * one RNG word, including rate endpoints and a missing callback.
 *
 * A selected event invokes the consumer callback exactly once when present.
 * The callback mutates in place, owns no storage, and has no rollback channel.
 */
evo_status_t evo_mutate_genome(const evo_problem_t *problem,
                               const evo_config_t *config,
                               void *context,
                               evo_rng_t *rng,
                               void *genome);

#endif
