#include "catalyst/evo/evo.h"
#include "internal/child_evaluation.h"
#include "internal/child_pair.h"
#include "internal/generation_advancement.h"
#include "internal/population_storage.h"

#include <assert.h>
#include <stddef.h>

void *__real_calloc(size_t count, size_t size);
void __real_free(void *allocation);

static size_t allocation_calls;
static size_t continuation_stop_calls;
static size_t fail_allocation_call;
static size_t observation_calls;
static size_t release_calls;
static size_t stop_calls;

void *__wrap_calloc(size_t count, size_t size)
{
    ++allocation_calls;
    if (fail_allocation_call != 0 &&
        allocation_calls == fail_allocation_call) {
        return NULL;
    }

    return __real_calloc(count, size);
}

void __wrap_free(void *allocation)
{
    if (allocation != NULL) {
        ++release_calls;
    }
    __real_free(allocation);
}

static void assert_completely_empty(const evo_result_t *result)
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

static evo_fitness_t deterministic_evaluator(const void *genome,
                                             void *context)
{
    (void)genome;
    (void)context;
    return (evo_fitness_t){.total = 1.0};
}

static void count_observation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    size_t *count = context;

    assert(result != NULL);
    assert(statistics != NULL);
    assert(count == &observation_calls);
    assert(result->version == EVO_GENERATION_RESULT_VIEW_VERSION);
    assert(statistics->version == EVO_GENERATION_STATISTICS_VERSION);
    ++*count;
}

static bool request_immediate_stop(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    size_t *count = context;

    assert(result != NULL);
    assert(statistics != NULL);
    assert(count == &stop_calls);
    assert(result->generations_completed == 0);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(statistics->generation_index == 0);
    ++*count;
    return true;
}

static bool continue_after_commit(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    size_t *count = context;

    assert(result != NULL);
    assert(statistics != NULL);
    assert(count == &continuation_stop_calls);
    assert(result->version == EVO_GENERATION_RESULT_VIEW_VERSION);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(statistics->version == EVO_GENERATION_STATISTICS_VERSION);
    assert(statistics->generation_index == result->generations_completed);
    ++*count;
    return false;
}

static void assert_population_empty(const evo_population_t *population)
{
    assert(population->genomes == NULL);
    assert(population->evaluations == NULL);
    assert(population->population_size == 0);
    assert(population->genome_size == 0);
    assert(population->storage_bytes == 0);
    assert(population->evaluation_bytes == 0);
    assert(population->valid_count == 0);
    assert(population->best_index == 0);
    assert(population->produced_count == 0);
    assert(population->initialization_seed == 0);
    assert(population->source_generation == 0);
    assert(population->rng_algorithm_version == 0);
    assert(population->operator_seed_schedule_version == 0);
    assert(population->odd_child_policy_version == 0);
    assert(population->fitness_comparison_policy_version == 0);
    assert(!population->initialized);
    assert(!population->has_best);
    assert(!population->evaluated);
}

static void assert_population_evaluation_empty(
    const evo_population_t *population)
{
    assert(population->evaluations == NULL);
    assert(population->evaluation_bytes == 0);
    assert(population->valid_count == 0);
    assert(population->best_index == 0);
    assert(population->produced_count == 0);
    assert(population->source_generation == 0);
    assert(population->operator_seed_schedule_version == 0);
    assert(population->odd_child_policy_version == 0);
    assert(population->fitness_comparison_policy_version == 0);
    assert(!population->has_best);
    assert(!population->evaluated);
}

static void assert_child_evaluation_empty(
    const evo_population_t *population,
    uint64_t source_generation)
{
    assert(population->genomes != NULL);
    assert(population->evaluations == NULL);
    assert(population->evaluation_bytes == 0);
    assert(population->valid_count == 0);
    assert(population->best_index == 0);
    assert(population->produced_count == population->population_size);
    assert(population->initialization_seed == 0);
    assert(population->source_generation == source_generation);
    assert(population->rng_algorithm_version == 0);
    assert(population->operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(population->odd_child_policy_version == 0);
    assert(population->fitness_comparison_policy_version == 0);
    assert(!population->initialized);
    assert(!population->has_best);
    assert(!population->evaluated);
}

static void reset_allocation_injection(size_t failure_call)
{
    allocation_calls = 0;
    continuation_stop_calls = 0;
    fail_allocation_call = failure_call;
    observation_calls = 0;
    stop_calls = 0;
}

static void assert_run_allocation_failure(const evo_problem_t *problem,
                                          const evo_config_t *config,
                                          size_t failure_call)
{
    evo_result_t result = {0};

    reset_allocation_injection(failure_call);
    assert(evo_run(problem, config, NULL, &result) ==
           EVO_ERROR_OUT_OF_MEMORY);
    assert(allocation_calls == failure_call);
    assert(continuation_stop_calls == 0);
    assert(observation_calls == 0);
    fail_allocation_call = 0;
    assert_completely_empty(&result);
}

int main(void)
{
    evo_problem_t problem = {
        .genome_size = 32,
        .evaluate = deterministic_evaluator,
    };
    evo_config_t config = {
        .population_size = 10,
        .max_genome_bytes = 100,
        .max_population_bytes = 320,
        .max_evaluation_bytes =
            10 * sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes = 320,
        .max_diversity_work = SIZE_MAX,
        .generation_observer = count_observation,
        .generation_observer_context = &observation_calls,
    };
    evo_population_t population = {0};
    evo_population_t children = {0};
    evo_child_pair_evidence_t pair = {0};
    evo_child_evaluation_evidence_t child_evaluation = {0};
    evo_generation_advancement_evidence_t advancement = {0};
    evo_result_t result = {0};
    evo_config_t run_config = {0};

    reset_allocation_injection(0);
    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    assert(allocation_calls == 3);
    assert(observation_calls == 1);
    assert(result.termination_reason == EVO_TERMINATION_GENERATION_LIMIT);
    assert(result.generation_statistics.version ==
           EVO_GENERATION_STATISTICS_VERSION);
    assert(result.generation_statistics.generation_index == 0);
    assert(result.generation_statistics.population_size == 10);
    assert(result.generation_statistics.valid_count == 10);
    assert(result.generation_statistics.invalid_count == 0);
    assert(result.generation_statistics.best_index == 0);
    assert(result.generation_statistics.fitness_sums.total == 10.0);
    assert(result.generation_statistics.has_best);
    evo_result_destroy(&result);
    assert_completely_empty(&result);

    assert_run_allocation_failure(&problem, &config, 1);
    assert_run_allocation_failure(&problem, &config, 2);
    assert_run_allocation_failure(&problem, &config, 3);

    run_config = config;
    run_config.generation_limit = 2;
    run_config.tournament_size = 2;
    run_config.crossover_rate = 0.0;
    run_config.mutation_rate = 0.0;
    run_config.generation_stop = request_immediate_stop;
    run_config.generation_stop_context = &stop_calls;

    reset_allocation_injection(0);
    {
        const size_t releases_before_run = release_calls;

        assert(evo_run(&problem, &run_config, NULL, &result) ==
               EVO_SUCCESS);
        assert(allocation_calls == 3);
        assert(continuation_stop_calls == 0);
        assert(stop_calls == 1);
        assert(observation_calls == 1);
        assert(release_calls == releases_before_run + 2);
        assert(result.best_genome != NULL);
        assert(result.generations_completed == 0);
        assert(result.termination_reason ==
               EVO_TERMINATION_APPLICATION_REQUESTED);
        evo_result_destroy(&result);
        assert(release_calls == releases_before_run + 3);
        assert_completely_empty(&result);
    }

    run_config = config;
    run_config.generation_limit = 1;
    run_config.tournament_size = 2;
    run_config.crossover_rate = 0.0;
    run_config.mutation_rate = 0.0;
    run_config.generation_stop = continue_after_commit;
    run_config.generation_stop_context = &continuation_stop_calls;

    reset_allocation_injection(0);
    {
        const size_t releases_before_run = release_calls;

        assert(evo_run(&problem, &run_config, NULL, &result) ==
               EVO_SUCCESS);
        assert(allocation_calls == 5);
        assert(continuation_stop_calls == 1);
        assert(observation_calls == 2);
        assert(release_calls == releases_before_run + 4);
        assert(result.best_genome != NULL);
        assert(result.generations_completed == 1);
        assert(result.termination_reason ==
               EVO_TERMINATION_GENERATION_LIMIT);
        assert(result.generation_statistics.generation_index == 1);
        assert(result.generation_statistics.population_size == 10);
        assert(result.generation_statistics.valid_count == 10);
        assert(result.generation_statistics.fitness_sums.total == 10.0);
        evo_result_destroy(&result);
        assert(release_calls == releases_before_run + 5);
        assert_completely_empty(&result);
    }

    reset_allocation_injection(4);
    {
        const size_t releases_before_run = release_calls;

        assert(evo_run(&problem, &run_config, NULL, &result) ==
               EVO_ERROR_OUT_OF_MEMORY);
        assert(allocation_calls == 4);
        assert(continuation_stop_calls == 1);
        assert(observation_calls == 1);
        assert(release_calls == releases_before_run + 3);
        assert_completely_empty(&result);
    }

    reset_allocation_injection(5);
    {
        const size_t releases_before_run = release_calls;

        assert(evo_run(&problem, &run_config, NULL, &result) ==
               EVO_ERROR_OUT_OF_MEMORY);
        assert(allocation_calls == 5);
        assert(continuation_stop_calls == 1);
        assert(observation_calls == 1);
        assert(release_calls == releases_before_run + 4);
        assert_completely_empty(&result);
    }

    run_config.generation_limit = 2;
    reset_allocation_injection(6);
    {
        const size_t releases_before_run = release_calls;

        assert(evo_run(&problem, &run_config, NULL, &result) ==
               EVO_ERROR_OUT_OF_MEMORY);
        assert(allocation_calls == 6);
        assert(continuation_stop_calls == 2);
        assert(observation_calls == 2);
        assert(release_calls == releases_before_run + 5);
        assert_completely_empty(&result);
    }

    reset_allocation_injection(7);
    {
        const size_t releases_before_run = release_calls;

        assert(evo_run(&problem, &run_config, NULL, &result) ==
               EVO_ERROR_OUT_OF_MEMORY);
        assert(allocation_calls == 7);
        assert(continuation_stop_calls == 2);
        assert(observation_calls == 2);
        assert(release_calls == releases_before_run + 6);
        assert_completely_empty(&result);
    }
    fail_allocation_call = 0;

    reset_allocation_injection(1);
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_ERROR_OUT_OF_MEMORY);
    fail_allocation_call = 0;
    assert_population_empty(&population);

    reset_allocation_injection(0);
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_SUCCESS);
    assert(evo_population_initialize(&problem, &config, NULL, &population) ==
           EVO_SUCCESS);
    assert_population_evaluation_empty(&population);

    reset_allocation_injection(1);
    assert(evo_population_evaluate(&problem, &config, NULL, &population) ==
           EVO_ERROR_OUT_OF_MEMORY);
    fail_allocation_call = 0;
    assert_population_evaluation_empty(&population);
    assert(population.genomes != NULL);
    assert(population.initialized);

    reset_allocation_injection(0);
    assert(evo_population_evaluate(
               &problem, &config, NULL, &population) == EVO_SUCCESS);

    reset_allocation_injection(1);
    assert(evo_child_population_create(
               &problem, &config, &population, &children) ==
           EVO_ERROR_OUT_OF_MEMORY);
    assert(allocation_calls == 1);
    fail_allocation_call = 0;
    assert_population_empty(&children);
    assert(population.genomes != NULL);
    assert(population.evaluations != NULL);
    assert(population.initialized);
    assert(population.evaluated);

    config.tournament_size = 2;
    reset_allocation_injection(0);
    assert(evo_child_population_create(
               &problem, &config, &population, &children) == EVO_SUCCESS);
    assert(allocation_calls == 1);
    for (size_t pair_index = 0;
         pair_index < config.population_size / 2;
         ++pair_index) {
        assert(evo_child_pair_produce(&problem,
                                      &config,
                                      NULL,
                                      &population,
                                      0,
                                      pair_index,
                                      &children,
                                      &pair) == EVO_SUCCESS);
    }
    assert_child_evaluation_empty(&children, 0);

    reset_allocation_injection(1);
    assert(evo_child_population_evaluate(&problem,
                                         &config,
                                         NULL,
                                         0,
                                         &children,
                                         &child_evaluation) ==
           EVO_ERROR_OUT_OF_MEMORY);
    assert(allocation_calls == 1);
    fail_allocation_call = 0;
    assert_child_evaluation_empty(&children, 0);
    assert(child_evaluation.population_size == 0);
    assert(!child_evaluation.complete);

    reset_allocation_injection(0);
    assert(evo_child_population_evaluate(&problem,
                                         &config,
                                         NULL,
                                         0,
                                         &children,
                                         &child_evaluation) == EVO_SUCCESS);

    reset_allocation_injection(1);
    {
        const size_t releases_before_advancement = release_calls;

        assert(evo_population_advance_generation(&problem,
                                                 &config,
                                                 0,
                                                 &population,
                                                 &children,
                                                 &advancement) ==
               EVO_SUCCESS);
        assert(allocation_calls == 0);
        assert(release_calls == releases_before_advancement + 2);
    }
    fail_allocation_call = 0;
    assert_population_empty(&children);
    assert(advancement.completed_generation == 1);
    assert(advancement.complete);

    evo_population_destroy(&children);
    assert_population_empty(&children);
    evo_population_destroy(&population);
    assert_population_empty(&population);
    return 0;
}
