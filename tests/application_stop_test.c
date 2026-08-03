#include "catalyst/evo/evo.h"
#include "internal/population_storage.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

enum {
    TEST_POPULATION_CAPACITY = 4,
    TEST_GENERATION_CAPACITY = 4,
    TEST_CALLBACK_CAPACITY = 16
};

_Static_assert(EVO_TERMINATION_APPLICATION_REQUESTED == 3,
               "the initial application-stop reason must remain stable");
_Static_assert(
    _Generic(((evo_generation_result_view_t *)0)->best_genome,
    const void *: 1,
    default: 0),
    "the stop callback must expose only a const genome view");

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

typedef struct callback_event {
    char operation;
    size_t generation;
    evo_termination_reason_t termination_reason;
} callback_event_t;

typedef struct callback_trace {
    callback_event_t events[TEST_CALLBACK_CAPACITY];
    size_t count;
} callback_trace_t;

typedef struct stop_state {
    const evolution_context_t *evolution_context;
    const evo_result_t *public_result;
    callback_trace_t *trace;
    size_t requested_generation;
    size_t calls;
    bool request_enabled;
} stop_state_t;

typedef struct observer_state {
    const evolution_context_t *evolution_context;
    const evo_result_t *public_result;
    callback_trace_t *trace;
    size_t calls;
} observer_state_t;

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
           left->has_best == right->has_best;
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

static void record_callback(callback_trace_t *trace,
                            char operation,
                            size_t generation,
                            evo_termination_reason_t termination_reason)
{
    callback_event_t *event = NULL;

    assert(trace != NULL);
    assert(trace->count < TEST_CALLBACK_CAPACITY);
    event = &trace->events[trace->count];
    event->operation = operation;
    event->generation = generation;
    event->termination_reason = termination_reason;
    ++trace->count;
}

static void assert_view(const evo_generation_result_view_t *result,
                        const evo_generation_statistics_t *statistics,
                        const evolution_context_t *evolution,
                        const evo_result_t *public_result)
{
    assert(result != NULL);
    assert(statistics != NULL);
    assert(evolution != NULL);
    assert(public_result != NULL);
    assert(result->version == EVO_GENERATION_RESULT_VIEW_VERSION);
    assert(result->best_genome != NULL);
    assert(result->best_genome_size == 1);
    assert(statistics->version == EVO_GENERATION_STATISTICS_VERSION);
    assert(statistics->generation_index == result->generations_completed);
    assert(evolution->initialization_calls == TEST_POPULATION_CAPACITY);
    assert(evolution->mutation_calls ==
           result->generations_completed * TEST_POPULATION_CAPACITY);
    assert((const void *)result != (const void *)public_result);
    assert(statistics != &public_result->generation_statistics);
    assert(result->best_genome == public_result->best_genome);
    assert(fitness_equal(&result->best_fitness,
                         &public_result->best_fitness));
    assert(statistics_equal(statistics,
                            &public_result->generation_statistics));
    assert(public_result->termination_reason == EVO_TERMINATION_NONE);
}

static bool request_application_stop(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *opaque)
{
    stop_state_t *state = opaque;

    assert(state != NULL);
    assert_view(result,
                statistics,
                state->evolution_context,
                state->public_result);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    record_callback(state->trace,
                    'S',
                    result->generations_completed,
                    result->termination_reason);
    ++state->calls;
    return state->request_enabled &&
           result->generations_completed == state->requested_generation;
}

static void observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *opaque)
{
    observer_state_t *state = opaque;

    assert(state != NULL);
    assert_view(result,
                statistics,
                state->evolution_context,
                state->public_result);
    record_callback(state->trace,
                    'O',
                    result->generations_completed,
                    result->termination_reason);
    ++state->calls;
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
                                stop_state_t *stop,
                                observer_state_t *observer)
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
        .generation_stop = request_application_stop,
        .generation_stop_context = stop,
    };
}

static evolution_context_t make_context(void)
{
    return (evolution_context_t){
        .initial_values = {1, 2, 3, 4},
        .population_size = TEST_POPULATION_CAPACITY,
    };
}

static void attach_callback_state(stop_state_t *stop,
                                  observer_state_t *observer,
                                  const evolution_context_t *evolution,
                                  const evo_result_t *result,
                                  callback_trace_t *trace)
{
    stop->evolution_context = evolution;
    stop->public_result = result;
    stop->trace = trace;
    observer->evolution_context = evolution;
    observer->public_result = result;
    observer->trace = trace;
}

static void assert_event(const callback_trace_t *trace,
                         size_t index,
                         char operation,
                         size_t generation,
                         evo_termination_reason_t termination_reason)
{
    assert(index < trace->count);
    assert(trace->events[index].operation == operation);
    assert(trace->events[index].generation == generation);
    assert(trace->events[index].termination_reason == termination_reason);
}

static void assert_result(const evo_result_t *result,
                          unsigned char best_value,
                          size_t generations_completed,
                          evo_termination_reason_t termination_reason,
                          uint64_t random_seed)
{
    assert(result->best_genome != NULL);
    assert(((const unsigned char *)result->best_genome)[0] == best_value);
    assert(result->best_fitness.total == (double)best_value);
    assert(result->generations_completed == generations_completed);
    assert(result->random_seed == random_seed);
    assert(result->termination_reason == termination_reason);
    assert(result->generation_statistics.generation_index ==
           generations_completed);
}

static void assert_results_equal(const evo_result_t *left,
                                 const evo_result_t *right)
{
    assert(((const unsigned char *)left->best_genome)[0] ==
           ((const unsigned char *)right->best_genome)[0]);
    assert(fitness_equal(&left->best_fitness, &right->best_fitness));
    assert(left->generations_completed == right->generations_completed);
    assert(left->random_seed == right->random_seed);
    assert(left->termination_reason == right->termination_reason);
    assert(statistics_equal(&left->generation_statistics,
                            &right->generation_statistics));
}

static void assert_result_empty(const evo_result_t *result)
{
    assert(result->best_genome == NULL);
    assert(result->best_fitness.total == 0.0);
    assert(result->generations_completed == 0);
    assert(result->random_seed == 0);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(result->generation_statistics.version == 0);
}

static void test_immediate_stop_after_generation_zero(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t context = make_context();
    callback_trace_t trace = {0};
    stop_state_t stop = {
        .requested_generation = 0,
        .request_enabled = true,
    };
    observer_state_t observer = {0};
    evo_config_t config =
        make_config(3, UINT64_C(301), &stop, &observer);
    evo_result_t result = {0};

    attach_callback_state(&stop, &observer, &context, &result, &trace);
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(stop.calls == 1);
    assert(observer.calls == 1);
    assert(context.mutation_calls == 0);
    assert(trace.count == 2);
    assert_event(&trace, 0, 'S', 0, EVO_TERMINATION_NONE);
    assert_event(&trace,
                 1,
                 'O',
                 0,
                 EVO_TERMINATION_APPLICATION_REQUESTED);
    assert_result(&result,
                  4,
                  0,
                  EVO_TERMINATION_APPLICATION_REQUESTED,
                  UINT64_C(301));
    evo_result_destroy(&result);
}

static void test_intermediate_stop_after_promoted_child(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t context = make_context();
    callback_trace_t trace = {0};
    stop_state_t stop = {
        .requested_generation = 2,
        .request_enabled = true,
    };
    observer_state_t observer = {0};
    evo_config_t config =
        make_config(4, UINT64_C(302), &stop, &observer);
    evo_result_t result = {0};

    context.mutation_values[0] = 10;
    context.mutation_values[1] = 20;
    attach_callback_state(&stop, &observer, &context, &result, &trace);
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(stop.calls == 3);
    assert(observer.calls == 3);
    assert(context.mutation_calls == 2 * TEST_POPULATION_CAPACITY);
    assert(trace.count == 6);
    assert_event(&trace, 0, 'S', 0, EVO_TERMINATION_NONE);
    assert_event(&trace, 1, 'O', 0, EVO_TERMINATION_NONE);
    assert_event(&trace, 2, 'S', 1, EVO_TERMINATION_NONE);
    assert_event(&trace, 3, 'O', 1, EVO_TERMINATION_NONE);
    assert_event(&trace, 4, 'S', 2, EVO_TERMINATION_NONE);
    assert_event(&trace,
                 5,
                 'O',
                 2,
                 EVO_TERMINATION_APPLICATION_REQUESTED);
    assert_result(&result,
                  20,
                  2,
                  EVO_TERMINATION_APPLICATION_REQUESTED,
                  UINT64_C(302));
    evo_result_destroy(&result);
}

static void test_never_stop_replays_null_callback_run(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t decision_context = make_context();
    evolution_context_t baseline_context = make_context();
    callback_trace_t decision_trace = {0};
    callback_trace_t baseline_trace = {0};
    stop_state_t decision_stop = {0};
    stop_state_t baseline_stop = {0};
    observer_state_t decision_observer = {0};
    observer_state_t baseline_observer = {0};
    evo_config_t decision_config = make_config(
        3, UINT64_C(303), &decision_stop, &decision_observer);
    evo_config_t baseline_config = make_config(
        3, UINT64_C(303), &baseline_stop, &baseline_observer);
    evo_result_t decision_result = {0};
    evo_result_t baseline_result = {0};

    decision_context.mutation_values[0] = 10;
    decision_context.mutation_values[1] = 20;
    decision_context.mutation_values[2] = 30;
    baseline_context.mutation_values[0] = 10;
    baseline_context.mutation_values[1] = 20;
    baseline_context.mutation_values[2] = 30;
    attach_callback_state(&decision_stop,
                          &decision_observer,
                          &decision_context,
                          &decision_result,
                          &decision_trace);
    attach_callback_state(&baseline_stop,
                          &baseline_observer,
                          &baseline_context,
                          &baseline_result,
                          &baseline_trace);
    baseline_config.generation_stop = NULL;
    baseline_config.generation_stop_context = NULL;

    assert(evo_run(&problem,
                   &decision_config,
                   &decision_context,
                   &decision_result) == EVO_SUCCESS);
    assert(evo_run(&problem,
                   &baseline_config,
                   &baseline_context,
                   &baseline_result) == EVO_SUCCESS);
    assert(decision_stop.calls == 3);
    assert(baseline_stop.calls == 0);
    assert(decision_observer.calls == 4);
    assert(baseline_observer.calls == 4);
    assert(decision_context.initialization_calls ==
           baseline_context.initialization_calls);
    assert(decision_context.mutation_calls == baseline_context.mutation_calls);
    assert_results_equal(&decision_result, &baseline_result);
    assert_event(&decision_trace, 5, 'O', 2, EVO_TERMINATION_NONE);
    assert_event(&decision_trace,
                 6,
                 'O',
                 3,
                 EVO_TERMINATION_GENERATION_LIMIT);
    assert_event(&baseline_trace,
                 3,
                 'O',
                 3,
                 EVO_TERMINATION_GENERATION_LIMIT);

    evo_result_destroy(&decision_result);
    evo_result_destroy(&baseline_result);
}

static void test_structural_termination_takes_precedence(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t zero_context = make_context();
    evolution_context_t invalid_context = make_context();
    callback_trace_t zero_trace = {0};
    callback_trace_t invalid_trace = {0};
    stop_state_t zero_stop = {
        .requested_generation = 0,
        .request_enabled = true,
    };
    stop_state_t invalid_stop = {0};
    observer_state_t zero_observer = {0};
    observer_state_t invalid_observer = {0};
    evo_config_t zero_config =
        make_config(0, UINT64_C(304), &zero_stop, &zero_observer);
    evo_config_t invalid_config =
        make_config(3, UINT64_C(305), &invalid_stop, &invalid_observer);
    evo_result_t zero_result = {0};
    evo_result_t invalid_result = {0};

    attach_callback_state(&zero_stop,
                          &zero_observer,
                          &zero_context,
                          &zero_result,
                          &zero_trace);
    assert(evo_run(&problem,
                   &zero_config,
                   &zero_context,
                   &zero_result) == EVO_SUCCESS);
    assert(zero_stop.calls == 0);
    assert(zero_observer.calls == 1);
    assert_event(&zero_trace,
                 0,
                 'O',
                 0,
                 EVO_TERMINATION_GENERATION_LIMIT);
    assert_result(&zero_result,
                  4,
                  0,
                  EVO_TERMINATION_GENERATION_LIMIT,
                  UINT64_C(304));

    invalid_context.reject_value = true;
    invalid_context.rejected_value = 0xee;
    invalid_context.mutation_values[0] = 0xee;
    attach_callback_state(&invalid_stop,
                          &invalid_observer,
                          &invalid_context,
                          &invalid_result,
                          &invalid_trace);
    assert(evo_run(&problem,
                   &invalid_config,
                   &invalid_context,
                   &invalid_result) == EVO_SUCCESS);
    assert(invalid_stop.calls == 1);
    assert(invalid_observer.calls == 2);
    assert(invalid_trace.count == 3);
    assert_event(&invalid_trace, 0, 'S', 0, EVO_TERMINATION_NONE);
    assert_event(&invalid_trace, 1, 'O', 0, EVO_TERMINATION_NONE);
    assert_event(&invalid_trace,
                 2,
                 'O',
                 1,
                 EVO_TERMINATION_ALL_INVALID);
    assert_result(&invalid_result,
                  4,
                  1,
                  EVO_TERMINATION_ALL_INVALID,
                  UINT64_C(305));

    evo_result_destroy(&zero_result);
    evo_result_destroy(&invalid_result);
}

static void test_failed_child_emits_no_stop_decision(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t context = make_context();
    callback_trace_t trace = {0};
    stop_state_t stop = {0};
    observer_state_t observer = {0};
    evo_config_t config =
        make_config(2, UINT64_C(306), &stop, &observer);
    evo_result_t result = {0};

    context.mutation_values[0] = 0xdd;
    context.non_finite_value = 0xdd;
    context.produce_non_finite = true;
    attach_callback_state(&stop, &observer, &context, &result, &trace);
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_EVALUATION);
    assert(stop.calls == 1);
    assert(observer.calls == 1);
    assert(trace.count == 2);
    assert_event(&trace, 0, 'S', 0, EVO_TERMINATION_NONE);
    assert_event(&trace, 1, 'O', 0, EVO_TERMINATION_NONE);
    assert_result_empty(&result);
}

int main(void)
{
    test_immediate_stop_after_generation_zero();
    test_intermediate_stop_after_promoted_child();
    test_never_stop_replays_null_callback_run();
    test_structural_termination_takes_precedence();
    test_failed_child_emits_no_stop_decision();
    return 0;
}
