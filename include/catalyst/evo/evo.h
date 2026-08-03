#ifndef CATALYST_EVO_EVO_H
#define CATALYST_EVO_EVO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVO_VERSION_MAJOR 0
#define EVO_VERSION_MINOR 21
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
    EVO_TERMINATION_APPLICATION_REQUESTED = 3
} evo_termination_reason_t;

#define EVO_FITNESS_COMPARISON_POLICY_VERSION UINT32_C(1)

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

#define EVO_GENERATION_STATISTICS_VERSION UINT32_C(2)

/*
 * Constant-space evidence for one committed generation. fitness_sums is the
 * component-wise sum of valid evaluated candidates in ascending population
 * index. Invalid fitness payloads are excluded. has_best is false, best_index
 * is zero, and best_fitness is zero when valid_count is zero. Successful
 * schema-2 records identify the comparison policy that established best_index.
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

typedef struct evo_problem {
    size_t genome_size;
    /*
     * Generation-zero population initialization receives deterministic bytes
     * supplied by EVO. The callback may transform only its genome, must be
     * deterministic for fixed bytes and context, and may not retain the view.
     */
    void (*initialize)(void *genome, void *context);
    /*
     * Mutation receives one bounded writable genome after EVO selects the
     * configured per-genome event. The callback receives mutation_rate
     * unchanged as its representation-specific intensity, must be
     * deterministic for fixed bytes, rate, and context, may use no unrecorded
     * entropy, and may not change ownership or retain the view.
     */
    void (*mutate)(void *genome, double mutation_rate, void *context);
    /*
     * Crossover receives two bounded read-only parents and two distinct,
     * non-overlapping writable children. It must initialize both children
     * completely, preserve ownership, retain no view, and remain deterministic
     * for fixed parents and context.
     */
    void (*crossover)(const void *parent_a, const void *parent_b, void *child_a, void *child_b, void *context);
    evo_fitness_t (*evaluate)(const void *genome, void *context);
    bool (*is_valid)(const void *genome, void *context);
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
 * An active result is rejected without modification. Other failures,
 * including completion with no valid candidate, leave a non-null, inactive
 * result in its empty, zero-initialized state.
 *
 * A later all-invalid child is promoted and terminates the run successfully
 * while retaining an earlier valid winner. generations_completed records the
 * number of child generations promoted. Every successful call records
 * EVO_TERMINATION_GENERATION_LIMIT, EVO_TERMINATION_ALL_INVALID, or
 * EVO_TERMINATION_APPLICATION_REQUESTED. The zero-valued
 * EVO_TERMINATION_NONE is reserved for an unstarted, failed, or destroyed
 * result and is never a successful termination reason. The result also
 * retains one versioned, constant-space statistics record for the most
 * recently committed generation. It does not allocate generation history.
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
