#include "internal/observer.h"

static void make_views(const evo_result_t *result,
                       evo_termination_reason_t termination_reason,
                       evo_generation_result_view_t *result_view,
                       evo_generation_statistics_t *statistics_view)
{
    *result_view = (evo_generation_result_view_t){
        .version = EVO_GENERATION_RESULT_VIEW_VERSION,
        .best_genome = result->best_genome,
        .best_genome_size = result->best_genome_size,
        .best_fitness = result->best_fitness,
        .generations_completed = result->generations_completed,
        .random_seed = result->random_seed,
        .termination_reason = termination_reason,
    };
    *statistics_view = result->generation_statistics;
}

evo_termination_reason_t evo_generation_callbacks_notify(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_result_t *result,
    evo_termination_reason_t natural_reason)
{
    evo_generation_result_view_t stop_result_view = {0};
    evo_generation_statistics_t stop_statistics_view = {0};
    evo_termination_reason_t final_reason = natural_reason;

    if (problem == NULL || config == NULL || result == NULL) {
        return natural_reason;
    }

    if (natural_reason == EVO_TERMINATION_NONE &&
        config->generation_stop != NULL) {
        make_views(result,
                   natural_reason,
                   &stop_result_view,
                   &stop_statistics_view);
        if (config->generation_stop(&stop_result_view,
                                    &stop_statistics_view,
                                    config->generation_stop_context)) {
            final_reason = EVO_TERMINATION_APPLICATION_REQUESTED;
        }
    }

    if (config->generation_observer != NULL) {
        evo_generation_result_view_t observer_result_view = {0};
        evo_generation_statistics_t observer_statistics_view = {0};

        make_views(result,
                   final_reason,
                   &observer_result_view,
                   &observer_statistics_view);
        config->generation_observer(&observer_result_view,
                                    &observer_statistics_view,
                                    config->generation_observer_context);
    }

    return final_reason;
}
