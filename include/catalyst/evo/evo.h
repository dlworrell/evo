#ifndef CATALYST_EVO_EVO_H
#define CATALYST_EVO_EVO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVO_VERSION_MAJOR 0
#define EVO_VERSION_MINOR 37
#define EVO_VERSION_PATCH 0

typedef enum evo_status {
    EVO_SUCCESS = 0,
    EVO_ERROR_INVALID_ARGUMENT = -1,
    EVO_ERROR_OUT_OF_MEMORY = -2,
    EVO_ERROR_RESULT_ACTIVE = -3,
    EVO_ERROR_RESOURCE_LIMIT = -4,
    EVO_ERROR_STATE = -5,
    EVO_ERROR_EVALUATION = -6,
    EVO_ERROR_NO_VALID_CANDIDATE = -7,
    EVO_ERROR_CHECKPOINT_INVALID = -8,
    EVO_ERROR_CHECKPOINT_INTEGRITY = -9,
    EVO_ERROR_CHECKPOINT_VERSION = -10,
    EVO_ERROR_CHECKPOINT_MISMATCH = -11
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

/*
 * Build-selected implementation used by secure-erasure policy version 1.
 * NONE is the canonical disabled/empty value. The volatile-byte fallback is
 * a reviewed portability boundary, not a claim about allocator or hardware
 * media sanitization.
 */
typedef enum evo_secure_erasure_backend {
    EVO_SECURE_ERASURE_BACKEND_NONE = 0,
    EVO_SECURE_ERASURE_BACKEND_EXPLICIT_BZERO = 1,
    EVO_SECURE_ERASURE_BACKEND_VOLATILE_BYTES = 2
} evo_secure_erasure_backend_t;

#define EVO_FITNESS_COMPARISON_POLICY_VERSION UINT32_C(1)
#define EVO_DIVERSITY_POLICY_VERSION UINT32_C(1)
#define EVO_BYTE_DIVERSITY_METRIC_VERSION UINT32_C(1)
#define EVO_STOPPING_POLICY_VERSION UINT32_C(1)
#define EVO_ELITE_POLICY_VERSION UINT32_C(1)
#define EVO_SELECTION_POLICY_VERSION UINT32_C(1)
#define EVO_BYTE_OPERATOR_POLICY_VERSION UINT32_C(1)
#define EVO_MUTATION_ADAPTATION_POLICY_VERSION UINT32_C(1)
#define EVO_SECURE_ERASURE_POLICY_VERSION UINT32_C(1)
#define EVO_POPULATION_RECYCLING_POLICY_VERSION UINT32_C(1)
#define EVO_POPULATION_STORAGE_REGISTRY_VERSION UINT32_C(1)
#define EVO_POPULATION_STORAGE_OWNER_SLOTS 2
#define EVO_PARALLEL_EVALUATION_POLICY_VERSION UINT32_C(1)
#define EVO_EVALUATION_SCHEDULE_VERSION UINT32_C(1)

typedef enum evo_population_storage_lifecycle {
    EVO_POPULATION_STORAGE_EMPTY = 0,
    EVO_POPULATION_STORAGE_ACTIVE = 1,
    EVO_POPULATION_STORAGE_REUSABLE = 2
} evo_population_storage_lifecycle_t;

typedef enum evo_population_storage_reset_disposition {
    EVO_POPULATION_STORAGE_RESET_NONE = 0,
    EVO_POPULATION_STORAGE_RESET_ZERO_BYTES = 1,
    EVO_POPULATION_STORAGE_RESET_SECURE_ERASE = 2
} evo_population_storage_reset_disposition_t;

/*
 * Address-free audit record for one logical population-storage owner. Owner
 * identities are stable slot identities, never allocator addresses. Entries
 * are ordered by owner_identity in the enclosing registry.
 */
typedef struct evo_population_storage_entry {
    uint64_t owner_identity;
    evo_population_storage_lifecycle_t lifecycle;
    uint64_t population_generation;
    uint64_t source_generation;
    size_t genome_capacity_bytes;
    size_t evaluation_capacity_bytes;
    size_t handoff_count;
    size_t reset_count;
    size_t genome_erasure_count;
    size_t evaluation_erasure_count;
    evo_population_storage_reset_disposition_t last_reset_disposition;
    bool genome_owner_present;
    bool evaluation_owner_present;
} evo_population_storage_entry_t;

/*
 * Human-readable projection of the optional two-slot recycler. Disabled mode
 * is canonical with entry_count and both owner identities zero. Enabled mode
 * orders one or two complete entries by stable owner identity; exactly one is
 * ACTIVE and, after the first transition, exactly one is REUSABLE.
 */
typedef struct evo_population_storage_registry {
    uint32_t version;
    uint32_t policy_version;
    bool recycling_enabled;
    size_t entry_count;
    uint64_t active_owner_identity;
    uint64_t reusable_owner_identity;
    uint32_t secure_erasure_policy_version;
    evo_secure_erasure_backend_t secure_erasure_backend;
    bool secure_erasure_enabled;
    evo_population_storage_entry_t
        entries[EVO_POPULATION_STORAGE_OWNER_SLOTS];
} evo_population_storage_registry_t;

/*
 * Synchronous, non-stopping audit notification after the existing generation
 * observer and before checkpoint delivery. The registry owns no caller-visible
 * storage and the callback must not retain its borrowed address.
 */
typedef void (*evo_population_storage_observer_fn)(
    const evo_population_storage_registry_t *registry,
    void *context);

/*
 * Explicit consumer declaration for the evaluate callback. SERIAL is the
 * zero-valued compatibility contract. THREAD_SAFE permits concurrent calls
 * over distinct read-only genomes with the same caller-owned context.
 */
typedef enum evo_evaluation_callback_thread_safety {
    EVO_EVALUATION_CALLBACK_SERIAL = 0,
    EVO_EVALUATION_CALLBACK_THREAD_SAFE = 1
} evo_evaluation_callback_thread_safety_t;

typedef enum evo_evaluation_assignment_disposition {
    EVO_EVALUATION_NOT_VALIDATED = 0,
    EVO_EVALUATION_EXCLUDED = 1,
    EVO_EVALUATION_PENDING = 2,
    EVO_EVALUATION_COMPLETED = 3,
    EVO_EVALUATION_FAILED = 4,
    EVO_EVALUATION_CANCELED = 5
} evo_evaluation_assignment_disposition_t;

typedef enum evo_evaluation_schedule_outcome {
    EVO_EVALUATION_SCHEDULE_NOT_RUN = 0,
    EVO_EVALUATION_SCHEDULE_COMMITTED = 1,
    EVO_EVALUATION_SCHEDULE_FITNESS_REJECTED = 2,
    EVO_EVALUATION_SCHEDULE_WORKER_START_FAILED = 3,
    EVO_EVALUATION_SCHEDULE_WORKER_JOIN_FAILED = 4
} evo_evaluation_schedule_outcome_t;

/*
 * One explicit candidate-to-worker projection. Worker identities are stable,
 * one-based logical labels; dispatch_wave and commit_order are zero-based.
 * committed distinguishes the first commit from an absent commit-order value.
 */
typedef struct evo_evaluation_assignment {
    size_t population_index;
    size_t worker_identity;
    size_t dispatch_wave;
    size_t commit_order;
    evo_evaluation_assignment_disposition_t disposition;
    bool committed;
} evo_evaluation_assignment_t;

/*
 * Complete, candidate-ordered audit projection for one configured worker
 * evaluation attempt. assignments is borrowed only for the observer call.
 * Runtime queue or thread handles are neither exposed nor authoritative.
 */
typedef struct evo_evaluation_schedule {
    uint32_t version;
    uint32_t policy_version;
    uint64_t population_generation;
    size_t population_size;
    size_t worker_count;
    size_t scratch_bytes;
    size_t validated_count;
    size_t hard_invalid_count;
    size_t scheduled_count;
    size_t completed_count;
    size_t failed_count;
    size_t canceled_count;
    size_t committed_count;
    size_t first_failure_index;
    size_t failed_worker_identity;
    evo_evaluation_schedule_outcome_t outcome;
    const evo_evaluation_assignment_t *assignments;
    size_t assignment_count;
    bool has_failure_index;
    bool complete;
} evo_evaluation_schedule_t;

/*
 * Synchronous audit delivery after every configured worker attempt has joined.
 * Failure schedules are diagnostic only and never publish a generation.
 */
typedef void (*evo_evaluation_schedule_observer_fn)(
    const evo_evaluation_schedule_t *schedule,
    void *context);

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

#define EVO_CHECKPOINT_FORMAT_VERSION UINT32_C(3)
#define EVO_CHECKPOINT_VIEW_VERSION UINT32_C(3)
#define EVO_CHECKPOINT_CONFIGURATION_VIEW_VERSION UINT32_C(3)
#define EVO_CHECKPOINT_CANDIDATE_VIEW_VERSION UINT32_C(1)
#define EVO_CHECKPOINT_INTEGRITY_CRC32 UINT32_C(1)

/*
 * Human-readable projection of every deterministic configuration field bound
 * by checkpoint format 3. Pointer values are never serialized. Callback
 * presence and caller-declared stable problem/context identities stand in for
 * reattached executable and external state.
 */
typedef struct evo_checkpoint_configuration_view {
    uint32_t version;
    size_t genome_size;
    size_t population_size;
    size_t generation_limit;
    size_t tournament_size;
    double crossover_rate;
    double mutation_rate;
    uint64_t random_seed;
    size_t max_genome_bytes;
    size_t max_population_bytes;
    size_t max_evaluation_bytes;
    size_t max_child_population_bytes;
    size_t max_diversity_work;
    bool fitness_target_enabled;
    double fitness_target;
    bool stagnation_enabled;
    double improvement_tolerance;
    size_t stagnation_patience;
    bool diversity_floor_enabled;
    double diversity_floor;
    bool elite_count_enabled;
    size_t elite_count;
    evo_selection_policy_t selection_policy;
    size_t rank_base_weight;
    size_t rank_step_weight;
    evo_crossover_operator_t crossover_operator;
    evo_mutation_operator_t mutation_operator;
    bool adaptive_mutation_enabled;
    double adaptive_mutation_min_rate;
    double adaptive_mutation_max_rate;
    double adaptive_mutation_step;
    double adaptive_mutation_diversity_threshold;
    bool adaptive_mutation_reset_on_improvement;
    bool secure_erasure_enabled;
    uint32_t genome_distance_version;
    uint64_t checkpoint_problem_identity;
    uint64_t checkpoint_context_identity;
    bool initialize_callback_present;
    bool mutate_callback_present;
    bool crossover_callback_present;
    bool evaluate_callback_present;
    bool validity_callback_present;
    bool distance_callback_present;
    bool generation_observer_present;
    bool generation_stop_present;
    bool population_recycling_enabled;
    bool population_storage_observer_present;
    evo_evaluation_callback_thread_safety_t
        evaluation_callback_thread_safety;
    size_t evaluation_worker_count;
    size_t max_evaluation_worker_scratch_bytes;
    bool evaluation_schedule_observer_present;
} evo_checkpoint_configuration_view_t;

/* One explicit candidate in checkpoint population order. */
typedef struct evo_checkpoint_candidate_view {
    uint32_t version;
    size_t population_index;
    const void *genome;
    size_t genome_size;
    evo_fitness_t fitness;
    bool valid;
    bool evaluated;
} evo_checkpoint_candidate_view_t;

/*
 * Ordered, allocation-free audit projection over one validated checkpoint.
 * serialized_checkpoint and all candidate genome views are caller-owned and
 * remain valid only while the original byte range remains unchanged.
 */
typedef struct evo_checkpoint_view {
    uint32_t version;
    uint32_t format_version;
    uint32_t integrity_algorithm;
    uint32_t integrity_value;
    uint64_t configuration_fingerprint;
    const void *serialized_checkpoint;
    size_t serialized_checkpoint_size;
    evo_checkpoint_configuration_view_t configuration;
    uint64_t current_generation;
    evo_termination_reason_t termination_reason;
    size_t population_size;
    size_t valid_count;
    size_t current_best_index;
    bool current_has_best;
    uint64_t global_best_generation;
    size_t global_best_population_index;
    evo_fitness_t global_best_fitness;
    const void *global_best_genome;
    size_t global_best_genome_size;
    evo_generation_statistics_t generation_statistics;
    uint32_t rng_algorithm_version;
    uint32_t operator_seed_schedule_version;
    uint32_t bounded_run_policy_version;
    uint32_t selection_policy_version;
    uint32_t byte_operator_policy_version;
    uint32_t parallel_evaluation_policy_version;
    size_t evaluation_worker_count;
    uint32_t fitness_comparison_policy_version;
    uint32_t diversity_policy_version;
    uint32_t diversity_metric_version;
    uint32_t adaptive_mutation_policy_version;
    double effective_mutation_rate;
    size_t adaptive_mutation_stagnant_generations;
    double significant_best_total;
    size_t stopping_stagnant_generations;
    uint32_t secure_erasure_policy_version;
    evo_secure_erasure_backend_t secure_erasure_backend;
    bool secure_erasure_enabled;
    size_t population_genome_bytes;
    const void *population_genomes;
    size_t population_genome_stride;
    size_t population_evaluation_records;
    size_t population_evaluation_bytes;
    const void *serialized_evaluations;
    size_t serialized_evaluation_record_size;
    evo_population_storage_registry_t population_storage_registry;
} evo_checkpoint_view_t;

/*
 * Synchronous checkpoint delivery after a committed generation's stop and
 * observer callbacks. The caller may copy the bytes but must not retain or
 * modify either borrowed view. No pointer value appears in the checkpoint.
 */
typedef void (*evo_checkpoint_observer_fn)(
    const void *checkpoint,
    size_t checkpoint_size,
    const evo_checkpoint_view_t *view,
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
    /*
     * Fitness evaluation receives one complete read-only genome. Positive
     * evaluation_worker_count may invoke this callback concurrently over
     * independent genomes only when the thread-safety field below opts in.
     */
    evo_fitness_t (*evaluate)(const void *genome, void *context);
    bool (*is_valid)(const void *genome, void *context);
    /* NULL selects built-in byte mismatch metric version 1. */
    evo_genome_distance_fn genome_distance;
    /* Must be nonzero exactly when genome_distance is non-NULL. */
    uint32_t genome_distance_version;
    /* Stable nonzero semantic identity required for checkpoint operations. */
    uint64_t checkpoint_problem_identity;
    /* Explicit concurrency contract for evaluate; zero is serial-only. */
    evo_evaluation_callback_thread_safety_t
        evaluation_callback_thread_safety;
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
    /*
     * Opt in to secure-erasure policy version 1 for every EVO-owned genome
     * and candidate-evaluation allocation. Disabled ordinary release makes
     * no erasure claim and preserves the pre-0.28.0 lifecycle behavior.
     */
    bool secure_erasure_enabled;
    /*
     * Checkpoint format 3 uses a caller-owned scratch buffer and invokes the
     * observer synchronously after each committed generation. A non-NULL
     * observer requires a non-NULL buffer of at least the size reported by
     * evo_checkpoint_size(), bounded by max_checkpoint_bytes. The buffer and
     * callback pointers are reattached runtime resources and are not
     * serialized. max_checkpoint_bytes also bounds untrusted resume input.
     */
    size_t max_checkpoint_bytes;
    void *checkpoint_buffer;
    size_t checkpoint_buffer_size;
    evo_checkpoint_observer_fn checkpoint_observer;
    void *checkpoint_observer_context;
    /* Stable nonzero identity for reattached caller context semantics. */
    uint64_t checkpoint_context_identity;
    /*
     * Opt in to bounded two-slot population-storage recycling. The zero value
     * preserves the pre-0.30.0 allocate/promote/release lifecycle. Enabled
     * mode reuses only run-local owners and never introduces a global pool.
     */
    bool population_recycling_enabled;
    /* Optional address-free owner-registry audit; NULL disables delivery. */
    evo_population_storage_observer_fn population_storage_observer;
    /* Caller-owned audit state, never inspected or retained by EVO. */
    void *population_storage_observer_context;
    /*
     * Zero preserves exact serial evaluation. A positive count no greater
     * than population_size enables fixed-assignment worker policy version 1.
     * max_evaluation_worker_scratch_bytes bounds the sole temporary worker
     * allocation and must cover evo_evaluation_worker_scratch_size().
     */
    size_t evaluation_worker_count;
    size_t max_evaluation_worker_scratch_bytes;
    /* Optional complete candidate-assignment audit for worker attempts. */
    evo_evaluation_schedule_observer_fn evaluation_schedule_observer;
    /* Caller-owned audit state, never inspected or retained by EVO. */
    void *evaluation_schedule_observer_context;
} evo_config_t;

typedef struct evo_result {
    void *best_genome;
    evo_fitness_t best_fitness;
    size_t generations_completed;
    uint64_t random_seed;
    evo_termination_reason_t termination_reason;
    evo_generation_statistics_t generation_statistics;
    /* Stable audit projection for the sole public genome owner. */
    size_t best_genome_size;
    uint32_t secure_erasure_policy_version;
    evo_secure_erasure_backend_t secure_erasure_backend;
    bool secure_erasure_enabled;
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
 * Zero evaluation workers preserve the exact serial validity/evaluation path.
 * A positive bounded count keeps validity serial, invokes only the explicitly
 * thread-safe evaluator in fixed waves, joins every worker, and commits valid
 * records in ascending candidate order. The optional schedule observer receives
 * a complete borrowed assignment/completion/commit projection after join.
 * Runtime thread identity and callback completion timing never affect results.
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

/** Compute the exact format-3 byte count for this population configuration. */
evo_status_t evo_checkpoint_size(const evo_problem_t *problem,
                                 const evo_config_t *config,
                                 size_t *checkpoint_size);

/*
 * Report the exact library scratch allocation required by one configured
 * worker evaluation. Zero workers require zero bytes. No callback, allocation,
 * or thread operation occurs.
 */
evo_status_t evo_evaluation_worker_scratch_size(
    size_t population_size,
    size_t worker_count,
    size_t *scratch_size);

/**
 * Validate an untrusted checkpoint without allocation and return its ordered
 * human-readable projection. Integrity is CRC-32 corruption detection only;
 * it provides neither authentication nor encryption.
 */
evo_status_t evo_checkpoint_inspect(const void *checkpoint,
                                    size_t checkpoint_size,
                                    size_t max_checkpoint_bytes,
                                    evo_checkpoint_view_t *view);

/** Decode one explicit candidate from a previously inspected checkpoint. */
evo_status_t evo_checkpoint_candidate_inspect(
    const evo_checkpoint_view_t *checkpoint,
    size_t population_index,
    evo_checkpoint_candidate_view_t *candidate);

/**
 * Resume from one validated committed-generation checkpoint. The supplied
 * problem, configuration, callbacks, and context are newly attached runtime
 * resources. Their canonical scalar configuration, callback-presence flags,
 * and stable identities must match the checkpoint before allocation, RNG, or
 * callback work. The checkpoint bytes and output result object must be
 * disjoint. A restored generation is never notified a second time.
 */
evo_status_t evo_resume(const evo_problem_t *problem,
                        const evo_config_t *config,
                        void *context,
                        const void *checkpoint,
                        size_t checkpoint_size,
                        evo_result_t *result);

/**
 * Release the owned genome and reset every result field to zero.
 *
 * This operation is null-safe and repeatable for initialized result objects.
 * A result created with secure_erasure_enabled erases exactly
 * best_genome_size bytes through the recorded policy/backend immediately
 * before release. A result created without that opt-in uses ordinary release
 * and makes no claim that allocator, operating-system, or hardware copies are
 * scrubbed. Callers must not modify the result's owner or erasure metadata.
 */
void evo_result_destroy(evo_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
