#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif

#include "catalyst/evo/evo.h"
#include "internal/population_storage.h"
#include "internal/rng.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>

#ifndef EVO_BENCHMARK_COMMIT
#define EVO_BENCHMARK_COMMIT "local-tree"
#endif

#ifndef EVO_BENCHMARK_LINKER_ID
#define EVO_BENCHMARK_LINKER_ID "build-default"
#endif

#ifndef EVO_BENCHMARK_BUILD_FRONTEND
#define EVO_BENCHMARK_BUILD_FRONTEND "direct-compiler"
#endif

#ifndef EVO_BENCHMARK_BUILD_PROFILE
#define EVO_BENCHMARK_BUILD_PROFILE "unspecified"
#endif

#define EVO_BENCHMARK_SCHEMA_VERSION "1.0.0"
#define EVO_BENCHMARK_ID "EVO-CORE-001"
#define EVO_BENCHMARK_GENOME_SIZE ((size_t)16)
#define EVO_BENCHMARK_POPULATION_SIZE ((size_t)32)
#define EVO_BENCHMARK_GENERATION_LIMIT ((size_t)12)
#define EVO_BENCHMARK_TRACE_CAPACITY \
    (EVO_BENCHMARK_GENERATION_LIMIT + (size_t)1)
#define EVO_BENCHMARK_MODE_COUNT ((size_t)4)
#define EVO_BENCHMARK_SMOKE_SEED_COUNT ((size_t)3)
#define EVO_BENCHMARK_EXTENDED_SEED_COUNT ((size_t)16)
#define EVO_BENCHMARK_MAX_SEED_COUNT EVO_BENCHMARK_EXTENDED_SEED_COUNT
#define EVO_BENCHMARK_SMOKE_REPETITIONS ((size_t)3)
#define EVO_BENCHMARK_EXTENDED_REPETITIONS ((size_t)15)
#define EVO_BENCHMARK_MAX_REPETITIONS EVO_BENCHMARK_EXTENDED_REPETITIONS
#define EVO_BENCHMARK_PARALLEL_WORKERS ((size_t)4)
#define EVO_BENCHMARK_DIVERSITY_SCALE UINT64_C(1000000)

typedef enum benchmark_tier {
    BENCHMARK_TIER_SMOKE = 0,
    BENCHMARK_TIER_EXTENDED = 1
} benchmark_tier_t;

typedef struct benchmark_options {
    benchmark_tier_t tier;
    const char *commit;
    const char *linker;
} benchmark_options_t;

typedef struct generation_record {
    uint64_t generation_index;
    size_t valid_count;
    size_t invalid_count;
    size_t generation_best_index;
    double generation_best_total;
    double global_best_total;
    size_t diversity_pair_count;
    size_t diversity_work_units;
    double diversity;
    double mutation_rate_prior;
    double mutation_rate_effective;
    size_t mutation_stagnant_generations;
    evo_mutation_adaptation_reason_t mutation_adaptation_reason;
} generation_record_t;

typedef struct run_capture {
    generation_record_t records[EVO_BENCHMARK_TRACE_CAPACITY];
    size_t count;
} run_capture_t;

typedef struct semantic_result {
    unsigned char best_genome[EVO_BENCHMARK_GENOME_SIZE];
    evo_fitness_t best_fitness;
    size_t generations_completed;
    uint64_t random_seed;
    evo_termination_reason_t termination_reason;
    evo_generation_statistics_t final_statistics;
    generation_record_t trace[EVO_BENCHMARK_TRACE_CAPACITY];
    size_t trace_count;
} semantic_result_t;

typedef struct timing_sample {
    uint64_t wall_time_ns;
    uintmax_t cpu_clock_ticks;
    bool matches_seed_reference;
    bool matches_serial_reference;
} timing_sample_t;

typedef struct seed_record {
    uint64_t seed;
    semantic_result_t reference_result;
    timing_sample_t samples[EVO_BENCHMARK_MAX_REPETITIONS];
    size_t sample_count;
    bool oracle_applicable;
    bool oracle_passed;
    bool cross_mode_passed;
} seed_record_t;

typedef struct mode_definition {
    const char *id;
    bool parallel;
    bool recycling;
} mode_definition_t;

typedef struct case_record {
    mode_definition_t mode;
    size_t worker_count;
    size_t worker_scratch_bytes;
    size_t requested_heap_peak_bytes;
    size_t requested_heap_total_bytes;
    seed_record_t seeds[EVO_BENCHMARK_MAX_SEED_COUNT];
    size_t seed_count;
    bool correctness_passed;
} case_record_t;

typedef struct benchmark_suite {
    benchmark_options_t options;
    size_t warmup_runs;
    size_t repetitions;
    size_t seed_count;
    case_record_t cases[EVO_BENCHMARK_MODE_COUNT];
    uint64_t process_peak_resident_native;
    const char *process_peak_resident_unit;
    uint64_t record_locator;
    bool correctness_passed;
} benchmark_suite_t;

/*
 * The expected vectors are intentionally explicit. Their fields, rather than
 * the informational FNV record locator, are the correctness authority.
 */
typedef struct benchmark_oracle {
    uint64_t seed;
    unsigned char best_genome[EVO_BENCHMARK_GENOME_SIZE];
    double best_total;
    double global_best_totals[EVO_BENCHMARK_TRACE_CAPACITY];
    double generation_best_totals[EVO_BENCHMARK_TRACE_CAPACITY];
    uint64_t diversity_scaled[EVO_BENCHMARK_TRACE_CAPACITY];
} benchmark_oracle_t;

static const uint64_t smoke_seeds[EVO_BENCHMARK_SMOKE_SEED_COUNT] = {
    UINT64_C(0),
    UINT64_C(42),
    UINT64_MAX,
};

static const uint64_t extended_seeds[EVO_BENCHMARK_EXTENDED_SEED_COUNT] = {
    UINT64_C(0),
    UINT64_C(1),
    UINT64_C(2),
    UINT64_C(3),
    UINT64_C(7),
    UINT64_C(11),
    UINT64_C(17),
    UINT64_C(23),
    UINT64_C(42),
    UINT64_C(99),
    UINT64_C(255),
    UINT64_C(1024),
    UINT64_C(65537),
    UINT64_C(0x0123456789abcdef),
    UINT64_C(0xfedcba9876543210),
    UINT64_MAX,
};

static const mode_definition_t mode_definitions[EVO_BENCHMARK_MODE_COUNT] = {
    {"serial-allocate", false, false},
    {"serial-recycle", false, true},
    {"parallel-allocate", true, false},
    {"parallel-recycle", true, true},
};

static const benchmark_oracle_t benchmark_oracles[] = {
    {
        .seed = UINT64_C(0),
        .best_genome = {
            0xf1,
            0xdb,
            0xaf,
            0xbb,
            0xbb,
            0xbe,
            0xff,
            0xfd,
            0x73,
            0xb7,
            0xbb,
            0xff,
            0xfb,
            0xaf,
            0xf7,
            0x7b,
        },
        .best_total = 101.0,
        .global_best_totals = {
            76.0,
            78.0,
            80.0,
            84.0,
            85.0,
            85.0,
            92.0,
            92.0,
            94.0,
            94.0,
            96.0,
            96.0,
            101.0,
        },
        .generation_best_totals = {
            76.0,
            78.0,
            80.0,
            84.0,
            85.0,
            85.0,
            92.0,
            92.0,
            94.0,
            94.0,
            96.0,
            96.0,
            101.0,
        },
        .diversity_scaled = {
            994330,
            962072,
            910156,
            844128,
            737399,
            577747,
            510837,
            456149,
            442288,
            393523,
            375882,
            318170,
            342238,
        },
    },
    {
        .seed = UINT64_C(42),
        .best_genome = {
            0xfb,
            0xfd,
            0xc7,
            0xf5,
            0x76,
            0xbf,
            0xbe,
            0xe5,
            0xf5,
            0x1b,
            0xf5,
            0xad,
            0xf7,
            0xad,
            0xef,
            0x6f,
        },
        .best_total = 94.0,
        .global_best_totals = {
            78.0,
            81.0,
            81.0,
            84.0,
            85.0,
            89.0,
            90.0,
            90.0,
            90.0,
            91.0,
            94.0,
            94.0,
            94.0,
        },
        .generation_best_totals = {
            78.0,
            81.0,
            81.0,
            84.0,
            85.0,
            89.0,
            90.0,
            90.0,
            90.0,
            91.0,
            94.0,
            94.0,
            94.0,
        },
        .diversity_scaled = {
            996094,
            960181,
            926537,
            853705,
            808720,
            669733,
            650328,
            611265,
            605595,
            572455,
            505922,
            442288,
            392515,
        },
    },
    {
        .seed = UINT64_MAX,
        .best_genome = {
            0xfe,
            0xd7,
            0x3d,
            0xbb,
            0xdb,
            0xb6,
            0xfe,
            0xae,
            0xdb,
            0xfa,
            0xdb,
            0xfc,
            0xdb,
            0xf2,
            0x27,
            0xcd,
        },
        .best_total = 91.0,
        .global_best_totals = {
            74.0,
            74.0,
            76.0,
            76.0,
            77.0,
            82.0,
            82.0,
            83.0,
            85.0,
            87.0,
            88.0,
            88.0,
            91.0,
        },
        .generation_best_totals = {
            74.0,
            74.0,
            76.0,
            76.0,
            77.0,
            82.0,
            82.0,
            83.0,
            85.0,
            87.0,
            88.0,
            88.0,
            91.0,
        },
        .diversity_scaled = {
            996094,
            976310,
            946951,
            853831,
            730091,
            581905,
            571069,
            609375,
            630292,
            624370,
            616305,
            576487,
            572707,
        },
    },
};

static benchmark_suite_t suite;

static bool checked_size_add(size_t left, size_t right, size_t *sum)
{
    if (sum == NULL || right > SIZE_MAX - left) {
        return false;
    }
    *sum = left + right;
    return true;
}

static bool checked_size_multiply(size_t left,
                                  size_t right,
                                  size_t *product)
{
    if (product == NULL || (left != 0 && right > SIZE_MAX / left)) {
        return false;
    }
    *product = left * right;
    return true;
}

static bool bounded_string_is_valid(const char *value, size_t maximum)
{
    size_t length = 0;

    if (value == NULL || value[0] == '\0') {
        return false;
    }
    while (value[length] != '\0') {
        if (length == maximum) {
            return false;
        }
        ++length;
    }
    return true;
}

static bool metadata_string_is_valid(const char *value, size_t maximum)
{
    const unsigned char *bytes = (const unsigned char *)value;

    if (!bounded_string_is_valid(value, maximum)) {
        return false;
    }
    for (size_t index = 0; bytes[index] != 0; ++index) {
        if (bytes[index] < 0x20u || bytes[index] > 0x7eu) {
            return false;
        }
    }
    return true;
}

static const char *tier_name(benchmark_tier_t tier)
{
    return tier == BENCHMARK_TIER_EXTENDED ? "extended" : "smoke";
}

static const char *termination_name(evo_termination_reason_t reason)
{
    switch (reason) {
    case EVO_TERMINATION_NONE:
        return "none";
    case EVO_TERMINATION_GENERATION_LIMIT:
        return "generation-limit";
    case EVO_TERMINATION_ALL_INVALID:
        return "all-invalid";
    case EVO_TERMINATION_APPLICATION_REQUESTED:
        return "application-requested";
    case EVO_TERMINATION_CONVERGED:
        return "converged";
    case EVO_TERMINATION_STAGNATED:
        return "stagnated";
    default:
        return "invalid";
    }
}

static const char *adaptation_reason_name(
    evo_mutation_adaptation_reason_t reason)
{
    switch (reason) {
    case EVO_MUTATION_ADAPTATION_NOT_APPLICABLE:
        return "not-applicable";
    case EVO_MUTATION_ADAPTATION_DISABLED:
        return "disabled";
    case EVO_MUTATION_ADAPTATION_INITIAL:
        return "initial";
    case EVO_MUTATION_ADAPTATION_LOW_DIVERSITY:
        return "low-diversity";
    case EVO_MUTATION_ADAPTATION_STAGNATION:
        return "stagnation";
    case EVO_MUTATION_ADAPTATION_STAGNATION_LOW_DIVERSITY:
        return "stagnation-low-diversity";
    case EVO_MUTATION_ADAPTATION_IMPROVEMENT_RESET:
        return "improvement-reset";
    case EVO_MUTATION_ADAPTATION_IMPROVEMENT_HOLD:
        return "improvement-hold";
    default:
        return "invalid";
    }
}

static const char *compiler_family(void)
{
#if defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#else
    return "unknown";
#endif
}

static const char *platform_family(void)
{
#if defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

static const char *architecture_family(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#else
    return "unknown";
#endif
}

static void initialize_genome(void *genome, void *context)
{
    (void)genome;
    (void)context;
}

static bool genome_is_valid(const void *genome, void *context)
{
    (void)genome;
    (void)context;
    return true;
}

static unsigned int one_bits(unsigned char value)
{
    unsigned int count = 0;

    while (value != 0) {
        count += (unsigned int)(value & (unsigned char)1);
        value = (unsigned char)(value >> 1u);
    }
    return count;
}

static evo_fitness_t evaluate_genome(const void *genome, void *context)
{
    const unsigned char *bytes = genome;
    unsigned int score = 0;

    (void)context;
    for (size_t index = 0; index < EVO_BENCHMARK_GENOME_SIZE; ++index) {
        score += one_bits(bytes[index]);
    }
    return (evo_fitness_t){
        .correctness = (double)score,
        .total = (double)score,
    };
}

static void observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    run_capture_t *capture = context;
    generation_record_t *record = NULL;

    if (capture == NULL || result == NULL || statistics == NULL ||
        capture->count >= EVO_BENCHMARK_TRACE_CAPACITY) {
        return;
    }
    record = &capture->records[capture->count];
    *record = (generation_record_t){
        .generation_index = statistics->generation_index,
        .valid_count = statistics->valid_count,
        .invalid_count = statistics->invalid_count,
        .generation_best_index = statistics->best_index,
        .generation_best_total = statistics->best_fitness.total,
        .global_best_total = result->best_fitness.total,
        .diversity_pair_count = statistics->diversity_pair_count,
        .diversity_work_units = statistics->diversity_work_units,
        .diversity = statistics->diversity,
        .mutation_rate_prior = statistics->mutation_rate_prior,
        .mutation_rate_effective = statistics->mutation_rate_effective,
        .mutation_stagnant_generations =
            statistics->adaptive_mutation_stagnant_generations,
        .mutation_adaptation_reason =
            statistics->mutation_adaptation_reason,
    };
    ++capture->count;
}

static bool monotonic_time_ns(uint64_t *nanoseconds)
{
    struct timespec value = {0};
    uint64_t seconds = 0;
    uint64_t nanos = 0;

    if (nanoseconds == NULL ||
        clock_gettime(CLOCK_MONOTONIC, &value) != 0 || value.tv_sec < 0 ||
        value.tv_nsec < 0 || value.tv_nsec >= 1000000000L) {
        return false;
    }
    seconds = (uint64_t)value.tv_sec;
    nanos = (uint64_t)value.tv_nsec;
    if (seconds > (UINT64_MAX - nanos) / UINT64_C(1000000000)) {
        return false;
    }
    *nanoseconds = seconds * UINT64_C(1000000000) + nanos;
    return true;
}

static bool fitness_equal(const evo_fitness_t *left,
                          const evo_fitness_t *right)
{
    return left->correctness == right->correctness &&
           left->performance == right->performance &&
           left->memory_use == right->memory_use &&
           left->reliability == right->reliability &&
           left->maintainability == right->maintainability &&
           left->constraint_penalty == right->constraint_penalty &&
           left->total == right->total;
}

static bool statistics_equal(const evo_generation_statistics_t *left,
                             const evo_generation_statistics_t *right)
{
    return left->version == right->version &&
           left->generation_index == right->generation_index &&
           left->population_size == right->population_size &&
           left->valid_count == right->valid_count &&
           left->invalid_count == right->invalid_count &&
           left->best_index == right->best_index &&
           fitness_equal(&left->best_fitness, &right->best_fitness) &&
           fitness_equal(&left->fitness_sums, &right->fitness_sums) &&
           left->has_best == right->has_best &&
           left->fitness_comparison_policy_version ==
               right->fitness_comparison_policy_version &&
           left->diversity_policy_version ==
               right->diversity_policy_version &&
           left->diversity_metric_version ==
               right->diversity_metric_version &&
           left->diversity_pair_count == right->diversity_pair_count &&
           left->diversity_work_units == right->diversity_work_units &&
           left->diversity == right->diversity &&
           left->diversity_uses_domain_distance ==
               right->diversity_uses_domain_distance &&
           left->adaptive_mutation_policy_version ==
               right->adaptive_mutation_policy_version &&
           left->mutation_rate_prior == right->mutation_rate_prior &&
           left->mutation_rate_effective ==
               right->mutation_rate_effective &&
           left->adaptive_mutation_min_rate ==
               right->adaptive_mutation_min_rate &&
           left->adaptive_mutation_max_rate ==
               right->adaptive_mutation_max_rate &&
           left->adaptive_mutation_step ==
               right->adaptive_mutation_step &&
           left->adaptive_mutation_diversity_threshold ==
               right->adaptive_mutation_diversity_threshold &&
           left->adaptive_mutation_stagnant_generations ==
               right->adaptive_mutation_stagnant_generations &&
           left->mutation_adaptation_reason ==
               right->mutation_adaptation_reason &&
           left->adaptive_mutation_enabled ==
               right->adaptive_mutation_enabled &&
           left->adaptive_mutation_low_diversity ==
               right->adaptive_mutation_low_diversity &&
           left->adaptive_mutation_global_best_improved ==
               right->adaptive_mutation_global_best_improved &&
           left->adaptive_mutation_clamped_to_min ==
               right->adaptive_mutation_clamped_to_min &&
           left->adaptive_mutation_clamped_to_max ==
               right->adaptive_mutation_clamped_to_max &&
           left->adaptive_mutation_reset_on_improvement ==
               right->adaptive_mutation_reset_on_improvement;
}

static bool generation_record_equal(const generation_record_t *left,
                                    const generation_record_t *right)
{
    return left->generation_index == right->generation_index &&
           left->valid_count == right->valid_count &&
           left->invalid_count == right->invalid_count &&
           left->generation_best_index == right->generation_best_index &&
           left->generation_best_total == right->generation_best_total &&
           left->global_best_total == right->global_best_total &&
           left->diversity_pair_count == right->diversity_pair_count &&
           left->diversity_work_units == right->diversity_work_units &&
           left->diversity == right->diversity &&
           left->mutation_rate_prior == right->mutation_rate_prior &&
           left->mutation_rate_effective ==
               right->mutation_rate_effective &&
           left->mutation_stagnant_generations ==
               right->mutation_stagnant_generations &&
           left->mutation_adaptation_reason ==
               right->mutation_adaptation_reason;
}

static bool semantic_result_equal(const semantic_result_t *left,
                                  const semantic_result_t *right)
{
    if (!fitness_equal(&left->best_fitness, &right->best_fitness) ||
        left->generations_completed != right->generations_completed ||
        left->random_seed != right->random_seed ||
        left->termination_reason != right->termination_reason ||
        !statistics_equal(&left->final_statistics,
                          &right->final_statistics) ||
        left->trace_count != right->trace_count) {
        return false;
    }
    for (size_t index = 0; index < EVO_BENCHMARK_GENOME_SIZE; ++index) {
        if (left->best_genome[index] != right->best_genome[index]) {
            return false;
        }
    }
    for (size_t index = 0; index < left->trace_count; ++index) {
        if (!generation_record_equal(&left->trace[index],
                                     &right->trace[index])) {
            return false;
        }
    }
    return true;
}

static bool capture_semantic_result(const evo_result_t *result,
                                    const run_capture_t *capture,
                                    semantic_result_t *semantic)
{
    if (result == NULL || capture == NULL || semantic == NULL ||
        result->best_genome == NULL ||
        result->best_genome_size != EVO_BENCHMARK_GENOME_SIZE ||
        capture->count != EVO_BENCHMARK_TRACE_CAPACITY) {
        return false;
    }
    *semantic = (semantic_result_t){
        .best_fitness = result->best_fitness,
        .generations_completed = result->generations_completed,
        .random_seed = result->random_seed,
        .termination_reason = result->termination_reason,
        .final_statistics = result->generation_statistics,
        .trace_count = capture->count,
    };
    for (size_t index = 0; index < EVO_BENCHMARK_GENOME_SIZE; ++index) {
        semantic->best_genome[index] =
            ((const unsigned char *)result->best_genome)[index];
    }
    for (size_t index = 0; index < capture->count; ++index) {
        semantic->trace[index] = capture->records[index];
    }
    return true;
}

static evo_problem_t make_problem(void)
{
    return (evo_problem_t){
        .genome_size = EVO_BENCHMARK_GENOME_SIZE,
        .initialize = initialize_genome,
        .evaluate = evaluate_genome,
        .is_valid = genome_is_valid,
        .evaluation_callback_thread_safety =
            EVO_EVALUATION_CALLBACK_THREAD_SAFE,
    };
}

static bool make_config(const mode_definition_t *mode,
                        uint64_t seed,
                        run_capture_t *capture,
                        evo_config_t *config,
                        size_t *worker_scratch_bytes)
{
    size_t genome_bytes = 0;
    size_t evaluation_bytes = 0;
    size_t diversity_pairs = 0;
    size_t diversity_work = 0;
    size_t scratch_bytes = 0;
    const size_t workers =
        mode->parallel ? EVO_BENCHMARK_PARALLEL_WORKERS : 0;

    if (!checked_size_multiply(EVO_BENCHMARK_POPULATION_SIZE,
                               EVO_BENCHMARK_GENOME_SIZE,
                               &genome_bytes) ||
        !checked_size_multiply(EVO_BENCHMARK_POPULATION_SIZE,
                               sizeof(evo_candidate_evaluation_t),
                               &evaluation_bytes) ||
        !checked_size_multiply(EVO_BENCHMARK_POPULATION_SIZE,
                               EVO_BENCHMARK_POPULATION_SIZE - 1,
                               &diversity_pairs)) {
        return false;
    }
    diversity_pairs /= 2;
    if (!checked_size_multiply(diversity_pairs,
                               EVO_BENCHMARK_GENOME_SIZE,
                               &diversity_work) ||
        evo_evaluation_worker_scratch_size(
            EVO_BENCHMARK_POPULATION_SIZE, workers, &scratch_bytes) !=
            EVO_SUCCESS) {
        return false;
    }

    *config = (evo_config_t){
        .population_size = EVO_BENCHMARK_POPULATION_SIZE,
        .generation_limit = EVO_BENCHMARK_GENERATION_LIMIT,
        .tournament_size = 0,
        .crossover_rate = 0.75,
        .mutation_rate = 0.35,
        .random_seed = seed,
        .max_genome_bytes = EVO_BENCHMARK_GENOME_SIZE,
        .max_population_bytes = genome_bytes,
        .max_evaluation_bytes = evaluation_bytes,
        .max_child_population_bytes = genome_bytes,
        .generation_observer = observe_generation,
        .generation_observer_context = capture,
        .max_diversity_work = diversity_work,
        .elite_count_enabled = true,
        .elite_count = 2,
        .selection_policy = EVO_SELECTION_RANK,
        .rank_base_weight = 1,
        .rank_step_weight = 1,
        .crossover_operator = EVO_CROSSOVER_BYTE_UNIFORM,
        .mutation_operator = EVO_MUTATION_BYTE_XOR,
        .adaptive_mutation_enabled = true,
        .adaptive_mutation_min_rate = 0.10,
        .adaptive_mutation_max_rate = 0.80,
        .adaptive_mutation_step = 0.10,
        .adaptive_mutation_diversity_threshold = 0.35,
        .adaptive_mutation_reset_on_improvement = true,
        .population_recycling_enabled = mode->recycling,
        .evaluation_worker_count = workers,
        .max_evaluation_worker_scratch_bytes = scratch_bytes,
    };
    *worker_scratch_bytes = scratch_bytes;
    return true;
}

static bool run_once(const mode_definition_t *mode,
                     uint64_t seed,
                     bool measure,
                     semantic_result_t *semantic,
                     timing_sample_t *sample)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = {0};
    evo_result_t result = {0};
    run_capture_t capture = {0};
    size_t worker_scratch_bytes = 0;
    uint64_t wall_start = 0;
    uint64_t wall_end = 0;
    clock_t cpu_start = 0;
    clock_t cpu_end = 0;
    bool succeeded = false;

    if (semantic == NULL ||
        !make_config(mode,
                     seed,
                     &capture,
                     &config,
                     &worker_scratch_bytes)) {
        return false;
    }
    (void)worker_scratch_bytes;
    if (measure) {
        if (!monotonic_time_ns(&wall_start)) {
            return false;
        }
        cpu_start = clock();
        if (cpu_start == (clock_t)-1) {
            return false;
        }
    }

    if (evo_run(&problem, &config, &capture, &result) == EVO_SUCCESS &&
        result.generations_completed == EVO_BENCHMARK_GENERATION_LIMIT &&
        result.termination_reason == EVO_TERMINATION_GENERATION_LIMIT &&
        capture_semantic_result(&result, &capture, semantic)) {
        succeeded = true;
    }

    if (measure && succeeded) {
        cpu_end = clock();
        if (!monotonic_time_ns(&wall_end) || cpu_end == (clock_t)-1 ||
            wall_end < wall_start || cpu_end < cpu_start || sample == NULL) {
            succeeded = false;
        } else {
            sample->wall_time_ns = wall_end - wall_start;
            sample->cpu_clock_ticks = (uintmax_t)(cpu_end - cpu_start);
        }
    }
    evo_result_destroy(&result);
    return succeeded;
}

static uint64_t scaled_diversity(double value)
{
    const double scaled = value * (double)EVO_BENCHMARK_DIVERSITY_SCALE;

    if (value < 0.0 || value > 1.0 ||
        scaled > (double)UINT64_MAX - 0.5) {
        return UINT64_MAX;
    }
    return (uint64_t)(scaled + 0.5);
}

static const benchmark_oracle_t *find_oracle(uint64_t seed)
{
    for (size_t index = 0;
         index < sizeof(benchmark_oracles) / sizeof(benchmark_oracles[0]);
         ++index) {
        if (benchmark_oracles[index].seed == seed) {
            return &benchmark_oracles[index];
        }
    }
    return NULL;
}

static bool result_matches_oracle(const semantic_result_t *result,
                                  const benchmark_oracle_t *oracle)
{
    if (result == NULL || oracle == NULL ||
        result->random_seed != oracle->seed ||
        result->best_fitness.total != oracle->best_total ||
        result->generations_completed != EVO_BENCHMARK_GENERATION_LIMIT ||
        result->termination_reason != EVO_TERMINATION_GENERATION_LIMIT ||
        result->trace_count != EVO_BENCHMARK_TRACE_CAPACITY) {
        return false;
    }
    for (size_t index = 0; index < EVO_BENCHMARK_GENOME_SIZE; ++index) {
        if (result->best_genome[index] != oracle->best_genome[index]) {
            return false;
        }
    }
    for (size_t index = 0; index < result->trace_count; ++index) {
        if (result->trace[index].global_best_total !=
                oracle->global_best_totals[index] ||
            result->trace[index].generation_best_total !=
                oracle->generation_best_totals[index] ||
            scaled_diversity(result->trace[index].diversity) !=
                oracle->diversity_scaled[index]) {
            return false;
        }
    }
    return true;
}

static bool memory_model(const mode_definition_t *mode,
                         size_t worker_scratch_bytes,
                         size_t *peak_bytes,
                         size_t *total_bytes)
{
    size_t genome_bytes = 0;
    size_t evaluation_bytes = 0;
    size_t owner_bytes = 0;
    size_t peak_owner_bytes = 0;
    size_t scratch_total = 0;
    size_t owner_allocation_count = 0;
    size_t owner_total = 0;
    size_t peak = 0;
    size_t total = 0;

    if (!checked_size_multiply(EVO_BENCHMARK_POPULATION_SIZE,
                               EVO_BENCHMARK_GENOME_SIZE,
                               &genome_bytes) ||
        !checked_size_multiply(EVO_BENCHMARK_POPULATION_SIZE,
                               sizeof(evo_candidate_evaluation_t),
                               &evaluation_bytes) ||
        !checked_size_add(genome_bytes, evaluation_bytes, &owner_bytes) ||
        !checked_size_multiply(owner_bytes, 2, &peak_owner_bytes) ||
        !checked_size_add(peak_owner_bytes,
                          EVO_BENCHMARK_GENOME_SIZE,
                          &peak) ||
        !checked_size_add(peak, worker_scratch_bytes, &peak)) {
        return false;
    }

    owner_allocation_count = mode->recycling
                                 ? 2
                                 : EVO_BENCHMARK_GENERATION_LIMIT + 1;
    if (!checked_size_multiply(owner_allocation_count,
                               owner_bytes,
                               &owner_total) ||
        !checked_size_multiply(EVO_BENCHMARK_GENERATION_LIMIT + 1,
                               worker_scratch_bytes,
                               &scratch_total) ||
        !checked_size_add(owner_total,
                          EVO_BENCHMARK_GENOME_SIZE,
                          &total) ||
        !checked_size_add(total, scratch_total, &total)) {
        return false;
    }
    *peak_bytes = peak;
    *total_bytes = total;
    return true;
}

static bool execute_suite(benchmark_suite_t *benchmark)
{
    const uint64_t *seeds = benchmark->options.tier == BENCHMARK_TIER_EXTENDED
                                ? extended_seeds
                                : smoke_seeds;
    semantic_result_t serial_references[EVO_BENCHMARK_MAX_SEED_COUNT] = {0};
    struct rusage usage = {0};

    benchmark->seed_count =
        benchmark->options.tier == BENCHMARK_TIER_EXTENDED
            ? EVO_BENCHMARK_EXTENDED_SEED_COUNT
            : EVO_BENCHMARK_SMOKE_SEED_COUNT;
    benchmark->warmup_runs =
        benchmark->options.tier == BENCHMARK_TIER_EXTENDED ? 3 : 1;
    benchmark->repetitions =
        benchmark->options.tier == BENCHMARK_TIER_EXTENDED
            ? EVO_BENCHMARK_EXTENDED_REPETITIONS
            : EVO_BENCHMARK_SMOKE_REPETITIONS;
    benchmark->correctness_passed = true;

    for (size_t mode_index = 0;
         mode_index < EVO_BENCHMARK_MODE_COUNT;
         ++mode_index) {
        case_record_t *case_record = &benchmark->cases[mode_index];

        case_record->mode = mode_definitions[mode_index];
        case_record->worker_count = case_record->mode.parallel
                                        ? EVO_BENCHMARK_PARALLEL_WORKERS
                                        : 0;
        if (evo_evaluation_worker_scratch_size(
                EVO_BENCHMARK_POPULATION_SIZE,
                case_record->worker_count,
                &case_record->worker_scratch_bytes) != EVO_SUCCESS ||
            !memory_model(&case_record->mode,
                          case_record->worker_scratch_bytes,
                          &case_record->requested_heap_peak_bytes,
                          &case_record->requested_heap_total_bytes)) {
            return false;
        }
        case_record->seed_count = benchmark->seed_count;
        case_record->correctness_passed = true;

        for (size_t seed_index = 0;
             seed_index < benchmark->seed_count;
             ++seed_index) {
            seed_record_t *seed_record = &case_record->seeds[seed_index];
            const benchmark_oracle_t *oracle = find_oracle(seeds[seed_index]);

            seed_record->seed = seeds[seed_index];
            seed_record->oracle_applicable = oracle != NULL;
            seed_record->oracle_passed = oracle == NULL;
            seed_record->cross_mode_passed = true;

            for (size_t warmup = 0;
                 warmup < benchmark->warmup_runs;
                 ++warmup) {
                semantic_result_t warmup_result = {0};

                if (!run_once(&case_record->mode,
                              seed_record->seed,
                              false,
                              &warmup_result,
                              NULL)) {
                    return false;
                }
                if (warmup == 0) {
                    seed_record->reference_result = warmup_result;
                } else if (!semantic_result_equal(
                               &seed_record->reference_result,
                               &warmup_result)) {
                    seed_record->cross_mode_passed = false;
                }
            }

            if (oracle != NULL) {
                seed_record->oracle_passed = result_matches_oracle(
                    &seed_record->reference_result, oracle);
            }
            if (mode_index == 0) {
                serial_references[seed_index] =
                    seed_record->reference_result;
            } else if (!semantic_result_equal(
                           &serial_references[seed_index],
                           &seed_record->reference_result)) {
                seed_record->cross_mode_passed = false;
            }

            for (size_t repetition = 0;
                 repetition < benchmark->repetitions;
                 ++repetition) {
                semantic_result_t measured_result = {0};
                timing_sample_t *sample = &seed_record->samples[repetition];

                if (!run_once(&case_record->mode,
                              seed_record->seed,
                              true,
                              &measured_result,
                              sample)) {
                    return false;
                }
                sample->matches_seed_reference = semantic_result_equal(
                    &seed_record->reference_result, &measured_result);
                sample->matches_serial_reference = semantic_result_equal(
                    &serial_references[seed_index], &measured_result);
                if (!sample->matches_seed_reference ||
                    !sample->matches_serial_reference) {
                    seed_record->cross_mode_passed = false;
                }
                ++seed_record->sample_count;
            }

            if (!seed_record->oracle_passed ||
                !seed_record->cross_mode_passed) {
                case_record->correctness_passed = false;
                benchmark->correctness_passed = false;
            }
        }
    }

    if (getrusage(RUSAGE_SELF, &usage) == 0 && usage.ru_maxrss >= 0) {
        benchmark->process_peak_resident_native =
            (uint64_t)usage.ru_maxrss;
    }
#if defined(__APPLE__)
    benchmark->process_peak_resident_unit = "bytes";
#else
    benchmark->process_peak_resident_unit = "kibibytes";
#endif
    return true;
}

static void fnv_byte(uint64_t *digest, unsigned char value)
{
    *digest ^= value;
    *digest *= UINT64_C(1099511628211);
}

static void fnv_uint64(uint64_t *digest, uint64_t value)
{
    for (unsigned int byte = 0; byte < 8u; ++byte) {
        fnv_byte(digest, (unsigned char)(value & UINT64_C(0xff)));
        value >>= 8u;
    }
}

static uint64_t record_locator(const benchmark_suite_t *benchmark)
{
    uint64_t digest = UINT64_C(14695981039346656037);

    for (size_t mode_index = 0;
         mode_index < EVO_BENCHMARK_MODE_COUNT;
         ++mode_index) {
        const case_record_t *case_record = &benchmark->cases[mode_index];

        fnv_uint64(&digest, (uint64_t)mode_index);
        fnv_uint64(&digest, (uint64_t)case_record->worker_count);
        fnv_uint64(&digest,
                   case_record->mode.recycling ? UINT64_C(1) : UINT64_C(0));
        for (size_t seed_index = 0;
             seed_index < case_record->seed_count;
             ++seed_index) {
            const seed_record_t *seed_record =
                &case_record->seeds[seed_index];

            fnv_uint64(&digest, seed_record->seed);
            for (size_t byte = 0;
                 byte < EVO_BENCHMARK_GENOME_SIZE;
                 ++byte) {
                fnv_byte(&digest,
                         seed_record->reference_result.best_genome[byte]);
            }
            for (size_t sample_index = 0;
                 sample_index < seed_record->sample_count;
                 ++sample_index) {
                fnv_uint64(&digest,
                           seed_record->samples[sample_index].wall_time_ns);
                fnv_uint64(
                    &digest,
                    (uint64_t)seed_record->samples[sample_index]
                        .cpu_clock_ticks);
            }
        }
    }
    return digest;
}

static void print_json_string(FILE *output, const char *value)
{
    const unsigned char *bytes = (const unsigned char *)value;

    (void)fputc('"', output);
    for (size_t index = 0; bytes[index] != 0; ++index) {
        const unsigned char byte = bytes[index];

        if (byte == '"' || byte == '\\') {
            (void)fputc('\\', output);
            (void)fputc((int)byte, output);
        } else if (byte == '\b') {
            (void)fputs("\\b", output);
        } else if (byte == '\f') {
            (void)fputs("\\f", output);
        } else if (byte == '\n') {
            (void)fputs("\\n", output);
        } else if (byte == '\r') {
            (void)fputs("\\r", output);
        } else if (byte == '\t') {
            (void)fputs("\\t", output);
        } else if (byte < 0x20u) {
            (void)fprintf(output, "\\u%04x", (unsigned int)byte);
        } else {
            (void)fputc((int)byte, output);
        }
    }
    (void)fputc('"', output);
}

static void print_json_bool(FILE *output, bool value)
{
    (void)fputs(value ? "true" : "false", output);
}

static void print_fitness_json(FILE *output,
                               const evo_fitness_t *fitness)
{
    (void)fprintf(output,
                  "{\"correctness\": %.17g, \"performance\": %.17g, "
                  "\"memory_use\": %.17g, \"reliability\": %.17g, "
                  "\"maintainability\": %.17g, "
                  "\"constraint_penalty\": %.17g, \"total\": %.17g}",
                  fitness->correctness,
                  fitness->performance,
                  fitness->memory_use,
                  fitness->reliability,
                  fitness->maintainability,
                  fitness->constraint_penalty,
                  fitness->total);
}

static void print_statistics_json(
    FILE *output,
    const evo_generation_statistics_t *statistics)
{
    (void)fprintf(output,
                  "          {\"version\": %" PRIu32
                  ", \"generation_index\": %" PRIu64
                  ", \"population_size\": %zu, \"valid_count\": %zu, "
                  "\"invalid_count\": %zu, \"best_index\": %zu, "
                  "\"has_best\": ",
                  statistics->version,
                  statistics->generation_index,
                  statistics->population_size,
                  statistics->valid_count,
                  statistics->invalid_count,
                  statistics->best_index);
    print_json_bool(output, statistics->has_best);
    (void)fputs(",\n            \"best_fitness\": ", output);
    print_fitness_json(output, &statistics->best_fitness);
    (void)fputs(",\n            \"fitness_sums\": ", output);
    print_fitness_json(output, &statistics->fitness_sums);
    (void)fprintf(
        output,
        ",\n            \"fitness_comparison_policy_version\": %" PRIu32
        ", \"diversity_policy_version\": %" PRIu32
        ", \"diversity_metric_version\": %" PRIu32
        ",\n            \"diversity_pair_count\": %zu, "
        "\"diversity_work_units\": %zu, \"diversity\": %.17g, "
        "\"diversity_uses_domain_distance\": ",
        statistics->fitness_comparison_policy_version,
        statistics->diversity_policy_version,
        statistics->diversity_metric_version,
        statistics->diversity_pair_count,
        statistics->diversity_work_units,
        statistics->diversity);
    print_json_bool(output, statistics->diversity_uses_domain_distance);
    (void)fprintf(
        output,
        ",\n            \"adaptive_mutation_policy_version\": %" PRIu32
        ", \"mutation_rate_prior\": %.17g, "
        "\"mutation_rate_effective\": %.17g, "
        "\"adaptive_mutation_min_rate\": %.17g, "
        "\"adaptive_mutation_max_rate\": %.17g, "
        "\"adaptive_mutation_step\": %.17g, "
        "\"adaptive_mutation_diversity_threshold\": %.17g,\n"
        "            \"adaptive_mutation_stagnant_generations\": %zu, "
        "\"mutation_adaptation_reason\": ",
        statistics->adaptive_mutation_policy_version,
        statistics->mutation_rate_prior,
        statistics->mutation_rate_effective,
        statistics->adaptive_mutation_min_rate,
        statistics->adaptive_mutation_max_rate,
        statistics->adaptive_mutation_step,
        statistics->adaptive_mutation_diversity_threshold,
        statistics->adaptive_mutation_stagnant_generations);
    print_json_string(
        output,
        adaptation_reason_name(statistics->mutation_adaptation_reason));
    (void)fputs(", \"adaptive_mutation_enabled\": ", output);
    print_json_bool(output, statistics->adaptive_mutation_enabled);
    (void)fputs(", \"adaptive_mutation_low_diversity\": ", output);
    print_json_bool(output, statistics->adaptive_mutation_low_diversity);
    (void)fputs(", \"adaptive_mutation_global_best_improved\": ",
                output);
    print_json_bool(output,
                    statistics->adaptive_mutation_global_best_improved);
    (void)fputs(", \"adaptive_mutation_clamped_to_min\": ", output);
    print_json_bool(output, statistics->adaptive_mutation_clamped_to_min);
    (void)fputs(", \"adaptive_mutation_clamped_to_max\": ", output);
    print_json_bool(output, statistics->adaptive_mutation_clamped_to_max);
    (void)fputs(", \"adaptive_mutation_reset_on_improvement\": ",
                output);
    print_json_bool(output,
                    statistics->adaptive_mutation_reset_on_improvement);
    (void)fputs("}", output);
}

static void print_genome_hex(FILE *output, const unsigned char *genome)
{
    (void)fputc('"', output);
    for (size_t index = 0; index < EVO_BENCHMARK_GENOME_SIZE; ++index) {
        (void)fprintf(output, "%02x", (unsigned int)genome[index]);
    }
    (void)fputc('"', output);
}

static void print_oracle_json(FILE *output,
                              const seed_record_t *seed_record)
{
    const benchmark_oracle_t *oracle = find_oracle(seed_record->seed);

    (void)fputs("        \"oracle\": {\"applicable\": ", output);
    print_json_bool(output, seed_record->oracle_applicable);
    (void)fputs(", \"passed\": ", output);
    print_json_bool(output, seed_record->oracle_passed);
    if (oracle == NULL) {
        (void)fputs(", \"expected\": null},\n", output);
        return;
    }
    (void)fputs(", \"expected\": {\"best_genome_hex\": ", output);
    print_genome_hex(output, oracle->best_genome);
    (void)fprintf(output,
                  ", \"best_total\": %.17g, "
                  "\"generations_completed\": %zu, "
                  "\"termination_reason\": \"generation-limit\",\n"
                  "          \"trace\": [\n",
                  oracle->best_total,
                  EVO_BENCHMARK_GENERATION_LIMIT);
    for (size_t index = 0; index < EVO_BENCHMARK_TRACE_CAPACITY; ++index) {
        (void)fprintf(
            output,
            "            {\"generation_index\": %zu, "
            "\"global_best_total\": %.17g, "
            "\"generation_best_total\": %.17g, "
            "\"diversity_scaled_1e6\": %" PRIu64 "}%s\n",
            index,
            oracle->global_best_totals[index],
            oracle->generation_best_totals[index],
            oracle->diversity_scaled[index],
            index + 1 == EVO_BENCHMARK_TRACE_CAPACITY ? "" : ",");
    }
    (void)fputs("          ]}},\n", output);
}

static void print_semantic_result_json(FILE *output,
                                       const semantic_result_t *result)
{
    (void)fputs("        \"result\": {\"best_genome_hex\": ", output);
    print_genome_hex(output, result->best_genome);
    (void)fputs(", \"best_fitness\": ", output);
    print_fitness_json(output, &result->best_fitness);
    (void)fprintf(output,
                  ", \"generations_completed\": %zu, "
                  "\"random_seed_hex\": \"%016" PRIx64
                  "\", \"termination_reason\": ",
                  result->generations_completed,
                  result->random_seed);
    print_json_string(output, termination_name(result->termination_reason));
    (void)fputs(",\n          \"final_statistics\":\n", output);
    print_statistics_json(output, &result->final_statistics);
    (void)fputs("},\n", output);

    (void)fputs("        \"generation_trace\": [\n", output);
    for (size_t index = 0; index < result->trace_count; ++index) {
        const generation_record_t *record = &result->trace[index];

        (void)fprintf(
            output,
            "          {\"generation_index\": %" PRIu64
            ", \"valid_count\": %zu, \"invalid_count\": %zu, "
            "\"generation_best_index\": %zu, "
            "\"generation_best_total\": %.17g, "
            "\"global_best_total\": %.17g, "
            "\"diversity_pair_count\": %zu, "
            "\"diversity_work_units\": %zu, "
            "\"diversity\": %.17g, "
            "\"diversity_scaled_1e6\": %" PRIu64
            ", \"mutation_rate_prior\": %.17g, "
            "\"mutation_rate_effective\": %.17g, "
            "\"mutation_stagnant_generations\": %zu, "
            "\"mutation_adaptation_reason\": ",
            record->generation_index,
            record->valid_count,
            record->invalid_count,
            record->generation_best_index,
            record->generation_best_total,
            record->global_best_total,
            record->diversity_pair_count,
            record->diversity_work_units,
            record->diversity,
            scaled_diversity(record->diversity),
            record->mutation_rate_prior,
            record->mutation_rate_effective,
            record->mutation_stagnant_generations);
        print_json_string(
            output,
            adaptation_reason_name(record->mutation_adaptation_reason));
        (void)fprintf(output,
                      "}%s\n",
                      index + 1 == result->trace_count ? "" : ",");
    }
    (void)fputs("        ],\n", output);
}

static bool write_json(FILE *output, const benchmark_suite_t *benchmark)
{
    (void)fputs("{\n  \"schema\": \"catalyst.evo-core-benchmark.v1\",\n",
                output);
    (void)fprintf(output,
                  "  \"schema_version\": \"%s\",\n"
                  "  \"benchmark_id\": \"%s\",\n"
                  "  \"tier\": \"%s\",\n"
                  "  \"correctness_passed\": ",
                  EVO_BENCHMARK_SCHEMA_VERSION,
                  EVO_BENCHMARK_ID,
                  tier_name(benchmark->options.tier));
    print_json_bool(output, benchmark->correctness_passed);
    (void)fprintf(output,
                  ",\n  \"record_locator\": {\"algorithm\": "
                  "\"fnv1a64-navigation-only\", \"value\": "
                  "\"%016" PRIx64
                  "\", \"scope\": "
                  "\"mode-seed-best-genome-and-raw-timing-fields\", "
                  "\"authoritative\": false},\n",
                  benchmark->record_locator);

    (void)fprintf(output,
                  "  \"evo\": {\"version\": \"%d.%d.%d\", "
                  "\"commit\": ",
                  EVO_VERSION_MAJOR,
                  EVO_VERSION_MINOR,
                  EVO_VERSION_PATCH);
    print_json_string(output, benchmark->options.commit);
    (void)fputs("},\n  \"environment\": {\"platform\": ", output);
    print_json_string(output, platform_family());
    (void)fputs(", \"architecture\": ", output);
    print_json_string(output, architecture_family());
    (void)fputs(", \"compiler_family\": ", output);
    print_json_string(output, compiler_family());
    (void)fputs(", \"compiler_version\": ", output);
    print_json_string(output, __VERSION__);
    (void)fputs(", \"linker\": ", output);
    print_json_string(output, benchmark->options.linker);
    (void)fputs(", \"build_frontend\": ", output);
    print_json_string(output, EVO_BENCHMARK_BUILD_FRONTEND);
    (void)fputs(", \"build_profile\": ", output);
    print_json_string(output, EVO_BENCHMARK_BUILD_PROFILE);
    (void)fprintf(output,
                  ", \"c_standard\": %ld, \"size_t_bits\": %zu, "
                  "\"clock_ticks_per_second\": %ld, "
                  "\"process_peak_resident_native\": %" PRIu64
                  ", \"process_peak_resident_unit\": ",
                  (long)__STDC_VERSION__,
                  sizeof(size_t) * (size_t)8,
                  (long)CLOCKS_PER_SEC,
                  benchmark->process_peak_resident_native);
    print_json_string(output, benchmark->process_peak_resident_unit);
    (void)fputs("},\n", output);

    (void)fprintf(
        output,
        "  \"measurement_policy\": {\"warmup_runs_per_case_seed\": %zu, "
        "\"measured_repetitions_per_case_seed\": %zu, "
        "\"sample_order\": \"case-seed-repetition\", "
        "\"wall_timer\": \"posix-clock-monotonic-nanoseconds\", "
        "\"cpu_timer\": \"iso-c-clock-ticks\", "
        "\"timed_region\": \"evo_run-only-result-destruction-excluded\", "
        "\"runtime_thresholds_enforced\": false, "
        "\"cross_machine_equivalence_claimed\": false, "
        "\"aggregation\": \"markdown-only-min-lower-median-max-from-raw-samples\", "
        "\"platform_tolerance\": "
        "\"runtime-and-rss-are-reporting-only; deterministic-result-fields-must-match-exactly\"},\n",
        benchmark->warmup_runs,
        benchmark->repetitions);
    (void)fputs(
        "  \"memory_policy\": {\"exact_model\": "
        "\"library-requested-calloc-bytes\", "
        "\"exact_model_includes\": "
        "[\"population-genomes\", \"candidate-evaluations\", "
        "\"owned-best-genome\", \"worker-scratch\"], "
        "\"exact_model_excludes\": "
        "[\"allocator-overhead\", \"thread-stacks\", \"consumer-context\", "
        "\"process-runtime\"], \"rss_scope\": \"complete-benchmark-process\", "
        "\"rss_authoritative\": false},\n",
        output);
    (void)fprintf(
        output,
        "  \"workload\": {\"name\": \"byte-onemax-v1\", "
        "\"genome_representation\": \"explicit-%zu-byte-array\", "
        "\"fitness_definition\": "
        "\"maximize-set-bits; correctness=total=bit-count\", "
        "\"population_size\": %zu, \"generation_limit\": %zu, "
        "\"seed_count\": %zu, \"validity\": \"all-candidates-valid\", "
        "\"stopping\": \"generation-limit-only\"},\n",
        EVO_BENCHMARK_GENOME_SIZE,
        EVO_BENCHMARK_POPULATION_SIZE,
        EVO_BENCHMARK_GENERATION_LIMIT,
        benchmark->seed_count);
    (void)fprintf(
        output,
        "  \"common_policy\": {\"selection\": "
        "{\"kind\": \"stable-rank\", \"version\": 1, "
        "\"base_weight\": 1, \"step_weight\": 1}, "
        "\"crossover\": {\"kind\": \"uniform-byte\", "
        "\"version\": 1, \"rate\": 0.75}, "
        "\"mutation\": {\"kind\": \"nonzero-byte-xor\", "
        "\"version\": 1, \"initial_rate\": 0.35}, "
        "\"adaptive_mutation\": {\"enabled\": true, \"version\": 1, "
        "\"min_rate\": 0.10, \"max_rate\": 0.80, \"step\": 0.10, "
        "\"diversity_threshold\": 0.35, "
        "\"reset_on_improvement\": true}, "
        "\"elite\": {\"enabled\": true, \"version\": 1, "
        "\"count\": 2}, \"diversity\": "
        "{\"kind\": \"byte-mismatch\", \"policy_version\": 1, "
        "\"metric_version\": 1}, \"rng\": "
        "{\"algorithm\": \"pcg-xsh-rr\", \"algorithm_version\": %" PRIu32
        ", \"operator_seed_schedule_version\": %" PRIu32 "}, "
        "\"resource_budgets\": {\"max_genome_bytes\": %zu, "
        "\"max_population_bytes\": %zu, \"max_evaluation_bytes\": %zu, "
        "\"max_child_population_bytes\": %zu, "
        "\"max_diversity_work\": %zu, \"max_checkpoint_bytes\": 0}, "
        "\"callback_policy\": {\"initialize\": \"deterministic-no-op\", "
        "\"validity\": \"all-valid\", \"evaluation\": "
        "\"thread-safe-byte-popcount\", \"generation_observer\": true, "
        "\"all_other_observers\": false}, "
        "\"secure_erasure_enabled\": false, "
        "\"fitness_target_enabled\": false, "
        "\"stagnation_enabled\": false, "
        "\"diversity_floor_enabled\": false, "
        "\"checkpointing_enabled\": false},\n",
        EVO_RNG_ALGORITHM_VERSION,
        EVO_OPERATOR_SEED_SCHEDULE_VERSION,
        EVO_BENCHMARK_GENOME_SIZE,
        EVO_BENCHMARK_POPULATION_SIZE * EVO_BENCHMARK_GENOME_SIZE,
        EVO_BENCHMARK_POPULATION_SIZE * sizeof(evo_candidate_evaluation_t),
        EVO_BENCHMARK_POPULATION_SIZE * EVO_BENCHMARK_GENOME_SIZE,
        (EVO_BENCHMARK_POPULATION_SIZE *
         (EVO_BENCHMARK_POPULATION_SIZE - 1) / 2) *
            EVO_BENCHMARK_GENOME_SIZE);
    (void)fputs(
        "  \"human_readable_abstraction\": {\"accelerated_authority\": "
        "false, \"canonical_authority\": "
        "\"ordered explicit cases, seeds, traces, and raw samples\", "
        "\"summary_projection\": \"companion Markdown from these records\", "
        "\"locator_role\": \"navigation only\"},\n",
        output);

    (void)fputs("  \"cases\": [\n", output);
    for (size_t mode_index = 0;
         mode_index < EVO_BENCHMARK_MODE_COUNT;
         ++mode_index) {
        const case_record_t *case_record = &benchmark->cases[mode_index];

        (void)fputs("    {\"case_id\": ", output);
        print_json_string(output, case_record->mode.id);
        (void)fputs(", \"parallel_evaluation\": ", output);
        print_json_bool(output, case_record->mode.parallel);
        (void)fputs(", \"population_recycling\": ", output);
        print_json_bool(output, case_record->mode.recycling);
        (void)fprintf(
            output,
            ", \"evaluation_worker_count\": %zu, "
            "\"worker_scratch_bytes\": %zu, "
            "\"requested_heap_peak_bytes\": %zu, "
            "\"requested_heap_total_bytes\": %zu, "
            "\"correctness_passed\": ",
            case_record->worker_count,
            case_record->worker_scratch_bytes,
            case_record->requested_heap_peak_bytes,
            case_record->requested_heap_total_bytes);
        print_json_bool(output, case_record->correctness_passed);
        (void)fputs(",\n      \"seeds\": [\n", output);

        for (size_t seed_index = 0;
             seed_index < case_record->seed_count;
             ++seed_index) {
            const seed_record_t *seed_record =
                &case_record->seeds[seed_index];

            (void)fprintf(output,
                          "      {\"seed_hex\": \"%016" PRIx64
                          "\", \"cross_mode_passed\": ",
                          seed_record->seed);
            print_json_bool(output, seed_record->cross_mode_passed);
            (void)fputs(",\n", output);
            print_oracle_json(output, seed_record);
            print_semantic_result_json(output,
                                       &seed_record->reference_result);
            (void)fputs("        \"raw_samples\": [\n", output);
            for (size_t sample_index = 0;
                 sample_index < seed_record->sample_count;
                 ++sample_index) {
                const timing_sample_t *sample =
                    &seed_record->samples[sample_index];

                (void)fprintf(output,
                              "          {\"repetition\": %zu, "
                              "\"wall_time_ns\": %" PRIu64
                              ", \"cpu_clock_ticks\": %" PRIuMAX
                              ", \"matches_seed_reference\": ",
                              sample_index,
                              sample->wall_time_ns,
                              sample->cpu_clock_ticks);
                print_json_bool(output, sample->matches_seed_reference);
                (void)fputs(", \"matches_serial_reference\": ", output);
                print_json_bool(output,
                                sample->matches_serial_reference);
                (void)fprintf(
                    output,
                    "}%s\n",
                    sample_index + 1 == seed_record->sample_count ? "" : ",");
            }
            (void)fprintf(output,
                          "        ]}%s\n",
                          seed_index + 1 == case_record->seed_count ? ""
                                                                    : ",");
        }
        (void)fprintf(output,
                      "      ]}%s\n",
                      mode_index + 1 == EVO_BENCHMARK_MODE_COUNT ? "" : ",");
    }
    (void)fputs("  ]\n}\n", output);
    return ferror(output) == 0;
}

static void print_usage(FILE *output, const char *program)
{
    (void)fprintf(
        output,
        "usage: %s [--tier smoke|extended] [--commit ID] [--linker ID] "
        "\n",
        program);
}

static bool parse_options(int argc,
                          char **argv,
                          benchmark_options_t *options,
                          bool *help_requested)
{
    *options = (benchmark_options_t){
        .tier = BENCHMARK_TIER_SMOKE,
        .commit = EVO_BENCHMARK_COMMIT,
        .linker = EVO_BENCHMARK_LINKER_ID,
    };
    *help_requested = false;

    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];

        if (strcmp(argument, "--help") == 0) {
            *help_requested = true;
            return true;
        }
        if (index + 1 >= argc) {
            return false;
        }
        ++index;
        if (strcmp(argument, "--tier") == 0) {
            if (strcmp(argv[index], "smoke") == 0) {
                options->tier = BENCHMARK_TIER_SMOKE;
            } else if (strcmp(argv[index], "extended") == 0) {
                options->tier = BENCHMARK_TIER_EXTENDED;
            } else {
                return false;
            }
        } else if (strcmp(argument, "--commit") == 0) {
            options->commit = argv[index];
        } else if (strcmp(argument, "--linker") == 0) {
            options->linker = argv[index];
        } else {
            return false;
        }
    }

    return metadata_string_is_valid(options->commit, 128) &&
           metadata_string_is_valid(options->linker, 256);
}

int main(int argc, char **argv)
{
    benchmark_options_t options = {0};
    bool help_requested = false;

    if (!parse_options(argc, argv, &options, &help_requested)) {
        print_usage(stderr, argv[0]);
        return 2;
    }
    if (help_requested) {
        print_usage(stdout, argv[0]);
        return 0;
    }

    suite = (benchmark_suite_t){.options = options};
    if (!execute_suite(&suite)) {
        (void)fputs("benchmark execution failed\n", stderr);
        return 1;
    }
    suite.record_locator = record_locator(&suite);

    return write_json(stdout, &suite) && suite.correctness_passed ? 0 : 1;
}
