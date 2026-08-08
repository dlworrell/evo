#include "internal/evaluation_workers.h"

#include "internal/fitness.h"

#include <errno.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct evaluation_scheduler evaluation_scheduler_t;

typedef struct evaluation_worker {
    pthread_t thread;
    evaluation_scheduler_t *scheduler;
    size_t worker_index;
    atomic_size_t completed_epoch;
    atomic_bool terminated;
    bool started;
} evaluation_worker_t;

struct evaluation_scheduler {
    const evo_problem_t *problem;
    void *consumer_context;
    const evo_population_t *population;
    evo_candidate_evaluation_t *evaluations;
    size_t worker_count;
    atomic_size_t epoch;
    atomic_bool stop;
};

static bool checked_size_add(size_t left,
                             size_t right,
                             size_t *sum)
{
    if (sum == NULL || right > SIZE_MAX - left) {
        return false;
    }
    *sum = left + right;
    return true;
}

static bool checked_size_multiply(size_t left,
                                  size_t right,
                                  size_t *product)
{
    if (product == NULL || (left != 0 && right > SIZE_MAX / left)) {
        return false;
    }
    *product = left * right;
    return true;
}

static bool checked_align_size(size_t value,
                               size_t alignment,
                               size_t *aligned)
{
    size_t remainder = 0;
    size_t padding = 0;

    if (aligned == NULL || alignment == 0) {
        return false;
    }
    remainder = value % alignment;
    padding = remainder == 0 ? 0 : alignment - remainder;
    return checked_size_add(value, padding, aligned);
}

static bool scratch_layout_size(size_t population_size,
                                size_t worker_count,
                                size_t *assignment_offset,
                                size_t *scratch_size)
{
    size_t worker_bytes = 0;
    size_t assignment_bytes = 0;
    size_t offset = 0;
    size_t total = 0;

    if (population_size == 0 || worker_count == 0 ||
        worker_count > population_size || assignment_offset == NULL ||
        scratch_size == NULL ||
        !checked_size_multiply(worker_count,
                               sizeof(evaluation_worker_t),
                               &worker_bytes) ||
        !checked_align_size(worker_bytes,
                            _Alignof(evo_evaluation_assignment_t),
                            &offset) ||
        !checked_size_multiply(population_size,
                               sizeof(evo_evaluation_assignment_t),
                               &assignment_bytes) ||
        !checked_size_add(offset, assignment_bytes, &total)) {
        return false;
    }
    *assignment_offset = offset;
    *scratch_size = total;
    return true;
}

evo_status_t evo_evaluation_worker_scratch_size(
    size_t population_size,
    size_t worker_count,
    size_t *scratch_size)
{
    size_t assignment_offset = 0;
    size_t candidate = 0;

    if (scratch_size == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (population_size == 0 || worker_count > population_size) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    if (worker_count == 0) {
        *scratch_size = 0;
        return EVO_SUCCESS;
    }
    if (!scratch_layout_size(population_size,
                             worker_count,
                             &assignment_offset,
                             &candidate)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    (void)assignment_offset;
    *scratch_size = candidate;
    return EVO_SUCCESS;
}

evo_status_t evo_evaluation_workers_validate_config(
    const evo_problem_t *problem,
    const evo_config_t *config)
{
    size_t required_scratch = 0;

    if (problem == NULL || config == NULL ||
        (problem->evaluation_callback_thread_safety !=
             EVO_EVALUATION_CALLBACK_SERIAL &&
         problem->evaluation_callback_thread_safety !=
             EVO_EVALUATION_CALLBACK_THREAD_SAFE)) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (config->evaluation_worker_count == 0) {
        return config->max_evaluation_worker_scratch_bytes == 0 &&
                       config->evaluation_schedule_observer == NULL
                   ? EVO_SUCCESS
                   : EVO_ERROR_INVALID_ARGUMENT;
    }
    if (problem->evaluation_callback_thread_safety !=
            EVO_EVALUATION_CALLBACK_THREAD_SAFE ||
        config->evaluation_worker_count > config->population_size) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (evo_evaluation_worker_scratch_size(
            config->population_size,
            config->evaluation_worker_count,
            &required_scratch) != EVO_SUCCESS ||
        config->max_evaluation_worker_scratch_bytes < required_scratch) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    return EVO_SUCCESS;
}

static int default_thread_create(pthread_t *thread,
                                 void *(*entry)(void *),
                                 void *argument,
                                 void *context)
{
    (void)context;
    return pthread_create(thread, NULL, entry, argument);
}

static int default_thread_join(pthread_t thread,
                               void **result,
                               void *context)
{
    (void)context;
    return pthread_join(thread, result);
}

static const evo_evaluation_thread_backend_t default_backend = {
    .create = default_thread_create,
    .join = default_thread_join,
};

static void *evaluation_worker_main(void *argument)
{
    evaluation_worker_t *worker = argument;
    evaluation_scheduler_t *scheduler = worker->scheduler;
    size_t observed_epoch = 0;

    for (;;) {
        size_t epoch = atomic_load_explicit(&scheduler->epoch,
                                            memory_order_acquire);

        while (epoch == observed_epoch &&
               !atomic_load_explicit(&scheduler->stop,
                                     memory_order_acquire)) {
            (void)sched_yield();
            epoch = atomic_load_explicit(&scheduler->epoch,
                                         memory_order_acquire);
        }
        if (atomic_load_explicit(&scheduler->stop,
                                 memory_order_acquire)) {
            break;
        }
        observed_epoch = epoch;
        {
            const size_t wave = epoch - 1;
            const size_t population_index =
                wave * scheduler->worker_count + worker->worker_index;

            if (population_index < scheduler->population->population_size &&
                scheduler->evaluations[population_index].valid) {
                const unsigned char *genome =
                    scheduler->population->genomes +
                    population_index * scheduler->population->genome_size;

                scheduler->evaluations[population_index].fitness =
                    scheduler->problem->evaluate(
                        genome,
                        scheduler->consumer_context);
            }
        }
        atomic_store_explicit(&worker->completed_epoch,
                              epoch,
                              memory_order_release);
    }
    atomic_store_explicit(&worker->terminated,
                          true,
                          memory_order_release);
    return NULL;
}

static bool join_started_workers(
    evaluation_worker_t *workers,
    size_t worker_count,
    const evo_evaluation_thread_backend_t *backend,
    size_t *first_failure_identity)
{
    bool joined = true;

    for (size_t index = 0; index < worker_count; ++index) {
        void *worker_result = NULL;
        int join_status = 0;

        if (!workers[index].started) {
            continue;
        }
        join_status = backend->join(workers[index].thread,
                                    &worker_result,
                                    backend->context);
        if (join_status != 0 || worker_result != NULL) {
            if (first_failure_identity != NULL &&
                *first_failure_identity == 0) {
                *first_failure_identity = index + 1;
            }
            joined = false;
            if (join_status != 0) {
                void *recovery_result = NULL;
                int recovery_status = 0;

                while (!atomic_load_explicit(&workers[index].terminated,
                                             memory_order_acquire)) {
                    (void)sched_yield();
                }
                recovery_status = backend->join(workers[index].thread,
                                                &recovery_result,
                                                backend->context);
                if (recovery_status != 0) {
                    (void)pthread_detach(workers[index].thread);
                }
            }
        }
    }
    return joined;
}

static void clear_and_release_scratch(void *scratch, size_t scratch_size)
{
    unsigned char *bytes = scratch;

    for (size_t index = 0; index < scratch_size; ++index) {
        bytes[index] = 0;
    }
    free(scratch);
}

static void notify_schedule(
    const evo_config_t *config,
    const evo_evaluation_schedule_t *schedule)
{
    if (config->evaluation_schedule_observer != NULL) {
        config->evaluation_schedule_observer(
            schedule,
            config->evaluation_schedule_observer_context);
    }
}

static void cancel_pending_assignments(
    evo_evaluation_assignment_t *assignments,
    size_t population_size,
    evo_evaluation_schedule_t *schedule)
{
    for (size_t index = 0; index < population_size; ++index) {
        if (assignments[index].disposition == EVO_EVALUATION_PENDING) {
            assignments[index].disposition = EVO_EVALUATION_CANCELED;
            ++schedule->canceled_count;
        }
    }
}

static uint64_t population_generation(const evo_population_t *population)
{
    return population->initialized
               ? UINT64_C(0)
               : population->source_generation + UINT64_C(1);
}

evo_status_t evo_evaluation_workers_run(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    const evo_population_t *population,
    evo_candidate_evaluation_t *evaluations,
    size_t *valid_count,
    size_t *best_index,
    bool *has_best,
    const evo_evaluation_thread_backend_t *backend)
{
    const evo_evaluation_thread_backend_t *selected_backend =
        backend == NULL ? &default_backend : backend;
    evaluation_scheduler_t scheduler = {0};
    evo_evaluation_schedule_t schedule = {0};
    evaluation_worker_t *workers = NULL;
    evo_evaluation_assignment_t *assignments = NULL;
    void *scratch = NULL;
    size_t assignment_offset = 0;
    size_t scratch_size = 0;
    size_t local_valid_count = 0;
    size_t local_best_index = 0;
    size_t join_failure_identity = 0;
    bool local_has_best = false;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || population == NULL ||
        evaluations == NULL || valid_count == NULL || best_index == NULL ||
        has_best == NULL || config->evaluation_worker_count == 0 ||
        selected_backend->create == NULL || selected_backend->join == NULL ||
        population->source_generation == UINT64_MAX ||
        evo_evaluation_workers_validate_config(problem, config) !=
            EVO_SUCCESS ||
        !scratch_layout_size(population->population_size,
                             config->evaluation_worker_count,
                             &assignment_offset,
                             &scratch_size)) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    scratch = calloc(1, scratch_size);
    if (scratch == NULL) {
        return EVO_ERROR_OUT_OF_MEMORY;
    }
    workers = scratch;
    assignments = (evo_evaluation_assignment_t *)((unsigned char *)scratch +
                                                  assignment_offset);
    schedule = (evo_evaluation_schedule_t){
        .version = EVO_EVALUATION_SCHEDULE_VERSION,
        .policy_version = EVO_PARALLEL_EVALUATION_POLICY_VERSION,
        .population_generation = population_generation(population),
        .population_size = population->population_size,
        .worker_count = config->evaluation_worker_count,
        .scratch_bytes = scratch_size,
        .assignments = assignments,
        .assignment_count = population->population_size,
    };
    scheduler = (evaluation_scheduler_t){
        .problem = problem,
        .consumer_context = context,
        .population = population,
        .evaluations = evaluations,
        .worker_count = config->evaluation_worker_count,
    };
    atomic_init(&scheduler.epoch, 0);
    atomic_init(&scheduler.stop, false);

    for (size_t index = 0; index < population->population_size; ++index) {
        assignments[index] = (evo_evaluation_assignment_t){
            .population_index = index,
            .worker_identity =
                index % config->evaluation_worker_count + 1,
            .dispatch_wave = index / config->evaluation_worker_count,
            .disposition = EVO_EVALUATION_NOT_VALIDATED,
        };
    }
    for (size_t index = 0; index < config->evaluation_worker_count; ++index) {
        int create_status = 0;

        workers[index].scheduler = &scheduler;
        workers[index].worker_index = index;
        atomic_init(&workers[index].completed_epoch, 0);
        atomic_init(&workers[index].terminated, false);
        create_status = selected_backend->create(
            &workers[index].thread,
            evaluation_worker_main,
            &workers[index],
            selected_backend->context);
        if (create_status != 0) {
            schedule.failed_worker_identity = index + 1;
            schedule.outcome =
                EVO_EVALUATION_SCHEDULE_WORKER_START_FAILED;
            schedule.complete = true;
            atomic_store_explicit(&scheduler.stop,
                                  true,
                                  memory_order_release);
            if (!join_started_workers(workers,
                                      config->evaluation_worker_count,
                                      selected_backend,
                                      &join_failure_identity)) {
                schedule.outcome =
                    EVO_EVALUATION_SCHEDULE_WORKER_JOIN_FAILED;
                schedule.failed_worker_identity =
                    join_failure_identity;
            }
            notify_schedule(config, &schedule);
            clear_and_release_scratch(scratch, scratch_size);
            return create_status == ENOMEM || create_status == EAGAIN
                       ? EVO_ERROR_OUT_OF_MEMORY
                       : EVO_ERROR_STATE;
        }
        workers[index].started = true;
    }

    for (size_t index = 0; index < population->population_size; ++index) {
        const void *genome = evo_population_genome_const(population, index);

        if (genome == NULL) {
            status = EVO_ERROR_STATE;
            break;
        }
        evaluations[index].valid =
            problem->is_valid == NULL || problem->is_valid(genome, context);
        ++schedule.validated_count;
        if (evaluations[index].valid) {
            assignments[index].disposition = EVO_EVALUATION_PENDING;
            ++schedule.scheduled_count;
            ++local_valid_count;
        } else {
            assignments[index].disposition = EVO_EVALUATION_EXCLUDED;
            ++schedule.hard_invalid_count;
        }
    }

    if (status == EVO_SUCCESS) {
        const size_t wave_count =
            1 + (population->population_size - 1) /
                    config->evaluation_worker_count;

        for (size_t wave = 0; wave < wave_count; ++wave) {
            const size_t epoch = wave + 1;
            size_t wave_end = population->population_size;

            atomic_store_explicit(&scheduler.epoch,
                                  epoch,
                                  memory_order_release);
            for (size_t worker = 0;
                 worker < config->evaluation_worker_count;
                 ++worker) {
                while (atomic_load_explicit(
                           &workers[worker].completed_epoch,
                           memory_order_acquire) != epoch) {
                    (void)sched_yield();
                }
            }
            if (config->evaluation_worker_count <=
                population->population_size -
                    wave * config->evaluation_worker_count) {
                wave_end =
                    wave * config->evaluation_worker_count +
                    config->evaluation_worker_count;
            }
            for (size_t index =
                     wave * config->evaluation_worker_count;
                 index < wave_end;
                 ++index) {
                if (!evaluations[index].valid) {
                    continue;
                }
                if (evo_fitness_evidence_is_valid(
                        &evaluations[index].fitness)) {
                    assignments[index].disposition =
                        EVO_EVALUATION_COMPLETED;
                    ++schedule.completed_count;
                } else {
                    assignments[index].disposition =
                        EVO_EVALUATION_FAILED;
                    if (!schedule.has_failure_index) {
                        schedule.first_failure_index = index;
                        schedule.failed_worker_identity =
                            assignments[index].worker_identity;
                        schedule.has_failure_index = true;
                    }
                    ++schedule.failed_count;
                }
            }
            if (schedule.failed_count != 0) {
                schedule.outcome =
                    EVO_EVALUATION_SCHEDULE_FITNESS_REJECTED;
                cancel_pending_assignments(assignments,
                                           population->population_size,
                                           &schedule);
                status = EVO_ERROR_EVALUATION;
                break;
            }
        }
    }

    atomic_store_explicit(&scheduler.stop, true, memory_order_release);
    join_failure_identity = 0;
    if (!join_started_workers(workers,
                              config->evaluation_worker_count,
                              selected_backend,
                              &join_failure_identity)) {
        schedule.outcome = EVO_EVALUATION_SCHEDULE_WORKER_JOIN_FAILED;
        schedule.failed_worker_identity = join_failure_identity;
        cancel_pending_assignments(assignments,
                                   population->population_size,
                                   &schedule);
        status = EVO_ERROR_STATE;
    }

    if (status == EVO_SUCCESS) {
        for (size_t index = 0; index < population->population_size; ++index) {
            evo_fitness_candidate_view_t candidate_view = {0};
            evo_fitness_candidate_view_t best_view = {0};
            evo_fitness_order_t order = EVO_FITNESS_ORDER_EQUAL;

            if (!evaluations[index].valid) {
                continue;
            }
            evaluations[index].evaluated = true;
            candidate_view = (evo_fitness_candidate_view_t){
                .fitness = &evaluations[index].fitness,
                .generation = UINT64_C(0),
                .population_index = index,
                .hard_valid = true,
                .evaluated = true,
            };
            if (!evo_fitness_candidate_is_rankable(&candidate_view)) {
                status = EVO_ERROR_EVALUATION;
                break;
            }
            assignments[index].commit_order = schedule.committed_count;
            assignments[index].committed = true;
            ++schedule.committed_count;
            if (!local_has_best) {
                local_best_index = index;
                local_has_best = true;
                continue;
            }
            best_view = (evo_fitness_candidate_view_t){
                .fitness = &evaluations[local_best_index].fitness,
                .generation = UINT64_C(0),
                .population_index = local_best_index,
                .hard_valid = true,
                .evaluated = true,
            };
            if (!evo_fitness_compare_candidates(&candidate_view,
                                                &best_view,
                                                &order)) {
                status = EVO_ERROR_EVALUATION;
                break;
            }
            if (order == EVO_FITNESS_ORDER_LEFT) {
                local_best_index = index;
            }
        }
    }
    if (status == EVO_SUCCESS) {
        schedule.outcome = EVO_EVALUATION_SCHEDULE_COMMITTED;
        *valid_count = local_valid_count;
        *best_index = local_best_index;
        *has_best = local_has_best;
    } else if (schedule.outcome == EVO_EVALUATION_SCHEDULE_NOT_RUN) {
        schedule.outcome = EVO_EVALUATION_SCHEDULE_FITNESS_REJECTED;
    }
    schedule.complete = true;
    notify_schedule(config, &schedule);
    clear_and_release_scratch(scratch, scratch_size);
    return status;
}
