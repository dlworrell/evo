#ifndef CATALYST_EVO_INTERNAL_BOUNDED_RUN_H
#define CATALYST_EVO_INTERNAL_BOUNDED_RUN_H

#include "internal/generation_advancement.h"

#define EVO_BOUNDED_RUN_POLICY_VERSION UINT32_C(1)

typedef struct evo_bounded_run_evidence {
    size_t population_size;
    size_t requested_transitions;
    size_t completed_transitions;
    size_t final_valid_count;
    size_t final_best_index;
    uint64_t final_generation;
    uint64_t best_generation;
    uint32_t operator_seed_schedule_version;
    uint32_t odd_child_policy_version;
    uint32_t child_evaluation_policy_version;
    uint32_t generation_advancement_policy_version;
    uint32_t policy_version;
    bool final_has_best;
    bool stopped_all_invalid;
    bool complete;
} evo_bounded_run_evidence_t;

/*
 * Validate transition-only policy before generation-zero callback dispatch.
 * A zero generation limit deliberately preserves the generation-zero public
 * contract and does not require unused child or operator configuration.
 */
evo_status_t evo_bounded_run_validate_config(
    const evo_problem_t *problem,
    const evo_config_t *config);

/*
 * Execute at most generation_limit transitions from one completed
 * generation-zero parent. The active best result must be an independent copy
 * of that parent's stable winner. On successful completion the parent owns
 * the final promoted population and the result records the stable best seen
 * across all completed populations.
 *
 * A failure after one or more successful transitions may leave private parent
 * and best-result state advanced. The public owner must destroy both before
 * returning; caller-visible state is committed only after this operation
 * succeeds.
 */
evo_status_t evo_bounded_run_advance(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *parents,
    evo_result_t *best_result,
    evo_bounded_run_evidence_t *evidence);

#endif
