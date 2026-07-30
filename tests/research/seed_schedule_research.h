#ifndef CATALYST_EVO_TESTS_RESEARCH_SEED_SCHEDULE_RESEARCH_H
#define CATALYST_EVO_TESTS_RESEARCH_SEED_SCHEDULE_RESEARCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_RESEARCH_PRIME_VECTOR_COUNT ((size_t)4099)
#define EVO_RESEARCH_PRIME_VECTOR_SHA256 \
    "a6ad2811fbf74c2879900a93fecd6ae85b4915e0d9fb2192f4241ac0a2b91869"
#define EVO_RESEARCH_FIELD_PRIME UINT32_C(2147483647)
#define EVO_RESEARCH_CURVE_A UINT32_C(1)
#define EVO_RESEARCH_CURVE_B UINT32_C(1)

typedef enum evo_research_seed_domain {
    EVO_RESEARCH_DOMAIN_INITIALIZATION = 1,
    EVO_RESEARCH_DOMAIN_SELECTION = 2,
    EVO_RESEARCH_DOMAIN_CROSSOVER = 3,
    EVO_RESEARCH_DOMAIN_MUTATION = 4
} evo_research_seed_domain_t;

typedef enum evo_research_seed_candidate {
    EVO_RESEARCH_CANDIDATE_V1_BASELINE = 1,
    EVO_RESEARCH_CANDIDATE_MIXED_CONTROL = 2,
    EVO_RESEARCH_CANDIDATE_PRIME_INDEXED = 3,
    EVO_RESEARCH_CANDIDATE_ELLIPTIC = 4
} evo_research_seed_candidate_t;

typedef struct evo_research_seed_tuple {
    uint64_t master_seed;
    uint64_t generation;
    uint64_t population_index;
    evo_research_seed_domain_t domain;
} evo_research_seed_tuple_t;

typedef struct evo_research_pcg_schedule {
    uint64_t state;
    uint64_t increment;
} evo_research_pcg_schedule_t;

typedef struct evo_research_curve_point {
    uint32_t x;
    uint32_t y;
    bool infinity;
} evo_research_curve_point_t;

/*
 * Generate the first prime_count primes in ascending order. Candidates after
 * 3 are visited as 6k - 1 and 6k + 1, following the bounded 6-wheel idea in
 * dlworrell/code-noodling at commit
 * 43c4b386acfcc634f1d62e96a5b7809e96d8a1ec.
 */
bool evo_research_generate_prime_vector(uint32_t *primes, size_t prime_count);

/*
 * Derive an experimental PCG state and stream increment. These schedules are
 * research-only and do not change EVO_RNG_ALGORITHM_VERSION 1.
 */
bool evo_research_derive_schedule(
    evo_research_seed_candidate_t candidate,
    const evo_research_seed_tuple_t *tuple,
    const uint32_t *primes,
    size_t prime_count,
    evo_research_pcg_schedule_t *schedule);

/* Advance a derived experimental PCG-XSH-RR 64/32 stream. */
uint32_t evo_research_schedule_next_u32(
    evo_research_pcg_schedule_t *schedule);

/*
 * Multiply the fixed base point G = (0, 1) on
 * y^2 = x^3 + x + 1 over F_2147483647.
 */
bool evo_research_curve_multiply(uint32_t scalar,
                                 evo_research_curve_point_t *point);

#endif
