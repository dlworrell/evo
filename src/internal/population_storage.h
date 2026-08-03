#ifndef CATALYST_EVO_INTERNAL_POPULATION_STORAGE_H
#define CATALYST_EVO_INTERNAL_POPULATION_STORAGE_H

#include "catalyst/evo/evo.h"

typedef struct evo_candidate_evaluation {
    evo_fitness_t fitness;
    bool valid;
    bool evaluated;
} evo_candidate_evaluation_t;

typedef struct evo_population {
    unsigned char *genomes;
    evo_candidate_evaluation_t *evaluations;
    size_t population_size;
    size_t genome_size;
    size_t storage_bytes;
    size_t evaluation_bytes;
    size_t valid_count;
    size_t best_index;
    size_t produced_count;
    uint64_t initialization_seed;
    uint64_t source_generation;
    uint32_t rng_algorithm_version;
    uint32_t operator_seed_schedule_version;
    uint32_t odd_child_policy_version;
    uint32_t fitness_comparison_policy_version;
    bool initialized;
    bool has_best;
    bool evaluated;
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
 * Construct an independently owned, zero-initialized child genome slab from
 * one structurally consistent completed parent population. The child uses its
 * own caller-provided byte budget and begins without initialization or
 * evaluation evidence or committed child-production progress.
 */
evo_status_t evo_child_population_create(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    evo_population_t *children);

/*
 * Validate structurally complete evaluated population evidence without
 * changing the population. Generation-zero and produced-child provenance are
 * accepted through distinct invariant sets. Selection and child allocation
 * share this authority.
 */
bool evo_population_validate_completed(
    const evo_config_t *config,
    const evo_population_t *population,
    size_t *validated_valid_count);

/*
 * Return a bounded non-owning view of one genome, or NULL when the population
 * is inactive or the index is outside the constructed population.
 */
void *evo_population_genome(evo_population_t *population, size_t index);
const void *evo_population_genome_const(const evo_population_t *population,
                                        size_t index);

/*
 * Fill every genome from the configured deterministic random stream, then
 * invoke the optional problem initializer once per genome in ascending order.
 *
 * The population must be active, uninitialized, and consistent with the
 * supplied problem and configuration. Lifecycle-state rejection preserves the
 * complete population unchanged.
 */
evo_status_t evo_population_initialize(const evo_problem_t *problem,
                                       const evo_config_t *config,
                                       void *context,
                                       evo_population_t *population);

/*
 * Validate every initialized generation-zero candidate, then evaluate only
 * valid candidates in ascending index order.
 *
 * Evaluation records are private, caller-budgeted, and committed to the
 * population only after every returned fitness field is proven finite.
 * Completing the phase with no valid candidates succeeds without a best
 * candidate.
 */
evo_status_t evo_population_evaluate(const evo_problem_t *problem,
                                     const evo_config_t *config,
                                     void *context,
                                     evo_population_t *population);

/*
 * Return a read-only evaluation record for a completed population, or NULL
 * when the phase is incomplete or the index is out of range.
 */
const evo_candidate_evaluation_t *
evo_population_evaluation_const(const evo_population_t *population,
                                size_t index);

/*
 * Store the deterministic best-candidate index and return true, or return
 * false without modifying best_index when no evaluated winner exists.
 */
bool evo_population_best_index(const evo_population_t *population,
                               size_t *best_index);

/*
 * Release the population storage and reset the complete object to zero.
 * This operation is null-safe and repeatable for initialized objects.
 * It releases ordinary genome storage without securely erasing its bytes.
 */
void evo_population_destroy(evo_population_t *population);

#endif
