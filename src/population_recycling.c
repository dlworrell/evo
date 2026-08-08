#include "internal/population_recycling.h"

#include "internal/secure_erasure.h"

#include <stdint.h>

static bool evaluation_storage_size(size_t population_size, size_t *bytes)
{
    if (bytes == NULL || population_size == 0 ||
        population_size > SIZE_MAX / sizeof(evo_candidate_evaluation_t)) {
        return false;
    }
    *bytes = population_size * sizeof(evo_candidate_evaluation_t);
    return true;
}

static bool recycling_metadata_is_canonical(
    const evo_config_t *config,
    const evo_population_t *population)
{
    if (config == NULL || population == NULL ||
        population->population_recycling_enabled !=
            config->population_recycling_enabled) {
        return false;
    }
    if (!config->population_recycling_enabled) {
        return population->population_recycling_policy_version == 0 &&
               population->storage_owner_identity == 0 &&
               population->reusable_evaluations == NULL &&
               population->reusable_evaluation_bytes == 0 &&
               !population->evaluations_recycled;
    }
    return population->population_recycling_policy_version ==
               EVO_POPULATION_RECYCLING_POLICY_VERSION &&
           (population->storage_owner_identity == UINT64_C(1) ||
            population->storage_owner_identity == UINT64_C(2));
}

bool evo_population_recycling_completed_is_valid(
    const evo_config_t *config,
    const evo_population_t *population)
{
    size_t expected_evaluation_bytes = 0;

    if (!recycling_metadata_is_canonical(config, population) ||
        population->evaluations == NULL ||
        !evaluation_storage_size(population->population_size,
                                 &expected_evaluation_bytes) ||
        population->evaluation_bytes != expected_evaluation_bytes) {
        return false;
    }
    return !config->population_recycling_enabled ||
           (population->reusable_evaluations == NULL &&
            population->reusable_evaluation_bytes == 0 &&
            !population->evaluations_recycled);
}

bool evo_population_recycling_child_is_valid(
    const evo_config_t *config,
    const evo_population_t *population)
{
    size_t expected_evaluation_bytes = 0;

    if (!recycling_metadata_is_canonical(config, population) ||
        population->evaluations != NULL || population->evaluation_bytes != 0 ||
        population->evaluations_recycled) {
        return false;
    }
    if (!config->population_recycling_enabled) {
        return true;
    }
    return evaluation_storage_size(population->population_size,
                                   &expected_evaluation_bytes) &&
           population->reusable_evaluations != NULL &&
           population->reusable_evaluation_bytes ==
               expected_evaluation_bytes;
}

static bool initial_recycling_owner_is_valid(
    const evo_config_t *config,
    const evo_population_t *population)
{
    if (!recycling_metadata_is_canonical(config, population) ||
        population->evaluations != NULL || population->evaluation_bytes != 0) {
        return false;
    }
    return !config->population_recycling_enabled ||
           (population->storage_owner_identity == UINT64_C(1) &&
            population->reusable_evaluations == NULL &&
            population->reusable_evaluation_bytes == 0 &&
            !population->evaluations_recycled);
}

bool evo_population_recycling_initial_is_valid(
    const evo_config_t *config,
    const evo_population_t *population)
{
    return initial_recycling_owner_is_valid(config, population);
}

static evo_population_storage_reset_disposition_t reset_disposition(
    const evo_config_t *config,
    size_t reset_count)
{
    if (reset_count == 0) {
        return EVO_POPULATION_STORAGE_RESET_NONE;
    }
    return config->secure_erasure_enabled
               ? EVO_POPULATION_STORAGE_RESET_SECURE_ERASE
               : EVO_POPULATION_STORAGE_RESET_ZERO_BYTES;
}

static void make_entry(const evo_config_t *config,
                       const evo_population_t *active,
                       uint64_t owner_identity,
                       uint64_t population_generation,
                       evo_population_storage_lifecycle_t lifecycle,
                       size_t handoff_count,
                       size_t reset_count,
                       evo_population_storage_entry_t *entry)
{
    *entry = (evo_population_storage_entry_t){
        .owner_identity = owner_identity,
        .lifecycle = lifecycle,
        .population_generation = population_generation,
        .source_generation = population_generation == UINT64_C(0)
                                 ? UINT64_C(0)
                                 : population_generation - UINT64_C(1),
        .genome_capacity_bytes = active->storage_bytes,
        .evaluation_capacity_bytes = active->evaluation_bytes,
        .handoff_count = handoff_count,
        .reset_count = reset_count,
        .genome_erasure_count =
            config->secure_erasure_enabled ? reset_count : 0,
        .evaluation_erasure_count =
            config->secure_erasure_enabled ? reset_count : 0,
        .last_reset_disposition = reset_disposition(config, reset_count),
        .genome_owner_present = true,
        .evaluation_owner_present = true,
    };
}

static bool make_registry(const evo_config_t *config,
                          const evo_population_t *active,
                          uint64_t current_generation,
                          evo_population_storage_registry_t *registry)
{
    evo_population_storage_registry_t candidate = {0};
    size_t generation = 0;
    size_t half = 0;
    size_t odd = 0;
    uint64_t active_identity = 0;
    uint64_t reusable_identity = 0;

    if (config == NULL || active == NULL || registry == NULL ||
        !evo_population_recycling_completed_is_valid(config, active)) {
        return false;
    }
#if SIZE_MAX < UINT64_MAX
    if (current_generation > (uint64_t)SIZE_MAX) {
        return false;
    }
#endif
    generation = (size_t)current_generation;
    candidate.version = EVO_POPULATION_STORAGE_REGISTRY_VERSION;
    candidate.policy_version = EVO_POPULATION_RECYCLING_POLICY_VERSION;
    candidate.recycling_enabled = config->population_recycling_enabled;
    candidate.secure_erasure_policy_version =
        EVO_SECURE_ERASURE_POLICY_VERSION;
    candidate.secure_erasure_backend = active->secure_erasure_backend;
    candidate.secure_erasure_enabled = config->secure_erasure_enabled;

    if (!config->population_recycling_enabled) {
        *registry = candidate;
        return true;
    }

    half = generation / 2;
    odd = generation % 2;
    active_identity = odd == 0 ? UINT64_C(1) : UINT64_C(2);
    reusable_identity = current_generation == UINT64_C(0)
                            ? UINT64_C(0)
                            : (active_identity == UINT64_C(1)
                                   ? UINT64_C(2)
                                   : UINT64_C(1));
    if (active->storage_owner_identity != active_identity ||
        (odd != 0 && half == SIZE_MAX)) {
        return false;
    }

    candidate.entry_count = current_generation == UINT64_C(0) ? 1 : 2;
    candidate.active_owner_identity = active_identity;
    candidate.reusable_owner_identity = reusable_identity;
    make_entry(config,
               active,
               UINT64_C(1),
               active_identity == UINT64_C(1)
                   ? current_generation
                   : current_generation - UINT64_C(1),
               active_identity == UINT64_C(1)
                   ? EVO_POPULATION_STORAGE_ACTIVE
                   : EVO_POPULATION_STORAGE_REUSABLE,
               half,
               half + odd,
               &candidate.entries[0]);
    if (current_generation != UINT64_C(0)) {
        make_entry(config,
                   active,
                   UINT64_C(2),
                   active_identity == UINT64_C(2)
                       ? current_generation
                       : current_generation - UINT64_C(1),
                   active_identity == UINT64_C(2)
                       ? EVO_POPULATION_STORAGE_ACTIVE
                       : EVO_POPULATION_STORAGE_REUSABLE,
                   half + odd,
                   half,
                   &candidate.entries[1]);
    }
    *registry = candidate;
    return true;
}

static bool entries_equal(const evo_population_storage_entry_t *left,
                          const evo_population_storage_entry_t *right)
{
    return left->owner_identity == right->owner_identity &&
           left->lifecycle == right->lifecycle &&
           left->population_generation == right->population_generation &&
           left->source_generation == right->source_generation &&
           left->genome_capacity_bytes == right->genome_capacity_bytes &&
           left->evaluation_capacity_bytes ==
               right->evaluation_capacity_bytes &&
           left->handoff_count == right->handoff_count &&
           left->reset_count == right->reset_count &&
           left->genome_erasure_count == right->genome_erasure_count &&
           left->evaluation_erasure_count ==
               right->evaluation_erasure_count &&
           left->last_reset_disposition ==
               right->last_reset_disposition &&
           left->genome_owner_present == right->genome_owner_present &&
           left->evaluation_owner_present ==
               right->evaluation_owner_present;
}

static bool registries_equal(
    const evo_population_storage_registry_t *left,
    const evo_population_storage_registry_t *right)
{
    if (left->version != right->version ||
        left->policy_version != right->policy_version ||
        left->recycling_enabled != right->recycling_enabled ||
        left->entry_count != right->entry_count ||
        left->active_owner_identity != right->active_owner_identity ||
        left->reusable_owner_identity != right->reusable_owner_identity ||
        left->secure_erasure_policy_version !=
            right->secure_erasure_policy_version ||
        left->secure_erasure_backend != right->secure_erasure_backend ||
        left->secure_erasure_enabled != right->secure_erasure_enabled) {
        return false;
    }
    for (size_t index = 0;
         index < EVO_POPULATION_STORAGE_OWNER_SLOTS;
         ++index) {
        if (!entries_equal(&left->entries[index], &right->entries[index])) {
            return false;
        }
    }
    return true;
}

evo_status_t evo_population_storage_registry_initialize(
    const evo_config_t *config,
    const evo_population_t *active,
    evo_population_storage_registry_t *registry)
{
    evo_population_storage_registry_t candidate = {0};

    if (config == NULL || active == NULL || registry == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (!make_registry(config, active, UINT64_C(0), &candidate)) {
        return EVO_ERROR_STATE;
    }
    *registry = candidate;
    return EVO_SUCCESS;
}

bool evo_population_storage_registry_is_valid(
    const evo_config_t *config,
    const evo_population_t *active,
    uint64_t current_generation,
    const evo_population_storage_registry_t *registry)
{
    evo_population_storage_registry_t expected = {0};

    return registry != NULL &&
           make_registry(config, active, current_generation, &expected) &&
           registries_equal(&expected, registry);
}

bool evo_population_storage_registry_prepare_transition(
    const evo_config_t *config,
    uint64_t current_generation,
    const evo_population_t *evaluated_children,
    evo_population_storage_registry_t *registry)
{
    if (current_generation == UINT64_MAX) {
        return false;
    }
    return make_registry(config,
                         evaluated_children,
                         current_generation + UINT64_C(1),
                         registry);
}

bool evo_population_reusable_reset_is_valid(
    const evo_config_t *config,
    const evo_population_t *population)
{
    return config != NULL && population != NULL &&
           config->population_recycling_enabled &&
           evo_population_recycling_completed_is_valid(config, population) &&
           population->genomes != NULL && population->storage_bytes != 0 &&
           population->evaluations != NULL &&
           population->evaluation_bytes != 0;
}

void evo_population_reset_for_reuse(const evo_config_t *config,
                                    evo_population_t *population)
{
    unsigned char *genomes = population->genomes;
    evo_candidate_evaluation_t *evaluations = population->evaluations;
    const size_t population_size = population->population_size;
    const size_t genome_size = population->genome_size;
    const size_t storage_bytes = population->storage_bytes;
    const size_t evaluation_bytes = population->evaluation_bytes;
    const uint32_t erasure_policy =
        population->secure_erasure_policy_version;
    const evo_secure_erasure_backend_t erasure_backend =
        population->secure_erasure_backend;
    const bool erasure_enabled = population->secure_erasure_enabled;
    const uint32_t recycling_policy =
        population->population_recycling_policy_version;
    const uint64_t owner_identity = population->storage_owner_identity;

    if (config->secure_erasure_enabled) {
        evo_secure_erase(evaluations, evaluation_bytes);
        evo_secure_erase(genomes, storage_bytes);
    } else {
        unsigned char *evaluation_bytes_view =
            (unsigned char *)evaluations;

        for (size_t index = 0; index < evaluation_bytes; ++index) {
            evaluation_bytes_view[index] = 0;
        }
        for (size_t index = 0; index < storage_bytes; ++index) {
            genomes[index] = 0;
        }
    }

    *population = (evo_population_t){
        .genomes = genomes,
        .reusable_evaluations = evaluations,
        .population_size = population_size,
        .genome_size = genome_size,
        .storage_bytes = storage_bytes,
        .reusable_evaluation_bytes = evaluation_bytes,
        .secure_erasure_policy_version = erasure_policy,
        .secure_erasure_backend = erasure_backend,
        .population_recycling_policy_version = recycling_policy,
        .storage_owner_identity = owner_identity,
        .secure_erasure_enabled = erasure_enabled,
        .population_recycling_enabled = true,
    };
}

void evo_population_storage_registry_notify(
    const evo_config_t *config,
    const evo_population_storage_registry_t *registry)
{
    if (config != NULL && registry != NULL &&
        config->population_storage_observer != NULL) {
        config->population_storage_observer(
            registry,
            config->population_storage_observer_context);
    }
}
