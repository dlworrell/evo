#include "internal/population_evaluation.h"
#include "internal/diversity.h"
#include "internal/evaluation_workers.h"
#include "internal/fitness.h"
#include "internal/rng.h"
#include "internal/secure_erasure.h"

#include <stdint.h>
#include <stdlib.h>

static bool evaluation_storage_size(size_t population_size, size_t *bytes)
{
    if (bytes == NULL || population_size == 0 ||
        population_size > SIZE_MAX / sizeof(evo_candidate_evaluation_t)) {
        return false;
    }

    *bytes = population_size * sizeof(evo_candidate_evaluation_t);
    return true;
}

static evo_status_t allocate_evaluation_storage(
    const evo_config_t *config,
    size_t population_size,
    evo_candidate_evaluation_t **evaluations,
    size_t *evaluation_bytes)
{
    if (config == NULL || evaluations == NULL || evaluation_bytes == NULL ||
        *evaluations != NULL || *evaluation_bytes != 0 ||
        config->max_evaluation_bytes == 0 ||
        !evaluation_storage_size(population_size, evaluation_bytes) ||
        *evaluation_bytes > config->max_evaluation_bytes) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    *evaluations = calloc(1, *evaluation_bytes);
    if (*evaluations == NULL) {
        *evaluation_bytes = 0;
        return EVO_ERROR_OUT_OF_MEMORY;
    }
    return EVO_SUCCESS;
}

static bool initialized_population_ready_for_evaluation(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *population)
{
    size_t expected_storage_bytes = 0;

    if (population->genomes == NULL || population->evaluations != NULL ||
        !population->initialized || population->evaluated ||
        population->evaluation_bytes != 0 || population->valid_count != 0 ||
        population->best_index != 0 || population->has_best ||
        population->population_size == 0 || population->genome_size == 0 ||
        population->storage_bytes == 0 ||
        population->population_size != config->population_size ||
        population->genome_size != problem->genome_size ||
        population->initialization_seed != config->random_seed ||
        population->rng_algorithm_version != EVO_RNG_ALGORITHM_VERSION ||
        population->produced_count != 0 ||
        population->elite_count != 0 ||
        population->elite_source_valid_count != 0 ||
        population->source_generation != 0 ||
        population->operator_seed_schedule_version != 0 ||
        !evo_population_secure_erasure_is_valid(config, population) ||
        !evo_population_recycling_initial_is_valid(config, population) ||
        population->byte_operator_policy_version != 0 ||
        population->crossover_operator != EVO_CROSSOVER_CONSUMER ||
        population->mutation_operator != EVO_MUTATION_CONSUMER ||
        population->mutation_rate_used != 0.0 ||
        population->odd_child_policy_version != 0 ||
        population->elite_policy_version != 0 ||
        population->singleton_child_policy_version != 0 ||
        population->fitness_comparison_policy_version != 0 ||
        population->diversity_policy_version != 0 ||
        population->diversity_metric_version != 0 ||
        population->parallel_evaluation_policy_version != 0 ||
        population->evaluation_worker_count != 0 ||
        population->diversity_pair_count != 0 ||
        population->diversity_work_units != 0 ||
        population->diversity != 0.0 ||
        population->elite_count_explicit ||
        population->diversity_uses_domain_distance ||
        config->max_genome_bytes < population->genome_size ||
        config->max_population_bytes < population->storage_bytes ||
        population->population_size >
            SIZE_MAX / population->genome_size) {
        return false;
    }

    expected_storage_bytes =
        population->population_size * population->genome_size;
    return expected_storage_bytes == population->storage_bytes;
}

static void reset_evaluation_bytes(
    evo_candidate_evaluation_t *evaluations,
    size_t evaluation_bytes,
    bool secure_erasure_enabled)
{
    if (secure_erasure_enabled) {
        evo_secure_erase(evaluations, evaluation_bytes);
    } else {
        unsigned char *bytes = (unsigned char *)evaluations;

        for (size_t index = 0; index < evaluation_bytes; ++index) {
            bytes[index] = 0;
        }
    }
}

static evo_status_t discard_provisional_evaluations(
    evo_population_t *population,
    evo_candidate_evaluation_t *evaluations,
    size_t evaluation_bytes,
    bool reused_storage,
    evo_status_t status)
{
    if (reused_storage) {
        reset_evaluation_bytes(evaluations,
                               evaluation_bytes,
                               population->secure_erasure_enabled);
        population->reusable_evaluations = evaluations;
        population->reusable_evaluation_bytes = evaluation_bytes;
        population->evaluations_recycled = false;
        return status;
    }
    if (population->secure_erasure_enabled) {
        evo_secure_erase(evaluations, evaluation_bytes);
    }
    free(evaluations);
    return status;
}

static evo_status_t rollback_population_evaluations(
    evo_population_t *population,
    evo_status_t status)
{
    const bool preserve_for_reuse =
        population->population_recycling_enabled &&
        population->evaluations_recycled;

    if (preserve_for_reuse) {
        reset_evaluation_bytes(population->evaluations,
                               population->evaluation_bytes,
                               population->secure_erasure_enabled);
        population->reusable_evaluations = population->evaluations;
        population->reusable_evaluation_bytes =
            population->evaluation_bytes;
    } else if (population->secure_erasure_enabled &&
               evo_secure_erasure_metadata_is_valid(
                   population->secure_erasure_enabled,
                   population->secure_erasure_policy_version,
                   population->secure_erasure_backend)) {
        evo_secure_erase(population->evaluations,
                         population->evaluation_bytes);
    }
    if (!preserve_for_reuse) {
        free(population->evaluations);
    }
    population->evaluations = NULL;
    population->evaluation_bytes = 0;
    population->valid_count = 0;
    population->best_index = 0;
    population->fitness_comparison_policy_version = 0;
    population->diversity_policy_version = 0;
    population->diversity_metric_version = 0;
    population->parallel_evaluation_policy_version = 0;
    population->evaluation_worker_count = 0;
    population->diversity_pair_count = 0;
    population->diversity_work_units = 0;
    population->diversity = 0.0;
    population->has_best = false;
    population->evaluated = false;
    population->diversity_uses_domain_distance = false;
    population->evaluations_recycled = false;
    return status;
}

evo_status_t evo_population_evaluate_ready_with_worker_backend(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *population,
    const struct evo_evaluation_thread_backend *backend)
{
    evo_candidate_evaluation_t *evaluations = NULL;
    size_t evaluation_bytes = 0;
    size_t valid_count = 0;
    size_t best_index = 0;
    bool has_best = false;
    bool reused_storage = false;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || population == NULL ||
        problem->evaluate == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = evo_evaluation_workers_validate_config(problem, config);
    if (status != EVO_SUCCESS) {
        return status;
    }

    status = evo_diversity_validate_config(problem, config);
    if (status != EVO_SUCCESS) {
        return status;
    }

    if (population->reusable_evaluations != NULL) {
        if (!config->population_recycling_enabled ||
            !evo_population_recycling_child_is_valid(config, population)) {
            return EVO_ERROR_STATE;
        }
        evaluations = population->reusable_evaluations;
        evaluation_bytes = population->reusable_evaluation_bytes;
        population->reusable_evaluations = NULL;
        population->reusable_evaluation_bytes = 0;
        reused_storage = true;
    } else {
        status = allocate_evaluation_storage(config,
                                             population->population_size,
                                             &evaluations,
                                             &evaluation_bytes);
        if (status != EVO_SUCCESS) {
            return status;
        }
    }

    if (config->evaluation_worker_count == 0) {
        for (size_t index = 0;
             index < population->population_size;
             ++index) {
            const void *genome =
                evo_population_genome_const(population, index);

            if (genome == NULL) {
                return discard_provisional_evaluations(
                    population,
                    evaluations,
                    evaluation_bytes,
                    reused_storage,
                    EVO_ERROR_STATE);
            }

            evaluations[index].valid =
                problem->is_valid == NULL ||
                problem->is_valid(genome, context);
            if (evaluations[index].valid) {
                ++valid_count;
            }
        }

        for (size_t index = 0;
             index < population->population_size;
             ++index) {
            const void *genome = NULL;
            evo_fitness_candidate_view_t candidate_view = {0};
            evo_fitness_candidate_view_t best_view = {0};
            evo_fitness_order_t order = EVO_FITNESS_ORDER_EQUAL;

            if (!evaluations[index].valid) {
                continue;
            }

            genome = evo_population_genome_const(population, index);
            if (genome == NULL) {
                return discard_provisional_evaluations(
                    population,
                    evaluations,
                    evaluation_bytes,
                    reused_storage,
                    EVO_ERROR_STATE);
            }

            evaluations[index].fitness =
                problem->evaluate(genome, context);
            evaluations[index].evaluated = true;
            candidate_view = (evo_fitness_candidate_view_t){
                .fitness = &evaluations[index].fitness,
                .generation = UINT64_C(0),
                .population_index = index,
                .hard_valid = evaluations[index].valid,
                .evaluated = evaluations[index].evaluated,
            };
            if (!evo_fitness_candidate_is_rankable(&candidate_view)) {
                return discard_provisional_evaluations(
                    population,
                    evaluations,
                    evaluation_bytes,
                    reused_storage,
                    EVO_ERROR_EVALUATION);
            }

            if (!has_best) {
                best_index = index;
                has_best = true;
                continue;
            }

            best_view = (evo_fitness_candidate_view_t){
                .fitness = &evaluations[best_index].fitness,
                .generation = UINT64_C(0),
                .population_index = best_index,
                .hard_valid = evaluations[best_index].valid,
                .evaluated = evaluations[best_index].evaluated,
            };
            if (!evo_fitness_compare_candidates(&candidate_view,
                                                &best_view,
                                                &order)) {
                return discard_provisional_evaluations(
                    population,
                    evaluations,
                    evaluation_bytes,
                    reused_storage,
                    EVO_ERROR_EVALUATION);
            }
            if (order == EVO_FITNESS_ORDER_LEFT) {
                best_index = index;
            }
        }
    } else {
        status = evo_evaluation_workers_run(
            problem,
            config,
            context,
            population,
            evaluations,
            &valid_count,
            &best_index,
            &has_best,
            backend);
        if (status != EVO_SUCCESS) {
            return discard_provisional_evaluations(population,
                                                   evaluations,
                                                   evaluation_bytes,
                                                   reused_storage,
                                                   status);
        }
    }

    population->evaluations = evaluations;
    population->evaluation_bytes = evaluation_bytes;
    population->valid_count = valid_count;
    population->best_index = best_index;
    population->fitness_comparison_policy_version =
        EVO_FITNESS_COMPARISON_POLICY_VERSION;
    population->parallel_evaluation_policy_version =
        config->evaluation_worker_count == 0
            ? 0
            : EVO_PARALLEL_EVALUATION_POLICY_VERSION;
    population->evaluation_worker_count =
        config->evaluation_worker_count;
    population->has_best = has_best;
    population->evaluated = true;
    population->evaluations_recycled = reused_storage;
    {
        const evo_status_t diversity_status =
            evo_population_measure_diversity(problem,
                                             config,
                                             context,
                                             population);

        if (diversity_status != EVO_SUCCESS) {
            return rollback_population_evaluations(population,
                                                   diversity_status);
        }
    }
    population->evaluations_recycled = false;

    return EVO_SUCCESS;
}

evo_status_t evo_population_evaluate_ready(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *population)
{
    return evo_population_evaluate_ready_with_worker_backend(
        problem,
        config,
        context,
        population,
        NULL);
}

evo_status_t evo_population_restore_evaluations_allocate(
    const evo_config_t *config,
    evo_population_t *population)
{
    evo_candidate_evaluation_t *evaluations = NULL;
    size_t evaluation_bytes = 0;
    evo_status_t status = EVO_SUCCESS;

    if (config == NULL || population == NULL || population->genomes == NULL ||
        population->population_size != config->population_size ||
        population->evaluations != NULL || population->evaluation_bytes != 0 ||
        population->reusable_evaluations != NULL ||
        population->reusable_evaluation_bytes != 0 ||
        population->evaluated) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    status = allocate_evaluation_storage(config,
                                         population->population_size,
                                         &evaluations,
                                         &evaluation_bytes);
    if (status != EVO_SUCCESS) {
        return status;
    }
    population->evaluations = evaluations;
    population->evaluation_bytes = evaluation_bytes;
    return EVO_SUCCESS;
}

evo_status_t evo_population_reusable_evaluations_allocate(
    const evo_config_t *config,
    evo_population_t *population)
{
    evo_candidate_evaluation_t *evaluations = NULL;
    size_t evaluation_bytes = 0;
    evo_status_t status = EVO_SUCCESS;

    if (config == NULL || population == NULL ||
        !config->population_recycling_enabled ||
        population->genomes == NULL ||
        population->population_size != config->population_size ||
        population->evaluations != NULL || population->evaluation_bytes != 0 ||
        population->reusable_evaluations != NULL ||
        population->reusable_evaluation_bytes != 0 ||
        population->evaluated ||
        population->population_recycling_policy_version !=
            EVO_POPULATION_RECYCLING_POLICY_VERSION ||
        (population->storage_owner_identity != UINT64_C(1) &&
         population->storage_owner_identity != UINT64_C(2))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    status = allocate_evaluation_storage(config,
                                         population->population_size,
                                         &evaluations,
                                         &evaluation_bytes);
    if (status != EVO_SUCCESS) {
        return status;
    }
    population->reusable_evaluations = evaluations;
    population->reusable_evaluation_bytes = evaluation_bytes;
    return EVO_SUCCESS;
}

evo_status_t evo_population_evaluate(const evo_problem_t *problem,
                                     const evo_config_t *config,
                                     void *context,
                                     evo_population_t *population)
{
    if (problem == NULL || config == NULL || population == NULL ||
        problem->evaluate == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!initialized_population_ready_for_evaluation(problem,
                                                     config,
                                                     population)) {
        return EVO_ERROR_STATE;
    }

    return evo_population_evaluate_ready(problem,
                                         config,
                                         context,
                                         population);
}

const evo_candidate_evaluation_t *
evo_population_evaluation_const(const evo_population_t *population,
                                size_t index)
{
    if (population == NULL || population->evaluations == NULL ||
        !population->evaluated || index >= population->population_size) {
        return NULL;
    }

    return &population->evaluations[index];
}

bool evo_population_best_index(const evo_population_t *population,
                               size_t *best_index)
{
    if (population == NULL || best_index == NULL ||
        population->evaluations == NULL || !population->evaluated ||
        !population->has_best ||
        population->best_index >= population->population_size) {
        return false;
    }

    *best_index = population->best_index;
    return true;
}
