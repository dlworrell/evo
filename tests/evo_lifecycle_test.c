#include "catalyst/evo/evo.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

enum {
    TEST_POPULATION_CAPACITY = 8,
    TEST_EVENT_CAPACITY = 24
};

typedef struct lifecycle_event {
    char operation;
    size_t index;
} lifecycle_event_t;

typedef struct lifecycle_context {
    bool validity[TEST_POPULATION_CAPACITY];
    evo_fitness_t fitness[TEST_POPULATION_CAPACITY];
    lifecycle_event_t events[TEST_EVENT_CAPACITY];
    size_t genome_size;
    size_t initialization_calls;
    size_t validation_calls;
    size_t evaluation_calls;
    size_t event_count;
} lifecycle_context_t;

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

static void assert_results_equal(const evo_result_t *actual,
                                 const evo_result_t *expected)
{
    assert(actual->best_genome == expected->best_genome);
    assert(actual->best_fitness.correctness ==
           expected->best_fitness.correctness);
    assert(actual->best_fitness.performance ==
           expected->best_fitness.performance);
    assert(actual->best_fitness.memory_use ==
           expected->best_fitness.memory_use);
    assert(actual->best_fitness.reliability ==
           expected->best_fitness.reliability);
    assert(actual->best_fitness.maintainability ==
           expected->best_fitness.maintainability);
    assert(actual->best_fitness.constraint_penalty ==
           expected->best_fitness.constraint_penalty);
    assert(actual->best_fitness.total == expected->best_fitness.total);
    assert(actual->generations_completed == expected->generations_completed);
    assert(actual->random_seed == expected->random_seed);
}

static void record_event(lifecycle_context_t *context,
                         char operation,
                         size_t index)
{
    assert(context->event_count < TEST_EVENT_CAPACITY);
    context->events[context->event_count].operation = operation;
    context->events[context->event_count].index = index;
    ++context->event_count;
}

static size_t genome_index(const void *genome)
{
    const unsigned char *bytes = genome;
    return bytes[0];
}

static void deterministic_initializer(void *genome, void *opaque)
{
    lifecycle_context_t *context = opaque;
    unsigned char *bytes = genome;
    size_t index = context->initialization_calls;

    assert(index < TEST_POPULATION_CAPACITY);
    record_event(context, 'I', index);
    bytes[0] = (unsigned char)index;
    for (size_t offset = 1; offset < context->genome_size; ++offset) {
        bytes[offset] = (unsigned char)(index * 16 + offset);
    }
    ++context->initialization_calls;
}

static bool deterministic_validator(const void *genome, void *opaque)
{
    lifecycle_context_t *context = opaque;
    size_t index = genome_index(genome);

    assert(index < TEST_POPULATION_CAPACITY);
    record_event(context, 'V', index);
    ++context->validation_calls;
    return context->validity[index];
}

static evo_fitness_t deterministic_evaluator(const void *genome, void *opaque)
{
    lifecycle_context_t *context = opaque;
    size_t index = genome_index(genome);

    assert(index < TEST_POPULATION_CAPACITY);
    record_event(context, 'E', index);
    ++context->evaluation_calls;
    return context->fitness[index];
}

static evo_fitness_t constant_evaluator(const void *genome, void *context)
{
    (void)genome;
    (void)context;
    return (evo_fitness_t){.total = 1.0};
}

static evo_fitness_t fitness_with_total(double total)
{
    return (evo_fitness_t){
        .correctness = total + 1.0,
        .performance = total + 2.0,
        .memory_use = total + 3.0,
        .reliability = total + 4.0,
        .maintainability = total + 5.0,
        .constraint_penalty = total + 6.0,
        .total = total,
    };
}

static evo_problem_t test_problem(size_t genome_size)
{
    evo_problem_t problem = {
        .genome_size = genome_size,
        .evaluate = constant_evaluator,
    };
    return problem;
}

static evo_config_t test_config(size_t population_size,
                                size_t genome_size,
                                uint64_t random_seed)
{
    evo_config_t config = {
        .population_size = population_size,
        .random_seed = random_seed,
        .max_genome_bytes = genome_size,
        .max_population_bytes = population_size * genome_size,
        .max_evaluation_bytes = SIZE_MAX,
    };
    return config;
}

static void assert_event(const lifecycle_context_t *context,
                         size_t position,
                         char operation,
                         size_t index)
{
    assert(position < context->event_count);
    assert(context->events[position].operation == operation);
    assert(context->events[position].index == index);
}

static void test_invalid_argument_handling(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, problem.genome_size, 1);
    lifecycle_context_t context = {
        .genome_size = problem.genome_size,
    };
    evo_result_t result = {0};

    assert(evo_run(NULL, NULL, NULL, NULL) == EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_run(NULL, &config, NULL, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_completely_empty(&result);

    assert(evo_run(&problem, NULL, NULL, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_completely_empty(&result);

    problem.initialize = deterministic_initializer;
    problem.evaluate = NULL;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_completely_empty(&result);
    assert(context.initialization_calls == 0);
}

static void test_resource_limit_rejections(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, problem.genome_size, 2);
    evo_result_t result = {0};

    problem.genome_size = 0;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);

    problem.genome_size = 4;
    config.population_size = 0;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);

    config.population_size = 2;
    config.max_genome_bytes = 0;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);

    config.max_genome_bytes = 3;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);

    config.max_genome_bytes = 4;
    config.max_population_bytes = 7;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);

    config.max_population_bytes = 8;
    config.max_evaluation_bytes = 1;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);

    problem.genome_size = SIZE_MAX;
    config.population_size = 2;
    config.max_genome_bytes = SIZE_MAX;
    config.max_population_bytes = SIZE_MAX;
    config.max_evaluation_bytes = SIZE_MAX;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_completely_empty(&result);
}

static void test_generation_zero_execution_and_winner_transfer(void)
{
    static const lifecycle_event_t expected_events[] = {
        {'I', 0},
        {'I', 1},
        {'I', 2},
        {'I', 3},
        {'V', 0},
        {'V', 1},
        {'V', 2},
        {'V', 3},
        {'E', 0},
        {'E', 2},
        {'E', 3},
    };
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(4, problem.genome_size, 42);
    lifecycle_context_t context = {
        .validity = {true, false, true, true},
        .genome_size = problem.genome_size,
    };
    evo_result_t result = {0};

    problem.initialize = deterministic_initializer;
    problem.is_valid = deterministic_validator;
    problem.evaluate = deterministic_evaluator;
    context.fitness[0] = fitness_with_total(2.0);
    context.fitness[2] = fitness_with_total(8.0);
    context.fitness[3] = fitness_with_total(7.0);

    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.best_genome != NULL);
    assert(result.best_fitness.correctness == 9.0);
    assert(result.best_fitness.performance == 10.0);
    assert(result.best_fitness.memory_use == 11.0);
    assert(result.best_fitness.reliability == 12.0);
    assert(result.best_fitness.maintainability == 13.0);
    assert(result.best_fitness.constraint_penalty == 14.0);
    assert(result.best_fitness.total == 8.0);
    assert(result.generations_completed == 0);
    assert(result.random_seed == 42);

    const unsigned char *winner = result.best_genome;
    assert(winner[0] == 2);
    for (size_t offset = 1; offset < problem.genome_size; ++offset) {
        assert(winner[offset] == (unsigned char)(2 * 16 + offset));
    }

    assert(context.initialization_calls == 4);
    assert(context.validation_calls == 4);
    assert(context.evaluation_calls == 3);
    assert(context.event_count ==
           sizeof(expected_events) / sizeof(expected_events[0]));
    for (size_t position = 0;
         position < sizeof(expected_events) / sizeof(expected_events[0]);
         ++position) {
        assert_event(&context,
                     position,
                     expected_events[position].operation,
                     expected_events[position].index);
    }

    evo_result_destroy(&result);
    assert_completely_empty(&result);
}

static void test_exact_tie_preserves_lower_index(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(3, problem.genome_size, 43);
    lifecycle_context_t context = {
        .validity = {true, true, true},
        .genome_size = problem.genome_size,
    };
    evo_result_t result = {0};

    problem.initialize = deterministic_initializer;
    problem.evaluate = deterministic_evaluator;
    context.fitness[0] = fitness_with_total(5.0);
    context.fitness[1] = fitness_with_total(5.0);
    context.fitness[2] = fitness_with_total(5.0);

    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.best_genome != NULL);
    assert(((const unsigned char *)result.best_genome)[0] == 0);
    assert(result.best_fitness.total == 5.0);

    evo_result_destroy(&result);
}

static void test_all_invalid_has_distinct_empty_result(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(3, problem.genome_size, 44);
    lifecycle_context_t context = {
        .validity = {false, false, false},
        .genome_size = problem.genome_size,
    };
    evo_result_t result = {0};

    problem.initialize = deterministic_initializer;
    problem.is_valid = deterministic_validator;
    problem.evaluate = deterministic_evaluator;

    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_NO_VALID_CANDIDATE);
    assert_completely_empty(&result);
    assert(context.initialization_calls == 3);
    assert(context.validation_calls == 3);
    assert(context.evaluation_calls == 0);
}

static void test_non_finite_evaluation_has_empty_result(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, problem.genome_size, 45);
    lifecycle_context_t context = {
        .validity = {true, true},
        .genome_size = problem.genome_size,
    };
    evo_result_t result = {0};

    problem.initialize = deterministic_initializer;
    problem.evaluate = deterministic_evaluator;
    context.fitness[0] = fitness_with_total(1.0);
    context.fitness[1] = fitness_with_total(2.0);
    context.fitness[1].reliability = NAN;

    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_EVALUATION);
    assert_completely_empty(&result);
}

static void test_destruction_idempotency(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, problem.genome_size, 46);
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
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, problem.genome_size, 47);
    lifecycle_context_t context = {
        .validity = {true, true},
        .genome_size = problem.genome_size,
    };
    evo_result_t result = {0};

    problem.initialize = deterministic_initializer;
    problem.evaluate = deterministic_evaluator;
    context.fitness[0] = fitness_with_total(1.0);
    context.fitness[1] = fitness_with_total(2.0);

    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    const evo_result_t active = result;
    context = (lifecycle_context_t){
        .validity = {true, true},
        .genome_size = problem.genome_size,
    };

    assert(evo_run(NULL, NULL, &context, &result) ==
           EVO_ERROR_RESULT_ACTIVE);
    assert_results_equal(&result, &active);
    assert(context.initialization_calls == 0);
    assert(context.validation_calls == 0);
    assert(context.evaluation_calls == 0);

    evo_result_destroy(&result);
    assert_completely_empty(&result);

    context.fitness[0] = fitness_with_total(1.0);
    context.fitness[1] = fitness_with_total(2.0);
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.best_genome != NULL);

    evo_result_destroy(&result);
    assert_completely_empty(&result);
}

int main(void)
{
    test_invalid_argument_handling();
    test_resource_limit_rejections();
    test_generation_zero_execution_and_winner_transfer();
    test_exact_tie_preserves_lower_index();
    test_all_invalid_has_distinct_empty_result();
    test_non_finite_evaluation_has_empty_result();
    test_destruction_idempotency();
    test_active_result_rejection_and_reuse();
    return 0;
}
