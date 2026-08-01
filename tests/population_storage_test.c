#include "internal/population_storage.h"

#include <assert.h>
#include <stdint.h>

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

static evo_problem_t test_problem(size_t genome_size)
{
    evo_problem_t problem = {0};
    problem.genome_size = genome_size;
    return problem;
}

static evo_config_t test_config(size_t population_size,
                                size_t max_genome_bytes,
                                size_t max_population_bytes)
{
    evo_config_t config = {0};
    config.population_size = population_size;
    config.max_genome_bytes = max_genome_bytes;
    config.max_population_bytes = max_population_bytes;
    return config;
}

static void test_invalid_arguments_leave_empty_population(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, 4, 8);
    evo_population_t population = {
        .population_size = 11,
        .genome_size = 12,
        .storage_bytes = 13,
    };

    assert(evo_population_create(&problem, &config, NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_create(NULL, &config, &population) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_population_empty(&population);

    population.population_size = 11;
    assert(evo_population_create(&problem, NULL, &population) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_population_empty(&population);
}

static void test_resource_limit_rejections(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, 4, 8);
    evo_population_t population = {0};

    problem.genome_size = 0;
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_population_empty(&population);

    problem.genome_size = 4;
    config.population_size = 0;
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_population_empty(&population);

    config.population_size = 2;
    config.max_genome_bytes = 0;
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_population_empty(&population);

    config.max_genome_bytes = 3;
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_population_empty(&population);

    config.max_genome_bytes = 4;
    config.max_population_bytes = 0;
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_population_empty(&population);

    config.max_population_bytes = 7;
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_population_empty(&population);
}

static void test_population_size_overflow_rejection(void)
{
    evo_problem_t problem = test_problem(2);
    evo_config_t config = test_config(SIZE_MAX, 2, SIZE_MAX);
    evo_population_t population = {0};

    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_population_empty(&population);
}

static void test_contiguous_zero_initialized_storage_and_access(void)
{
    evo_problem_t problem = test_problem(8);
    evo_config_t config = test_config(3, 8, 24);
    evo_population_t population = {0};

    assert(evo_population_create(&problem, &config, &population) ==
           EVO_SUCCESS);
    assert(population.genomes != NULL);
    assert(population.population_size == 3);
    assert(population.genome_size == 8);
    assert(population.storage_bytes == 24);

    for (size_t index = 0; index < population.storage_bytes; ++index) {
        assert(population.genomes[index] == 0);
    }

    unsigned char *first = evo_population_genome(&population, 0);
    unsigned char *middle = evo_population_genome(&population, 1);
    unsigned char *last = evo_population_genome(&population, 2);

    assert(first != NULL);
    assert(middle != NULL);
    assert(last != NULL);
    assert(first == population.genomes);
    assert(middle == population.genomes + 8);
    assert(last == population.genomes + 16);
    assert(evo_population_genome(&population, 3) == NULL);
    assert(evo_population_genome(NULL, 0) == NULL);

    first[0] = 0x11;
    middle[0] = 0x22;
    last[7] = 0x33;
    assert(population.genomes[0] == 0x11);
    assert(population.genomes[8] == 0x22);
    assert(population.genomes[23] == 0x33);

    const evo_population_t *const_population = &population;
    assert(evo_population_genome_const(const_population, 1) == middle);
    assert(evo_population_genome_const(const_population, 3) == NULL);
    assert(evo_population_genome_const(NULL, 0) == NULL);

    evo_population_t inconsistent = population;
    inconsistent.storage_bytes = 7;
    assert(evo_population_genome(&inconsistent, 1) == NULL);
    inconsistent.storage_bytes = population.storage_bytes;
    inconsistent.genome_size = SIZE_MAX;
    assert(evo_population_genome(&inconsistent, 1) == NULL);

    evo_population_destroy(&population);
    assert_population_empty(&population);
}

static void test_active_rejection_and_destruction_idempotency(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, 4, 8);
    evo_population_t population = {0};

    evo_population_destroy(NULL);
    evo_population_destroy(&population);
    assert_population_empty(&population);

    assert(evo_population_create(&problem, &config, &population) ==
           EVO_SUCCESS);
    const evo_population_t active = population;

    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(population.genomes == active.genomes);
    assert(population.population_size == active.population_size);
    assert(population.genome_size == active.genome_size);
    assert(population.storage_bytes == active.storage_bytes);
    assert(population.initialization_seed == active.initialization_seed);
    assert(population.source_generation == active.source_generation);
    assert(population.rng_algorithm_version == active.rng_algorithm_version);
    assert(population.operator_seed_schedule_version ==
           active.operator_seed_schedule_version);
    assert(population.produced_count == active.produced_count);
    assert(population.initialized == active.initialized);

    evo_population_destroy(&population);
    assert_population_empty(&population);
    evo_population_destroy(&population);
    assert_population_empty(&population);
}

int main(void)
{
    test_invalid_arguments_leave_empty_population();
    test_resource_limit_rejections();
    test_population_size_overflow_rejection();
    test_contiguous_zero_initialized_storage_and_access();
    test_active_rejection_and_destruction_idempotency();
    return 0;
}
