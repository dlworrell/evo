#include "internal/population_storage.h"
#include "internal/rng.h"

#include <assert.h>

typedef struct initializer_context {
    unsigned char *population_base;
    size_t genome_size;
    size_t expected_index;
    size_t calls;
    size_t validity_calls;
    size_t evaluation_calls;
} initializer_context_t;

static evo_problem_t test_problem(size_t genome_size)
{
    evo_problem_t problem = {0};
    problem.genome_size = genome_size;
    return problem;
}

static evo_config_t test_config(size_t population_size,
                                size_t genome_size,
                                uint64_t random_seed)
{
    evo_config_t config = {0};
    config.population_size = population_size;
    config.random_seed = random_seed;
    config.max_genome_bytes = genome_size;
    config.max_population_bytes = population_size * genome_size;
    return config;
}

static void assert_population_empty(const evo_population_t *population)
{
    assert(population->genomes == NULL);
    assert(population->evaluations == NULL);
    assert(population->population_size == 0);
    assert(population->genome_size == 0);
    assert(population->storage_bytes == 0);
    assert(population->evaluation_bytes == 0);
    assert(population->valid_count == 0);
    assert(population->best_index == 0);
    assert(population->produced_count == 0);
    assert(population->initialization_seed == 0);
    assert(population->source_generation == 0);
    assert(population->rng_algorithm_version == 0);
    assert(population->operator_seed_schedule_version == 0);
    assert(!population->initialized);
    assert(!population->has_best);
    assert(!population->evaluated);
}

static void deterministic_initializer(void *genome, void *context)
{
    initializer_context_t *initializer = context;
    unsigned char *bytes = genome;

    assert(bytes ==
           initializer->population_base +
               initializer->expected_index * initializer->genome_size);

    bytes[0] ^= (unsigned char)(0xa0u + initializer->expected_index);
    bytes[initializer->genome_size - 1] ^=
        (unsigned char)(0x50u + initializer->expected_index);
    ++initializer->expected_index;
    ++initializer->calls;
}

static bool unexpected_validity_callback(const void *genome, void *context)
{
    initializer_context_t *initializer = context;

    (void)genome;
    ++initializer->validity_calls;
    return true;
}

static evo_fitness_t unexpected_evaluation_callback(const void *genome,
                                                    void *context)
{
    initializer_context_t *initializer = context;

    (void)genome;
    ++initializer->evaluation_calls;
    return (evo_fitness_t){0};
}

static void assert_populations_equal(const evo_population_t *left,
                                     const evo_population_t *right)
{
    assert(left->population_size == right->population_size);
    assert(left->genome_size == right->genome_size);
    assert(left->storage_bytes == right->storage_bytes);
    assert(left->evaluation_bytes == right->evaluation_bytes);
    assert(left->valid_count == right->valid_count);
    assert(left->best_index == right->best_index);
    assert(left->produced_count == right->produced_count);
    assert(left->initialization_seed == right->initialization_seed);
    assert(left->source_generation == right->source_generation);
    assert(left->rng_algorithm_version == right->rng_algorithm_version);
    assert(left->operator_seed_schedule_version ==
           right->operator_seed_schedule_version);
    assert(left->initialized == right->initialized);
    assert(left->has_best == right->has_best);
    assert(left->evaluated == right->evaluated);

    for (size_t index = 0; index < left->storage_bytes; ++index) {
        assert(left->genomes[index] == right->genomes[index]);
    }
}

static void test_raw_population_fixed_vectors(void)
{
    static const unsigned char seed_42_expected[] = {
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
    static const unsigned char seed_43_expected[] = {
        0x5f,
        0x5e,
        0xca,
        0xcc,
        0x32,
        0x27,
        0x0a,
        0x66,
        0x3f,
        0x78,
        0x0e,
        0x1b,
        0xb8,
        0xc0,
        0x61,
        0x5c,
    };
    evo_problem_t problem = test_problem(8);
    evo_config_t config = test_config(3, problem.genome_size, 42);
    evo_population_t first = {0};
    evo_population_t replay = {0};
    evo_population_t different = {0};

    assert(evo_population_create(&problem, &config, &first) == EVO_SUCCESS);
    assert(evo_population_create(&problem, &config, &replay) == EVO_SUCCESS);
    assert(evo_population_initialize(&problem, &config, NULL, &first) ==
           EVO_SUCCESS);
    assert(evo_population_initialize(&problem, &config, NULL, &replay) ==
           EVO_SUCCESS);
    assert_populations_equal(&first, &replay);

    for (size_t index = 0; index < sizeof(seed_42_expected); ++index) {
        assert(first.genomes[index] == seed_42_expected[index]);
    }

    config.random_seed = 43;
    assert(evo_population_create(&problem, &config, &different) ==
           EVO_SUCCESS);
    assert(evo_population_initialize(&problem, &config, NULL, &different) ==
           EVO_SUCCESS);
    for (size_t index = 0; index < sizeof(seed_43_expected); ++index) {
        assert(different.genomes[index] == seed_43_expected[index]);
    }

    assert(first.initialization_seed == 42);
    assert(different.initialization_seed == 43);
    assert(first.rng_algorithm_version == EVO_RNG_ALGORITHM_VERSION);
    assert(first.initialized);

    evo_population_destroy(&different);
    evo_population_destroy(&replay);
    evo_population_destroy(&first);
}

static void test_zero_seed_and_callback_reproducibility(void)
{
    static const unsigned char zero_seed_prefix[] = {
        0x4e,
        0xa2,
        0x23,
        0xe8,
        0xd9,
        0xcb,
        0x7e,
        0x7a,
    };
    evo_problem_t problem = test_problem(8);
    evo_config_t config = test_config(3, problem.genome_size, 0);
    evo_population_t first = {0};
    evo_population_t replay = {0};
    initializer_context_t first_context = {0};
    initializer_context_t replay_context = {0};

    problem.initialize = deterministic_initializer;
    problem.is_valid = unexpected_validity_callback;
    problem.evaluate = unexpected_evaluation_callback;
    assert(evo_population_create(&problem, &config, &first) == EVO_SUCCESS);
    assert(evo_population_create(&problem, &config, &replay) == EVO_SUCCESS);

    first_context.population_base = first.genomes;
    first_context.genome_size = first.genome_size;
    replay_context.population_base = replay.genomes;
    replay_context.genome_size = replay.genome_size;

    assert(evo_population_initialize(
               &problem, &config, &first_context, &first) == EVO_SUCCESS);
    assert(evo_population_initialize(
               &problem, &config, &replay_context, &replay) == EVO_SUCCESS);

    assert(first_context.calls == config.population_size);
    assert(first_context.expected_index == config.population_size);
    assert(replay_context.calls == config.population_size);
    assert(replay_context.expected_index == config.population_size);
    assert(first_context.validity_calls == 0);
    assert(first_context.evaluation_calls == 0);
    assert(replay_context.validity_calls == 0);
    assert(replay_context.evaluation_calls == 0);
    assert_populations_equal(&first, &replay);

    assert(first.genomes[0] == (unsigned char)(zero_seed_prefix[0] ^ 0xa0u));
    for (size_t index = 1; index < sizeof(zero_seed_prefix) - 1; ++index) {
        assert(first.genomes[index] == zero_seed_prefix[index]);
    }
    assert(first.genomes[sizeof(zero_seed_prefix) - 1] ==
           (unsigned char)(zero_seed_prefix[sizeof(zero_seed_prefix) - 1] ^
                           0x50u));

    evo_population_destroy(&replay);
    evo_population_destroy(&first);
    assert_population_empty(&first);
}

static void test_invalid_and_inconsistent_state_rejection(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, problem.genome_size, 7);
    evo_population_t population = {0};

    assert(evo_population_initialize(NULL, &config, NULL, &population) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_initialize(&problem, NULL, NULL, &population) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_initialize(&problem, &config, NULL, NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_ERROR_STATE);
    assert_population_empty(&population);

    assert(evo_population_create(&problem, &config, &population) ==
           EVO_SUCCESS);
    population.storage_bytes -= 1;
    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_ERROR_STATE);
    assert(population.storage_bytes == 7);
    for (size_t index = 0; index < 8; ++index) {
        assert(population.genomes[index] == 0);
    }

    population.storage_bytes = 8;
    config.population_size = 3;
    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_ERROR_STATE);
    assert(!population.initialized);

    config.population_size = 2;
    problem.genome_size = 5;
    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_ERROR_STATE);
    assert(!population.initialized);

    problem.genome_size = 4;
    config.max_genome_bytes = 3;
    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_ERROR_STATE);
    assert(!population.initialized);

    config.max_genome_bytes = 4;
    config.max_population_bytes = 7;
    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_ERROR_STATE);
    assert(!population.initialized);

    config.max_population_bytes = 8;
    evo_population_destroy(&population);
    assert_population_empty(&population);
}

static void test_reinitialization_rejection_preserves_population(void)
{
    evo_problem_t problem = test_problem(8);
    evo_config_t config = test_config(2, problem.genome_size, 101);
    evo_population_t population = {0};
    unsigned char snapshot[16] = {0};

    assert(evo_population_create(&problem, &config, &population) ==
           EVO_SUCCESS);
    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_SUCCESS);
    for (size_t index = 0; index < sizeof(snapshot); ++index) {
        snapshot[index] = population.genomes[index];
    }

    config.random_seed = 202;
    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_ERROR_STATE);
    assert(population.initialization_seed == 101);
    assert(population.rng_algorithm_version == EVO_RNG_ALGORITHM_VERSION);
    assert(population.initialized);
    for (size_t index = 0; index < sizeof(snapshot); ++index) {
        assert(population.genomes[index] == snapshot[index]);
    }

    evo_population_destroy(&population);
    assert_population_empty(&population);
    evo_population_destroy(&population);
    assert_population_empty(&population);
}

int main(void)
{
    test_raw_population_fixed_vectors();
    test_zero_seed_and_callback_reproducibility();
    test_invalid_and_inconsistent_state_rejection();
    test_reinitialization_rejection_preserves_population();
    return 0;
}
