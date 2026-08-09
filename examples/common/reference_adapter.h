#ifndef EVO_EXAMPLES_REFERENCE_ADAPTER_H
#define EVO_EXAMPLES_REFERENCE_ADAPTER_H

#include <catalyst/evo/evo.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ADAPTER_SCHEMA_VERSION "1.0.0"
#define ADAPTER_GENOME_SIZE ((size_t)4)
#define ADAPTER_POPULATION_SIZE ((size_t)12)
#define ADAPTER_GENERATION_LIMIT ((size_t)6)
#define ADAPTER_TRACE_CAPACITY (ADAPTER_GENERATION_LIMIT + (size_t)1)
#define ADAPTER_CHECKPOINT_CAPACITY ((size_t)16384)
#define ADAPTER_RESUME_GENERATION UINT64_C(2)
#define ADAPTER_MAX_SCHEDULES ADAPTER_TRACE_CAPACITY
#define ADAPTER_MAX_ASSIGNMENTS \
    (ADAPTER_MAX_SCHEDULES * ADAPTER_POPULATION_SIZE)

typedef struct adapter_trace_record {
    uint64_t generation;
    size_t valid_count;
    size_t invalid_count;
    size_t best_index;
    double generation_best_total;
    double global_best_total;
    double diversity;
    size_t diversity_pair_count;
    size_t diversity_work_units;
    unsigned char global_best_genome[ADAPTER_GENOME_SIZE];
} adapter_trace_record_t;

typedef struct adapter_schedule_record {
    uint64_t generation;
    size_t worker_count;
    size_t scratch_bytes;
    size_t validated_count;
    size_t hard_invalid_count;
    size_t scheduled_count;
    size_t completed_count;
    size_t committed_count;
    evo_evaluation_schedule_outcome_t outcome;
    size_t assignment_offset;
    size_t assignment_count;
    bool complete;
} adapter_schedule_record_t;

typedef struct adapter_checkpoint_candidate {
    size_t population_index;
    unsigned char genome[ADAPTER_GENOME_SIZE];
    evo_fitness_t fitness;
    bool valid;
    bool evaluated;
} adapter_checkpoint_candidate_t;

typedef struct adapter_capture {
    adapter_trace_record_t traces[ADAPTER_TRACE_CAPACITY];
    size_t trace_count;
    adapter_schedule_record_t schedules[ADAPTER_MAX_SCHEDULES];
    evo_evaluation_assignment_t assignments[ADAPTER_MAX_ASSIGNMENTS];
    size_t schedule_count;
    size_t assignment_count;
    unsigned char checkpoint_scratch[ADAPTER_CHECKPOINT_CAPACITY];
    unsigned char checkpoint_snapshot[ADAPTER_CHECKPOINT_CAPACITY];
    size_t checkpoint_size;
    uint32_t checkpoint_integrity;
    uint64_t checkpoint_generation;
    adapter_checkpoint_candidate_t
        checkpoint_candidates[ADAPTER_POPULATION_SIZE];
    size_t checkpoint_candidate_count;
    bool checkpoint_captured;
    bool callback_error;
} adapter_capture_t;

typedef struct adapter_definition {
    const char *adapter_id;
    const char *domain;
    const char *fixture_id;
    const char *fixture_json;
    const char *limitation;
    uint64_t random_seed;
    uint64_t problem_identity;
    uint64_t context_identity;
    evo_fitness_t (*evaluate)(const void *genome, void *context);
    bool (*is_valid)(const void *genome, void *context);
    const void *model;
    bool checkpoint_resume;
    size_t evaluation_worker_count;
    uint64_t application_stop_generation;
    size_t stagnation_patience;
} adapter_definition_t;

static bool adapter_fitness_equal(const evo_fitness_t *left,
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

static bool adapter_bytes_equal(const unsigned char *left,
                                const unsigned char *right,
                                size_t size)
{
    for (size_t index = 0; index < size; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static void adapter_observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *opaque)
{
    adapter_capture_t *capture = opaque;
    adapter_trace_record_t *record = NULL;

    if (capture == NULL || result == NULL || statistics == NULL ||
        result->best_genome == NULL ||
        result->best_genome_size != ADAPTER_GENOME_SIZE ||
        capture->trace_count >= ADAPTER_TRACE_CAPACITY) {
        if (capture != NULL) {
            capture->callback_error = true;
        }
        return;
    }

    record = &capture->traces[capture->trace_count];
    *record = (adapter_trace_record_t){
        .generation = statistics->generation_index,
        .valid_count = statistics->valid_count,
        .invalid_count = statistics->invalid_count,
        .best_index = statistics->best_index,
        .generation_best_total = statistics->best_fitness.total,
        .global_best_total = result->best_fitness.total,
        .diversity = statistics->diversity,
        .diversity_pair_count = statistics->diversity_pair_count,
        .diversity_work_units = statistics->diversity_work_units,
    };
    for (size_t index = 0; index < ADAPTER_GENOME_SIZE; ++index) {
        record->global_best_genome[index] =
            ((const unsigned char *)result->best_genome)[index];
    }
    ++capture->trace_count;
}

static void adapter_observe_schedule(
    const evo_evaluation_schedule_t *schedule,
    void *opaque)
{
    adapter_capture_t *capture = opaque;
    adapter_schedule_record_t *record = NULL;

    if (capture == NULL || schedule == NULL ||
        capture->schedule_count >= ADAPTER_MAX_SCHEDULES ||
        schedule->assignment_count > ADAPTER_POPULATION_SIZE ||
        capture->assignment_count >
            ADAPTER_MAX_ASSIGNMENTS - schedule->assignment_count) {
        if (capture != NULL) {
            capture->callback_error = true;
        }
        return;
    }

    record = &capture->schedules[capture->schedule_count];
    *record = (adapter_schedule_record_t){
        .generation = schedule->population_generation,
        .worker_count = schedule->worker_count,
        .scratch_bytes = schedule->scratch_bytes,
        .validated_count = schedule->validated_count,
        .hard_invalid_count = schedule->hard_invalid_count,
        .scheduled_count = schedule->scheduled_count,
        .completed_count = schedule->completed_count,
        .committed_count = schedule->committed_count,
        .outcome = schedule->outcome,
        .assignment_offset = capture->assignment_count,
        .assignment_count = schedule->assignment_count,
        .complete = schedule->complete,
    };
    for (size_t index = 0; index < schedule->assignment_count; ++index) {
        capture->assignments[capture->assignment_count + index] =
            schedule->assignments[index];
    }
    capture->assignment_count += schedule->assignment_count;
    ++capture->schedule_count;
}

static void adapter_observe_checkpoint(const void *checkpoint,
                                       size_t checkpoint_size,
                                       const evo_checkpoint_view_t *view,
                                       void *opaque)
{
    adapter_capture_t *capture = opaque;

    if (capture == NULL || checkpoint == NULL || view == NULL) {
        if (capture != NULL) {
            capture->callback_error = true;
        }
        return;
    }
    if (view->current_generation != ADAPTER_RESUME_GENERATION ||
        capture->checkpoint_captured) {
        return;
    }
    if (checkpoint_size > ADAPTER_CHECKPOINT_CAPACITY ||
        view->population_size != ADAPTER_POPULATION_SIZE) {
        capture->callback_error = true;
        return;
    }

    for (size_t index = 0; index < checkpoint_size; ++index) {
        capture->checkpoint_snapshot[index] =
            ((const unsigned char *)checkpoint)[index];
    }
    capture->checkpoint_size = checkpoint_size;
    capture->checkpoint_integrity = view->integrity_value;
    capture->checkpoint_generation = view->current_generation;
    capture->checkpoint_candidate_count = view->population_size;

    for (size_t index = 0; index < view->population_size; ++index) {
        evo_checkpoint_candidate_view_t candidate = {0};
        adapter_checkpoint_candidate_t *destination =
            &capture->checkpoint_candidates[index];

        if (evo_checkpoint_candidate_inspect(view, index, &candidate) !=
                EVO_SUCCESS ||
            candidate.genome == NULL ||
            candidate.genome_size != ADAPTER_GENOME_SIZE) {
            capture->callback_error = true;
            return;
        }
        *destination = (adapter_checkpoint_candidate_t){
            .population_index = candidate.population_index,
            .fitness = candidate.fitness,
            .valid = candidate.valid,
            .evaluated = candidate.evaluated,
        };
        for (size_t byte = 0; byte < ADAPTER_GENOME_SIZE; ++byte) {
            destination->genome[byte] =
                ((const unsigned char *)candidate.genome)[byte];
        }
    }
    capture->checkpoint_captured = true;
}

static bool adapter_request_stop(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *opaque)
{
    const adapter_definition_t *definition = opaque;

    if (definition == NULL || result == NULL || statistics == NULL) {
        return false;
    }
    return definition->application_stop_generation != UINT64_C(0) &&
           statistics->generation_index >=
               definition->application_stop_generation;
}

static evo_problem_t adapter_problem(const adapter_definition_t *definition)
{
    return (evo_problem_t){
        .genome_size = ADAPTER_GENOME_SIZE,
        .evaluate = definition->evaluate,
        .is_valid = definition->is_valid,
        .checkpoint_problem_identity = definition->problem_identity,
        .evaluation_callback_thread_safety =
            EVO_EVALUATION_CALLBACK_THREAD_SAFE,
    };
}

static evo_status_t adapter_config(const adapter_definition_t *definition,
                                   adapter_capture_t *capture,
                                   evo_config_t *config,
                                   size_t *worker_scratch_size)
{
    evo_status_t status = EVO_SUCCESS;

    *worker_scratch_size = 0;
    if (definition->evaluation_worker_count != 0) {
        status = evo_evaluation_worker_scratch_size(
            ADAPTER_POPULATION_SIZE,
            definition->evaluation_worker_count,
            worker_scratch_size);
        if (status != EVO_SUCCESS) {
            return status;
        }
    }

    *config = (evo_config_t){
        .population_size = ADAPTER_POPULATION_SIZE,
        .generation_limit = ADAPTER_GENERATION_LIMIT,
        .tournament_size = 0,
        .crossover_rate = 0.75,
        .mutation_rate = 0.35,
        .random_seed = definition->random_seed,
        .max_genome_bytes = ADAPTER_GENOME_SIZE,
        .max_population_bytes =
            ADAPTER_POPULATION_SIZE * ADAPTER_GENOME_SIZE,
        .max_evaluation_bytes = 4096,
        .max_child_population_bytes =
            ADAPTER_POPULATION_SIZE * ADAPTER_GENOME_SIZE,
        .generation_observer = adapter_observe_generation,
        .generation_observer_context = capture,
        .max_diversity_work = 264,
        .stagnation_enabled = definition->stagnation_patience != 0,
        .improvement_tolerance = 0.0,
        .stagnation_patience = definition->stagnation_patience,
        .elite_count_enabled = true,
        .elite_count = 1,
        .selection_policy = EVO_SELECTION_RANK,
        .rank_base_weight = 1,
        .rank_step_weight = 1,
        .crossover_operator = EVO_CROSSOVER_BYTE_UNIFORM,
        .mutation_operator = EVO_MUTATION_BYTE_XOR,
        .population_recycling_enabled = true,
        .evaluation_worker_count = definition->evaluation_worker_count,
        .max_evaluation_worker_scratch_bytes = *worker_scratch_size,
    };

    if (definition->application_stop_generation != UINT64_C(0)) {
        config->generation_stop = adapter_request_stop;
        config->generation_stop_context = (void *)definition;
    }
    if (definition->checkpoint_resume) {
        config->max_checkpoint_bytes = ADAPTER_CHECKPOINT_CAPACITY;
        config->checkpoint_buffer = capture->checkpoint_scratch;
        config->checkpoint_buffer_size = ADAPTER_CHECKPOINT_CAPACITY;
        config->checkpoint_observer = adapter_observe_checkpoint;
        config->checkpoint_observer_context = capture;
        config->checkpoint_context_identity = definition->context_identity;
    }
    if (definition->evaluation_worker_count != 0) {
        config->evaluation_schedule_observer = adapter_observe_schedule;
        config->evaluation_schedule_observer_context = capture;
    }
    return EVO_SUCCESS;
}

static evo_status_t adapter_execute(const adapter_definition_t *definition,
                                    adapter_capture_t *capture,
                                    evo_result_t *result,
                                    size_t *worker_scratch_size,
                                    size_t *checkpoint_size)
{
    const evo_problem_t problem = adapter_problem(definition);
    evo_config_t config = {0};
    evo_status_t status = adapter_config(
        definition, capture, &config, worker_scratch_size);

    *checkpoint_size = 0;
    if (status != EVO_SUCCESS) {
        return status;
    }
    if (definition->checkpoint_resume) {
        status = evo_checkpoint_size(&problem, &config, checkpoint_size);
        if (status != EVO_SUCCESS ||
            *checkpoint_size > ADAPTER_CHECKPOINT_CAPACITY) {
            return status == EVO_SUCCESS ? EVO_ERROR_RESOURCE_LIMIT : status;
        }
    }
    return evo_run(&problem,
                   &config,
                   (void *)definition->model,
                   result);
}

static evo_status_t adapter_resume(const adapter_definition_t *definition,
                                   const adapter_capture_t *source,
                                   adapter_capture_t *capture,
                                   evo_result_t *result)
{
    const evo_problem_t problem = adapter_problem(definition);
    evo_config_t config = {0};
    size_t worker_scratch_size = 0;
    evo_status_t status = adapter_config(
        definition, capture, &config, &worker_scratch_size);

    if (status != EVO_SUCCESS || !source->checkpoint_captured) {
        return status == EVO_SUCCESS ? EVO_ERROR_STATE : status;
    }
    return evo_resume(&problem,
                      &config,
                      (void *)definition->model,
                      source->checkpoint_snapshot,
                      source->checkpoint_size,
                      result);
}

static bool adapter_result_equal(const evo_result_t *left,
                                 const evo_result_t *right)
{
    if (left->best_genome == NULL || right->best_genome == NULL ||
        left->best_genome_size != ADAPTER_GENOME_SIZE ||
        right->best_genome_size != ADAPTER_GENOME_SIZE) {
        return false;
    }
    return adapter_bytes_equal(left->best_genome,
                               right->best_genome,
                               ADAPTER_GENOME_SIZE) &&
           adapter_fitness_equal(&left->best_fitness,
                                 &right->best_fitness) &&
           left->generations_completed == right->generations_completed &&
           left->random_seed == right->random_seed &&
           left->termination_reason == right->termination_reason &&
           left->best_genome_size == right->best_genome_size &&
           left->secure_erasure_policy_version ==
               right->secure_erasure_policy_version &&
           left->secure_erasure_backend == right->secure_erasure_backend &&
           left->secure_erasure_enabled == right->secure_erasure_enabled &&
           left->generation_statistics.generation_index ==
               right->generation_statistics.generation_index &&
           left->generation_statistics.valid_count ==
               right->generation_statistics.valid_count &&
           left->generation_statistics.invalid_count ==
               right->generation_statistics.invalid_count &&
           left->generation_statistics.best_index ==
               right->generation_statistics.best_index &&
           left->generation_statistics.diversity ==
               right->generation_statistics.diversity &&
           adapter_fitness_equal(
               &left->generation_statistics.best_fitness,
               &right->generation_statistics.best_fitness) &&
           adapter_fitness_equal(
               &left->generation_statistics.fitness_sums,
               &right->generation_statistics.fitness_sums);
}

static bool adapter_trace_equal(const adapter_capture_t *left,
                                const adapter_capture_t *right)
{
    if (left->trace_count != right->trace_count) {
        return false;
    }
    for (size_t index = 0; index < left->trace_count; ++index) {
        const adapter_trace_record_t *a = &left->traces[index];
        const adapter_trace_record_t *b = &right->traces[index];

        if (a->generation != b->generation ||
            a->valid_count != b->valid_count ||
            a->invalid_count != b->invalid_count ||
            a->best_index != b->best_index ||
            a->generation_best_total != b->generation_best_total ||
            a->global_best_total != b->global_best_total ||
            a->diversity != b->diversity ||
            a->diversity_pair_count != b->diversity_pair_count ||
            a->diversity_work_units != b->diversity_work_units ||
            !adapter_bytes_equal(a->global_best_genome,
                                 b->global_best_genome,
                                 ADAPTER_GENOME_SIZE)) {
            return false;
        }
    }
    return true;
}

static bool adapter_schedule_equal(const adapter_capture_t *left,
                                   const adapter_capture_t *right)
{
    if (left->schedule_count != right->schedule_count ||
        left->assignment_count != right->assignment_count) {
        return false;
    }
    for (size_t index = 0; index < left->schedule_count; ++index) {
        const adapter_schedule_record_t *a = &left->schedules[index];
        const adapter_schedule_record_t *b = &right->schedules[index];

        if (a->generation != b->generation ||
            a->worker_count != b->worker_count ||
            a->scratch_bytes != b->scratch_bytes ||
            a->validated_count != b->validated_count ||
            a->hard_invalid_count != b->hard_invalid_count ||
            a->scheduled_count != b->scheduled_count ||
            a->completed_count != b->completed_count ||
            a->committed_count != b->committed_count ||
            a->outcome != b->outcome ||
            a->assignment_offset != b->assignment_offset ||
            a->assignment_count != b->assignment_count ||
            a->complete != b->complete) {
            return false;
        }
    }
    for (size_t index = 0; index < left->assignment_count; ++index) {
        const evo_evaluation_assignment_t *a = &left->assignments[index];
        const evo_evaluation_assignment_t *b = &right->assignments[index];

        if (a->population_index != b->population_index ||
            a->worker_identity != b->worker_identity ||
            a->dispatch_wave != b->dispatch_wave ||
            a->commit_order != b->commit_order ||
            a->disposition != b->disposition ||
            a->committed != b->committed) {
            return false;
        }
    }
    return true;
}

static bool adapter_checkpoint_equal(const adapter_capture_t *left,
                                     const adapter_capture_t *right)
{
    if (left->checkpoint_captured != right->checkpoint_captured) {
        return false;
    }
    if (!left->checkpoint_captured) {
        return true;
    }
    if (left->checkpoint_size != right->checkpoint_size ||
        left->checkpoint_integrity != right->checkpoint_integrity ||
        left->checkpoint_generation != right->checkpoint_generation ||
        left->checkpoint_candidate_count !=
            right->checkpoint_candidate_count ||
        !adapter_bytes_equal(left->checkpoint_snapshot,
                             right->checkpoint_snapshot,
                             left->checkpoint_size)) {
        return false;
    }
    for (size_t index = 0;
         index < left->checkpoint_candidate_count;
         ++index) {
        const adapter_checkpoint_candidate_t *a =
            &left->checkpoint_candidates[index];
        const adapter_checkpoint_candidate_t *b =
            &right->checkpoint_candidates[index];

        if (a->population_index != b->population_index ||
            a->valid != b->valid || a->evaluated != b->evaluated ||
            !adapter_fitness_equal(&a->fitness, &b->fitness) ||
            !adapter_bytes_equal(a->genome,
                                 b->genome,
                                 ADAPTER_GENOME_SIZE)) {
            return false;
        }
    }
    return true;
}

static bool adapter_capture_equal(const adapter_capture_t *left,
                                  const adapter_capture_t *right)
{
    return !left->callback_error && !right->callback_error &&
           adapter_trace_equal(left, right) &&
           adapter_schedule_equal(left, right) &&
           adapter_checkpoint_equal(left, right);
}

static bool adapter_resume_suffix_equal(const adapter_capture_t *full,
                                        const adapter_capture_t *resumed)
{
    size_t full_index = 0;

    while (full_index < full->trace_count &&
           full->traces[full_index].generation <=
               ADAPTER_RESUME_GENERATION) {
        ++full_index;
    }
    if (full->trace_count - full_index != resumed->trace_count) {
        return false;
    }
    for (size_t resumed_index = 0;
         resumed_index < resumed->trace_count;
         ++resumed_index) {
        const adapter_trace_record_t *a =
            &full->traces[full_index + resumed_index];
        const adapter_trace_record_t *b =
            &resumed->traces[resumed_index];

        if (a->generation != b->generation ||
            a->valid_count != b->valid_count ||
            a->invalid_count != b->invalid_count ||
            a->best_index != b->best_index ||
            a->generation_best_total != b->generation_best_total ||
            a->global_best_total != b->global_best_total ||
            a->diversity != b->diversity ||
            !adapter_bytes_equal(a->global_best_genome,
                                 b->global_best_genome,
                                 ADAPTER_GENOME_SIZE)) {
            return false;
        }
    }
    return true;
}

static const char *adapter_termination_name(
    evo_termination_reason_t reason)
{
    switch (reason) {
    case EVO_TERMINATION_GENERATION_LIMIT:
        return "generation-limit";
    case EVO_TERMINATION_ALL_INVALID:
        return "all-invalid";
    case EVO_TERMINATION_APPLICATION_REQUESTED:
        return "application-requested";
    case EVO_TERMINATION_CONVERGED:
        return "converged";
    case EVO_TERMINATION_STAGNATED:
        return "stagnated";
    case EVO_TERMINATION_NONE:
    default:
        return "none";
    }
}

static const char *adapter_schedule_outcome_name(
    evo_evaluation_schedule_outcome_t outcome)
{
    switch (outcome) {
    case EVO_EVALUATION_SCHEDULE_COMMITTED:
        return "committed";
    case EVO_EVALUATION_SCHEDULE_FITNESS_REJECTED:
        return "fitness-rejected";
    case EVO_EVALUATION_SCHEDULE_WORKER_START_FAILED:
        return "worker-start-failed";
    case EVO_EVALUATION_SCHEDULE_WORKER_JOIN_FAILED:
        return "worker-join-failed";
    case EVO_EVALUATION_SCHEDULE_NOT_RUN:
    default:
        return "not-run";
    }
}

static const char *adapter_disposition_name(
    evo_evaluation_assignment_disposition_t disposition)
{
    switch (disposition) {
    case EVO_EVALUATION_EXCLUDED:
        return "excluded";
    case EVO_EVALUATION_PENDING:
        return "pending";
    case EVO_EVALUATION_COMPLETED:
        return "completed";
    case EVO_EVALUATION_FAILED:
        return "failed";
    case EVO_EVALUATION_CANCELED:
        return "canceled";
    case EVO_EVALUATION_NOT_VALIDATED:
    default:
        return "not-validated";
    }
}

static void adapter_print_genome(const unsigned char *genome)
{
    for (size_t index = 0; index < ADAPTER_GENOME_SIZE; ++index) {
        (void)printf("%02x", (unsigned int)genome[index]);
    }
}

static void adapter_print_fitness(const evo_fitness_t *fitness)
{
    (void)printf(
        "{\"correctness\":%.17g,\"performance\":%.17g,"
        "\"memory_use\":%.17g,\"reliability\":%.17g,"
        "\"maintainability\":%.17g,\"constraint_penalty\":%.17g,"
        "\"total\":%.17g}",
        fitness->correctness,
        fitness->performance,
        fitness->memory_use,
        fitness->reliability,
        fitness->maintainability,
        fitness->constraint_penalty,
        fitness->total);
}

static void adapter_print_trace(const adapter_capture_t *capture)
{
    (void)fputs("[", stdout);
    for (size_t index = 0; index < capture->trace_count; ++index) {
        const adapter_trace_record_t *record = &capture->traces[index];

        if (index != 0) {
            (void)fputs(",", stdout);
        }
        (void)printf(
            "{\"generation\":%" PRIu64
            ",\"valid_count\":%zu,\"invalid_count\":%zu,"
            "\"best_index\":%zu,\"generation_best_total\":%.17g,"
            "\"global_best_total\":%.17g,\"diversity\":%.17g,"
            "\"diversity_pair_count\":%zu,"
            "\"diversity_work_units\":%zu,\"global_best_genome\":\"",
            record->generation,
            record->valid_count,
            record->invalid_count,
            record->best_index,
            record->generation_best_total,
            record->global_best_total,
            record->diversity,
            record->diversity_pair_count,
            record->diversity_work_units);
        adapter_print_genome(record->global_best_genome);
        (void)fputs("\"}", stdout);
    }
    (void)fputs("]", stdout);
}

static void adapter_print_schedules(const adapter_capture_t *capture)
{
    (void)fputs("[", stdout);
    for (size_t index = 0; index < capture->schedule_count; ++index) {
        const adapter_schedule_record_t *record =
            &capture->schedules[index];

        if (index != 0) {
            (void)fputs(",", stdout);
        }
        (void)printf(
            "{\"generation\":%" PRIu64
            ",\"worker_count\":%zu,\"scratch_bytes\":%zu,"
            "\"validated_count\":%zu,\"hard_invalid_count\":%zu,"
            "\"scheduled_count\":%zu,\"completed_count\":%zu,"
            "\"committed_count\":%zu,\"outcome\":\"%s\","
            "\"complete\":%s,\"assignments\":[",
            record->generation,
            record->worker_count,
            record->scratch_bytes,
            record->validated_count,
            record->hard_invalid_count,
            record->scheduled_count,
            record->completed_count,
            record->committed_count,
            adapter_schedule_outcome_name(record->outcome),
            record->complete ? "true" : "false");
        for (size_t assignment = 0;
             assignment < record->assignment_count;
             ++assignment) {
            const evo_evaluation_assignment_t *view =
                &capture->assignments[record->assignment_offset +
                                      assignment];

            if (assignment != 0) {
                (void)fputs(",", stdout);
            }
            (void)printf(
                "{\"population_index\":%zu,\"worker_identity\":%zu,"
                "\"dispatch_wave\":%zu,\"commit_order\":%zu,"
                "\"disposition\":\"%s\",\"committed\":%s}",
                view->population_index,
                view->worker_identity,
                view->dispatch_wave,
                view->commit_order,
                adapter_disposition_name(view->disposition),
                view->committed ? "true" : "false");
        }
        (void)fputs("]}", stdout);
    }
    (void)fputs("]", stdout);
}

static void adapter_print_checkpoint(const adapter_capture_t *capture,
                                     bool result_equal,
                                     bool suffix_equal)
{
    (void)printf(
        "{\"format_version\":%" PRIu32
        ",\"captured_generation\":%" PRIu64
        ",\"serialized_size\":%zu,\"integrity_crc32\":%" PRIu32
        ",\"candidate_projection_complete\":true,"
        "\"resumed_result_equal\":%s,\"resumed_trace_suffix_equal\":%s,"
        "\"candidates\":[",
        EVO_CHECKPOINT_FORMAT_VERSION,
        capture->checkpoint_generation,
        capture->checkpoint_size,
        capture->checkpoint_integrity,
        result_equal ? "true" : "false",
        suffix_equal ? "true" : "false");
    for (size_t index = 0;
         index < capture->checkpoint_candidate_count;
         ++index) {
        const adapter_checkpoint_candidate_t *candidate =
            &capture->checkpoint_candidates[index];

        if (index != 0) {
            (void)fputs(",", stdout);
        }
        (void)printf(
            "{\"population_index\":%zu,\"genome\":\"",
            candidate->population_index);
        adapter_print_genome(candidate->genome);
        (void)fputs("\",\"fitness\":", stdout);
        adapter_print_fitness(&candidate->fitness);
        (void)printf(
            ",\"valid\":%s,\"evaluated\":%s}",
            candidate->valid ? "true" : "false",
            candidate->evaluated ? "true" : "false");
    }
    (void)fputs("]}", stdout);
}

static void adapter_emit_json(const adapter_definition_t *definition,
                              const adapter_capture_t *capture,
                              const evo_result_t *result,
                              size_t worker_scratch_size,
                              size_t checkpoint_size,
                              bool replay_equal,
                              bool resumed_result_equal,
                              bool resumed_suffix_equal)
{
    (void)printf(
        "{\"schema_version\":\"%s\",\"adapter_id\":\"%s\","
        "\"domain\":\"%s\",\"fixture_id\":\"%s\","
        "\"evo_version\":\"%u.%u.%u\",\"fixture\":%s,"
        "\"configuration\":{\"genome_size\":%zu,"
        "\"population_size\":%zu,\"generation_limit\":%zu,"
        "\"random_seed\":%" PRIu64
        ",\"selection\":\"stable-linear-rank-v1\","
        "\"rank_base_weight\":1,\"rank_step_weight\":1,"
        "\"crossover\":\"uniform-byte-v1\","
        "\"crossover_rate\":0.75,\"mutation\":\"nonzero-xor-byte-v1\","
        "\"mutation_rate\":0.35,\"elite_count\":1,"
        "\"population_recycling\":true,\"max_genome_bytes\":4,"
        "\"max_population_bytes\":48,\"max_evaluation_bytes\":4096,"
        "\"max_child_population_bytes\":48,"
        "\"max_diversity_work\":264,\"max_checkpoint_bytes\":%zu,"
        "\"checkpoint_required_bytes\":%zu,"
        "\"checkpoint_problem_identity\":\"%016" PRIx64 "\","
        "\"checkpoint_context_identity\":\"%016" PRIx64 "\","
        "\"fitness_target_enabled\":false,"
        "\"diversity_floor_enabled\":false,"
        "\"stagnation_patience\":%zu,"
        "\"application_stop_generation\":%" PRIu64 ","
        "\"evaluation_callback_thread_safety\":\"thread-safe\","
        "\"evaluation_worker_count\":%zu,"
        "\"evaluation_worker_scratch_bytes\":%zu},"
        "\"capabilities\":{\"deterministic_replay\":true,"
        "\"hard_constraints\":true,\"soft_penalties\":true,"
        "\"checkpoint_resume\":%s,\"bounded_parallel_evaluation\":%s},"
        "\"authority\":{\"accelerated_structure\":null,"
        "\"reference_form\":\"explicit fixed fixture and ordered core evidence\","
        "\"projection_complete\":true,\"probabilistic_authority\":false,"
        "\"source_optimizer_claimed\":false,\"limitation\":\"%s\"},"
        "\"result\":{\"best_genome\":\"",
        ADAPTER_SCHEMA_VERSION,
        definition->adapter_id,
        definition->domain,
        definition->fixture_id,
        EVO_VERSION_MAJOR,
        EVO_VERSION_MINOR,
        EVO_VERSION_PATCH,
        definition->fixture_json,
        ADAPTER_GENOME_SIZE,
        ADAPTER_POPULATION_SIZE,
        ADAPTER_GENERATION_LIMIT,
        definition->random_seed,
        definition->checkpoint_resume ? ADAPTER_CHECKPOINT_CAPACITY : (size_t)0,
        definition->checkpoint_resume ? checkpoint_size : (size_t)0,
        definition->problem_identity,
        definition->context_identity,
        definition->stagnation_patience,
        definition->application_stop_generation,
        definition->evaluation_worker_count,
        worker_scratch_size,
        definition->checkpoint_resume ? "true" : "false",
        definition->evaluation_worker_count != 0 ? "true" : "false",
        definition->limitation);
    adapter_print_genome(result->best_genome);
    (void)fputs("\",\"best_fitness\":", stdout);
    adapter_print_fitness(&result->best_fitness);
    (void)printf(
        ",\"generations_completed\":%zu,\"random_seed\":%" PRIu64
        ",\"termination_reason\":\"%s\"},\"trace\":",
        result->generations_completed,
        result->random_seed,
        adapter_termination_name(result->termination_reason));
    adapter_print_trace(capture);
    (void)printf(
        ",\"replay\":{\"run_count\":2,\"exact\":%s}",
        replay_equal ? "true" : "false");
    if (definition->checkpoint_resume) {
        (void)fputs(",\"checkpoint_resume\":", stdout);
        adapter_print_checkpoint(
            capture, resumed_result_equal, resumed_suffix_equal);
    }
    if (definition->evaluation_worker_count != 0) {
        (void)fputs(",\"evaluation_schedules\":", stdout);
        adapter_print_schedules(capture);
    }
    (void)fputs("}\n", stdout);
}

static int adapter_reference_main(const adapter_definition_t *definition)
{
    adapter_capture_t first_capture = {0};
    adapter_capture_t second_capture = {0};
    adapter_capture_t resumed_capture = {0};
    evo_result_t first_result = {0};
    evo_result_t second_result = {0};
    evo_result_t resumed_result = {0};
    size_t first_worker_scratch = 0;
    size_t second_worker_scratch = 0;
    size_t first_checkpoint_size = 0;
    size_t second_checkpoint_size = 0;
    evo_status_t status = EVO_SUCCESS;
    bool replay_equal = false;
    bool resumed_result_equal = false;
    bool resumed_suffix_equal = false;
    int exit_code = 1;

    if (definition == NULL || definition->adapter_id == NULL ||
        definition->domain == NULL || definition->fixture_id == NULL ||
        definition->fixture_json == NULL || definition->limitation == NULL ||
        definition->evaluate == NULL || definition->is_valid == NULL ||
        definition->model == NULL) {
        return 1;
    }

    status = adapter_execute(definition,
                             &first_capture,
                             &first_result,
                             &first_worker_scratch,
                             &first_checkpoint_size);
    if (status != EVO_SUCCESS || first_capture.callback_error) {
        goto cleanup;
    }
    status = adapter_execute(definition,
                             &second_capture,
                             &second_result,
                             &second_worker_scratch,
                             &second_checkpoint_size);
    if (status != EVO_SUCCESS || second_capture.callback_error) {
        goto cleanup;
    }

    replay_equal =
        first_worker_scratch == second_worker_scratch &&
        first_checkpoint_size == second_checkpoint_size &&
        adapter_result_equal(&first_result, &second_result) &&
        adapter_capture_equal(&first_capture, &second_capture);
    if (!replay_equal) {
        goto cleanup;
    }

    if (definition->checkpoint_resume) {
        status = adapter_resume(definition,
                                &first_capture,
                                &resumed_capture,
                                &resumed_result);
        if (status != EVO_SUCCESS || resumed_capture.callback_error) {
            goto cleanup;
        }
        resumed_result_equal =
            adapter_result_equal(&first_result, &resumed_result);
        resumed_suffix_equal =
            adapter_resume_suffix_equal(&first_capture, &resumed_capture);
        if (!resumed_result_equal || !resumed_suffix_equal) {
            goto cleanup;
        }
    }

    adapter_emit_json(definition,
                      &first_capture,
                      &first_result,
                      first_worker_scratch,
                      first_checkpoint_size,
                      replay_equal,
                      resumed_result_equal,
                      resumed_suffix_equal);
    if (ferror(stdout) == 0) {
        exit_code = 0;
    }

cleanup:
    evo_result_destroy(&resumed_result);
    evo_result_destroy(&second_result);
    evo_result_destroy(&first_result);
    return exit_code;
}

#endif
