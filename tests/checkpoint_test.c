#include "catalyst/evo/evo.h"

#include <assert.h>
#include <stdint.h>

enum {
    TEST_POPULATION_SIZE = 4,
    TEST_GENOME_SIZE = 8,
    TEST_GENERATION_LIMIT = 4,
    TEST_CHECKPOINT_CAPACITY = 8192,
    TEST_SNAPSHOT_COUNT = TEST_GENERATION_LIMIT + 1,
    TEST_HEADER_CONFIGURATION_OFFSET = 24,
    TEST_HEADER_CONFIGURATION_SIZE = 32,
    TEST_HEADER_STATE_OFFSET = 40,
    TEST_HEADER_CONFIGURATION_FINGERPRINT = 120,
    TEST_HEADER_CHECKSUM = 132,
    TEST_CONFIGURATION_MAX_POPULATION_BYTES = 68,
    TEST_STATE_TERMINATION_REASON = 28,
    TEST_STATE_INITIALIZATION_SEED = 160
};

typedef struct event_log {
    size_t generations[TEST_SNAPSHOT_COUNT];
    evo_termination_reason_t reasons[TEST_SNAPSHOT_COUNT];
    size_t count;
} event_log_t;

typedef struct checkpoint_log {
    unsigned char snapshots[TEST_SNAPSHOT_COUNT]
                           [TEST_CHECKPOINT_CAPACITY];
    size_t sizes[TEST_SNAPSHOT_COUNT];
    size_t count;
} checkpoint_log_t;

static uint32_t read_u32_le(const unsigned char *bytes, size_t offset)
{
    uint32_t value = 0;

    for (size_t index = 0; index < 4; ++index) {
        value |= (uint32_t)bytes[offset + index] << (index * 8);
    }
    return value;
}

static uint64_t read_u64_le(const unsigned char *bytes, size_t offset)
{
    uint64_t value = 0;

    for (size_t index = 0; index < 8; ++index) {
        value |= (uint64_t)bytes[offset + index] << (index * 8);
    }
    return value;
}

static void write_u32_le(unsigned char *bytes,
                         size_t offset,
                         uint32_t value)
{
    for (size_t index = 0; index < 4; ++index) {
        bytes[offset + index] =
            (unsigned char)(value >> (index * 8));
    }
}

static void write_u64_le(unsigned char *bytes,
                         size_t offset,
                         uint64_t value)
{
    for (size_t index = 0; index < 8; ++index) {
        bytes[offset + index] =
            (unsigned char)(value >> (index * 8));
    }
}

static uint64_t test_fingerprint(const unsigned char *bytes, size_t size)
{
    uint64_t fingerprint = UINT64_C(14695981039346656037);

    for (size_t index = 0; index < size; ++index) {
        fingerprint ^= (uint64_t)bytes[index];
        fingerprint *= UINT64_C(1099511628211);
    }
    return fingerprint;
}

static uint32_t test_crc32(const unsigned char *bytes, size_t size)
{
    uint32_t crc = UINT32_MAX;

    for (size_t index = 0; index < size; ++index) {
        const unsigned char byte =
            index >= TEST_HEADER_CHECKSUM &&
                    index < TEST_HEADER_CHECKSUM + 4
                ? 0
                : bytes[index];

        crc ^= (uint32_t)byte;
        for (size_t bit = 0; bit < 8; ++bit) {
            const uint32_t mask = UINT32_C(0) - (crc & UINT32_C(1));
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static void rewrite_integrity(unsigned char *checkpoint, size_t size)
{
    const size_t configuration_offset =
        (size_t)read_u64_le(checkpoint,
                            TEST_HEADER_CONFIGURATION_OFFSET);
    const size_t configuration_size =
        (size_t)read_u64_le(checkpoint,
                            TEST_HEADER_CONFIGURATION_SIZE);

    write_u64_le(checkpoint,
                 TEST_HEADER_CONFIGURATION_FINGERPRINT,
                 test_fingerprint(checkpoint + configuration_offset,
                                  configuration_size));
    write_u32_le(checkpoint, TEST_HEADER_CHECKSUM, 0);
    write_u32_le(checkpoint,
                 TEST_HEADER_CHECKSUM,
                 test_crc32(checkpoint, size));
}

static evo_fitness_t evaluate_genome(const void *genome, void *context)
{
    const unsigned char *bytes = genome;
    double total = 0.0;

    (void)context;
    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        total += (double)bytes[index];
    }
    return (evo_fitness_t){
        .correctness = total,
        .performance = total / 2.0,
        .memory_use = total / 4.0,
        .reliability = total / 8.0,
        .maintainability = total / 16.0,
        .constraint_penalty = 0.0,
        .total = total,
    };
}

static bool validate_genome(const void *genome, void *context)
{
    (void)genome;
    (void)context;
    return true;
}

static evo_fitness_t evaluate_constant(const void *genome, void *context)
{
    (void)genome;
    (void)context;
    return (evo_fitness_t){.correctness = 1.0, .total = 1.0};
}

static void observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *opaque)
{
    event_log_t *log = opaque;

    assert(log->count < TEST_SNAPSHOT_COUNT);
    assert(result->generations_completed == statistics->generation_index);
    log->generations[log->count] = result->generations_completed;
    log->reasons[log->count] = result->termination_reason;
    ++log->count;
}

static void observe_checkpoint(const void *checkpoint,
                               size_t checkpoint_size,
                               const evo_checkpoint_view_t *view,
                               void *opaque)
{
    checkpoint_log_t *log = opaque;
    evo_checkpoint_candidate_view_t candidate = {0};
    const size_t generation = (size_t)view->current_generation;

    assert(log->count < TEST_SNAPSHOT_COUNT);
    assert(generation < TEST_SNAPSHOT_COUNT);
    assert(checkpoint != NULL);
    assert(checkpoint_size <= TEST_CHECKPOINT_CAPACITY);
    assert(view->version == EVO_CHECKPOINT_VIEW_VERSION);
    assert(view->format_version == EVO_CHECKPOINT_FORMAT_VERSION);
    assert(view->integrity_algorithm == EVO_CHECKPOINT_INTEGRITY_CRC32);
    assert(view->rng_algorithm_version != 0);
    assert(view->operator_seed_schedule_version != 0);
    assert(view->bounded_run_policy_version != 0);
    assert(view->selection_policy_version != 0);
    assert(view->byte_operator_policy_version != 0);
    assert(view->configuration.checkpoint_problem_identity ==
           UINT64_C(0x5100510051005100));
    assert(view->configuration.checkpoint_context_identity ==
           UINT64_C(0x5151515151515151));
    assert(view->population_size == TEST_POPULATION_SIZE);
    assert(view->population_evaluation_records == TEST_POPULATION_SIZE);
    assert(evo_checkpoint_candidate_inspect(view, 0, &candidate) ==
           EVO_SUCCESS);
    assert(candidate.version == EVO_CHECKPOINT_CANDIDATE_VIEW_VERSION);
    assert(candidate.population_index == 0);
    assert(candidate.genome_size == TEST_GENOME_SIZE);
    assert(candidate.genome != NULL);
    assert(candidate.valid);
    assert(candidate.evaluated);

    for (size_t index = 0; index < checkpoint_size; ++index) {
        log->snapshots[generation][index] =
            ((const unsigned char *)checkpoint)[index];
    }
    log->sizes[generation] = checkpoint_size;
    ++log->count;
}

static evo_problem_t make_problem(void)
{
    return (evo_problem_t){
        .genome_size = TEST_GENOME_SIZE,
        .evaluate = evaluate_genome,
        .is_valid = validate_genome,
        .checkpoint_problem_identity = UINT64_C(0x5100510051005100),
    };
}

static evo_config_t make_config(event_log_t *events,
                                checkpoint_log_t *checkpoints,
                                unsigned char *checkpoint_buffer,
                                size_t checkpoint_buffer_size)
{
    return (evo_config_t){
        .population_size = TEST_POPULATION_SIZE,
        .generation_limit = TEST_GENERATION_LIMIT,
        .tournament_size = 0,
        .crossover_rate = 0.75,
        .mutation_rate = 0.25,
        .random_seed = UINT64_C(510029),
        .max_genome_bytes = TEST_GENOME_SIZE,
        .max_population_bytes =
            TEST_POPULATION_SIZE * TEST_GENOME_SIZE,
        .max_evaluation_bytes = SIZE_MAX,
        .max_child_population_bytes =
            TEST_POPULATION_SIZE * TEST_GENOME_SIZE,
        .generation_observer = observe_generation,
        .generation_observer_context = events,
        .max_diversity_work = SIZE_MAX,
        .stagnation_enabled = true,
        .improvement_tolerance = 0.0,
        .stagnation_patience = 32,
        .elite_count_enabled = true,
        .elite_count = 1,
        .selection_policy = EVO_SELECTION_RANK,
        .rank_base_weight = 1,
        .rank_step_weight = 2,
        .crossover_operator = EVO_CROSSOVER_BYTE_UNIFORM,
        .mutation_operator = EVO_MUTATION_BYTE_XOR,
        .adaptive_mutation_enabled = true,
        .adaptive_mutation_min_rate = 0.1,
        .adaptive_mutation_max_rate = 0.9,
        .adaptive_mutation_step = 0.1,
        .adaptive_mutation_diversity_threshold = 0.5,
        .adaptive_mutation_reset_on_improvement = true,
        .max_checkpoint_bytes = TEST_CHECKPOINT_CAPACITY,
        .checkpoint_buffer = checkpoint_buffer,
        .checkpoint_buffer_size = checkpoint_buffer_size,
        .checkpoint_observer = observe_checkpoint,
        .checkpoint_observer_context = checkpoints,
        .checkpoint_context_identity = UINT64_C(0x5151515151515151),
    };
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
    assert(left->best_genome != NULL);
    assert(right->best_genome != NULL);
    assert(left->best_genome != right->best_genome);
    assert(left->best_genome_size == right->best_genome_size);
    for (size_t index = 0; index < left->best_genome_size; ++index) {
        assert(((const unsigned char *)left->best_genome)[index] ==
               ((const unsigned char *)right->best_genome)[index]);
    }
    assert_fitness_equal(&left->best_fitness, &right->best_fitness);
    assert(left->generations_completed == right->generations_completed);
    assert(left->random_seed == right->random_seed);
    assert(left->termination_reason == right->termination_reason);
    assert(left->generation_statistics.generation_index ==
           right->generation_statistics.generation_index);
    assert(left->generation_statistics.best_index ==
           right->generation_statistics.best_index);
    assert_fitness_equal(&left->generation_statistics.best_fitness,
                         &right->generation_statistics.best_fitness);
    assert_fitness_equal(&left->generation_statistics.fitness_sums,
                         &right->generation_statistics.fitness_sums);
    assert(left->generation_statistics.diversity ==
           right->generation_statistics.diversity);
    assert(left->generation_statistics.mutation_rate_effective ==
           right->generation_statistics.mutation_rate_effective);
    assert(left->generation_statistics
               .adaptive_mutation_stagnant_generations ==
           right->generation_statistics
               .adaptive_mutation_stagnant_generations);
    assert(left->generation_statistics.mutation_adaptation_reason ==
           right->generation_statistics.mutation_adaptation_reason);
}

static void test_checkpoint_resume_replay(void)
{
    evo_problem_t problem = make_problem();
    event_log_t uninterrupted_events = {0};
    checkpoint_log_t uninterrupted_checkpoints = {0};
    unsigned char uninterrupted_buffer[TEST_CHECKPOINT_CAPACITY] = {0};
    evo_config_t config = make_config(&uninterrupted_events,
                                      &uninterrupted_checkpoints,
                                      uninterrupted_buffer,
                                      sizeof(uninterrupted_buffer));
    evo_result_t uninterrupted = {0};
    size_t checkpoint_size = 0;

    assert(evo_checkpoint_size(&problem, &config, &checkpoint_size) ==
           EVO_SUCCESS);
    assert(checkpoint_size > 0);
    assert(checkpoint_size <= sizeof(uninterrupted_buffer));
    assert(evo_run(&problem, &config, NULL, &uninterrupted) == EVO_SUCCESS);
    assert(uninterrupted.termination_reason ==
           EVO_TERMINATION_GENERATION_LIMIT);
    assert(uninterrupted_events.count == TEST_SNAPSHOT_COUNT);
    assert(uninterrupted_checkpoints.count == TEST_SNAPSHOT_COUNT);

    for (size_t restored_generation = 0;
         restored_generation < TEST_SNAPSHOT_COUNT;
         restored_generation += 2) {
        event_log_t resumed_events = {0};
        checkpoint_log_t resumed_checkpoints = {0};
        unsigned char resumed_buffer[TEST_CHECKPOINT_CAPACITY] = {0};
        evo_config_t resumed_config = make_config(&resumed_events,
                                                  &resumed_checkpoints,
                                                  resumed_buffer,
                                                  sizeof(resumed_buffer));
        evo_result_t resumed = {0};

        assert(uninterrupted_checkpoints.sizes[restored_generation] ==
               checkpoint_size);
        assert(evo_resume(
                   &problem,
                   &resumed_config,
                   NULL,
                   uninterrupted_checkpoints.snapshots[restored_generation],
                   uninterrupted_checkpoints.sizes[restored_generation],
                   &resumed) == EVO_SUCCESS);
        assert_results_equal(&uninterrupted, &resumed);
        assert(resumed_events.count ==
               TEST_GENERATION_LIMIT - restored_generation);
        assert(resumed_checkpoints.count ==
               TEST_GENERATION_LIMIT - restored_generation);
        for (size_t index = 0; index < resumed_events.count; ++index) {
            assert(resumed_events.generations[index] ==
                   restored_generation + index + 1);
        }
        evo_result_destroy(&resumed);
    }
    evo_result_destroy(&uninterrupted);
}

static void test_inspection_and_rejection(void)
{
    evo_problem_t problem = make_problem();
    event_log_t events = {0};
    checkpoint_log_t checkpoints = {0};
    unsigned char checkpoint_buffer[TEST_CHECKPOINT_CAPACITY] = {0};
    unsigned char corrupted[TEST_CHECKPOINT_CAPACITY] = {0};
    evo_config_t config = make_config(&events,
                                      &checkpoints,
                                      checkpoint_buffer,
                                      sizeof(checkpoint_buffer));
    evo_checkpoint_view_t view = {0};
    evo_checkpoint_candidate_view_t candidate = {0};
    evo_result_t result = {0};
    evo_result_t rejected = {0};
    evo_result_t alias_result = {
        .random_seed = UINT64_C(0x5151515151515151),
        .termination_reason = EVO_TERMINATION_STAGNATED,
    };
    unsigned char alias_snapshot[sizeof(alias_result)] = {0};
    const size_t generation = 2;
    size_t configuration_offset = 0;
    size_t state_offset = 0;
    size_t size = 0;

    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    size = checkpoints.sizes[generation];
    assert(evo_checkpoint_inspect(checkpoints.snapshots[generation],
                                  size,
                                  TEST_CHECKPOINT_CAPACITY,
                                  &view) == EVO_SUCCESS);
    assert(view.current_generation == generation);
    assert(view.configuration.population_size == TEST_POPULATION_SIZE);
    assert(view.configuration.generation_limit == TEST_GENERATION_LIMIT);
    assert(view.generation_statistics.generation_index == generation);
    assert(view.global_best_generation <= generation);
    assert(read_u32_le(checkpoints.snapshots[generation],
                       TEST_HEADER_CHECKSUM) ==
           test_crc32(checkpoints.snapshots[generation], size));
    assert(evo_checkpoint_candidate_inspect(&view,
                                            TEST_POPULATION_SIZE,
                                            &candidate) ==
           EVO_ERROR_INVALID_ARGUMENT);
    --view.population_genome_bytes;
    assert(evo_checkpoint_candidate_inspect(&view, 0, &candidate) ==
           EVO_ERROR_CHECKPOINT_INVALID);
    assert(candidate.version == 0);
    assert(evo_checkpoint_inspect(checkpoints.snapshots[generation],
                                  size,
                                  TEST_CHECKPOINT_CAPACITY,
                                  &view) == EVO_SUCCESS);

    configuration_offset =
        (size_t)read_u64_le(checkpoints.snapshots[generation],
                            TEST_HEADER_CONFIGURATION_OFFSET);
    state_offset =
        (size_t)read_u64_le(checkpoints.snapshots[generation],
                            TEST_HEADER_STATE_OFFSET);
    for (size_t index = 0; index < size; ++index) {
        corrupted[index] = checkpoints.snapshots[generation][index];
    }
    write_u64_le(corrupted,
                 configuration_offset +
                     TEST_CONFIGURATION_MAX_POPULATION_BYTES,
                 UINT64_C(1));
    rewrite_integrity(corrupted, size);
    assert(evo_checkpoint_inspect(corrupted,
                                  size,
                                  TEST_CHECKPOINT_CAPACITY,
                                  &view) == EVO_ERROR_CHECKPOINT_INVALID);

    for (size_t index = 0; index < size; ++index) {
        corrupted[index] = checkpoints.snapshots[generation][index];
    }
    write_u32_le(corrupted,
                 state_offset + TEST_STATE_TERMINATION_REASON,
                 (uint32_t)EVO_TERMINATION_CONVERGED);
    rewrite_integrity(corrupted, size);
    assert(evo_checkpoint_inspect(corrupted,
                                  size,
                                  TEST_CHECKPOINT_CAPACITY,
                                  &view) == EVO_ERROR_CHECKPOINT_INVALID);

    for (size_t index = 0; index < size; ++index) {
        corrupted[index] = checkpoints.snapshots[generation][index];
    }
    write_u64_le(corrupted,
                 state_offset + TEST_STATE_INITIALIZATION_SEED,
                 UINT64_C(1));
    rewrite_integrity(corrupted, size);
    assert(evo_checkpoint_inspect(corrupted,
                                  size,
                                  TEST_CHECKPOINT_CAPACITY,
                                  &view) == EVO_ERROR_CHECKPOINT_INVALID);

    for (size_t index = 0; index < size; ++index) {
        corrupted[index] = checkpoints.snapshots[generation][index];
    }
    corrupted[size - 1] ^= UINT8_C(1);
    assert(evo_checkpoint_inspect(corrupted,
                                  size,
                                  TEST_CHECKPOINT_CAPACITY,
                                  &view) == EVO_ERROR_CHECKPOINT_INTEGRITY);
    assert(evo_checkpoint_inspect(checkpoints.snapshots[generation],
                                  size - 1,
                                  TEST_CHECKPOINT_CAPACITY,
                                  &view) == EVO_ERROR_CHECKPOINT_INVALID);
    for (size_t index = 0; index < size; ++index) {
        corrupted[index] = checkpoints.snapshots[generation][index];
    }
    corrupted[8] = UINT8_C(2);
    assert(evo_checkpoint_inspect(corrupted,
                                  size,
                                  TEST_CHECKPOINT_CAPACITY,
                                  &view) == EVO_ERROR_CHECKPOINT_VERSION);

    config.random_seed += UINT64_C(1);
    assert(evo_resume(&problem,
                      &config,
                      NULL,
                      checkpoints.snapshots[generation],
                      size,
                      &rejected) == EVO_ERROR_CHECKPOINT_MISMATCH);
    assert(rejected.best_genome == NULL);
    config.random_seed -= UINT64_C(1);
    config.max_checkpoint_bytes = size - 1;
    assert(evo_resume(&problem,
                      &config,
                      NULL,
                      checkpoints.snapshots[generation],
                      size,
                      &rejected) == EVO_ERROR_RESOURCE_LIMIT);
    assert(rejected.best_genome == NULL);

    config.max_checkpoint_bytes = TEST_CHECKPOINT_CAPACITY;
    for (size_t index = 0; index < sizeof(alias_result); ++index) {
        alias_snapshot[index] =
            ((const unsigned char *)&alias_result)[index];
    }
    assert(evo_resume(&problem,
                      &config,
                      NULL,
                      (const unsigned char *)&alias_result + 1,
                      1,
                      &alias_result) == EVO_ERROR_INVALID_ARGUMENT);
    for (size_t index = 0; index < sizeof(alias_result); ++index) {
        assert(((const unsigned char *)&alias_result)[index] ==
               alias_snapshot[index]);
    }

    evo_result_destroy(&result);
}

static void test_resume_preserves_patience_and_adaptation_state(void)
{
    evo_problem_t problem = make_problem();
    event_log_t uninterrupted_events = {0};
    checkpoint_log_t uninterrupted_checkpoints = {0};
    unsigned char uninterrupted_buffer[TEST_CHECKPOINT_CAPACITY] = {0};
    evo_config_t config = make_config(&uninterrupted_events,
                                      &uninterrupted_checkpoints,
                                      uninterrupted_buffer,
                                      sizeof(uninterrupted_buffer));
    evo_result_t uninterrupted = {0};
    event_log_t resumed_events = {0};
    checkpoint_log_t resumed_checkpoints = {0};
    unsigned char resumed_buffer[TEST_CHECKPOINT_CAPACITY] = {0};
    evo_config_t resumed_config = make_config(&resumed_events,
                                              &resumed_checkpoints,
                                              resumed_buffer,
                                              sizeof(resumed_buffer));
    evo_result_t resumed = {0};

    problem.evaluate = evaluate_constant;
    config.stagnation_patience = 2;
    resumed_config.stagnation_patience = 2;
    assert(evo_run(&problem, &config, NULL, &uninterrupted) == EVO_SUCCESS);
    assert(uninterrupted.termination_reason == EVO_TERMINATION_STAGNATED);
    assert(uninterrupted.generations_completed == 2);
    assert(uninterrupted_checkpoints.count == 3);
    assert(evo_resume(&problem,
                      &resumed_config,
                      NULL,
                      uninterrupted_checkpoints.snapshots[1],
                      uninterrupted_checkpoints.sizes[1],
                      &resumed) == EVO_SUCCESS);
    assert_results_equal(&uninterrupted, &resumed);
    assert(resumed_events.count == 1);
    assert(resumed_events.generations[0] == 2);
    assert(resumed_events.reasons[0] == EVO_TERMINATION_STAGNATED);
    assert(resumed_checkpoints.count == 1);
    assert(resumed.generation_statistics
               .adaptive_mutation_stagnant_generations ==
           uninterrupted.generation_statistics
               .adaptive_mutation_stagnant_generations);
    evo_result_destroy(&resumed);
    evo_result_destroy(&uninterrupted);
}

static void test_checkpoint_preflight_is_callback_free(void)
{
    evo_problem_t problem = make_problem();
    event_log_t events = {0};
    checkpoint_log_t checkpoints = {0};
    unsigned char checkpoint_buffer[TEST_CHECKPOINT_CAPACITY] = {0};
    evo_config_t config = make_config(&events,
                                      &checkpoints,
                                      checkpoint_buffer,
                                      sizeof(checkpoint_buffer));
    evo_result_t result = {0};

    problem.checkpoint_problem_identity = 0;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(events.count == 0);
    assert(checkpoints.count == 0);
    assert(result.best_genome == NULL);

    problem = make_problem();
    config.checkpoint_context_identity = 0;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(events.count == 0);
    assert(checkpoints.count == 0);

    config = make_config(&events,
                         &checkpoints,
                         checkpoint_buffer,
                         sizeof(checkpoint_buffer));
    config.max_checkpoint_bytes = 1;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert(events.count == 0);
    assert(checkpoints.count == 0);

    config = make_config(&events,
                         &checkpoints,
                         checkpoint_buffer,
                         sizeof(checkpoint_buffer));
    config.generation_limit = 0;
    config.selection_policy = EVO_SELECTION_TOURNAMENT;
    config.tournament_size = TEST_POPULATION_SIZE + 1;
    config.rank_base_weight = 0;
    config.rank_step_weight = 0;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert(events.count == 0);
    assert(checkpoints.count == 0);

    config = make_config(&events,
                         &checkpoints,
                         checkpoint_buffer,
                         sizeof(checkpoint_buffer));
    config.checkpoint_buffer = &result;
    config.checkpoint_buffer_size = TEST_CHECKPOINT_CAPACITY;
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(events.count == 0);
    assert(checkpoints.count == 0);
    assert(result.best_genome == NULL);
}

int main(void)
{
    test_checkpoint_resume_replay();
    test_inspection_and_rejection();
    test_resume_preserves_patience_and_adaptation_state();
    test_checkpoint_preflight_is_callback_free();
    return 0;
}
