#include "catalyst/evo/evo.h"
#include "internal/population_storage.h"

#include <assert.h>
#include <stddef.h>

void *__real_calloc(size_t count, size_t size);

static size_t allocation_calls;
static size_t fail_allocation_call;

void *__wrap_calloc(size_t count, size_t size)
{
    ++allocation_calls;
    if (fail_allocation_call != 0 &&
        allocation_calls == fail_allocation_call) {
        return NULL;
    }

    return __real_calloc(count, size);
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
}

static evo_fitness_t deterministic_evaluator(const void *genome,
                                             void *context)
{
    (void)genome;
    (void)context;
    return (evo_fitness_t){.total = 1.0};
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
    assert(!population->has_best);
    assert(!population->evaluated);
}

static void reset_allocation_injection(size_t failure_call)
{
    allocation_calls = 0;
    fail_allocation_call = failure_call;
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
    };
    evo_population_t population = {0};
    evo_population_t children = {0};
    evo_result_t result = {0};

    reset_allocation_injection(0);
    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    assert(allocation_calls == 3);
    evo_result_destroy(&result);
    assert_completely_empty(&result);

    assert_run_allocation_failure(&problem, &config, 1);
    assert_run_allocation_failure(&problem, &config, 2);
    assert_run_allocation_failure(&problem, &config, 3);

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

    evo_population_destroy(&population);
    assert_population_empty(&population);
    return 0;
}
