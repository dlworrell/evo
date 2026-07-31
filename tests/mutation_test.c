#include "internal/mutation.h"

#include <assert.h>
#include <math.h>

enum {
    TEST_GENOME_SIZE = 4
};

typedef struct mutation_evidence {
    size_t calls;
    void *genome;
    double mutation_rate;
    unsigned char xor_mask;
} mutation_evidence_t;

static void test_mutation(void *genome,
                          double mutation_rate,
                          void *context)
{
    unsigned char *bytes = genome;
    mutation_evidence_t *evidence = context;

    assert(evidence != NULL);
    ++evidence->calls;
    evidence->genome = genome;
    evidence->mutation_rate = mutation_rate;

    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        bytes[index] ^=
            (unsigned char)(evidence->xor_mask + (unsigned char)index);
    }
}

static evo_problem_t test_problem(void)
{
    evo_problem_t problem = {0};

    problem.genome_size = TEST_GENOME_SIZE;
    problem.mutate = test_mutation;
    return problem;
}

static evo_config_t test_config(double mutation_rate)
{
    evo_config_t config = {0};

    config.mutation_rate = mutation_rate;
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
    static const unsigned char genome_before[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    unsigned char genome[TEST_GENOME_SIZE] = {1, 2, 3, 4};
    mutation_evidence_t evidence = {
        .xor_mask = 0x10};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.5);
    evo_rng_t rng = {0};

    assert(evo_rng_seed(&rng, 42));
    const evo_rng_t before_rng = rng;

    assert(evo_mutate_genome(NULL,
                             &config,
                             &evidence,
                             &rng,
                             genome) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_mutate_genome(&problem,
                             NULL,
                             &evidence,
                             &rng,
                             genome) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             NULL,
                             genome) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);

    problem.genome_size = 0;
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) ==
           EVO_ERROR_RESOURCE_LIMIT);
    problem.genome_size = TEST_GENOME_SIZE;
    config.max_genome_bytes = 0;
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) ==
           EVO_ERROR_RESOURCE_LIMIT);
    config.max_genome_bytes = TEST_GENOME_SIZE - 1;
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) ==
           EVO_ERROR_RESOURCE_LIMIT);

    config.max_genome_bytes = TEST_GENOME_SIZE;
    config.mutation_rate = NAN;
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) ==
           EVO_ERROR_RESOURCE_LIMIT);
    config.mutation_rate = INFINITY;
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) ==
           EVO_ERROR_RESOURCE_LIMIT);
    config.mutation_rate = -0.1;
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) ==
           EVO_ERROR_RESOURCE_LIMIT);
    config.mutation_rate = 1.1;
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) ==
           EVO_ERROR_RESOURCE_LIMIT);

    assert(evidence.calls == 0);
    assert_rng_equal(&rng, &before_rng);
    assert_bytes_equal(genome, genome_before);
}

static void test_unseeded_rng_preserves_genome(void)
{
    static const unsigned char genome_before[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    unsigned char genome[TEST_GENOME_SIZE] = {1, 2, 3, 4};
    mutation_evidence_t evidence = {
        .xor_mask = 0x10};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.5);
    evo_rng_t rng = {0};

    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) == EVO_ERROR_STATE);
    assert(evidence.calls == 0);
    assert_bytes_equal(genome, genome_before);
}

static void test_noop_paths_consume_one_word(void)
{
    static const unsigned char genome_before[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    unsigned char genome[TEST_GENOME_SIZE] = {1, 2, 3, 4};
    mutation_evidence_t evidence = {
        .xor_mask = 0x10};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.0);
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    assert(evo_rng_seed(&rng, 42));
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) == EVO_SUCCESS);
    assert(evidence.calls == 0);
    assert_bytes_equal(genome, genome_before);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x6b07c4a9));

    problem.mutate = NULL;
    config.mutation_rate = 1.0;
    assert(evo_rng_seed(&rng, 42));
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) == EVO_SUCCESS);
    assert(evidence.calls == 0);
    assert_bytes_equal(genome, genome_before);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x6b07c4a9));
}

static void test_fixed_gate_and_callback_dispatch(void)
{
    static const unsigned char genome_before[TEST_GENOME_SIZE] = {
        1, 2, 3, 4};
    static const unsigned char genome_after[TEST_GENOME_SIZE] = {
        0x11, 0x13, 0x11, 0x17};
    unsigned char genome[TEST_GENOME_SIZE] = {1, 2, 3, 4};
    mutation_evidence_t evidence = {
        .xor_mask = 0x10};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.5);
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    assert(evo_rng_seed(&rng, 42));

    /* 0xc2f57bd6 is above the half-range threshold: no mutation. */
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) == EVO_SUCCESS);
    assert(evidence.calls == 0);
    assert_bytes_equal(genome, genome_before);

    /* 0x6b07c4a9 is below the threshold: dispatch exactly once. */
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) == EVO_SUCCESS);
    assert(evidence.calls == 1);
    assert(evidence.genome == genome);
    assert(evidence.mutation_rate == config.mutation_rate);
    assert_bytes_equal(genome, genome_after);

    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x72b7b29b));
}

static void test_rate_one_dispatches_once(void)
{
    static const unsigned char genome_after[TEST_GENOME_SIZE] = {
        0x21, 0x23, 0x21, 0x27};
    unsigned char genome[TEST_GENOME_SIZE] = {1, 2, 3, 4};
    mutation_evidence_t evidence = {
        .xor_mask = 0x20};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(1.0);
    evo_rng_t rng = {0};
    uint32_t next_value = 0;

    assert(evo_rng_seed(&rng, 42));
    assert(evo_mutate_genome(&problem,
                             &config,
                             &evidence,
                             &rng,
                             genome) == EVO_SUCCESS);
    assert(evidence.calls == 1);
    assert(evidence.mutation_rate == 1.0);
    assert_bytes_equal(genome, genome_after);
    assert(evo_rng_next_u32(&rng, &next_value));
    assert(next_value == UINT32_C(0x6b07c4a9));
}

static void test_deterministic_replay(void)
{
    unsigned char first_genome[TEST_GENOME_SIZE] = {3, 5, 7, 11};
    unsigned char replay_genome[TEST_GENOME_SIZE] = {3, 5, 7, 11};
    mutation_evidence_t first_evidence = {
        .xor_mask = 0x30};
    mutation_evidence_t replay_evidence = {
        .xor_mask = 0x30};
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config(0.375);
    evo_rng_t first = {0};
    evo_rng_t replay = {0};

    assert(evo_rng_seed(&first, UINT64_C(20260731)));
    assert(evo_rng_seed(&replay, UINT64_C(20260731)));

    for (size_t attempt = 0; attempt < 32; ++attempt) {
        assert(evo_mutate_genome(&problem,
                                 &config,
                                 &first_evidence,
                                 &first,
                                 first_genome) == EVO_SUCCESS);
        assert(evo_mutate_genome(&problem,
                                 &config,
                                 &replay_evidence,
                                 &replay,
                                 replay_genome) == EVO_SUCCESS);
        assert_bytes_equal(first_genome, replay_genome);
        assert(first_evidence.calls == replay_evidence.calls);
    }

    assert_rng_equal(&first, &replay);
}

int main(void)
{
    test_invalid_input_preserves_state();
    test_unseeded_rng_preserves_genome();
    test_noop_paths_consume_one_word();
    test_fixed_gate_and_callback_dispatch();
    test_rate_one_dispatches_once();
    test_deterministic_replay();
    return 0;
}
