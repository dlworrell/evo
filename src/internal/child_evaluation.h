#ifndef CATALYST_EVO_INTERNAL_CHILD_EVALUATION_H
#define CATALYST_EVO_INTERNAL_CHILD_EVALUATION_H

#include "internal/population_evaluation.h"

#define EVO_CHILD_EVALUATION_POLICY_VERSION UINT32_C(4)

typedef struct evo_child_evaluation_evidence {
    size_t population_size;
    size_t evaluation_bytes;
    size_t valid_count;
    size_t best_index;
    size_t elite_count;
    size_t elite_source_valid_count;
    uint64_t source_generation;
    uint32_t operator_seed_schedule_version;
    uint32_t odd_child_policy_version;
    uint32_t elite_policy_version;
    uint32_t singleton_child_policy_version;
    uint32_t fitness_comparison_policy_version;
    uint32_t diversity_policy_version;
    uint32_t diversity_metric_version;
    uint32_t policy_version;
    bool has_best;
    bool elite_count_explicit;
    bool complete;
} evo_child_evaluation_evidence_t;

/*
 * Validate and evaluate one fully produced child population.
 *
 * Validation and evaluation callbacks run in ascending candidate order, and
 * invalid candidates are never evaluated. Diversity then follows its fixed
 * valid-pair schedule. No RNG state is consumed and child genome bytes are
 * read-only. Rejection preserves library-owned child state and caller-owned
 * evidence, although consumer callback side effects cannot be rolled back
 * after dispatch begins.
 */
evo_status_t evo_child_population_evaluate(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    uint64_t source_generation,
    evo_population_t *children,
    evo_child_evaluation_evidence_t *evidence);

#endif
