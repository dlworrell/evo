#ifndef CATALYST_EVO_INTERNAL_GENERATION_ADVANCEMENT_H
#define CATALYST_EVO_INTERNAL_GENERATION_ADVANCEMENT_H

#include "internal/population_storage.h"

#define EVO_GENERATION_ADVANCEMENT_POLICY_VERSION UINT32_C(9)

typedef struct evo_generation_advancement_evidence {
    size_t population_size;
    size_t valid_count;
    size_t best_index;
    size_t elite_count;
    size_t elite_source_valid_count;
    uint64_t previous_generation;
    uint64_t completed_generation;
    uint32_t operator_seed_schedule_version;
    uint32_t selection_policy_version;
    evo_selection_policy_t selection_policy;
    uint32_t byte_operator_policy_version;
    evo_crossover_operator_t crossover_operator;
    evo_mutation_operator_t mutation_operator;
    double mutation_rate_used;
    uint32_t odd_child_policy_version;
    uint32_t elite_policy_version;
    uint32_t singleton_child_policy_version;
    uint32_t fitness_comparison_policy_version;
    uint32_t diversity_policy_version;
    uint32_t diversity_metric_version;
    uint32_t parallel_evaluation_policy_version;
    size_t evaluation_worker_count;
    uint32_t population_recycling_policy_version;
    uint64_t active_storage_owner_identity;
    uint64_t reusable_storage_owner_identity;
    uint32_t policy_version;
    bool has_best;
    bool elite_count_explicit;
    bool population_recycling_enabled;
    bool complete;
} evo_generation_advancement_evidence_t;

/*
 * Move one completed evaluated child population into the parent handle.
 *
 * Every fallible condition is checked before either population changes. On
 * success, the child allocations become parent-owned without copying, the
 * child handle is reset to zero, and the former parent is released. The
 * operation allocates no memory, consumes no RNG state, and invokes no
 * consumer callback.
 */
evo_status_t evo_population_advance_generation(
    const evo_problem_t *problem,
    const evo_config_t *config,
    uint64_t current_generation,
    evo_population_t *parents,
    evo_population_t *evaluated_children,
    evo_generation_advancement_evidence_t *evidence);

/* Registry-aware variant used by opt-in bounded-run recycling. */
evo_status_t evo_population_advance_generation_with_registry(
    const evo_problem_t *problem,
    const evo_config_t *config,
    uint64_t current_generation,
    evo_population_t *parents,
    evo_population_t *evaluated_children,
    evo_population_storage_registry_t *storage_registry,
    evo_generation_advancement_evidence_t *evidence);

#endif
