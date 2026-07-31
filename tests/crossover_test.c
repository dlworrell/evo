#include "internal/crossover.h"

#include <assert.h>
#include <math.h>

enum {
    TEST_GENOME_SIZE = 4
};

typedef struct crossover_evidence {
    size_t calls;
    const void *parent_a;
    const void *parent_b;
    void *child_a;
    void *child_b;
} crossover_evidence_t;

static void test_crossover(const void *parent_a,
                           const void *parent_b,
                           void *child_a,
                           void *child_b,
                           void *context)
{
    const unsigned char *left = parent_a;
    const unsigned char *right = parent_b;
    unsigned char *first = child_a;
    unsigned char *second = child_b;
    crossover_evidence_t *evidence = context;

    assert(evidence != NULL);
    ++evidence->calls;
    evidence->parent_a = parent_a;
    evidence->parent_b = parent_b;
    evidence->child_a = child_a;
    evidence->child_b = child_b;

    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        first[index] = (unsigned char)(left[index] ^ right[index]);
        second[index] =
            (unsigned char)(left[index] + right[index]);
    }
}

static evo_problem_t test_problem(void)
{
    evo_problem_t problem = {0};

    problem.genome_size = TEST_GENOME_SIZE;
    problem.crossover = test_crossover;
    return problem;
}

static evo_config_t test_config(double crossover_rate)
{
    evo_config_t config = {0};

    config.crossover_rate = crossover_rate;
    config.max_genome_bytes = TEST_GENOME_SIZE;
    return config;
}

static void assert_bytes_equal(const unsigned char *actual,
                               const unsigned char *expected)
{
    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        assert(actual[index] == expected[index]);
    }
}

static void assert_rng_equal(const evo_rng_t *left,
                             const evo_rng_t *right)
{
    assert(left->state == right->state);
    assert(left->increment == right->increment);
    assert(left->seeded == right->seeded);
}

static void test_invalid_input_preserves_state(void)
{
    static const unsigned char parent_a[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    static const unsigned char parent_b[TEST_GENOME_SIZE] = {
        9, 8, 7, 6};
    static const unsigned char child_a_before[TEST_GENOME_SIZE] = {
        0xa1, 0xa2, 0xa3, 0xa4};
    static const unsigned char child_b_before[TEST_GENOME_SIZE] = {
        0xb1, 0xb2, 0xb3, 0xb4};
    unsigned char child_a[TEST_GENOME_SIZE] = {
        0xa1, 0xa2, 0xa3, 0xa4};
    unsigned char child_b[TEST_GENOME_SIZE] = {
        0xb1, 0xb2, 0xb3, 0xb4};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.5);
    evo_rng_t rng = {0};

    assert(evo_rng_seed(&rng, 42));
    const evo_rng_t before_rng = rng;

    assert(evo_crossover_pair(NULL,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_crossover_pair(&problem,
                              NULL,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              NULL,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              NULL,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_a) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              (void *)parent_a,
                              child_b) ==
           EVO_ERROR_INVALID_ARGUMENT);

    problem.genome_size = 0;
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_RESOURCE_LIMIT);
    problem.genome_size = TEST_GENOME_SIZE;
    config.max_genome_bytes = TEST_GENOME_SIZE - 1;
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_RESOURCE_LIMIT);

    config.max_genome_bytes = TEST_GENOME_SIZE;
    config.crossover_rate = NAN;
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_RESOURCE_LIMIT);
    config.crossover_rate = INFINITY;
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_RESOURCE_LIMIT);
    config.crossover_rate = -0.1;
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_RESOURCE_LIMIT);
    config.crossover_rate = 1.1;
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) ==
           EVO_ERROR_RESOURCE_LIMIT);

    assert_rng_equal(&rng, &before_rng);
    assert_bytes_equal(child_a, child_a_before);
    assert_bytes_equal(child_b, child_b_before);
}

static void test_unseeded_rng_preserves_children(void)
{
    static const unsigned char parent_a[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    static const unsigned char parent_b[TEST_GENOME_SIZE] = {
        9, 8, 7, 6};
    static const unsigned char expected_a[TEST_GENOME_SIZE] = {
        0xa1, 0xa2, 0xa3, 0xa4};
    static const unsigned char expected_b[TEST_GENOME_SIZE] = {
        0xb1, 0xb2, 0xb3, 0xb4};
    unsigned char child_a[TEST_GENOME_SIZE] = {
        0xa1, 0xa2, 0xa3, 0xa4};
    unsigned char child_b[TEST_GENOME_SIZE] = {
        0xb1, 0xb2, 0xb3, 0xb4};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.5);
    evo_rng_t rng = {0};

    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) == EVO_ERROR_STATE);
    assert_bytes_equal(child_a, expected_a);
    assert_bytes_equal(child_b, expected_b);
}

static void test_clone_paths_consume_one_word(void)
{
    static const unsigned char parent_a[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    static const unsigned char parent_b[TEST_GENOME_SIZE] = {
        9, 8, 7, 6};
    unsigned char child_a[TEST_GENOME_SIZE] = {0};
    unsigned char child_b[TEST_GENOME_SIZE] = {0};
    crossover_evidence_t evidence = {0};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.0);
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    assert(evo_rng_seed(&rng, 42));
    assert(evo_crossover_pair(&problem,
                              &config,
                              &evidence,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) == EVO_SUCCESS);
    assert(evidence.calls == 0);
    assert_bytes_equal(child_a, parent_a);
    assert_bytes_equal(child_b, parent_b);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x6b07c4a9));

    problem.crossover = NULL;
    config.crossover_rate = 1.0;
    assert(evo_rng_seed(&rng, 42));
    assert(evo_crossover_pair(&problem,
                              &config,
                              &evidence,
                              &rng,
                              parent_a,
                              parent_a,
                              child_a,
                              child_b) == EVO_SUCCESS);
    assert(evidence.calls == 0);
    assert_bytes_equal(child_a, parent_a);
    assert_bytes_equal(child_b, parent_a);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x6b07c4a9));
}

static void test_fixed_gate_and_callback_dispatch(void)
{
    static const unsigned char parent_a[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    static const unsigned char parent_b[TEST_GENOME_SIZE] = {
        9, 8, 7, 6};
    static const unsigned char crossed_a[TEST_GENOME_SIZE] = {
        8, 10, 4, 2};
    static const unsigned char crossed_b[TEST_GENOME_SIZE] = {
        10, 10, 10, 10};
    unsigned char child_a[TEST_GENOME_SIZE] = {0};
    unsigned char child_b[TEST_GENOME_SIZE] = {0};
    crossover_evidence_t evidence = {0};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.5);
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    assert(evo_rng_seed(&rng, 42));

    /* 0xc2f57bd6 is above the half-range threshold: clone. */
    assert(evo_crossover_pair(&problem,
                              &config,
                              &evidence,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) == EVO_SUCCESS);
    assert(evidence.calls == 0);
    assert_bytes_equal(child_a, parent_a);
    assert_bytes_equal(child_b, parent_b);

    /* 0x6b07c4a9 is below the threshold: dispatch once. */
    assert(evo_crossover_pair(&problem,
                              &config,
                              &evidence,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) == EVO_SUCCESS);
    assert(evidence.calls == 1);
    assert(evidence.parent_a == parent_a);
    assert(evidence.parent_b == parent_b);
    assert(evidence.child_a == child_a);
    assert(evidence.child_b == child_b);
    assert_bytes_equal(child_a, crossed_a);
    assert_bytes_equal(child_b, crossed_b);
    assert_bytes_equal(parent_a,
                       (const unsigned char[]){1, 2, 3, 4});
    assert_bytes_equal(parent_b,
                       (const unsigned char[]){9, 8, 7, 6});

    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x72b7b29b));
}

static void test_deterministic_replay(void)
{
    static const unsigned char parent_a[TEST_GENOME_SIZE] = {
        3, 5, 7, 11};
    static const unsigned char parent_b[TEST_GENOME_SIZE] = {
        13, 17, 19, 23};
    unsigned char first_a[TEST_GENOME_SIZE] = {0};
    unsigned char first_b[TEST_GENOME_SIZE] = {0};
    unsigned char replay_a[TEST_GENOME_SIZE] = {0};
    unsigned char replay_b[TEST_GENOME_SIZE] = {0};
    crossover_evidence_t first_evidence = {0};
    crossover_evidence_t replay_evidence = {0};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.375);
    evo_rng_t first = {0};
    evo_rng_t replay = {0};

    assert(evo_rng_seed(&first, UINT64_C(20260731)));
    assert(evo_rng_seed(&replay, UINT64_C(20260731)));

    for (size_t pair = 0; pair < 32; ++pair) {
        assert(evo_crossover_pair(&problem,
                                  &config,
                                  &first_evidence,
                                  &first,
                                  parent_a,
                                  parent_b,
                                  first_a,
                                  first_b) == EVO_SUCCESS);
        assert(evo_crossover_pair(&problem,
                                  &config,
                                  &replay_evidence,
                                  &replay,
                                  parent_a,
                                  parent_b,
                                  replay_a,
                                  replay_b) == EVO_SUCCESS);
        assert_bytes_equal(first_a, replay_a);
        assert_bytes_equal(first_b, replay_b);
        assert(first_evidence.calls == replay_evidence.calls);
    }

    assert_rng_equal(&first, &replay);
}

int main(void)
{
    test_invalid_input_preserves_state();
    test_unseeded_rng_preserves_children();
    test_clone_paths_consume_one_word();
    test_fixed_gate_and_callback_dispatch();
    test_deterministic_replay();
    return 0;
}
