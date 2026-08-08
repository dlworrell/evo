#include "catalyst/evo/evo.h"
#include "internal/evaluation_workers.h"
#include "internal/population_evaluation.h"
#include "internal/population_storage.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>

#define TEST_POPULATION_SIZE 8
#define TEST_MAX_WORKERS 4

typedef struct schedule_capture {
    evo_evaluation_schedule_t schedule;
    evo_evaluation_assignment_t assignments[TEST_POPULATION_SIZE];
    size_t calls;
} schedule_capture_t;

typedef struct evaluation_context {
    size_t next_initial_index;
    size_t initialization_calls;
    size_t validity_calls;
    size_t validity_order[TEST_POPULATION_SIZE];
    unsigned int invalid_mask;
    size_t failing_index;
    bool failure_enabled;
    atomic_size_t evaluation_calls;
    atomic_size_t active_callbacks;
    atomic_size_t maximum_active_callbacks;
    schedule_capture_t capture;
} evaluation_context_t;

typedef struct failing_backend_context {
    size_t create_calls;
    size_t join_calls;
    size_t fail_create_call;
    size_t fail_join_call;
} failing_backend_context_t;

typedef struct run_schedule_capture {
    evo_evaluation_schedule_t schedules[4];
    evo_evaluation_assignment_t assignments[4][TEST_POPULATION_SIZE];
    size_t calls;
} run_schedule_capture_t;

static void initialize_index(void *genome, void *opaque)
{
    evaluation_context_t *context = opaque;
    unsigned char *bytes = genome;

    assert(context->next_initial_index < TEST_POPULATION_SIZE);
    bytes[0] = (unsigned char)context->next_initial_index;
    ++context->next_initial_index;
    ++context->initialization_calls;
}

static bool validate_index(const void *genome, void *opaque)
{
    evaluation_context_t *context = opaque;
    const unsigned char *bytes = genome;
    const size_t index = bytes[0];

    assert(context->validity_calls < TEST_POPULATION_SIZE);
    context->validity_order[context->validity_calls] = index;
    ++context->validity_calls;
    return (context->invalid_mask & (1U << index)) == 0;
}

static evo_fitness_t evaluate_index(const void *genome, void *opaque)
{
    evaluation_context_t *context = opaque;
    const unsigned char *bytes = genome;
    const size_t index = bytes[0];
    const size_t active =
        atomic_fetch_add_explicit(&context->active_callbacks,
                                  1,
                                  memory_order_acq_rel) +
        1;
    size_t maximum = atomic_load_explicit(
        &context->maximum_active_callbacks,
        memory_order_acquire);

    while (maximum < active &&
           !atomic_compare_exchange_weak_explicit(
               &context->maximum_active_callbacks,
               &maximum,
               active,
               memory_order_acq_rel,
               memory_order_acquire)) {
    }
    for (size_t attempt = 0; attempt < 128; ++attempt) {
        (void)sched_yield();
    }
    atomic_fetch_add_explicit(&context->evaluation_calls,
                              1,
                              memory_order_acq_rel);
    atomic_fetch_sub_explicit(&context->active_callbacks,
                              1,
                              memory_order_acq_rel);
    if (context->failure_enabled && index == context->failing_index) {
        return (evo_fitness_t){.total = NAN};
    }
    return (evo_fitness_t){
        .correctness = (double)index,
        .performance = (double)(TEST_POPULATION_SIZE - index),
        .total = (double)index,
    };
}

static void capture_schedule(const evo_evaluation_schedule_t *schedule,
                             void *opaque)
{
    evaluation_context_t *context = opaque;
    schedule_capture_t *capture = &context->capture;

    assert(capture->calls == 0);
    assert(schedule->assignment_count == TEST_POPULATION_SIZE);
    capture->schedule = *schedule;
    for (size_t index = 0; index < schedule->assignment_count; ++index) {
        capture->assignments[index] = schedule->assignments[index];
    }
    capture->schedule.assignments = capture->assignments;
    ++capture->calls;
}

static void capture_run_schedule(
    const evo_evaluation_schedule_t *schedule,
    void *opaque)
{
    run_schedule_capture_t *capture = opaque;
    const size_t capture_index = capture->calls;

    assert(capture_index < 4);
    assert(schedule->assignment_count == TEST_POPULATION_SIZE);
    capture->schedules[capture_index] = *schedule;
    for (size_t index = 0; index < schedule->assignment_count; ++index) {
        capture->assignments[capture_index][index] =
            schedule->assignments[index];
    }
    capture->schedules[capture_index].assignments =
        capture->assignments[capture_index];
    ++capture->calls;
}

static evo_problem_t make_problem(void)
{
    return (evo_problem_t){
        .genome_size = 1,
        .initialize = initialize_index,
        .evaluate = evaluate_index,
        .is_valid = validate_index,
        .evaluation_callback_thread_safety =
            EVO_EVALUATION_CALLBACK_THREAD_SAFE,
    };
}

static evo_config_t make_config(void)
{
    return (evo_config_t){
        .population_size = TEST_POPULATION_SIZE,
        .max_genome_bytes = 1,
        .max_population_bytes = TEST_POPULATION_SIZE,
        .max_evaluation_bytes =
            TEST_POPULATION_SIZE * sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes = TEST_POPULATION_SIZE,
        .max_diversity_work = SIZE_MAX,
    };
}

static void initialize_context(evaluation_context_t *context)
{
    *context = (evaluation_context_t){0};
    atomic_init(&context->evaluation_calls, 0);
    atomic_init(&context->active_callbacks, 0);
    atomic_init(&context->maximum_active_callbacks, 0);
}

static void configure_workers(evo_config_t *config,
                              size_t worker_count,
                              evaluation_context_t *context)
{
    size_t scratch_size = 0;

    assert(evo_evaluation_worker_scratch_size(
               config->population_size,
               worker_count,
               &scratch_size) == EVO_SUCCESS);
    config->evaluation_worker_count = worker_count;
    config->max_evaluation_worker_scratch_bytes = scratch_size;
    config->evaluation_schedule_observer = capture_schedule;
    config->evaluation_schedule_observer_context = context;
}

static void create_initialized_population(
    const evo_problem_t *problem,
    const evo_config_t *config,
    evaluation_context_t *context,
    evo_population_t *population)
{
    assert(evo_population_create(problem, config, population) == EVO_SUCCESS);
    assert(evo_population_initialize(problem,
                                     config,
                                     context,
                                     population) == EVO_SUCCESS);
}

static void assert_validity_order(const evaluation_context_t *context)
{
    assert(context->validity_calls == TEST_POPULATION_SIZE);
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        assert(context->validity_order[index] == index);
    }
}

static void assert_population_matches(
    const evo_population_t *left,
    const evo_population_t *right)
{
    assert(left->population_size == right->population_size);
    assert(left->valid_count == right->valid_count);
    assert(left->best_index == right->best_index);
    assert(left->has_best == right->has_best);
    assert(left->evaluated == right->evaluated);
    for (size_t index = 0; index < left->population_size; ++index) {
        const evo_candidate_evaluation_t *left_evaluation =
            evo_population_evaluation_const(left, index);
        const evo_candidate_evaluation_t *right_evaluation =
            evo_population_evaluation_const(right, index);

        assert(left_evaluation != NULL);
        assert(right_evaluation != NULL);
        assert(left_evaluation->valid == right_evaluation->valid);
        assert(left_evaluation->evaluated == right_evaluation->evaluated);
        assert(left_evaluation->fitness.correctness ==
               right_evaluation->fitness.correctness);
        assert(left_evaluation->fitness.performance ==
               right_evaluation->fitness.performance);
        assert(left_evaluation->fitness.total ==
               right_evaluation->fitness.total);
    }
}

static void assert_fitness_equal(const evo_fitness_t *left,
                                 const evo_fitness_t *right)
{
    assert(left->correctness == right->correctness);
    assert(left->performance == right->performance);
    assert(left->memory_use == right->memory_use);
    assert(left->reliability == right->reliability);
    assert(left->maintainability == right->maintainability);
    assert(left->constraint_penalty == right->constraint_penalty);
    assert(left->total == right->total);
}

static void assert_results_equal(const evo_result_t *left,
                                 const evo_result_t *right)
{
    assert(left->generations_completed == right->generations_completed);
    assert(left->random_seed == right->random_seed);
    assert(left->termination_reason == right->termination_reason);
    assert(left->best_genome_size == right->best_genome_size);
    assert(left->secure_erasure_policy_version ==
           right->secure_erasure_policy_version);
    assert(left->secure_erasure_backend == right->secure_erasure_backend);
    assert(left->secure_erasure_enabled == right->secure_erasure_enabled);
    assert_fitness_equal(&left->best_fitness, &right->best_fitness);
    assert(left->generation_statistics.version ==
           right->generation_statistics.version);
    assert(left->generation_statistics.generation_index ==
           right->generation_statistics.generation_index);
    assert(left->generation_statistics.population_size ==
           right->generation_statistics.population_size);
    assert(left->generation_statistics.valid_count ==
           right->generation_statistics.valid_count);
    assert(left->generation_statistics.invalid_count ==
           right->generation_statistics.invalid_count);
    assert(left->generation_statistics.best_index ==
           right->generation_statistics.best_index);
    assert(left->generation_statistics.has_best ==
           right->generation_statistics.has_best);
    assert_fitness_equal(&left->generation_statistics.best_fitness,
                         &right->generation_statistics.best_fitness);
    assert_fitness_equal(&left->generation_statistics.fitness_sums,
                         &right->generation_statistics.fitness_sums);
    assert(left->generation_statistics.diversity ==
           right->generation_statistics.diversity);
    for (size_t index = 0; index < left->best_genome_size; ++index) {
        const unsigned char *left_bytes = left->best_genome;
        const unsigned char *right_bytes = right->best_genome;

        assert(left_bytes[index] == right_bytes[index]);
    }
}

static void test_scratch_size_and_configuration_boundaries(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = make_config();
    evaluation_context_t context = {0};
    evo_result_t result = {0};
    size_t scratch_size = 91;

    assert(evo_evaluation_worker_scratch_size(
               TEST_POPULATION_SIZE, 0, &scratch_size) == EVO_SUCCESS);
    assert(scratch_size == 0);
    scratch_size = 91;
    assert(evo_evaluation_worker_scratch_size(
               0, 1, &scratch_size) == EVO_ERROR_RESOURCE_LIMIT);
    assert(scratch_size == 91);
    assert(evo_evaluation_worker_scratch_size(
               TEST_POPULATION_SIZE,
               TEST_POPULATION_SIZE + 1,
               &scratch_size) == EVO_ERROR_RESOURCE_LIMIT);
    assert(scratch_size == 91);
    assert(evo_evaluation_worker_scratch_size(
               TEST_POPULATION_SIZE, 2, NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);

    initialize_context(&context);
    configure_workers(&config, 2, &context);
    --config.max_evaluation_worker_scratch_bytes;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert(context.initialization_calls == 0);
    assert(context.validity_calls == 0);
    assert(atomic_load_explicit(&context.evaluation_calls,
                                memory_order_acquire) == 0);
    assert(result.best_genome == NULL);

    problem.evaluation_callback_thread_safety =
        EVO_EVALUATION_CALLBACK_SERIAL;
    ++config.max_evaluation_worker_scratch_bytes;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(context.initialization_calls == 0);

    problem.evaluation_callback_thread_safety =
        EVO_EVALUATION_CALLBACK_THREAD_SAFE;
    config.evaluation_worker_count = 0;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(context.initialization_calls == 0);
}

static void test_worker_counts_match_serial_reference(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t serial_config = make_config();
    evaluation_context_t serial_context = {0};
    evo_population_t serial = {0};

    initialize_context(&serial_context);
    serial_context.invalid_mask = (1U << 2) | (1U << 5);
    create_initialized_population(&problem,
                                  &serial_config,
                                  &serial_context,
                                  &serial);
    assert(evo_population_evaluate(&problem,
                                   &serial_config,
                                   &serial_context,
                                   &serial) == EVO_SUCCESS);
    assert(serial.parallel_evaluation_policy_version == 0);
    assert(serial.evaluation_worker_count == 0);
    assert_validity_order(&serial_context);

    for (size_t worker_count = 1;
         worker_count <= TEST_MAX_WORKERS;
         ++worker_count) {
        evo_config_t config = make_config();
        evaluation_context_t context = {0};
        evo_population_t parallel = {0};
        size_t commit_order = 0;

        initialize_context(&context);
        context.invalid_mask = serial_context.invalid_mask;
        configure_workers(&config, worker_count, &context);
        create_initialized_population(&problem, &config, &context, &parallel);
        assert(evo_population_evaluate(&problem,
                                       &config,
                                       &context,
                                       &parallel) == EVO_SUCCESS);
        assert_population_matches(&serial, &parallel);
        assert(parallel.parallel_evaluation_policy_version ==
               EVO_PARALLEL_EVALUATION_POLICY_VERSION);
        assert(parallel.evaluation_worker_count == worker_count);
        assert_validity_order(&context);
        assert(context.capture.calls == 1);
        assert(context.capture.schedule.outcome ==
               EVO_EVALUATION_SCHEDULE_COMMITTED);
        assert(context.capture.schedule.worker_count == worker_count);
        assert(context.capture.schedule.validated_count ==
               TEST_POPULATION_SIZE);
        assert(context.capture.schedule.hard_invalid_count == 2);
        assert(context.capture.schedule.scheduled_count == 6);
        assert(context.capture.schedule.completed_count == 6);
        assert(context.capture.schedule.failed_count == 0);
        assert(context.capture.schedule.canceled_count == 0);
        assert(context.capture.schedule.committed_count == 6);
        assert(context.capture.schedule.complete);
        for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
            const evo_evaluation_assignment_t *assignment =
                &context.capture.assignments[index];
            const bool valid =
                (context.invalid_mask & (1U << index)) == 0;

            assert(assignment->population_index == index);
            assert(assignment->worker_identity ==
                   index % worker_count + 1);
            assert(assignment->dispatch_wave == index / worker_count);
            assert(assignment->committed == valid);
            if (valid) {
                assert(assignment->disposition ==
                       EVO_EVALUATION_COMPLETED);
                assert(assignment->commit_order == commit_order);
                ++commit_order;
            } else {
                assert(assignment->disposition ==
                       EVO_EVALUATION_EXCLUDED);
                assert(assignment->commit_order == 0);
            }
        }
        assert(atomic_load_explicit(&context.evaluation_calls,
                                    memory_order_acquire) == 6);
        assert(atomic_load_explicit(&context.maximum_active_callbacks,
                                    memory_order_acquire) >= 1);
        assert(atomic_load_explicit(&context.maximum_active_callbacks,
                                    memory_order_acquire) <= worker_count);
        evo_population_destroy(&parallel);
    }
    evo_population_destroy(&serial);
}

static void test_non_finite_wave_cancels_later_work(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = make_config();
    evaluation_context_t context = {0};
    evo_population_t population = {0};

    initialize_context(&context);
    context.failure_enabled = true;
    context.failing_index = 1;
    configure_workers(&config, 3, &context);
    create_initialized_population(&problem, &config, &context, &population);
    assert(evo_population_evaluate(&problem,
                                   &config,
                                   &context,
                                   &population) ==
           EVO_ERROR_EVALUATION);
    assert(population.evaluations == NULL);
    assert(population.evaluation_bytes == 0);
    assert(!population.evaluated);
    assert(population.valid_count == 0);
    assert(population.parallel_evaluation_policy_version == 0);
    assert(population.evaluation_worker_count == 0);
    assert_validity_order(&context);
    assert(atomic_load_explicit(&context.evaluation_calls,
                                memory_order_acquire) == 3);
    assert(context.capture.calls == 1);
    assert(context.capture.schedule.outcome ==
           EVO_EVALUATION_SCHEDULE_FITNESS_REJECTED);
    assert(context.capture.schedule.completed_count == 2);
    assert(context.capture.schedule.failed_count == 1);
    assert(context.capture.schedule.canceled_count == 5);
    assert(context.capture.schedule.committed_count == 0);
    assert(context.capture.schedule.has_failure_index);
    assert(context.capture.schedule.first_failure_index == 1);
    assert(context.capture.schedule.failed_worker_identity == 2);
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        const evo_evaluation_assignment_disposition_t expected =
            index == 1
                ? EVO_EVALUATION_FAILED
                : (index < 3 ? EVO_EVALUATION_COMPLETED
                             : EVO_EVALUATION_CANCELED);

        assert(context.capture.assignments[index].disposition == expected);
        assert(!context.capture.assignments[index].committed);
    }
    evo_population_destroy(&population);
}

static int inject_create_failure(pthread_t *thread,
                                 void *(*entry)(void *),
                                 void *argument,
                                 void *opaque)
{
    failing_backend_context_t *context = opaque;

    ++context->create_calls;
    if (context->create_calls == context->fail_create_call) {
        return ENOMEM;
    }
    return pthread_create(thread, NULL, entry, argument);
}

static int join_real_thread(pthread_t thread, void **result, void *opaque)
{
    failing_backend_context_t *context = opaque;

    ++context->join_calls;
    if (context->join_calls == context->fail_join_call) {
        return EINVAL;
    }
    return pthread_join(thread, result);
}

static void test_worker_start_failure_joins_without_callbacks(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = make_config();
    evaluation_context_t context = {0};
    failing_backend_context_t backend_context = {
        .fail_create_call = 2,
    };
    const evo_evaluation_thread_backend_t backend = {
        .create = inject_create_failure,
        .join = join_real_thread,
        .context = &backend_context,
    };
    evo_population_t population = {0};

    initialize_context(&context);
    configure_workers(&config, 3, &context);
    create_initialized_population(&problem, &config, &context, &population);
    assert(evo_population_evaluate_ready_with_worker_backend(
               &problem,
               &config,
               &context,
               &population,
               &backend) == EVO_ERROR_OUT_OF_MEMORY);
    assert(backend_context.create_calls == 2);
    assert(backend_context.join_calls == 1);
    assert(context.validity_calls == 0);
    assert(atomic_load_explicit(&context.evaluation_calls,
                                memory_order_acquire) == 0);
    assert(population.evaluations == NULL);
    assert(!population.evaluated);
    assert(context.capture.calls == 1);
    assert(context.capture.schedule.outcome ==
           EVO_EVALUATION_SCHEDULE_WORKER_START_FAILED);
    assert(context.capture.schedule.failed_worker_identity == 2);
    assert(context.capture.schedule.validated_count == 0);
    assert(context.capture.schedule.scheduled_count == 0);
    assert(context.capture.schedule.committed_count == 0);
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        assert(context.capture.assignments[index].disposition ==
               EVO_EVALUATION_NOT_VALIDATED);
    }
    evo_population_destroy(&population);
}

static void test_worker_join_failure_terminates_before_rollback(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = make_config();
    evaluation_context_t context = {0};
    failing_backend_context_t backend_context = {
        .fail_join_call = 2,
    };
    const evo_evaluation_thread_backend_t backend = {
        .create = inject_create_failure,
        .join = join_real_thread,
        .context = &backend_context,
    };
    evo_population_t population = {0};

    initialize_context(&context);
    configure_workers(&config, 3, &context);
    create_initialized_population(&problem, &config, &context, &population);
    assert(evo_population_evaluate_ready_with_worker_backend(
               &problem,
               &config,
               &context,
               &population,
               &backend) == EVO_ERROR_STATE);
    assert(backend_context.create_calls == 3);
    assert(backend_context.join_calls == 4);
    assert(context.validity_calls == TEST_POPULATION_SIZE);
    assert(atomic_load_explicit(&context.evaluation_calls,
                                memory_order_acquire) ==
           TEST_POPULATION_SIZE);
    assert(atomic_load_explicit(&context.active_callbacks,
                                memory_order_acquire) == 0);
    assert(population.evaluations == NULL);
    assert(!population.evaluated);
    assert(context.capture.calls == 1);
    assert(context.capture.schedule.outcome ==
           EVO_EVALUATION_SCHEDULE_WORKER_JOIN_FAILED);
    assert(context.capture.schedule.failed_worker_identity == 2);
    assert(context.capture.schedule.completed_count ==
           TEST_POPULATION_SIZE);
    assert(context.capture.schedule.canceled_count == 0);
    assert(context.capture.schedule.committed_count == 0);
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        assert(context.capture.assignments[index].disposition ==
               EVO_EVALUATION_COMPLETED);
        assert(!context.capture.assignments[index].committed);
    }
    evo_population_destroy(&population);
}

static void test_public_multigeneration_replay_and_commit_order(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t serial_config = make_config();
    evaluation_context_t serial_context = {0};
    evo_result_t serial = {0};

    problem.is_valid = NULL;
    serial_config.generation_limit = 3;
    serial_config.tournament_size = 2;
    serial_config.population_recycling_enabled = true;
    initialize_context(&serial_context);
    assert(evo_run(&problem,
                   &serial_config,
                   &serial_context,
                   &serial) == EVO_SUCCESS);
    assert(serial.generations_completed == 3);

    for (size_t worker_count = 1;
         worker_count <= TEST_MAX_WORKERS;
         ++worker_count) {
        evo_config_t config = serial_config;
        evaluation_context_t context = {0};
        run_schedule_capture_t schedules = {0};
        evo_result_t parallel = {0};
        size_t scratch_size = 0;

        initialize_context(&context);
        assert(evo_evaluation_worker_scratch_size(
                   config.population_size,
                   worker_count,
                   &scratch_size) == EVO_SUCCESS);
        config.evaluation_worker_count = worker_count;
        config.max_evaluation_worker_scratch_bytes = scratch_size;
        config.evaluation_schedule_observer = capture_run_schedule;
        config.evaluation_schedule_observer_context = &schedules;
        assert(evo_run(&problem, &config, &context, &parallel) ==
               EVO_SUCCESS);
        assert_results_equal(&serial, &parallel);
        assert(schedules.calls == 4);
        for (size_t generation = 0; generation < schedules.calls;
             ++generation) {
            const evo_evaluation_schedule_t *schedule =
                &schedules.schedules[generation];

            assert(schedule->population_generation == generation);
            assert(schedule->worker_count == worker_count);
            assert(schedule->outcome ==
                   EVO_EVALUATION_SCHEDULE_COMMITTED);
            assert(schedule->validated_count == TEST_POPULATION_SIZE);
            assert(schedule->hard_invalid_count == 0);
            assert(schedule->scheduled_count == TEST_POPULATION_SIZE);
            assert(schedule->completed_count == TEST_POPULATION_SIZE);
            assert(schedule->committed_count == TEST_POPULATION_SIZE);
            for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
                const evo_evaluation_assignment_t *assignment =
                    &schedules.assignments[generation][index];

                assert(assignment->worker_identity ==
                       index % worker_count + 1);
                assert(assignment->dispatch_wave == index / worker_count);
                assert(assignment->commit_order == index);
                assert(assignment->disposition ==
                       EVO_EVALUATION_COMPLETED);
                assert(assignment->committed);
            }
        }
        evo_result_destroy(&parallel);
    }
    evo_result_destroy(&serial);
}

int main(void)
{
    test_scratch_size_and_configuration_boundaries();
    test_worker_counts_match_serial_reference();
    test_non_finite_wave_cancels_later_work();
    test_worker_start_failure_joins_without_callbacks();
    test_worker_join_failure_terminates_before_rollback();
    test_public_multigeneration_replay_and_commit_order();
    return 0;
}
