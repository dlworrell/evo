#include "internal/rng.h"

#include <assert.h>

static void test_invalid_state_rejection(void)
{
    evo_rng_t rng = {0};
    uint32_t value = 0;
    unsigned char byte = 0;

    assert(!evo_rng_seed(NULL, 0));
    assert(!evo_rng_next_u32(NULL, &value));
    assert(!evo_rng_next_u32(&rng, NULL));
    assert(!evo_rng_next_u32(&rng, &value));
    assert(!evo_rng_fill_bytes(&rng, &byte, 1));

    assert(evo_rng_seed(&rng, 0));
    assert(!evo_rng_fill_bytes(&rng, NULL, 1));
    assert(evo_rng_fill_bytes(&rng, NULL, 0));
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

int main(void)
{
    test_invalid_state_rejection();
    test_zero_seed_fixed_vector();
    test_seed_42_fixed_vector();
    test_explicit_byte_emission_order();
    test_partial_word_boundary();
    test_reseeding_restores_stream();
    return 0;
}
