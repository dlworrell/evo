#define _POSIX_C_SOURCE 200809L

#include "internal/project_provider_clang.h"

#include "internal/project_json.h"
#include "internal/project_provider.h"
#include "internal/project_provider_sandbox.h"
#include "internal/project_runtime.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *evo_clang_duplicate(const char *value)
{
    size_t size;
    char *copy;

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
    (void)memcpy(copy, value, size);
    copy[size] = '\0';
    return copy;
}

static char *evo_clang_format_identity(
    size_t maximum_bytes,
    const char *domain,
    const char *translation_unit,
    uint32_t line,
    uint32_t column,
    const char *name)
{
    char *value;
    int written;

    if (maximum_bytes == 0U || maximum_bytes == SIZE_MAX) {
        return NULL;
    }
    value = evo_project_allocate_zeroed(maximum_bytes + 1U, sizeof(*value));
    if (value == NULL) {
        return NULL;
    }
    written = evo_project_format(
        value,
        maximum_bytes + 1U,
        "%s:%s:%u:%u:%s",
        domain,
        translation_unit,
        (unsigned int)line,
        (unsigned int)column,
        name);
    if (written <= 0 || (size_t)written > maximum_bytes) {
        evo_project_release(value);
        return NULL;
    }
    return value;
}

static void evo_clang_release_location(
    evo_project_source_location_record_t *record)
{
    if (record == NULL) {
        return;
    }
    evo_project_release((void *)record->identity);
    evo_project_release((void *)record->file);
    evo_project_release((void *)record->spelling_identity);
    *record = (evo_project_source_location_record_t){0};
}

static void evo_clang_release_declaration(
    evo_project_declaration_record_t *record)
{
    if (record == NULL) {
        return;
    }
    evo_project_release((void *)record->identity);
    evo_project_release((void *)record->name);
    evo_project_release((void *)record->translation_unit);
    *record = (evo_project_declaration_record_t){0};
}

void evo_project_clang_analysis_context_destroy(
    evo_project_clang_analysis_context_t *context)
{
    size_t index;

    if (context == NULL) {
        return;
    }
    for (index = 0U; index < context->source_location_count; index += 1U) {
        evo_clang_release_location(&context->source_locations[index]);
    }
    for (index = 0U; index < context->declaration_count; index += 1U) {
        evo_clang_release_declaration(&context->declarations[index]);
    }
    evo_project_release(context->source_locations);
    evo_project_release(context->declarations);
    *context = (evo_project_clang_analysis_context_t){0};
}

static bool evo_clang_context_prepare(
    const evo_project_analysis_request_t *request,
    evo_project_clang_analysis_context_t *context)
{
    if (request->limits.max_source_locations == 0U ||
        request->limits.max_declarations == 0U) {
        return false;
    }
    context->source_locations = evo_project_allocate_zeroed(
        request->limits.max_source_locations,
        sizeof(*context->source_locations));
    context->declarations = evo_project_allocate_zeroed(
        request->limits.max_declarations,
        sizeof(*context->declarations));
    if (context->source_locations == NULL || context->declarations == NULL) {
        evo_project_clang_analysis_context_destroy(context);
        return false;
    }
    context->source_location_capacity = request->limits.max_source_locations;
    context->declaration_capacity = request->limits.max_declarations;
    return true;
}

static bool evo_clang_relative_path_safe(const char *path)
{
    size_t component_start = 0U;
    size_t index;

    if (path == NULL || path[0] == '\0' || path[0] == '/') {
        return false;
    }
    for (index = 0U;; index += 1U) {
        const unsigned char byte = (unsigned char)path[index];

        if (byte == 0U || byte == (unsigned char)'/') {
            const size_t component_size = index - component_start;

            if (component_size == 0U ||
                (component_size == 1U && path[component_start] == '.') ||
                (component_size == 2U && path[component_start] == '.' &&
                 path[component_start + 1U] == '.')) {
                return false;
            }
            if (byte == 0U) {
                return true;
            }
            component_start = index + 1U;
            continue;
        }
        if (byte == (unsigned char)'\\' || byte == (unsigned char)':' ||
            byte < 0x20U || byte == 0x7fU) {
            return false;
        }
    }
}

static bool evo_clang_safe_define(const char *argument)
{
    size_t index;

    if (argument == NULL || argument[0] != '-' ||
        (argument[1] != 'D' && argument[1] != 'U') || argument[2] == '\0') {
        return false;
    }
    for (index = 2U; argument[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)argument[index];

        if (byte < 0x20U || byte == 0x7fU || byte == (unsigned char)'@') {
            return false;
        }
    }
    return true;
}

static bool evo_clang_safe_standard(const char *argument)
{
    static const char prefix[] = "-std=";
    size_t index;

    if (argument == NULL || strncmp(argument, prefix, sizeof(prefix) - 1U) != 0 ||
        argument[sizeof(prefix) - 1U] == '\0') {
        return false;
    }
    for (index = sizeof(prefix) - 1U; argument[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)argument[index];

        if (!((byte >= (unsigned char)'A' && byte <= (unsigned char)'Z') ||
              (byte >= (unsigned char)'a' && byte <= (unsigned char)'z') ||
              (byte >= (unsigned char)'0' && byte <= (unsigned char)'9') ||
              byte == (unsigned char)'+' || byte == (unsigned char)'-')) {
            return false;
        }
    }
    return true;
}

static bool evo_clang_ignored_flag(const char *argument)
{
    return strcmp(argument, "-c") == 0 || strcmp(argument, "-g") == 0 ||
           strcmp(argument, "-pipe") == 0 || strcmp(argument, "-pthread") == 0 ||
           strncmp(argument, "-O", 2U) == 0 ||
           strncmp(argument, "-W", 2U) == 0;
}

static bool evo_clang_forbidden_flag(const char *argument)
{
    static const char *const prefixes[] = {
        "@",
        "-Xclang",
        "-load",
        "-fplugin",
        "-plugin",
        "-cc1",
        "--config",
        "-include",
        "-imacros",
        "-B",
        "--sysroot",
        "-isysroot",
        "-isystem",
        "-iquote",
        "-MJ",
        "-MF",
        "-MT",
        "-MQ"};
    size_t index;

    for (index = 0U; index < sizeof(prefixes) / sizeof(prefixes[0]); index += 1U) {
        const size_t length = strlen(prefixes[index]);

        if (strncmp(argument, prefixes[index], length) == 0) {
            return true;
        }
    }
    return false;
}

static bool evo_clang_same_source_operand(
    const char *argument,
    const evo_project_analysis_request_t *request)
{
    size_t index;

    for (index = 0U; index < request->compilation_unit_count; index += 1U) {
        if (strcmp(argument, request->compilation_units[index].file) == 0) {
            return true;
        }
    }
    return false;
}

static char **evo_clang_build_arguments(
    const evo_project_analysis_request_t *request,
    const evo_project_compilation_record_t *unit,
    size_t *argument_count)
{
    size_t capacity;
    size_t position = 0U;
    size_t index;
    char **arguments;

    if (unit->command_form != EVO_PROJECT_COMPILE_COMMAND_ARGUMENTS ||
        unit->argument_count == 0U || unit->arguments == NULL ||
        unit->argument_count > SIZE_MAX - 8U) {
        return NULL;
    }
    capacity = unit->argument_count + 8U;
    arguments = evo_project_allocate_zeroed(capacity, sizeof(*arguments));
    if (arguments == NULL) {
        return NULL;
    }
    arguments[position++] = (char *)"clang";
    arguments[position++] = (char *)"-fsyntax-only";
    arguments[position++] = (char *)"-Xclang";
    arguments[position++] = (char *)"-ast-dump=json";
    for (index = 1U; index < unit->argument_count; index += 1U) {
        const char *argument = unit->arguments[index];

        if (argument == NULL || argument[0] == '\0' ||
            evo_clang_forbidden_flag(argument)) {
            evo_project_release(arguments);
            return NULL;
        }
        if (strcmp(argument, "-o") == 0) {
            if (index + 1U >= unit->argument_count) {
                evo_project_release(arguments);
                return NULL;
            }
            index += 1U;
            continue;
        }
        if (strcmp(argument, "-I") == 0) {
            if (index + 1U >= unit->argument_count ||
                !evo_clang_relative_path_safe(unit->arguments[index + 1U])) {
                evo_project_release(arguments);
                return NULL;
            }
            arguments[position++] = (char *)argument;
            arguments[position++] = (char *)unit->arguments[index + 1U];
            index += 1U;
            continue;
        }
        if (strncmp(argument, "-I", 2U) == 0) {
            if (!evo_clang_relative_path_safe(argument + 2U)) {
                evo_project_release(arguments);
                return NULL;
            }
            arguments[position++] = (char *)argument;
            continue;
        }
        if (evo_clang_safe_standard(argument) || evo_clang_safe_define(argument)) {
            arguments[position++] = (char *)argument;
            continue;
        }
        if (evo_clang_ignored_flag(argument) ||
            evo_clang_same_source_operand(argument, request)) {
            continue;
        }
        if (argument[0] != '-') {
            continue;
        }
        evo_project_release(arguments);
        return NULL;
    }
    arguments[position++] = (char *)unit->file;
    arguments[position] = NULL;
    *argument_count = position;
    return arguments;
}

static int evo_clang_object_member(
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *name,
    size_t *value_index)
{
    if (object_index >= token_count ||
        tokens[object_index].type != EVO_PROJECT_JSON_OBJECT) {
        return -1;
    }
    return evo_project_json_object_get(
        json, tokens, token_count, object_index, name, value_index);
}

static char *evo_clang_string_member(
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *name,
    size_t maximum_bytes)
{
    size_t value_index;
    char *value = NULL;

    if (evo_clang_object_member(
            json,
            tokens,
            token_count,
            object_index,
            name,
            &value_index) != 1 ||
        tokens[value_index].type != EVO_PROJECT_JSON_STRING ||
        evo_project_json_decode_string(
            json, &tokens[value_index], maximum_bytes, &value) !=
            EVO_PROJECT_JSON_SUCCESS) {
        return NULL;
    }
    return value;
}

static bool evo_clang_u32_member(
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *name,
    uint32_t *value)
{
    size_t value_index;
    uint64_t parsed;

    if (evo_clang_object_member(
            json,
            tokens,
            token_count,
            object_index,
            name,
            &value_index) != 1 ||
        !evo_project_json_parse_u64(json, &tokens[value_index], &parsed) ||
        parsed == 0U || parsed > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool evo_clang_file_matches_unit(
    const char *snapshot_path,
    const char *observed,
    const char *unit_file)
{
    size_t snapshot_size;

    if (observed == NULL || strcmp(observed, unit_file) == 0) {
        return true;
    }
    if (observed[0] == '.' && observed[1] == '/' &&
        strcmp(observed + 2, unit_file) == 0) {
        return true;
    }
    snapshot_size = strlen(snapshot_path);
    return strncmp(observed, snapshot_path, snapshot_size) == 0 &&
           observed[snapshot_size] == '/' &&
           strcmp(observed + snapshot_size + 1U, unit_file) == 0;
}

static bool evo_clang_object_has_descendant_kind(
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *kind,
    size_t maximum_bytes)
{
    size_t index;

    for (index = object_index + 1U;
         index < token_count && tokens[index].start < tokens[object_index].end;
         index += 1U) {
        char *observed;

        if (tokens[index].type != EVO_PROJECT_JSON_OBJECT) {
            continue;
        }
        observed = evo_clang_string_member(
            json, tokens, token_count, index, "kind", maximum_bytes);
        if (observed != NULL) {
            const bool matched = strcmp(observed, kind) == 0;

            evo_project_release(observed);
            if (matched) {
                return true;
            }
        }
    }
    return false;
}

static bool evo_clang_identity_exists(
    const evo_project_clang_analysis_context_t *context,
    const char *identity)
{
    size_t index;

    for (index = 0U; index < context->declaration_count; index += 1U) {
        if (strcmp(context->declarations[index].identity, identity) == 0) {
            return true;
        }
    }
    return false;
}

static evo_project_analysis_status_t evo_clang_append_function(
    const evo_project_analysis_request_t *request,
    const evo_project_compilation_record_t *unit,
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    evo_project_clang_analysis_context_t *context)
{
    char *name = NULL;
    char *storage = NULL;
    char *observed_file = NULL;
    char *location_identity = NULL;
    char *declaration_identity = NULL;
    size_t loc_index;
    uint32_t line;
    uint32_t column;
    bool definition;
    evo_project_linkage_t linkage;
    evo_project_source_location_record_t location = {0};
    evo_project_declaration_record_t declaration = {0};

    if (evo_clang_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "loc",
            &loc_index) != 1 ||
        tokens[loc_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_clang_u32_member(
            json, tokens, token_count, loc_index, "line", &line) ||
        !evo_clang_u32_member(
            json, tokens, token_count, loc_index, "col", &column)) {
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    name = evo_clang_string_member(
        json,
        tokens,
        token_count,
        object_index,
        "name",
        request->limits.max_string_bytes);
    if (name == NULL) {
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    observed_file = evo_clang_string_member(
        json,
        tokens,
        token_count,
        loc_index,
        "file",
        request->limits.max_path_bytes);
    if (!evo_clang_file_matches_unit(
            request->snapshot_path, observed_file, unit->file)) {
        evo_project_release(name);
        evo_project_release(observed_file);
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    location_identity = evo_clang_format_identity(
        request->limits.max_string_bytes,
        "clang-location-v1",
        unit->file,
        line,
        column,
        name);
    declaration_identity = evo_clang_format_identity(
        request->limits.max_string_bytes,
        "clang-function-v1",
        unit->file,
        line,
        column,
        name);
    if (location_identity == NULL || declaration_identity == NULL) {
        evo_project_release(name);
        evo_project_release(observed_file);
        evo_project_release(location_identity);
        evo_project_release(declaration_identity);
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    if (evo_clang_identity_exists(context, declaration_identity)) {
        evo_project_release(name);
        evo_project_release(observed_file);
        evo_project_release(location_identity);
        evo_project_release(declaration_identity);
        return EVO_PROJECT_ANALYSIS_SUCCESS;
    }
    if (context->source_location_count >= context->source_location_capacity ||
        context->declaration_count >= context->declaration_capacity) {
        evo_project_release(name);
        evo_project_release(observed_file);
        evo_project_release(location_identity);
        evo_project_release(declaration_identity);
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    storage = evo_clang_string_member(
        json,
        tokens,
        token_count,
        object_index,
        "storageClass",
        request->limits.max_string_bytes);
    linkage = storage != NULL && strcmp(storage, "static") == 0
                  ? EVO_PROJECT_LINKAGE_INTERNAL
                  : EVO_PROJECT_LINKAGE_EXTERNAL;
    definition = evo_clang_object_has_descendant_kind(
        json,
        tokens,
        token_count,
        object_index,
        "CompoundStmt",
        request->limits.max_string_bytes);
    location.identity = location_identity;
    location.file = evo_clang_duplicate(unit->file);
    location.line = line;
    location.column = column;
    location.end_line = line;
    location.end_column = column;
    location.kind = EVO_PROJECT_LOCATION_SPELLING;
    if (location.file == NULL) {
        evo_project_release(name);
        evo_project_release(storage);
        evo_project_release(observed_file);
        evo_project_release(location_identity);
        evo_project_release(declaration_identity);
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    declaration.identity = declaration_identity;
    declaration.name = name;
    declaration.translation_unit = evo_clang_duplicate(unit->file);
    declaration.location_identity = location_identity;
    declaration.kind = EVO_PROJECT_DECLARATION_FUNCTION;
    declaration.linkage = linkage;
    declaration.definition = definition;
    if (declaration.translation_unit == NULL) {
        evo_clang_release_location(&location);
        evo_project_release((void *)declaration.identity);
        evo_project_release((void *)declaration.name);
        evo_project_release(storage);
        evo_project_release(observed_file);
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    context->source_locations[context->source_location_count] = location;
    context->source_location_count += 1U;
    context->declarations[context->declaration_count] = declaration;
    context->declaration_count += 1U;
    evo_project_release(storage);
    evo_project_release(observed_file);
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static evo_project_analysis_status_t evo_clang_parse_ast(
    const evo_project_analysis_request_t *request,
    const evo_project_compilation_record_t *unit,
    const char *json,
    size_t json_size,
    evo_project_clang_analysis_context_t *context)
{
    evo_project_json_token_t *tokens;
    size_t token_capacity;
    size_t token_count = 0U;
    size_t index;
    evo_project_json_status_t json_status;

    if (json == NULL || json_size == 0U || json_size == SIZE_MAX) {
        return EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    }
    token_capacity = json_size + 1U;
    if (token_capacity > SIZE_MAX / sizeof(*tokens)) {
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    tokens = evo_project_allocate_zeroed(token_capacity, sizeof(*tokens));
    if (tokens == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    json_status = evo_project_json_parse(
        json,
        json_size,
        tokens,
        token_capacity,
        256U,
        &token_count);
    if (json_status != EVO_PROJECT_JSON_SUCCESS || token_count == 0U) {
        evo_project_release(tokens);
        return json_status == EVO_PROJECT_JSON_OUT_OF_MEMORY
                   ? EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY
                   : (json_status == EVO_PROJECT_JSON_RESOURCE_LIMIT
                          ? EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT
                          : EVO_PROJECT_ANALYSIS_ERROR_PROVIDER);
    }
    for (index = 0U; index < token_count; index += 1U) {
        char *kind;
        evo_project_analysis_status_t status;

        if (tokens[index].type != EVO_PROJECT_JSON_OBJECT) {
            continue;
        }
        kind = evo_clang_string_member(
            json,
            tokens,
            token_count,
            index,
            "kind",
            request->limits.max_string_bytes);
        if (kind == NULL) {
            continue;
        }
        if (strcmp(kind, "FunctionDecl") != 0) {
            evo_project_release(kind);
            continue;
        }
        evo_project_release(kind);
        status = evo_clang_append_function(
            request, unit, json, tokens, token_count, index, context);
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            evo_project_release(tokens);
            return status;
        }
    }
    evo_project_release(tokens);
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}

static evo_project_analysis_status_t evo_clang_analyze_unit(
    const evo_project_analysis_request_t *request,
    const evo_project_compilation_record_t *unit,
    evo_project_clang_analysis_context_t *context)
{
    static const char *const environment[] = {"LANG=C", "LC_ALL=C"};
    char **arguments;
    size_t argument_count = 0U;
    evo_project_sandbox_command_t command;
    evo_project_sandbox_result_t sandbox = {0};
    evo_project_sandbox_status_t sandbox_status;
    evo_project_analysis_status_t status;

    arguments = evo_clang_build_arguments(request, unit, &argument_count);
    if (arguments == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_UNSUPPORTED_EVIDENCE;
    }
    command = (evo_project_sandbox_command_t){
        .schema_version = EVO_PROJECT_SANDBOX_SCHEMA_VERSION,
        .workspace_path = request->snapshot_path,
        .working_directory = unit->directory,
        .argument_count = argument_count,
        .arguments = (const char *const *)arguments,
        .environment_count = sizeof(environment) / sizeof(environment[0]),
        .environment = environment,
        .limits = {
            .cpu_time_ms = request->timeout_ms,
            .address_space_bytes = request->max_memory_bytes,
            .descendant_process_count = request->max_processes,
            .storage_bytes = request->max_storage_bytes,
            .output_bytes = request->max_output_bytes,
            .wall_timeout_ms = request->timeout_ms,
            .network_access = false,
        },
    };
    sandbox_status = evo_project_sandbox_run(&command, &sandbox);
    evo_project_release(arguments);
    if (sandbox_status == EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY) {
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    if (sandbox_status == EVO_PROJECT_SANDBOX_ERROR_RESOURCE_LIMIT) {
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    if (sandbox_status != EVO_PROJECT_SANDBOX_SUCCESS || !sandbox.completed ||
        sandbox.exit_code != 0 || sandbox.resource_exhausted || sandbox.signaled ||
        sandbox.stdout_text == NULL || sandbox.stdout_bytes == 0U) {
        evo_project_sandbox_result_destroy(&sandbox);
        return EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    }
    status = evo_clang_parse_ast(
        request, unit, sandbox.stdout_text, sandbox.stdout_bytes, context);
    evo_project_sandbox_result_destroy(&sandbox);
    return status;
}

evo_project_analysis_status_t evo_project_clang_analysis_provider(
    const evo_project_analysis_request_t *request,
    void *opaque,
    evo_project_analysis_provider_result_t *result)
{
    evo_project_clang_analysis_context_t *context = opaque;
    const evo_project_provider_record_t *provider;
    size_t index;
    evo_project_analysis_status_t status;

    if (request == NULL || context == NULL || result == NULL ||
        request->schema_version != EVO_PROJECT_ANALYSIS_SCHEMA_VERSION ||
        request->snapshot_path == NULL || request->compilation_unit_count == 0U ||
        request->compilation_units == NULL || request->provider_identity == NULL ||
        strcmp(request->provider_identity, EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID) != 0 ||
        request->network_access || request->timeout_ms == 0U ||
        request->max_memory_bytes == 0U || request->max_processes == 0U ||
        request->max_storage_bytes == 0U || request->max_output_bytes == 0U) {
        return EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT;
    }
    provider = evo_project_provider_find(EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID);
    if (provider == NULL || !evo_project_provider_available(provider) ||
        !evo_project_sandbox_available()) {
        return EVO_PROJECT_ANALYSIS_ERROR_PROVIDER;
    }
    evo_project_clang_analysis_context_destroy(context);
    if (!evo_clang_context_prepare(request, context)) {
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < request->compilation_unit_count; index += 1U) {
        status = evo_clang_analyze_unit(
            request, &request->compilation_units[index], context);
        if (status != EVO_PROJECT_ANALYSIS_SUCCESS) {
            evo_project_clang_analysis_context_destroy(context);
            return status;
        }
    }
    if (context->source_location_count == 0U || context->declaration_count == 0U) {
        evo_project_clang_analysis_context_destroy(context);
        return EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE;
    }
    *result = (evo_project_analysis_provider_result_t){0};
    result->schema_version = EVO_PROJECT_ANALYSIS_SCHEMA_VERSION;
    result->completed = true;
    result->source_location_count = context->source_location_count;
    result->source_locations = context->source_locations;
    result->declaration_count = context->declaration_count;
    result->declarations = context->declarations;
    return EVO_PROJECT_ANALYSIS_SUCCESS;
}
