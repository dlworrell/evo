#include "catalyst/evo/evo.h"
#include "internal/adaptive_mutation.h"
#include "internal/bounded_run.h"
#include "internal/checkpoint.h"
#include "internal/observer.h"
#include "internal/population_recycling.h"
#include "internal/population_storage.h"
#include "internal/result_storage.h"
#include "internal/secure_erasure.h"
#include "internal/statistics.h"
#include "internal/stopping.h"

#include <stdint.h>
#include <stdlib.h>

static bool checkpoint_overlaps_result(const void *checkpoint,
                                       size_t checkpoint_size,
                                       const evo_result_t *result)
{
    const uintmax_t checkpoint_start =
        (uintmax_t)(uintptr_t)checkpoint;
    const uintmax_t result_start = (uintmax_t)(uintptr_t)result;
    uintmax_t checkpoint_end = 0;
    uintmax_t result_end = 0;

    if (checkpoint == NULL || result == NULL || checkpoint_size == 0) {
        return false;
    }
    if ((uintmax_t)checkpoint_size > UINTMAX_MAX - checkpoint_start ||
        sizeof(*result) > UINTMAX_MAX - result_start) {
        return true;
    }
    checkpoint_end = checkpoint_start + (uintmax_t)checkpoint_size;
    result_end = result_start + sizeof(*result);
    return checkpoint_start < result_end && result_start < checkpoint_end;
}

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

    {
        const evo_status_t allocation_status =
            evo_result_storage_allocate(problem, config, result);

        if (allocation_status != EVO_SUCCESS) {
            return allocation_status;
        }
    }
    destination = result->best_genome;

    for (size_t index = 0; index < problem->genome_size; ++index) {
        destination[index] = source[index];
    }

    result->best_fitness = evaluation->fitness;
    result->generations_completed = 0;
    result->random_seed = config->random_seed;
    return EVO_SUCCESS;
}

evo_status_t evo_result_storage_allocate(const evo_problem_t *problem,
                                         const evo_config_t *config,
                                         evo_result_t *result)
{
    void *allocation = NULL;

    if (problem == NULL || config == NULL || result == NULL ||
        result->best_genome != NULL || problem->genome_size == 0 ||
        problem->genome_size > config->max_genome_bytes) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    allocation = calloc(1, problem->genome_size);
    if (allocation == NULL) {
        return EVO_ERROR_OUT_OF_MEMORY;
    }
    result->best_genome = allocation;
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
    evo_run_state_t run_state = {0};
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
    status = evo_checkpoint_validate_config(problem, config, result);
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

    status = evo_run_state_initialize(problem,
                                      config,
                                      &population,
                                      result,
                                      &run_state);
    if (status != EVO_SUCCESS) {
        evo_population_destroy(&population);
        evo_result_destroy(result);
        return EVO_ERROR_STATE;
    }

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
    run_state.termination_reason = termination_reason;
    evo_population_storage_registry_notify(
        config,
        &run_state.population_storage_registry);
    status = evo_checkpoint_emit(problem,
                                 config,
                                 &population,
                                 result,
                                 &run_state);
    if (status != EVO_SUCCESS) {
        evo_population_destroy(&population);
        evo_result_destroy(result);
        return status;
    }

    if (config->generation_limit != 0 &&
        termination_reason == EVO_TERMINATION_NONE) {
        status = evo_bounded_run_continue(problem,
                                          config,
                                          context,
                                          &population,
                                          result,
                                          &run_state,
                                          &run_evidence);
    }

    evo_population_destroy(&population);
    if (status != EVO_SUCCESS) {
        evo_result_destroy(result);
        return status;
    }

    termination_reason = run_state.termination_reason;
    result->termination_reason = termination_reason;

    return EVO_SUCCESS;
}

evo_status_t evo_resume(const evo_problem_t *problem,
                        const evo_config_t *config,
                        void *context,
                        const void *checkpoint,
                        size_t checkpoint_size,
                        evo_result_t *result)
{
    evo_bounded_run_evidence_t run_evidence = {0};
    evo_population_t population = {0};
    evo_run_state_t run_state = {0};
    evo_status_t status = EVO_SUCCESS;

    if (result == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (result->best_genome != NULL) {
        return EVO_ERROR_RESULT_ACTIVE;
    }
    if (checkpoint_overlaps_result(checkpoint,
                                   checkpoint_size,
                                   result)) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    *result = (evo_result_t){0};
    if (problem == NULL || config == NULL || checkpoint == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = evo_checkpoint_restore(problem,
                                    config,
                                    checkpoint,
                                    checkpoint_size,
                                    &population,
                                    result,
                                    &run_state);
    if (status != EVO_SUCCESS) {
        return status;
    }
    if (run_state.termination_reason == EVO_TERMINATION_NONE) {
        status = evo_bounded_run_continue(problem,
                                          config,
                                          context,
                                          &population,
                                          result,
                                          &run_state,
                                          &run_evidence);
    }
    evo_population_destroy(&population);
    if (status != EVO_SUCCESS) {
        evo_result_destroy(result);
        return status;
    }
    result->termination_reason = run_state.termination_reason;
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
