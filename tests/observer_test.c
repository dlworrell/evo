#include "catalyst/evo/evo.h"
#include "internal/population_storage.h"

#include <assert.h>
#include <math.h>

enum {
    TEST_POPULATION_CAPACITY = 4,
    TEST_GENERATION_CAPACITY = 4,
    TEST_OBSERVATION_CAPACITY = 8
};

_Static_assert(
    _Generic(((evo_generation_result_view_t *)0)->best_genome,
    const void *: 1,
    default: 0),
    "the observer must expose only a const genome view");

typedef struct evolution_context {
    unsigned char initial_values[TEST_POPULATION_CAPACITY];
    unsigned char mutation_values[TEST_GENERATION_CAPACITY];
    size_t population_size;
    size_t initialization_calls;
    size_t mutation_calls;
    unsigned char rejected_value;
    unsigned char non_finite_value;
    bool reject_value;
    bool produce_non_finite;
} evolution_context_t;

typedef struct observed_generation {
    uint32_t result_version;
    size_t best_genome_size;
    unsigned char best_genome_value;
    evo_fitness_t best_fitness;
    size_t generations_completed;
    uint64_t random_seed;
    evo_termination_reason_t termination_reason;
    evo_generation_statistics_t statistics;
} observed_generation_t;

typedef struct observer_log {
    observed_generation_t generations[TEST_OBSERVATION_CAPACITY];
    const evolution_context_t *evolution_context;
    const evo_result_t *public_result;
    size_t count;
} observer_log_t;

static bool fitness_equal(const evo_fitness_t *left,
                          const evo_fitness_t *right)
{
    return left->correctness == right->correctness &&
           left->performance == right->performance &&
           left->memory_use == right->memory_use &&
           left->reliability == right->reliability &&
           left->maintainability == right->maintainability &&
           left->constraint_penalty == right->constraint_penalty &&
           left->total == right->total;
}

static bool statistics_equal(
    const evo_generation_statistics_t *left,
    const evo_generation_statistics_t *right)
{
    return left->version == right->version &&
           left->generation_index == right->generation_index &&
           left->population_size == right->population_size &&
           left->valid_count == right->valid_count &&
           left->invalid_count == right->invalid_count &&
           left->best_index == right->best_index &&
           fitness_equal(&left->best_fitness, &right->best_fitness) &&
           fitness_equal(&left->fitness_sums, &right->fitness_sums) &&
           left->has_best == right->has_best &&
           left->fitness_comparison_policy_version ==
               right->fitness_comparison_policy_version;
}

static evo_fitness_t fitness_from_value(unsigned char value)
{
    const double total = (double)value;

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

static void initialize_genome(void *genome, void *opaque)
{
    evolution_context_t *context = opaque;
    const size_t index = context->initialization_calls;

    assert(index < context->population_size);
    ((unsigned char *)genome)[0] = context->initial_values[index];
    ++context->initialization_calls;
}

static void mutate_genome(void *genome,
                          double mutation_rate,
                          void *opaque)
{
    evolution_context_t *context = opaque;
    const size_t generation =
        context->mutation_calls / context->population_size;

    assert(mutation_rate == 1.0);
    assert(generation < TEST_GENERATION_CAPACITY);
    ((unsigned char *)genome)[0] = context->mutation_values[generation];
    ++context->mutation_calls;
}

static bool validate_genome(const void *genome, void *opaque)
{
    const evolution_context_t *context = opaque;
    const unsigned char value = ((const unsigned char *)genome)[0];

    return !context->reject_value || value != context->rejected_value;
}

static evo_fitness_t evaluate_genome(const void *genome, void *opaque)
{
    const evolution_context_t *context = opaque;
    const unsigned char value = ((const unsigned char *)genome)[0];
    evo_fitness_t fitness = fitness_from_value(value);

    if (context->produce_non_finite &&
        value == context->non_finite_value) {
        fitness.total = NAN;
    }
    return fitness;
}

static void observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *opaque)
{
    observer_log_t *log = opaque;
    observed_generation_t *generation = NULL;

    assert(result != NULL);
    assert(statistics != NULL);
    assert(log != NULL);
    assert(log->evolution_context != NULL);
    assert(log->public_result != NULL);
    assert(log->count < TEST_OBSERVATION_CAPACITY);
    assert(result->version == EVO_GENERATION_RESULT_VIEW_VERSION);
    assert(result->best_genome != NULL);
    assert(result->best_genome_size == 1);
    assert(statistics->version == EVO_GENERATION_STATISTICS_VERSION);
    assert(statistics->fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert(statistics->generation_index == result->generations_completed);
    assert(log->evolution_context->initialization_calls ==
           TEST_POPULATION_CAPACITY);
    assert(log->evolution_context->mutation_calls ==
           result->generations_completed * TEST_POPULATION_CAPACITY);

    /* The callback receives value snapshots, not writable result structures. */
    assert((const void *)result != (const void *)log->public_result);
    assert(statistics != &log->public_result->generation_statistics);
    assert(result->best_genome == log->public_result->best_genome);
    assert(fitness_equal(&result->best_fitness,
                         &log->public_result->best_fitness));
    assert(statistics_equal(statistics,
                            &log->public_result->generation_statistics));
    generation = &log->generations[log->count];
    generation->result_version = result->version;
    generation->best_genome_size = result->best_genome_size;
    generation->best_genome_value =
        ((const unsigned char *)result->best_genome)[0];
    generation->best_fitness = result->best_fitness;
    generation->generations_completed = result->generations_completed;
    generation->random_seed = result->random_seed;
    generation->termination_reason = result->termination_reason;
    generation->statistics = *statistics;
    ++log->count;
}

static evo_problem_t make_problem(void)
{
    return (evo_problem_t){
        .genome_size = 1,
        .initialize = initialize_genome,
        .mutate = mutate_genome,
        .evaluate = evaluate_genome,
        .is_valid = validate_genome,
    };
}

static evo_config_t make_config(size_t generation_limit,
                                uint64_t random_seed,
                                observer_log_t *observer)
{
    return (evo_config_t){
        .population_size = TEST_POPULATION_CAPACITY,
        .generation_limit = generation_limit,
        .tournament_size = 2,
        .crossover_rate = 0.0,
        .mutation_rate = 1.0,
        .random_seed = random_seed,
        .max_genome_bytes = 1,
        .max_population_bytes = TEST_POPULATION_CAPACITY,
        .max_evaluation_bytes =
            TEST_POPULATION_CAPACITY *
            sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes = TEST_POPULATION_CAPACITY,
        .generation_observer = observe_generation,
        .generation_observer_context = observer,
    };
}

static evolution_context_t make_context(void)
{
    return (evolution_context_t){
        .initial_values = {1, 2, 3, 4},
        .population_size = TEST_POPULATION_CAPACITY,
    };
}

static void assert_observation(
    const observed_generation_t *generation,
    size_t generation_index,
    unsigned char global_best,
    size_t valid_count,
    double generation_total_sum,
    evo_termination_reason_t termination_reason,
    uint64_t random_seed)
{
    assert(generation->result_version ==
           EVO_GENERATION_RESULT_VIEW_VERSION);
    assert(generation->best_genome_size == 1);
    assert(generation->best_genome_value == global_best);
    assert(generation->best_fitness.total == (double)global_best);
    assert(generation->generations_completed == generation_index);
    assert(generation->random_seed == random_seed);
    assert(generation->termination_reason == termination_reason);
    assert(generation->statistics.version ==
           EVO_GENERATION_STATISTICS_VERSION);
    assert(generation->statistics.generation_index == generation_index);
    assert(generation->statistics.population_size ==
           TEST_POPULATION_CAPACITY);
    assert(generation->statistics.valid_count == valid_count);
    assert(generation->statistics.invalid_count ==
           TEST_POPULATION_CAPACITY - valid_count);
    assert(generation->statistics.fitness_sums.total ==
           generation_total_sum);
    assert(generation->statistics.has_best == (valid_count != 0));
}

static void assert_observations_equal(const observer_log_t *left,
                                      const observer_log_t *right)
{
    assert(left->count == right->count);
    for (size_t index = 0; index < left->count; ++index) {
        const observed_generation_t *left_generation =
            &left->generations[index];
        const observed_generation_t *right_generation =
            &right->generations[index];

        assert(left_generation->result_version ==
               right_generation->result_version);
        assert(left_generation->best_genome_size ==
               right_generation->best_genome_size);
        assert(left_generation->best_genome_value ==
               right_generation->best_genome_value);
        assert(fitness_equal(&left_generation->best_fitness,
                             &right_generation->best_fitness));
        assert(left_generation->generations_completed ==
               right_generation->generations_completed);
        assert(left_generation->random_seed ==
               right_generation->random_seed);
        assert(left_generation->termination_reason ==
               right_generation->termination_reason);
        assert(statistics_equal(&left_generation->statistics,
                                &right_generation->statistics));
    }
}

static void test_zero_limit_emits_one_terminal_generation(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t context = make_context();
    observer_log_t observer = {0};
    evo_config_t config = make_config(0, UINT64_C(201), &observer);
    evo_result_t result = {0};

    observer.public_result = &result;
    observer.evolution_context = &context;
    config.tournament_size = 0;
    config.crossover_rate = NAN;
    config.mutation_rate = NAN;
    config.max_child_population_bytes = 0;

    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(observer.count == 1);
    assert_observation(&observer.generations[0],
                       0,
                       4,
                       4,
                       10.0,
                       EVO_TERMINATION_GENERATION_LIMIT,
                       UINT64_C(201));
    evo_result_destroy(&result);
}

static void test_three_transitions_emit_four_replayable_generations(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t first_context = make_context();
    evolution_context_t second_context = make_context();
    observer_log_t first_observer = {0};
    observer_log_t second_observer = {0};
    evo_config_t first_config =
        make_config(3, UINT64_C(202), &first_observer);
    evo_config_t second_config =
        make_config(3, UINT64_C(202), &second_observer);
    evo_result_t first_result = {0};
    evo_result_t second_result = {0};

    first_context.mutation_values[0] = 10;
    first_context.mutation_values[1] = 20;
    first_context.mutation_values[2] = 30;
    second_context.mutation_values[0] = 10;
    second_context.mutation_values[1] = 20;
    second_context.mutation_values[2] = 30;
    first_observer.public_result = &first_result;
    first_observer.evolution_context = &first_context;
    second_observer.public_result = &second_result;
    second_observer.evolution_context = &second_context;

    assert(evo_run(&problem,
                   &first_config,
                   &first_context,
                   &first_result) == EVO_SUCCESS);
    assert(evo_run(&problem,
                   &second_config,
                   &second_context,
                   &second_result) == EVO_SUCCESS);
    assert(first_observer.count == 4);
    assert_observation(&first_observer.generations[0],
                       0,
                       4,
                       4,
                       10.0,
                       EVO_TERMINATION_NONE,
                       UINT64_C(202));
    assert_observation(&first_observer.generations[1],
                       1,
                       10,
                       4,
                       40.0,
                       EVO_TERMINATION_NONE,
                       UINT64_C(202));
    assert_observation(&first_observer.generations[2],
                       2,
                       20,
                       4,
                       80.0,
                       EVO_TERMINATION_NONE,
                       UINT64_C(202));
    assert_observation(&first_observer.generations[3],
                       3,
                       30,
                       4,
                       120.0,
                       EVO_TERMINATION_GENERATION_LIMIT,
                       UINT64_C(202));
    assert_observations_equal(&first_observer, &second_observer);

    evo_result_destroy(&first_result);
    evo_result_destroy(&second_result);
}

static void test_all_invalid_terminal_generation_retains_global_winner(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t context = make_context();
    observer_log_t observer = {0};
    evo_config_t config = make_config(3, UINT64_C(203), &observer);
    evo_result_t result = {0};

    context.reject_value = true;
    context.rejected_value = 0xee;
    context.mutation_values[0] = 0xee;
    observer.public_result = &result;
    observer.evolution_context = &context;

    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(observer.count == 2);
    assert_observation(&observer.generations[0],
                       0,
                       4,
                       4,
                       10.0,
                       EVO_TERMINATION_NONE,
                       UINT64_C(203));
    assert_observation(&observer.generations[1],
                       1,
                       4,
                       0,
                       0.0,
                       EVO_TERMINATION_ALL_INVALID,
                       UINT64_C(203));
    assert(!observer.generations[1].statistics.has_best);
    assert(observer.generations[1].statistics.best_fitness.total == 0.0);
    evo_result_destroy(&result);
}

static void test_failed_generations_emit_no_event(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t zero_context = make_context();
    evolution_context_t child_context = make_context();
    observer_log_t zero_observer = {0};
    observer_log_t child_observer = {0};
    evo_config_t zero_config =
        make_config(2, UINT64_C(204), &zero_observer);
    evo_config_t child_config =
        make_config(2, UINT64_C(205), &child_observer);
    evo_result_t zero_result = {0};
    evo_result_t child_result = {0};

    zero_context.initial_values[0] = 0xdd;
    zero_context.non_finite_value = 0xdd;
    zero_context.produce_non_finite = true;
    zero_observer.public_result = &zero_result;
    zero_observer.evolution_context = &zero_context;
    assert(evo_run(&problem,
                   &zero_config,
                   &zero_context,
                   &zero_result) == EVO_ERROR_EVALUATION);
    assert(zero_observer.count == 0);
    assert(zero_result.best_genome == NULL);

    child_context.mutation_values[0] = 0xdd;
    child_context.non_finite_value = 0xdd;
    child_context.produce_non_finite = true;
    child_observer.public_result = &child_result;
    child_observer.evolution_context = &child_context;
    assert(evo_run(&problem,
                   &child_config,
                   &child_context,
                   &child_result) == EVO_ERROR_EVALUATION);
    assert(child_observer.count == 1);
    assert_observation(&child_observer.generations[0],
                       0,
                       4,
                       4,
                       10.0,
                       EVO_TERMINATION_NONE,
                       UINT64_C(205));
    assert(child_result.best_genome == NULL);
}

static void test_active_result_rejection_emits_no_event(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t context = make_context();
    observer_log_t observer = {0};
    evo_config_t config = make_config(1, UINT64_C(206), &observer);
    unsigned char active_genome = 9;
    evo_result_t result = {
        .best_genome = &active_genome,
    };

    observer.public_result = &result;
    observer.evolution_context = &context;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_RESULT_ACTIVE);
    assert(observer.count == 0);
    assert(result.best_genome == &active_genome);
}

int main(void)
{
    test_zero_limit_emits_one_terminal_generation();
    test_three_transitions_emit_four_replayable_generations();
    test_all_invalid_terminal_generation_retains_global_winner();
    test_failed_generations_emit_no_event();
    test_active_result_rejection_emits_no_event();
    return 0;
}
