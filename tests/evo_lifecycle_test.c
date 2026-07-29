#include "catalyst/evo/evo.h"

#include <assert.h>

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

static void assert_results_equal(const evo_result_t *actual, const evo_result_t *expected)
{
    assert(actual->best_genome == expected->best_genome);
    assert(actual->best_fitness.correctness == expected->best_fitness.correctness);
    assert(actual->best_fitness.performance == expected->best_fitness.performance);
    assert(actual->best_fitness.memory_use == expected->best_fitness.memory_use);
    assert(actual->best_fitness.reliability == expected->best_fitness.reliability);
    assert(actual->best_fitness.maintainability == expected->best_fitness.maintainability);
    assert(actual->best_fitness.constraint_penalty == expected->best_fitness.constraint_penalty);
    assert(actual->best_fitness.total == expected->best_fitness.total);
    assert(actual->generations_completed == expected->generations_completed);
    assert(actual->random_seed == expected->random_seed);
}

static void test_invalid_argument_handling(void)
{
    evo_result_t result = {0};

    assert(evo_run(NULL, NULL, NULL, NULL) == EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_run(NULL, NULL, NULL, &result) == EVO_ERROR_INVALID_ARGUMENT);
    assert_completely_empty(&result);
}

static void test_resource_limit_rejections(void)
{
    evo_problem_t problem = {.genome_size = 1};
    evo_config_t config = {.population_size = 1, .max_genome_bytes = 1};
    evo_result_t result = {0};

    problem.genome_size = 0;
    assert(evo_run(&problem, &config, NULL, &result) == EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);

    problem.genome_size = 1;
    config.population_size = 0;
    assert(evo_run(&problem, &config, NULL, &result) == EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);

    config.population_size = 1;
    config.max_genome_bytes = 0;
    assert(evo_run(&problem, &config, NULL, &result) == EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);

    problem.genome_size = 5000;
    config.max_genome_bytes = 2000;
    assert(evo_run(&problem, &config, NULL, &result) == EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);
}

static void test_successful_allocation_and_zero_initialization(void)
{
    evo_problem_t problem = {.genome_size = 64};
    evo_config_t config = {.population_size = 5, .random_seed = 42, .max_genome_bytes = 128};
    evo_result_t result = {0};

    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    assert(result.best_genome != NULL);
    assert(result.best_fitness.correctness == 0.0);
    assert(result.best_fitness.performance == 0.0);
    assert(result.best_fitness.memory_use == 0.0);
    assert(result.best_fitness.reliability == 0.0);
    assert(result.best_fitness.maintainability == 0.0);
    assert(result.best_fitness.constraint_penalty == 0.0);
    assert(result.best_fitness.total == 0.0);
    assert(result.generations_completed == 0);
    assert(result.random_seed == 42);

    const unsigned char *bytes = result.best_genome;
    for (size_t index = 0; index < problem.genome_size; ++index) {
        assert(bytes[index] == 0);
    }

    evo_result_destroy(&result);
    assert_completely_empty(&result);
}

static void test_destruction_idempotency(void)
{
    evo_problem_t problem = {.genome_size = 32};
    evo_config_t config = {.population_size = 2, .max_genome_bytes = 64};
    evo_result_t result = {0};

    evo_result_destroy(NULL);
    evo_result_destroy(&result);
    assert_completely_empty(&result);

    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    evo_result_destroy(&result);
    assert_completely_empty(&result);

    evo_result_destroy(&result);
    assert_completely_empty(&result);
}

static void test_active_result_rejection_and_reuse(void)
{
    evo_problem_t problem = {.genome_size = 16};
    evo_config_t config = {.population_size = 2, .random_seed = 101, .max_genome_bytes = 32};
    evo_result_t result = {0};

    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    const evo_result_t active = result;

    assert(evo_run(&problem, &config, NULL, &result) == EVO_ERROR_RESULT_ACTIVE);
    assert_results_equal(&result, &active);

    evo_result_destroy(&result);
    assert_completely_empty(&result);

    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    assert(result.best_genome != NULL);

    evo_result_destroy(&result);
    assert_completely_empty(&result);
}

int main(void)
{
    test_invalid_argument_handling();
    test_resource_limit_rejections();
    test_successful_allocation_and_zero_initialization();
    test_destruction_idempotency();
    test_active_result_rejection_and_reuse();
    return 0;
}
