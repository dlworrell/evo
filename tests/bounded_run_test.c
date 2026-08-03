#include "catalyst/evo/evo.h"
#include "internal/bounded_run.h"
#include "internal/child_evaluation.h"
#include "internal/child_pair.h"
#include "internal/population_storage.h"
#include "internal/statistics.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

enum {
    TEST_POPULATION_CAPACITY = 8,
    TEST_GENERATION_CAPACITY = 8,
    TEST_EVENT_CAPACITY = 256
};

typedef struct test_event {
    char operation;
    unsigned char value;
} test_event_t;

typedef struct run_context {
    unsigned char initial_values[TEST_POPULATION_CAPACITY];
    unsigned char mutation_values[TEST_GENERATION_CAPACITY];
    test_event_t events[TEST_EVENT_CAPACITY];
    size_t population_size;
    size_t genome_size;
    size_t initialization_calls;
    size_t mutation_calls;
    size_t validation_calls;
    size_t evaluation_calls;
    size_t event_count;
    unsigned char rejected_value;
    bool reject_value;
    bool reject_all;
    bool constant_fitness;
} run_context_t;

static void assert_result_empty(const evo_result_t *result)
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
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(result->generation_statistics.version == 0);
    assert(result->generation_statistics.generation_index == 0);
    assert(result->generation_statistics.population_size == 0);
    assert(result->generation_statistics.valid_count == 0);
    assert(result->generation_statistics.invalid_count == 0);
    assert(result->generation_statistics.best_index == 0);
    assert(result->generation_statistics.best_fitness.total == 0.0);
    assert(result->generation_statistics.fitness_sums.total == 0.0);
    assert(!result->generation_statistics.has_best);
    assert(result->generation_statistics
               .fitness_comparison_policy_version == 0);
}

static void record_event(run_context_t *context,
                         char operation,
                         unsigned char value)
{
    assert(context->event_count < TEST_EVENT_CAPACITY);
    context->events[context->event_count].operation = operation;
    context->events[context->event_count].value = value;
    ++context->event_count;
}

static void initialize_genome(void *genome, void *opaque)
{
    run_context_t *context = opaque;
    unsigned char *bytes = genome;
    const size_t index = context->initialization_calls;
    const unsigned char value = context->initial_values[index];

    assert(index < context->population_size);
    for (size_t offset = 0; offset < context->genome_size; ++offset) {
        bytes[offset] = value;
    }
    record_event(context, 'I', value);
    ++context->initialization_calls;
}

static void mutate_genome(void *genome,
                          double mutation_rate,
                          void *opaque)
{
    run_context_t *context = opaque;
    unsigned char *bytes = genome;
    const size_t mutations_per_generation =
        context->population_size - context->population_size % 2;
    size_t generation_index = 0;
    unsigned char value = 0;

    assert(mutation_rate == 1.0);
    assert(mutations_per_generation != 0);
    generation_index = context->mutation_calls /
                       mutations_per_generation;
    assert(generation_index < TEST_GENERATION_CAPACITY);
    value = context->mutation_values[generation_index];
    for (size_t offset = 0; offset < context->genome_size; ++offset) {
        bytes[offset] = value;
    }
    record_event(context, 'M', value);
    ++context->mutation_calls;
}

static bool validate_genome(const void *genome, void *opaque)
{
    run_context_t *context = opaque;
    const unsigned char value = ((const unsigned char *)genome)[0];

    record_event(context, 'V', value);
    ++context->validation_calls;
    if (context->reject_all) {
        return false;
    }
    return !context->reject_value || value != context->rejected_value;
}

static evo_fitness_t evaluate_genome(const void *genome, void *opaque)
{
    run_context_t *context = opaque;
    const unsigned char value = ((const unsigned char *)genome)[0];
    const double total = context->constant_fitness ? 10.0 : (double)value;

    record_event(context, 'E', value);
    ++context->evaluation_calls;
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

static evo_problem_t make_problem(size_t genome_size)
{
    return (evo_problem_t){
        .genome_size = genome_size,
        .initialize = initialize_genome,
        .mutate = mutate_genome,
        .evaluate = evaluate_genome,
        .is_valid = validate_genome,
    };
}

static evo_config_t make_config(size_t population_size,
                                size_t genome_size,
                                size_t generation_limit,
                                uint64_t random_seed)
{
    return (evo_config_t){
        .population_size = population_size,
        .generation_limit = generation_limit,
        .tournament_size = population_size == 1 ? 1 : 2,
        .crossover_rate = 0.0,
        .mutation_rate = 1.0,
        .random_seed = random_seed,
        .max_genome_bytes = genome_size,
        .max_population_bytes = population_size * genome_size,
        .max_evaluation_bytes =
            population_size * sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes = population_size * genome_size,
    };
}

static run_context_t make_context(size_t population_size,
                                  size_t genome_size)
{
    run_context_t context = {
        .population_size = population_size,
        .genome_size = genome_size,
    };

    for (size_t index = 0; index < population_size; ++index) {
        context.initial_values[index] = (unsigned char)(index + 1);
    }
    return context;
}

static void assert_result(const evo_result_t *result,
                          unsigned char expected_value,
                          double expected_total,
                          size_t expected_generations,
                          evo_termination_reason_t expected_reason,
                          uint64_t expected_seed)
{
    assert(result->best_genome != NULL);
    for (size_t offset = 0; offset < 4; ++offset) {
        assert(((const unsigned char *)result->best_genome)[offset] ==
               expected_value);
    }
    assert(result->best_fitness.correctness == expected_total + 1.0);
    assert(result->best_fitness.performance == expected_total + 2.0);
    assert(result->best_fitness.memory_use == expected_total + 3.0);
    assert(result->best_fitness.reliability == expected_total + 4.0);
    assert(result->best_fitness.maintainability == expected_total + 5.0);
    assert(result->best_fitness.constraint_penalty == expected_total + 6.0);
    assert(result->best_fitness.total == expected_total);
    assert(result->generations_completed == expected_generations);
    assert(result->random_seed == expected_seed);
    assert(result->termination_reason == expected_reason);
}

static void assert_generation_statistics(
    const evo_result_t *result,
    size_t population_size,
    size_t valid_count,
    size_t best_index,
    double best_total,
    double total_sum)
{
    const evo_generation_statistics_t *statistics =
        &result->generation_statistics;
    const double valid = (double)valid_count;

    assert(statistics->version == EVO_GENERATION_STATISTICS_VERSION);
    assert(statistics->fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert(statistics->generation_index == result->generations_completed);
    assert(statistics->population_size == population_size);
    assert(statistics->valid_count == valid_count);
    assert(statistics->invalid_count == population_size - valid_count);
    assert(statistics->has_best == (valid_count != 0));
    assert(statistics->best_index == best_index);
    assert(statistics->fitness_sums.correctness == total_sum + valid);
    assert(statistics->fitness_sums.performance == total_sum + 2.0 * valid);
    assert(statistics->fitness_sums.memory_use == total_sum + 3.0 * valid);
    assert(statistics->fitness_sums.reliability == total_sum + 4.0 * valid);
    assert(statistics->fitness_sums.maintainability == total_sum + 5.0 * valid);
    assert(statistics->fitness_sums.constraint_penalty ==
           total_sum + 6.0 * valid);
    assert(statistics->fitness_sums.total == total_sum);

    if (valid_count == 0) {
        assert(statistics->best_fitness.total == 0.0);
    } else {
        assert(statistics->best_fitness.correctness == best_total + 1.0);
        assert(statistics->best_fitness.performance == best_total + 2.0);
        assert(statistics->best_fitness.memory_use == best_total + 3.0);
        assert(statistics->best_fitness.reliability == best_total + 4.0);
        assert(statistics->best_fitness.maintainability == best_total + 5.0);
        assert(statistics->best_fitness.constraint_penalty ==
               best_total + 6.0);
        assert(statistics->best_fitness.total == best_total);
    }
}

static void test_zero_limit_preserves_generation_zero(void)
{
    evo_problem_t problem = make_problem(4);
    evo_config_t config = make_config(4, 4, 0, 101);
    run_context_t context = make_context(4, 4);
    evo_result_t result = {0};

    config.tournament_size = 0;
    config.crossover_rate = NAN;
    config.mutation_rate = NAN;
    config.max_child_population_bytes = 0;

    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert_result(&result,
                  4,
                  4.0,
                  0,
                  EVO_TERMINATION_GENERATION_LIMIT,
                  101);
    assert_generation_statistics(&result, 4, 4, 3, 4.0, 10.0);
    assert(context.initialization_calls == 4);
    assert(context.mutation_calls == 0);
    assert(context.validation_calls == 4);
    assert(context.evaluation_calls == 4);
    evo_result_destroy(&result);
}

static void test_even_one_transition_improves_best(void)
{
    evo_problem_t problem = make_problem(4);
    evo_config_t config = make_config(4, 4, 1, 102);
    run_context_t context = make_context(4, 4);
    evo_result_t result = {0};

    context.mutation_values[0] = 20;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert_result(&result,
                  20,
                  20.0,
                  1,
                  EVO_TERMINATION_GENERATION_LIMIT,
                  102);
    assert_generation_statistics(&result, 4, 4, 0, 20.0, 80.0);
    assert(context.initialization_calls == 4);
    assert(context.mutation_calls == 4);
    assert(context.validation_calls == 8);
    assert(context.evaluation_calls == 8);
    evo_result_destroy(&result);
}

static void test_multiple_transitions_replay(void)
{
    evo_problem_t problem = make_problem(4);
    evo_config_t config = make_config(4, 4, 3, 103);
    run_context_t first_context = make_context(4, 4);
    run_context_t second_context = make_context(4, 4);
    evo_result_t first = {0};
    evo_result_t second = {0};

    first_context.mutation_values[0] = 10;
    first_context.mutation_values[1] = 20;
    first_context.mutation_values[2] = 30;
    second_context.mutation_values[0] = 10;
    second_context.mutation_values[1] = 20;
    second_context.mutation_values[2] = 30;

    assert(evo_run(&problem, &config, &first_context, &first) ==
           EVO_SUCCESS);
    assert(evo_run(&problem, &config, &second_context, &second) ==
           EVO_SUCCESS);
    assert_result(&first,
                  30,
                  30.0,
                  3,
                  EVO_TERMINATION_GENERATION_LIMIT,
                  103);
    assert_result(&second,
                  30,
                  30.0,
                  3,
                  EVO_TERMINATION_GENERATION_LIMIT,
                  103);
    assert_generation_statistics(&first, 4, 4, 0, 30.0, 120.0);
    assert_generation_statistics(&second, 4, 4, 0, 30.0, 120.0);
    assert(first.best_genome != second.best_genome);
    assert(first_context.initialization_calls ==
           second_context.initialization_calls);
    assert(first_context.mutation_calls == second_context.mutation_calls);
    assert(first_context.validation_calls ==
           second_context.validation_calls);
    assert(first_context.evaluation_calls == second_context.evaluation_calls);
    assert(first_context.event_count == second_context.event_count);
    for (size_t index = 0; index < first_context.event_count; ++index) {
        assert(first_context.events[index].operation ==
               second_context.events[index].operation);
        assert(first_context.events[index].value ==
               second_context.events[index].value);
    }
    evo_result_destroy(&first);
    evo_result_destroy(&second);
}

static void test_cross_generation_tie_preserves_earlier_winner(void)
{
    evo_problem_t problem = make_problem(4);
    evo_config_t config = make_config(4, 4, 1, 104);
    run_context_t context = make_context(4, 4);
    evo_result_t result = {0};

    context.constant_fitness = true;
    context.mutation_values[0] = 99;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert_result(&result,
                  1,
                  10.0,
                  1,
                  EVO_TERMINATION_GENERATION_LIMIT,
                  104);
    assert_generation_statistics(&result, 4, 4, 0, 10.0, 40.0);
    evo_result_destroy(&result);
}

static void test_odd_and_single_member_populations(void)
{
    evo_problem_t odd_problem = make_problem(4);
    evo_config_t odd_config = make_config(3, 4, 1, 105);
    run_context_t odd_context = make_context(3, 4);
    evo_result_t odd_result = {0};
    evo_problem_t single_problem = make_problem(4);
    evo_config_t single_config = make_config(1, 4, 2, 106);
    run_context_t single_context = make_context(1, 4);
    evo_result_t single_result = {0};

    odd_context.mutation_values[0] = 20;
    assert(evo_run(&odd_problem,
                   &odd_config,
                   &odd_context,
                   &odd_result) == EVO_SUCCESS);
    assert_result(&odd_result,
                  20,
                  20.0,
                  1,
                  EVO_TERMINATION_GENERATION_LIMIT,
                  105);
    assert_generation_statistics(&odd_result, 3, 3, 0, 20.0, 43.0);
    assert(odd_context.mutation_calls == 2);

    single_context.initial_values[0] = 7;
    single_config.tournament_size = 0;
    single_config.crossover_rate = NAN;
    single_config.mutation_rate = NAN;
    assert(evo_run(&single_problem,
                   &single_config,
                   &single_context,
                   &single_result) == EVO_SUCCESS);
    assert_result(&single_result,
                  7,
                  7.0,
                  2,
                  EVO_TERMINATION_GENERATION_LIMIT,
                  106);
    assert_generation_statistics(&single_result, 1, 1, 0, 7.0, 7.0);
    assert(single_context.mutation_calls == 0);
    assert(single_context.validation_calls == 3);
    assert(single_context.evaluation_calls == 3);

    evo_result_destroy(&odd_result);
    evo_result_destroy(&single_result);
}

static void test_later_all_invalid_stops_with_earlier_best(void)
{
    evo_problem_t problem = make_problem(4);
    evo_config_t config = make_config(4, 4, 3, 107);
    run_context_t context = make_context(4, 4);
    evo_result_t result = {0};

    context.reject_value = true;
    context.rejected_value = 0xee;
    context.mutation_values[0] = 0xee;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert_result(&result,
                  4,
                  4.0,
                  1,
                  EVO_TERMINATION_ALL_INVALID,
                  107);
    assert_generation_statistics(&result, 4, 0, 0, 0.0, 0.0);
    assert(context.mutation_calls == 4);
    assert(context.validation_calls == 8);
    assert(context.evaluation_calls == 4);
    evo_result_destroy(&result);
}

static void test_generation_zero_all_invalid_remains_error(void)
{
    evo_problem_t problem = make_problem(4);
    evo_config_t config = make_config(4, 4, 2, 108);
    run_context_t context = make_context(4, 4);
    evo_result_t result = {0};

    context.reject_all = true;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_NO_VALID_CANDIDATE);
    assert_result_empty(&result);
    assert(context.initialization_calls == 4);
    assert(context.mutation_calls == 0);
    assert(context.validation_calls == 4);
    assert(context.evaluation_calls == 0);
}

static void test_transition_preflight_and_active_result(void)
{
    evo_problem_t problem = make_problem(4);
    evo_config_t config = make_config(4, 4, 1, 109);
    run_context_t context = make_context(4, 4);
    evo_result_t result = {0};
    unsigned char active_storage[4] = {9, 9, 9, 9};
    evo_result_t active = {
        .best_genome = active_storage,
        .best_fitness = {.total = 9.0},
        .generations_completed = 7,
        .random_seed = 55,
        .termination_reason = EVO_TERMINATION_ALL_INVALID,
        .generation_statistics = {.version = UINT32_C(77)},
    };

    config.tournament_size = 0;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_result_empty(&result);
    assert(context.initialization_calls == 0);

    config = make_config(4, 4, 1, 109);
    config.max_child_population_bytes = 15;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_result_empty(&result);
    assert(context.initialization_calls == 0);

    config = make_config(4, 4, 1, 109);
    assert(evo_run(&problem, &config, &context, &active) ==
           EVO_ERROR_RESULT_ACTIVE);
    assert(active.best_genome == active_storage);
    assert(active.best_fitness.total == 9.0);
    assert(active.generations_completed == 7);
    assert(active.random_seed == 55);
    assert(active.termination_reason == EVO_TERMINATION_ALL_INVALID);
    assert(active.generation_statistics.version == UINT32_C(77));
    assert(context.initialization_calls == 0);
}

static void snapshot_generation_zero(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *population,
    unsigned char *storage,
    evo_result_t *result)
{
    const evo_candidate_evaluation_t *evaluation = NULL;
    const void *genome = NULL;
    size_t best_index = 0;

    assert(evo_population_best_index(population, &best_index));
    genome = evo_population_genome_const(population, best_index);
    evaluation = evo_population_evaluation_const(population, best_index);
    assert(genome != NULL);
    assert(evaluation != NULL);
    for (size_t offset = 0; offset < problem->genome_size; ++offset) {
        storage[offset] = ((const unsigned char *)genome)[offset];
    }
    result->best_genome = storage;
    result->best_fitness = evaluation->fitness;
    result->random_seed = config->random_seed;
    assert(evo_generation_statistics_record(population,
                                            UINT64_C(0),
                                            &result->generation_statistics) ==
           EVO_SUCCESS);
}

static void test_private_bounded_run_evidence(void)
{
    evo_problem_t problem = make_problem(4);
    evo_config_t config = make_config(2, 4, 2, 110);
    run_context_t context = make_context(2, 4);
    evo_population_t population = {0};
    unsigned char best_storage[4] = {0};
    evo_result_t best = {0};
    evo_bounded_run_evidence_t evidence = {0};

    context.mutation_values[0] = 10;
    context.mutation_values[1] = 20;
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_SUCCESS);
    assert(evo_population_initialize(&problem,
                                     &config,
                                     &context,
                                     &population) == EVO_SUCCESS);
    assert(evo_population_evaluate(&problem,
                                   &config,
                                   &context,
                                   &population) == EVO_SUCCESS);
    snapshot_generation_zero(&problem,
                             &config,
                             &population,
                             best_storage,
                             &best);

    best.termination_reason = EVO_TERMINATION_GENERATION_LIMIT;
    evidence.population_size = 99;
    assert(evo_bounded_run_advance(&problem,
                                   &config,
                                   &context,
                                   &population,
                                   &best,
                                   &evidence) == EVO_ERROR_STATE);
    assert(best.termination_reason == EVO_TERMINATION_GENERATION_LIMIT);
    assert(best.generations_completed == 0);
    assert(population.source_generation == 0);
    assert(evidence.population_size == 99);
    best.termination_reason = EVO_TERMINATION_NONE;
    best.generation_statistics.version = UINT32_C(99);
    evidence.population_size = 98;
    assert(evo_bounded_run_advance(&problem,
                                   &config,
                                   &context,
                                   &population,
                                   &best,
                                   &evidence) == EVO_ERROR_STATE);
    assert(best.generation_statistics.version == UINT32_C(99));
    assert(best.generations_completed == 0);
    assert(population.source_generation == 0);
    assert(evidence.population_size == 98);
    best.generation_statistics.version =
        EVO_GENERATION_STATISTICS_VERSION;
    evidence = (evo_bounded_run_evidence_t){0};

    assert(evo_bounded_run_advance(&problem,
                                   &config,
                                   &context,
                                   &population,
                                   &best,
                                   &evidence) == EVO_SUCCESS);
    assert(evidence.population_size == 2);
    assert(evidence.requested_transitions == 2);
    assert(evidence.completed_transitions == 2);
    assert(evidence.final_generation == 2);
    assert(evidence.best_generation == 2);
    assert(evidence.best_population_index == 0);
    assert(evidence.final_valid_count == 2);
    assert(evidence.final_has_best);
    assert(!evidence.stopped_all_invalid);
    assert(!evidence.stopped_application_requested);
    assert(evidence.termination_reason ==
           EVO_TERMINATION_GENERATION_LIMIT);
    assert(evidence.operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(evidence.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert(evidence.child_evaluation_policy_version ==
           EVO_CHILD_EVALUATION_POLICY_VERSION);
    assert(evidence.generation_advancement_policy_version ==
           EVO_GENERATION_ADVANCEMENT_POLICY_VERSION);
    assert(evidence.policy_version == EVO_BOUNDED_RUN_POLICY_VERSION);
    assert(evidence.complete);
    assert_result(&best,
                  20,
                  20.0,
                  2,
                  EVO_TERMINATION_NONE,
                  110);
    assert_generation_statistics(&best, 2, 2, 0, 20.0, 40.0);

    best = (evo_result_t){0};
    evo_population_destroy(&population);
}

int main(void)
{
    test_zero_limit_preserves_generation_zero();
    test_even_one_transition_improves_best();
    test_multiple_transitions_replay();
    test_cross_generation_tie_preserves_earlier_winner();
    test_odd_and_single_member_populations();
    test_later_all_invalid_stops_with_earlier_best();
    test_generation_zero_all_invalid_remains_error();
    test_transition_preflight_and_active_result();
    test_private_bounded_run_evidence();
    return 0;
}
