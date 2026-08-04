#ifndef CATALYST_EVO_INTERNAL_MUTATION_H
#define CATALYST_EVO_INTERNAL_MUTATION_H

#include "catalyst/evo/evo.h"
#include "internal/rng.h"

/* Return whether an explicit mutation enum value is defined by policy 1. */
bool evo_mutation_operator_is_valid(
    evo_mutation_operator_t mutation_operator);

/* Validate the explicit mutation enum, rate, and bounded byte policy. */
evo_status_t evo_mutation_validate_config(const evo_problem_t *problem,
                                          const evo_config_t *config);

/*
 * Resolve one fixed-rate mutation decision over a bounded writable genome.
 * Invalid input preserves the RNG and genome. Every valid attempt consumes
 * one RNG word, including rate endpoints and a missing callback.
 *
 * Consumer mode invokes the callback exactly once for a selected event when it
 * is present. Explicit byte mode uses the same stream to select one byte and
 * one nonzero XOR mask and never invokes the callback. The callback mutates in
 * place, owns no storage, and has no rollback channel.
 */
evo_status_t evo_mutate_genome(const evo_problem_t *problem,
                               const evo_config_t *config,
                               void *context,
                               evo_rng_t *rng,
                               void *genome);

#endif
