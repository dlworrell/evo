#include "internal/parent_pair.h"

#include "internal/selection.h"

evo_status_t evo_parent_pair_plan(
    const evo_config_t *config,
    const evo_population_t *parents,
    uint64_t source_generation,
    size_t pair_index,
    evo_parent_pair_t *pair)
{
    evo_parent_pair_t candidate = {0};
    evo_rng_t selection_rng = {0};
    size_t valid_count = 0;
    size_t pair_count = 0;
    uint64_t stream_index = 0;
    evo_status_t status = EVO_SUCCESS;

    if (config == NULL || parents == NULL || pair == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }

    if (config->population_size == 0 ||
        config->tournament_size == 0 ||
        config->tournament_size > config->population_size) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    pair_count = config->population_size / 2;
    if (pair_index >= pair_count) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    stream_index = (uint64_t)pair_index;
    if ((size_t)stream_index != pair_index) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }

    if (!evo_population_validate_completed(
            config, parents, &valid_count)) {
        return EVO_ERROR_STATE;
    }
    if (valid_count == 0) {
        return EVO_ERROR_NO_VALID_CANDIDATE;
    }

    if (!evo_rng_derive_operator_stream(
            &selection_rng,
            config->random_seed,
            source_generation,
            stream_index,
            EVO_OPERATOR_RNG_DOMAIN_SELECTION)) {
        return EVO_ERROR_STATE;
    }

    status = evo_population_select_tournament(
        config,
        parents,
        &selection_rng,
        &candidate.parent_a_index);
    if (status != EVO_SUCCESS) {
        return status;
    }

    status = evo_population_select_tournament(
        config,
        parents,
        &selection_rng,
        &candidate.parent_b_index);
    if (status != EVO_SUCCESS) {
        return status;
    }

    candidate.child_a_index = pair_index * 2;
    candidate.child_b_index = candidate.child_a_index + 1;
    candidate.pair_index = pair_index;
    candidate.source_generation = source_generation;
    candidate.seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;

    *pair = candidate;
    return EVO_SUCCESS;
}
