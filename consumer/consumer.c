#include <catalyst/evo/evo.h>

typedef struct observer_state {
    size_t calls;
    int valid;
} observer_state_t;

static evo_fitness_t evaluate_genome(const void *genome, void *context)
{
    (void)genome;
    (void)context;
    return (evo_fitness_t){
        .correctness = 1.0,
        .total = 1.0,
    };
}

static void observe_generation(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    observer_state_t *state = context;

    ++state->calls;
    if (result == NULL || statistics == NULL ||
        result->version != EVO_GENERATION_RESULT_VIEW_VERSION ||
        result->best_genome == NULL || result->best_genome_size != 8 ||
        result->best_fitness.total != 1.0 ||
        result->generations_completed != 0 ||
        result->termination_reason != EVO_TERMINATION_GENERATION_LIMIT ||
        statistics->version != EVO_GENERATION_STATISTICS_VERSION ||
        statistics->generation_index != 0 ||
        statistics->population_size != 1 ||
        statistics->valid_count != 1 ||
        statistics->fitness_sums.total != 1.0) {
        state->valid = 0;
    }
}

int main(void)
{
    observer_state_t observer = {
        .valid = 1,
    };
    const evo_problem_t problem = {
        .genome_size = 8,
        .evaluate = evaluate_genome,
    };
    const evo_config_t config = {
        .population_size = 1,
        .max_genome_bytes = 8,
        .max_population_bytes = 8,
        .max_evaluation_bytes = 1024,
        .generation_observer = observe_generation,
        .generation_observer_context = &observer,
    };
    evo_result_t result = {0};

    if (evo_run(&problem, &config, NULL, &result) != EVO_SUCCESS) {
        return 1;
    }

    if (result.best_genome == NULL ||
        result.best_fitness.correctness != 1.0 ||
        result.best_fitness.total != 1.0 ||
        result.termination_reason != EVO_TERMINATION_GENERATION_LIMIT ||
        result.generation_statistics.version !=
            EVO_GENERATION_STATISTICS_VERSION ||
        result.generation_statistics.generation_index != 0 ||
        result.generation_statistics.population_size != 1 ||
        result.generation_statistics.valid_count != 1 ||
        result.generation_statistics.invalid_count != 0 ||
        result.generation_statistics.best_index != 0 ||
        result.generation_statistics.best_fitness.total != 1.0 ||
        result.generation_statistics.fitness_sums.total != 1.0 ||
        !result.generation_statistics.has_best || observer.calls != 1 ||
        !observer.valid) {
        evo_result_destroy(&result);
        return 1;
    }

    evo_result_destroy(&result);
    return result.best_genome == NULL &&
                   result.termination_reason == EVO_TERMINATION_NONE &&
                   result.generation_statistics.version == 0
               ? 0
               : 1;
}
