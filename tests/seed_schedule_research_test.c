#include "research/seed_schedule_research.h"

#include "internal/rng.h"

#include <assert.h>

static uint32_t field_multiply(uint32_t left, uint32_t right)
{
    return (uint32_t)(((uint64_t)left * right) %
                      EVO_RESEARCH_FIELD_PRIME);
}

static bool point_is_on_curve(const evo_research_curve_point_t *point)
{
    uint32_t left = 0;
    uint32_t right = 0;

    if (point->infinity) {
        return true;
    }

    left = field_multiply(point->y, point->y);
    right = field_multiply(
        field_multiply(point->x, point->x),
        point->x);
    right = (uint32_t)(((uint64_t)right + point->x +
                        EVO_RESEARCH_CURVE_B) %
                       EVO_RESEARCH_FIELD_PRIME);
    return left == right;
}

static void test_prime_vector(void)
{
    static const uint32_t expected[] = {
        2u,
        3u,
        5u,
        7u,
        11u,
        13u,
        17u,
        19u,
        23u,
        29u,
        31u,
        37u,
        41u,
        43u,
        47u,
        53u,
    };
    uint32_t primes[EVO_RESEARCH_PRIME_VECTOR_COUNT] = {0};

    assert(evo_research_generate_prime_vector(NULL, 0));
    assert(!evo_research_generate_prime_vector(NULL, 1));
    assert(evo_research_generate_prime_vector(
        primes,
        EVO_RESEARCH_PRIME_VECTOR_COUNT));

    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]);
         ++index) {
        assert(primes[index] == expected[index]);
    }
    for (size_t index = 1; index < EVO_RESEARCH_PRIME_VECTOR_COUNT;
         ++index) {
        assert(primes[index] > primes[index - 1]);
    }
    assert(primes[EVO_RESEARCH_PRIME_VECTOR_COUNT - 1] == 38917u);
}

static void test_curve_vectors_and_membership(void)
{
    static const struct {
        uint32_t scalar;
        uint32_t x;
        uint32_t y;
    } vectors[] = {
        {1u, 0u, 1u},
        {2u, 536870912u, 1879048190u},
        {3u, 72u, 611u},
        {17u, 1195819830u, 1566573052u},
        {65537u, 1531601172u, 1989404780u},
    };
    evo_research_curve_point_t point = {0};
    evo_research_curve_point_t replay = {0};
    const uint32_t discriminant =
        (uint32_t)((4u * EVO_RESEARCH_CURVE_A *
                        EVO_RESEARCH_CURVE_A *
                        EVO_RESEARCH_CURVE_A +
                    27u * EVO_RESEARCH_CURVE_B *
                        EVO_RESEARCH_CURVE_B) %
                   EVO_RESEARCH_FIELD_PRIME);

    assert(!evo_research_curve_multiply(1, NULL));
    assert(discriminant != 0u);

    assert(evo_research_curve_multiply(0, &point));
    assert(point.infinity);

    assert(evo_research_curve_multiply(1, &point));
    assert(!point.infinity);
    assert(point.x == 0u);
    assert(point.y == 1u);
    assert(point_is_on_curve(&point));

    for (size_t index = 0; index < sizeof(vectors) / sizeof(vectors[0]);
         ++index) {
        assert(evo_research_curve_multiply(
            vectors[index].scalar,
            &point));
        assert(!point.infinity);
        assert(point.x == vectors[index].x);
        assert(point.y == vectors[index].y);
    }

    for (uint32_t scalar = 2; scalar <= 4096u; ++scalar) {
        assert(evo_research_curve_multiply(scalar, &point));
        assert(!point.infinity);
        assert(point_is_on_curve(&point));
        assert(evo_research_curve_multiply(scalar, &replay));
        assert(point.x == replay.x);
        assert(point.y == replay.y);
        assert(point.infinity == replay.infinity);
    }
}

static void test_invalid_schedule_inputs(void)
{
    uint32_t primes[8] = {0};
    evo_research_seed_tuple_t tuple = {
        .master_seed = 42,
        .generation = 7,
        .population_index = 2,
        .domain = EVO_RESEARCH_DOMAIN_MUTATION,
    };
    evo_research_pcg_schedule_t schedule = {
        .state = UINT64_MAX,
        .increment = UINT64_MAX,
    };

    assert(evo_research_generate_prime_vector(
        primes,
        sizeof(primes) / sizeof(primes[0])));

    assert(!evo_research_derive_schedule(
        EVO_RESEARCH_CANDIDATE_V1_BASELINE,
        NULL,
        primes,
        sizeof(primes) / sizeof(primes[0]),
        &schedule));
    assert(schedule.state == 0);
    assert(schedule.increment == 0);
    assert(!evo_research_derive_schedule(
        EVO_RESEARCH_CANDIDATE_V1_BASELINE,
        &tuple,
        primes,
        sizeof(primes) / sizeof(primes[0]),
        NULL));

    tuple.domain = (evo_research_seed_domain_t)0;
    assert(!evo_research_derive_schedule(
        EVO_RESEARCH_CANDIDATE_MIXED_CONTROL,
        &tuple,
        primes,
        sizeof(primes) / sizeof(primes[0]),
        &schedule));
    assert(schedule.state == 0);
    assert(schedule.increment == 0);

    tuple.domain = EVO_RESEARCH_DOMAIN_MUTATION;
    tuple.population_index = 6;
    assert(!evo_research_derive_schedule(
        EVO_RESEARCH_CANDIDATE_PRIME_INDEXED,
        &tuple,
        primes,
        sizeof(primes) / sizeof(primes[0]),
        &schedule));
    assert(schedule.state == 0);
    assert(schedule.increment == 0);
    assert(!evo_research_derive_schedule(
        (evo_research_seed_candidate_t)99,
        &tuple,
        primes,
        sizeof(primes) / sizeof(primes[0]),
        &schedule));
    assert(schedule.state == 0);
    assert(schedule.increment == 0);
}

static void test_baseline_matches_rng_version_one(void)
{
    evo_research_seed_tuple_t tuple = {
        .master_seed = UINT64_C(0x0123456789abcdef),
        .generation = 91,
        .population_index = 17,
        .domain = EVO_RESEARCH_DOMAIN_CROSSOVER,
    };
    evo_research_pcg_schedule_t schedule = {0};
    evo_rng_t rng = {0};

    assert(evo_rng_seed(&rng, tuple.master_seed));
    assert(evo_research_derive_schedule(
        EVO_RESEARCH_CANDIDATE_V1_BASELINE,
        &tuple,
        NULL,
        0,
        &schedule));
    assert(schedule.state == rng.state);
    assert(schedule.increment == rng.increment);

    for (size_t index = 0; index < 16; ++index) {
        uint32_t expected = 0;

        assert(evo_rng_next_u32(&rng, &expected));
        assert(evo_research_schedule_next_u32(&schedule) == expected);
    }
}

static void test_schedule_fixed_vectors(void)
{
    static const struct {
        evo_research_seed_candidate_t candidate;
        uint64_t state;
        uint64_t increment;
        uint32_t prefix[4];
    } vectors[] = {
        {
            EVO_RESEARCH_CANDIDATE_V1_BASELINE,
            UINT64_C(0x977afd8015414a94),
            UINT64_C(0x14057b7ef767814f),
            {
                UINT32_C(0xc2f57bd6),
                UINT32_C(0x6b07c4a9),
                UINT32_C(0x72b7b29b),
                UINT32_C(0x44215383),
            },
        },
        {
            EVO_RESEARCH_CANDIDATE_MIXED_CONTROL,
            UINT64_C(0x524955425ff5cf53),
            UINT64_C(0x5e6ab376c2e2ce3d),
            {
                UINT32_C(0x80524a0e),
                UINT32_C(0x665016de),
                UINT32_C(0x2feb6ab6),
                UINT32_C(0xbb7ebd14),
            },
        },
        {
            EVO_RESEARCH_CANDIDATE_PRIME_INDEXED,
            UINT64_C(0x6247e56edd869b81),
            UINT64_C(0x372f16ff356ca537),
            {
                UINT32_C(0xfe448ffb),
                UINT32_C(0x73394107),
                UINT32_C(0xc0adcd2d),
                UINT32_C(0xcc8b085a),
            },
        },
        {
            EVO_RESEARCH_CANDIDATE_ELLIPTIC,
            UINT64_C(0x3ef5b4a0b4b5b60e),
            UINT64_C(0x1182e91ce7f6dc5b),
            {
                UINT32_C(0x77bd6ec7),
                UINT32_C(0xd798b537),
                UINT32_C(0x518f2208),
                UINT32_C(0xbd599819),
            },
        },
    };
    uint32_t primes[EVO_RESEARCH_PRIME_VECTOR_COUNT] = {0};
    const evo_research_seed_tuple_t tuple = {
        .master_seed = 42,
        .generation = 7,
        .population_index = 11,
        .domain = EVO_RESEARCH_DOMAIN_MUTATION,
    };

    assert(evo_research_generate_prime_vector(
        primes,
        EVO_RESEARCH_PRIME_VECTOR_COUNT));
    assert(evo_research_schedule_next_u32(NULL) == 0);

    for (size_t vector_index = 0;
         vector_index < sizeof(vectors) / sizeof(vectors[0]);
         ++vector_index) {
        evo_research_pcg_schedule_t schedule = {0};

        assert(evo_research_derive_schedule(
            vectors[vector_index].candidate,
            &tuple,
            primes,
            EVO_RESEARCH_PRIME_VECTOR_COUNT,
            &schedule));
        assert(schedule.state == vectors[vector_index].state);
        assert(schedule.increment == vectors[vector_index].increment);
        for (size_t prefix_index = 0;
             prefix_index <
             sizeof(vectors[vector_index].prefix) /
                 sizeof(vectors[vector_index].prefix[0]);
             ++prefix_index) {
            assert(evo_research_schedule_next_u32(&schedule) ==
                   vectors[vector_index].prefix[prefix_index]);
        }
    }
}

static void test_domain_separation_and_replay(void)
{
    uint32_t primes[EVO_RESEARCH_PRIME_VECTOR_COUNT] = {0};
    evo_research_seed_tuple_t tuple = {
        .master_seed = 42,
        .generation = 7,
        .population_index = 11,
        .domain = EVO_RESEARCH_DOMAIN_INITIALIZATION,
    };

    assert(evo_research_generate_prime_vector(
        primes,
        EVO_RESEARCH_PRIME_VECTOR_COUNT));

    for (evo_research_seed_candidate_t candidate =
             EVO_RESEARCH_CANDIDATE_MIXED_CONTROL;
         candidate <= EVO_RESEARCH_CANDIDATE_ELLIPTIC;
         candidate = (evo_research_seed_candidate_t)(candidate + 1)) {
        evo_research_pcg_schedule_t initialization = {0};
        evo_research_pcg_schedule_t replay = {0};
        evo_research_pcg_schedule_t mutation = {0};

        assert(evo_research_derive_schedule(
            candidate,
            &tuple,
            primes,
            EVO_RESEARCH_PRIME_VECTOR_COUNT,
            &initialization));
        assert(evo_research_derive_schedule(
            candidate,
            &tuple,
            primes,
            EVO_RESEARCH_PRIME_VECTOR_COUNT,
            &replay));
        assert(initialization.state == replay.state);
        assert(initialization.increment == replay.increment);
        assert((initialization.increment & UINT64_C(1)) != 0);

        tuple.domain = EVO_RESEARCH_DOMAIN_MUTATION;
        assert(evo_research_derive_schedule(
            candidate,
            &tuple,
            primes,
            EVO_RESEARCH_PRIME_VECTOR_COUNT,
            &mutation));
        assert(initialization.state != mutation.state ||
               initialization.increment != mutation.increment);
        tuple.domain = EVO_RESEARCH_DOMAIN_INITIALIZATION;
    }
}

int main(void)
{
    test_prime_vector();
    test_curve_vectors_and_membership();
    test_invalid_schedule_inputs();
    test_baseline_matches_rng_version_one();
    test_schedule_fixed_vectors();
    test_domain_separation_and_replay();
    return 0;
}
