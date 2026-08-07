#ifndef CATALYST_EVO_EVO_H
#define CATALYST_EVO_EVO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVO_VERSION_MAJOR 0
#define EVO_VERSION_MINOR 27
#define EVO_VERSION_PATCH 0

typedef enum evo_status {
    EVO_SUCCESS = 0,
    EVO_ERROR_INVALID_ARGUMENT = -1,
    EVO_ERROR_OUT_OF_MEMORY = -2,
    EVO_ERROR_RESULT_ACTIVE = -3,
    EVO_ERROR_RESOURCE_LIMIT = -4,
    EVO_ERROR_STATE = -5,
    EVO_ERROR_EVALUATION = -6,
    EVO_ERROR_NO_VALID_CANDIDATE = -7
} evo_status_t;

typedef enum evo_termination_reason {
    EVO_TERMINATION_NONE = 0,
    EVO_TERMINATION_GENERATION_LIMIT = 1,
    EVO_TERMINATION_ALL_INVALID = 2,
    EVO_TERMINATION_APPLICATION_REQUESTED = 3,
    EVO_TERMINATION_CONVERGED = 4,
    EVO_TERMINATION_STAGNATED = 5
} evo_termination_reason_t;

typedef enum evo_selection_policy {
    EVO_SELECTION_TOURNAMENT = 0,
    EVO_SELECTION_RANK = 1
} evo_selection_policy_t;

/*
 * The zero-valued operator modes preserve the consumer callback dispatch used
 * before version 0.26.0. Nonzero modes explicitly select bounded reference
 * operators over byte genomes; EVO never infers a byte representation from a
 * callback being absent.
 */
typedef enum evo_crossover_operator {
    EVO_CROSSOVER_CONSUMER = 0,
    EVO_CROSSOVER_BYTE_ONE_POINT = 1,
    EVO_CROSSOVER_BYTE_TWO_POINT = 2,
    EVO_CROSSOVER_BYTE_UNIFORM = 3
} evo_crossover_operator_t;

typedef enum evo_mutation_operator {
    EVO_MUTATION_CONSUMER = 0,
    EVO_MUTATION_BYTE_XOR = 1
} evo_mutation_operator_t;

/*
 * Explainable result of one committed-generation adaptive-mutation decision.
 * The zero value is reserved for runs where transition mutation cannot occur
 * and its policy is deliberately not inspected.
 */
typedef enum evo_mutation_adaptation_reason {
    EVO_MUTATION_ADAPTATION_NOT_APPLICABLE = 0,
    EVO_MUTATION_ADAPTATION_DISABLED = 1,
    EVO_MUTATION_ADAPTATION_INITIAL = 2,
    EVO_MUTATION_ADAPTATION_LOW_DIVERSITY = 3,
    EVO_MUTATION_ADAPTATION_STAGNATION = 4,
    EVO_MUTATION_ADAPTATION_STAGNATION_LOW_DIVERSITY = 5,
    EVO_MUTATION_ADAPTATION_IMPROVEMENT_RESET = 6,
    EVO_MUTATION_ADAPTATION_IMPROVEMENT_HOLD = 7
} evo_mutation_adaptation_reason_t;

#define EVO_FITNESS_COMPARISON_POLICY_VERSION UINT32_C(1)
#define EVO_DIVERSITY_POLICY_VERSION UINT32_C(1)
#define EVO_BYTE_DIVERSITY_METRIC_VERSION UINT32_C(1)
#define EVO_STOPPING_POLICY_VERSION UINT32_C(1)
#define EVO_ELITE_POLICY_VERSION UINT32_C(1)
#define EVO_SELECTION_POLICY_VERSION UINT32_C(1)
#define EVO_BYTE_OPERATOR_POLICY_VERSION UINT32_C(1)
#define EVO_MUTATION_ADAPTATION_POLICY_VERSION UINT32_C(1)

/*
 * Fitness components are caller-owned evidence. constraint_penalty is a
 * finite, non-negative soft-constraint penalty magnitude. total is the
 * caller-computed scalar objective: callers account for any penalty they want
 * applied before returning it, and EVO never subtracts or reweights the
 * penalty independently. Hard-invalid candidates are excluded before
 * evaluation. Among evaluated hard-valid candidates, comparison policy 1
 * maximizes total and resolves an exact tie by stable generation/index order.
 */
typedef struct evo_fitness {
    double correctness;
    double performance;
    double memory_use;
    double reliability;
    double maintainability;
    double constraint_penalty;
    double total;
} evo_fitness_t;

#define EVO_GENERATION_STATISTICS_VERSION UINT32_C(4)

/*
 * Constant-space evidence for one committed generation. fitness_sums is the
 * component-wise sum of valid evaluated candidates in ascending population
 * index. Invalid fitness payloads are excluded. has_best is false, best_index
 * is zero, and best_fitness is zero when valid_count is zero. Successful
 * schema-4 records identify the comparison policy plus deterministic
 * diversity and adaptive-mutation policy. Diversity is the arithmetic mean of normalized
 * distances over all unordered pairs of hard-valid candidates in
 * lexicographic index order. Zero or one valid candidate has diversity zero
 * and no pair work.
 */
typedef struct evo_generation_statistics {
    uint32_t version;
    uint64_t generation_index;
    size_t population_size;
    size_t valid_count;
    size_t invalid_count;
    size_t best_index;
    evo_fitness_t best_fitness;
    evo_fitness_t fitness_sums;
    bool has_best;
    uint32_t fitness_comparison_policy_version;
    uint32_t diversity_policy_version;
    uint32_t diversity_metric_version;
    size_t diversity_pair_count;
    size_t diversity_work_units;
    double diversity;
    bool diversity_uses_domain_distance;
    /*
     * Schema-4 adaptive-mutation audit projection. mutation_rate_prior is the
     * requested base rate for generation zero and the rate used to produce a
     * later committed generation. mutation_rate_effective is the rate selected
     * after that commit for a possible next transition.
     */
    uint32_t adaptive_mutation_policy_version;
    double mutation_rate_prior;
    double mutation_rate_effective;
    double adaptive_mutation_min_rate;
    double adaptive_mutation_max_rate;
    double adaptive_mutation_step;
    double adaptive_mutation_diversity_threshold;
    size_t adaptive_mutation_stagnant_generations;
    evo_mutation_adaptation_reason_t mutation_adaptation_reason;
    bool adaptive_mutation_enabled;
    bool adaptive_mutation_low_diversity;
    bool adaptive_mutation_global_best_improved;
    bool adaptive_mutation_clamped_to_min;
    bool adaptive_mutation_clamped_to_max;
    bool adaptive_mutation_reset_on_improvement;
} evo_generation_statistics_t;

#define EVO_GENERATION_RESULT_VIEW_VERSION UINT32_C(1)

/*
 * Read-only, callback-lifetime view of the global result after one generation
 * commits. best_genome points to exactly best_genome_size bytes and may be
 * inspected only until the callback returns. The view owns no storage.
 */
typedef struct evo_generation_result_view {
    uint32_t version;
    const void *best_genome;
    size_t best_genome_size;
    evo_fitness_t best_fitness;
    size_t generations_completed;
    uint64_t random_seed;
    evo_termination_reason_t termination_reason;
} evo_generation_result_view_t;

/*
 * Synchronous, non-stopping notification for one committed generation. Both
 * view pointers and best_genome are non-owning and valid only for the duration
 * of the call. The observer must not retain them or cast away const, and it
 * cannot cancel or otherwise change EVO control flow.
 */
typedef void (*evo_generation_observer_fn)(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context);

/*
 * Synchronous decision evaluated only for a committed generation from which
 * EVO could otherwise continue. Returning true requests successful stopping;
 * returning false preserves the configured bounded run. The view pointers and
 * best_genome are non-owning and valid only for the duration of the call.
 */
typedef bool (*evo_generation_stop_fn)(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context);

/*
 * Optional deterministic domain distance for two hard-valid genomes. The
 * callback must return a finite normalized value in [0, 1], use no
 * unrecorded entropy, retain no genome view, and leave ownership unchanged.
 */
typedef double (*evo_genome_distance_fn)(
    const void *genome_a,
    const void *genome_b,
    size_t genome_size,
    void *context);

typedef struct evo_problem {
    size_t genome_size;
    /*
     * Generation-zero population initialization receives deterministic bytes
     * supplied by EVO. The callback may transform only its genome, must be
     * deterministic for fixed bytes and context, and may not retain the view.
     */
    void (*initialize)(void *genome, void *context);
    /*
     * In EVO_MUTATION_CONSUMER mode, mutation receives one bounded writable
     * genome after EVO selects the configured per-genome event. The callback
     * receives mutation_rate unchanged as its representation-specific
     * intensity, must be deterministic for fixed bytes, rate, and context, may
     * use no unrecorded entropy, and may not change ownership or retain the
     * view. Explicit built-in modes do not invoke this callback.
     */
    void (*mutate)(void *genome, double mutation_rate, void *context);
    /*
     * In EVO_CROSSOVER_CONSUMER mode, crossover receives two bounded read-only
     * parents and two distinct, non-overlapping writable children. It must
     * initialize both children completely, preserve ownership, retain no view,
     * and remain deterministic for fixed parents and context. Explicit
     * built-in modes do not invoke this callback.
     */
    void (*crossover)(const void *parent_a, const void *parent_b, void *child_a, void *child_b, void *context);
    evo_fitness_t (*evaluate)(const void *genome, void *context);
    bool (*is_valid)(const void *genome, void *context);
    /* NULL selects built-in byte mismatch metric version 1. */
    evo_genome_distance_fn genome_distance;
    /* Must be nonzero exactly when genome_distance is non-NULL. */
    uint32_t genome_distance_version;
} evo_problem_t;

typedef struct evo_config {
    size_t population_size;
    size_t generation_limit;
    size_t tournament_size;
    double crossover_rate;
    double mutation_rate;
    uint64_t random_seed;
    /* Maximum bytes accepted for one genome allocation. */
    size_t max_genome_bytes;
    /* Maximum bytes accepted for the contiguous population genome slab. */
    size_t max_population_bytes;
    /* Maximum bytes accepted for private candidate-evaluation records. */
    size_t max_evaluation_bytes;
    /* Maximum bytes accepted for one private child-population genome slab. */
    size_t max_child_population_bytes;
    /* Optional committed-generation observer; NULL disables observation. */
    evo_generation_observer_fn generation_observer;
    /* Caller-owned observer state, never inspected or retained by EVO. */
    void *generation_observer_context;
    /* Optional committed-generation stop decision; NULL disables stopping. */
    evo_generation_stop_fn generation_stop;
    /* Caller-owned stop-decision state, never inspected or retained by EVO. */
    void *generation_stop_context;
    /*
     * Maximum diversity work per generation. Built-in work is byte
     * comparisons; domain-distance work is callback invocations. EVO rejects
     * an insufficient all-valid worst-case budget before any run callback.
     */
    size_t max_diversity_work;
    /*
     * Stop when the stable global-best total reaches fitness_target. The
     * target must be finite when enabled and zero when disabled.
     */
    bool fitness_target_enabled;
    double fitness_target;
    /*
     * Stop after stagnation_patience committed child generations without a
     * global-best improvement greater than improvement_tolerance. Enabled
     * tolerance must be finite and non-negative and patience must be positive;
     * both payloads must be zero when disabled.
     */
    bool stagnation_enabled;
    double improvement_tolerance;
    size_t stagnation_patience;
    /*
     * Stop when committed generation diversity is at or below this floor.
     * The floor must be finite in [0, 1] when enabled and zero when disabled.
     */
    bool diversity_floor_enabled;
    double diversity_floor;
    /*
     * When enabled, preserve up to elite_count distinct valid parents in
     * stable best-to-worst order at the child-population suffix. The count may
     * be zero and must not exceed population_size. If fewer valid parents are
     * available, every distinct valid parent is preserved and the remaining
     * slots are ordinary offspring. When disabled, elite_count must be zero
     * and EVO preserves the pre-0.24.0 compatibility behavior: one best-parent
     * tail clone for odd populations and no elite for even populations.
     */
    bool elite_count_enabled;
    size_t elite_count;
    /*
     * Selection policy version 1 uses tournament selection for the zero-valued
     * compatibility default. Rank mode requires tournament_size to be zero
     * and gives stable rank r among n valid candidates the exact integer
     * weight rank_base_weight + (n - 1 - r) * rank_step_weight. The base
     * weight must be positive. Both rank weights must be zero in tournament
     * mode, and the worst-case configured total must fit in size_t.
     */
    evo_selection_policy_t selection_policy;
    size_t rank_base_weight;
    size_t rank_step_weight;
    /*
     * Explicit operator dispatch. Zero selects the existing consumer callback
     * path, including clone/no-op behavior when its callback is NULL. Built-in
     * crossover and mutation modes operate only on the complete bounded byte
     * genome and use the existing domain-separated operator streams.
     */
    evo_crossover_operator_t crossover_operator;
    evo_mutation_operator_t mutation_operator;
    /*
     * Disabled-by-default deterministic mutation-rate adaptation. When
     * enabled, finite bounds and threshold lie in [0, 1], min <= max, and the
     * finite step is positive. The requested mutation_rate is clamped into the
     * configured interval after generation zero commits. Low diversity uses an
     * inclusive threshold. A strict global-best improvement resets the next
     * rate to the minimum when requested; otherwise low diversity increases
     * the rate and a high-diversity improvement holds it. Non-improvement
     * always increases the rate by one bounded step.
     */
    bool adaptive_mutation_enabled;
    double adaptive_mutation_min_rate;
    double adaptive_mutation_max_rate;
    double adaptive_mutation_step;
    double adaptive_mutation_diversity_threshold;
    bool adaptive_mutation_reset_on_improvement;
} evo_config_t;

typedef struct evo_result {
    void *best_genome;
    evo_fitness_t best_fitness;
    size_t generations_completed;
    uint64_t random_seed;
    evo_termination_reason_t termination_reason;
    evo_generation_statistics_t generation_statistics;
} evo_result_t;

/**
 * Execute a deterministic caller-bounded evolutionary run.
 *
 * The result object must be zero-initialized before its first use. A successful
 * call transfers exclusive ownership of best_genome to the result object.
 * Callers may use bounded, non-owning aliases to the genome bytes while the
 * result remains alive, but aliases may not free or reallocate the storage and
 * must not survive evo_result_destroy().
 *
 * Generation zero is always initialized and evaluated. generation_limit is
 * the maximum number of completed child-generation transitions after that
 * baseline; zero preserves generation-zero-only execution. A successful call
 * transfers an independent copy of the highest-total hard-valid candidate
 * observed across the run together with its complete fitness evidence. Soft
 * constraint_penalty is a finite non-negative magnitude already accounted for
 * by the caller in total; EVO never applies it again. Exact ties preserve the
 * earlier generation, then the lower population index.
 *
 * For each positive-limit transition, ordinary offspring occupy the child
 * prefix and stable elites occupy the suffix. Complete pairs use the existing
 * pair-local streams. If that prefix is odd, its last child is selected with
 * the next selection stream, cloned, and mutated with its child-index stream.
 * Elite copies consume no RNG state and invoke no consumer callback.
 *
 * An active result is rejected without modification. Other failures,
 * including completion with no valid candidate, leave a non-null, inactive
 * result in its empty, zero-initialized state.
 *
 * A later all-invalid child is promoted and terminates the run successfully
 * while retaining an earlier valid winner. generations_completed records the
 * number of child generations promoted. Every successful call records
 * EVO_TERMINATION_GENERATION_LIMIT, EVO_TERMINATION_ALL_INVALID,
 * EVO_TERMINATION_APPLICATION_REQUESTED, EVO_TERMINATION_CONVERGED, or
 * EVO_TERMINATION_STAGNATED. The zero-valued
 * EVO_TERMINATION_NONE is reserved for an unstarted, failed, or destroyed
 * result and is never a successful termination reason. The result also
 * retains one versioned, constant-space statistics record for the most
 * recently committed generation. It does not allocate generation history.
 * Before any run callback, EVO checks max_diversity_work against the
 * all-valid population. Each completed generation records deterministic
 * diversity evidence without consuming operator RNG or changing selection.
 * For positive-limit runs, schema-4 statistics also expose the complete
 * adaptive-mutation decision that follows each commit. The decision consumes
 * no RNG. Its effective rate is passed unchanged to both consumer and
 * reference mutation dispatch in the next attempted transition.
 *
 * Optional fitness-target, patience, and diversity-floor stopping is disabled
 * by the zero-initialized configuration. Enabled policies inspect only the
 * committed global winner and latest generation statistics. Natural reason
 * precedence is all-invalid, converged, stagnated, then generation limit.
 * The application stop callback is considered only when none applies.
 *
 * If generation_stop is non-null, EVO invokes it synchronously after a
 * committed generation only when another child could otherwise be attempted.
 * Returning true stops successfully at that exact committed generation. The
 * callback is never invoked for provisional work or after generation-limit or
 * all-invalid termination is already known.
 *
 * If generation_observer is non-null, EVO invokes it synchronously after
 * generation zero commits and after every successfully promoted child. The
 * callback sees the updated global winner and statistics after any stop
 * decision. Its termination reason is NONE while execution continues and the
 * final reason when the run stops. Failed or provisional generations never
 * produce an event.
 */
evo_status_t evo_run(const evo_problem_t *problem, const evo_config_t *config, void *context, evo_result_t *result);

/**
 * Release the owned genome and reset every result field to zero.
 *
 * This operation is null-safe and repeatable for initialized result objects.
 * It does not securely erase genome bytes before releasing them. Consumers
 * handling secret material require a separately reviewed erasure boundary.
 */
void evo_result_destroy(evo_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
