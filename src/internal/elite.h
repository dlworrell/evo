#ifndef CATALYST_EVO_INTERNAL_ELITE_H
#define CATALYST_EVO_INTERNAL_ELITE_H

#include "internal/population_storage.h"

#define EVO_SINGLETON_CHILD_POLICY_VERSION UINT32_C(1)

typedef struct evo_elite_evidence {
    size_t requested_count;
    size_t effective_count;
    size_t source_valid_count;
    size_t offspring_count;
    size_t best_parent_index;
    size_t worst_elite_parent_index;
    uint64_t source_generation;
    uint32_t operator_seed_schedule_version;
    uint32_t selection_policy_version;
    evo_selection_policy_t selection_policy;
    uint32_t byte_operator_policy_version;
    evo_crossover_operator_t crossover_operator;
    evo_mutation_operator_t mutation_operator;
    uint32_t odd_child_policy_version;
    uint32_t elite_policy_version;
    uint32_t singleton_child_policy_version;
    bool elite_count_explicit;
    bool complete;
} evo_elite_evidence_t;

/* Validate the public elite-count mode without inspecting a population. */
evo_status_t evo_elite_validate_config(const evo_config_t *config);

/*
 * Resolve the requested, effective, and ordinary-offspring counts. Explicit
 * requests preserve at most the distinct hard-valid parents available. The
 * disabled compatibility mode requests one elite for odd populations and zero
 * for even populations.
 */
evo_status_t evo_elite_policy_counts(const evo_config_t *config,
                                     size_t source_valid_count,
                                     size_t *requested_count,
                                     size_t *effective_count,
                                     size_t *offspring_count);

/*
 * Prove that population objects, their owned ranges, and a caller evidence
 * range are mutually independent before completion can commit any byte.
 */
bool evo_elite_completion_objects_are_independent(
    const evo_population_t *parents,
    const evo_population_t *children,
    const void *evidence,
    size_t evidence_size);

/*
 * Clone the stable elite suffix after the ordinary offspring prefix is fully
 * produced. Ranking uses the common comparison authority. All fallible
 * library checks, including a complete dry ranking pass, precede byte copies.
 */
evo_status_t evo_elite_population_complete(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    evo_population_t *children,
    evo_elite_evidence_t *evidence);

#endif
