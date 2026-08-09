#include "internal/project_analysis_model.h"

#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_runtime.h"

#include <stdlib.h>
#include <string.h>

static evo_project_analysis_status_t evo_project_analysis_copy_text(
    const char *value,
    size_t maximum_bytes,
    char **copy)
{
    size_t size;
    size_t index;

    if (!evo_project_json_text_valid(value, maximum_bytes, false)) {
        return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
    }
    size = strlen(value);
    if (size == SIZE_MAX) {
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    *copy = evo_project_allocate_zeroed(size + 1U, sizeof(**copy));
    if (*copy == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < size; index += 1U) {
        (*copy)[index] = value[index];
    }
    (*copy)[size] = '\0';
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static evo_project_analysis_status_t evo_project_analysis_copy_nullable_text(
    const char *value,
    size_t maximum_bytes,
    char **copy)
{
    if (value == NULL) {
        *copy = NULL;
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    return evo_project_analysis_copy_text(value, maximum_bytes, copy);
}

static evo_project_analysis_status_t evo_project_analysis_array_status(
    size_t count,
    const void *records,
    size_t maximum_count)
{
    if (count > maximum_count) {
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    if ((count == 0U && records != NULL) ||
        (count > 0U && records == NULL)) {
        return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static void *evo_project_analysis_allocate_array(
    size_t count,
    size_t element_size,
    evo_project_analysis_status_t *status)
{
    void *allocation;

    if (count == 0U || element_size == 0U ||
        count > SIZE_MAX / element_size) {
        *status = EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
        return NULL;
    }
    allocation = evo_project_allocate_zeroed(count, element_size);
    if (allocation == NULL) {
        *status = EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    *status = EVO_PROJECT_ANALYSIS_SUCCESS;
    return allocation;
}

static const evo_project_file_record_t *evo_project_analysis_find_file(
    const evo_project_baseline_owner_t *baseline_owner,
    const char *path)
{
    size_t index;

    if (path == NULL) {
        return NULL;
    }
    for (index = 0U; index < baseline_owner->file_count; index += 1U) {
        if (strcmp(baseline_owner->files[index].path, path) == 0) {
            return &baseline_owner->files[index];
        }
    }
    return NULL;
}

static const evo_project_compilation_record_t *evo_project_analysis_find_unit(
    const evo_project_baseline_owner_t *baseline_owner,
    const char *file)
{
    size_t index;

    if (file == NULL) {
        return NULL;
    }
    for (index = 0U; index < baseline_owner->compilation_unit_count;
         index += 1U) {
        if (strcmp(baseline_owner->compilation_units[index].file, file) == 0) {
            return &baseline_owner->compilation_units[index];
        }
    }
    return NULL;
}

static bool evo_project_analysis_workload_exists(
    const evo_project_baseline_owner_t *baseline_owner,
    const char *identity)
{
    size_t index;

    if (identity == NULL) {
        return false;
    }
    for (index = 0U; index < baseline_owner->manifest.workload_count;
         index += 1U) {
        if (strcmp(baseline_owner->manifest.workloads[index], identity) == 0) {
            return true;
        }
    }
    return false;
}

static evo_project_analysis_status_t evo_project_analysis_copy_units(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    evo_project_analysis_owner_t *owner)
{
    size_t index;
    evo_project_analysis_status_t status;

    if (baseline_owner->compilation_unit_count == 0U) {
        return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
    }
    if (baseline_owner->compilation_unit_count >
        config->limits.max_translation_units) {
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    owner->translation_units = evo_project_analysis_allocate_array(
        baseline_owner->compilation_unit_count,
        sizeof(*owner->translation_units),
        &status);
    if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return status;
    }
    owner->translation_unit_count = baseline_owner->compilation_unit_count;
    for (index = 0U; index < owner->translation_unit_count; index += 1U) {
        status = evo_project_analysis_copy_text(
            baseline_owner->compilation_units[index].file,
            config->limits.max_path_bytes,
            &owner->translation_units[index]);

        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static int evo_project_analysis_location_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_source_location_record_t *left = left_value;
    const evo_project_source_location_record_t *right = right_value;

    return strcmp(left->identity, right->identity);
}

static int evo_project_analysis_declaration_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_declaration_record_t *left = left_value;
    const evo_project_declaration_record_t *right = right_value;

    return strcmp(left->identity, right->identity);
}

static int evo_project_analysis_call_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_call_record_t *left = left_value;
    const evo_project_call_record_t *right = right_value;

    return strcmp(left->identity, right->identity);
}

static int evo_project_analysis_control_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_control_flow_record_t *left = left_value;
    const evo_project_control_flow_record_t *right = right_value;

    return strcmp(left->identity, right->identity);
}

static int evo_project_analysis_data_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_data_flow_record_t *left = left_value;
    const evo_project_data_flow_record_t *right = right_value;

    return strcmp(left->identity, right->identity);
}

static int evo_project_analysis_optimization_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_optimization_record_t *left = left_value;
    const evo_project_optimization_record_t *right = right_value;

    return strcmp(left->identity, right->identity);
}

static int evo_project_analysis_runtime_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_runtime_record_t *left = left_value;
    const evo_project_runtime_record_t *right = right_value;

    return strcmp(left->identity, right->identity);
}

static int evo_project_analysis_opportunity_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_opportunity_record_t *left = left_value;
    const evo_project_opportunity_record_t *right = right_value;

    if (left->runtime_evidence_present != right->runtime_evidence_present) {
        return left->runtime_evidence_present ? -1 : 1;
    }
    if (left->runtime_sample_count != right->runtime_sample_count) {
        return left->runtime_sample_count > right->runtime_sample_count ? -1
                                                                        : 1;
    }
    if (left->missed_optimization_count !=
        right->missed_optimization_count) {
        return left->missed_optimization_count >
                       right->missed_optimization_count
                   ? -1
                   : 1;
    }
    return strcmp(left->location_identity, right->location_identity);
}

static const evo_project_source_location_record_t *
evo_project_analysis_find_location(
    const evo_project_analysis_owner_t *owner,
    const char *identity)
{
    size_t index;

    if (identity == NULL) {
        return NULL;
    }
    for (index = 0U; index < owner->source_location_count; index += 1U) {
        if (strcmp(owner->source_locations[index].identity, identity) == 0) {
            return &owner->source_locations[index];
        }
    }
    return NULL;
}

static const evo_project_declaration_record_t *
evo_project_analysis_find_declaration(
    const evo_project_analysis_owner_t *owner,
    const char *identity)
{
    size_t index;

    if (identity == NULL) {
        return NULL;
    }
    for (index = 0U; index < owner->declaration_count; index += 1U) {
        if (strcmp(owner->declarations[index].identity, identity) == 0) {
            return &owner->declarations[index];
        }
    }
    return NULL;
}

static bool evo_project_analysis_location_range_valid(
    const evo_project_source_location_record_t *record)
{
    if (record->line == 0U || record->column == 0U ||
        record->end_line == 0U || record->end_column == 0U) {
        return false;
    }
    return record->end_line > record->line ||
           (record->end_line == record->line &&
            record->end_column >= record->column);
}

static evo_project_analysis_status_t evo_project_analysis_copy_locations(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    const evo_project_analysis_provider_result_t *provider_result,
    evo_project_analysis_owner_t *owner)
{
    const evo_project_analysis_status_t array_status =
        evo_project_analysis_array_status(
            provider_result->source_location_count,
            provider_result->source_locations,
            config->limits.max_source_locations);
    size_t index;

    if (array_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return array_status;
    }
    if (provider_result->source_location_count == 0U) {
        return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
    }
    {
        evo_project_analysis_status_t allocation_status;

        owner->source_locations = evo_project_analysis_allocate_array(
            provider_result->source_location_count,
            sizeof(*owner->source_locations),
            &allocation_status);
        if (allocation_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return allocation_status;
        }
    }
    owner->source_location_count = provider_result->source_location_count;
    for (index = 0U; index < owner->source_location_count; index += 1U) {
        const evo_project_source_location_record_t *source =
            &provider_result->source_locations[index];
        evo_project_source_location_record_t *destination =
            &owner->source_locations[index];
        evo_project_analysis_status_t status;

        if (!evo_project_analysis_location_range_valid(source) ||
            (source->kind != EVO_PROJECT_LOCATION_SPELLING &&
             source->kind != EVO_PROJECT_LOCATION_MACRO_EXPANSION &&
             source->kind != EVO_PROJECT_LOCATION_GENERATED)) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        if (source->kind == EVO_PROJECT_LOCATION_GENERATED) {
            return EVO_PROJECT_ANALYSIS_ERROR_UNSUPPORTED_EVIDENCE;
        }
        if (!evo_project_json_text_valid(
                source->identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->file, config->limits.max_path_bytes, false) ||
            (source->spelling_identity != NULL &&
             !evo_project_json_text_valid(
                 source->spelling_identity,
                 config->limits.max_string_bytes,
                 false)) ||
            evo_project_analysis_find_file(baseline_owner, source->file) ==
                NULL) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        if ((source->kind == EVO_PROJECT_LOCATION_MACRO_EXPANSION &&
             source->spelling_identity == NULL) ||
            (source->kind == EVO_PROJECT_LOCATION_SPELLING &&
             source->spelling_identity != NULL)) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        status = evo_project_analysis_copy_text(
            source->identity,
            config->limits.max_string_bytes,
            (char **)&destination->identity);
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->file,
                config->limits.max_path_bytes,
                (char **)&destination->file);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_nullable_text(
                source->spelling_identity,
                config->limits.max_string_bytes,
                (char **)&destination->spelling_identity);
        }
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
        destination->line = source->line;
        destination->column = source->column;
        destination->end_line = source->end_line;
        destination->end_column = source->end_column;
        destination->kind = source->kind;
    }
    qsort(
        owner->source_locations,
        owner->source_location_count,
        sizeof(*owner->source_locations),
        evo_project_analysis_location_compare);
    for (index = 0U; index < owner->source_location_count; index += 1U) {
        const evo_project_source_location_record_t *record =
            &owner->source_locations[index];

        if (index > 0U &&
            strcmp(
                owner->source_locations[index - 1U].identity,
                record->identity) == 0) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        if (record->kind == EVO_PROJECT_LOCATION_MACRO_EXPANSION) {
            const evo_project_source_location_record_t *spelling =
                evo_project_analysis_find_location(
                    owner, record->spelling_identity);

            if (spelling == NULL || spelling == record ||
                spelling->kind != EVO_PROJECT_LOCATION_SPELLING) {
                return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
            }
        }
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static bool evo_project_analysis_declaration_enums_valid(
    const evo_project_declaration_record_t *record)
{
    const bool kind_valid =
        record->kind == EVO_PROJECT_DECLARATION_FUNCTION ||
        record->kind == EVO_PROJECT_DECLARATION_VARIABLE ||
        record->kind == EVO_PROJECT_DECLARATION_TYPE;
    const bool linkage_valid = record->linkage == EVO_PROJECT_LINKAGE_NONE ||
                               record->linkage ==
                                   EVO_PROJECT_LINKAGE_INTERNAL ||
                               record->linkage ==
                                   EVO_PROJECT_LINKAGE_EXTERNAL;

    return kind_valid && linkage_valid;
}

static evo_project_analysis_status_t evo_project_analysis_copy_declarations(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    const evo_project_analysis_provider_result_t *provider_result,
    evo_project_analysis_owner_t *owner)
{
    const evo_project_analysis_status_t array_status =
        evo_project_analysis_array_status(
            provider_result->declaration_count,
            provider_result->declarations,
            config->limits.max_declarations);
    size_t index;

    if (array_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return array_status;
    }
    if (provider_result->declaration_count == 0U) {
        return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
    }
    {
        evo_project_analysis_status_t allocation_status;

        owner->declarations = evo_project_analysis_allocate_array(
            provider_result->declaration_count,
            sizeof(*owner->declarations),
            &allocation_status);
        if (allocation_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return allocation_status;
        }
    }
    owner->declaration_count = provider_result->declaration_count;
    for (index = 0U; index < owner->declaration_count; index += 1U) {
        const evo_project_declaration_record_t *source =
            &provider_result->declarations[index];
        evo_project_declaration_record_t *destination =
            &owner->declarations[index];
        evo_project_analysis_status_t status;

        if (!evo_project_analysis_declaration_enums_valid(source) ||
            !evo_project_json_text_valid(
                source->identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->name, config->limits.max_string_bytes, false) ||
            !evo_project_json_text_valid(
                source->translation_unit,
                config->limits.max_path_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->location_identity,
                config->limits.max_string_bytes,
                false) ||
            evo_project_analysis_find_unit(
                baseline_owner, source->translation_unit) == NULL ||
            evo_project_analysis_find_location(
                owner, source->location_identity) == NULL) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        status = evo_project_analysis_copy_text(
            source->identity,
            config->limits.max_string_bytes,
            (char **)&destination->identity);
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->name,
                config->limits.max_string_bytes,
                (char **)&destination->name);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->translation_unit,
                config->limits.max_path_bytes,
                (char **)&destination->translation_unit);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->location_identity,
                config->limits.max_string_bytes,
                (char **)&destination->location_identity);
        }
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
        destination->kind = source->kind;
        destination->linkage = source->linkage;
        destination->definition = source->definition;
    }
    qsort(
        owner->declarations,
        owner->declaration_count,
        sizeof(*owner->declarations),
        evo_project_analysis_declaration_compare);
    for (index = 1U; index < owner->declaration_count; index += 1U) {
        if (strcmp(
                owner->declarations[index - 1U].identity,
                owner->declarations[index].identity) == 0) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static bool evo_project_analysis_function_exists(
    const evo_project_analysis_owner_t *owner,
    const char *identity)
{
    const evo_project_declaration_record_t *record =
        evo_project_analysis_find_declaration(owner, identity);

    return record != NULL &&
           record->kind == EVO_PROJECT_DECLARATION_FUNCTION;
}

static evo_project_analysis_status_t evo_project_analysis_copy_calls(
    const evo_project_analysis_config_t *config,
    const evo_project_analysis_provider_result_t *provider_result,
    evo_project_analysis_owner_t *owner)
{
    const evo_project_analysis_status_t array_status =
        evo_project_analysis_array_status(
            provider_result->call_count,
            provider_result->calls,
            config->limits.max_calls);
    size_t index;

    if (array_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return array_status;
    }
    if (provider_result->call_count == 0U) {
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    {
        evo_project_analysis_status_t allocation_status;

        owner->calls = evo_project_analysis_allocate_array(
            provider_result->call_count,
            sizeof(*owner->calls),
            &allocation_status);
        if (allocation_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return allocation_status;
        }
    }
    owner->call_count = provider_result->call_count;
    for (index = 0U; index < owner->call_count; index += 1U) {
        const evo_project_call_record_t *source =
            &provider_result->calls[index];
        evo_project_call_record_t *destination = &owner->calls[index];
        evo_project_analysis_status_t status;

        if ((source->kind != EVO_PROJECT_CALL_DIRECT &&
             source->kind != EVO_PROJECT_CALL_INDIRECT) ||
            !evo_project_json_text_valid(
                source->identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->caller_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->callee_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->location_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_analysis_function_exists(
                owner, source->caller_identity) ||
            !evo_project_analysis_function_exists(
                owner, source->callee_identity) ||
            evo_project_analysis_find_location(
                owner, source->location_identity) == NULL) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        status = evo_project_analysis_copy_text(
            source->identity,
            config->limits.max_string_bytes,
            (char **)&destination->identity);
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->caller_identity,
                config->limits.max_string_bytes,
                (char **)&destination->caller_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->callee_identity,
                config->limits.max_string_bytes,
                (char **)&destination->callee_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->location_identity,
                config->limits.max_string_bytes,
                (char **)&destination->location_identity);
        }
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
        destination->kind = source->kind;
    }
    qsort(
        owner->calls,
        owner->call_count,
        sizeof(*owner->calls),
        evo_project_analysis_call_compare);
    for (index = 1U; index < owner->call_count; index += 1U) {
        if (strcmp(
                owner->calls[index - 1U].identity,
                owner->calls[index].identity) == 0) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static bool evo_project_analysis_control_kind_valid(
    evo_project_control_flow_kind_t kind)
{
    return kind == EVO_PROJECT_CONTROL_FALLTHROUGH ||
           kind == EVO_PROJECT_CONTROL_BRANCH_TRUE ||
           kind == EVO_PROJECT_CONTROL_BRANCH_FALSE ||
           kind == EVO_PROJECT_CONTROL_BACK_EDGE ||
           kind == EVO_PROJECT_CONTROL_RETURN;
}

static evo_project_analysis_status_t evo_project_analysis_copy_controls(
    const evo_project_analysis_config_t *config,
    const evo_project_analysis_provider_result_t *provider_result,
    evo_project_analysis_owner_t *owner)
{
    const evo_project_analysis_status_t array_status =
        evo_project_analysis_array_status(
            provider_result->control_flow_count,
            provider_result->control_flows,
            config->limits.max_control_flows);
    size_t index;

    if (array_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return array_status;
    }
    if (provider_result->control_flow_count == 0U) {
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    {
        evo_project_analysis_status_t allocation_status;

        owner->control_flows = evo_project_analysis_allocate_array(
            provider_result->control_flow_count,
            sizeof(*owner->control_flows),
            &allocation_status);
        if (allocation_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return allocation_status;
        }
    }
    owner->control_flow_count = provider_result->control_flow_count;
    for (index = 0U; index < owner->control_flow_count; index += 1U) {
        const evo_project_control_flow_record_t *source =
            &provider_result->control_flows[index];
        evo_project_control_flow_record_t *destination =
            &owner->control_flows[index];
        evo_project_analysis_status_t status;

        if (!evo_project_analysis_control_kind_valid(source->kind) ||
            !evo_project_json_text_valid(
                source->identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->function_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->from_block_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->to_block_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->location_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_analysis_function_exists(
                owner, source->function_identity) ||
            evo_project_analysis_find_location(
                owner, source->location_identity) == NULL) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        status = evo_project_analysis_copy_text(
            source->identity,
            config->limits.max_string_bytes,
            (char **)&destination->identity);
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->function_identity,
                config->limits.max_string_bytes,
                (char **)&destination->function_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->from_block_identity,
                config->limits.max_string_bytes,
                (char **)&destination->from_block_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->to_block_identity,
                config->limits.max_string_bytes,
                (char **)&destination->to_block_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->location_identity,
                config->limits.max_string_bytes,
                (char **)&destination->location_identity);
        }
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
        destination->kind = source->kind;
    }
    qsort(
        owner->control_flows,
        owner->control_flow_count,
        sizeof(*owner->control_flows),
        evo_project_analysis_control_compare);
    for (index = 1U; index < owner->control_flow_count; index += 1U) {
        if (strcmp(
                owner->control_flows[index - 1U].identity,
                owner->control_flows[index].identity) == 0) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static bool evo_project_analysis_data_kind_valid(
    evo_project_data_flow_kind_t kind)
{
    return kind == EVO_PROJECT_DATA_READ ||
           kind == EVO_PROJECT_DATA_WRITE ||
           kind == EVO_PROJECT_DATA_ADDRESS ||
           kind == EVO_PROJECT_DATA_ESCAPE;
}

static evo_project_analysis_status_t evo_project_analysis_copy_data(
    const evo_project_analysis_config_t *config,
    const evo_project_analysis_provider_result_t *provider_result,
    evo_project_analysis_owner_t *owner)
{
    const evo_project_analysis_status_t array_status =
        evo_project_analysis_array_status(
            provider_result->data_flow_count,
            provider_result->data_flows,
            config->limits.max_data_flows);
    size_t index;

    if (array_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return array_status;
    }
    if (provider_result->data_flow_count == 0U) {
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    {
        evo_project_analysis_status_t allocation_status;

        owner->data_flows = evo_project_analysis_allocate_array(
            provider_result->data_flow_count,
            sizeof(*owner->data_flows),
            &allocation_status);
        if (allocation_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return allocation_status;
        }
    }
    owner->data_flow_count = provider_result->data_flow_count;
    for (index = 0U; index < owner->data_flow_count; index += 1U) {
        const evo_project_data_flow_record_t *source =
            &provider_result->data_flows[index];
        evo_project_data_flow_record_t *destination =
            &owner->data_flows[index];
        evo_project_analysis_status_t status;

        if (!evo_project_analysis_data_kind_valid(source->kind) ||
            !evo_project_json_text_valid(
                source->identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->function_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->declaration_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->location_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_analysis_function_exists(
                owner, source->function_identity) ||
            evo_project_analysis_find_declaration(
                owner, source->declaration_identity) == NULL ||
            evo_project_analysis_find_location(
                owner, source->location_identity) == NULL) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        status = evo_project_analysis_copy_text(
            source->identity,
            config->limits.max_string_bytes,
            (char **)&destination->identity);
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->function_identity,
                config->limits.max_string_bytes,
                (char **)&destination->function_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->declaration_identity,
                config->limits.max_string_bytes,
                (char **)&destination->declaration_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->location_identity,
                config->limits.max_string_bytes,
                (char **)&destination->location_identity);
        }
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
        destination->kind = source->kind;
    }
    qsort(
        owner->data_flows,
        owner->data_flow_count,
        sizeof(*owner->data_flows),
        evo_project_analysis_data_compare);
    for (index = 1U; index < owner->data_flow_count; index += 1U) {
        if (strcmp(
                owner->data_flows[index - 1U].identity,
                owner->data_flows[index].identity) == 0) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static bool evo_project_analysis_optimization_kind_valid(
    evo_project_optimization_disposition_t disposition)
{
    return disposition == EVO_PROJECT_OPTIMIZATION_PASSED ||
           disposition == EVO_PROJECT_OPTIMIZATION_MISSED ||
           disposition == EVO_PROJECT_OPTIMIZATION_ANALYSIS;
}

static evo_project_analysis_status_t evo_project_analysis_copy_optimizations(
    const evo_project_analysis_config_t *config,
    const evo_project_analysis_provider_result_t *provider_result,
    evo_project_analysis_owner_t *owner)
{
    const evo_project_analysis_status_t array_status =
        evo_project_analysis_array_status(
            provider_result->optimization_record_count,
            provider_result->optimization_records,
            config->limits.max_optimization_records);
    size_t index;

    if (array_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return array_status;
    }
    if (provider_result->optimization_record_count == 0U) {
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    {
        evo_project_analysis_status_t allocation_status;

        owner->optimization_records = evo_project_analysis_allocate_array(
            provider_result->optimization_record_count,
            sizeof(*owner->optimization_records),
            &allocation_status);
        if (allocation_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return allocation_status;
        }
    }
    owner->optimization_record_count =
        provider_result->optimization_record_count;
    for (index = 0U; index < owner->optimization_record_count; index += 1U) {
        const evo_project_optimization_record_t *source =
            &provider_result->optimization_records[index];
        evo_project_optimization_record_t *destination =
            &owner->optimization_records[index];
        evo_project_analysis_status_t status;

        if (!evo_project_analysis_optimization_kind_valid(
                source->disposition) ||
            !evo_project_json_text_valid(
                source->identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->pass_name,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->function_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->location_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->message,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_analysis_function_exists(
                owner, source->function_identity) ||
            evo_project_analysis_find_location(
                owner, source->location_identity) == NULL) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        status = evo_project_analysis_copy_text(
            source->identity,
            config->limits.max_string_bytes,
            (char **)&destination->identity);
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->pass_name,
                config->limits.max_string_bytes,
                (char **)&destination->pass_name);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->function_identity,
                config->limits.max_string_bytes,
                (char **)&destination->function_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->location_identity,
                config->limits.max_string_bytes,
                (char **)&destination->location_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->message,
                config->limits.max_string_bytes,
                (char **)&destination->message);
        }
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
        destination->disposition = source->disposition;
    }
    qsort(
        owner->optimization_records,
        owner->optimization_record_count,
        sizeof(*owner->optimization_records),
        evo_project_analysis_optimization_compare);
    for (index = 1U; index < owner->optimization_record_count; index += 1U) {
        if (strcmp(
                owner->optimization_records[index - 1U].identity,
                owner->optimization_records[index].identity) == 0) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static evo_project_analysis_status_t evo_project_analysis_copy_runtime(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    const evo_project_analysis_provider_result_t *provider_result,
    evo_project_analysis_owner_t *owner)
{
    const evo_project_analysis_status_t array_status =
        evo_project_analysis_array_status(
            provider_result->runtime_record_count,
            provider_result->runtime_records,
            config->limits.max_runtime_records);
    size_t index;

    if (array_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return array_status;
    }
    if (config->runtime_profile_state != EVO_PROJECT_RUNTIME_AVAILABLE &&
        provider_result->runtime_record_count != 0U) {
        return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
    }
    if (provider_result->runtime_record_count == 0U) {
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    {
        evo_project_analysis_status_t allocation_status;

        owner->runtime_records = evo_project_analysis_allocate_array(
            provider_result->runtime_record_count,
            sizeof(*owner->runtime_records),
            &allocation_status);
        if (allocation_status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return allocation_status;
        }
    }
    owner->runtime_record_count = provider_result->runtime_record_count;
    for (index = 0U; index < owner->runtime_record_count; index += 1U) {
        const evo_project_runtime_record_t *source =
            &provider_result->runtime_records[index];
        evo_project_runtime_record_t *destination =
            &owner->runtime_records[index];
        evo_project_analysis_status_t status;

        if (source->metric != EVO_PROJECT_RUNTIME_SAMPLE_COUNT ||
            source->value == 0U ||
            !evo_project_json_text_valid(
                source->identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->workload_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->function_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_json_text_valid(
                source->location_identity,
                config->limits.max_string_bytes,
                false) ||
            !evo_project_analysis_workload_exists(
                baseline_owner, source->workload_identity) ||
            !evo_project_analysis_function_exists(
                owner, source->function_identity) ||
            evo_project_analysis_find_location(
                owner, source->location_identity) == NULL) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        status = evo_project_analysis_copy_text(
            source->identity,
            config->limits.max_string_bytes,
            (char **)&destination->identity);
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->workload_identity,
                config->limits.max_string_bytes,
                (char **)&destination->workload_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->function_identity,
                config->limits.max_string_bytes,
                (char **)&destination->function_identity);
        }
        if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
            status = evo_project_analysis_copy_text(
                source->location_identity,
                config->limits.max_string_bytes,
                (char **)&destination->location_identity);
        }
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
        destination->metric = source->metric;
        destination->value = source->value;
    }
    qsort(
        owner->runtime_records,
        owner->runtime_record_count,
        sizeof(*owner->runtime_records),
        evo_project_analysis_runtime_compare);
    for (index = 1U; index < owner->runtime_record_count; index += 1U) {
        if (strcmp(
                owner->runtime_records[index - 1U].identity,
                owner->runtime_records[index].identity) == 0) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static evo_project_opportunity_record_t *evo_project_analysis_find_opportunity(
    evo_project_analysis_owner_t *owner,
    const char *location_identity)
{
    size_t index;

    for (index = 0U; index < owner->opportunity_count; index += 1U) {
        if (strcmp(
                owner->opportunities[index].location_identity,
                location_identity) == 0) {
            return &owner->opportunities[index];
        }
    }
    return NULL;
}

static evo_project_analysis_status_t evo_project_analysis_add_opportunity(
    const evo_project_analysis_config_t *config,
    evo_project_analysis_owner_t *owner,
    const char *location_identity,
    bool missed,
    uint64_t runtime_samples)
{
    evo_project_opportunity_record_t *record =
        evo_project_analysis_find_opportunity(owner, location_identity);

    if (record == NULL) {
        const evo_project_source_location_record_t *location;

        if (owner->opportunity_count >= config->limits.max_opportunities) {
            return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
        }
        location = evo_project_analysis_find_location(
            owner, location_identity);
        if (location == NULL) {
            return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
        }
        record = &owner->opportunities[owner->opportunity_count];
        record->location_identity = location->identity;
        owner->opportunity_count += 1U;
    }
    if (missed) {
        if (record->missed_optimization_count == SIZE_MAX) {
            return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
        }
        record->missed_optimization_count += 1U;
    }
    if (runtime_samples > 0U) {
        if (record->runtime_sample_count > UINT64_MAX - runtime_samples) {
            return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
        }
        record->runtime_evidence_present = true;
        record->runtime_sample_count += runtime_samples;
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static evo_project_analysis_status_t evo_project_analysis_build_opportunities(
    const evo_project_analysis_config_t *config,
    evo_project_analysis_owner_t *owner)
{
    size_t missed_count = 0U;
    size_t maximum_count;
    size_t index;
    evo_project_analysis_status_t status;

    for (index = 0U; index < owner->optimization_record_count; index += 1U) {
        if (owner->optimization_records[index].disposition ==
            EVO_PROJECT_OPTIMIZATION_MISSED) {
            if (missed_count == SIZE_MAX) {
                return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
            }
            missed_count += 1U;
        }
    }
    if (missed_count > SIZE_MAX - owner->runtime_record_count) {
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    maximum_count = missed_count + owner->runtime_record_count;
    if (maximum_count == 0U) {
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    if (maximum_count > config->limits.max_opportunities) {
        maximum_count = config->limits.max_opportunities;
    }
    owner->opportunities = evo_project_analysis_allocate_array(
        maximum_count, sizeof(*owner->opportunities), &status);
    if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return status;
    }
    for (index = 0U; index < owner->optimization_record_count; index += 1U) {
        const evo_project_optimization_record_t *record =
            &owner->optimization_records[index];

        if (record->disposition != EVO_PROJECT_OPTIMIZATION_MISSED) {
            continue;
        }
        status = evo_project_analysis_add_opportunity(
            config, owner, record->location_identity, true, 0U);
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
    }
    for (index = 0U; index < owner->runtime_record_count; index += 1U) {
        const evo_project_runtime_record_t *record =
            &owner->runtime_records[index];

        status = evo_project_analysis_add_opportunity(
            config, owner, record->location_identity, false, record->value);
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            return status;
        }
    }
    qsort(
        owner->opportunities,
        owner->opportunity_count,
        sizeof(*owner->opportunities),
        evo_project_analysis_opportunity_compare);
    for (index = 0U; index < owner->opportunity_count; index += 1U) {
        owner->opportunities[index].rank = index + 1U;
    }
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static void evo_project_analysis_fingerprint_text(
    evo_project_fingerprint_t *fingerprint,
    const char *value)
{
    evo_project_fingerprint_string(
        fingerprint, value == NULL ? "" : value);
}

static void evo_project_analysis_compute_fingerprint(
    evo_project_analysis_owner_t *owner)
{
    evo_project_fingerprint_t fingerprint;
    size_t index;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, "catalyst.evo-project-analysis.v1");
    evo_project_fingerprint_string(
        &fingerprint, owner->baseline_fingerprint);
    evo_project_fingerprint_string(&fingerprint, owner->provider_identity);
    evo_project_fingerprint_string(&fingerprint, owner->clang_identity);
    evo_project_fingerprint_string(&fingerprint, owner->llvm_identity);
    evo_project_fingerprint_string(&fingerprint, owner->target_identity);
    evo_project_fingerprint_string(&fingerprint, owner->flags_identity);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)owner->runtime_profile_state);
    evo_project_analysis_fingerprint_text(
        &fingerprint, owner->runtime_profile_identity);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)owner->translation_unit_count);
    for (index = 0U; index < owner->translation_unit_count; index += 1U) {
        evo_project_fingerprint_string(
            &fingerprint, owner->translation_units[index]);
    }
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)owner->source_location_count);
    for (index = 0U; index < owner->source_location_count; index += 1U) {
        const evo_project_source_location_record_t *record =
            &owner->source_locations[index];

        evo_project_fingerprint_string(&fingerprint, record->identity);
        evo_project_fingerprint_string(&fingerprint, record->file);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->line);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->column);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->end_line);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->end_column);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->kind);
        evo_project_analysis_fingerprint_text(
            &fingerprint, record->spelling_identity);
    }
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)owner->declaration_count);
    for (index = 0U; index < owner->declaration_count; index += 1U) {
        const evo_project_declaration_record_t *record =
            &owner->declarations[index];

        evo_project_fingerprint_string(&fingerprint, record->identity);
        evo_project_fingerprint_string(&fingerprint, record->name);
        evo_project_fingerprint_string(
            &fingerprint, record->translation_unit);
        evo_project_fingerprint_string(
            &fingerprint, record->location_identity);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->kind);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->linkage);
        evo_project_fingerprint_u64(
            &fingerprint, record->definition ? 1U : 0U);
    }
#define EVO_PROJECT_FINGERPRINT_RELATION(records, count, BODY)        \
    do {                                                              \
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)(count)); \
        for (index = 0U; index < (count); index += 1U) {              \
            BODY                                                      \
        }                                                             \
    } while (false)
    EVO_PROJECT_FINGERPRINT_RELATION(
        owner->calls,
        owner->call_count,
        {
            const evo_project_call_record_t *record = &owner->calls[index];
            evo_project_fingerprint_string(&fingerprint, record->identity);
            evo_project_fingerprint_string(
                &fingerprint, record->caller_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->callee_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->location_identity);
            evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->kind);
        });
    EVO_PROJECT_FINGERPRINT_RELATION(
        owner->control_flows,
        owner->control_flow_count,
        {
            const evo_project_control_flow_record_t *record =
                &owner->control_flows[index];
            evo_project_fingerprint_string(&fingerprint, record->identity);
            evo_project_fingerprint_string(
                &fingerprint, record->function_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->from_block_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->to_block_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->location_identity);
            evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->kind);
        });
    EVO_PROJECT_FINGERPRINT_RELATION(
        owner->data_flows,
        owner->data_flow_count,
        {
            const evo_project_data_flow_record_t *record =
                &owner->data_flows[index];
            evo_project_fingerprint_string(&fingerprint, record->identity);
            evo_project_fingerprint_string(
                &fingerprint, record->function_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->declaration_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->location_identity);
            evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->kind);
        });
    EVO_PROJECT_FINGERPRINT_RELATION(
        owner->optimization_records,
        owner->optimization_record_count,
        {
            const evo_project_optimization_record_t *record =
                &owner->optimization_records[index];
            evo_project_fingerprint_string(&fingerprint, record->identity);
            evo_project_fingerprint_string(&fingerprint, record->pass_name);
            evo_project_fingerprint_string(
                &fingerprint, record->function_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->location_identity);
            evo_project_fingerprint_string(&fingerprint, record->message);
            evo_project_fingerprint_u64(
                &fingerprint, (uint64_t)record->disposition);
        });
    EVO_PROJECT_FINGERPRINT_RELATION(
        owner->runtime_records,
        owner->runtime_record_count,
        {
            const evo_project_runtime_record_t *record =
                &owner->runtime_records[index];
            evo_project_fingerprint_string(&fingerprint, record->identity);
            evo_project_fingerprint_string(
                &fingerprint, record->workload_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->function_identity);
            evo_project_fingerprint_string(
                &fingerprint, record->location_identity);
            evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->metric);
            evo_project_fingerprint_u64(&fingerprint, record->value);
        });
    EVO_PROJECT_FINGERPRINT_RELATION(
        owner->opportunities,
        owner->opportunity_count,
        {
            const evo_project_opportunity_record_t *record =
                &owner->opportunities[index];
            evo_project_fingerprint_u64(&fingerprint, (uint64_t)record->rank);
            evo_project_fingerprint_string(
                &fingerprint, record->location_identity);
            evo_project_fingerprint_u64(
                &fingerprint,
                (uint64_t)record->missed_optimization_count);
            evo_project_fingerprint_u64(
                &fingerprint, record->runtime_evidence_present ? 1U : 0U);
            evo_project_fingerprint_u64(
                &fingerprint, record->runtime_sample_count);
        });
#undef EVO_PROJECT_FINGERPRINT_RELATION
    owner->analysis_fingerprint = fingerprint.value;
}

evo_project_analysis_status_t evo_project_analysis_model_build(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    const evo_project_analysis_provider_result_t *provider_result,
    evo_project_analysis_owner_t *owner)
{
    evo_project_analysis_status_t status;

    status = evo_project_analysis_copy_text(
        config->baseline->baseline_fingerprint,
        EVO_PROJECT_FINGERPRINT_TEXT_SIZE - 1U,
        &owner->baseline_fingerprint);
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_text(
            config->provider_identity,
            config->limits.max_string_bytes,
            &owner->provider_identity);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_text(
            config->clang_identity,
            config->limits.max_string_bytes,
            &owner->clang_identity);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_text(
            config->llvm_identity,
            config->limits.max_string_bytes,
            &owner->llvm_identity);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_text(
            config->target_identity,
            config->limits.max_string_bytes,
            &owner->target_identity);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_text(
            config->flags_identity,
            config->limits.max_string_bytes,
            &owner->flags_identity);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_nullable_text(
            config->runtime_profile_identity,
            config->limits.max_string_bytes,
            &owner->runtime_profile_identity);
    }
    if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
        return status;
    }
    owner->runtime_profile_state = config->runtime_profile_state;
    status = evo_project_analysis_copy_units(config, baseline_owner, owner);
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_locations(
            config, baseline_owner, provider_result, owner);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_declarations(
            config, baseline_owner, provider_result, owner);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_calls(
            config, provider_result, owner);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_controls(
            config, provider_result, owner);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_data(
            config, provider_result, owner);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_optimizations(
            config, provider_result, owner);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_copy_runtime(
            config, baseline_owner, provider_result, owner);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_build_opportunities(config, owner);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        evo_project_analysis_compute_fingerprint(owner);
    }
    return status;
}

void evo_project_analysis_model_destroy(evo_project_analysis_owner_t *owner)
{
    size_t index;

    if (owner == NULL) {
        return;
    }
    for (index = 0U; index < owner->translation_unit_count; index += 1U) {
        evo_project_release(owner->translation_units[index]);
    }
    for (index = 0U; index < owner->source_location_count; index += 1U) {
        evo_project_release((void *)owner->source_locations[index].identity);
        evo_project_release((void *)owner->source_locations[index].file);
        evo_project_release(
            (void *)owner->source_locations[index].spelling_identity);
    }
    for (index = 0U; index < owner->declaration_count; index += 1U) {
        evo_project_release((void *)owner->declarations[index].identity);
        evo_project_release((void *)owner->declarations[index].name);
        evo_project_release(
            (void *)owner->declarations[index].translation_unit);
        evo_project_release(
            (void *)owner->declarations[index].location_identity);
    }
    for (index = 0U; index < owner->call_count; index += 1U) {
        evo_project_release((void *)owner->calls[index].identity);
        evo_project_release((void *)owner->calls[index].caller_identity);
        evo_project_release((void *)owner->calls[index].callee_identity);
        evo_project_release((void *)owner->calls[index].location_identity);
    }
    for (index = 0U; index < owner->control_flow_count; index += 1U) {
        evo_project_release((void *)owner->control_flows[index].identity);
        evo_project_release(
            (void *)owner->control_flows[index].function_identity);
        evo_project_release(
            (void *)owner->control_flows[index].from_block_identity);
        evo_project_release(
            (void *)owner->control_flows[index].to_block_identity);
        evo_project_release(
            (void *)owner->control_flows[index].location_identity);
    }
    for (index = 0U; index < owner->data_flow_count; index += 1U) {
        evo_project_release((void *)owner->data_flows[index].identity);
        evo_project_release(
            (void *)owner->data_flows[index].function_identity);
        evo_project_release(
            (void *)owner->data_flows[index].declaration_identity);
        evo_project_release(
            (void *)owner->data_flows[index].location_identity);
    }
    for (index = 0U; index < owner->optimization_record_count; index += 1U) {
        evo_project_release(
            (void *)owner->optimization_records[index].identity);
        evo_project_release(
            (void *)owner->optimization_records[index].pass_name);
        evo_project_release(
            (void *)owner->optimization_records[index].function_identity);
        evo_project_release(
            (void *)owner->optimization_records[index].location_identity);
        evo_project_release(
            (void *)owner->optimization_records[index].message);
    }
    for (index = 0U; index < owner->runtime_record_count; index += 1U) {
        evo_project_release((void *)owner->runtime_records[index].identity);
        evo_project_release(
            (void *)owner->runtime_records[index].workload_identity);
        evo_project_release(
            (void *)owner->runtime_records[index].function_identity);
        evo_project_release(
            (void *)owner->runtime_records[index].location_identity);
    }
    evo_project_release(owner->translation_units);
    evo_project_release(owner->source_locations);
    evo_project_release(owner->declarations);
    evo_project_release(owner->calls);
    evo_project_release(owner->control_flows);
    evo_project_release(owner->data_flows);
    evo_project_release(owner->optimization_records);
    evo_project_release(owner->runtime_records);
    evo_project_release(owner->opportunities);
    evo_project_release(owner->output_path);
    evo_project_release(owner->baseline_fingerprint);
    evo_project_release(owner->provider_identity);
    evo_project_release(owner->clang_identity);
    evo_project_release(owner->llvm_identity);
    evo_project_release(owner->target_identity);
    evo_project_release(owner->flags_identity);
    evo_project_release(owner->runtime_profile_identity);
    *owner = (evo_project_analysis_owner_t){0};
}
