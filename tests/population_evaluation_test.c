#include "internal/population_storage.h"
#include "internal/rng.h"

#include <assert.h>
#include <math.h>

enum {
    TEST_POPULATION_CAPACITY = 8,
    TEST_EVENT_CAPACITY = 24
};

typedef struct evaluation_event {
    char operation;
    size_t index;
} evaluation_event_t;

typedef struct evaluation_context {
    const unsigned char *population_base;
    size_t genome_size;
    bool validity[TEST_POPULATION_CAPACITY];
    evo_fitness_t fitness[TEST_POPULATION_CAPACITY];
    evaluation_event_t events[TEST_EVENT_CAPACITY];
    size_t event_count;
    size_t validation_calls;
    size_t evaluation_calls;
} evaluation_context_t;

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
    config.max_evaluation_bytes =
        population_size * sizeof(evo_candidate_evaluation_t);
    config.max_diversity_work = SIZE_MAX;
    return config;
}

static evo_fitness_t fitness_with_total(double total)
{
    evo_fitness_t fitness = {
        .correctness = total + 1.0,
        .performance = total + 2.0,
        .memory_use = total + 3.0,
        .reliability = total + 4.0,
        .maintainability = total + 5.0,
        .constraint_penalty = total + 6.0,
        .total = total,
    };
    return fitness;
}

static void set_non_finite_field(evo_fitness_t *fitness, size_t field)
{
    double value = field % 2 == 0 ? NAN : INFINITY;

    switch (field) {
    case 0:
        fitness->correctness = value;
        break;
    case 1:
        fitness->performance = value;
        break;
    case 2:
        fitness->memory_use = value;
        break;
    case 3:
        fitness->reliability = value;
        break;
    case 4:
        fitness->maintainability = value;
        break;
    case 5:
        fitness->constraint_penalty = value;
        break;
    case 6:
        fitness->total = value;
        break;
    default:
        assert(false);
    }
}

static void assert_evaluation_empty(const evo_population_t *population)
{
    assert(population->evaluations == NULL);
    assert(population->evaluation_bytes == 0);
    assert(population->valid_count == 0);
    assert(population->best_index == 0);
    assert(!population->has_best);
    assert(!population->evaluated);
}

static size_t genome_index(const void *genome,
                           const evaluation_context_t *context)
{
    const unsigned char *bytes = genome;
    size_t offset = (size_t)(bytes - context->population_base);

    assert(context->genome_size != 0);
    assert(offset % context->genome_size == 0);
    assert(offset / context->genome_size < TEST_POPULATION_CAPACITY);
    return offset / context->genome_size;
}

static void record_event(evaluation_context_t *context,
                         char operation,
                         size_t index)
{
    assert(context->event_count < TEST_EVENT_CAPACITY);
    context->events[context->event_count].operation = operation;
    context->events[context->event_count].index = index;
    ++context->event_count;
}

static bool deterministic_validator(const void *genome, void *opaque)
{
    evaluation_context_t *context = opaque;
    size_t index = genome_index(genome, context);

    record_event(context, 'V', index);
    ++context->validation_calls;
    return context->validity[index];
}

static evo_fitness_t deterministic_evaluator(const void *genome, void *opaque)
{
    evaluation_context_t *context = opaque;
    size_t index = genome_index(genome, context);

    record_event(context, 'E', index);
    ++context->evaluation_calls;
    return context->fitness[index];
}

static void initialize_context(evaluation_context_t *context,
                               const evo_population_t *population)
{
    context->population_base = population->genomes;
    context->genome_size = population->genome_size;
}

static void create_and_initialize(const evo_problem_t *problem,
                                  const evo_config_t *config,
                                  evo_population_t *population)
{
    assert(evo_population_create(problem, config, population) == EVO_SUCCESS);
    assert(evo_population_initialize(problem, config, NULL, population) ==
           EVO_SUCCESS);
    assert_evaluation_empty(population);
}

static void test_invalid_arguments_and_missing_evaluator(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, problem.genome_size, 11);
    evo_population_t population = {0};

    create_and_initialize(&problem, &config, &population);

    assert(evo_population_evaluate(NULL, &config, NULL, &population) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_evaluate(&problem, NULL, NULL, &population) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_evaluate(&problem, &config, NULL, NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_evaluate(&problem, &config, NULL, &population) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_evaluation_empty(&population);

    evo_population_destroy(&population);
}

static void test_evaluation_budget_boundary(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(3, problem.genome_size, 12);
    evo_population_t population = {0};
    evaluation_context_t context = {0};
    size_t exact_budget = config.max_evaluation_bytes;

    problem.evaluate = deterministic_evaluator;
    create_and_initialize(&problem, &config, &population);
    initialize_context(&context, &population);
    for (size_t index = 0; index < config.population_size; ++index) {
        context.fitness[index] = fitness_with_total((double)index);
    }

    config.max_evaluation_bytes = 0;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_evaluation_empty(&population);
    assert(context.evaluation_calls == 0);

    config.max_evaluation_bytes = exact_budget - 1;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_evaluation_empty(&population);
    assert(context.evaluation_calls == 0);

    config.max_evaluation_bytes = exact_budget;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_SUCCESS);
    assert(population.evaluation_bytes == exact_budget);
    assert(population.evaluated);

    evo_population_destroy(&population);
}

static void test_evaluation_size_overflow_rejection(void)
{
    unsigned char placeholder_genome = 0;
    size_t population_size =
        SIZE_MAX / sizeof(evo_candidate_evaluation_t) + 1;
    evo_problem_t problem = test_problem(1);
    evo_config_t config = {
        .population_size = population_size,
        .random_seed = 18,
        .max_genome_bytes = 1,
        .max_population_bytes = population_size,
        .max_evaluation_bytes = SIZE_MAX,
        .max_diversity_work = SIZE_MAX,
    };
    evo_population_t population = {
        .genomes = &placeholder_genome,
        .population_size = population_size,
        .genome_size = 1,
        .storage_bytes = population_size,
        .secure_erasure_policy_version =
            EVO_SECURE_ERASURE_POLICY_VERSION,
        .secure_erasure_backend =
            EVO_SECURE_ERASURE_BACKEND_NONE,
        .initialization_seed = 18,
        .rng_algorithm_version = EVO_RNG_ALGORITHM_VERSION,
        .initialized = true,
    };

    problem.evaluate = deterministic_evaluator;
    assert(evo_population_evaluate(&problem, &config, NULL, &population) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_evaluation_empty(&population);
    assert(population.genomes == &placeholder_genome);
}

static void test_inactive_uninitialized_and_inconsistent_state_rejection(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, problem.genome_size, 19);
    evo_population_t population = {0};
    evaluation_context_t context = {0};

    problem.evaluate = deterministic_evaluator;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_ERROR_STATE);
    assert_evaluation_empty(&population);

    assert(evo_population_create(&problem, &config, &population) ==
           EVO_SUCCESS);
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_ERROR_STATE);
    assert_evaluation_empty(&population);

    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_SUCCESS);
    initialize_context(&context, &population);

    config.population_size = 3;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_ERROR_STATE);
    config.population_size = 2;

    problem.genome_size = 5;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_ERROR_STATE);
    problem.genome_size = 4;

    config.random_seed = 20;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_ERROR_STATE);
    config.random_seed = 19;

    config.max_genome_bytes = 3;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_ERROR_STATE);
    config.max_genome_bytes = 4;

    config.max_population_bytes = 7;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_ERROR_STATE);
    config.max_population_bytes = 8;

    assert_evaluation_empty(&population);
    assert(context.validation_calls == 0);
    assert(context.evaluation_calls == 0);
    evo_population_destroy(&population);
}

static void test_validation_order_invalid_suppression_and_best_candidate(void)
{
    static const evaluation_event_t expected_events[] = {
        {'V', 0},
        {'V', 1},
        {'V', 2},
        {'V', 3},
        {'E', 0},
        {'E', 2},
        {'E', 3},
    };
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(4, problem.genome_size, 13);
    evo_population_t population = {0};
    evaluation_context_t context = {0};
    const evo_candidate_evaluation_t *record = NULL;
    size_t best_index = TEST_POPULATION_CAPACITY;

    problem.is_valid = deterministic_validator;
    problem.evaluate = deterministic_evaluator;
    create_and_initialize(&problem, &config, &population);
    initialize_context(&context, &population);

    context.validity[0] = true;
    context.validity[1] = false;
    context.validity[2] = true;
    context.validity[3] = true;
    context.fitness[0] = fitness_with_total(1.0);
    context.fitness[1] = fitness_with_total(1000.0);
    context.fitness[2] = fitness_with_total(3.0);
    context.fitness[3] = fitness_with_total(3.0);

    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_SUCCESS);
    assert(context.validation_calls == 4);
    assert(context.evaluation_calls == 3);
    assert(context.event_count ==
           sizeof(expected_events) / sizeof(expected_events[0]));
    for (size_t index = 0; index < context.event_count; ++index) {
        assert(context.events[index].operation ==
               expected_events[index].operation);
        assert(context.events[index].index == expected_events[index].index);
    }

    assert(population.valid_count == 3);
    assert(population.has_best);
    assert(population.best_index == 2);
    assert(evo_population_best_index(&population, &best_index));
    assert(best_index == 2);

    record = evo_population_evaluation_const(&population, 1);
    assert(record != NULL);
    assert(!record->valid);
    assert(!record->evaluated);
    assert(record->fitness.total == 0.0);

    record = evo_population_evaluation_const(&population, 2);
    assert(record != NULL);
    assert(record->valid);
    assert(record->evaluated);
    assert(record->fitness.total == 3.0);

    assert(evo_population_evaluation_const(&population, 4) == NULL);
    assert(evo_population_evaluation_const(NULL, 0) == NULL);
    assert(!evo_population_best_index(&population, NULL));

    evo_population_destroy(&population);
}

static void test_optional_validator_defaults_to_valid(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(3, problem.genome_size, 14);
    evo_population_t population = {0};
    evaluation_context_t context = {0};
    size_t best_index = TEST_POPULATION_CAPACITY;

    problem.evaluate = deterministic_evaluator;
    create_and_initialize(&problem, &config, &population);
    initialize_context(&context, &population);
    context.fitness[0] = fitness_with_total(-5.0);
    context.fitness[1] = fitness_with_total(-2.0);
    context.fitness[2] = fitness_with_total(-3.0);

    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_SUCCESS);
    assert(context.validation_calls == 0);
    assert(context.evaluation_calls == 3);
    assert(population.valid_count == 3);
    assert(evo_population_best_index(&population, &best_index));
    assert(best_index == 1);

    for (size_t index = 0; index < config.population_size; ++index) {
        const evo_candidate_evaluation_t *record =
            evo_population_evaluation_const(&population, index);

        assert(record != NULL);
        assert(record->valid);
        assert(record->evaluated);
    }

    evo_population_destroy(&population);
}

static void test_all_invalid_is_completed_without_winner(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(3, problem.genome_size, 15);
    evo_population_t population = {0};
    evaluation_context_t context = {0};
    size_t best_index = TEST_POPULATION_CAPACITY;

    problem.is_valid = deterministic_validator;
    problem.evaluate = deterministic_evaluator;
    create_and_initialize(&problem, &config, &population);
    initialize_context(&context, &population);

    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_SUCCESS);
    assert(context.validation_calls == 3);
    assert(context.evaluation_calls == 0);
    assert(population.evaluated);
    assert(population.evaluations != NULL);
    assert(population.valid_count == 0);
    assert(!population.has_best);
    assert(!evo_population_best_index(&population, &best_index));
    assert(best_index == TEST_POPULATION_CAPACITY);

    for (size_t index = 0; index < config.population_size; ++index) {
        const evo_candidate_evaluation_t *record =
            evo_population_evaluation_const(&population, index);

        assert(record != NULL);
        assert(!record->valid);
        assert(!record->evaluated);
    }

    evo_population_destroy(&population);
}

static void test_non_finite_fitness_rolls_back_and_allows_retry(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(3, problem.genome_size, 16);
    evo_population_t population = {0};
    evaluation_context_t context = {0};

    problem.evaluate = deterministic_evaluator;
    create_and_initialize(&problem, &config, &population);
    initialize_context(&context, &population);
    context.fitness[0] = fitness_with_total(1.0);
    context.fitness[2] = fitness_with_total(3.0);

    for (size_t field = 0; field < 7; ++field) {
        context.event_count = 0;
        context.validation_calls = 0;
        context.evaluation_calls = 0;
        context.fitness[1] = fitness_with_total(2.0);
        set_non_finite_field(&context.fitness[1], field);

        assert(evo_population_evaluate(
                   &problem, &config, &context, &population) ==
               EVO_ERROR_EVALUATION);
        assert(context.evaluation_calls == 2);
        assert_evaluation_empty(&population);
        assert(population.initialized);
        assert(population.genomes != NULL);
    }

    context.event_count = 0;
    context.validation_calls = 0;
    context.evaluation_calls = 0;
    context.fitness[1] = fitness_with_total(2.0);
    context.fitness[1].constraint_penalty = -1.0;
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) ==
           EVO_ERROR_EVALUATION);
    assert(context.evaluation_calls == 2);
    assert_evaluation_empty(&population);
    assert(population.initialized);
    assert(population.genomes != NULL);

    context.event_count = 0;
    context.evaluation_calls = 0;
    context.fitness[1] = fitness_with_total(2.0);
    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_SUCCESS);
    assert(context.evaluation_calls == 3);
    assert(population.evaluated);
    assert(population.best_index == 2);

    evo_population_destroy(&population);
}

static void test_repeated_evaluation_rejection_preserves_state(void)
{
    evo_problem_t problem = test_problem(4);
    evo_config_t config = test_config(2, problem.genome_size, 17);
    evo_population_t population = {0};
    evaluation_context_t context = {0};
    evo_candidate_evaluation_t *evaluation_owner = NULL;
    size_t evaluation_bytes = 0;
    size_t event_count = 0;

    problem.evaluate = deterministic_evaluator;
    create_and_initialize(&problem, &config, &population);
    initialize_context(&context, &population);
    context.fitness[0] = fitness_with_total(4.0);
    context.fitness[1] = fitness_with_total(5.0);

    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_SUCCESS);
    evaluation_owner = population.evaluations;
    evaluation_bytes = population.evaluation_bytes;
    event_count = context.event_count;

    assert(evo_population_evaluate(
               &problem, &config, &context, &population) == EVO_ERROR_STATE);
    assert(population.evaluations == evaluation_owner);
    assert(population.evaluation_bytes == evaluation_bytes);
    assert(population.valid_count == 2);
    assert(population.best_index == 1);
    assert(population.has_best);
    assert(population.evaluated);
    assert(context.event_count == event_count);

    evo_population_destroy(&population);
    assert(population.genomes == NULL);
    assert(population.population_size == 0);
    assert(population.genome_size == 0);
    assert(population.storage_bytes == 0);
    assert(population.initialization_seed == 0);
    assert(population.rng_algorithm_version == 0);
    assert(!population.initialized);
    assert_evaluation_empty(&population);

    evo_population_destroy(&population);
    assert_evaluation_empty(&population);
}

int main(void)
{
    test_invalid_arguments_and_missing_evaluator();
    test_evaluation_budget_boundary();
    test_evaluation_size_overflow_rejection();
    test_inactive_uninitialized_and_inconsistent_state_rejection();
    test_validation_order_invalid_suppression_and_best_candidate();
    test_optional_validator_defaults_to_valid();
    test_all_invalid_is_completed_without_winner();
    test_non_finite_fitness_rolls_back_and_allows_retry();
    test_repeated_evaluation_rejection_preserves_state();
    return 0;
}
