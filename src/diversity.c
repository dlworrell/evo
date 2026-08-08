#include "internal/diversity.h"

#include <math.h>
#include <stdint.h>

static bool checked_size_multiply(size_t left,
                                  size_t right,
                                  size_t *product)
{
    if (product == NULL || (left != 0 && right > SIZE_MAX / left)) {
        return false;
    }

    *product = left * right;
    return true;
}

static bool checked_unordered_pair_count(size_t count, size_t *pair_count)
{
    size_t left = count;
    size_t right = 0;

    if (pair_count == NULL) {
        return false;
    }

    if (count < 2) {
        *pair_count = 0;
        return true;
    }

    right = count - 1;
    if (left % 2 == 0) {
        left /= 2;
    } else {
        right /= 2;
    }

    return checked_size_multiply(left, right, pair_count);
}

static evo_status_t diversity_required_work(
    const evo_problem_t *problem,
    size_t candidate_count,
    size_t *pair_count,
    size_t *work_units)
{
    if (!checked_unordered_pair_count(candidate_count, pair_count)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (problem->genome_distance != NULL) {
        *work_units = *pair_count;
        return EVO_SUCCESS;
    }

    if (!checked_size_multiply(*pair_count,
                               problem->genome_size,
                               work_units)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    return EVO_SUCCESS;
}

evo_status_t evo_diversity_validate_config(
    const evo_problem_t *problem,
    const evo_config_t *config)
{
    size_t pair_count = 0;
    size_t required_work = 0;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if ((problem->genome_distance == NULL &&
         problem->genome_distance_version != UINT32_C(0)) ||
        (problem->genome_distance != NULL &&
         problem->genome_distance_version == UINT32_C(0))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = diversity_required_work(problem,
                                     config->population_size,
                                     &pair_count,
                                     &required_work);
    if (status != EVO_SUCCESS) {
        return status;
    }

    if (required_work > config->max_diversity_work) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    return EVO_SUCCESS;
}

static void byte_distance(const unsigned char *left,
                          const unsigned char *right,
                          size_t genome_size,
                          size_t *different_bytes)
{
    size_t differences = 0;

    for (size_t offset = 0; offset < genome_size; ++offset) {
        if (left[offset] != right[offset]) {
            ++differences;
        }
    }

    *different_bytes += differences;
}

evo_status_t evo_population_measure_diversity(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    evo_population_t *population)
{
    size_t pair_count = 0;
    size_t work_units = 0;
    size_t different_bytes = 0;
    size_t observed_pairs = 0;
    double distance_sum = 0.0;
    double diversity = 0.0;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || population == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    status = evo_diversity_validate_config(problem, config);
    if (status != EVO_SUCCESS) {
        return status;
    }

    if (population->genomes == NULL || population->evaluations == NULL ||
        !evo_population_secure_erasure_is_valid(config, population) ||
        !population->evaluated ||
        population->population_size != config->population_size ||
        population->genome_size != problem->genome_size ||
        population->valid_count > population->population_size ||
        population->diversity_policy_version != UINT32_C(0) ||
        population->diversity_metric_version != UINT32_C(0) ||
        population->diversity_pair_count != 0 ||
        population->diversity_work_units != 0 ||
        population->diversity != 0.0 ||
        population->diversity_uses_domain_distance) {
        return EVO_ERROR_STATE;
    }

    status = diversity_required_work(problem,
                                     population->valid_count,
                                     &pair_count,
                                     &work_units);
    if (status != EVO_SUCCESS || work_units > config->max_diversity_work) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    for (size_t left_index = 0;
         left_index < population->population_size;
         ++left_index) {
        const evo_candidate_evaluation_t *left_evaluation =
            &population->evaluations[left_index];
        const unsigned char *left_genome = NULL;

        if (!left_evaluation->valid) {
            continue;
        }

        if (!left_evaluation->evaluated) {
            return EVO_ERROR_STATE;
        }
        left_genome = evo_population_genome_const(population, left_index);
        if (left_genome == NULL) {
            return EVO_ERROR_STATE;
        }

        for (size_t right_index = left_index + 1;
             right_index < population->population_size;
             ++right_index) {
            const evo_candidate_evaluation_t *right_evaluation =
                &population->evaluations[right_index];
            const unsigned char *right_genome = NULL;
            double distance = 0.0;

            if (!right_evaluation->valid) {
                continue;
            }

            if (!right_evaluation->evaluated) {
                return EVO_ERROR_STATE;
            }
            right_genome = evo_population_genome_const(population,
                                                       right_index);
            if (right_genome == NULL) {
                return EVO_ERROR_STATE;
            }

            if (problem->genome_distance == NULL) {
                byte_distance(left_genome,
                              right_genome,
                              population->genome_size,
                              &different_bytes);
            } else {
                distance = problem->genome_distance(left_genome,
                                                    right_genome,
                                                    population->genome_size,
                                                    context);
                if (!isfinite(distance) || distance < 0.0 ||
                    distance > 1.0) {
                    return EVO_ERROR_EVALUATION;
                }
                distance_sum += distance;
                if (!isfinite(distance_sum)) {
                    return EVO_ERROR_EVALUATION;
                }
            }
            ++observed_pairs;
        }
    }

    if (observed_pairs != pair_count) {
        return EVO_ERROR_STATE;
    }

    if (pair_count != 0) {
        if (problem->genome_distance == NULL) {
            diversity = (double)different_bytes / (double)work_units;
        } else {
            diversity = distance_sum / (double)pair_count;
        }
    }

    if (!isfinite(diversity) || diversity < 0.0 || diversity > 1.0) {
        return EVO_ERROR_EVALUATION;
    }

    population->diversity_policy_version = EVO_DIVERSITY_POLICY_VERSION;
    population->diversity_metric_version =
        problem->genome_distance == NULL
            ? EVO_BYTE_DIVERSITY_METRIC_VERSION
            : problem->genome_distance_version;
    population->diversity_pair_count = pair_count;
    population->diversity_work_units = work_units;
    population->diversity = diversity;
    population->diversity_uses_domain_distance =
        problem->genome_distance != NULL;
    return EVO_SUCCESS;
}

bool evo_population_diversity_evidence_is_valid(
    const evo_config_t *config,
    const evo_population_t *population)
{
    size_t expected_pairs = 0;
    size_t expected_work = 0;
    size_t maximum_pairs = 0;
    size_t maximum_work = 0;

    if (config == NULL || population == NULL || !population->evaluated ||
        population->diversity_policy_version !=
            EVO_DIVERSITY_POLICY_VERSION ||
        population->diversity_metric_version == UINT32_C(0) ||
        !isfinite(population->diversity) ||
        population->diversity < 0.0 || population->diversity > 1.0 ||
        !checked_unordered_pair_count(population->valid_count,
                                      &expected_pairs) ||
        expected_pairs != population->diversity_pair_count) {
        return false;
    }

    if (!checked_unordered_pair_count(population->population_size,
                                      &maximum_pairs)) {
        return false;
    }

    if (population->diversity_uses_domain_distance) {
        expected_work = expected_pairs;
        maximum_work = maximum_pairs;
    } else {
        if (population->diversity_metric_version !=
                EVO_BYTE_DIVERSITY_METRIC_VERSION ||
            !checked_size_multiply(expected_pairs,
                                   population->genome_size,
                                   &expected_work) ||
            !checked_size_multiply(maximum_pairs,
                                   population->genome_size,
                                   &maximum_work)) {
            return false;
        }
    }

    return expected_work == population->diversity_work_units &&
           maximum_work <= config->max_diversity_work &&
           (expected_pairs != 0 || population->diversity == 0.0);
}
