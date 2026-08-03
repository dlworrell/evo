#include "internal/population_storage.h"
#include "internal/rng.h"

#include <stdint.h>

static bool population_matches_inputs(const evo_problem_t *problem,
                                      const evo_config_t *config,
                                      const evo_population_t *population)
{
    size_t expected_storage_bytes = 0;

    if (population->genomes == NULL || population->initialized ||
        population->initialization_seed != 0 ||
        population->rng_algorithm_version != 0 ||
        population->produced_count != 0 ||
        population->source_generation != 0 ||
        population->operator_seed_schedule_version != 0 ||
        population->odd_child_policy_version != 0 ||
        population->fitness_comparison_policy_version != 0 ||
        population->diversity_policy_version != 0 ||
        population->diversity_metric_version != 0 ||
        population->diversity_pair_count != 0 ||
        population->diversity_work_units != 0 ||
        population->diversity != 0.0 ||
        population->diversity_uses_domain_distance ||
        population->population_size == 0 || population->genome_size == 0 ||
        population->storage_bytes == 0 ||
        population->population_size != config->population_size ||
        population->genome_size != problem->genome_size ||
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

evo_status_t evo_population_initialize(const evo_problem_t *problem,
                                       const evo_config_t *config,
                                       void *context,
                                       evo_population_t *population)
{
    evo_rng_t rng = {0};

    if (problem == NULL || config == NULL || population == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (!population_matches_inputs(problem, config, population)) {
        return EVO_ERROR_STATE;
    }

    if (!evo_rng_seed(&rng, config->random_seed) ||
        !evo_rng_fill_bytes(&rng,
                            population->genomes,
                            population->storage_bytes)) {
        return EVO_ERROR_STATE;
    }

    if (problem->initialize != NULL) {
        for (size_t index = 0; index < population->population_size; ++index) {
            problem->initialize(
                population->genomes + index * population->genome_size,
                context);
        }
    }

    population->initialization_seed = config->random_seed;
    population->rng_algorithm_version = EVO_RNG_ALGORITHM_VERSION;
    population->initialized = true;
    return EVO_SUCCESS;
}
