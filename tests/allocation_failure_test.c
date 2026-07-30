#include "catalyst/evo/evo.h"
#include "internal/population_storage.h"

#include <assert.h>
#include <stddef.h>

void *__real_calloc(size_t count, size_t size);

static int inject_allocation_failure;

void *__wrap_calloc(size_t count, size_t size)
{
    if (inject_allocation_failure != 0) {
        return NULL;
    }

    return __real_calloc(count, size);
}

static void assert_completely_empty(const evo_result_t *result)
{
    assert(result->best_genome == NULL);
    assert(result->best_fitness.correctness == 0.0);
    assert(result->best_fitness.performance == 0.0);
    assert(result->best_fitness.memory_use == 0.0);
    assert(result->best_fitness.reliability == 0.0);
    assert(result->best_fitness.maintainability == 0.0);
    assert(result->best_fitness.constraint_penalty == 0.0);
    assert(result->best_fitness.total == 0.0);
    assert(result->generations_completed == 0);
    assert(result->random_seed == 0);
}

int main(void)
{
    evo_problem_t problem = {.genome_size = 32};
    evo_config_t config = {
        .population_size = 10,
        .max_genome_bytes = 100,
        .max_population_bytes = 320,
    };
    evo_population_t population = {0};
    evo_result_t result = {0};

    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    evo_result_destroy(&result);
    assert_completely_empty(&result);

    inject_allocation_failure = 1;
    assert(evo_run(&problem, &config, NULL, &result) == EVO_ERROR_OUT_OF_MEMORY);
    inject_allocation_failure = 0;

    assert_completely_empty(&result);

    inject_allocation_failure = 1;
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_OUT_OF_MEMORY);
    inject_allocation_failure = 0;

    assert(population.genomes == NULL);
    assert(population.population_size == 0);
    assert(population.genome_size == 0);
    assert(population.storage_bytes == 0);
    assert(population.initialization_seed == 0);
    assert(population.rng_algorithm_version == 0);
    assert(!population.initialized);
    return 0;
}
