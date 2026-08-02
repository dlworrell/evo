#include <catalyst/evo/evo.h>

static evo_fitness_t evaluate_genome(const void *genome, void *context)
{
    (void)genome;
    (void)context;
    return (evo_fitness_t){
        .correctness = 1.0,
        .total = 1.0,
    };
}

int main(void)
{
    const evo_problem_t problem = {
        .genome_size = 8,
        .evaluate = evaluate_genome,
    };
    const evo_config_t config = {
        .population_size = 1,
        .max_genome_bytes = 8,
        .max_population_bytes = 8,
        .max_evaluation_bytes = 1024,
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
        !result.generation_statistics.has_best) {
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
