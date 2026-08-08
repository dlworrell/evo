#include "catalyst/evo/evo.h"
#include "internal/population_storage.h"

#include <assert.h>
#include <stdint.h>

enum {
    TEST_POPULATION_SIZE = 6,
    TEST_GENOME_SIZE = 5,
    TEST_GENERATION_LIMIT = 5,
    TEST_GENERATION_EVENTS = TEST_GENERATION_LIMIT + 1,
    TEST_CALLBACK_EVENTS = 192
};

typedef enum callback_kind {
    CALLBACK_INITIALIZE = 1,
    CALLBACK_VALIDITY = 2,
    CALLBACK_EVALUATE = 3,
    CALLBACK_CROSSOVER = 4,
    CALLBACK_MUTATION = 5
} callback_kind_t;

typedef struct callback_event {
    callback_kind_t kind;
    unsigned char first[TEST_GENOME_SIZE];
    unsigned char second[TEST_GENOME_SIZE];
    double mutation_rate;
} callback_event_t;

typedef struct callback_log {
    callback_event_t events[TEST_CALLBACK_EVENTS];
    size_t count;
} callback_log_t;

typedef struct generation_event {
    size_t generations_completed;
    evo_termination_reason_t termination_reason;
    evo_fitness_t best_fitness;
    unsigned char best_genome[TEST_GENOME_SIZE];
    evo_generation_statistics_t statistics;
} generation_event_t;

typedef struct run_capture {
    generation_event_t generations[TEST_GENERATION_EVENTS];
    evo_population_storage_registry_t registries[TEST_GENERATION_EVENTS];
    size_t generation_count;
    size_t registry_count;
} run_capture_t;

typedef struct delivery_log {
    char events[3];
    size_t count;
} delivery_log_t;

static callback_event_t *begin_callback(callback_log_t *log,
                                        callback_kind_t kind,
                                        const void *first)
{
    callback_event_t *event = NULL;
    const unsigned char *bytes = first;

    assert(log != NULL);
    assert(first != NULL);
    assert(log->count < TEST_CALLBACK_EVENTS);
    event = &log->events[log->count];
    *event = (callback_event_t){.kind = kind};
    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        event->first[index] = bytes[index];
    }
    ++log->count;
    return event;
}

static void initialize_genome(void *genome, void *context)
{
    callback_log_t *log = context;
    callback_event_t *event = begin_callback(log,
                                             CALLBACK_INITIALIZE,
                                             genome);
    unsigned char *bytes = genome;

    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        bytes[index] ^= (unsigned char)(UINT8_C(0x31) + (uint8_t)index);
        event->second[index] = bytes[index];
    }
}

static bool validate_genome(const void *genome, void *context)
{
    callback_log_t *log = context;

    (void)begin_callback(log, CALLBACK_VALIDITY, genome);
    return true;
}

static evo_fitness_t evaluate_genome(const void *genome, void *context)
{
    callback_log_t *log = context;
    const unsigned char *bytes = genome;
    double total = 0.0;

    (void)begin_callback(log, CALLBACK_EVALUATE, genome);
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

static void crossover_genomes(const void *parent_a,
                              const void *parent_b,
                              void *child_a,
                              void *child_b,
                              void *context)
{
    callback_log_t *log = context;
    callback_event_t *event = begin_callback(log,
                                             CALLBACK_CROSSOVER,
                                             parent_a);
    const unsigned char *a = parent_a;
    const unsigned char *b = parent_b;
    unsigned char *first_child = child_a;
    unsigned char *second_child = child_b;

    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        event->second[index] = b[index];
        first_child[index] =
            (unsigned char)(a[index] ^ b[(index + 1) % TEST_GENOME_SIZE]);
        second_child[index] =
            (unsigned char)(b[index] ^ a[(index + 2) % TEST_GENOME_SIZE]);
    }
}

static void mutate_genome(void *genome,
                          double mutation_rate,
                          void *context)
{
    callback_log_t *log = context;
    callback_event_t *event = begin_callback(log,
                                             CALLBACK_MUTATION,
                                             genome);
    unsigned char *bytes = genome;

    event->mutation_rate = mutation_rate;
    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        bytes[index] ^= (unsigned char)(UINT8_C(0xa5) + (uint8_t)index);
        event->second[index] = bytes[index];
    }
}

static void observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    run_capture_t *capture = context;
    generation_event_t *event = NULL;
    const unsigned char *best = result->best_genome;

    assert(capture->generation_count < TEST_GENERATION_EVENTS);
    assert(result->version == EVO_GENERATION_RESULT_VIEW_VERSION);
    assert(statistics->version == EVO_GENERATION_STATISTICS_VERSION);
    event = &capture->generations[capture->generation_count];
    *event = (generation_event_t){
        .generations_completed = result->generations_completed,
        .termination_reason = result->termination_reason,
        .best_fitness = result->best_fitness,
        .statistics = *statistics,
    };
    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        event->best_genome[index] = best[index];
    }
    ++capture->generation_count;
}

static void observe_storage(
    const evo_population_storage_registry_t *registry,
    void *context)
{
    run_capture_t *capture = context;

    assert(capture->registry_count < TEST_GENERATION_EVENTS);
    capture->registries[capture->registry_count] = *registry;
    ++capture->registry_count;
}

static evo_problem_t make_problem(void)
{
    return (evo_problem_t){
        .genome_size = TEST_GENOME_SIZE,
        .initialize = initialize_genome,
        .mutate = mutate_genome,
        .crossover = crossover_genomes,
        .evaluate = evaluate_genome,
        .is_valid = validate_genome,
    };
}

static evo_config_t make_config(run_capture_t *capture, bool recycling)
{
    return (evo_config_t){
        .population_size = TEST_POPULATION_SIZE,
        .generation_limit = TEST_GENERATION_LIMIT,
        .tournament_size = 3,
        .crossover_rate = 0.75,
        .mutation_rate = 0.5,
        .random_seed = UINT64_C(0x52c0ffee12345678),
        .max_genome_bytes = TEST_GENOME_SIZE,
        .max_population_bytes =
            TEST_POPULATION_SIZE * TEST_GENOME_SIZE,
        .max_evaluation_bytes =
            TEST_POPULATION_SIZE * sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes =
            TEST_POPULATION_SIZE * TEST_GENOME_SIZE,
        .generation_observer = observe_generation,
        .generation_observer_context = capture,
        .max_diversity_work = SIZE_MAX,
        .elite_count_enabled = true,
        .elite_count = 1,
        .population_recycling_enabled = recycling,
        .population_storage_observer = observe_storage,
        .population_storage_observer_context = capture,
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

static void assert_statistics_equal(
    const evo_generation_statistics_t *left,
    const evo_generation_statistics_t *right)
{
    assert(left->version == right->version);
    assert(left->generation_index == right->generation_index);
    assert(left->population_size == right->population_size);
    assert(left->valid_count == right->valid_count);
    assert(left->invalid_count == right->invalid_count);
    assert(left->best_index == right->best_index);
    assert_fitness_equal(&left->best_fitness, &right->best_fitness);
    assert_fitness_equal(&left->fitness_sums, &right->fitness_sums);
    assert(left->has_best == right->has_best);
    assert(left->fitness_comparison_policy_version ==
           right->fitness_comparison_policy_version);
    assert(left->diversity_policy_version ==
           right->diversity_policy_version);
    assert(left->diversity_metric_version ==
           right->diversity_metric_version);
    assert(left->diversity_pair_count == right->diversity_pair_count);
    assert(left->diversity_work_units == right->diversity_work_units);
    assert(left->diversity == right->diversity);
    assert(left->diversity_uses_domain_distance ==
           right->diversity_uses_domain_distance);
    assert(left->adaptive_mutation_policy_version ==
           right->adaptive_mutation_policy_version);
    assert(left->mutation_rate_prior == right->mutation_rate_prior);
    assert(left->mutation_rate_effective == right->mutation_rate_effective);
    assert(left->adaptive_mutation_min_rate ==
           right->adaptive_mutation_min_rate);
    assert(left->adaptive_mutation_max_rate ==
           right->adaptive_mutation_max_rate);
    assert(left->adaptive_mutation_step == right->adaptive_mutation_step);
    assert(left->adaptive_mutation_diversity_threshold ==
           right->adaptive_mutation_diversity_threshold);
    assert(left->adaptive_mutation_stagnant_generations ==
           right->adaptive_mutation_stagnant_generations);
    assert(left->mutation_adaptation_reason ==
           right->mutation_adaptation_reason);
    assert(left->adaptive_mutation_enabled ==
           right->adaptive_mutation_enabled);
    assert(left->adaptive_mutation_low_diversity ==
           right->adaptive_mutation_low_diversity);
    assert(left->adaptive_mutation_global_best_improved ==
           right->adaptive_mutation_global_best_improved);
    assert(left->adaptive_mutation_clamped_to_min ==
           right->adaptive_mutation_clamped_to_min);
    assert(left->adaptive_mutation_clamped_to_max ==
           right->adaptive_mutation_clamped_to_max);
    assert(left->adaptive_mutation_reset_on_improvement ==
           right->adaptive_mutation_reset_on_improvement);
}

static void assert_results_equal(const evo_result_t *left,
                                 const evo_result_t *right)
{
    assert(left->best_genome != NULL);
    assert(right->best_genome != NULL);
    assert(left->best_genome != right->best_genome);
    assert(left->best_genome_size == TEST_GENOME_SIZE);
    assert(right->best_genome_size == TEST_GENOME_SIZE);
    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        assert(((const unsigned char *)left->best_genome)[index] ==
               ((const unsigned char *)right->best_genome)[index]);
    }
    assert_fitness_equal(&left->best_fitness, &right->best_fitness);
    assert(left->generations_completed == right->generations_completed);
    assert(left->random_seed == right->random_seed);
    assert(left->termination_reason == right->termination_reason);
    assert_statistics_equal(&left->generation_statistics,
                            &right->generation_statistics);
}

static void assert_callback_logs_equal(const callback_log_t *left,
                                       const callback_log_t *right)
{
    assert(left->count == right->count);
    for (size_t event_index = 0;
         event_index < left->count;
         ++event_index) {
        const callback_event_t *left_event = &left->events[event_index];
        const callback_event_t *right_event = &right->events[event_index];

        assert(left_event->kind == right_event->kind);
        assert(left_event->mutation_rate == right_event->mutation_rate);
        for (size_t byte_index = 0;
             byte_index < TEST_GENOME_SIZE;
             ++byte_index) {
            assert(left_event->first[byte_index] ==
                   right_event->first[byte_index]);
            assert(left_event->second[byte_index] ==
                   right_event->second[byte_index]);
        }
    }
}

static void assert_generation_logs_equal(const run_capture_t *left,
                                         const run_capture_t *right)
{
    assert(left->generation_count == right->generation_count);
    for (size_t event_index = 0;
         event_index < left->generation_count;
         ++event_index) {
        const generation_event_t *left_event =
            &left->generations[event_index];
        const generation_event_t *right_event =
            &right->generations[event_index];

        assert(left_event->generations_completed ==
               right_event->generations_completed);
        assert(left_event->termination_reason ==
               right_event->termination_reason);
        assert_fitness_equal(&left_event->best_fitness,
                             &right_event->best_fitness);
        assert_statistics_equal(&left_event->statistics,
                                &right_event->statistics);
        for (size_t byte_index = 0;
             byte_index < TEST_GENOME_SIZE;
             ++byte_index) {
            assert(left_event->best_genome[byte_index] ==
                   right_event->best_genome[byte_index]);
        }
    }
}

static void assert_empty_entry(
    const evo_population_storage_entry_t *entry)
{
    assert(entry->owner_identity == 0);
    assert(entry->lifecycle == EVO_POPULATION_STORAGE_EMPTY);
    assert(entry->population_generation == 0);
    assert(entry->source_generation == 0);
    assert(entry->genome_capacity_bytes == 0);
    assert(entry->evaluation_capacity_bytes == 0);
    assert(entry->handoff_count == 0);
    assert(entry->reset_count == 0);
    assert(entry->genome_erasure_count == 0);
    assert(entry->evaluation_erasure_count == 0);
    assert(entry->last_reset_disposition ==
           EVO_POPULATION_STORAGE_RESET_NONE);
    assert(!entry->genome_owner_present);
    assert(!entry->evaluation_owner_present);
}

static void assert_registry_sequence(const run_capture_t *capture,
                                     bool recycling)
{
    assert(capture->registry_count == TEST_GENERATION_EVENTS);
    for (size_t generation = 0;
         generation < TEST_GENERATION_EVENTS;
         ++generation) {
        const evo_population_storage_registry_t *registry =
            &capture->registries[generation];

        assert(registry->version ==
               EVO_POPULATION_STORAGE_REGISTRY_VERSION);
        assert(registry->policy_version ==
               EVO_POPULATION_RECYCLING_POLICY_VERSION);
        assert(registry->recycling_enabled == recycling);
        assert(registry->secure_erasure_policy_version ==
               EVO_SECURE_ERASURE_POLICY_VERSION);
        assert(registry->secure_erasure_backend ==
               EVO_SECURE_ERASURE_BACKEND_NONE);
        assert(!registry->secure_erasure_enabled);
        if (!recycling) {
            assert(registry->entry_count == 0);
            assert(registry->active_owner_identity == 0);
            assert(registry->reusable_owner_identity == 0);
            assert_empty_entry(&registry->entries[0]);
            assert_empty_entry(&registry->entries[1]);
            continue;
        }

        {
            const size_t half = generation / 2;
            const size_t odd = generation % 2;
            const uint64_t active_identity =
                odd == 0 ? UINT64_C(1) : UINT64_C(2);

            assert(registry->entry_count == (generation == 0 ? 1 : 2));
            assert(registry->active_owner_identity == active_identity);
            assert(registry->reusable_owner_identity ==
                   (generation == 0
                        ? UINT64_C(0)
                        : (active_identity == UINT64_C(1)
                               ? UINT64_C(2)
                               : UINT64_C(1))));
            for (size_t entry_index = 0;
                 entry_index < registry->entry_count;
                 ++entry_index) {
                const uint64_t owner_identity =
                    (uint64_t)entry_index + UINT64_C(1);
                const bool active = owner_identity == active_identity;
                const uint64_t population_generation =
                    active ? (uint64_t)generation
                           : (uint64_t)generation - UINT64_C(1);
                const size_t handoff_count =
                    entry_index == 0 ? half : half + odd;
                const size_t reset_count =
                    entry_index == 0 ? half + odd : half;
                const evo_population_storage_entry_t *entry =
                    &registry->entries[entry_index];

                assert(entry->owner_identity == owner_identity);
                assert(entry->lifecycle ==
                       (active ? EVO_POPULATION_STORAGE_ACTIVE
                               : EVO_POPULATION_STORAGE_REUSABLE));
                assert(entry->population_generation ==
                       population_generation);
                assert(entry->source_generation ==
                       (population_generation == UINT64_C(0)
                            ? UINT64_C(0)
                            : population_generation - UINT64_C(1)));
                assert(entry->genome_capacity_bytes ==
                       TEST_POPULATION_SIZE * TEST_GENOME_SIZE);
                assert(entry->evaluation_capacity_bytes ==
                       TEST_POPULATION_SIZE *
                           sizeof(evo_candidate_evaluation_t));
                assert(entry->handoff_count == handoff_count);
                assert(entry->reset_count == reset_count);
                assert(entry->genome_erasure_count == 0);
                assert(entry->evaluation_erasure_count == 0);
                assert(entry->last_reset_disposition ==
                       (reset_count == 0
                            ? EVO_POPULATION_STORAGE_RESET_NONE
                            : EVO_POPULATION_STORAGE_RESET_ZERO_BYTES));
                assert(entry->genome_owner_present);
                assert(entry->evaluation_owner_present);
            }
            if (registry->entry_count == 1) {
                assert_empty_entry(&registry->entries[1]);
            }
        }
    }
}

static void test_recycling_is_replay_neutral_and_explainable(void)
{
    const evo_problem_t problem = make_problem();
    callback_log_t ordinary_callbacks = {0};
    callback_log_t recycled_callbacks = {0};
    callback_log_t replay_callbacks = {0};
    run_capture_t ordinary_capture = {0};
    run_capture_t recycled_capture = {0};
    run_capture_t replay_capture = {0};
    const evo_config_t ordinary_config =
        make_config(&ordinary_capture, false);
    const evo_config_t recycled_config =
        make_config(&recycled_capture, true);
    const evo_config_t replay_config = make_config(&replay_capture, true);
    evo_result_t ordinary = {0};
    evo_result_t recycled = {0};
    evo_result_t replay = {0};

    assert(evo_run(&problem,
                   &ordinary_config,
                   &ordinary_callbacks,
                   &ordinary) == EVO_SUCCESS);
    assert(evo_run(&problem,
                   &recycled_config,
                   &recycled_callbacks,
                   &recycled) == EVO_SUCCESS);
    assert(evo_run(&problem,
                   &replay_config,
                   &replay_callbacks,
                   &replay) == EVO_SUCCESS);

    assert(ordinary.termination_reason == EVO_TERMINATION_GENERATION_LIMIT);
    assert(ordinary.generations_completed == TEST_GENERATION_LIMIT);
    assert_results_equal(&ordinary, &recycled);
    assert_results_equal(&recycled, &replay);
    assert_callback_logs_equal(&ordinary_callbacks, &recycled_callbacks);
    assert_callback_logs_equal(&recycled_callbacks, &replay_callbacks);
    assert_generation_logs_equal(&ordinary_capture, &recycled_capture);
    assert_generation_logs_equal(&recycled_capture, &replay_capture);
    assert_registry_sequence(&ordinary_capture, false);
    assert_registry_sequence(&recycled_capture, true);
    assert_registry_sequence(&replay_capture, true);

    evo_result_destroy(&replay);
    evo_result_destroy(&recycled);
    evo_result_destroy(&ordinary);
}

static void record_delivery(delivery_log_t *log, char event)
{
    assert(log->count < 3);
    log->events[log->count] = event;
    ++log->count;
}

static void observe_delivery_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    delivery_log_t *log = context;

    assert(result->generations_completed == 0);
    assert(statistics->generation_index == 0);
    record_delivery(log, 'G');
}

static void observe_delivery_storage(
    const evo_population_storage_registry_t *registry,
    void *context)
{
    delivery_log_t *log = context;

    assert(registry->recycling_enabled);
    assert(registry->active_owner_identity == UINT64_C(1));
    record_delivery(log, 'S');
}

static void observe_delivery_checkpoint(
    const void *checkpoint,
    size_t checkpoint_size,
    const evo_checkpoint_view_t *view,
    void *context)
{
    delivery_log_t *log = context;

    assert(checkpoint != NULL);
    assert(checkpoint_size != 0);
    assert(view->current_generation == 0);
    assert(view->population_storage_registry.recycling_enabled);
    assert(view->population_storage_registry.active_owner_identity ==
           UINT64_C(1));
    record_delivery(log, 'C');
}

static void test_projection_delivery_order(void)
{
    evo_problem_t problem = make_problem();
    callback_log_t callbacks = {0};
    run_capture_t unused_capture = {0};
    delivery_log_t deliveries = {0};
    unsigned char checkpoint[4096] = {0};
    evo_config_t config = make_config(&unused_capture, true);
    evo_result_t result = {0};

    problem.checkpoint_problem_identity = UINT64_C(0x5230523052305230);
    config.generation_limit = 0;
    config.generation_observer = observe_delivery_generation;
    config.generation_observer_context = &deliveries;
    config.population_storage_observer = observe_delivery_storage;
    config.population_storage_observer_context = &deliveries;
    config.max_checkpoint_bytes = sizeof(checkpoint);
    config.checkpoint_buffer = checkpoint;
    config.checkpoint_buffer_size = sizeof(checkpoint);
    config.checkpoint_observer = observe_delivery_checkpoint;
    config.checkpoint_observer_context = &deliveries;
    config.checkpoint_context_identity = UINT64_C(0x5252525252525252);

    assert(evo_run(&problem, &config, &callbacks, &result) == EVO_SUCCESS);
    assert(deliveries.count == 3);
    assert(deliveries.events[0] == 'G');
    assert(deliveries.events[1] == 'S');
    assert(deliveries.events[2] == 'C');
    evo_result_destroy(&result);
}

int main(void)
{
    test_recycling_is_replay_neutral_and_explainable();
    test_projection_delivery_order();
    return 0;
}
