#include "internal/project_search_internal.h"

#include "internal/project_candidate_internal.h"
#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <math.h>
#include <string.h>

static size_t evo_search_bounded_length(const char *value, size_t maximum)
{
    size_t length = 0U;

    if (value == NULL) {
        return SIZE_MAX;
    }
    while (length <= maximum && value[length] != '\0') {
        length += 1U;
    }
    return length <= maximum ? length : SIZE_MAX;
}

bool evo_search_copy_text(
    char *destination,
    size_t destination_size,
    const char *source)
{
    size_t index = 0U;

    if (destination == NULL || destination_size == 0U || source == NULL) {
        return false;
    }
    while (source[index] != '\0') {
        if (index + 1U >= destination_size) {
            return false;
        }
        destination[index] = source[index];
        index += 1U;
    }
    destination[index] = '\0';
    return true;
}

char *evo_search_duplicate(const char *value, size_t maximum_bytes)
{
    const size_t length = evo_search_bounded_length(value, maximum_bytes);
    char *copy;
    size_t index;

    if (length == SIZE_MAX || length == 0U || length == SIZE_MAX - 1U) {
        return NULL;
    }
    copy = evo_project_allocate_zeroed(length + 1U, sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }
    for (index = 0U; index < length; index += 1U) {
        copy[index] = value[index];
    }
    copy[length] = '\0';
    return copy;
}

static bool evo_search_multiply_size(size_t left, size_t right, size_t *value)
{
    if (value == NULL || (right != 0U && left > SIZE_MAX / right)) {
        return false;
    }
    *value = left * right;
    return true;
}

bool evo_search_config_valid(const evo_project_search_config_t *config)
{
    size_t lineage_count = 0U;
    size_t population_bytes = 0U;

    if (config == NULL || config->recipe_context.baseline == NULL ||
        config->recipe_context.analysis == NULL ||
        config->recipe_context.catalogue == NULL ||
        config->recipe_context.baseline->private_owner == NULL ||
        config->recipe_context.analysis->private_owner == NULL ||
        config->recipe_context.catalogue->entries == NULL ||
        config->recipe_context.catalogue->entry_count == 0U ||
        config->recipe_context.analysis->opportunities == NULL ||
        config->recipe_context.analysis->opportunity_count == 0U ||
        config->genome_size <= EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE ||
        config->genome_size > config->recipe_context.limits.max_genome_bytes ||
        config->population_size == 0U || config->tournament_size == 0U ||
        config->tournament_size > config->population_size ||
        !isfinite(config->crossover_rate) || config->crossover_rate < 0.0 ||
        config->crossover_rate > 1.0 || !isfinite(config->mutation_rate) ||
        config->mutation_rate < 0.0 || config->mutation_rate > 1.0 ||
        config->max_core_population_bytes == 0U ||
        config->max_core_evaluation_bytes == 0U ||
        config->max_core_child_population_bytes == 0U ||
        config->max_core_diversity_work == 0U ||
        config->evaluation_provider_identity == NULL ||
        config->evaluation_provider == NULL ||
        config->policy.schema_version != EVO_PROJECT_SEARCH_SCHEMA_VERSION ||
        config->policy.identity == NULL ||
        config->policy.mutation_policy_version !=
            EVO_PROJECT_SEARCH_MUTATION_POLICY_VERSION ||
        config->policy.crossover_policy_version !=
            EVO_PROJECT_SEARCH_CROSSOVER_POLICY_VERSION ||
        config->policy.repair_policy_version !=
            EVO_PROJECT_SEARCH_REPAIR_POLICY_VERSION ||
        config->policy.initial_record_count == 0U ||
        config->policy.maximum_record_count == 0U ||
        config->policy.initial_record_count >
            config->policy.maximum_record_count ||
        config->policy.maximum_record_count > config->limits.max_records ||
        config->policy.maximum_record_count >
            config->recipe_context.limits.max_records ||
        config->policy.mutation_operation_mask == 0U ||
        (config->policy.mutation_operation_mask &
         ~EVO_PROJECT_SEARCH_MUTATION_ALL) != 0U ||
        config->policy.max_mutations_per_event == 0U ||
        config->policy.max_mutations_per_event >
            config->limits.max_mutations_per_event ||
        config->policy.max_repair_passes == 0U ||
        config->policy.max_repair_passes > config->limits.max_repair_passes ||
        config->limits.max_string_bytes == 0U ||
        config->limits.max_records == 0U ||
        config->limits.max_parameters_per_record == 0U ||
        config->limits.max_mutations_per_event == 0U ||
        config->limits.max_repair_passes == 0U ||
        config->limits.max_lineage_records == 0U ||
        config->limits.max_operator_events == 0U ||
        config->limits.max_evidence_bytes == 0U ||
        config->limits.max_total_bytes == 0U ||
        evo_search_bounded_length(
            config->policy.identity, config->limits.max_string_bytes) ==
            SIZE_MAX ||
        evo_search_bounded_length(
            config->evaluation_provider_identity,
            config->limits.max_string_bytes) == SIZE_MAX ||
        config->recipe_context.catalogue->identity == NULL ||
        evo_search_bounded_length(
            config->recipe_context.catalogue->identity,
            config->limits.max_string_bytes) == SIZE_MAX) {
        return false;
    }
    if (config->generation_limit == SIZE_MAX ||
        !evo_search_multiply_size(
            config->generation_limit + 1U,
            config->population_size,
            &lineage_count) ||
        lineage_count > config->limits.max_lineage_records ||
        lineage_count > config->limits.max_operator_events ||
        !evo_search_multiply_size(
            config->population_size, config->genome_size, &population_bytes) ||
        population_bytes > config->max_core_population_bytes ||
        population_bytes > config->max_core_child_population_bytes) {
        return false;
    }
    return true;
}

bool evo_search_fitness_valid(const evo_fitness_t *fitness)
{
    return fitness != NULL && isfinite(fitness->correctness) &&
           isfinite(fitness->performance) && isfinite(fitness->memory_use) &&
           isfinite(fitness->reliability) &&
           isfinite(fitness->maintainability) &&
           isfinite(fitness->constraint_penalty) &&
           fitness->constraint_penalty >= 0.0 && isfinite(fitness->total);
}

bool evo_search_fitness_equal(
    const evo_fitness_t *left,
    const evo_fitness_t *right)
{
    return left != NULL && right != NULL &&
           left->correctness == right->correctness &&
           left->performance == right->performance &&
           left->memory_use == right->memory_use &&
           left->reliability == right->reliability &&
           left->maintainability == right->maintainability &&
           left->constraint_penalty == right->constraint_penalty &&
           left->total == right->total;
}

uint64_t evo_search_selector(
    const evo_project_search_config_t *config,
    const char *domain,
    size_t ordinal,
    const unsigned char *first,
    size_t first_size,
    const unsigned char *second,
    size_t second_size)
{
    evo_project_fingerprint_t fingerprint;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(&fingerprint, "evo-project-search-selector-v1");
    evo_project_fingerprint_string(&fingerprint, domain);
    evo_project_fingerprint_u64(&fingerprint, config->random_seed);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)ordinal);
    evo_project_fingerprint_string(&fingerprint, config->policy.identity);
    if (first != NULL && first_size > 0U) {
        evo_project_fingerprint_bytes(&fingerprint, first, first_size);
    }
    if (second != NULL && second_size > 0U) {
        evo_project_fingerprint_bytes(&fingerprint, second, second_size);
    }
    return fingerprint.value;
}

void evo_search_zero_genome(unsigned char *genome, size_t genome_size)
{
    size_t index;

    if (genome == NULL) {
        return;
    }
    for (index = 0U; index < genome_size; index += 1U) {
        genome[index] = 0U;
    }
}

bool evo_search_copy_genome(
    unsigned char *destination,
    const unsigned char *source,
    size_t genome_size)
{
    size_t index;

    if (destination == NULL || source == NULL || destination == source) {
        return false;
    }
    for (index = 0U; index < genome_size; index += 1U) {
        destination[index] = source[index];
    }
    return true;
}

static evo_project_recipe_parameter_value_t *evo_search_parameters(
    evo_project_search_mutable_recipe_t *mutable_recipe,
    size_t record_index)
{
    return mutable_recipe->parameter_storage +
           record_index * mutable_recipe->parameter_capacity;
}

static const evo_project_recipe_parameter_value_t *evo_search_parameters_const(
    const evo_project_search_mutable_recipe_t *mutable_recipe,
    size_t record_index)
{
    return mutable_recipe->parameter_storage +
           record_index * mutable_recipe->parameter_capacity;
}

bool evo_search_mutable_open(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe)
{
    size_t parameter_count = 0U;

    if (config == NULL || mutable_recipe == NULL ||
        mutable_recipe->records != NULL || mutable_recipe->parameter_storage != NULL ||
        mutable_recipe->proposals != NULL ||
        !evo_search_multiply_size(
            config->policy.maximum_record_count,
            config->limits.max_parameters_per_record,
            &parameter_count)) {
        return false;
    }
    mutable_recipe->records = evo_project_allocate_zeroed(
        config->policy.maximum_record_count, sizeof(*mutable_recipe->records));
    mutable_recipe->parameter_storage = evo_project_allocate_zeroed(
        parameter_count, sizeof(*mutable_recipe->parameter_storage));
    mutable_recipe->proposals = evo_project_allocate_zeroed(
        config->policy.maximum_record_count, sizeof(*mutable_recipe->proposals));
    if (mutable_recipe->records == NULL ||
        mutable_recipe->parameter_storage == NULL ||
        mutable_recipe->proposals == NULL) {
        evo_search_mutable_close(mutable_recipe);
        return false;
    }
    mutable_recipe->capacity = config->policy.maximum_record_count;
    mutable_recipe->parameter_capacity =
        config->limits.max_parameters_per_record;
    return true;
}

void evo_search_mutable_close(
    evo_project_search_mutable_recipe_t *mutable_recipe)
{
    if (mutable_recipe == NULL) {
        return;
    }
    evo_project_release(mutable_recipe->records);
    evo_project_release(mutable_recipe->parameter_storage);
    evo_project_release(mutable_recipe->proposals);
    *mutable_recipe = (evo_project_search_mutable_recipe_t){0};
}

static bool evo_search_record_set(
    evo_project_search_mutable_recipe_t *mutable_recipe,
    size_t record_index,
    const char *target_identity,
    const char *transformation_identity,
    uint32_t transformation_version,
    size_t parameter_count,
    const evo_project_recipe_parameter_value_t *parameters)
{
    evo_project_recipe_parameter_value_t *destination;
    size_t index;

    if (mutable_recipe == NULL || record_index >= mutable_recipe->capacity ||
        target_identity == NULL || transformation_identity == NULL ||
        parameter_count > mutable_recipe->parameter_capacity ||
        (parameter_count > 0U && parameters == NULL)) {
        return false;
    }
    destination = evo_search_parameters(mutable_recipe, record_index);
    mutable_recipe->records[record_index].target_location_identity =
        target_identity;
    mutable_recipe->records[record_index].transformation_identity =
        transformation_identity;
    mutable_recipe->records[record_index].transformation_version =
        transformation_version;
    mutable_recipe->records[record_index].parameter_count = parameter_count;
    for (index = 0U; index < mutable_recipe->parameter_capacity; index += 1U) {
        destination[index] = (evo_project_recipe_parameter_value_t){0};
    }
    for (index = 0U; index < parameter_count; index += 1U) {
        destination[index] = parameters[index];
    }
    return true;
}

static bool evo_search_record_copy(
    evo_project_search_mutable_recipe_t *destination,
    size_t destination_index,
    const evo_project_search_mutable_recipe_t *source,
    size_t source_index)
{
    const evo_project_search_mutable_record_t *record;

    if (destination == NULL || source == NULL ||
        source_index >= source->record_count) {
        return false;
    }
    record = &source->records[source_index];
    return evo_search_record_set(
        destination,
        destination_index,
        record->target_location_identity,
        record->transformation_identity,
        record->transformation_version,
        record->parameter_count,
        evo_search_parameters_const(source, source_index));
}

bool evo_search_mutable_from_recipe(
    const evo_project_search_config_t *config,
    const evo_project_recipe_t *recipe,
    evo_project_search_mutable_recipe_t *mutable_recipe)
{
    size_t index;

    if (config == NULL || recipe == NULL || mutable_recipe == NULL ||
        recipe->private_owner == NULL || recipe->record_count > mutable_recipe->capacity) {
        return false;
    }
    for (index = 0U; index < recipe->record_count; index += 1U) {
        const evo_project_recipe_record_t *record = &recipe->records[index];

        if (!evo_search_record_set(
                mutable_recipe,
                index,
                record->target.location_identity,
                record->transformation_identity,
                record->transformation_version,
                record->parameter_count,
                record->parameters)) {
            return false;
        }
    }
    mutable_recipe->record_count = recipe->record_count;
    return evo_search_mutable_normalize(mutable_recipe);
}

bool evo_search_mutable_normalize(
    evo_project_search_mutable_recipe_t *mutable_recipe)
{
    size_t index;

    if (mutable_recipe == NULL) {
        return false;
    }
    for (index = 0U; index < mutable_recipe->record_count; index += 1U) {
        const int written = evo_project_format(
            mutable_recipe->records[index].identity,
            sizeof(mutable_recipe->records[index].identity),
            "record-%06zu",
            index + 1U);

        if (written <= 0 ||
            (size_t)written >= sizeof(mutable_recipe->records[index].identity)) {
            return false;
        }
    }
    return true;
}

static const evo_project_transformation_catalogue_entry_t *
evo_search_catalogue_entry(
    const evo_project_search_config_t *config,
    const char *identity,
    uint32_t version)
{
    size_t index;

    for (index = 0U; index < config->recipe_context.catalogue->entry_count; index += 1U) {
        const evo_project_transformation_catalogue_entry_t *entry =
            &config->recipe_context.catalogue->entries[index];

        if (entry->implementation_version == version &&
            strcmp(entry->identity, identity) == 0) {
            return entry;
        }
    }
    return NULL;
}

static const evo_project_source_location_record_t *evo_search_location(
    const evo_project_search_config_t *config,
    const char *identity)
{
    size_t index;

    for (index = 0U; index < config->recipe_context.analysis->source_location_count; index += 1U) {
        const evo_project_source_location_record_t *location =
            &config->recipe_context.analysis->source_locations[index];

        if (strcmp(location->identity, identity) == 0) {
            return location;
        }
    }
    return NULL;
}

static uint32_t evo_search_location_mask(evo_project_source_location_kind_t kind)
{
    switch (kind) {
    case EVO_PROJECT_LOCATION_SPELLING:
        return EVO_PROJECT_RECIPE_LOCATION_SPELLING;
    case EVO_PROJECT_LOCATION_MACRO_EXPANSION:
        return EVO_PROJECT_RECIPE_LOCATION_MACRO_EXPANSION;
    default:
        return 0U;
    }
}

static bool evo_search_entry_allowed_at_target(
    const evo_project_search_config_t *config,
    const evo_project_transformation_catalogue_entry_t *entry,
    const char *target_identity)
{
    const evo_project_source_location_record_t *location =
        evo_search_location(config, target_identity);
    const uint32_t mask =
        location == NULL ? 0U : evo_search_location_mask(location->kind);

    return location != NULL && mask != 0U &&
           (entry->allowed_location_kinds & mask) != 0U;
}

static bool evo_search_logical_duplicate(
    const evo_project_search_mutable_recipe_t *mutable_recipe,
    const char *target_identity,
    const char *transformation_identity,
    uint32_t transformation_version,
    size_t ignore_index)
{
    size_t index;

    for (index = 0U; index < mutable_recipe->record_count; index += 1U) {
        const evo_project_search_mutable_record_t *record =
            &mutable_recipe->records[index];

        if (index == ignore_index) {
            continue;
        }
        if (record->transformation_version == transformation_version &&
            strcmp(record->target_location_identity, target_identity) == 0 &&
            strcmp(record->transformation_identity, transformation_identity) == 0) {
            return true;
        }
    }
    return false;
}

static bool evo_search_default_parameters(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe,
    size_t record_index,
    const evo_project_transformation_catalogue_entry_t *entry)
{
    evo_project_recipe_parameter_value_t *parameters;
    size_t index;

    if (entry->parameter_schema_count > mutable_recipe->parameter_capacity ||
        entry->parameter_schema_count >
            config->recipe_context.limits.max_parameters_per_record) {
        return false;
    }
    parameters = evo_search_parameters(mutable_recipe, record_index);
    for (index = 0U; index < mutable_recipe->parameter_capacity; index += 1U) {
        parameters[index] = (evo_project_recipe_parameter_value_t){0};
    }
    for (index = 0U; index < entry->parameter_schema_count; index += 1U) {
        const evo_project_transformation_parameter_schema_t *schema =
            &entry->parameter_schemas[index];
        evo_project_recipe_parameter_value_t *parameter = &parameters[index];

        parameter->identity = schema->identity;
        parameter->kind = schema->kind;
        switch (schema->kind) {
        case EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN:
            parameter->boolean_value = false;
            break;
        case EVO_PROJECT_RECIPE_PARAMETER_INTEGER:
            parameter->integer_value = schema->minimum_integer;
            break;
        case EVO_PROJECT_RECIPE_PARAMETER_CHOICE:
            if (schema->choice_count == 0U || schema->choices == NULL) {
                return false;
            }
            parameter->choice_value = schema->choices[0];
            break;
        default:
            return false;
        }
    }
    mutable_recipe->records[record_index].parameter_count =
        entry->parameter_schema_count;
    return true;
}

static bool evo_search_set_catalogue_record(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe,
    size_t record_index,
    const char *target_identity,
    const evo_project_transformation_catalogue_entry_t *entry)
{
    if (!evo_search_record_set(
            mutable_recipe,
            record_index,
            target_identity,
            entry->identity,
            entry->implementation_version,
            0U,
            NULL)) {
        return false;
    }
    return evo_search_default_parameters(
        config, mutable_recipe, record_index, entry);
}

static void evo_search_remove_record(
    evo_project_search_mutable_recipe_t *mutable_recipe,
    size_t remove_index)
{
    size_t index;

    if (mutable_recipe == NULL || remove_index >= mutable_recipe->record_count) {
        return;
    }
    for (index = remove_index; index + 1U < mutable_recipe->record_count; index += 1U) {
        (void)evo_search_record_copy(
            mutable_recipe, index, mutable_recipe, index + 1U);
    }
    mutable_recipe->record_count -= 1U;
    if (mutable_recipe->record_count < mutable_recipe->capacity) {
        evo_project_search_mutable_record_t *record =
            &mutable_recipe->records[mutable_recipe->record_count];
        evo_project_recipe_parameter_value_t *parameters =
            evo_search_parameters(mutable_recipe, mutable_recipe->record_count);

        *record = (evo_project_search_mutable_record_t){0};
        for (index = 0U; index < mutable_recipe->parameter_capacity; index += 1U) {
            parameters[index] = (evo_project_recipe_parameter_value_t){0};
        }
    }
}

static bool evo_search_reference_matches(
    const evo_project_transformation_reference_t *reference,
    const evo_project_search_mutable_record_t *record)
{
    return reference->implementation_version == record->transformation_version &&
           strcmp(reference->identity, record->transformation_identity) == 0;
}

static bool evo_search_entries_conflict(
    const evo_project_transformation_catalogue_entry_t *left,
    const evo_project_search_mutable_record_t *right)
{
    size_t index;

    for (index = 0U; index < left->conflict_count; index += 1U) {
        if (evo_search_reference_matches(&left->conflicts[index], right)) {
            return true;
        }
    }
    return false;
}

static evo_project_recipe_status_t evo_search_add_dependency(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe,
    const evo_project_transformation_reference_t *dependency)
{
    const evo_project_transformation_catalogue_entry_t *entry =
        evo_search_catalogue_entry(
            config, dependency->identity, dependency->implementation_version);
    size_t opportunity_index;

    if (entry == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION;
    }
    if (mutable_recipe->record_count >= mutable_recipe->capacity) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    for (opportunity_index = 0U;
         opportunity_index < config->recipe_context.analysis->opportunity_count;
         opportunity_index += 1U) {
        const char *target =
            config->recipe_context.analysis->opportunities[opportunity_index]
                .location_identity;

        if (evo_search_entry_allowed_at_target(config, entry, target) &&
            !evo_search_logical_duplicate(
                mutable_recipe,
                target,
                entry->identity,
                entry->implementation_version,
                SIZE_MAX)) {
            const size_t record_index = mutable_recipe->record_count;

            if (!evo_search_set_catalogue_record(
                    config, mutable_recipe, record_index, target, entry)) {
                return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
            }
            mutable_recipe->record_count += 1U;
            return EVO_PROJECT_RECIPE_SUCCESS;
        }
    }
    return EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_MISSING;
}

static evo_project_recipe_status_t evo_search_repair(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe)
{
    size_t pass;

    for (pass = 0U; pass < config->policy.max_repair_passes; pass += 1U) {
        bool changed = false;
        size_t left;

        for (left = 0U; left < mutable_recipe->record_count; left += 1U) {
            size_t right = left + 1U;

            while (right < mutable_recipe->record_count) {
                const evo_project_search_mutable_record_t *left_record =
                    &mutable_recipe->records[left];
                const evo_project_search_mutable_record_t *right_record =
                    &mutable_recipe->records[right];

                if (left_record->transformation_version ==
                        right_record->transformation_version &&
                    strcmp(
                        left_record->target_location_identity,
                        right_record->target_location_identity) == 0 &&
                    strcmp(
                        left_record->transformation_identity,
                        right_record->transformation_identity) == 0) {
                    evo_search_remove_record(mutable_recipe, right);
                    changed = true;
                    continue;
                }
                right += 1U;
            }
        }

        for (left = 0U; left < mutable_recipe->record_count; left += 1U) {
            const evo_project_transformation_catalogue_entry_t *left_entry =
                evo_search_catalogue_entry(
                    config,
                    mutable_recipe->records[left].transformation_identity,
                    mutable_recipe->records[left].transformation_version);
            size_t right = left + 1U;

            if (left_entry == NULL) {
                return EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION;
            }
            while (right < mutable_recipe->record_count) {
                const evo_project_transformation_catalogue_entry_t *right_entry =
                    evo_search_catalogue_entry(
                        config,
                        mutable_recipe->records[right].transformation_identity,
                        mutable_recipe->records[right].transformation_version);

                if (right_entry == NULL) {
                    return EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION;
                }
                if (evo_search_entries_conflict(
                        left_entry, &mutable_recipe->records[right]) ||
                    evo_search_entries_conflict(
                        right_entry, &mutable_recipe->records[left])) {
                    evo_search_remove_record(mutable_recipe, right);
                    changed = true;
                    continue;
                }
                right += 1U;
            }
        }

        for (left = 0U; left < mutable_recipe->record_count; left += 1U) {
            const evo_project_transformation_catalogue_entry_t *entry =
                evo_search_catalogue_entry(
                    config,
                    mutable_recipe->records[left].transformation_identity,
                    mutable_recipe->records[left].transformation_version);
            size_t dependency_index;

            if (entry == NULL) {
                return EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION;
            }
            for (dependency_index = 0U;
                 dependency_index < entry->dependency_count;
                 dependency_index += 1U) {
                const evo_project_transformation_reference_t *dependency =
                    &entry->dependencies[dependency_index];
                size_t match_count = 0U;
                size_t record_index;

                for (record_index = 0U;
                     record_index < mutable_recipe->record_count;
                     record_index += 1U) {
                    if (evo_search_reference_matches(
                            dependency, &mutable_recipe->records[record_index])) {
                        match_count += 1U;
                    }
                }
                if (match_count > 1U) {
                    return EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_AMBIGUOUS;
                }
                if (match_count == 0U) {
                    const evo_project_recipe_status_t status =
                        evo_search_add_dependency(
                            config, mutable_recipe, dependency);

                    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
                        return status;
                    }
                    changed = true;
                }
            }
        }
        if (!changed) {
            return evo_search_mutable_normalize(mutable_recipe)
                       ? EVO_PROJECT_RECIPE_SUCCESS
                       : EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
        }
    }
    return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
}

evo_project_recipe_status_t evo_search_mutable_build(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe,
    evo_project_recipe_t *recipe)
{
    evo_project_recipe_build_config_t build_config = {0};
    evo_project_recipe_status_t status;
    size_t index;

    status = evo_search_repair(config, mutable_recipe);
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return status;
    }
    for (index = 0U; index < mutable_recipe->record_count; index += 1U) {
        evo_project_recipe_proposal_record_t *proposal =
            &mutable_recipe->proposals[index];
        const evo_project_search_mutable_record_t *record =
            &mutable_recipe->records[index];

        proposal->identity = record->identity;
        proposal->target_location_identity = record->target_location_identity;
        proposal->transformation_identity = record->transformation_identity;
        proposal->transformation_version = record->transformation_version;
        proposal->parameter_count = record->parameter_count;
        proposal->parameters = evo_search_parameters(mutable_recipe, index);
    }
    build_config.context = config->recipe_context;
    build_config.record_count = mutable_recipe->record_count;
    build_config.records = mutable_recipe->proposals;
    build_config.genome_size = config->genome_size;
    return evo_project_recipe_build(&build_config, recipe);
}

static size_t evo_search_compatible_pair_count(
    const evo_project_search_config_t *config,
    const evo_project_search_mutable_recipe_t *mutable_recipe)
{
    size_t count = 0U;
    size_t opportunity_index;

    for (opportunity_index = 0U;
         opportunity_index < config->recipe_context.analysis->opportunity_count;
         opportunity_index += 1U) {
        const char *target =
            config->recipe_context.analysis->opportunities[opportunity_index]
                .location_identity;
        size_t entry_index;

        for (entry_index = 0U;
             entry_index < config->recipe_context.catalogue->entry_count;
             entry_index += 1U) {
            const evo_project_transformation_catalogue_entry_t *entry =
                &config->recipe_context.catalogue->entries[entry_index];

            if (evo_search_entry_allowed_at_target(config, entry, target) &&
                !evo_search_logical_duplicate(
                    mutable_recipe,
                    target,
                    entry->identity,
                    entry->implementation_version,
                    SIZE_MAX)) {
                count += 1U;
            }
        }
    }
    return count;
}

static bool evo_search_select_compatible_pair(
    const evo_project_search_config_t *config,
    const evo_project_search_mutable_recipe_t *mutable_recipe,
    uint64_t selector,
    const char **target,
    const evo_project_transformation_catalogue_entry_t **entry)
{
    const size_t count =
        evo_search_compatible_pair_count(config, mutable_recipe);
    size_t selected;
    size_t current = 0U;
    size_t opportunity_index;

    if (count == 0U || target == NULL || entry == NULL) {
        return false;
    }
    selected = (size_t)(selector % (uint64_t)count);
    for (opportunity_index = 0U;
         opportunity_index < config->recipe_context.analysis->opportunity_count;
         opportunity_index += 1U) {
        const char *candidate_target =
            config->recipe_context.analysis->opportunities[opportunity_index]
                .location_identity;
        size_t entry_index;

        for (entry_index = 0U;
             entry_index < config->recipe_context.catalogue->entry_count;
             entry_index += 1U) {
            const evo_project_transformation_catalogue_entry_t *candidate_entry =
                &config->recipe_context.catalogue->entries[entry_index];

            if (!evo_search_entry_allowed_at_target(
                    config, candidate_entry, candidate_target) ||
                evo_search_logical_duplicate(
                    mutable_recipe,
                    candidate_target,
                    candidate_entry->identity,
                    candidate_entry->implementation_version,
                    SIZE_MAX)) {
                continue;
            }
            if (current == selected) {
                *target = candidate_target;
                *entry = candidate_entry;
                return true;
            }
            current += 1U;
        }
    }
    return false;
}

static evo_project_recipe_status_t evo_search_add_operation(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe,
    uint64_t selector)
{
    const char *target = NULL;
    const evo_project_transformation_catalogue_entry_t *entry = NULL;
    size_t record_index;

    if (mutable_recipe->record_count >= mutable_recipe->capacity) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    if (!evo_search_select_compatible_pair(
            config, mutable_recipe, selector, &target, &entry)) {
        return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
    }
    record_index = mutable_recipe->record_count;
    if (!evo_search_set_catalogue_record(
            config, mutable_recipe, record_index, target, entry)) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    mutable_recipe->record_count += 1U;
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static evo_project_recipe_status_t evo_search_remove_operation(
    evo_project_search_mutable_recipe_t *mutable_recipe,
    uint64_t selector)
{
    if (mutable_recipe->record_count == 0U) {
        return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
    }
    evo_search_remove_record(
        mutable_recipe,
        (size_t)(selector % (uint64_t)mutable_recipe->record_count));
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static const evo_project_transformation_parameter_schema_t *
evo_search_parameter_schema(
    const evo_project_transformation_catalogue_entry_t *entry,
    const char *identity)
{
    size_t index;

    for (index = 0U; index < entry->parameter_schema_count; index += 1U) {
        if (strcmp(entry->parameter_schemas[index].identity, identity) == 0) {
            return &entry->parameter_schemas[index];
        }
    }
    return NULL;
}

static evo_project_recipe_status_t evo_search_parameterize_operation(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe,
    uint64_t selector)
{
    size_t total = 0U;
    size_t record_index;
    size_t selected;

    for (record_index = 0U; record_index < mutable_recipe->record_count; record_index += 1U) {
        total += mutable_recipe->records[record_index].parameter_count;
    }
    if (total == 0U) {
        return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
    }
    selected = (size_t)(selector % (uint64_t)total);
    for (record_index = 0U; record_index < mutable_recipe->record_count; record_index += 1U) {
        evo_project_search_mutable_record_t *record =
            &mutable_recipe->records[record_index];
        evo_project_recipe_parameter_value_t *parameters =
            evo_search_parameters(mutable_recipe, record_index);
        size_t parameter_index;

        if (selected >= record->parameter_count) {
            selected -= record->parameter_count;
            continue;
        }
        parameter_index = selected;
        {
            const evo_project_transformation_catalogue_entry_t *entry =
                evo_search_catalogue_entry(
                    config,
                    record->transformation_identity,
                    record->transformation_version);
            const evo_project_transformation_parameter_schema_t *schema =
                entry == NULL
                    ? NULL
                    : evo_search_parameter_schema(
                          entry, parameters[parameter_index].identity);
            evo_project_recipe_parameter_value_t *parameter =
                &parameters[parameter_index];

            if (schema == NULL || parameter->kind != schema->kind) {
                return EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER;
            }
            switch (schema->kind) {
            case EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN:
                parameter->boolean_value = !parameter->boolean_value;
                return EVO_PROJECT_RECIPE_SUCCESS;
            case EVO_PROJECT_RECIPE_PARAMETER_INTEGER:
                if (parameter->integer_value < schema->maximum_integer) {
                    parameter->integer_value += 1;
                } else if (config->policy.integer_parameter_wrap) {
                    parameter->integer_value = schema->minimum_integer;
                } else if (parameter->integer_value > schema->minimum_integer) {
                    parameter->integer_value -= 1;
                }
                return EVO_PROJECT_RECIPE_SUCCESS;
            case EVO_PROJECT_RECIPE_PARAMETER_CHOICE: {
                size_t choice_index;

                if (schema->choice_count < 2U || schema->choices == NULL) {
                    return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
                }
                for (choice_index = 0U;
                     choice_index < schema->choice_count;
                     choice_index += 1U) {
                    if (strcmp(
                            schema->choices[choice_index],
                            parameter->choice_value) == 0) {
                        parameter->choice_value = schema->choices[(choice_index + 1U) % schema->choice_count];
                        return EVO_PROJECT_RECIPE_SUCCESS;
                    }
                }
                return EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER;
            }
            default:
                return EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER;
            }
        }
    }
    return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
}

static size_t evo_search_replacement_count(
    const evo_project_search_config_t *config,
    const evo_project_search_mutable_recipe_t *mutable_recipe)
{
    size_t count = 0U;
    size_t record_index;

    for (record_index = 0U; record_index < mutable_recipe->record_count; record_index += 1U) {
        const evo_project_search_mutable_record_t *record =
            &mutable_recipe->records[record_index];
        size_t entry_index;

        for (entry_index = 0U;
             entry_index < config->recipe_context.catalogue->entry_count;
             entry_index += 1U) {
            const evo_project_transformation_catalogue_entry_t *entry =
                &config->recipe_context.catalogue->entries[entry_index];

            if (entry->implementation_version == record->transformation_version &&
                strcmp(entry->identity, record->transformation_identity) == 0) {
                continue;
            }
            if (evo_search_entry_allowed_at_target(
                    config, entry, record->target_location_identity) &&
                !evo_search_logical_duplicate(
                    mutable_recipe,
                    record->target_location_identity,
                    entry->identity,
                    entry->implementation_version,
                    record_index)) {
                count += 1U;
            }
        }
    }
    return count;
}

static evo_project_recipe_status_t evo_search_replace_operation(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe,
    uint64_t selector)
{
    const size_t count = evo_search_replacement_count(config, mutable_recipe);
    size_t selected;
    size_t current = 0U;
    size_t record_index;

    if (count == 0U) {
        return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
    }
    selected = (size_t)(selector % (uint64_t)count);
    for (record_index = 0U; record_index < mutable_recipe->record_count; record_index += 1U) {
        const char *target =
            mutable_recipe->records[record_index].target_location_identity;
        size_t entry_index;

        for (entry_index = 0U;
             entry_index < config->recipe_context.catalogue->entry_count;
             entry_index += 1U) {
            const evo_project_transformation_catalogue_entry_t *entry =
                &config->recipe_context.catalogue->entries[entry_index];
            const evo_project_search_mutable_record_t *record =
                &mutable_recipe->records[record_index];

            if ((entry->implementation_version == record->transformation_version &&
                 strcmp(entry->identity, record->transformation_identity) == 0) ||
                !evo_search_entry_allowed_at_target(config, entry, target) ||
                evo_search_logical_duplicate(
                    mutable_recipe,
                    target,
                    entry->identity,
                    entry->implementation_version,
                    record_index)) {
                continue;
            }
            if (current == selected) {
                return evo_search_set_catalogue_record(
                           config, mutable_recipe, record_index, target, entry)
                           ? EVO_PROJECT_RECIPE_SUCCESS
                           : EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
            }
            current += 1U;
        }
    }
    return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
}

static evo_project_recipe_status_t evo_search_reorder_operation(
    evo_project_search_mutable_recipe_t *mutable_recipe,
    uint64_t selector)
{
    evo_project_recipe_parameter_value_t *temporary_parameters;
    evo_project_search_mutable_record_t temporary_record;
    size_t first;
    size_t second;
    size_t index;

    if (mutable_recipe->record_count < 2U) {
        return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
    }
    first = (size_t)(selector % (uint64_t)mutable_recipe->record_count);
    second = (size_t)((selector / (uint64_t)mutable_recipe->record_count) %
                      (uint64_t)(mutable_recipe->record_count - 1U));
    if (second >= first) {
        second += 1U;
    }
    temporary_parameters = evo_project_allocate_zeroed(
        mutable_recipe->parameter_capacity, sizeof(*temporary_parameters));
    if (temporary_parameters == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    temporary_record = mutable_recipe->records[first];
    for (index = 0U; index < mutable_recipe->parameter_capacity; index += 1U) {
        temporary_parameters[index] =
            evo_search_parameters(mutable_recipe, first)[index];
    }
    (void)evo_search_record_copy(
        mutable_recipe, first, mutable_recipe, second);
    (void)evo_search_record_set(
        mutable_recipe,
        second,
        temporary_record.target_location_identity,
        temporary_record.transformation_identity,
        temporary_record.transformation_version,
        temporary_record.parameter_count,
        temporary_parameters);
    evo_project_release(temporary_parameters);
    return evo_search_mutable_normalize(mutable_recipe)
               ? EVO_PROJECT_RECIPE_SUCCESS
               : EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
}

static evo_project_recipe_status_t evo_search_apply_operation(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe,
    evo_project_search_operator_kind_t operation,
    uint64_t selector)
{
    switch (operation) {
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_ADD:
        return evo_search_add_operation(config, mutable_recipe, selector);
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REMOVE:
        return evo_search_remove_operation(mutable_recipe, selector);
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_PARAMETERIZE:
        return evo_search_parameterize_operation(
            config, mutable_recipe, selector);
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REPLACE:
        return evo_search_replace_operation(config, mutable_recipe, selector);
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REORDER:
        return evo_search_reorder_operation(mutable_recipe, selector);
    default:
        return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
    }
}

evo_project_recipe_status_t evo_search_initialize_recipe(
    const evo_project_search_config_t *config,
    uint64_t selector,
    evo_project_recipe_t *recipe)
{
    evo_project_search_mutable_recipe_t mutable_recipe = {0};
    evo_project_recipe_status_t status = EVO_PROJECT_RECIPE_SUCCESS;
    size_t index;

    if (!evo_search_mutable_open(config, &mutable_recipe)) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < config->policy.initial_record_count; index += 1U) {
        status = evo_search_add_operation(
            config,
            &mutable_recipe,
            selector + UINT64_C(0x9e3779b97f4a7c15) * (uint64_t)(index + 1U));
        if (status != EVO_PROJECT_RECIPE_SUCCESS) {
            break;
        }
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_search_mutable_build(config, &mutable_recipe, recipe);
    }
    evo_search_mutable_close(&mutable_recipe);
    return status;
}

evo_project_recipe_status_t evo_search_mutate_recipe(
    const evo_project_search_config_t *config,
    const unsigned char *parent_genome,
    evo_project_search_operator_kind_t operation,
    uint64_t selector,
    evo_project_recipe_t *recipe)
{
    evo_project_recipe_t parent = {0};
    evo_project_search_mutable_recipe_t mutable_recipe = {0};
    evo_project_recipe_status_t status;

    status = evo_project_recipe_decode(
        &config->recipe_context,
        parent_genome,
        config->genome_size,
        &parent);
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return status;
    }
    if (!evo_search_mutable_open(config, &mutable_recipe)) {
        evo_project_recipe_destroy(&parent);
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    if (!evo_search_mutable_from_recipe(config, &parent, &mutable_recipe)) {
        status = EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    } else {
        status = evo_search_apply_operation(
            config, &mutable_recipe, operation, selector);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_search_mutable_build(config, &mutable_recipe, recipe);
    }
    evo_search_mutable_close(&mutable_recipe);
    evo_project_recipe_destroy(&parent);
    return status;
}

static bool evo_search_append_record(
    evo_project_search_mutable_recipe_t *destination,
    const evo_project_search_mutable_recipe_t *source,
    size_t source_index)
{
    if (destination->record_count >= destination->capacity ||
        !evo_search_record_copy(
            destination,
            destination->record_count,
            source,
            source_index)) {
        return false;
    }
    destination->record_count += 1U;
    return true;
}

static bool evo_search_cross_model(
    evo_project_search_mutable_recipe_t *destination,
    const evo_project_search_mutable_recipe_t *prefix,
    size_t prefix_count,
    const evo_project_search_mutable_recipe_t *suffix,
    size_t suffix_start)
{
    size_t index;

    for (index = 0U; index < prefix_count; index += 1U) {
        if (!evo_search_append_record(destination, prefix, index)) {
            return false;
        }
    }
    for (index = suffix_start; index < suffix->record_count; index += 1U) {
        if (!evo_search_append_record(destination, suffix, index)) {
            return false;
        }
    }
    return true;
}

evo_project_recipe_status_t evo_search_crossover_recipes(
    const evo_project_search_config_t *config,
    const unsigned char *parent_a_genome,
    const unsigned char *parent_b_genome,
    uint64_t selector,
    evo_project_recipe_t *child_a,
    evo_project_recipe_t *child_b)
{
    evo_project_recipe_t parent_a = {0};
    evo_project_recipe_t parent_b = {0};
    evo_project_search_mutable_recipe_t model_a = {0};
    evo_project_search_mutable_recipe_t model_b = {0};
    evo_project_search_mutable_recipe_t crossed_a = {0};
    evo_project_search_mutable_recipe_t crossed_b = {0};
    evo_project_recipe_status_t status;
    size_t cut_a;
    size_t cut_b;

    status = evo_project_recipe_decode(
        &config->recipe_context,
        parent_a_genome,
        config->genome_size,
        &parent_a);
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return status;
    }
    status = evo_project_recipe_decode(
        &config->recipe_context,
        parent_b_genome,
        config->genome_size,
        &parent_b);
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        evo_project_recipe_destroy(&parent_a);
        return status;
    }
    if (!evo_search_mutable_open(config, &model_a) ||
        !evo_search_mutable_open(config, &model_b) ||
        !evo_search_mutable_open(config, &crossed_a) ||
        !evo_search_mutable_open(config, &crossed_b) ||
        !evo_search_mutable_from_recipe(config, &parent_a, &model_a) ||
        !evo_search_mutable_from_recipe(config, &parent_b, &model_b)) {
        status = EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        goto finish;
    }
    cut_a = (size_t)(selector % (uint64_t)(model_a.record_count + 1U));
    cut_b = (size_t)((selector >> 32U) %
                     (uint64_t)(model_b.record_count + 1U));
    if (!evo_search_cross_model(
            &crossed_a, &model_a, cut_a, &model_b, cut_b) ||
        !evo_search_cross_model(
            &crossed_b, &model_b, cut_b, &model_a, cut_a)) {
        status = EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
        goto finish;
    }
    status = evo_search_mutable_build(config, &crossed_a, child_a);
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        goto finish;
    }
    status = evo_search_mutable_build(config, &crossed_b, child_b);
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        evo_project_recipe_destroy(child_a);
    }

finish:
    evo_search_mutable_close(&model_a);
    evo_search_mutable_close(&model_b);
    evo_search_mutable_close(&crossed_a);
    evo_search_mutable_close(&crossed_b);
    evo_project_recipe_destroy(&parent_a);
    evo_project_recipe_destroy(&parent_b);
    return status;
}

evo_project_search_rejection_reason_t evo_search_rejection_from_recipe(
    evo_project_recipe_status_t status)
{
    switch (status) {
    case EVO_PROJECT_RECIPE_SUCCESS:
        return EVO_PROJECT_SEARCH_REJECTION_NONE;
    case EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT:
    case EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY:
        return EVO_PROJECT_SEARCH_REJECTION_RESOURCE_LIMIT;
    case EVO_PROJECT_RECIPE_ERROR_STATE:
        return EVO_PROJECT_SEARCH_REJECTION_STATE;
    default:
        return EVO_PROJECT_SEARCH_REJECTION_RECIPE_INVALID;
    }
}

static bool evo_search_append_double(
    evo_candidate_buffer_t *buffer,
    double value)
{
    char text[64];
    const int written =
        evo_project_format(text, sizeof(text), "%.17g", value);

    return written > 0 && (size_t)written < sizeof(text) &&
           evo_candidate_buffer_append_text(buffer, text);
}

static bool evo_search_append_bool(
    evo_candidate_buffer_t *buffer,
    bool value)
{
    return evo_candidate_buffer_append_text(buffer, value ? "true" : "false");
}

static bool evo_search_append_fitness_json(
    evo_candidate_buffer_t *buffer,
    const evo_fitness_t *fitness)
{
    return evo_candidate_buffer_append_text(buffer, "{") &&
           evo_candidate_buffer_append_text(buffer, "\"correctness\":") &&
           evo_search_append_double(buffer, fitness->correctness) &&
           evo_candidate_buffer_append_text(buffer, ",\"performance\":") &&
           evo_search_append_double(buffer, fitness->performance) &&
           evo_candidate_buffer_append_text(buffer, ",\"memory_use\":") &&
           evo_search_append_double(buffer, fitness->memory_use) &&
           evo_candidate_buffer_append_text(buffer, ",\"reliability\":") &&
           evo_search_append_double(buffer, fitness->reliability) &&
           evo_candidate_buffer_append_text(buffer, ",\"maintainability\":") &&
           evo_search_append_double(buffer, fitness->maintainability) &&
           evo_candidate_buffer_append_text(buffer, ",\"constraint_penalty\":") &&
           evo_search_append_double(buffer, fitness->constraint_penalty) &&
           evo_candidate_buffer_append_text(buffer, ",\"total\":") &&
           evo_search_append_double(buffer, fitness->total) &&
           evo_candidate_buffer_append_text(buffer, "}");
}

static bool evo_search_append_json_string_field(
    evo_candidate_buffer_t *buffer,
    const char *name,
    const char *value,
    bool comma)
{
    return evo_candidate_buffer_append_json_string(buffer, name) &&
           evo_candidate_buffer_append_text(buffer, ":") &&
           evo_candidate_buffer_append_json_string(buffer, value) &&
           (!comma || evo_candidate_buffer_append_text(buffer, ","));
}

static bool evo_search_build_json(
    const evo_project_search_config_t *config,
    const evo_project_search_owner_t *owner,
    evo_candidate_buffer_t *json)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(
            json, "{\"schema\":\"catalyst.evo-project-search.v1\",") ||
        !evo_search_append_json_string_field(
            json, "baseline_fingerprint", owner->view.baseline_fingerprint, true) ||
        !evo_search_append_json_string_field(
            json, "analysis_fingerprint", owner->view.analysis_fingerprint, true) ||
        !evo_search_append_json_string_field(
            json, "catalogue_identity", owner->view.catalogue_identity, true) ||
        !evo_candidate_buffer_append_text(json, "\"catalogue_version\":") ||
        !evo_candidate_buffer_append_u64(
            json, (uint64_t)owner->view.catalogue_version) ||
        !evo_candidate_buffer_append_text(json, ",") ||
        !evo_search_append_json_string_field(
            json, "policy_identity", owner->view.policy_identity, true) ||
        !evo_search_append_json_string_field(
            json,
            "evaluation_provider_identity",
            owner->view.evaluation_provider_identity,
            true) ||
        !evo_candidate_buffer_append_text(json, "\"random_seed\":") ||
        !evo_candidate_buffer_append_u64(json, owner->view.random_seed) ||
        !evo_candidate_buffer_append_text(json, ",\"population_size\":") ||
        !evo_candidate_buffer_append_size(json, owner->view.population_size) ||
        !evo_candidate_buffer_append_text(json, ",\"generation_limit\":") ||
        !evo_candidate_buffer_append_size(json, config->generation_limit) ||
        !evo_candidate_buffer_append_text(json, ",\"generations_completed\":") ||
        !evo_candidate_buffer_append_size(
            json, owner->view.generations_completed) ||
        !evo_candidate_buffer_append_text(json, ",\"termination_reason\":") ||
        !evo_candidate_buffer_append_u64(
            json, (uint64_t)owner->view.termination_reason) ||
        !evo_candidate_buffer_append_text(json, ",\"policy\":{") ||
        !evo_candidate_buffer_append_text(json, "\"mutation_policy_version\":1,") ||
        !evo_candidate_buffer_append_text(json, "\"crossover_policy_version\":1,") ||
        !evo_candidate_buffer_append_text(json, "\"repair_policy_version\":1,") ||
        !evo_candidate_buffer_append_text(json, "\"initial_record_count\":") ||
        !evo_candidate_buffer_append_size(
            json, config->policy.initial_record_count) ||
        !evo_candidate_buffer_append_text(json, ",\"maximum_record_count\":") ||
        !evo_candidate_buffer_append_size(
            json, config->policy.maximum_record_count) ||
        !evo_candidate_buffer_append_text(json, ",\"mutation_operation_mask\":") ||
        !evo_candidate_buffer_append_u64(
            json, (uint64_t)config->policy.mutation_operation_mask) ||
        !evo_candidate_buffer_append_text(json, ",\"max_mutations_per_event\":") ||
        !evo_candidate_buffer_append_size(
            json, config->policy.max_mutations_per_event) ||
        !evo_candidate_buffer_append_text(json, ",\"max_repair_passes\":") ||
        !evo_candidate_buffer_append_size(
            json, config->policy.max_repair_passes) ||
        !evo_candidate_buffer_append_text(json, ",\"integer_parameter_wrap\":") ||
        !evo_search_append_bool(json, config->policy.integer_parameter_wrap) ||
        !evo_candidate_buffer_append_text(json, "},\"lineage\":[")) {
        return false;
    }
    for (index = 0U; index < owner->lineage_count; index += 1U) {
        const evo_project_search_lineage_record_t *record =
            &owner->lineage[index];

        if (index > 0U && !evo_candidate_buffer_append_text(json, ",")) {
            return false;
        }
        if (!evo_candidate_buffer_append_text(json, "{") ||
            !evo_candidate_buffer_append_text(json, "\"generation\":") ||
            !evo_candidate_buffer_append_size(json, record->generation) ||
            !evo_candidate_buffer_append_text(json, ",\"population_index\":") ||
            !evo_candidate_buffer_append_size(json, record->population_index) ||
            !evo_candidate_buffer_append_text(json, ",\"operator_ordinal\":") ||
            !evo_candidate_buffer_append_size(json, record->operator_ordinal) ||
            !evo_candidate_buffer_append_text(json, ",\"operator_kind\":") ||
            !evo_candidate_buffer_append_json_string(
                json, evo_project_search_operator_kind_name(record->operator_kind)) ||
            !evo_candidate_buffer_append_text(json, ",\"rejection_reason\":") ||
            !evo_candidate_buffer_append_json_string(
                json,
                evo_project_search_rejection_reason_name(
                    record->rejection_reason)) ||
            !evo_candidate_buffer_append_text(json, ",\"recipe_status\":") ||
            !evo_candidate_buffer_append_json_string(
                json, evo_project_recipe_status_name(record->recipe_status)) ||
            !evo_candidate_buffer_append_text(json, ",") ||
            !evo_search_append_json_string_field(
                json, "recipe_fingerprint", record->recipe_fingerprint, true) ||
            !evo_search_append_json_string_field(
                json,
                "parent_a_recipe_fingerprint",
                record->parent_a_recipe_fingerprint,
                true) ||
            !evo_search_append_json_string_field(
                json,
                "parent_b_recipe_fingerprint",
                record->parent_b_recipe_fingerprint,
                true) ||
            !evo_search_append_json_string_field(
                json, "candidate_fingerprint", record->candidate_fingerprint, true) ||
            !evo_search_append_json_string_field(
                json, "assurance_fingerprint", record->assurance_fingerprint, true) ||
            !evo_search_append_json_string_field(
                json,
                "measurement_fingerprint",
                record->measurement_fingerprint,
                true) ||
            !evo_candidate_buffer_append_text(json, "\"fitness\":") ||
            !evo_search_append_fitness_json(json, &record->fitness) ||
            !evo_candidate_buffer_append_text(json, ",\"valid\":") ||
            !evo_search_append_bool(json, record->valid) ||
            !evo_candidate_buffer_append_text(json, ",\"evaluated\":") ||
            !evo_search_append_bool(json, record->evaluated) ||
            !evo_candidate_buffer_append_text(json, ",\"winner\":") ||
            !evo_search_append_bool(json, record->winner) ||
            !evo_candidate_buffer_append_text(json, "}")) {
            return false;
        }
    }
    return evo_candidate_buffer_append_text(json, "],\"best\":{") &&
           evo_search_append_json_string_field(
               json, "recipe_fingerprint", owner->view.best_recipe_fingerprint, true) &&
           evo_search_append_json_string_field(
               json,
               "candidate_fingerprint",
               owner->view.best_candidate_fingerprint,
               true) &&
           evo_search_append_json_string_field(
               json,
               "assurance_fingerprint",
               owner->view.best_assurance_fingerprint,
               true) &&
           evo_search_append_json_string_field(
               json,
               "measurement_fingerprint",
               owner->view.best_measurement_fingerprint,
               true) &&
           evo_candidate_buffer_append_text(json, "\"fitness\":") &&
           evo_search_append_fitness_json(json, &owner->view.best_fitness) &&
           evo_candidate_buffer_append_text(json, "},\"projection_complete\":true,") &&
           evo_candidate_buffer_append_text(json, "\"probabilistic_authority\":false,") &&
           evo_candidate_buffer_append_text(json, "\"raw_source_bytes\":false,") &&
           evo_search_append_json_string_field(
               json, "search_fingerprint", owner->view.search_fingerprint, false) &&
           evo_candidate_buffer_append_text(json, "}\n");
}

static bool evo_search_build_markdown(
    const evo_project_search_config_t *config,
    const evo_project_search_owner_t *owner,
    evo_candidate_buffer_t *markdown)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(
            markdown, "# EVO Structured Recipe Search\n\n") ||
        !evo_candidate_buffer_append_text(markdown, "- Policy: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.policy_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Baseline: `") ||
        !evo_candidate_buffer_append_text(
            markdown, owner->view.baseline_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Analysis: `") ||
        !evo_candidate_buffer_append_text(
            markdown, owner->view.analysis_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Catalogue: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.catalogue_identity) ||
        !evo_candidate_buffer_append_text(
            markdown,
            "`\n\nRaw C source bytes are never mutation or crossover authority. Exact recipe records, live recipe reconstruction, evaluation mappings, and the EVO core result remain authority.\n\n") ||
        !evo_candidate_buffer_append_text(
            markdown,
            "## Ordered lineage\n\n| Generation | Index | Operator | Recipe | Parent A | Parent B | Valid | Evaluated | Total | Rejection | Winner |\n|---:|---:|---|---|---|---|---|---|---:|---|---|\n")) {
        return false;
    }
    for (index = 0U; index < owner->lineage_count; index += 1U) {
        const evo_project_search_lineage_record_t *record =
            &owner->lineage[index];
        char row[640];
        const int written = evo_project_format(
            row,
            sizeof(row),
            "| %zu | %zu | %s | %s | %s | %s | %s | %s | %.17g | %s | %s |\n",
            record->generation,
            record->population_index,
            evo_project_search_operator_kind_name(record->operator_kind),
            record->recipe_fingerprint,
            record->parent_a_recipe_fingerprint,
            record->parent_b_recipe_fingerprint,
            record->valid ? "yes" : "no",
            record->evaluated ? "yes" : "no",
            record->fitness.total,
            evo_project_search_rejection_reason_name(record->rejection_reason),
            record->winner ? "yes" : "no");

        if (written <= 0 || (size_t)written >= sizeof(row) ||
            !evo_candidate_buffer_append_text(markdown, row)) {
            return false;
        }
    }
    if (!evo_candidate_buffer_append_text(
            markdown,
            "\n## Winner\n\nBest recipe: `") ||
        !evo_candidate_buffer_append_text(
            markdown, owner->view.best_recipe_fingerprint) ||
        !evo_candidate_buffer_append_text(
            markdown,
            "`\n\nThis is the **best verified candidate found within the recorded bounded search contract**; it is not a claim of global program optimality.\n\n") ||
        !evo_candidate_buffer_append_text(
            markdown,
            "No accelerated representation participates in recipe acceptance, rejection, ranking, exact-tie handling, or winner selection.\n")) {
        return false;
    }
    (void)config;
    return true;
}

bool evo_search_build_evidence(
    const evo_project_search_config_t *config,
    evo_project_search_owner_t *owner)
{
    evo_candidate_buffer_t json = {0};
    evo_candidate_buffer_t markdown = {0};

    if (!evo_candidate_buffer_open(&json, config->limits.max_evidence_bytes) ||
        !evo_candidate_buffer_open(
            &markdown, config->limits.max_evidence_bytes)) {
        evo_candidate_buffer_close(&json);
        evo_candidate_buffer_close(&markdown);
        return false;
    }
    if (!evo_search_build_json(config, owner, &json) ||
        !evo_search_build_markdown(config, owner, &markdown) ||
        json.size > config->limits.max_total_bytes ||
        markdown.size > config->limits.max_total_bytes - json.size) {
        evo_candidate_buffer_close(&json);
        evo_candidate_buffer_close(&markdown);
        return false;
    }
    owner->canonical_json = json.bytes;
    owner->audit_markdown = markdown.bytes;
    owner->view.canonical_json = owner->canonical_json;
    owner->view.canonical_json_size = json.size;
    owner->view.audit_markdown = owner->audit_markdown;
    owner->view.audit_markdown_size = markdown.size;
    return true;
}
