#include "catalyst/evo/evo.h"
#include "internal/adaptive_mutation.h"
#include "internal/bounded_run.h"
#include "internal/observer.h"
#include "internal/population_storage.h"
#include "internal/secure_erasure.h"
#include "internal/statistics.h"
#include "internal/stopping.h"

#include <stdlib.h>

static evo_status_t destroy_population_with_status(
    evo_population_t *population,
    evo_status_t status)
{
    evo_population_destroy(population);
    return status;
}

static evo_status_t transfer_best_candidate(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *population,
    evo_result_t *result)
{
    const evo_candidate_evaluation_t *evaluation = NULL;
    const unsigned char *source = NULL;
    unsigned char *destination = NULL;
    size_t best_index = 0;

    if (population->valid_count == 0) {
        return EVO_ERROR_NO_VALID_CANDIDATE;
    }

    if (!evo_population_best_index(population, &best_index)) {
        return EVO_ERROR_STATE;
    }

    source = evo_population_genome_const(population, best_index);
    evaluation = evo_population_evaluation_const(population, best_index);
    if (source == NULL || evaluation == NULL || !evaluation->valid ||
        !evaluation->evaluated) {
        return EVO_ERROR_STATE;
    }

    destination = calloc(1, problem->genome_size);
    if (destination == NULL) {
        return EVO_ERROR_OUT_OF_MEMORY;
    }

    for (size_t index = 0; index < problem->genome_size; ++index) {
        destination[index] = source[index];
    }

    result->best_genome = destination;
    result->best_fitness = evaluation->fitness;
    result->generations_completed = 0;
    result->random_seed = config->random_seed;
    result->best_genome_size = problem->genome_size;
    result->secure_erasure_policy_version =
        EVO_SECURE_ERASURE_POLICY_VERSION;
    result->secure_erasure_backend =
        config->secure_erasure_enabled
            ? evo_secure_erasure_selected_backend()
            : EVO_SECURE_ERASURE_BACKEND_NONE;
    result->secure_erasure_enabled = config->secure_erasure_enabled;
    return EVO_SUCCESS;
}

evo_status_t evo_run(const evo_problem_t *problem, const evo_config_t *config, void *context, evo_result_t *result)
{
    evo_generation_statistics_t generation_statistics = {0};
    evo_adaptive_mutation_state_t adaptive_mutation_state = {0};
    evo_bounded_run_evidence_t run_evidence = {0};
    evo_population_t population = {0};
    evo_status_t status = EVO_SUCCESS;
    evo_termination_reason_t termination_reason = EVO_TERMINATION_NONE;

    if (result == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (result->best_genome != NULL) {
        return EVO_ERROR_RESULT_ACTIVE;
    }

    *result = (evo_result_t){0};

    if (problem == NULL || config == NULL || problem->evaluate == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = evo_bounded_run_validate_config(problem, config);
    if (status != EVO_SUCCESS) {
        return status;
    }

    status = evo_population_create(problem, config, &population);
    if (status != EVO_SUCCESS) {
        return status;
    }

    status = evo_population_initialize(problem, config, context, &population);
    if (status != EVO_SUCCESS) {
        return destroy_population_with_status(&population, status);
    }

    status = evo_population_evaluate(problem, config, context, &population);
    if (status != EVO_SUCCESS) {
        return destroy_population_with_status(&population, status);
    }

    status = evo_generation_statistics_record(config,
                                              &population,
                                              UINT64_C(0),
                                              &generation_statistics);
    if (status != EVO_SUCCESS) {
        return destroy_population_with_status(&population, status);
    }

    status = transfer_best_candidate(problem, config, &population, result);
    if (status != EVO_SUCCESS) {
        evo_population_destroy(&population);
        *result = (evo_result_t){0};
        return status;
    }
    if (evo_adaptive_mutation_is_applicable(config)) {
        status = evo_adaptive_mutation_initialize(
            config,
            &generation_statistics,
            &adaptive_mutation_state);
        if (status != EVO_SUCCESS) {
            evo_population_destroy(&population);
            evo_result_destroy(result);
            return EVO_ERROR_STATE;
        }
    }
    result->generation_statistics = generation_statistics;

    status = evo_stopping_classify_initial(
        config,
        result,
        config->generation_limit == 0,
        &termination_reason);
    if (status != EVO_SUCCESS) {
        evo_population_destroy(&population);
        evo_result_destroy(result);
        return EVO_ERROR_STATE;
    }
    termination_reason = evo_generation_callbacks_notify(problem,
                                                         config,
                                                         result,
                                                         termination_reason);

    if (config->generation_limit != 0 &&
        termination_reason == EVO_TERMINATION_NONE) {
        status = evo_bounded_run_advance(problem,
                                         config,
                                         context,
                                         &population,
                                         result,
                                         &run_evidence);
    }

    evo_population_destroy(&population);
    if (status != EVO_SUCCESS) {
        evo_result_destroy(result);
        return status;
    }

    if (termination_reason == EVO_TERMINATION_NONE) {
        termination_reason = run_evidence.termination_reason;
    }
    result->termination_reason = termination_reason;

    return EVO_SUCCESS;
}

void evo_result_destroy(evo_result_t *result)
{
    if (result == NULL) {
        return;
    }

    if (result->secure_erasure_enabled &&
        evo_secure_erasure_metadata_is_valid(
            result->secure_erasure_enabled,
            result->secure_erasure_policy_version,
            result->secure_erasure_backend) &&
        result->best_genome != NULL &&
        result->best_genome_size != 0) {
        evo_secure_erase(result->best_genome,
                         result->best_genome_size);
    }
    free(result->best_genome);
    *result = (evo_result_t){0};
}
