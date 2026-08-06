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

static void assert_bytes_equal_count(const unsigned char *actual,
                                     const unsigned char *expected,
                                     size_t count)
{
    for (size_t index = 0; index < count; ++index) {
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

static void test_invalid_operator_and_partial_overlap_preserve_state(void)
{
    static const unsigned char parent_a[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    static const unsigned char parent_b[TEST_GENOME_SIZE] = {
        9, 8, 7, 6};
    static const unsigned char child_before[TEST_GENOME_SIZE] = {
        0xa1, 0xa2, 0xa3, 0xa4};
    unsigned char child_a[TEST_GENOME_SIZE] = {
        0xa1, 0xa2, 0xa3, 0xa4};
    unsigned char child_b[TEST_GENOME_SIZE] = {
        0xb1, 0xb2, 0xb3, 0xb4};
    unsigned char overlapping[TEST_GENOME_SIZE + 1] = {
        1, 2, 3, 4, 5};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(1.0);
    evo_rng_t rng = {0};

    assert(evo_rng_seed(&rng, 42));
    const evo_rng_t before_rng = rng;

    config.crossover_operator = (evo_crossover_operator_t)99;
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) == EVO_ERROR_RESOURCE_LIMIT);
    assert_rng_equal(&rng, &before_rng);
    assert_bytes_equal(child_a, child_before);

    config.crossover_operator = EVO_CROSSOVER_BYTE_TWO_POINT;
    problem.genome_size = SIZE_MAX;
    config.max_genome_bytes = SIZE_MAX;
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              child_a,
                              child_b) == EVO_ERROR_RESOURCE_LIMIT);
    assert_rng_equal(&rng, &before_rng);
    assert_bytes_equal(child_a, child_before);

    problem = test_problem();
    config = test_config(1.0);
    config.crossover_operator = EVO_CROSSOVER_CONSUMER;
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              parent_a,
                              parent_b,
                              overlapping,
                              overlapping + 1) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_rng_equal(&rng, &before_rng);
    assert_bytes_equal_count(overlapping,
                             (const unsigned char[]){1, 2, 3, 4, 5},
                             sizeof(overlapping));

    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              overlapping,
                              parent_b,
                              overlapping + 1,
                              child_b) == EVO_ERROR_INVALID_ARGUMENT);
    assert_rng_equal(&rng, &before_rng);
    assert_bytes_equal_count(overlapping,
                             (const unsigned char[]){1, 2, 3, 4, 5},
                             sizeof(overlapping));
}

static void test_reference_operator_golden_vectors(void)
{
    static const unsigned char parent_a[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    static const unsigned char parent_b[TEST_GENOME_SIZE] = {
        9, 8, 7, 6};
    static const unsigned char cut_a[TEST_GENOME_SIZE] = {
        1, 2, 3, 6};
    static const unsigned char cut_b[TEST_GENOME_SIZE] = {
        9, 8, 7, 4};
    static const unsigned char uniform_a[TEST_GENOME_SIZE] = {
        9, 2, 3, 6};
    static const unsigned char uniform_b[TEST_GENOME_SIZE] = {
        1, 8, 7, 4};
    unsigned char child_a[TEST_GENOME_SIZE] = {0};
    unsigned char child_b[TEST_GENOME_SIZE] = {0};
    crossover_evidence_t evidence = {0};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(1.0);
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    config.crossover_operator = EVO_CROSSOVER_BYTE_ONE_POINT;
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
    assert_bytes_equal(child_a, cut_a);
    assert_bytes_equal(child_b, cut_b);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x44215383));

    config.crossover_operator = EVO_CROSSOVER_BYTE_TWO_POINT;
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
    assert_bytes_equal(child_a, cut_a);
    assert_bytes_equal(child_b, cut_b);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x68beb632));

    config.crossover_operator = EVO_CROSSOVER_BYTE_UNIFORM;
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
    assert_bytes_equal(child_a, uniform_a);
    assert_bytes_equal(child_b, uniform_b);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x72b7b29b));
}

static void test_reference_boundary_genomes_and_equal_parents(void)
{
    static const unsigned char parent_a[TEST_GENOME_SIZE] = {
        3, 5, 7, 11};
    unsigned char child_a[TEST_GENOME_SIZE] = {0};
    unsigned char child_b[TEST_GENOME_SIZE] = {0};
    unsigned char one_a = 0;
    unsigned char one_b = 0;
    const unsigned char parent_one_a = 0x11;
    const unsigned char parent_one_b = 0x22;
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(1.0);
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    problem.genome_size = 1;
    config.max_genome_bytes = 1;
    config.crossover_operator = EVO_CROSSOVER_BYTE_ONE_POINT;
    assert(evo_rng_seed(&rng, 42));
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              &parent_one_a,
                              &parent_one_b,
                              &one_a,
                              &one_b) == EVO_SUCCESS);
    assert(one_a == parent_one_a);
    assert(one_b == parent_one_b);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x6b07c4a9));

    config.crossover_operator = EVO_CROSSOVER_BYTE_TWO_POINT;
    assert(evo_rng_seed(&rng, 42));
    assert(evo_crossover_pair(&problem,
                              &config,
                              NULL,
                              &rng,
                              &parent_one_a,
                              &parent_one_b,
                              &one_a,
                              &one_b) == EVO_SUCCESS);
    assert(one_a == parent_one_b);
    assert(one_b == parent_one_a);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x68beb632));

    problem.genome_size = TEST_GENOME_SIZE;
    config.max_genome_bytes = TEST_GENOME_SIZE;
    for (int mode = (int)EVO_CROSSOVER_BYTE_ONE_POINT;
         mode <= (int)EVO_CROSSOVER_BYTE_UNIFORM;
         ++mode) {
        config.crossover_operator = (evo_crossover_operator_t)mode;
        assert(evo_rng_seed(&rng, UINT64_C(20260804)));
        assert(evo_crossover_pair(&problem,
                                  &config,
                                  NULL,
                                  &rng,
                                  parent_a,
                                  parent_a,
                                  child_a,
                                  child_b) == EVO_SUCCESS);
        assert_bytes_equal(child_a, parent_a);
        assert_bytes_equal(child_b, parent_a);
    }
}

static void test_reference_cut_coverage(void)
{
    enum {
        COVERAGE_GENOME_SIZE = 5,
        BOUNDARY_COUNT = COVERAGE_GENOME_SIZE + 1
    };
    static const unsigned char parent_a[COVERAGE_GENOME_SIZE] = {0};
    static const unsigned char parent_b[COVERAGE_GENOME_SIZE] = {
        1, 1, 1, 1, 1};
    unsigned char child_a[COVERAGE_GENOME_SIZE] = {0};
    unsigned char child_b[COVERAGE_GENOME_SIZE] = {0};
    bool one_point_seen[BOUNDARY_COUNT] = {false};
    bool two_point_seen[BOUNDARY_COUNT][BOUNDARY_COUNT] = {{false}};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(1.0);

    problem.genome_size = COVERAGE_GENOME_SIZE;
    config.max_genome_bytes = COVERAGE_GENOME_SIZE;
    config.crossover_operator = EVO_CROSSOVER_BYTE_ONE_POINT;
    for (uint64_t seed = 0; seed < UINT64_C(4096); ++seed) {
        evo_rng_t rng = {0};
        size_t cut = 0;

        assert(evo_rng_seed(&rng, seed));
        assert(evo_crossover_pair(&problem,
                                  &config,
                                  NULL,
                                  &rng,
                                  parent_a,
                                  parent_b,
                                  child_a,
                                  child_b) == EVO_SUCCESS);
        while (cut < COVERAGE_GENOME_SIZE && child_a[cut] == 0) {
            ++cut;
        }
        assert(cut > 0 && cut < COVERAGE_GENOME_SIZE);
        one_point_seen[cut] = true;
    }
    for (size_t cut = 1; cut < COVERAGE_GENOME_SIZE; ++cut) {
        assert(one_point_seen[cut]);
    }

    config.crossover_operator = EVO_CROSSOVER_BYTE_TWO_POINT;
    for (uint64_t seed = 0; seed < UINT64_C(4096); ++seed) {
        evo_rng_t rng = {0};
        size_t lower = 0;
        size_t upper = 0;

        assert(evo_rng_seed(&rng, seed));
        assert(evo_crossover_pair(&problem,
                                  &config,
                                  NULL,
                                  &rng,
                                  parent_a,
                                  parent_b,
                                  child_a,
                                  child_b) == EVO_SUCCESS);
        while (lower < COVERAGE_GENOME_SIZE && child_a[lower] == 0) {
            ++lower;
        }
        upper = lower;
        while (upper < COVERAGE_GENOME_SIZE && child_a[upper] == 1) {
            ++upper;
        }
        assert(lower < upper);
        for (size_t offset = upper;
             offset < COVERAGE_GENOME_SIZE;
             ++offset) {
            assert(child_a[offset] == 0);
        }
        two_point_seen[lower][upper] = true;
    }
    for (size_t lower = 0; lower < BOUNDARY_COUNT; ++lower) {
        for (size_t upper = lower + 1;
             upper < BOUNDARY_COUNT;
             ++upper) {
            assert(two_point_seen[lower][upper]);
        }
    }
}

static void test_uniform_odd_tail_and_rate_zero(void)
{
    enum {
        ODD_GENOME_SIZE = 33
    };
    unsigned char parent_a[ODD_GENOME_SIZE] = {0};
    unsigned char parent_b[ODD_GENOME_SIZE] = {0};
    unsigned char guarded_a[ODD_GENOME_SIZE + 2] = {0};
    unsigned char guarded_b[ODD_GENOME_SIZE + 2] = {0};
    crossover_evidence_t evidence = {0};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(1.0);
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    for (size_t index = 0; index < ODD_GENOME_SIZE; ++index) {
        parent_a[index] = (unsigned char)index;
        parent_b[index] = (unsigned char)(UINT8_MAX - index);
    }
    guarded_a[0] = 0xa5;
    guarded_a[ODD_GENOME_SIZE + 1] = 0x5a;
    guarded_b[0] = 0xc3;
    guarded_b[ODD_GENOME_SIZE + 1] = 0x3c;
    problem.genome_size = ODD_GENOME_SIZE;
    config.max_genome_bytes = ODD_GENOME_SIZE;
    config.crossover_operator = EVO_CROSSOVER_BYTE_UNIFORM;

    assert(evo_rng_seed(&rng, 42));
    assert(evo_crossover_pair(&problem,
                              &config,
                              &evidence,
                              &rng,
                              parent_a,
                              parent_b,
                              guarded_a + 1,
                              guarded_b + 1) == EVO_SUCCESS);
    assert(evidence.calls == 0);
    for (size_t index = 0; index < ODD_GENOME_SIZE; ++index) {
        const bool corresponding =
            guarded_a[index + 1] == parent_a[index];

        assert(corresponding ||
               guarded_a[index + 1] == parent_b[index]);
        assert(guarded_b[index + 1] ==
               (corresponding ? parent_b[index] : parent_a[index]));
    }
    assert(guarded_a[0] == 0xa5);
    assert(guarded_a[ODD_GENOME_SIZE + 1] == 0x5a);
    assert(guarded_b[0] == 0xc3);
    assert(guarded_b[ODD_GENOME_SIZE + 1] == 0x3c);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x44215383));

    config.crossover_rate = 0.0;
    assert(evo_rng_seed(&rng, 42));
    assert(evo_crossover_pair(&problem,
                              &config,
                              &evidence,
                              &rng,
                              parent_a,
                              parent_b,
                              guarded_a + 1,
                              guarded_b + 1) == EVO_SUCCESS);
    assert_bytes_equal_count(guarded_a + 1,
                             parent_a,
                             ODD_GENOME_SIZE);
    assert_bytes_equal_count(guarded_b + 1,
                             parent_b,
                             ODD_GENOME_SIZE);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x6b07c4a9));
}

int main(void)
{
    test_invalid_input_preserves_state();
    test_unseeded_rng_preserves_children();
    test_clone_paths_consume_one_word();
    test_fixed_gate_and_callback_dispatch();
    test_deterministic_replay();
    test_invalid_operator_and_partial_overlap_preserve_state();
    test_reference_operator_golden_vectors();
    test_reference_boundary_genomes_and_equal_parents();
    test_reference_cut_coverage();
    test_uniform_odd_tail_and_rate_zero();
    return 0;
}
