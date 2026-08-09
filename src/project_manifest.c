#include "internal/project_manifest.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <stdlib.h>
#include <string.h>

static evo_project_status_t evo_project_manifest_json_status(
    evo_project_json_status_t status)
{
    switch (status) {
    case EVO_PROJECT_JSON_SUCCESS:
        return EVO_PROJECT_SUCCESS;
    case EVO_PROJECT_JSON_RESOURCE_LIMIT:
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    case EVO_PROJECT_JSON_OUT_OF_MEMORY:
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    case EVO_PROJECT_JSON_INVALID:
    default:
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
}

static bool evo_project_limits_valid(
    const evo_project_ingest_limits_t *limits)
{
    return limits != NULL && limits->max_manifest_bytes > 0U &&
           limits->max_json_tokens > 0U && limits->max_json_depth > 0U &&
           limits->max_string_bytes > 0U && limits->max_path_bytes > 0U &&
           limits->max_files > 0U && limits->max_file_bytes > 0U &&
           limits->max_total_bytes > 0U &&
           limits->max_compilation_database_bytes > 0U &&
           limits->max_permitted_roots > 0U &&
           limits->max_dependencies > 0U &&
           limits->max_toolchains > 0U &&
           limits->max_environment_entries > 0U &&
           limits->max_targets > 0U && limits->max_workloads > 0U &&
           limits->max_constraints > 0U &&
           limits->max_command_args > 0U &&
           limits->max_command_bytes > 0U &&
           limits->max_command_output_bytes > 0U &&
           limits->max_evidence_bytes > 0U &&
           limits->max_command_timeout_ms > 0U &&
           limits->max_memory_bytes > 0U &&
           limits->max_processes > 0U && limits->max_storage_bytes > 0U;
}

static bool evo_project_text_nonempty(const char *value)
{
    size_t index;

    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (index = 0U; value[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)value[index];

        if (byte < 0x20U || byte == 0x7fU) {
            return false;
        }
    }
    return true;
}

static bool evo_project_environment_name_valid(const char *value)
{
    size_t index;

    if (value == NULL ||
        !((value[0] >= 'A' && value[0] <= 'Z') || value[0] == '_')) {
        return false;
    }
    for (index = 1U; value[index] != '\0'; index += 1U) {
        if (!((value[index] >= 'A' && value[index] <= 'Z') ||
              (value[index] >= '0' && value[index] <= '9') ||
              value[index] == '_')) {
            return false;
        }
    }
    return true;
}

bool evo_project_relative_path_valid(
    const char *path,
    size_t maximum_bytes)
{
    size_t size;
    size_t component_start = 0U;
    size_t index;

    if (path == NULL || maximum_bytes == 0U) {
        return false;
    }
    size = strlen(path);
    if (size == 0U || size > maximum_bytes || path[0] == '/' ||
        path[size - 1U] == '/') {
        return false;
    }
    for (index = 0U; index <= size; index += 1U) {
        if (index < size) {
            const unsigned char byte = (unsigned char)path[index];

            if (byte < 0x20U || byte == 0x7fU || path[index] == '\\' ||
                path[index] == ':') {
                return false;
            }
        }
        if (index == size || path[index] == '/') {
            const size_t component_size = index - component_start;

            if (component_size == 0U ||
                (component_size == 1U && path[component_start] == '.') ||
                (component_size == 2U && path[component_start] == '.' &&
                 path[component_start + 1U] == '.')) {
                return false;
            }
            component_start = index + 1U;
        }
    }
    return true;
}

static evo_project_status_t evo_project_require_member(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *name,
    size_t *value_index)
{
    const int found = evo_project_json_object_get(
        text, tokens, token_count, object_index, name, value_index);

    return found == 1 ? EVO_PROJECT_SUCCESS
                      : EVO_PROJECT_ERROR_MANIFEST_INVALID;
}

static evo_project_status_t evo_project_decode_token_string(
    const char *text,
    const evo_project_json_token_t *token,
    size_t maximum_bytes,
    bool allow_empty,
    char **value)
{
    const evo_project_json_status_t json_status =
        evo_project_json_decode_string(text, token, maximum_bytes, value);
    evo_project_status_t status =
        evo_project_manifest_json_status(json_status);

    if (status == EVO_PROJECT_SUCCESS &&
        ((!allow_empty && !evo_project_text_nonempty(*value)) ||
         (allow_empty && (*value)[0] != '\0' &&
          !evo_project_text_nonempty(*value)))) {
        evo_project_release(*value);
        *value = NULL;
        status = EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    return status;
}

static evo_project_status_t evo_project_parse_string_member(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *name,
    size_t maximum_bytes,
    bool allow_empty,
    char **value)
{
    size_t value_index;
    evo_project_status_t status = evo_project_require_member(
        text, tokens, token_count, object_index, name, &value_index);

    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    return evo_project_decode_token_string(
        text, &tokens[value_index], maximum_bytes, allow_empty, value);
}

static evo_project_status_t evo_project_parse_bool_member(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *name,
    bool *value)
{
    size_t value_index;
    evo_project_status_t status = evo_project_require_member(
        text, tokens, token_count, object_index, name, &value_index);

    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    return evo_project_json_parse_bool(&tokens[value_index], value)
               ? EVO_PROJECT_SUCCESS
               : EVO_PROJECT_ERROR_MANIFEST_INVALID;
}

static evo_project_status_t evo_project_parse_u64_member(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *name,
    uint64_t *value)
{
    size_t value_index;
    evo_project_status_t status = evo_project_require_member(
        text, tokens, token_count, object_index, name, &value_index);

    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    return evo_project_json_parse_u64(text, &tokens[value_index], value)
               ? EVO_PROJECT_SUCCESS
               : EVO_PROJECT_ERROR_MANIFEST_INVALID;
}

static evo_project_status_t evo_project_parse_size_member(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *name,
    size_t *value)
{
    uint64_t parsed;
    evo_project_status_t status = evo_project_parse_u64_member(
        text, tokens, token_count, object_index, name, &parsed);

    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    if (parsed > (uint64_t)SIZE_MAX) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    *value = (size_t)parsed;
    return EVO_PROJECT_SUCCESS;
}

static int evo_project_string_pointer_compare(
    const void *left_value,
    const void *right_value)
{
    const char *const *left = left_value;
    const char *const *right = right_value;

    return strcmp(*left, *right);
}

static int evo_project_named_identity_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_named_identity_t *left = left_value;
    const evo_project_named_identity_t *right = right_value;
    const int name_order = strcmp(left->name, right->name);

    return name_order != 0 ? name_order : strcmp(left->identity, right->identity);
}

static int evo_project_environment_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_environment_entry_t *left = left_value;
    const evo_project_environment_entry_t *right = right_value;

    return strcmp(left->name, right->name);
}

static void evo_project_free_string_array(char **values, size_t count)
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

static evo_project_status_t evo_project_parse_string_array(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t array_index,
    size_t maximum_count,
    size_t maximum_string_bytes,
    bool require_nonempty_array,
    bool paths,
    char ***values,
    size_t *value_count)
{
    char **parsed = NULL;
    size_t count;
    size_t item;
    size_t token_index;

    if (array_index >= token_count ||
        tokens[array_index].type != EVO_PROJECT_JSON_ARRAY) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    count = tokens[array_index].child_count;
    if ((require_nonempty_array && count == 0U) || count > maximum_count) {
        return count > maximum_count ? EVO_PROJECT_ERROR_RESOURCE_LIMIT
                                     : EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    if (count > 0U) {
        if (count > SIZE_MAX / sizeof(*parsed)) {
            return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
        }
        parsed = evo_project_allocate_zeroed(count, sizeof(*parsed));
        if (parsed == NULL) {
            return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
        }
    }

    token_index = array_index + 1U;
    for (item = 0U; item < count; item += 1U) {
        evo_project_status_t status;

        if (token_index >= token_count ||
            tokens[token_index].parent != array_index) {
            evo_project_free_string_array(parsed, item);
            return EVO_PROJECT_ERROR_MANIFEST_INVALID;
        }
        status = evo_project_decode_token_string(
            text,
            &tokens[token_index],
            maximum_string_bytes,
            false,
            &parsed[item]);
        if (status != EVO_PROJECT_SUCCESS ||
            (paths && !evo_project_relative_path_valid(
                          parsed[item], maximum_string_bytes))) {
            evo_project_free_string_array(parsed, item + 1U);
            return status != EVO_PROJECT_SUCCESS
                       ? status
                       : EVO_PROJECT_ERROR_PATH_INVALID;
        }
        token_index = evo_project_json_next(
            tokens, token_count, token_index);
    }
    if (count > 1U) {
        qsort(parsed, count, sizeof(*parsed), evo_project_string_pointer_compare);
        for (item = 1U; item < count; item += 1U) {
            if (strcmp(parsed[item - 1U], parsed[item]) == 0) {
                evo_project_free_string_array(parsed, count);
                return EVO_PROJECT_ERROR_MANIFEST_INVALID;
            }
        }
    }
    *values = parsed;
    *value_count = count;
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_parse_named_identities(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t array_index,
    size_t maximum_count,
    size_t maximum_string_bytes,
    bool require_nonempty,
    evo_project_named_identity_t **values,
    size_t *value_count)
{
    static const char *const allowed[] = {"name", "identity"};
    evo_project_named_identity_t *parsed = NULL;
    size_t count;
    size_t item;
    size_t token_index;

    if (array_index >= token_count ||
        tokens[array_index].type != EVO_PROJECT_JSON_ARRAY) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    count = tokens[array_index].child_count;
    if ((require_nonempty && count == 0U) || count > maximum_count) {
        return count > maximum_count ? EVO_PROJECT_ERROR_RESOURCE_LIMIT
                                     : EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    if (count > 0U) {
        if (count > SIZE_MAX / sizeof(*parsed)) {
            return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
        }
        parsed = evo_project_allocate_zeroed(count, sizeof(*parsed));
        if (parsed == NULL) {
            return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
        }
    }

    token_index = array_index + 1U;
    for (item = 0U; item < count; item += 1U) {
        evo_project_status_t status;

        if (token_index >= token_count ||
            tokens[token_index].parent != array_index ||
            tokens[token_index].type != EVO_PROJECT_JSON_OBJECT ||
            !evo_project_json_object_has_only(
                text,
                tokens,
                token_count,
                token_index,
                allowed,
                sizeof(allowed) / sizeof(allowed[0]))) {
            status = EVO_PROJECT_ERROR_MANIFEST_INVALID;
        } else {
            status = evo_project_parse_string_member(
                text,
                tokens,
                token_count,
                token_index,
                "name",
                maximum_string_bytes,
                false,
                &parsed[item].name);
            if (status == EVO_PROJECT_SUCCESS) {
                status = evo_project_parse_string_member(
                    text,
                    tokens,
                    token_count,
                    token_index,
                    "identity",
                    maximum_string_bytes,
                    false,
                    &parsed[item].identity);
            }
        }
        if (status != EVO_PROJECT_SUCCESS) {
            size_t release;

            for (release = 0U; release <= item; release += 1U) {
                evo_project_release(parsed[release].name);
                evo_project_release(parsed[release].identity);
            }
            evo_project_release(parsed);
            return status;
        }
        token_index = evo_project_json_next(
            tokens, token_count, token_index);
    }
    if (count > 1U) {
        qsort(parsed, count, sizeof(*parsed), evo_project_named_identity_compare);
        for (item = 1U; item < count; item += 1U) {
            if (strcmp(parsed[item - 1U].name, parsed[item].name) == 0) {
                size_t release;

                for (release = 0U; release < count; release += 1U) {
                    evo_project_release(parsed[release].name);
                    evo_project_release(parsed[release].identity);
                }
                evo_project_release(parsed);
                return EVO_PROJECT_ERROR_MANIFEST_INVALID;
            }
        }
    }
    *values = parsed;
    *value_count = count;
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_parse_environment(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t array_index,
    const evo_project_ingest_limits_t *limits,
    evo_project_environment_entry_t **values,
    size_t *value_count)
{
    static const char *const allowed[] = {"name", "value"};
    evo_project_environment_entry_t *parsed = NULL;
    size_t count;
    size_t item;
    size_t token_index;

    if (array_index >= token_count ||
        tokens[array_index].type != EVO_PROJECT_JSON_ARRAY) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    count = tokens[array_index].child_count;
    if (count > limits->max_environment_entries) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    if (count > 0U) {
        if (count > SIZE_MAX / sizeof(*parsed)) {
            return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
        }
        parsed = evo_project_allocate_zeroed(count, sizeof(*parsed));
        if (parsed == NULL) {
            return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
        }
    }
    token_index = array_index + 1U;
    for (item = 0U; item < count; item += 1U) {
        evo_project_status_t status;

        if (token_index >= token_count ||
            tokens[token_index].parent != array_index ||
            tokens[token_index].type != EVO_PROJECT_JSON_OBJECT ||
            !evo_project_json_object_has_only(
                text,
                tokens,
                token_count,
                token_index,
                allowed,
                sizeof(allowed) / sizeof(allowed[0]))) {
            status = EVO_PROJECT_ERROR_MANIFEST_INVALID;
        } else {
            status = evo_project_parse_string_member(
                text,
                tokens,
                token_count,
                token_index,
                "name",
                limits->max_string_bytes,
                false,
                &parsed[item].name);
            if (status == EVO_PROJECT_SUCCESS &&
                !evo_project_environment_name_valid(parsed[item].name)) {
                status = EVO_PROJECT_ERROR_MANIFEST_INVALID;
            }
            if (status == EVO_PROJECT_SUCCESS) {
                status = evo_project_parse_string_member(
                    text,
                    tokens,
                    token_count,
                    token_index,
                    "value",
                    limits->max_string_bytes,
                    true,
                    &parsed[item].value);
            }
        }
        if (status != EVO_PROJECT_SUCCESS) {
            size_t release;

            for (release = 0U; release <= item; release += 1U) {
                evo_project_release(parsed[release].name);
                evo_project_release(parsed[release].value);
            }
            evo_project_release(parsed);
            return status;
        }
        token_index = evo_project_json_next(
            tokens, token_count, token_index);
    }
    if (count > 1U) {
        qsort(parsed, count, sizeof(*parsed), evo_project_environment_compare);
        for (item = 1U; item < count; item += 1U) {
            if (strcmp(parsed[item - 1U].name, parsed[item].name) == 0) {
                size_t release;

                for (release = 0U; release < count; release += 1U) {
                    evo_project_release(parsed[release].name);
                    evo_project_release(parsed[release].value);
                }
                evo_project_release(parsed);
                return EVO_PROJECT_ERROR_MANIFEST_INVALID;
            }
        }
    }
    *values = parsed;
    *value_count = count;
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_parse_command(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t array_index,
    const evo_project_ingest_limits_t *limits,
    evo_project_command_stage_t stage,
    const char *stage_id,
    evo_project_manifest_command_t *command)
{
    char **arguments = NULL;
    size_t count;
    size_t item;
    size_t token_index;
    size_t total_bytes = 0U;

    if (array_index >= token_count ||
        tokens[array_index].type != EVO_PROJECT_JSON_ARRAY) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    count = tokens[array_index].child_count;
    if (count == 0U || count > limits->max_command_args) {
        return count > limits->max_command_args
                   ? EVO_PROJECT_ERROR_RESOURCE_LIMIT
                   : EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    if (count > SIZE_MAX / sizeof(*arguments)) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    arguments = evo_project_allocate_zeroed(count, sizeof(*arguments));
    if (arguments == NULL) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    token_index = array_index + 1U;
    for (item = 0U; item < count; item += 1U) {
        evo_project_status_t status;
        size_t argument_size;

        if (token_index >= token_count ||
            tokens[token_index].parent != array_index) {
            evo_project_free_string_array(arguments, item);
            return EVO_PROJECT_ERROR_MANIFEST_INVALID;
        }
        status = evo_project_decode_token_string(
            text,
            &tokens[token_index],
            limits->max_string_bytes,
            false,
            &arguments[item]);
        if (status != EVO_PROJECT_SUCCESS) {
            evo_project_free_string_array(arguments, item + 1U);
            return status;
        }
        argument_size = strlen(arguments[item]);
        if (argument_size == SIZE_MAX ||
            argument_size >= limits->max_command_bytes ||
            total_bytes > limits->max_command_bytes - argument_size - 1U) {
            evo_project_free_string_array(arguments, item + 1U);
            return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
        }
        total_bytes += argument_size + 1U;
        token_index = evo_project_json_next(
            tokens, token_count, token_index);
    }
    command->stage = stage;
    command->stage_id = stage_id;
    command->argument_count = count;
    command->arguments = arguments;
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_parse_source(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const evo_project_ingest_limits_t *limits,
    evo_project_manifest_t *manifest)
{
    static const char *const allowed[] = {
        "declared_identity",
        "permitted_roots",
        "compilation_database",
        "generated_source_policy"};
    size_t roots_index;
    evo_project_status_t status;
    size_t index;

    if (tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_json_object_has_only(
            text,
            tokens,
            token_count,
            object_index,
            allowed,
            sizeof(allowed) / sizeof(allowed[0]))) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    status = evo_project_parse_string_member(
        text,
        tokens,
        token_count,
        object_index,
        "declared_identity",
        limits->max_string_bytes,
        false,
        &manifest->source_identity);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    status = evo_project_require_member(
        text,
        tokens,
        token_count,
        object_index,
        "permitted_roots",
        &roots_index);
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_parse_string_array(
            text,
            tokens,
            token_count,
            roots_index,
            limits->max_permitted_roots,
            limits->max_path_bytes,
            true,
            true,
            &manifest->permitted_roots,
            &manifest->permitted_root_count);
    }
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    for (index = 1U; index < manifest->permitted_root_count; index += 1U) {
        const char *previous = manifest->permitted_roots[index - 1U];
        const char *current = manifest->permitted_roots[index];
        const size_t previous_size = strlen(previous);

        if (strncmp(previous, current, previous_size) == 0 &&
            current[previous_size] == '/') {
            return EVO_PROJECT_ERROR_MANIFEST_INVALID;
        }
    }
    status = evo_project_parse_string_member(
        text,
        tokens,
        token_count,
        object_index,
        "compilation_database",
        limits->max_path_bytes,
        false,
        &manifest->compilation_database);
    if (status != EVO_PROJECT_SUCCESS ||
        !evo_project_relative_path_valid(
            manifest->compilation_database, limits->max_path_bytes)) {
        return status != EVO_PROJECT_SUCCESS ? status
                                             : EVO_PROJECT_ERROR_PATH_INVALID;
    }
    status = evo_project_parse_string_member(
        text,
        tokens,
        token_count,
        object_index,
        "generated_source_policy",
        limits->max_string_bytes,
        false,
        &manifest->generated_source_policy);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    if (strcmp(manifest->generated_source_policy, "reject") != 0) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    for (index = 0U; index < manifest->permitted_root_count; index += 1U) {
        const char *root = manifest->permitted_roots[index];
        const size_t root_size = strlen(root);

        if (strcmp(root, manifest->compilation_database) == 0 ||
            (strncmp(root, manifest->compilation_database, root_size) == 0 &&
             manifest->compilation_database[root_size] == '/')) {
            return EVO_PROJECT_SUCCESS;
        }
    }
    return EVO_PROJECT_ERROR_MANIFEST_INVALID;
}

static evo_project_status_t evo_project_parse_build(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const evo_project_ingest_limits_t *limits,
    evo_project_manifest_t *manifest)
{
    static const char *const allowed[] = {
        "frontend",
        "configure",
        "compile",
        "correctness",
        "benchmark",
        "benchmark_required"};
    static const char *const command_names[EVO_PROJECT_COMMAND_COUNT] = {
        "configure", "compile", "correctness", "benchmark"};
    static const char *const stage_ids[EVO_PROJECT_COMMAND_COUNT] = {
        "configure", "compile", "correctness", "benchmark"};
    static const evo_project_command_stage_t stages[EVO_PROJECT_COMMAND_COUNT] = {
        EVO_PROJECT_COMMAND_CONFIGURE,
        EVO_PROJECT_COMMAND_COMPILE,
        EVO_PROJECT_COMMAND_CORRECTNESS,
        EVO_PROJECT_COMMAND_BENCHMARK};
    evo_project_status_t status;
    size_t command;

    if (tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_json_object_has_only(
            text,
            tokens,
            token_count,
            object_index,
            allowed,
            sizeof(allowed) / sizeof(allowed[0]))) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    status = evo_project_parse_string_member(
        text,
        tokens,
        token_count,
        object_index,
        "frontend",
        limits->max_string_bytes,
        false,
        &manifest->build_frontend);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    if (strcmp(manifest->build_frontend, "cmake") != 0 &&
        strcmp(manifest->build_frontend, "autotools") != 0) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    for (command = 0U; command < EVO_PROJECT_COMMAND_COUNT; command += 1U) {
        size_t command_index;

        status = evo_project_require_member(
            text,
            tokens,
            token_count,
            object_index,
            command_names[command],
            &command_index);
        if (status == EVO_PROJECT_SUCCESS) {
            status = evo_project_parse_command(
                text,
                tokens,
                token_count,
                command_index,
                limits,
                stages[command],
                stage_ids[command],
                &manifest->commands[command]);
        }
        if (status != EVO_PROJECT_SUCCESS) {
            return status;
        }
    }
    return evo_project_parse_bool_member(
        text,
        tokens,
        token_count,
        object_index,
        "benchmark_required",
        &manifest->benchmark_required);
}

static evo_project_status_t evo_project_parse_target(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const evo_project_ingest_limits_t *limits,
    evo_project_manifest_t *manifest)
{
    static const char *const allowed[] = {"language", "platforms"};
    size_t platforms_index;
    evo_project_status_t status;

    if (tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_json_object_has_only(
            text,
            tokens,
            token_count,
            object_index,
            allowed,
            sizeof(allowed) / sizeof(allowed[0]))) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    status = evo_project_parse_string_member(
        text,
        tokens,
        token_count,
        object_index,
        "language",
        limits->max_string_bytes,
        false,
        &manifest->language);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    status = evo_project_require_member(
        text,
        tokens,
        token_count,
        object_index,
        "platforms",
        &platforms_index);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    return evo_project_parse_string_array(
        text,
        tokens,
        token_count,
        platforms_index,
        limits->max_targets,
        limits->max_string_bytes,
        true,
        false,
        &manifest->targets,
        &manifest->target_count);
}

static evo_project_status_t evo_project_parse_search(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    evo_project_manifest_search_t *search)
{
    static const char *const allowed[] = {
        "seed", "population", "generations", "workers"};
    evo_project_status_t status;

    if (tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_json_object_has_only(
            text,
            tokens,
            token_count,
            object_index,
            allowed,
            sizeof(allowed) / sizeof(allowed[0]))) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    status = evo_project_parse_u64_member(
        text, tokens, token_count, object_index, "seed", &search->seed);
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_parse_size_member(
            text,
            tokens,
            token_count,
            object_index,
            "population",
            &search->population);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_parse_size_member(
            text,
            tokens,
            token_count,
            object_index,
            "generations",
            &search->generations);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_parse_size_member(
            text,
            tokens,
            token_count,
            object_index,
            "workers",
            &search->workers);
    }
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    if (search->population == 0U || search->generations == 0U ||
        search->workers == 0U || search->workers > search->population) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_parse_budget(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const evo_project_ingest_limits_t *limits,
    evo_project_manifest_budget_t *budget)
{
    static const char *const allowed[] = {
        "max_files",
        "max_file_bytes",
        "max_total_bytes",
        "max_path_bytes",
        "max_compilation_database_bytes",
        "max_command_output_bytes",
        "max_evidence_bytes",
        "command_timeout_ms",
        "max_memory_bytes",
        "max_processes",
        "max_storage_bytes",
        "network_access"};
    evo_project_status_t status;

    if (tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_json_object_has_only(
            text,
            tokens,
            token_count,
            object_index,
            allowed,
            sizeof(allowed) / sizeof(allowed[0]))) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
#define EVO_PROJECT_PARSE_BUDGET_SIZE(member_name, field_name)                          \
    do {                                                                                \
        status = evo_project_parse_size_member(                                         \
            text, tokens, token_count, object_index, member_name, &budget->field_name); \
        if (status != EVO_PROJECT_SUCCESS) {                                            \
            return status;                                                              \
        }                                                                               \
    } while (0)

    EVO_PROJECT_PARSE_BUDGET_SIZE("max_files", max_files);
    EVO_PROJECT_PARSE_BUDGET_SIZE("max_file_bytes", max_file_bytes);
    EVO_PROJECT_PARSE_BUDGET_SIZE("max_total_bytes", max_total_bytes);
    EVO_PROJECT_PARSE_BUDGET_SIZE("max_path_bytes", max_path_bytes);
    EVO_PROJECT_PARSE_BUDGET_SIZE(
        "max_compilation_database_bytes", max_compilation_database_bytes);
    EVO_PROJECT_PARSE_BUDGET_SIZE(
        "max_command_output_bytes", max_command_output_bytes);
    EVO_PROJECT_PARSE_BUDGET_SIZE("max_evidence_bytes", max_evidence_bytes);
    status = evo_project_parse_u64_member(
        text,
        tokens,
        token_count,
        object_index,
        "command_timeout_ms",
        &budget->command_timeout_ms);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    status = evo_project_parse_u64_member(
        text,
        tokens,
        token_count,
        object_index,
        "max_memory_bytes",
        &budget->max_memory_bytes);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    EVO_PROJECT_PARSE_BUDGET_SIZE("max_processes", max_processes);
    status = evo_project_parse_u64_member(
        text,
        tokens,
        token_count,
        object_index,
        "max_storage_bytes",
        &budget->max_storage_bytes);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    status = evo_project_parse_bool_member(
        text,
        tokens,
        token_count,
        object_index,
        "network_access",
        &budget->network_access);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
#undef EVO_PROJECT_PARSE_BUDGET_SIZE

    if (budget->max_files == 0U || budget->max_file_bytes == 0U ||
        budget->max_total_bytes == 0U || budget->max_path_bytes == 0U ||
        budget->max_compilation_database_bytes == 0U ||
        budget->max_command_output_bytes == 0U ||
        budget->max_evidence_bytes == 0U ||
        budget->command_timeout_ms == 0U || budget->max_memory_bytes == 0U ||
        budget->max_processes == 0U || budget->max_storage_bytes == 0U ||
        budget->network_access ||
        budget->max_files > limits->max_files ||
        budget->max_file_bytes > limits->max_file_bytes ||
        budget->max_total_bytes > limits->max_total_bytes ||
        budget->max_path_bytes > limits->max_path_bytes ||
        budget->max_compilation_database_bytes >
            limits->max_compilation_database_bytes ||
        budget->max_command_output_bytes > limits->max_command_output_bytes ||
        budget->max_evidence_bytes > limits->max_evidence_bytes ||
        budget->command_timeout_ms > limits->max_command_timeout_ms ||
        budget->max_memory_bytes > limits->max_memory_bytes ||
        budget->max_processes > limits->max_processes ||
        budget->max_storage_bytes > limits->max_storage_bytes ||
        budget->max_file_bytes > budget->max_total_bytes ||
        budget->max_compilation_database_bytes > budget->max_file_bytes ||
        (uint64_t)budget->max_evidence_bytes > budget->max_storage_bytes ||
        budget->max_total_bytes > budget->max_storage_bytes ||
        budget->max_total_bytes >
            (budget->max_storage_bytes - budget->max_evidence_bytes) / 2U) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_parse_artifacts(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const evo_project_ingest_limits_t *limits,
    evo_project_manifest_t *manifest)
{
    static const char *const allowed[] = {"retention", "cleanup"};
    evo_project_status_t status;

    if (tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_json_object_has_only(
            text,
            tokens,
            token_count,
            object_index,
            allowed,
            sizeof(allowed) / sizeof(allowed[0]))) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    status = evo_project_parse_string_member(
        text,
        tokens,
        token_count,
        object_index,
        "retention",
        limits->max_string_bytes,
        false,
        &manifest->artifact_retention);
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_parse_string_member(
            text,
            tokens,
            token_count,
            object_index,
            "cleanup",
            limits->max_string_bytes,
            false,
            &manifest->cleanup_policy);
    }
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    if (strcmp(manifest->artifact_retention, "baseline-only") != 0 ||
        strcmp(manifest->cleanup_policy, "remove-derived-workspace") != 0) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    return EVO_PROJECT_SUCCESS;
}

static void evo_project_fingerprint_named_array(
    evo_project_fingerprint_t *fingerprint,
    const char *field,
    const evo_project_named_identity_t *values,
    size_t count)
{
    size_t index;

    evo_project_fingerprint_string(fingerprint, field);
    evo_project_fingerprint_u64(fingerprint, (uint64_t)count);
    for (index = 0U; index < count; index += 1U) {
        evo_project_fingerprint_string(fingerprint, values[index].name);
        evo_project_fingerprint_string(fingerprint, values[index].identity);
    }
}

static void evo_project_fingerprint_string_array(
    evo_project_fingerprint_t *fingerprint,
    const char *field,
    char *const *values,
    size_t count)
{
    size_t index;

    evo_project_fingerprint_string(fingerprint, field);
    evo_project_fingerprint_u64(fingerprint, (uint64_t)count);
    for (index = 0U; index < count; index += 1U) {
        evo_project_fingerprint_string(fingerprint, values[index]);
    }
}

static uint64_t evo_project_manifest_fingerprint(
    const evo_project_manifest_t *manifest)
{
    evo_project_fingerprint_t fingerprint;
    size_t index;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_field(&fingerprint, "schema", manifest->schema);
    evo_project_fingerprint_field(
        &fingerprint, "manifest_id", manifest->manifest_id);
    evo_project_fingerprint_field(
        &fingerprint, "source_identity", manifest->source_identity);
    evo_project_fingerprint_string_array(
        &fingerprint,
        "permitted_roots",
        manifest->permitted_roots,
        manifest->permitted_root_count);
    evo_project_fingerprint_field(
        &fingerprint,
        "compilation_database",
        manifest->compilation_database);
    evo_project_fingerprint_field(
        &fingerprint,
        "generated_source_policy",
        manifest->generated_source_policy);
    evo_project_fingerprint_field(
        &fingerprint, "build_frontend", manifest->build_frontend);
    for (index = 0U; index < EVO_PROJECT_COMMAND_COUNT; index += 1U) {
        evo_project_fingerprint_string(
            &fingerprint, manifest->commands[index].stage_id);
        evo_project_fingerprint_string_array(
            &fingerprint,
            "argv",
            manifest->commands[index].arguments,
            manifest->commands[index].argument_count);
    }
    evo_project_fingerprint_u64(
        &fingerprint, manifest->benchmark_required ? 1U : 0U);
    evo_project_fingerprint_field(&fingerprint, "language", manifest->language);
    evo_project_fingerprint_string_array(
        &fingerprint, "targets", manifest->targets, manifest->target_count);
    evo_project_fingerprint_named_array(
        &fingerprint,
        "dependencies",
        manifest->dependencies,
        manifest->dependency_count);
    evo_project_fingerprint_named_array(
        &fingerprint,
        "toolchains",
        manifest->toolchains,
        manifest->toolchain_count);
    evo_project_fingerprint_string(&fingerprint, "environment");
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->environment_count);
    for (index = 0U; index < manifest->environment_count; index += 1U) {
        evo_project_fingerprint_string(
            &fingerprint, manifest->environment[index].name);
        evo_project_fingerprint_string(
            &fingerprint, manifest->environment[index].value);
    }
    evo_project_fingerprint_string_array(
        &fingerprint,
        "workloads",
        manifest->workloads,
        manifest->workload_count);
    evo_project_fingerprint_string_array(
        &fingerprint,
        "constraints",
        manifest->constraints,
        manifest->constraint_count);
    evo_project_fingerprint_string(&fingerprint, "search");
    evo_project_fingerprint_u64(&fingerprint, manifest->search.seed);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->search.population);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->search.generations);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->search.workers);
    evo_project_fingerprint_string(&fingerprint, "budgets");
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->budget.max_files);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->budget.max_file_bytes);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->budget.max_total_bytes);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->budget.max_path_bytes);
    evo_project_fingerprint_u64(
        &fingerprint,
        (uint64_t)manifest->budget.max_compilation_database_bytes);
    evo_project_fingerprint_u64(
        &fingerprint,
        (uint64_t)manifest->budget.max_command_output_bytes);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->budget.max_evidence_bytes);
    evo_project_fingerprint_u64(
        &fingerprint, manifest->budget.command_timeout_ms);
    evo_project_fingerprint_u64(
        &fingerprint, manifest->budget.max_memory_bytes);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)manifest->budget.max_processes);
    evo_project_fingerprint_u64(
        &fingerprint, manifest->budget.max_storage_bytes);
    evo_project_fingerprint_u64(
        &fingerprint, manifest->budget.network_access ? 1U : 0U);
    evo_project_fingerprint_field(
        &fingerprint, "retention", manifest->artifact_retention);
    evo_project_fingerprint_field(
        &fingerprint, "cleanup", manifest->cleanup_policy);
    return fingerprint.value;
}

evo_project_status_t evo_project_manifest_parse(
    const char *text,
    size_t text_size,
    const evo_project_ingest_limits_t *limits,
    evo_project_manifest_t *manifest)
{
    static const char *const allowed[] = {
        "schema",
        "manifest_id",
        "source",
        "build",
        "target",
        "dependencies",
        "toolchains",
        "environment",
        "workloads",
        "constraints",
        "search",
        "budgets",
        "artifacts"};
    evo_project_json_token_t *tokens = NULL;
    evo_project_manifest_t parsed = {0};
    size_t token_count = 0U;
    evo_project_status_t status;
    evo_project_json_status_t json_status;
    size_t member_index;

    if (text == NULL || manifest == NULL || !evo_project_limits_valid(limits) ||
        text_size == 0U || text_size > limits->max_manifest_bytes ||
        manifest->schema != NULL) {
        return text_size > 0U && limits != NULL &&
                       text_size > limits->max_manifest_bytes
                   ? EVO_PROJECT_ERROR_RESOURCE_LIMIT
                   : EVO_PROJECT_ERROR_INVALID_ARGUMENT;
    }
    if (limits->max_json_tokens > SIZE_MAX / sizeof(*tokens)) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    tokens = evo_project_allocate_zeroed(limits->max_json_tokens, sizeof(*tokens));
    if (tokens == NULL) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    json_status = evo_project_json_parse(
        text,
        text_size,
        tokens,
        limits->max_json_tokens,
        limits->max_json_depth,
        &token_count);
    status = evo_project_manifest_json_status(json_status);
    if (status != EVO_PROJECT_SUCCESS) {
        evo_project_release(tokens);
        return status;
    }
    if (token_count == 0U || tokens[0].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_json_object_has_only(
            text,
            tokens,
            token_count,
            0U,
            allowed,
            sizeof(allowed) / sizeof(allowed[0]))) {
        evo_project_release(tokens);
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }

    status = evo_project_parse_string_member(
        text,
        tokens,
        token_count,
        0U,
        "schema",
        limits->max_string_bytes,
        false,
        &parsed.schema);
    if (status == EVO_PROJECT_SUCCESS &&
        strcmp(parsed.schema, "catalyst.evo-project-manifest.v1") != 0) {
        status = EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_parse_string_member(
            text,
            tokens,
            token_count,
            0U,
            "manifest_id",
            limits->max_string_bytes,
            false,
            &parsed.manifest_id);
    }
#define EVO_PROJECT_PARSE_OBJECT_MEMBER(name_value, parse_expression)      \
    do {                                                                   \
        if (status == EVO_PROJECT_SUCCESS) {                               \
            status = evo_project_require_member(                           \
                text, tokens, token_count, 0U, name_value, &member_index); \
        }                                                                  \
        if (status == EVO_PROJECT_SUCCESS) {                               \
            status = (parse_expression);                                   \
        }                                                                  \
    } while (0)

    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "source",
        evo_project_parse_source(
            text, tokens, token_count, member_index, limits, &parsed));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "build",
        evo_project_parse_build(
            text, tokens, token_count, member_index, limits, &parsed));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "target",
        evo_project_parse_target(
            text, tokens, token_count, member_index, limits, &parsed));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "dependencies",
        evo_project_parse_named_identities(
            text,
            tokens,
            token_count,
            member_index,
            limits->max_dependencies,
            limits->max_string_bytes,
            false,
            &parsed.dependencies,
            &parsed.dependency_count));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "toolchains",
        evo_project_parse_named_identities(
            text,
            tokens,
            token_count,
            member_index,
            limits->max_toolchains,
            limits->max_string_bytes,
            true,
            &parsed.toolchains,
            &parsed.toolchain_count));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "environment",
        evo_project_parse_environment(
            text,
            tokens,
            token_count,
            member_index,
            limits,
            &parsed.environment,
            &parsed.environment_count));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "workloads",
        evo_project_parse_string_array(
            text,
            tokens,
            token_count,
            member_index,
            limits->max_workloads,
            limits->max_string_bytes,
            true,
            false,
            &parsed.workloads,
            &parsed.workload_count));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "constraints",
        evo_project_parse_string_array(
            text,
            tokens,
            token_count,
            member_index,
            limits->max_constraints,
            limits->max_string_bytes,
            true,
            false,
            &parsed.constraints,
            &parsed.constraint_count));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "search",
        evo_project_parse_search(
            text, tokens, token_count, member_index, &parsed.search));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "budgets",
        evo_project_parse_budget(
            text,
            tokens,
            token_count,
            member_index,
            limits,
            &parsed.budget));
    EVO_PROJECT_PARSE_OBJECT_MEMBER(
        "artifacts",
        evo_project_parse_artifacts(
            text, tokens, token_count, member_index, limits, &parsed));
#undef EVO_PROJECT_PARSE_OBJECT_MEMBER

    evo_project_release(tokens);
    if (status != EVO_PROJECT_SUCCESS) {
        evo_project_manifest_destroy(&parsed);
        return status;
    }
    parsed.fingerprint = evo_project_manifest_fingerprint(&parsed);
    *manifest = parsed;
    return EVO_PROJECT_SUCCESS;
}

void evo_project_manifest_destroy(evo_project_manifest_t *manifest)
{
    size_t index;

    if (manifest == NULL) {
        return;
    }
    evo_project_release(manifest->schema);
    evo_project_release(manifest->manifest_id);
    evo_project_release(manifest->source_identity);
    evo_project_free_string_array(
        manifest->permitted_roots, manifest->permitted_root_count);
    evo_project_release(manifest->compilation_database);
    evo_project_release(manifest->generated_source_policy);
    evo_project_release(manifest->build_frontend);
    for (index = 0U; index < EVO_PROJECT_COMMAND_COUNT; index += 1U) {
        evo_project_free_string_array(
            manifest->commands[index].arguments,
            manifest->commands[index].argument_count);
    }
    evo_project_release(manifest->language);
    evo_project_free_string_array(manifest->targets, manifest->target_count);
    for (index = 0U; index < manifest->dependency_count; index += 1U) {
        evo_project_release(manifest->dependencies[index].name);
        evo_project_release(manifest->dependencies[index].identity);
    }
    evo_project_release(manifest->dependencies);
    for (index = 0U; index < manifest->toolchain_count; index += 1U) {
        evo_project_release(manifest->toolchains[index].name);
        evo_project_release(manifest->toolchains[index].identity);
    }
    evo_project_release(manifest->toolchains);
    for (index = 0U; index < manifest->environment_count; index += 1U) {
        evo_project_release(manifest->environment[index].name);
        evo_project_release(manifest->environment[index].value);
    }
    evo_project_release(manifest->environment);
    evo_project_free_string_array(
        manifest->workloads, manifest->workload_count);
    evo_project_free_string_array(
        manifest->constraints, manifest->constraint_count);
    evo_project_release(manifest->artifact_retention);
    evo_project_release(manifest->cleanup_policy);
    *manifest = (evo_project_manifest_t){0};
}
