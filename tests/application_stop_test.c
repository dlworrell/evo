#include "catalyst/evo/evo.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

enum {
    TEST_POPULATION_SIZE = 4,
    TEST_GENERATION_CAPACITY = 5,
    TEST_EVENT_CAPACITY = 16
};

_Static_assert(
    _Generic(((evo_generation_result_view_t *)0)->best_genome, const void *: 1, default: 0),
    "callback genome views must remain const");

typedef struct evolution_context {
    unsigned char initial_values[TEST_POPULATION_SIZE];
    unsigned char mutation_values[TEST_GENERATION_CAPACITY];
    size_t initialization_calls;
    size_t mutation_calls;
    unsigned char rejected_value;
    unsigned char non_finite_value;
    bool reject_value;
    bool produce_non_finite;
} evolution_context_t;

typedef struct callback_event {
    char kind;
    size_t generation;
} callback_event_t;

typedef struct callback_trace {
    callback_event_t events[TEST_EVENT_CAPACITY];
    size_t count;
} callback_trace_t;

typedef struct generation_record {
    unsigned char best_value;
    size_t generation;
    evo_termination_reason_t termination_reason;
    evo_generation_statistics_t statistics;
} generation_record_t;

typedef struct stop_context {
    generation_record_t records[TEST_EVENT_CAPACITY];
    callback_trace_t *trace;
    const evolution_context_t *evolution;
    const evo_result_t *public_result;
    size_t stop_generation;
    size_t calls;
} stop_context_t;

typedef struct observer_context {
    generation_record_t records[TEST_EVENT_CAPACITY];
    callback_trace_t *trace;
    const evo_result_t *public_result;
    size_t calls;
} observer_context_t;

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

static void assert_callback_view(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    const evo_result_t *public_result)
{
    assert(result != NULL);
    assert(statistics != NULL);
    assert(public_result != NULL);
    assert(result->version == EVO_GENERATION_RESULT_VIEW_VERSION);
    assert(statistics->version == EVO_GENERATION_STATISTICS_VERSION);
    assert(statistics->fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert((const void *)result != (const void *)public_result);
    assert(statistics != &public_result->generation_statistics);
    assert(result->best_genome == public_result->best_genome);
    assert(result->best_genome_size == 1);
    assert(fitness_equal(&result->best_fitness,
                         &public_result->best_fitness));
    assert(result->generations_completed ==
           public_result->generations_completed);
    assert(result->random_seed == public_result->random_seed);
    assert(statistics_equal(statistics,
                            &public_result->generation_statistics));
    assert(public_result->termination_reason == EVO_TERMINATION_NONE);
}

static void record_callback(callback_trace_t *trace,
                            char kind,
                            size_t generation)
{
    assert(trace != NULL);
    assert(trace->count < TEST_EVENT_CAPACITY);
    trace->events[trace->count] = (callback_event_t){
        .kind = kind,
        .generation = generation,
    };
    ++trace->count;
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

    assert(index < TEST_POPULATION_SIZE);
    ((unsigned char *)genome)[0] = context->initial_values[index];
    ++context->initialization_calls;
}

static void mutate_genome(void *genome,
                          double mutation_rate,
                          void *opaque)
{
    evolution_context_t *context = opaque;
    const size_t generation =
        context->mutation_calls / TEST_POPULATION_SIZE;

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

static void copy_record(generation_record_t *record,
                        const evo_generation_result_view_t *result,
                        const evo_generation_statistics_t *statistics)
{
    assert(result->best_genome != NULL);
    assert(result->best_genome_size == 1);
    assert(statistics->generation_index == result->generations_completed);
    record->best_value =
        ((const unsigned char *)result->best_genome)[0];
    record->generation = result->generations_completed;
    record->termination_reason = result->termination_reason;
    record->statistics = *statistics;
}

static bool decide_stop(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *opaque)
{
    stop_context_t *context = opaque;

    assert(context != NULL);
    assert(context->trace != NULL);
    assert(context->evolution != NULL);
    assert(context->public_result != NULL);
    assert(context->calls < TEST_EVENT_CAPACITY);
    assert_callback_view(result, statistics, context->public_result);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(context->evolution->mutation_calls ==
           result->generations_completed * TEST_POPULATION_SIZE);

    copy_record(&context->records[context->calls], result, statistics);
    record_callback(context->trace,
                    'S',
                    result->generations_completed);
    ++context->calls;
    return result->generations_completed == context->stop_generation;
}

static void observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *opaque)
{
    observer_context_t *context = opaque;

    assert(context != NULL);
    assert(context->trace != NULL);
    assert(context->public_result != NULL);
    assert(context->calls < TEST_EVENT_CAPACITY);
    assert_callback_view(result, statistics, context->public_result);

    copy_record(&context->records[context->calls], result, statistics);
    record_callback(context->trace,
                    'O',
                    result->generations_completed);
    ++context->calls;
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
                                stop_context_t *stop,
                                observer_context_t *observer)
{
    return (evo_config_t){
        .population_size = TEST_POPULATION_SIZE,
        .generation_limit = generation_limit,
        .tournament_size = 2,
        .crossover_rate = 0.0,
        .mutation_rate = 1.0,
        .random_seed = random_seed,
        .max_genome_bytes = 1,
        .max_population_bytes = TEST_POPULATION_SIZE,
        .max_evaluation_bytes = 4096,
        .max_child_population_bytes = TEST_POPULATION_SIZE,
        .generation_observer = observer == NULL
                                   ? NULL
                                   : observe_generation,
        .generation_observer_context = observer,
        .generation_stop = stop == NULL ? NULL : decide_stop,
        .generation_stop_context = stop,
    };
}

static evolution_context_t make_evolution(void)
{
    return (evolution_context_t){
        .initial_values = {1, 2, 3, 4},
    };
}

static void bind_contexts(stop_context_t *stop,
                          observer_context_t *observer,
                          callback_trace_t *trace,
                          const evolution_context_t *evolution,
                          const evo_result_t *result)
{
    if (stop != NULL) {
        stop->trace = trace;
        stop->evolution = evolution;
        stop->public_result = result;
    }
    if (observer != NULL) {
        observer->trace = trace;
        observer->public_result = result;
    }
}

static void assert_result(const evo_result_t *result,
                          size_t generation,
                          unsigned char best_value,
                          evo_termination_reason_t reason,
                          uint64_t random_seed)
{
    assert(result->best_genome != NULL);
    assert(((const unsigned char *)result->best_genome)[0] == best_value);
    assert(result->best_fitness.total == (double)best_value);
    assert(result->generations_completed == generation);
    assert(result->random_seed == random_seed);
    assert(result->termination_reason == reason);
    assert(result->generation_statistics.generation_index == generation);
}

static void assert_result_empty(const evo_result_t *result)
{
    const evo_fitness_t empty_fitness = {0};
    const evo_generation_statistics_t empty_statistics = {0};

    assert(result->best_genome == NULL);
    assert(fitness_equal(&result->best_fitness, &empty_fitness));
    assert(result->generations_completed == 0);
    assert(result->random_seed == 0);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(statistics_equal(&result->generation_statistics,
                            &empty_statistics));
}

static void assert_trace_event(const callback_trace_t *trace,
                               size_t index,
                               char kind,
                               size_t generation)
{
    assert(index < trace->count);
    assert(trace->events[index].kind == kind);
    assert(trace->events[index].generation == generation);
}

static void test_immediate_stop_after_generation_zero(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t evolution = make_evolution();
    callback_trace_t trace = {0};
    stop_context_t stop = {
        .stop_generation = 0,
    };
    observer_context_t observer = {0};
    evo_config_t config = make_config(4, UINT64_C(301), &stop, &observer);
    evo_result_t result = {0};

    bind_contexts(&stop, &observer, &trace, &evolution, &result);
    assert(evo_run(&problem, &config, &evolution, &result) == EVO_SUCCESS);
    assert_result(&result,
                  0,
                  4,
                  EVO_TERMINATION_APPLICATION_REQUESTED,
                  UINT64_C(301));
    assert(evolution.mutation_calls == 0);
    assert(stop.calls == 1);
    assert(observer.calls == 1);
    assert(trace.count == 2);
    assert_trace_event(&trace, 0, 'S', 0);
    assert_trace_event(&trace, 1, 'O', 0);
    assert(stop.records[0].termination_reason == EVO_TERMINATION_NONE);
    assert(observer.records[0].termination_reason ==
           EVO_TERMINATION_APPLICATION_REQUESTED);
    evo_result_destroy(&result);
}

static void test_intermediate_stop_after_promoted_child(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t evolution = make_evolution();
    callback_trace_t trace = {0};
    stop_context_t stop = {
        .stop_generation = 2,
    };
    observer_context_t observer = {0};
    evo_config_t config = make_config(5, UINT64_C(302), &stop, &observer);
    evo_result_t result = {0};

    evolution.mutation_values[0] = 10;
    evolution.mutation_values[1] = 20;
    bind_contexts(&stop, &observer, &trace, &evolution, &result);
    assert(evo_run(&problem, &config, &evolution, &result) == EVO_SUCCESS);
    assert_result(&result,
                  2,
                  20,
                  EVO_TERMINATION_APPLICATION_REQUESTED,
                  UINT64_C(302));
    assert(evolution.mutation_calls == 2 * TEST_POPULATION_SIZE);
    assert(stop.calls == 3);
    assert(observer.calls == 3);
    assert(trace.count == 6);
    for (size_t generation = 0; generation <= 2; ++generation) {
        assert_trace_event(&trace, generation * 2, 'S', generation);
        assert_trace_event(&trace, generation * 2 + 1, 'O', generation);
    }
    assert(observer.records[0].termination_reason == EVO_TERMINATION_NONE);
    assert(observer.records[1].termination_reason == EVO_TERMINATION_NONE);
    assert(observer.records[2].termination_reason ==
           EVO_TERMINATION_APPLICATION_REQUESTED);
    evo_result_destroy(&result);
}

static void assert_results_equal(const evo_result_t *left,
                                 const evo_result_t *right)
{
    assert(left->best_genome != NULL);
    assert(right->best_genome != NULL);
    assert(((const unsigned char *)left->best_genome)[0] ==
           ((const unsigned char *)right->best_genome)[0]);
    assert(fitness_equal(&left->best_fitness, &right->best_fitness));
    assert(left->generations_completed == right->generations_completed);
    assert(left->random_seed == right->random_seed);
    assert(left->termination_reason == right->termination_reason);
    assert(statistics_equal(&left->generation_statistics,
                            &right->generation_statistics));
}

static void assert_observers_equal(const observer_context_t *left,
                                   const observer_context_t *right)
{
    assert(left->calls == right->calls);
    for (size_t index = 0; index < left->calls; ++index) {
        assert(left->records[index].best_value ==
               right->records[index].best_value);
        assert(left->records[index].generation ==
               right->records[index].generation);
        assert(left->records[index].termination_reason ==
               right->records[index].termination_reason);
        assert(statistics_equal(&left->records[index].statistics,
                                &right->records[index].statistics));
    }
}

static void test_never_stop_replays_static_configuration(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t static_evolution = make_evolution();
    evolution_context_t stop_evolution = make_evolution();
    callback_trace_t static_trace = {0};
    callback_trace_t stop_trace = {0};
    stop_context_t stop = {
        .stop_generation = SIZE_MAX,
    };
    observer_context_t static_observer = {0};
    observer_context_t stop_observer = {0};
    evo_config_t static_config =
        make_config(3, UINT64_C(303), NULL, &static_observer);
    evo_config_t stop_config =
        make_config(3, UINT64_C(303), &stop, &stop_observer);
    evo_result_t static_result = {0};
    evo_result_t stop_result = {0};

    for (size_t index = 0; index < 3; ++index) {
        static_evolution.mutation_values[index] =
            (unsigned char)((index + 1) * 10);
        stop_evolution.mutation_values[index] =
            static_evolution.mutation_values[index];
    }
    bind_contexts(NULL,
                  &static_observer,
                  &static_trace,
                  &static_evolution,
                  &static_result);
    bind_contexts(&stop,
                  &stop_observer,
                  &stop_trace,
                  &stop_evolution,
                  &stop_result);

    assert(evo_run(&problem,
                   &static_config,
                   &static_evolution,
                   &static_result) == EVO_SUCCESS);
    assert(evo_run(&problem,
                   &stop_config,
                   &stop_evolution,
                   &stop_result) == EVO_SUCCESS);
    assert_results_equal(&static_result, &stop_result);
    assert_observers_equal(&static_observer, &stop_observer);
    assert(stop.calls == 3);
    assert(stop_observer.calls == 4);
    assert(stop.records[0].generation == 0);
    assert(stop.records[1].generation == 1);
    assert(stop.records[2].generation == 2);
    assert(stop_observer.records[3].generation == 3);
    assert(stop_observer.records[3].termination_reason ==
           EVO_TERMINATION_GENERATION_LIMIT);
    assert(static_evolution.initialization_calls ==
           stop_evolution.initialization_calls);
    assert(static_evolution.mutation_calls ==
           stop_evolution.mutation_calls);

    evo_result_destroy(&static_result);
    evo_result_destroy(&stop_result);
}

static void test_natural_termination_suppresses_stop_decision(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t zero_evolution = make_evolution();
    evolution_context_t invalid_evolution = make_evolution();
    callback_trace_t zero_trace = {0};
    callback_trace_t invalid_trace = {0};
    stop_context_t zero_stop = {
        .stop_generation = 0,
    };
    stop_context_t invalid_stop = {
        .stop_generation = SIZE_MAX,
    };
    observer_context_t zero_observer = {0};
    observer_context_t invalid_observer = {0};
    evo_config_t zero_config =
        make_config(0, UINT64_C(304), &zero_stop, &zero_observer);
    evo_config_t invalid_config =
        make_config(3, UINT64_C(305), &invalid_stop, &invalid_observer);
    evo_result_t zero_result = {0};
    evo_result_t invalid_result = {0};

    bind_contexts(&zero_stop,
                  &zero_observer,
                  &zero_trace,
                  &zero_evolution,
                  &zero_result);
    assert(evo_run(&problem,
                   &zero_config,
                   &zero_evolution,
                   &zero_result) == EVO_SUCCESS);
    assert(zero_stop.calls == 0);
    assert(zero_observer.calls == 1);
    assert_result(&zero_result,
                  0,
                  4,
                  EVO_TERMINATION_GENERATION_LIMIT,
                  UINT64_C(304));
    assert(zero_observer.records[0].termination_reason ==
           EVO_TERMINATION_GENERATION_LIMIT);

    invalid_evolution.reject_value = true;
    invalid_evolution.rejected_value = 0xee;
    invalid_evolution.mutation_values[0] = 0xee;
    bind_contexts(&invalid_stop,
                  &invalid_observer,
                  &invalid_trace,
                  &invalid_evolution,
                  &invalid_result);
    assert(evo_run(&problem,
                   &invalid_config,
                   &invalid_evolution,
                   &invalid_result) == EVO_SUCCESS);
    assert(invalid_stop.calls == 1);
    assert(invalid_observer.calls == 2);
    assert_result(&invalid_result,
                  1,
                  4,
                  EVO_TERMINATION_ALL_INVALID,
                  UINT64_C(305));
    assert(invalid_observer.records[1].termination_reason ==
           EVO_TERMINATION_ALL_INVALID);

    evo_result_destroy(&zero_result);
    evo_result_destroy(&invalid_result);
}

static void test_failed_child_emits_no_partial_stop_decision(void)
{
    evo_problem_t problem = make_problem();
    evolution_context_t evolution = make_evolution();
    callback_trace_t trace = {0};
    stop_context_t stop = {
        .stop_generation = SIZE_MAX,
    };
    observer_context_t observer = {0};
    evo_config_t config = make_config(3, UINT64_C(306), &stop, &observer);
    evo_result_t result = {0};

    evolution.mutation_values[0] = 0xdd;
    evolution.non_finite_value = 0xdd;
    evolution.produce_non_finite = true;
    bind_contexts(&stop, &observer, &trace, &evolution, &result);
    assert(evo_run(&problem, &config, &evolution, &result) ==
           EVO_ERROR_EVALUATION);
    assert(stop.calls == 1);
    assert(observer.calls == 1);
    assert(trace.count == 2);
    assert_trace_event(&trace, 0, 'S', 0);
    assert_trace_event(&trace, 1, 'O', 0);
    assert_result_empty(&result);
}

int main(void)
{
    test_immediate_stop_after_generation_zero();
    test_intermediate_stop_after_promoted_child();
    test_never_stop_replays_static_configuration();
    test_natural_termination_suppresses_stop_decision();
    test_failed_child_emits_no_partial_stop_decision();
    return 0;
}
