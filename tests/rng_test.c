#include "internal/rng.h"

#include <assert.h>
#include <math.h>

static void test_invalid_state_rejection(void)
{
    evo_rng_t rng = {0};
    uint32_t value = 0;
    unsigned char byte = 0;
    size_t index = 19;
    bool occurred = true;

    assert(!evo_rng_seed(NULL, 0));
    assert(!evo_rng_next_u32(NULL, &value));
    assert(!evo_rng_next_u32(&rng, NULL));
    assert(!evo_rng_next_u32(&rng, &value));
    assert(!evo_rng_uniform_index(NULL, 3, &index));
    assert(!evo_rng_uniform_index(&rng, 3, &index));
    assert(!evo_rng_probability_event(NULL, 0.5, &occurred));
    assert(!evo_rng_probability_event(&rng, 0.5, &occurred));
    assert(!evo_rng_fill_bytes(&rng, &byte, 1));

    assert(evo_rng_seed(&rng, 0));
    const evo_rng_t seeded = rng;
    assert(!evo_rng_uniform_index(&rng, 0, &index));
    assert(!evo_rng_uniform_index(&rng, 3, NULL));
    assert(!evo_rng_probability_event(&rng, 0.5, NULL));
    assert(!evo_rng_probability_event(&rng, -0.1, &occurred));
    assert(!evo_rng_probability_event(&rng, 1.1, &occurred));
    assert(!evo_rng_probability_event(&rng, NAN, &occurred));
    assert(!evo_rng_probability_event(&rng, INFINITY, &occurred));
    assert(index == 19);
    assert(occurred);
    assert(rng.state == seeded.state);
    assert(rng.increment == seeded.increment);
    assert(rng.seeded == seeded.seeded);
    assert(!evo_rng_fill_bytes(&rng, NULL, 1));
    assert(evo_rng_fill_bytes(&rng, NULL, 0));
}

static void test_probability_event_fixed_vector(void)
{
    static const double probabilities[] = {0.0, 1.0, 0.5, 0.25};
    static const bool expected[] = {false, true, true, false};
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    assert(evo_rng_seed(&rng, 42));
    for (size_t index = 0;
         index < sizeof(probabilities) / sizeof(probabilities[0]);
         ++index) {
        bool occurred = !expected[index];

        assert(evo_rng_probability_event(
            &rng, probabilities[index], &occurred));
        assert(occurred == expected[index]);
    }

    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0xf5af5ead));
}

static void test_probability_event_replay(void)
{
    static const double probabilities[] = {
        0.125,
        0.5,
        0.875,
        0.3333333333333333,
    };
    evo_rng_t first = {0};
    evo_rng_t replay = {0};

    assert(evo_rng_seed(&first, UINT64_C(20260731)));
    assert(evo_rng_seed(&replay, UINT64_C(20260731)));

    for (size_t cycle = 0; cycle < 8; ++cycle) {
        for (size_t index = 0;
             index < sizeof(probabilities) /
                         sizeof(probabilities[0]);
             ++index) {
            bool first_event = false;
            bool replay_event = true;

            assert(evo_rng_probability_event(
                &first, probabilities[index], &first_event));
            assert(evo_rng_probability_event(
                &replay, probabilities[index], &replay_event));
            assert(first_event == replay_event);
        }
    }

    assert(first.state == replay.state);
    assert(first.increment == replay.increment);
    assert(first.seeded == replay.seeded);
}

static void test_zero_seed_fixed_vector(void)
{
    static const uint32_t expected[] = {
        UINT32_C(0xe823a24e),
        UINT32_C(0x7a7ecbd9),
        UINT32_C(0x89fd6c06),
        UINT32_C(0xae646aa8),
        UINT32_C(0xcd3cf945),
        UINT32_C(0x6204b303),
        UINT32_C(0x198c8585),
        UINT32_C(0x49fce611),
    };
    evo_rng_t rng = {0};

    assert(evo_rng_seed(&rng, 0));
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]);
         ++index) {
        uint32_t value = 0;

        assert(evo_rng_next_u32(&rng, &value));
        assert(value == expected[index]);
    }
}

static void test_seed_42_fixed_vector(void)
{
    static const uint32_t expected[] = {
        UINT32_C(0xc2f57bd6),
        UINT32_C(0x6b07c4a9),
        UINT32_C(0x72b7b29b),
        UINT32_C(0x44215383),
        UINT32_C(0xf5af5ead),
        UINT32_C(0x68beb632),
        UINT32_C(0xcbc7312c),
        UINT32_C(0xd5efc7d7),
    };
    evo_rng_t rng = {0};

    assert(evo_rng_seed(&rng, 42));
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]);
         ++index) {
        uint32_t value = 0;

        assert(evo_rng_next_u32(&rng, &value));
        assert(value == expected[index]);
    }
}

static void test_explicit_byte_emission_order(void)
{
    static const unsigned char expected[] = {
        0xd6,
        0x7b,
        0xf5,
        0xc2,
        0xa9,
        0xc4,
        0x07,
        0x6b,
        0x9b,
        0xb2,
        0xb7,
        0x72,
        0x83,
        0x53,
        0x21,
        0x44,
    };
    unsigned char actual[sizeof(expected)] = {0};
    evo_rng_t rng = {0};

    assert(evo_rng_seed(&rng, 42));
    assert(evo_rng_fill_bytes(&rng, actual, sizeof(actual)));
    for (size_t index = 0; index < sizeof(expected); ++index) {
        assert(actual[index] == expected[index]);
    }
}

static void test_partial_word_boundary(void)
{
    static const unsigned char expected[] = {
        0xd6,
        0x7b,
        0xf5,
        0xc2,
        0xa9,
    };
    unsigned char guarded[sizeof(expected) + 2] = {0};
    evo_rng_t rng = {0};

    guarded[0] = 0x5a;
    guarded[sizeof(guarded) - 1] = 0xa5;
    assert(evo_rng_seed(&rng, 42));
    assert(evo_rng_fill_bytes(&rng, guarded + 1, sizeof(expected)));
    assert(guarded[0] == 0x5a);
    assert(guarded[sizeof(guarded) - 1] == 0xa5);
    for (size_t index = 0; index < sizeof(expected); ++index) {
        assert(guarded[index + 1] == expected[index]);
    }
}

static void test_reseeding_restores_stream(void)
{
    evo_rng_t rng = {0};
    uint32_t first = 0;
    uint32_t replay = 0;

    assert(evo_rng_seed(&rng, UINT64_C(0xffffffffffffffff)));
    assert(evo_rng_next_u32(&rng, &first));
    assert(evo_rng_seed(&rng, UINT64_C(0xffffffffffffffff)));
    assert(evo_rng_next_u32(&rng, &replay));
    assert(first == replay);
}

static void test_uniform_index_fixed_vector(void)
{
    static const size_t bounds[] = {1, 2, 10};
    static const size_t expected[] = {0, 1, 3};
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    assert(evo_rng_seed(&rng, 42));
    for (size_t vector_index = 0;
         vector_index < sizeof(bounds) / sizeof(bounds[0]);
         ++vector_index) {
        size_t actual = SIZE_MAX;

        assert(evo_rng_uniform_index(
            &rng, bounds[vector_index], &actual));
        assert(actual == expected[vector_index]);
    }

    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0xcbc7312c));
}

static void test_uniform_index_replay(void)
{
    evo_rng_t first = {0};
    evo_rng_t replay = {0};

    assert(evo_rng_seed(&first, UINT64_C(20260731)));
    assert(evo_rng_seed(&replay, UINT64_C(20260731)));

    for (size_t draw = 0; draw < 32; ++draw) {
        size_t first_index = SIZE_MAX;
        size_t replay_index = SIZE_MAX;

        assert(evo_rng_uniform_index(&first, 17, &first_index));
        assert(evo_rng_uniform_index(&replay, 17, &replay_index));
        assert(first_index == replay_index);
        assert(first_index < 17);
    }

    assert(first.state == replay.state);
    assert(first.increment == replay.increment);
    assert(first.seeded == replay.seeded);
}

static void test_uniform_index_rejection_path(void)
{
#if SIZE_MAX > UINT32_MAX
    const size_t bound =
        (size_t)UINT64_C(0x8000000000000001);
    const size_t expected =
        (size_t)UINT64_C(0x55efc7d7cbc7312b);
    evo_rng_t rng = {0};
    size_t actual = SIZE_MAX;
    uint32_t next_value = 0;

    assert(evo_rng_seed(&rng, 42));
    assert(evo_rng_uniform_index(&rng, bound, &actual));
    assert(actual == expected);

    /*
     * The first three 64-bit samples are below the rejection threshold.
     * The accepted fourth sample consumes words seven and eight.
     */
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x7aec0808));
#endif
}

int main(void)
{
    test_invalid_state_rejection();
    test_zero_seed_fixed_vector();
    test_seed_42_fixed_vector();
    test_explicit_byte_emission_order();
    test_partial_word_boundary();
    test_reseeding_restores_stream();
    test_uniform_index_fixed_vector();
    test_uniform_index_replay();
    test_uniform_index_rejection_path();
    test_probability_event_fixed_vector();
    test_probability_event_replay();
    return 0;
}
