#include "internal/project_recipe_model.h"

#include "internal/project_json.h"
#include "internal/project_runtime.h"

#include <stdlib.h>
#include <string.h>

static bool evo_project_recipe_count_view_valid(
    size_t count,
    const void *view)
{
    return (count == 0U && view == NULL) || (count > 0U && view != NULL);
}

static char *evo_project_recipe_duplicate(const char *value)
{
    size_t size;
    char *copy;
    size_t index;

    if (value == NULL) {
        return NULL;
    }
    size = strlen(value);
    if (size == SIZE_MAX) {
        return NULL;
    }
    copy = evo_project_allocate_zeroed(size + 1U, sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }
    for (index = 0U; index < size; index += 1U) {
        copy[index] = value[index];
    }
    copy[size] = '\0';
    return copy;
}

static int evo_project_recipe_text_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_recipe_parameter_value_t *left = left_value;
    const evo_project_recipe_parameter_value_t *right = right_value;

    return strcmp(left->identity, right->identity);
}

static int evo_project_recipe_reference_compare(
    const evo_project_transformation_reference_t *left,
    const evo_project_transformation_reference_t *right)
{
    const int identity_order = strcmp(left->identity, right->identity);

    if (identity_order != 0) {
        return identity_order;
    }
    if (left->implementation_version < right->implementation_version) {
        return -1;
    }
    if (left->implementation_version > right->implementation_version) {
        return 1;
    }
    return 0;
}

static int evo_project_recipe_entry_compare(
    const evo_project_transformation_catalogue_entry_t *left,
    const evo_project_transformation_catalogue_entry_t *right)
{
    const int identity_order = strcmp(left->identity, right->identity);

    if (identity_order != 0) {
        return identity_order;
    }
    if (left->implementation_version < right->implementation_version) {
        return -1;
    }
    if (left->implementation_version > right->implementation_version) {
        return 1;
    }
    return 0;
}

static bool evo_project_recipe_reference_valid(
    const evo_project_transformation_reference_t *reference,
    const evo_project_recipe_limits_t *limits)
{
    return reference != NULL && reference->implementation_version > 0U &&
           evo_project_json_text_valid(
               reference->identity, limits->max_string_bytes, false);
}

static const evo_project_transformation_catalogue_entry_t *
evo_project_recipe_find_catalogue_entry(
    const evo_project_transformation_catalogue_t *catalogue,
    const char *identity,
    uint32_t version)
{
    size_t index;

    for (index = 0U; index < catalogue->entry_count; index += 1U) {
        const evo_project_transformation_catalogue_entry_t *entry =
            &catalogue->entries[index];

        if (entry->implementation_version == version &&
            strcmp(entry->identity, identity) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool evo_project_recipe_choice_schema_valid(
    const evo_project_transformation_parameter_schema_t *schema,
    const evo_project_recipe_limits_t *limits,
    size_t *choice_total)
{
    size_t index;

    if (schema->choice_count == 0U || schema->choice_count > limits->max_choices ||
        schema->choices == NULL ||
        schema->choice_count > limits->max_choices - *choice_total) {
        return false;
    }
    for (index = 0U; index < schema->choice_count; index += 1U) {
        if (!evo_project_json_text_valid(
                schema->choices[index], limits->max_string_bytes, false) ||
            (index > 0U &&
             strcmp(schema->choices[index - 1U], schema->choices[index]) >= 0)) {
            return false;
        }
    }
    *choice_total += schema->choice_count;
    return true;
}

static bool evo_project_recipe_parameter_schema_valid(
    const evo_project_transformation_parameter_schema_t *schema,
    const evo_project_recipe_limits_t *limits,
    size_t *choice_total)
{
    if (!evo_project_json_text_valid(
            schema->identity, limits->max_string_bytes, false)) {
        return false;
    }
    switch (schema->kind) {
    case EVO_PROJECT_RECIPE_PARAMETER_INTEGER:
        return schema->minimum_integer <= schema->maximum_integer &&
               schema->choice_count == 0U && schema->choices == NULL;
    case EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN:
        return schema->minimum_integer == 0 && schema->maximum_integer == 0 &&
               schema->choice_count == 0U && schema->choices == NULL;
    case EVO_PROJECT_RECIPE_PARAMETER_CHOICE:
        return schema->minimum_integer == 0 && schema->maximum_integer == 0 &&
               evo_project_recipe_choice_schema_valid(
                   schema, limits, choice_total);
    default:
        return false;
    }
}

static bool evo_project_recipe_text_array_valid(
    size_t count,
    const char *const *values,
    size_t maximum,
    size_t maximum_string_bytes)
{
    size_t index;

    if (count > maximum || !evo_project_recipe_count_view_valid(count, values)) {
        return false;
    }
    for (index = 0U; index < count; index += 1U) {
        if (!evo_project_json_text_valid(
                values[index], maximum_string_bytes, false) ||
            (index > 0U && strcmp(values[index - 1U], values[index]) >= 0)) {
            return false;
        }
    }
    return true;
}

static bool evo_project_recipe_reference_array_valid(
    size_t count,
    const evo_project_transformation_reference_t *references,
    size_t maximum,
    const evo_project_recipe_limits_t *limits)
{
    size_t index;

    if (count > maximum ||
        !evo_project_recipe_count_view_valid(count, references)) {
        return false;
    }
    for (index = 0U; index < count; index += 1U) {
        if (!evo_project_recipe_reference_valid(&references[index], limits) ||
            (index > 0U &&
             evo_project_recipe_reference_compare(
                 &references[index - 1U], &references[index]) >= 0)) {
            return false;
        }
    }
    return true;
}

static bool evo_project_recipe_entry_arrays_valid(
    const evo_project_transformation_catalogue_entry_t *entry,
    const evo_project_recipe_limits_t *limits,
    size_t *schema_total,
    size_t *choice_total)
{
    size_t index;

    if (entry->parameter_schema_count > limits->max_parameter_schemas ||
        !evo_project_recipe_count_view_valid(
            entry->parameter_schema_count, entry->parameter_schemas) ||
        entry->parameter_schema_count >
            limits->max_parameter_schemas - *schema_total ||
        !evo_project_recipe_text_array_valid(
            entry->precondition_count,
            entry->preconditions,
            limits->max_preconditions_per_record,
            limits->max_string_bytes) ||
        !evo_project_recipe_reference_array_valid(
            entry->dependency_count,
            entry->dependencies,
            limits->max_dependencies_per_record,
            limits) ||
        !evo_project_recipe_reference_array_valid(
            entry->conflict_count,
            entry->conflicts,
            limits->max_conflicts_per_record,
            limits)) {
        return false;
    }
    for (index = 0U; index < entry->parameter_schema_count; index += 1U) {
        if (!evo_project_recipe_parameter_schema_valid(
                &entry->parameter_schemas[index], limits, choice_total) ||
            (index > 0U &&
             strcmp(
                 entry->parameter_schemas[index - 1U].identity,
                 entry->parameter_schemas[index].identity) >= 0)) {
            return false;
        }
    }
    *schema_total += entry->parameter_schema_count;
    return true;
}

static bool evo_project_recipe_catalogue_references_valid(
    const evo_project_transformation_catalogue_t *catalogue)
{
    size_t entry_index;

    for (entry_index = 0U; entry_index < catalogue->entry_count;
         entry_index += 1U) {
        const evo_project_transformation_catalogue_entry_t *entry =
            &catalogue->entries[entry_index];
        size_t dependency_index;
        size_t conflict_index;

        for (dependency_index = 0U;
             dependency_index < entry->dependency_count;
             dependency_index += 1U) {
            const evo_project_transformation_reference_t *reference =
                &entry->dependencies[dependency_index];

            if (evo_project_recipe_find_catalogue_entry(
                    catalogue,
                    reference->identity,
                    reference->implementation_version) == NULL ||
                (strcmp(reference->identity, entry->identity) == 0 &&
                 reference->implementation_version ==
                     entry->implementation_version)) {
                return false;
            }
        }
        for (conflict_index = 0U; conflict_index < entry->conflict_count;
             conflict_index += 1U) {
            const evo_project_transformation_reference_t *reference =
                &entry->conflicts[conflict_index];

            if (evo_project_recipe_find_catalogue_entry(
                    catalogue,
                    reference->identity,
                    reference->implementation_version) == NULL ||
                (strcmp(reference->identity, entry->identity) == 0 &&
                 reference->implementation_version ==
                     entry->implementation_version)) {
                return false;
            }
            for (dependency_index = 0U;
                 dependency_index < entry->dependency_count;
                 dependency_index += 1U) {
                if (evo_project_recipe_reference_compare(
                        reference,
                        &entry->dependencies[dependency_index]) == 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool evo_project_recipe_catalogue_valid(
    const evo_project_transformation_catalogue_t *catalogue,
    const evo_project_recipe_limits_t *limits)
{
    size_t schema_total = 0U;
    size_t choice_total = 0U;
    size_t index;

    if (catalogue->schema_version !=
            EVO_PROJECT_TRANSFORMATION_CATALOGUE_SCHEMA_VERSION ||
        catalogue->catalogue_version == 0U ||
        !evo_project_json_text_valid(
            catalogue->identity, limits->max_string_bytes, false) ||
        catalogue->entry_count == 0U ||
        catalogue->entry_count > limits->max_catalogue_entries ||
        catalogue->entries == NULL) {
        return false;
    }
    for (index = 0U; index < catalogue->entry_count; index += 1U) {
        const evo_project_transformation_catalogue_entry_t *entry =
            &catalogue->entries[index];

        if (!evo_project_json_text_valid(
                entry->identity, limits->max_string_bytes, false) ||
            entry->implementation_version == 0U ||
            entry->allowed_location_kinds == 0U ||
            (entry->allowed_location_kinds &
             ~EVO_PROJECT_RECIPE_LOCATION_ALL) != 0U ||
            !evo_project_recipe_entry_arrays_valid(
                entry, limits, &schema_total, &choice_total) ||
            (index > 0U &&
             evo_project_recipe_entry_compare(
                 &catalogue->entries[index - 1U], entry) >= 0)) {
            return false;
        }
    }
    return evo_project_recipe_catalogue_references_valid(catalogue);
}

static const evo_project_source_location_record_t *
evo_project_recipe_find_location(
    const evo_project_analysis_t *analysis,
    const char *identity)
{
    size_t index;

    for (index = 0U; index < analysis->source_location_count; index += 1U) {
        if (strcmp(analysis->source_locations[index].identity, identity) == 0) {
            return &analysis->source_locations[index];
        }
    }
    return NULL;
}

static const evo_project_opportunity_record_t *
evo_project_recipe_find_opportunity(
    const evo_project_analysis_t *analysis,
    const char *location_identity)
{
    size_t index;

    for (index = 0U; index < analysis->opportunity_count; index += 1U) {
        if (strcmp(
                analysis->opportunities[index].location_identity,
                location_identity) == 0) {
            return &analysis->opportunities[index];
        }
    }
    return NULL;
}

static uint32_t evo_project_recipe_location_mask(
    evo_project_source_location_kind_t kind)
{
    if (kind == EVO_PROJECT_LOCATION_SPELLING) {
        return EVO_PROJECT_RECIPE_LOCATION_SPELLING;
    }
    if (kind == EVO_PROJECT_LOCATION_MACRO_EXPANSION) {
        return EVO_PROJECT_RECIPE_LOCATION_MACRO_EXPANSION;
    }
    return 0U;
}

static const evo_project_transformation_parameter_schema_t *
evo_project_recipe_find_parameter_schema(
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

static bool evo_project_recipe_choice_valid(
    const evo_project_transformation_parameter_schema_t *schema,
    const char *choice)
{
    size_t index;

    for (index = 0U; index < schema->choice_count; index += 1U) {
        if (strcmp(schema->choices[index], choice) == 0) {
            return true;
        }
    }
    return false;
}

static bool evo_project_recipe_parameter_valid(
    const evo_project_recipe_parameter_value_t *value,
    const evo_project_transformation_parameter_schema_t *schema,
    const evo_project_recipe_limits_t *limits)
{
    if (!evo_project_json_text_valid(
            value->identity, limits->max_string_bytes, false) ||
        value->kind != schema->kind) {
        return false;
    }
    switch (value->kind) {
    case EVO_PROJECT_RECIPE_PARAMETER_INTEGER:
        return value->integer_value >= schema->minimum_integer &&
               value->integer_value <= schema->maximum_integer &&
               !value->boolean_value && value->choice_value == NULL;
    case EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN:
        return value->integer_value == 0 && value->choice_value == NULL;
    case EVO_PROJECT_RECIPE_PARAMETER_CHOICE:
        return value->integer_value == 0 && !value->boolean_value &&
               evo_project_json_text_valid(
                   value->choice_value, limits->max_string_bytes, false) &&
               evo_project_recipe_choice_valid(schema, value->choice_value);
    default:
        return false;
    }
}

static void evo_project_recipe_parameter_destroy(
    evo_project_recipe_parameter_value_t *parameter)
{
    evo_project_release((void *)parameter->identity);
    evo_project_release((void *)parameter->choice_value);
    *parameter = (evo_project_recipe_parameter_value_t){0};
}

static evo_project_recipe_status_t evo_project_recipe_copy_parameters(
    const evo_project_recipe_proposal_record_t *proposal,
    const evo_project_transformation_catalogue_entry_t *entry,
    const evo_project_recipe_limits_t *limits,
    evo_project_recipe_record_owner_t *record)
{
    size_t index;

    if (proposal->parameter_count > limits->max_parameters_per_record ||
        !evo_project_recipe_count_view_valid(
            proposal->parameter_count, proposal->parameters)) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    if (proposal->parameter_count > 0U) {
        record->parameters = evo_project_allocate_zeroed(
            proposal->parameter_count, sizeof(*record->parameters));
        if (record->parameters == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
    }
    record->view.parameter_count = proposal->parameter_count;
    record->view.parameters = record->parameters;
    for (index = 0U; index < proposal->parameter_count; index += 1U) {
        const evo_project_recipe_parameter_value_t *source =
            &proposal->parameters[index];
        const evo_project_transformation_parameter_schema_t *schema;

        if (!evo_project_json_text_valid(
                source->identity, limits->max_string_bytes, false)) {
            return EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER;
        }
        schema = evo_project_recipe_find_parameter_schema(
            entry, source->identity);
        if (schema == NULL ||
            !evo_project_recipe_parameter_valid(source, schema, limits)) {
            return EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER;
        }
        record->parameters[index] = *source;
        record->parameters[index].identity =
            evo_project_recipe_duplicate(source->identity);
        if (source->choice_value != NULL) {
            record->parameters[index].choice_value =
                evo_project_recipe_duplicate(source->choice_value);
        }
        if (record->parameters[index].identity == NULL ||
            (source->choice_value != NULL &&
             record->parameters[index].choice_value == NULL)) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
    }
    if (proposal->parameter_count > 1U) {
        qsort(
            record->parameters,
            proposal->parameter_count,
            sizeof(*record->parameters),
            evo_project_recipe_text_compare);
    }
    for (index = 0U; index < proposal->parameter_count; index += 1U) {
        if (index > 0U &&
            strcmp(
                record->parameters[index - 1U].identity,
                record->parameters[index].identity) == 0) {
            return EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER;
        }
    }
    for (index = 0U; index < entry->parameter_schema_count; index += 1U) {
        const evo_project_transformation_parameter_schema_t *schema =
            &entry->parameter_schemas[index];
        bool found = false;
        size_t parameter_index;

        for (parameter_index = 0U;
             parameter_index < proposal->parameter_count;
             parameter_index += 1U) {
            if (strcmp(
                    schema->identity,
                    record->parameters[parameter_index].identity) == 0) {
                found = true;
                break;
            }
        }
        if (schema->required && !found) {
            return EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER;
        }
    }
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static evo_project_recipe_status_t evo_project_recipe_copy_text_array(
    const char *const *source,
    size_t count,
    char ***destination)
{
    char **values;
    size_t index;

    *destination = NULL;
    if (count == 0U) {
        return EVO_PROJECT_RECIPE_SUCCESS;
    }
    values = evo_project_allocate_zeroed(count, sizeof(*values));
    if (values == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < count; index += 1U) {
        values[index] = evo_project_recipe_duplicate(source[index]);
        if (values[index] == NULL) {
            size_t release_index;

            for (release_index = 0U; release_index < index;
                 release_index += 1U) {
                evo_project_release(values[release_index]);
            }
            evo_project_release(values);
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
    }
    *destination = values;
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static evo_project_recipe_status_t evo_project_recipe_copy_preconditions(
    const evo_project_transformation_catalogue_entry_t *entry,
    evo_project_recipe_record_owner_t *record)
{
    evo_project_recipe_status_t status = evo_project_recipe_copy_text_array(
        entry->preconditions,
        entry->precondition_count,
        &record->preconditions);

    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        record->view.precondition_count = entry->precondition_count;
        record->view.preconditions =
            (const char *const *)record->preconditions;
    }
    return status;
}

static evo_project_recipe_status_t evo_project_recipe_copy_conflicts(
    const evo_project_transformation_catalogue_entry_t *entry,
    evo_project_recipe_record_owner_t *record)
{
    size_t index;

    record->view.conflict_count = entry->conflict_count;
    if (entry->conflict_count > 0U) {
        record->conflicts = evo_project_allocate_zeroed(
            entry->conflict_count, sizeof(*record->conflicts));
        if (record->conflicts == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
    }
    for (index = 0U; index < entry->conflict_count; index += 1U) {
        record->conflicts[index] = entry->conflicts[index];
        record->conflicts[index].identity =
            evo_project_recipe_duplicate(entry->conflicts[index].identity);
        if (record->conflicts[index].identity == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
    }
    record->view.conflicts = record->conflicts;
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static evo_project_recipe_status_t evo_project_recipe_copy_target(
    const evo_project_source_location_record_t *location,
    evo_project_recipe_record_owner_t *record)
{
    record->target_location_identity =
        evo_project_recipe_duplicate(location->identity);
    record->target_file = evo_project_recipe_duplicate(location->file);
    if (location->spelling_identity != NULL) {
        record->target_spelling_identity =
            evo_project_recipe_duplicate(location->spelling_identity);
    }
    if (record->target_location_identity == NULL ||
        record->target_file == NULL ||
        (location->spelling_identity != NULL &&
         record->target_spelling_identity == NULL)) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    record->view.target.location_identity = record->target_location_identity;
    record->view.target.file = record->target_file;
    record->view.target.line = location->line;
    record->view.target.column = location->column;
    record->view.target.end_line = location->end_line;
    record->view.target.end_column = location->end_column;
    record->view.target.kind = location->kind;
    record->view.target.spelling_identity =
        record->target_spelling_identity;
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static evo_project_recipe_status_t evo_project_recipe_copy_provenance(
    const evo_project_recipe_context_t *context,
    const evo_project_opportunity_record_t *opportunity,
    evo_project_recipe_record_owner_t *record)
{
    size_t compiler_count = 0U;
    size_t runtime_count = 0U;
    size_t index;
    evo_project_recipe_status_t status;

    for (index = 0U; index < context->analysis->optimization_record_count;
         index += 1U) {
        const evo_project_optimization_record_t *evidence =
            &context->analysis->optimization_records[index];

        if (evidence->disposition == EVO_PROJECT_OPTIMIZATION_MISSED &&
            strcmp(
                evidence->location_identity,
                record->view.target.location_identity) == 0) {
            compiler_count += 1U;
        }
    }
    for (index = 0U; index < context->analysis->runtime_record_count;
         index += 1U) {
        if (strcmp(
                context->analysis->runtime_records[index].location_identity,
                record->view.target.location_identity) == 0) {
            runtime_count += 1U;
        }
    }
    if (compiler_count > context->limits.max_provenance_records_per_record ||
        runtime_count >
            context->limits.max_provenance_records_per_record - compiler_count) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    record->view.opportunity_rank = opportunity->rank;
    record->view.compiler_record_count = compiler_count;
    record->view.runtime_record_count = runtime_count;
    if (compiler_count > 0U) {
        char **identities = evo_project_allocate_zeroed(
            compiler_count, sizeof(*record->compiler_record_identities));
        size_t output_index = 0U;

        if (identities == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
        record->compiler_record_identities = identities;
        for (index = 0U; index < context->analysis->optimization_record_count;
             index += 1U) {
            const evo_project_optimization_record_t *evidence =
                &context->analysis->optimization_records[index];

            if (evidence->disposition == EVO_PROJECT_OPTIMIZATION_MISSED &&
                strcmp(
                    evidence->location_identity,
                    record->view.target.location_identity) == 0) {
                record->compiler_record_identities[output_index] =
                    evo_project_recipe_duplicate(evidence->identity);
                if (record->compiler_record_identities[output_index] == NULL) {
                    return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
                }
                output_index += 1U;
            }
        }
    }
    if (runtime_count > 0U) {
        char **identities = evo_project_allocate_zeroed(
            runtime_count, sizeof(*record->runtime_record_identities));
        size_t output_index = 0U;

        if (identities == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
        record->runtime_record_identities = identities;
        for (index = 0U; index < context->analysis->runtime_record_count;
             index += 1U) {
            const evo_project_runtime_record_t *evidence =
                &context->analysis->runtime_records[index];

            if (strcmp(
                    evidence->location_identity,
                    record->view.target.location_identity) == 0) {
                record->runtime_record_identities[output_index] =
                    evo_project_recipe_duplicate(evidence->identity);
                if (record->runtime_record_identities[output_index] == NULL) {
                    return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
                }
                output_index += 1U;
            }
        }
    }
    record->view.compiler_record_identities =
        (const char *const *)record->compiler_record_identities;
    record->view.runtime_record_identities =
        (const char *const *)record->runtime_record_identities;
    status = compiler_count == opportunity->missed_optimization_count &&
                     (runtime_count > 0U) ==
                         opportunity->runtime_evidence_present
                 ? EVO_PROJECT_RECIPE_SUCCESS
                 : EVO_PROJECT_RECIPE_ERROR_ANALYSIS_STALE;
    return status;
}

static evo_project_recipe_status_t evo_project_recipe_build_record(
    const evo_project_recipe_context_t *context,
    const evo_project_recipe_proposal_record_t *proposal,
    size_t source_index,
    evo_project_recipe_record_owner_t *record)
{
    const evo_project_transformation_catalogue_entry_t *entry;
    const evo_project_source_location_record_t *location;
    const evo_project_opportunity_record_t *opportunity;
    evo_project_recipe_status_t status;

    if (!evo_project_json_text_valid(
            proposal->identity, context->limits.max_string_bytes, false) ||
        !evo_project_json_text_valid(
            proposal->target_location_identity,
            context->limits.max_string_bytes,
            false) ||
        !evo_project_json_text_valid(
            proposal->transformation_identity,
            context->limits.max_string_bytes,
            false) ||
        proposal->transformation_version == 0U) {
        return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
    }
    entry = evo_project_recipe_find_catalogue_entry(
        context->catalogue,
        proposal->transformation_identity,
        proposal->transformation_version);
    if (entry == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION;
    }
    location = evo_project_recipe_find_location(
        context->analysis, proposal->target_location_identity);
    opportunity = evo_project_recipe_find_opportunity(
        context->analysis, proposal->target_location_identity);
    if (location == NULL || opportunity == NULL ||
        (entry->allowed_location_kinds &
         evo_project_recipe_location_mask(location->kind)) == 0U) {
        return EVO_PROJECT_RECIPE_ERROR_STALE_TARGET;
    }
    if (!evo_project_json_text_valid(
            location->file, context->limits.max_path_bytes, false) ||
        (location->spelling_identity != NULL &&
         !evo_project_json_text_valid(
             location->spelling_identity,
             context->limits.max_string_bytes,
             false))) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    record->identity = evo_project_recipe_duplicate(proposal->identity);
    record->transformation_identity =
        evo_project_recipe_duplicate(proposal->transformation_identity);
    if (record->identity == NULL || record->transformation_identity == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    record->source_index = source_index;
    record->view.identity = record->identity;
    record->view.transformation_identity = record->transformation_identity;
    record->view.transformation_version = proposal->transformation_version;
    status = evo_project_recipe_copy_target(location, record);
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_copy_parameters(
            proposal, entry, &context->limits, record);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_copy_preconditions(entry, record);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_copy_conflicts(entry, record);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_copy_provenance(
            context, opportunity, record);
    }
    return status;
}

static size_t evo_project_recipe_matching_records(
    const evo_project_recipe_owner_t *owner,
    const evo_project_transformation_reference_t *reference,
    size_t *matching_index)
{
    size_t count = 0U;
    size_t index;

    for (index = 0U; index < owner->record_count; index += 1U) {
        const evo_project_recipe_record_t *record =
            &owner->record_owners[index].view;

        if (record->transformation_version ==
                reference->implementation_version &&
            strcmp(
                record->transformation_identity, reference->identity) == 0) {
            count += 1U;
            *matching_index = index;
        }
    }
    return count;
}

static evo_project_recipe_status_t evo_project_recipe_resolve_dependencies(
    const evo_project_recipe_context_t *context,
    evo_project_recipe_owner_t *owner)
{
    size_t record_index;

    for (record_index = 0U; record_index < owner->record_count;
         record_index += 1U) {
        evo_project_recipe_record_owner_t *record =
            &owner->record_owners[record_index];
        const evo_project_transformation_catalogue_entry_t *entry =
            evo_project_recipe_find_catalogue_entry(
                context->catalogue,
                record->view.transformation_identity,
                record->view.transformation_version);
        size_t dependency_index;
        size_t conflict_index;

        if (entry == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION;
        }
        if (entry->dependency_count > 0U) {
            record->dependencies = evo_project_allocate_zeroed(
                entry->dependency_count, sizeof(*record->dependencies));
            if (record->dependencies == NULL) {
                return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
            }
        }
        record->view.dependency_count = entry->dependency_count;
        record->view.dependencies = record->dependencies;
        for (dependency_index = 0U;
             dependency_index < entry->dependency_count;
             dependency_index += 1U) {
            const evo_project_transformation_reference_t *reference =
                &entry->dependencies[dependency_index];
            size_t matching_index = 0U;
            const size_t matching_count = evo_project_recipe_matching_records(
                owner, reference, &matching_index);
            evo_project_recipe_dependency_t *dependency =
                &record->dependencies[dependency_index];

            if (matching_count == 0U) {
                return EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_MISSING;
            }
            if (matching_count != 1U) {
                return EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_AMBIGUOUS;
            }
            dependency->record_identity = evo_project_recipe_duplicate(
                owner->record_owners[matching_index].view.identity);
            dependency->transformation_identity =
                evo_project_recipe_duplicate(reference->identity);
            dependency->transformation_version =
                reference->implementation_version;
            if (dependency->record_identity == NULL ||
                dependency->transformation_identity == NULL) {
                return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
            }
        }
        for (conflict_index = 0U; conflict_index < entry->conflict_count;
             conflict_index += 1U) {
            size_t matching_index = 0U;

            if (evo_project_recipe_matching_records(
                    owner,
                    &entry->conflicts[conflict_index],
                    &matching_index) > 0U) {
                return EVO_PROJECT_RECIPE_ERROR_CONFLICT;
            }
        }
    }
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static bool evo_project_recipe_dependency_emitted(
    const evo_project_recipe_dependency_t *dependency,
    const evo_project_recipe_owner_t *owner,
    const bool *emitted)
{
    size_t index;

    for (index = 0U; index < owner->record_count; index += 1U) {
        if (strcmp(
                owner->record_owners[index].view.identity,
                dependency->record_identity) == 0) {
            return emitted[index];
        }
    }
    return false;
}

static bool evo_project_recipe_record_ready(
    const evo_project_recipe_record_owner_t *record,
    const evo_project_recipe_owner_t *owner,
    const bool *emitted)
{
    size_t index;

    for (index = 0U; index < record->view.dependency_count; index += 1U) {
        if (!evo_project_recipe_dependency_emitted(
                &record->view.dependencies[index], owner, emitted)) {
            return false;
        }
    }
    return true;
}

static evo_project_recipe_status_t evo_project_recipe_canonical_order(
    evo_project_recipe_owner_t *owner)
{
    bool *emitted;
    size_t *order;
    evo_project_recipe_record_owner_t *ordered;
    size_t output_index;

    if (owner->record_count == 0U) {
        return EVO_PROJECT_RECIPE_SUCCESS;
    }
    emitted = evo_project_allocate_zeroed(
        owner->record_count, sizeof(*emitted));
    order = evo_project_allocate_zeroed(owner->record_count, sizeof(*order));
    ordered = evo_project_allocate_zeroed(
        owner->record_count, sizeof(*ordered));
    if (emitted == NULL || order == NULL || ordered == NULL) {
        evo_project_release(emitted);
        evo_project_release(order);
        evo_project_release(ordered);
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    for (output_index = 0U; output_index < owner->record_count;
         output_index += 1U) {
        size_t candidate = SIZE_MAX;
        size_t index;

        for (index = 0U; index < owner->record_count; index += 1U) {
            if (!emitted[index] &&
                evo_project_recipe_record_ready(
                    &owner->record_owners[index], owner, emitted) &&
                (candidate == SIZE_MAX ||
                 strcmp(
                     owner->record_owners[index].view.identity,
                     owner->record_owners[candidate].view.identity) < 0)) {
                candidate = index;
            }
        }
        if (candidate == SIZE_MAX) {
            evo_project_release(emitted);
            evo_project_release(order);
            evo_project_release(ordered);
            return EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_CYCLE;
        }
        emitted[candidate] = true;
        order[output_index] = candidate;
    }
    for (output_index = 0U; output_index < owner->record_count;
         output_index += 1U) {
        const size_t source_index = order[output_index];

        ordered[output_index] = owner->record_owners[source_index];
        owner->record_owners[source_index] =
            (evo_project_recipe_record_owner_t){0};
    }
    evo_project_release(owner->record_owners);
    owner->record_owners = ordered;
    evo_project_release(emitted);
    evo_project_release(order);
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static evo_project_recipe_status_t evo_project_recipe_publish_views(
    evo_project_recipe_owner_t *owner)
{
    size_t index;

    if (owner->record_count > 0U) {
        owner->records = evo_project_allocate_zeroed(
            owner->record_count, sizeof(*owner->records));
        if (owner->records == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
    }
    for (index = 0U; index < owner->record_count; index += 1U) {
        evo_project_recipe_record_owner_t *record =
            &owner->record_owners[index];

        record->view.baseline_fingerprint = owner->baseline_fingerprint;
        record->view.analysis_fingerprint = owner->analysis_fingerprint;
        record->view.catalogue_identity = owner->catalogue_identity;
        record->view.catalogue_version = owner->catalogue_version;
        owner->records[index] = record->view;
    }
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static void evo_project_recipe_release_text_array(
    char **values,
    size_t count)
{
    size_t index;

    if (values == NULL) {
        return;
    }
    for (index = 0U; index < count; index += 1U) {
        evo_project_release(values[index]);
    }
    evo_project_release(values);
}

static void evo_project_recipe_record_destroy(
    evo_project_recipe_record_owner_t *record)
{
    size_t index;

    evo_project_release(record->identity);
    evo_project_release(record->target_location_identity);
    evo_project_release(record->target_file);
    evo_project_release(record->target_spelling_identity);
    evo_project_release(record->transformation_identity);
    for (index = 0U; index < record->view.parameter_count; index += 1U) {
        evo_project_recipe_parameter_destroy(&record->parameters[index]);
    }
    evo_project_release(record->parameters);
    evo_project_recipe_release_text_array(
        record->preconditions, record->view.precondition_count);
    if (record->dependencies != NULL) {
        for (index = 0U; index < record->view.dependency_count; index += 1U) {
            evo_project_release(
                (void *)record->dependencies[index].record_identity);
            evo_project_release(
                (void *)record->dependencies[index].transformation_identity);
        }
    }
    evo_project_release(record->dependencies);
    if (record->conflicts != NULL) {
        for (index = 0U; index < record->view.conflict_count; index += 1U) {
            evo_project_release((void *)record->conflicts[index].identity);
        }
    }
    evo_project_release(record->conflicts);
    evo_project_recipe_release_text_array(
        record->compiler_record_identities,
        record->view.compiler_record_count);
    evo_project_recipe_release_text_array(
        record->runtime_record_identities,
        record->view.runtime_record_count);
    *record = (evo_project_recipe_record_owner_t){0};
}

evo_project_recipe_status_t evo_project_recipe_model_build(
    const evo_project_recipe_context_t *context,
    const evo_project_baseline_owner_t *baseline_owner,
    const evo_project_recipe_proposal_record_t *proposals,
    size_t proposal_count,
    evo_project_recipe_owner_t *owner)
{
    size_t index;
    evo_project_recipe_status_t status;

    (void)baseline_owner;
    if (!evo_project_recipe_catalogue_valid(
            context->catalogue, &context->limits)) {
        return EVO_PROJECT_RECIPE_ERROR_CATALOGUE_INVALID;
    }
    if (proposal_count > context->limits.max_records ||
        !evo_project_recipe_count_view_valid(proposal_count, proposals)) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    owner->baseline_fingerprint = evo_project_recipe_duplicate(
        context->baseline->baseline_fingerprint);
    owner->analysis_fingerprint = evo_project_recipe_duplicate(
        context->analysis->analysis_fingerprint);
    owner->catalogue_identity = evo_project_recipe_duplicate(
        context->catalogue->identity);
    owner->catalogue_version = context->catalogue->catalogue_version;
    owner->record_count = proposal_count;
    if (owner->baseline_fingerprint == NULL ||
        owner->analysis_fingerprint == NULL ||
        owner->catalogue_identity == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    if (proposal_count > 0U) {
        owner->record_owners = evo_project_allocate_zeroed(
            proposal_count, sizeof(*owner->record_owners));
        if (owner->record_owners == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
    }
    for (index = 0U; index < proposal_count; index += 1U) {
        size_t previous;

        for (previous = 0U; previous < index; previous += 1U) {
            if (proposals[previous].identity != NULL &&
                proposals[index].identity != NULL &&
                strcmp(
                    proposals[previous].identity,
                    proposals[index].identity) == 0) {
                return EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID;
            }
        }
        status = evo_project_recipe_build_record(
            context,
            &proposals[index],
            index,
            &owner->record_owners[index]);
        if (status != EVO_PROJECT_RECIPE_SUCCESS) {
            return status;
        }
    }
    status = evo_project_recipe_resolve_dependencies(context, owner);
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_canonical_order(owner);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_publish_views(owner);
    }
    return status;
}

void evo_project_recipe_model_destroy(evo_project_recipe_owner_t *owner)
{
    size_t index;

    if (owner == NULL) {
        return;
    }
    if (owner->record_owners != NULL) {
        for (index = 0U; index < owner->record_count; index += 1U) {
            evo_project_recipe_record_destroy(&owner->record_owners[index]);
        }
    }
    evo_project_release(owner->record_owners);
    evo_project_release(owner->records);
    evo_project_release(owner->baseline_fingerprint);
    evo_project_release(owner->analysis_fingerprint);
    evo_project_release(owner->catalogue_identity);
    evo_project_release(owner->genome);
    evo_project_release(owner->audit_markdown);
    *owner = (evo_project_recipe_owner_t){0};
}
