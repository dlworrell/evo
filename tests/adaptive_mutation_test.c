#include "catalyst/evo/evo.h"
#include "internal/adaptive_mutation.h"
#include "internal/population_storage.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

enum {
    TEST_TRACE_CAPACITY = 8,
    TEST_GENOME_SIZE = 1
};

typedef struct trace_entry {
    uint64_t generation;
    double prior_rate;
    double effective_rate;
    size_t stagnant_generations;
    evo_mutation_adaptation_reason_t reason;
    bool low_diversity;
    bool improved;
    bool clamped_to_min;
    bool clamped_to_max;
} trace_entry_t;

typedef struct run_context {
    trace_entry_t trace[TEST_TRACE_CAPACITY];
    double callback_rates[TEST_TRACE_CAPACITY];
    size_t trace_count;
    size_t callback_count;
    size_t initialize_calls;
    size_t validate_calls;
    size_t evaluate_calls;
    bool callback_since_observation;
    double latest_callback_rate;
} run_context_t;

static evo_generation_statistics_t make_statistics(uint64_t generation,
                                                   double diversity)
{
    return (evo_generation_statistics_t){
        .version = EVO_GENERATION_STATISTICS_VERSION,
        .generation_index = generation,
        .population_size = 2,
        .valid_count = 2,
        .invalid_count = 0,
        .has_best = true,
        .fitness_comparison_policy_version =
            EVO_FITNESS_COMPARISON_POLICY_VERSION,
        .diversity_policy_version = EVO_DIVERSITY_POLICY_VERSION,
        .diversity_metric_version = EVO_BYTE_DIVERSITY_METRIC_VERSION,
        .diversity = diversity,
    };
}

static evo_config_t make_adaptive_config(void)
{
    return (evo_config_t){
        .population_size = 2,
        .generation_limit = 8,
        .mutation_rate = 0.0625,
        .adaptive_mutation_enabled = true,
        .adaptive_mutation_min_rate = 0.125,
        .adaptive_mutation_max_rate = 0.5,
        .adaptive_mutation_step = 0.125,
        .adaptive_mutation_diversity_threshold = 0.25,
        .adaptive_mutation_reset_on_improvement = true,
    };
}

static void assert_state(const evo_adaptive_mutation_state_t *state,
                         double rate,
                         size_t stagnant_generations)
{
    assert(state->initialized);
    assert(state->effective_rate == rate);
    assert(state->stagnant_generations == stagnant_generations);
}

static void assert_projection(
    const evo_config_t *config,
    const evo_generation_statistics_t *statistics,
    double prior_rate,
    double effective_rate,
    size_t stagnant_generations,
    evo_mutation_adaptation_reason_t reason,
    bool low_diversity,
    bool improved,
    bool clamped_to_min,
    bool clamped_to_max)
{
    assert(evo_adaptive_mutation_statistics_are_valid(config, statistics));
    assert(statistics->adaptive_mutation_policy_version ==
           EVO_MUTATION_ADAPTATION_POLICY_VERSION);
    assert(statistics->mutation_rate_prior == prior_rate);
    assert(statistics->mutation_rate_effective == effective_rate);
    assert(statistics->adaptive_mutation_min_rate ==
           config->adaptive_mutation_min_rate);
    assert(statistics->adaptive_mutation_max_rate ==
           config->adaptive_mutation_max_rate);
    assert(statistics->adaptive_mutation_step ==
           config->adaptive_mutation_step);
    assert(statistics->adaptive_mutation_diversity_threshold ==
           config->adaptive_mutation_diversity_threshold);
    assert(statistics->adaptive_mutation_stagnant_generations ==
           stagnant_generations);
    assert(statistics->mutation_adaptation_reason == reason);
    assert(statistics->adaptive_mutation_enabled ==
           config->adaptive_mutation_enabled);
    assert(statistics->adaptive_mutation_low_diversity == low_diversity);
    assert(statistics->adaptive_mutation_global_best_improved == improved);
    assert(statistics->adaptive_mutation_clamped_to_min == clamped_to_min);
    assert(statistics->adaptive_mutation_clamped_to_max == clamped_to_max);
    assert(statistics->adaptive_mutation_reset_on_improvement ==
           config->adaptive_mutation_reset_on_improvement);
}

static void test_exact_adaptive_trace(void)
{
    evo_config_t config = make_adaptive_config();
    evo_generation_statistics_t statistics = make_statistics(0, 0.5);
    evo_adaptive_mutation_state_t state = {0};
    evo_adaptive_mutation_state_t restored = {0};

    assert(evo_adaptive_mutation_validate_config(&config) == EVO_SUCCESS);
    assert(evo_adaptive_mutation_initialize(&config,
                                            &statistics,
                                            &state) == EVO_SUCCESS);
    assert_state(&state, 0.125, 0);
    assert_projection(&config,
                      &statistics,
                      0.0625,
                      0.125,
                      0,
                      EVO_MUTATION_ADAPTATION_INITIAL,
                      false,
                      false,
                      true,
                      false);

    assert(evo_adaptive_mutation_restore_initial(&config,
                                                 &statistics,
                                                 &restored) == EVO_SUCCESS);
    assert_state(&restored, 0.125, 0);

    statistics = make_statistics(1, 0.5);
    assert(evo_adaptive_mutation_commit(&config,
                                        false,
                                        &statistics,
                                        &state) == EVO_SUCCESS);
    assert_state(&state, 0.25, 1);
    assert_projection(&config,
                      &statistics,
                      0.125,
                      0.25,
                      1,
                      EVO_MUTATION_ADAPTATION_STAGNATION,
                      false,
                      false,
                      false,
                      false);

    statistics = make_statistics(2, 0.25);
    assert(evo_adaptive_mutation_commit(&config,
                                        false,
                                        &statistics,
                                        &state) == EVO_SUCCESS);
    assert_state(&state, 0.375, 2);
    assert_projection(&config,
                      &statistics,
                      0.25,
                      0.375,
                      2,
                      EVO_MUTATION_ADAPTATION_STAGNATION_LOW_DIVERSITY,
                      true,
                      false,
                      false,
                      false);

    statistics = make_statistics(3, 0.5);
    assert(evo_adaptive_mutation_commit(&config,
                                        false,
                                        &statistics,
                                        &state) == EVO_SUCCESS);
    assert_state(&state, 0.5, 3);
    assert_projection(&config,
                      &statistics,
                      0.375,
                      0.5,
                      3,
                      EVO_MUTATION_ADAPTATION_STAGNATION,
                      false,
                      false,
                      false,
                      false);

    statistics = make_statistics(4, 0.5);
    assert(evo_adaptive_mutation_commit(&config,
                                        false,
                                        &statistics,
                                        &state) == EVO_SUCCESS);
    assert_state(&state, 0.5, 4);
    assert_projection(&config,
                      &statistics,
                      0.5,
                      0.5,
                      4,
                      EVO_MUTATION_ADAPTATION_STAGNATION,
                      false,
                      false,
                      false,
                      true);

    statistics = make_statistics(5, 0.75);
    assert(evo_adaptive_mutation_commit(&config,
                                        true,
                                        &statistics,
                                        &state) == EVO_SUCCESS);
    assert_state(&state, 0.125, 0);
    assert_projection(&config,
                      &statistics,
                      0.5,
                      0.125,
                      0,
                      EVO_MUTATION_ADAPTATION_IMPROVEMENT_RESET,
                      false,
                      true,
                      false,
                      false);
}

static void test_improvement_hold_and_low_diversity(void)
{
    evo_config_t config = make_adaptive_config();
    evo_generation_statistics_t statistics = make_statistics(0, 0.5);
    evo_adaptive_mutation_state_t state = {0};

    config.mutation_rate = 0.25;
    config.adaptive_mutation_reset_on_improvement = false;
    assert(evo_adaptive_mutation_initialize(&config,
                                            &statistics,
                                            &state) == EVO_SUCCESS);
    assert_state(&state, 0.25, 0);

    statistics = make_statistics(1, 0.5);
    assert(evo_adaptive_mutation_commit(&config,
                                        true,
                                        &statistics,
                                        &state) == EVO_SUCCESS);
    assert_state(&state, 0.25, 0);
    assert_projection(&config,
                      &statistics,
                      0.25,
                      0.25,
                      0,
                      EVO_MUTATION_ADAPTATION_IMPROVEMENT_HOLD,
                      false,
                      true,
                      false,
                      false);

    statistics = make_statistics(2, 0.25);
    assert(evo_adaptive_mutation_commit(&config,
                                        true,
                                        &statistics,
                                        &state) == EVO_SUCCESS);
    assert_state(&state, 0.375, 0);
    assert_projection(&config,
                      &statistics,
                      0.25,
                      0.375,
                      0,
                      EVO_MUTATION_ADAPTATION_LOW_DIVERSITY,
                      true,
                      true,
                      false,
                      false);
}

static void test_stagnation_counter_saturates(void)
{
    evo_config_t config = make_adaptive_config();
    evo_generation_statistics_t statistics = make_statistics(9, 0.5);
    evo_adaptive_mutation_state_t state = {
        .effective_rate = 0.5,
        .stagnant_generations = SIZE_MAX,
        .initialized = true,
    };

    assert(evo_adaptive_mutation_commit(&config,
                                        false,
                                        &statistics,
                                        &state) == EVO_SUCCESS);
    assert_state(&state, 0.5, SIZE_MAX);
    assert_projection(&config,
                      &statistics,
                      0.5,
                      0.5,
                      SIZE_MAX,
                      EVO_MUTATION_ADAPTATION_STAGNATION,
                      false,
                      false,
                      false,
                      true);
}

static void test_disabled_and_boundary_configuration(void)
{
    evo_config_t config = {
        .population_size = 2,
        .generation_limit = 2,
        .mutation_rate = 0.375,
    };
    evo_generation_statistics_t statistics = make_statistics(0, 0.0);
    evo_adaptive_mutation_state_t state = {0};

    assert(evo_adaptive_mutation_validate_config(&config) == EVO_SUCCESS);
    assert(evo_adaptive_mutation_initialize(&config,
                                            &statistics,
                                            &state) == EVO_SUCCESS);
    assert_state(&state, 0.375, 0);
    assert_projection(&config,
                      &statistics,
                      0.375,
                      0.375,
                      0,
                      EVO_MUTATION_ADAPTATION_DISABLED,
                      false,
                      false,
                      false,
                      false);

    statistics = make_statistics(1, 0.0);
    assert(evo_adaptive_mutation_commit(&config,
                                        false,
                                        &statistics,
                                        &state) == EVO_SUCCESS);
    assert_state(&state, 0.375, 0);
    assert_projection(&config,
                      &statistics,
                      0.375,
                      0.375,
                      0,
                      EVO_MUTATION_ADAPTATION_DISABLED,
                      false,
                      false,
                      false,
                      false);

    config = make_adaptive_config();
    config.mutation_rate = 0.875;
    statistics = make_statistics(0, 0.25);
    state = (evo_adaptive_mutation_state_t){0};
    assert(evo_adaptive_mutation_initialize(&config,
                                            &statistics,
                                            &state) == EVO_SUCCESS);
    assert_state(&state, 0.5, 0);
    assert_projection(&config,
                      &statistics,
                      0.875,
                      0.5,
                      0,
                      EVO_MUTATION_ADAPTATION_LOW_DIVERSITY,
                      true,
                      false,
                      false,
                      true);

    config = make_adaptive_config();
    config.mutation_rate = 0.0;
    config.adaptive_mutation_min_rate = 0.5;
    config.adaptive_mutation_max_rate = 0.5;
    statistics = make_statistics(0, 0.25);
    state = (evo_adaptive_mutation_state_t){0};
    assert(evo_adaptive_mutation_initialize(&config,
                                            &statistics,
                                            &state) == EVO_SUCCESS);
    assert_state(&state, 0.5, 0);
    assert_projection(&config,
                      &statistics,
                      0.0,
                      0.5,
                      0,
                      EVO_MUTATION_ADAPTATION_LOW_DIVERSITY,
                      true,
                      false,
                      true,
                      true);
}

static void test_invalid_and_alias_inputs_are_atomic(void)
{
    evo_config_t config = make_adaptive_config();
    evo_generation_statistics_t statistics = make_statistics(0, 0.5);
    evo_adaptive_mutation_state_t state = {0};

    config.adaptive_mutation_step = 0.0;
    assert(evo_adaptive_mutation_validate_config(&config) ==
           EVO_ERROR_INVALID_ARGUMENT);
    config.adaptive_mutation_step = NAN;
    assert(evo_adaptive_mutation_validate_config(&config) ==
           EVO_ERROR_INVALID_ARGUMENT);
    config = make_adaptive_config();
    config.adaptive_mutation_min_rate = 0.5;
    config.adaptive_mutation_max_rate = 0.4;
    assert(evo_adaptive_mutation_validate_config(&config) ==
           EVO_ERROR_INVALID_ARGUMENT);
    config = make_adaptive_config();
    config.adaptive_mutation_diversity_threshold = INFINITY;
    assert(evo_adaptive_mutation_validate_config(&config) ==
           EVO_ERROR_INVALID_ARGUMENT);
    config = make_adaptive_config();
    config.mutation_rate = NAN;
    assert(evo_adaptive_mutation_validate_config(&config) ==
           EVO_ERROR_RESOURCE_LIMIT);

    config = make_adaptive_config();
    config.adaptive_mutation_enabled = false;
    assert(evo_adaptive_mutation_validate_config(&config) ==
           EVO_ERROR_INVALID_ARGUMENT);

    config = make_adaptive_config();
    assert(evo_adaptive_mutation_initialize(
               &config,
               (evo_generation_statistics_t *)(void *)&config,
               &state) == EVO_ERROR_INVALID_ARGUMENT);
    assert(!state.initialized);
    assert(state.effective_rate == 0.0);
    assert(state.stagnant_generations == 0);

    statistics.adaptive_mutation_policy_version = UINT32_C(99);
    assert(evo_adaptive_mutation_initialize(&config,
                                            &statistics,
                                            &state) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(!state.initialized);
    assert(state.effective_rate == 0.0);
    assert(state.stagnant_generations == 0);
    assert(statistics.adaptive_mutation_policy_version == UINT32_C(99));

    statistics = make_statistics(0, 0.5);
    state = (evo_adaptive_mutation_state_t){0};
    assert(evo_adaptive_mutation_initialize(&config,
                                            &statistics,
                                            &state) == EVO_SUCCESS);
    statistics.mutation_rate_effective = 0.375;
    assert(!evo_adaptive_mutation_statistics_are_valid(&config,
                                                       &statistics));
    state = (evo_adaptive_mutation_state_t){0};
    assert(evo_adaptive_mutation_restore_initial(&config,
                                                 &statistics,
                                                 &state) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(!state.initialized);
}

static void initialize_genome(void *genome, void *opaque)
{
    run_context_t *context = opaque;

    ((unsigned char *)genome)[0] = 0;
    ++context->initialize_calls;
}

static void mutate_genome(void *genome,
                          double mutation_rate,
                          void *opaque)
{
    run_context_t *context = opaque;

    assert(context->callback_count < TEST_TRACE_CAPACITY);
    context->callback_rates[context->callback_count] = mutation_rate;
    ++context->callback_count;
    context->callback_since_observation = true;
    context->latest_callback_rate = mutation_rate;
    ((unsigned char *)genome)[0] ^= UINT8_C(1);
}

static bool validate_genome(const void *genome, void *opaque)
{
    run_context_t *context = opaque;

    (void)genome;
    ++context->validate_calls;
    return true;
}

static evo_fitness_t evaluate_genome(const void *genome, void *opaque)
{
    run_context_t *context = opaque;

    (void)genome;
    ++context->evaluate_calls;
    return (evo_fitness_t){.total = 1.0};
}

static void observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *opaque)
{
    run_context_t *context = opaque;
    trace_entry_t *entry = NULL;

    assert(result != NULL);
    assert(statistics != NULL);
    assert(context->trace_count < TEST_TRACE_CAPACITY);
    assert(result->generations_completed == statistics->generation_index);
    if (statistics->generation_index != UINT64_C(0) &&
        context->callback_since_observation) {
        assert(context->latest_callback_rate ==
               statistics->mutation_rate_prior);
    }

    entry = &context->trace[context->trace_count];
    entry->generation = statistics->generation_index;
    entry->prior_rate = statistics->mutation_rate_prior;
    entry->effective_rate = statistics->mutation_rate_effective;
    entry->stagnant_generations =
        statistics->adaptive_mutation_stagnant_generations;
    entry->reason = statistics->mutation_adaptation_reason;
    entry->low_diversity = statistics->adaptive_mutation_low_diversity;
    entry->improved = statistics->adaptive_mutation_global_best_improved;
    entry->clamped_to_min =
        statistics->adaptive_mutation_clamped_to_min;
    entry->clamped_to_max =
        statistics->adaptive_mutation_clamped_to_max;
    ++context->trace_count;
    context->callback_since_observation = false;
}

static evo_problem_t make_problem(void)
{
    return (evo_problem_t){
        .genome_size = TEST_GENOME_SIZE,
        .initialize = initialize_genome,
        .mutate = mutate_genome,
        .evaluate = evaluate_genome,
        .is_valid = validate_genome,
    };
}

static evo_config_t make_run_config(void)
{
    return (evo_config_t){
        .population_size = 1,
        .generation_limit = 3,
        .tournament_size = 1,
        .crossover_rate = NAN,
        .mutation_rate = 0.25,
        .random_seed = UINT64_C(20260807),
        .max_genome_bytes = TEST_GENOME_SIZE,
        .max_population_bytes = TEST_GENOME_SIZE,
        .max_evaluation_bytes = sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes = TEST_GENOME_SIZE,
        .max_diversity_work = 0,
        .elite_count_enabled = true,
        .elite_count = 0,
        .adaptive_mutation_enabled = true,
        .adaptive_mutation_min_rate = 0.25,
        .adaptive_mutation_max_rate = 1.0,
        .adaptive_mutation_step = 0.25,
        .adaptive_mutation_diversity_threshold = 0.0,
        .adaptive_mutation_reset_on_improvement = true,
        .generation_observer = observe_generation,
    };
}

static void assert_public_trace(const run_context_t *context)
{
    static const double prior_rates[4] = {0.25, 0.5, 0.75, 1.0};
    static const double effective_rates[4] = {0.5, 0.75, 1.0, 1.0};
    static const size_t stagnant_counts[4] = {0, 1, 2, 3};
    static const evo_mutation_adaptation_reason_t reasons[4] = {
        EVO_MUTATION_ADAPTATION_LOW_DIVERSITY,
        EVO_MUTATION_ADAPTATION_STAGNATION_LOW_DIVERSITY,
        EVO_MUTATION_ADAPTATION_STAGNATION_LOW_DIVERSITY,
        EVO_MUTATION_ADAPTATION_STAGNATION_LOW_DIVERSITY,
    };

    assert(context->trace_count == 4);
    for (size_t index = 0; index < 4; ++index) {
        const trace_entry_t *entry = &context->trace[index];

        assert(entry->generation == (uint64_t)index);
        assert(entry->prior_rate == prior_rates[index]);
        assert(entry->effective_rate == effective_rates[index]);
        assert(entry->stagnant_generations == stagnant_counts[index]);
        assert(entry->reason == reasons[index]);
        assert(entry->low_diversity);
        assert(!entry->improved);
        assert(!entry->clamped_to_min);
        assert(entry->clamped_to_max == (index == 3));
    }
}

static void assert_results_equal(const evo_result_t *left,
                                 const evo_result_t *right)
{
    assert(left->best_genome != NULL);
    assert(right->best_genome != NULL);
    assert(((const unsigned char *)left->best_genome)[0] ==
           ((const unsigned char *)right->best_genome)[0]);
    assert(left->best_fitness.total == right->best_fitness.total);
    assert(left->generations_completed == right->generations_completed);
    assert(left->random_seed == right->random_seed);
    assert(left->termination_reason == right->termination_reason);
    assert(left->generation_statistics.mutation_rate_prior ==
           right->generation_statistics.mutation_rate_prior);
    assert(left->generation_statistics.mutation_rate_effective ==
           right->generation_statistics.mutation_rate_effective);
    assert(left->generation_statistics.mutation_adaptation_reason ==
           right->generation_statistics.mutation_adaptation_reason);
}

static void test_public_consumer_and_reference_traces(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = make_run_config();
    run_context_t consumer_context = {0};
    run_context_t reference_context = {0};
    run_context_t replay_context = {0};
    evo_result_t consumer_result = {0};
    evo_result_t reference_result = {0};
    evo_result_t replay_result = {0};

    config.generation_observer_context = &consumer_context;
    assert(evo_run(&problem,
                   &config,
                   &consumer_context,
                   &consumer_result) == EVO_SUCCESS);
    assert(consumer_result.generations_completed == 3);
    assert(consumer_result.termination_reason ==
           EVO_TERMINATION_GENERATION_LIMIT);
    assert(consumer_context.initialize_calls == 1);
    assert(consumer_context.validate_calls == 4);
    assert(consumer_context.evaluate_calls == 4);
    assert(consumer_context.callback_count != 0);
    assert_public_trace(&consumer_context);

    config.mutation_operator = EVO_MUTATION_BYTE_XOR;
    config.generation_observer_context = &reference_context;
    assert(evo_run(&problem,
                   &config,
                   &reference_context,
                   &reference_result) == EVO_SUCCESS);
    assert(reference_context.callback_count == 0);
    assert_public_trace(&reference_context);

    config.generation_observer_context = &replay_context;
    assert(evo_run(&problem,
                   &config,
                   &replay_context,
                   &replay_result) == EVO_SUCCESS);
    assert(replay_context.callback_count == 0);
    assert_public_trace(&replay_context);
    assert_results_equal(&reference_result, &replay_result);

    evo_result_destroy(&consumer_result);
    evo_result_destroy(&reference_result);
    evo_result_destroy(&replay_result);
}

static void test_unused_policy_and_preflight(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = make_run_config();
    run_context_t context = {0};
    evo_result_t result = {0};

    config.generation_limit = 0;
    config.mutation_rate = NAN;
    config.adaptive_mutation_step = NAN;
    config.generation_observer_context = &context;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.generations_completed == 0);
    assert(result.termination_reason == EVO_TERMINATION_GENERATION_LIMIT);
    assert(result.generation_statistics.adaptive_mutation_policy_version ==
           UINT32_C(0));
    assert(result.generation_statistics.mutation_adaptation_reason ==
           EVO_MUTATION_ADAPTATION_NOT_APPLICABLE);
    assert(context.trace_count == 1);
    assert(context.callback_count == 0);
    evo_result_destroy(&result);

    context = (run_context_t){0};
    config = make_run_config();
    config.elite_count_enabled = false;
    config.mutation_rate = NAN;
    config.adaptive_mutation_step = NAN;
    config.generation_observer_context = &context;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.generations_completed == 3);
    assert(result.generation_statistics.adaptive_mutation_policy_version ==
           UINT32_C(0));
    assert(context.callback_count == 0);
    evo_result_destroy(&result);

    context = (run_context_t){0};
    config = make_run_config();
    config.adaptive_mutation_step = 0.0;
    config.generation_observer_context = &context;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(result.best_genome == NULL);
    assert(result.generation_statistics.version == UINT32_C(0));
    assert(context.initialize_calls == 0);
    assert(context.callback_count == 0);
    assert(context.validate_calls == 0);
    assert(context.evaluate_calls == 0);
    assert(context.trace_count == 0);
}

int main(void)
{
    test_exact_adaptive_trace();
    test_improvement_hold_and_low_diversity();
    test_stagnation_counter_saturates();
    test_disabled_and_boundary_configuration();
    test_invalid_and_alias_inputs_are_atomic();
    test_public_consumer_and_reference_traces();
    test_unused_policy_and_preflight();
    return 0;
}
