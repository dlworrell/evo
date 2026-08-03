#include "catalyst/evo/evo.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

enum {
    TEST_GENERATION_CAPACITY = 8,
    TEST_OBSERVER_CAPACITY = 9
};

typedef struct stopping_context {
    double totals[TEST_GENERATION_CAPACITY];
    double distances[TEST_GENERATION_CAPACITY];
    bool all_invalid[TEST_GENERATION_CAPACITY];
    evo_termination_reason_t observed_reasons[TEST_OBSERVER_CAPACITY];
    size_t observed_generations[TEST_OBSERVER_CAPACITY];
    size_t current_generation;
    size_t initialization_calls;
    size_t mutation_calls;
    size_t validation_calls;
    size_t evaluation_calls;
    size_t distance_calls;
    size_t stop_calls;
    size_t observer_calls;
    size_t stop_on_generation;
} stopping_context_t;

static void initialize_genome(void *genome, void *context)
{
    stopping_context_t *evidence = context;

    ((unsigned char *)genome)[0] =
        (unsigned char)(evidence->initialization_calls + 1);
    ++evidence->initialization_calls;
}

static void mutate_genome(void *genome,
                          double mutation_rate,
                          void *context)
{
    stopping_context_t *evidence = context;

    (void)genome;
    assert(mutation_rate == 1.0);
    evidence->current_generation = evidence->mutation_calls / 2 + 1;
    ++evidence->mutation_calls;
}

static bool validate_genome(const void *genome, void *context)
{
    stopping_context_t *evidence = context;

    (void)genome;
    ++evidence->validation_calls;
    assert(evidence->current_generation < TEST_GENERATION_CAPACITY);
    return !evidence->all_invalid[evidence->current_generation];
}

static evo_fitness_t evaluate_genome(const void *genome, void *context)
{
    stopping_context_t *evidence = context;
    evo_fitness_t fitness = {0};

    (void)genome;
    ++evidence->evaluation_calls;
    assert(evidence->current_generation < TEST_GENERATION_CAPACITY);
    fitness.total = evidence->totals[evidence->current_generation];
    return fitness;
}

static double measure_distance(const void *genome_a,
                               const void *genome_b,
                               size_t genome_size,
                               void *context)
{
    stopping_context_t *evidence = context;

    assert(genome_a != NULL);
    assert(genome_b != NULL);
    assert(genome_a != genome_b);
    assert(genome_size == 1);
    ++evidence->distance_calls;
    assert(evidence->current_generation < TEST_GENERATION_CAPACITY);
    return evidence->distances[evidence->current_generation];
}

static bool stop_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    stopping_context_t *evidence = context;

    assert(result != NULL);
    assert(statistics != NULL);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(result->generations_completed == statistics->generation_index);
    ++evidence->stop_calls;
    return result->generations_completed == evidence->stop_on_generation;
}

static void observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    stopping_context_t *evidence = context;
    const size_t index = evidence->observer_calls;

    assert(index < TEST_OBSERVER_CAPACITY);
    assert(result != NULL);
    assert(statistics != NULL);
    assert(result->generations_completed == statistics->generation_index);
    evidence->observed_reasons[index] = result->termination_reason;
    evidence->observed_generations[index] = result->generations_completed;
    ++evidence->observer_calls;
}

static evo_problem_t make_problem(void)
{
    return (evo_problem_t){
        .genome_size = 1,
        .initialize = initialize_genome,
        .mutate = mutate_genome,
        .evaluate = evaluate_genome,
        .is_valid = validate_genome,
        .genome_distance = measure_distance,
        .genome_distance_version = UINT32_C(9),
    };
}

static evo_config_t make_config(size_t generation_limit,
                                stopping_context_t *context)
{
    return (evo_config_t){
        .population_size = 2,
        .generation_limit = generation_limit,
        .tournament_size = 2,
        .crossover_rate = 0.0,
        .mutation_rate = 1.0,
        .random_seed = UINT64_C(0x45),
        .max_genome_bytes = 1,
        .max_population_bytes = 2,
        .max_evaluation_bytes = 1024,
        .max_child_population_bytes = 2,
        .generation_observer = observe_generation,
        .generation_observer_context = context,
        .max_diversity_work = 1,
    };
}

static stopping_context_t make_context(void)
{
    stopping_context_t context = {
        .stop_on_generation = SIZE_MAX,
    };

    for (size_t generation = 0;
         generation < TEST_GENERATION_CAPACITY;
         ++generation) {
        context.totals[generation] = 10.0;
        context.distances[generation] = 1.0;
    }
    return context;
}

static void assert_empty_result(const evo_result_t *result)
{
    assert(result->best_genome == NULL);
    assert(result->best_fitness.total == 0.0);
    assert(result->generations_completed == 0);
    assert(result->random_seed == 0);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(result->generation_statistics.version == 0);
}

static void test_disabled_controls_preserve_generation_limit(void)
{
    evo_problem_t problem = make_problem();
    stopping_context_t context = make_context();
    evo_config_t config = make_config(3, &context);
    evo_result_t result = {0};

    context.totals[1] = 11.0;
    context.totals[2] = 12.0;
    context.totals[3] = 13.0;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.best_fitness.total == 13.0);
    assert(result.generations_completed == 3);
    assert(result.termination_reason == EVO_TERMINATION_GENERATION_LIMIT);
    assert(context.observer_calls == 4);
    assert(context.observed_reasons[0] == EVO_TERMINATION_NONE);
    assert(context.observed_reasons[1] == EVO_TERMINATION_NONE);
    assert(context.observed_reasons[2] == EVO_TERMINATION_NONE);
    assert(context.observed_reasons[3] ==
           EVO_TERMINATION_GENERATION_LIMIT);
    evo_result_destroy(&result);
}

static void test_target_exact_boundary_and_precedence(void)
{
    evo_problem_t problem = make_problem();
    stopping_context_t context = make_context();
    evo_config_t config = make_config(1, &context);
    evo_result_t result = {0};

    context.totals[1] = 20.0;
    context.distances[1] = 0.25;
    context.stop_on_generation = 1;
    config.generation_stop = stop_generation;
    config.generation_stop_context = &context;
    config.fitness_target_enabled = true;
    config.fitness_target = 20.0;
    config.diversity_floor_enabled = true;
    config.diversity_floor = 0.25;

    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.best_fitness.total == 20.0);
    assert(result.generations_completed == 1);
    assert(result.termination_reason == EVO_TERMINATION_CONVERGED);
    assert(context.stop_calls == 1);
    assert(context.observer_calls == 2);
    assert(context.observed_reasons[1] == EVO_TERMINATION_CONVERGED);
    evo_result_destroy(&result);

    context = make_context();
    config = make_config(0, &context);
    config.fitness_target_enabled = true;
    config.fitness_target = 10.0;
    config.generation_stop = stop_generation;
    config.generation_stop_context = &context;
    context.stop_on_generation = 0;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.generations_completed == 0);
    assert(result.termination_reason == EVO_TERMINATION_CONVERGED);
    assert(context.stop_calls == 0);
    assert(context.observer_calls == 1);
    assert(context.observed_reasons[0] == EVO_TERMINATION_CONVERGED);
    evo_result_destroy(&result);
}

static void configure_patience_sequence(stopping_context_t *context)
{
    context->totals[0] = 10.0;
    context->totals[1] = 10.5;
    context->totals[2] = 12.0;
    context->totals[3] = 12.0;
    context->totals[4] = 13.0;
    context->totals[5] = 99.0;
}

static void assert_patience_result(const evo_result_t *result,
                                   const stopping_context_t *context)
{
    assert(result->best_fitness.total == 13.0);
    assert(result->generations_completed == 4);
    assert(result->termination_reason == EVO_TERMINATION_STAGNATED);
    assert(result->generation_statistics.generation_index == 4);
    assert(context->mutation_calls == 8);
    assert(context->observer_calls == 5);
    assert(context->observed_reasons[3] == EVO_TERMINATION_NONE);
    assert(context->observed_reasons[4] == EVO_TERMINATION_STAGNATED);
}

static void test_patience_boundary_reset_tie_and_replay(void)
{
    evo_problem_t problem = make_problem();
    stopping_context_t first_context = make_context();
    stopping_context_t replay_context = make_context();
    evo_config_t first_config = make_config(6, &first_context);
    evo_config_t replay_config = make_config(6, &replay_context);
    evo_result_t first = {0};
    evo_result_t replay = {0};

    configure_patience_sequence(&first_context);
    configure_patience_sequence(&replay_context);
    first_config.stagnation_enabled = true;
    first_config.improvement_tolerance = 1.0;
    first_config.stagnation_patience = 2;
    replay_config.stagnation_enabled = true;
    replay_config.improvement_tolerance = 1.0;
    replay_config.stagnation_patience = 2;

    assert(evo_run(&problem, &first_config, &first_context, &first) ==
           EVO_SUCCESS);
    assert(evo_run(&problem, &replay_config, &replay_context, &replay) ==
           EVO_SUCCESS);
    assert_patience_result(&first, &first_context);
    assert_patience_result(&replay, &replay_context);
    assert(first.best_fitness.total == replay.best_fitness.total);
    assert(*(const unsigned char *)first.best_genome ==
           *(const unsigned char *)replay.best_genome);
    assert(first_context.validation_calls == replay_context.validation_calls);
    assert(first_context.evaluation_calls == replay_context.evaluation_calls);
    assert(first_context.distance_calls == replay_context.distance_calls);
    evo_result_destroy(&first);
    evo_result_destroy(&replay);
}

static void test_diversity_exact_floor(void)
{
    evo_problem_t problem = make_problem();
    stopping_context_t context = make_context();
    evo_config_t config = make_config(5, &context);
    evo_result_t result = {0};

    context.distances[0] = 0.25;
    context.stop_on_generation = 0;
    config.generation_stop = stop_generation;
    config.generation_stop_context = &context;
    config.diversity_floor_enabled = true;
    config.diversity_floor = 0.25;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.generations_completed == 0);
    assert(result.termination_reason == EVO_TERMINATION_STAGNATED);
    assert(result.generation_statistics.diversity == 0.25);
    assert(context.stop_calls == 0);
    assert(context.observed_reasons[0] == EVO_TERMINATION_STAGNATED);
    evo_result_destroy(&result);
}

static void test_extinction_and_application_precedence(void)
{
    evo_problem_t problem = make_problem();
    stopping_context_t context = make_context();
    evo_config_t config = make_config(3, &context);
    evo_result_t result = {0};

    context.all_invalid[1] = true;
    context.stop_on_generation = 1;
    config.generation_stop = stop_generation;
    config.generation_stop_context = &context;
    config.stagnation_enabled = true;
    config.improvement_tolerance = 0.0;
    config.stagnation_patience = 1;
    config.diversity_floor_enabled = true;
    config.diversity_floor = 0.0;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.generations_completed == 1);
    assert(result.termination_reason == EVO_TERMINATION_ALL_INVALID);
    assert(result.generation_statistics.valid_count == 0);
    assert(result.generation_statistics.diversity == 0.0);
    assert(context.stop_calls == 1);
    assert(context.observed_reasons[1] == EVO_TERMINATION_ALL_INVALID);
    evo_result_destroy(&result);

    context = make_context();
    config = make_config(1, &context);
    context.stop_on_generation = 1;
    config.generation_stop = stop_generation;
    config.generation_stop_context = &context;
    config.stagnation_enabled = true;
    config.improvement_tolerance = 0.0;
    config.stagnation_patience = 1;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.generations_completed == 1);
    assert(result.termination_reason == EVO_TERMINATION_STAGNATED);
    assert(context.stop_calls == 1);
    assert(context.observed_reasons[1] == EVO_TERMINATION_STAGNATED);
    evo_result_destroy(&result);

    context = make_context();
    config = make_config(3, &context);
    context.stop_on_generation = 1;
    config.generation_stop = stop_generation;
    config.generation_stop_context = &context;
    config.stagnation_enabled = true;
    config.improvement_tolerance = 0.0;
    config.stagnation_patience = 2;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.generations_completed == 1);
    assert(result.termination_reason ==
           EVO_TERMINATION_APPLICATION_REQUESTED);
    assert(context.stop_calls == 2);
    evo_result_destroy(&result);

    context = make_context();
    config = make_config(3, &context);
    context.stop_on_generation = 2;
    config.generation_stop = stop_generation;
    config.generation_stop_context = &context;
    config.stagnation_enabled = true;
    config.improvement_tolerance = 0.0;
    config.stagnation_patience = 2;
    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(result.generations_completed == 2);
    assert(result.termination_reason == EVO_TERMINATION_STAGNATED);
    assert(context.stop_calls == 2);
    assert(context.observed_reasons[2] == EVO_TERMINATION_STAGNATED);
    evo_result_destroy(&result);
}

static void assert_invalid_preflight(evo_config_t config)
{
    evo_problem_t problem = make_problem();
    stopping_context_t context = make_context();
    evo_result_t result = {0};

    config.generation_observer_context = &context;
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_empty_result(&result);
    assert(context.initialization_calls == 0);
    assert(context.validation_calls == 0);
    assert(context.evaluation_calls == 0);
    assert(context.distance_calls == 0);
    assert(context.observer_calls == 0);
}

static void test_malformed_controls_reject_before_callbacks(void)
{
    stopping_context_t unused = make_context();
    evo_config_t config = make_config(1, &unused);

    config.fitness_target = 1.0;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.fitness_target_enabled = true;
    config.fitness_target = NAN;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.improvement_tolerance = 1.0;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.stagnation_patience = 1;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.stagnation_enabled = true;
    config.improvement_tolerance = -1.0;
    config.stagnation_patience = 1;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.stagnation_enabled = true;
    config.improvement_tolerance = NAN;
    config.stagnation_patience = 1;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.stagnation_enabled = true;
    config.stagnation_patience = 0;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.diversity_floor = 0.5;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.diversity_floor_enabled = true;
    config.diversity_floor = 1.5;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.diversity_floor_enabled = true;
    config.diversity_floor = -0.5;
    assert_invalid_preflight(config);

    config = make_config(1, &unused);
    config.diversity_floor_enabled = true;
    config.diversity_floor = NAN;
    assert_invalid_preflight(config);
}

int main(void)
{
    test_disabled_controls_preserve_generation_limit();
    test_target_exact_boundary_and_precedence();
    test_patience_boundary_reset_tie_and_replay();
    test_diversity_exact_floor();
    test_extinction_and_application_precedence();
    test_malformed_controls_reject_before_callbacks();
    return 0;
}
