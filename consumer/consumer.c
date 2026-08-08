#include <catalyst/evo/evo.h>

_Static_assert(EVO_FITNESS_COMPARISON_POLICY_VERSION == UINT32_C(1),
               "unsupported EVO fitness-comparison policy");
_Static_assert(EVO_STOPPING_POLICY_VERSION == UINT32_C(1),
               "unsupported EVO stopping policy");
_Static_assert(EVO_ELITE_POLICY_VERSION == UINT32_C(1),
               "unsupported EVO elite policy");
_Static_assert(EVO_SELECTION_POLICY_VERSION == UINT32_C(1),
               "unsupported EVO selection policy");
_Static_assert(EVO_BYTE_OPERATOR_POLICY_VERSION == UINT32_C(1),
               "unsupported EVO byte-operator policy");
_Static_assert(EVO_MUTATION_ADAPTATION_POLICY_VERSION == UINT32_C(1),
               "unsupported EVO mutation-adaptation policy");
_Static_assert(EVO_SECURE_ERASURE_POLICY_VERSION == UINT32_C(1),
               "unsupported EVO secure-erasure policy");
_Static_assert(EVO_POPULATION_RECYCLING_POLICY_VERSION == UINT32_C(1),
               "unsupported EVO population-recycling policy");
_Static_assert(EVO_POPULATION_STORAGE_REGISTRY_VERSION == UINT32_C(1),
               "unsupported EVO population-storage registry");
_Static_assert(EVO_POPULATION_STORAGE_OWNER_SLOTS == 2,
               "unsupported EVO population-storage slot count");
_Static_assert(EVO_PARALLEL_EVALUATION_POLICY_VERSION == UINT32_C(1),
               "unsupported EVO parallel-evaluation policy");
_Static_assert(EVO_EVALUATION_SCHEDULE_VERSION == UINT32_C(1),
               "unsupported EVO evaluation-schedule view");
_Static_assert(EVO_CHECKPOINT_FORMAT_VERSION == UINT32_C(3),
               "unsupported EVO checkpoint format");
_Static_assert(EVO_CHECKPOINT_VIEW_VERSION == UINT32_C(3),
               "unsupported EVO checkpoint view");
_Static_assert(EVO_CHECKPOINT_CONFIGURATION_VIEW_VERSION == UINT32_C(3),
               "unsupported EVO checkpoint configuration view");
_Static_assert(EVO_CHECKPOINT_CANDIDATE_VIEW_VERSION == UINT32_C(1),
               "unsupported EVO checkpoint candidate view");
_Static_assert(EVO_CHECKPOINT_INTEGRITY_CRC32 == UINT32_C(1),
               "unsupported EVO checkpoint integrity policy");

typedef struct callback_state {
    size_t observer_calls;
    size_t stop_calls;
    int observer_valid;
    int stop_valid;
} callback_state_t;

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
    callback_state_t *state = context;

    ++state->observer_calls;
    if (result == NULL || statistics == NULL ||
        result->version != EVO_GENERATION_RESULT_VIEW_VERSION ||
        result->best_genome == NULL || result->best_genome_size != 8 ||
        result->best_fitness.total != 1.0 ||
        result->generations_completed != 0 ||
        result->termination_reason != EVO_TERMINATION_CONVERGED ||
        statistics->version != EVO_GENERATION_STATISTICS_VERSION ||
        statistics->fitness_comparison_policy_version !=
            EVO_FITNESS_COMPARISON_POLICY_VERSION ||
        statistics->diversity_policy_version !=
            EVO_DIVERSITY_POLICY_VERSION ||
        statistics->diversity_metric_version !=
            EVO_BYTE_DIVERSITY_METRIC_VERSION ||
        statistics->diversity_pair_count != 0 ||
        statistics->diversity_work_units != 0 ||
        statistics->diversity != 0.0 ||
        statistics->diversity_uses_domain_distance ||
        statistics->adaptive_mutation_policy_version != UINT32_C(0) ||
        statistics->mutation_rate_prior != 0.0 ||
        statistics->mutation_rate_effective != 0.0 ||
        statistics->mutation_adaptation_reason !=
            EVO_MUTATION_ADAPTATION_NOT_APPLICABLE ||
        statistics->adaptive_mutation_enabled ||
        statistics->generation_index != 0 ||
        statistics->population_size != 1 ||
        statistics->valid_count != 1 ||
        statistics->fitness_sums.total != 1.0) {
        state->observer_valid = 0;
    }
}

static bool request_stop(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context)
{
    callback_state_t *state = context;

    ++state->stop_calls;
    if (result == NULL || statistics == NULL ||
        result->version != EVO_GENERATION_RESULT_VIEW_VERSION ||
        result->best_genome == NULL || result->best_genome_size != 8 ||
        result->best_fitness.total != 1.0 ||
        result->generations_completed != 0 ||
        result->termination_reason != EVO_TERMINATION_NONE ||
        statistics->version != EVO_GENERATION_STATISTICS_VERSION ||
        statistics->fitness_comparison_policy_version !=
            EVO_FITNESS_COMPARISON_POLICY_VERSION ||
        statistics->diversity_policy_version !=
            EVO_DIVERSITY_POLICY_VERSION ||
        statistics->diversity_metric_version !=
            EVO_BYTE_DIVERSITY_METRIC_VERSION ||
        statistics->diversity_pair_count != 0 ||
        statistics->diversity_work_units != 0 ||
        statistics->diversity != 0.0 ||
        statistics->diversity_uses_domain_distance ||
        statistics->adaptive_mutation_policy_version != UINT32_C(0) ||
        statistics->mutation_rate_prior != 0.0 ||
        statistics->mutation_rate_effective != 0.0 ||
        statistics->mutation_adaptation_reason !=
            EVO_MUTATION_ADAPTATION_NOT_APPLICABLE ||
        statistics->adaptive_mutation_enabled ||
        statistics->generation_index != 0 ||
        statistics->population_size != 1 ||
        statistics->valid_count != 1 ||
        statistics->fitness_sums.total != 1.0) {
        state->stop_valid = 0;
    }
    return true;
}

int main(void)
{
    callback_state_t callbacks = {
        .observer_valid = 1,
        .stop_valid = 1,
    };
    const evo_problem_t problem = {
        .genome_size = 8,
        .evaluate = evaluate_genome,
    };
    const evo_config_t config = {
        .population_size = 1,
        .generation_limit = 2,
        .max_genome_bytes = 8,
        .max_population_bytes = 8,
        .max_evaluation_bytes = 1024,
        .max_child_population_bytes = 8,
        .max_diversity_work = 0,
        .generation_observer = observe_generation,
        .generation_observer_context = &callbacks,
        .generation_stop = request_stop,
        .generation_stop_context = &callbacks,
        .fitness_target_enabled = true,
        .fitness_target = 1.0,
        .elite_count_enabled = true,
        .elite_count = 1,
    };
    evo_result_t result = {0};
    size_t checkpoint_size = 0;
    size_t evaluation_worker_scratch_size = 1;

    if (evo_evaluation_worker_scratch_size(
            config.population_size,
            0,
            &evaluation_worker_scratch_size) != EVO_SUCCESS ||
        evaluation_worker_scratch_size != 0 ||
        evo_checkpoint_size(&problem, &config, &checkpoint_size) !=
            EVO_SUCCESS ||
        checkpoint_size == 0) {
        return 1;
    }

    if (evo_run(&problem, &config, NULL, &result) != EVO_SUCCESS) {
        return 1;
    }

    if (result.best_genome == NULL ||
        result.best_genome_size != 8 ||
        result.secure_erasure_policy_version !=
            EVO_SECURE_ERASURE_POLICY_VERSION ||
        result.secure_erasure_backend !=
            EVO_SECURE_ERASURE_BACKEND_NONE ||
        result.secure_erasure_enabled ||
        result.best_fitness.correctness != 1.0 ||
        result.best_fitness.total != 1.0 ||
        result.termination_reason != EVO_TERMINATION_CONVERGED ||
        result.generation_statistics.version !=
            EVO_GENERATION_STATISTICS_VERSION ||
        result.generation_statistics.fitness_comparison_policy_version !=
            EVO_FITNESS_COMPARISON_POLICY_VERSION ||
        result.generation_statistics.diversity_policy_version !=
            EVO_DIVERSITY_POLICY_VERSION ||
        result.generation_statistics.diversity_metric_version !=
            EVO_BYTE_DIVERSITY_METRIC_VERSION ||
        result.generation_statistics.diversity_pair_count != 0 ||
        result.generation_statistics.diversity_work_units != 0 ||
        result.generation_statistics.diversity != 0.0 ||
        result.generation_statistics.diversity_uses_domain_distance ||
        result.generation_statistics.adaptive_mutation_policy_version !=
            UINT32_C(0) ||
        result.generation_statistics.mutation_rate_prior != 0.0 ||
        result.generation_statistics.mutation_rate_effective != 0.0 ||
        result.generation_statistics.mutation_adaptation_reason !=
            EVO_MUTATION_ADAPTATION_NOT_APPLICABLE ||
        result.generation_statistics.adaptive_mutation_enabled ||
        result.generation_statistics.generation_index != 0 ||
        result.generation_statistics.population_size != 1 ||
        result.generation_statistics.valid_count != 1 ||
        result.generation_statistics.invalid_count != 0 ||
        result.generation_statistics.best_index != 0 ||
        result.generation_statistics.best_fitness.total != 1.0 ||
        result.generation_statistics.fitness_sums.total != 1.0 ||
        !result.generation_statistics.has_best ||
        callbacks.stop_calls != 0 || callbacks.observer_calls != 1 ||
        !callbacks.observer_valid || !callbacks.stop_valid) {
        evo_result_destroy(&result);
        return 1;
    }

    evo_result_destroy(&result);
    return result.best_genome == NULL &&
                   result.best_genome_size == 0 &&
                   result.secure_erasure_policy_version == 0 &&
                   result.secure_erasure_backend ==
                       EVO_SECURE_ERASURE_BACKEND_NONE &&
                   !result.secure_erasure_enabled &&
                   result.termination_reason == EVO_TERMINATION_NONE &&
                   result.generation_statistics.version == 0
               ? 0
               : 1;
}
