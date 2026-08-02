#ifndef CATALYST_EVO_EVO_H
#define CATALYST_EVO_EVO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVO_VERSION_MAJOR 0
#define EVO_VERSION_MINOR 18
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
    EVO_TERMINATION_ALL_INVALID = 2
} evo_termination_reason_t;

typedef struct evo_fitness {
    double correctness;
    double performance;
    double memory_use;
    double reliability;
    double maintainability;
    double constraint_penalty;
    double total;
} evo_fitness_t;

#define EVO_GENERATION_STATISTICS_VERSION UINT32_C(1)

/*
 * Constant-space evidence for one committed generation. fitness_sums is the
 * component-wise sum of valid evaluated candidates in ascending population
 * index. Invalid fitness payloads are excluded. has_best is false, best_index
 * is zero, and best_fitness is zero when valid_count is zero.
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
} evo_generation_statistics_t;

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
 * transfers an independent copy of the highest-total valid candidate observed
 * across the run together with its complete fitness evidence. Exact ties
 * preserve the earlier generation, while ties within one generation preserve
 * the lower population index.
 *
 * An active result is rejected without modification. Other failures,
 * including completion with no valid candidate, leave a non-null, inactive
 * result in its empty, zero-initialized state.
 *
 * A later all-invalid child is promoted and terminates the run successfully
 * while retaining an earlier valid winner. generations_completed records the
 * number of child generations promoted. Every successful call records either
 * EVO_TERMINATION_GENERATION_LIMIT or EVO_TERMINATION_ALL_INVALID. The
 * zero-valued EVO_TERMINATION_NONE is reserved for an unstarted, failed, or
 * destroyed result and is never a successful termination reason. The result
 * also retains one versioned, constant-space statistics record for the most
 * recently committed generation. It does not allocate generation history.
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
