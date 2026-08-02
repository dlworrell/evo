#include "internal/observer.h"

void evo_generation_observer_notify(const evo_problem_t *problem,
                                    const evo_config_t *config,
                                    const evo_result_t *result,
                                    evo_termination_reason_t termination_reason)
{
    evo_generation_result_view_t result_view = {0};
    evo_generation_statistics_t statistics_view = {0};

    if (problem == NULL || config == NULL || result == NULL ||
        config->generation_observer == NULL) {
        return;
    }

    result_view.version = EVO_GENERATION_RESULT_VIEW_VERSION;
    result_view.best_genome = result->best_genome;
    result_view.best_genome_size = problem->genome_size;
    result_view.best_fitness = result->best_fitness;
    result_view.generations_completed = result->generations_completed;
    result_view.random_seed = result->random_seed;
    result_view.termination_reason = termination_reason;
    statistics_view = result->generation_statistics;

    config->generation_observer(&result_view,
                                &statistics_view,
                                config->generation_observer_context);
}
