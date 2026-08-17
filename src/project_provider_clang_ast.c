#define _XOPEN_SOURCE 700

#include "internal/project_provider_clang_ast.h"

#include "internal/project_fingerprint.h"
#include "internal/project_provider.h"
#include "internal/project_provider_clang_ast_authority.h"
#include "internal/project_provider_sandbox.h"
#include "internal/project_runtime.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *evo_clang_ast_duplicate(const char *text)
{
    size_t size;
    size_t index;
    char *copy;

    if (text == NULL) {
        return NULL;
    }
    size = strlen(text);
    if (size == SIZE_MAX) {
        return NULL;
    }
    copy = evo_project_allocate_zeroed(size + 1U, sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }
    for (index = 0U; index < size; index += 1U) {
        copy[index] = text[index];
    }
    copy[size] = '\0';
    return copy;
}

void evo_project_clang_ast_context_destroy(evo_project_clang_ast_context_t *context)
{
    if (context == NULL) {
        return;
    }
    evo_project_release(context->location_identity);
    evo_project_release(context->file);
    evo_project_release(context->primary_declaration_identity);
    evo_project_release(context->duplicate_declaration_identity);
    context->location_identity = NULL;
    context->file = NULL;
    context->primary_declaration_identity = NULL;
    context->duplicate_declaration_identity = NULL;
}

static bool evo_clang_ast_relative_path_safe(const char *path)
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

static bool evo_clang_ast_path_within(const char *root, const char *path)
{
    const size_t root_size = strlen(root);

    return strncmp(root, path, root_size) == 0 &&
           (path[root_size] == '\0' || path[root_size] == '/');
}

static bool evo_clang_ast_read_source(
    const evo_project_transformation_request_t *request,
    unsigned char **bytes_out,
    size_t *size_out)
{
    char root[4096];
    char requested[4096];
    char resolved[4096];
    struct stat metadata;
    unsigned char *bytes = NULL;
    size_t size;
    size_t position = 0U;
    int descriptor = -1;
    int written;

    if (request == NULL || request->target == NULL ||
        !evo_clang_ast_relative_path_safe(request->target->file) ||
        request->limits.max_source_bytes == 0U ||
        realpath(request->snapshot_path, root) == NULL) {
        return false;
    }
    written = evo_project_format(
        requested, sizeof(requested), "%s/%s", root, request->target->file);
    if (written <= 0 || (size_t)written >= sizeof(requested) ||
        realpath(requested, resolved) == NULL ||
        !evo_clang_ast_path_within(root, resolved)) {
        return false;
    }
    descriptor = open(resolved, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (descriptor < 0 || fstat(descriptor, &metadata) != 0 ||
        !S_ISREG(metadata.st_mode) || metadata.st_size <= 0 ||
        (uintmax_t)metadata.st_size > (uintmax_t)SIZE_MAX) {
        if (descriptor >= 0) {
            (void)close(descriptor);
        }
        return false;
    }
    size = (size_t)metadata.st_size;
    if (size > request->limits.max_source_bytes || size != request->source_size ||
        size == SIZE_MAX) {
        (void)close(descriptor);
        return false;
    }
    bytes = evo_project_allocate_zeroed(size + 1U, sizeof(*bytes));
    if (bytes == NULL) {
        (void)close(descriptor);
        return false;
    }
    while (position < size) {
        const ssize_t count = read(descriptor, bytes + position, size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            evo_project_release(bytes);
            (void)close(descriptor);
            return false;
        }
        position += (size_t)count;
    }
    if (close(descriptor) != 0) {
        evo_project_release(bytes);
        return false;
    }
    bytes[size] = 0U;
    *bytes_out = bytes;
    *size_out = size;
    return true;
}

static bool evo_clang_ast_offset(
    const unsigned char *source,
    size_t source_size,
    uint32_t line,
    uint32_t column,
    size_t *offset_out)
{
    uint32_t current_line = 1U;
    uint32_t current_column = 1U;
    size_t index;

    if (line == 0U || column == 0U || offset_out == NULL) {
        return false;
    }
    for (index = 0U; index <= source_size; index += 1U) {
        if (current_line == line && current_column == column) {
            *offset_out = index;
            return true;
        }
        if (index == source_size) {
            break;
        }
        if (source[index] == (unsigned char)'\n') {
            current_line += 1U;
            current_column = 1U;
        } else {
            current_column += 1U;
        }
    }
    return false;
}

static size_t evo_clang_ast_skip_space(
    const unsigned char *source,
    size_t position,
    size_t end)
{
    while (position < end && isspace((int)source[position]) != 0) {
        position += 1U;
    }
    return position;
}

static size_t evo_clang_ast_trim_end(
    const unsigned char *source,
    size_t start,
    size_t end)
{
    while (end > start && isspace((int)source[end - 1U]) != 0) {
        end -= 1U;
    }
    return end;
}

static bool evo_clang_ast_identifier_range(
    const unsigned char *source,
    size_t start,
    size_t end,
    evo_project_transformation_byte_range_t *range)
{
    size_t position = evo_clang_ast_skip_space(source, start, end);
    size_t finish;

    if (position >= end ||
        !(isalpha((int)source[position]) != 0 || source[position] == '_')) {
        return false;
    }
    finish = position + 1U;
    while (finish < end &&
           (isalnum((int)source[finish]) != 0 || source[finish] == '_')) {
        finish += 1U;
    }
    range->start = position;
    range->end = finish;
    return true;
}

static bool evo_clang_ast_bytes_equal(
    const unsigned char *source,
    evo_project_transformation_byte_range_t left,
    evo_project_transformation_byte_range_t right)
{
    size_t index;
    const size_t left_size = left.end - left.start;

    if (left_size != right.end - right.start) {
        return false;
    }
    for (index = 0U; index < left_size; index += 1U) {
        if (source[left.start + index] != source[right.start + index]) {
            return false;
        }
    }
    return true;
}

static evo_project_transformation_operator_t evo_clang_ast_operator(char value)
{
    switch (value) {
    case '+':
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_ADD;
    case '&':
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_AND;
    case '|':
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_OR;
    case '^':
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_XOR;
    case '*':
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_MULTIPLY;
    case '-':
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_SUBTRACT;
    default:
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_NONE;
    }
}

static bool evo_clang_ast_parse_u64(
    const unsigned char *source,
    evo_project_transformation_byte_range_t range,
    uint64_t *value_out)
{
    uint64_t value = 0U;
    size_t position = range.start;
    size_t digits = 0U;

    while (position < range.end && source[position] >= '0' && source[position] <= '9') {
        const uint64_t digit = (uint64_t)(source[position] - '0');

        if (value > (UINT64_MAX - digit) / UINT64_C(10)) {
            return false;
        }
        value = value * UINT64_C(10) + digit;
        digits += 1U;
        position += 1U;
    }
    if (digits == 0U) {
        return false;
    }
    while (position < range.end) {
        const unsigned char byte = source[position];

        if (byte != 'u' && byte != 'U' && byte != 'l' && byte != 'L') {
            return false;
        }
        position += 1U;
    }
    *value_out = value;
    return true;
}

static bool evo_clang_ast_contains_unsafe_text(
    const unsigned char *source,
    evo_project_transformation_byte_range_t range,
    bool *comment,
    bool *preprocessor)
{
    size_t index;

    *comment = false;
    *preprocessor = false;
    for (index = range.start; index < range.end; index += 1U) {
        if (source[index] == '#' ||
            (index + 1U < range.end && source[index] == '\\' &&
             source[index + 1U] == '\n')) {
            *preprocessor = true;
        }
        if (index + 1U < range.end && source[index] == '/' &&
            (source[index + 1U] == '/' || source[index + 1U] == '*')) {
            *comment = true;
        }
    }
    return *comment || *preprocessor;
}

static char *evo_clang_ast_declaration_identity(
    const evo_project_transformation_request_t *request,
    const unsigned char *source,
    evo_project_transformation_byte_range_t identifier)
{
    char *identity;
    char name[256];
    size_t name_size = identifier.end - identifier.start;
    size_t index;
    int written;

    if (name_size == 0U || name_size >= sizeof(name) ||
        request->limits.max_string_bytes == 0U ||
        request->limits.max_string_bytes == SIZE_MAX) {
        return NULL;
    }
    for (index = 0U; index < name_size; index += 1U) {
        name[index] = (char)source[identifier.start + index];
    }
    name[name_size] = '\0';
    identity = evo_project_allocate_zeroed(
        request->limits.max_string_bytes + 1U, sizeof(*identity));
    if (identity == NULL) {
        return NULL;
    }
    written = evo_project_format(
        identity,
        request->limits.max_string_bytes + 1U,
        "clang-decl-v1:%s:%s",
        request->target->file,
        name);
    if (written <= 0 || (size_t)written > request->limits.max_string_bytes) {
        evo_project_release(identity);
        return NULL;
    }
    return identity;
}

static const evo_project_compilation_record_t *evo_clang_ast_compilation_unit(
    const evo_project_clang_ast_context_t *context,
    const char *file)
{
    size_t index;

    for (index = 0U; index < context->compilation_unit_count; index += 1U) {
        if (context->compilation_units[index].file != NULL &&
            strcmp(context->compilation_units[index].file, file) == 0) {
            return &context->compilation_units[index];
        }
    }
    return NULL;
}

static bool evo_clang_ast_safe_standard(const char *argument)
{
    size_t index;

    if (argument == NULL || strncmp(argument, "-std=", 5U) != 0 || argument[5] == '\0') {
        return false;
    }
    for (index = 5U; argument[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)argument[index];

        if (!(isalnum((int)byte) != 0 || byte == '+' || byte == '-')) {
            return false;
        }
    }
    return true;
}

static bool evo_clang_ast_safe_define(const char *argument)
{
    size_t index;

    if (argument == NULL || argument[0] != '-' ||
        (argument[1] != 'D' && argument[1] != 'U') || argument[2] == '\0') {
        return false;
    }
    for (index = 2U; argument[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)argument[index];

        if (byte < 0x20U || byte == 0x7fU || byte == '@') {
            return false;
        }
    }
    return true;
}

static bool evo_clang_ast_ignored_flag(const char *argument)
{
    return strcmp(argument, "-c") == 0 || strcmp(argument, "-g") == 0 ||
           strcmp(argument, "-pipe") == 0 || strcmp(argument, "-pthread") == 0 ||
           strncmp(argument, "-O", 2U) == 0 || strncmp(argument, "-W", 2U) == 0;
}

static bool evo_clang_ast_forbidden_flag(const char *argument)
{
    static const char *const prefixes[] = {
        "@", "-Xclang", "-load", "-fplugin", "-plugin", "-cc1", "--config",
        "-include", "-imacros", "-B", "--sysroot", "-isysroot", "-isystem",
        "-iquote", "-MJ", "-MF", "-MT", "-MQ"};
    size_t index;

    for (index = 0U; index < sizeof(prefixes) / sizeof(prefixes[0]); index += 1U) {
        const size_t prefix_size = strlen(prefixes[index]);
        if (strncmp(argument, prefixes[index], prefix_size) == 0) {
            return true;
        }
    }
    return false;
}

static char **evo_clang_ast_arguments(
    const evo_project_compilation_record_t *unit,
    size_t *argument_count_out)
{
    size_t position = 0U;
    size_t index;
    char **arguments;

    if (unit == NULL || unit->command_form != EVO_PROJECT_COMPILE_COMMAND_ARGUMENTS ||
        unit->arguments == NULL || unit->argument_count == 0U ||
        unit->argument_count > SIZE_MAX - 8U) {
        return NULL;
    }
    arguments = evo_project_allocate_zeroed(unit->argument_count + 8U, sizeof(*arguments));
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
            evo_clang_ast_forbidden_flag(argument)) {
            evo_project_release(arguments);
            return NULL;
        }
        if (strcmp(argument, "-o") == 0) {
            if (index + 1U >= unit->argument_count) {
                evo_project_release(arguments);
                return NULL;
            }
            index += 1U;
        } else if (strcmp(argument, "-I") == 0) {
            if (index + 1U >= unit->argument_count ||
                !evo_clang_ast_relative_path_safe(unit->arguments[index + 1U])) {
                evo_project_release(arguments);
                return NULL;
            }
            arguments[position++] = (char *)argument;
            arguments[position++] = (char *)unit->arguments[++index];
        } else if (strncmp(argument, "-I", 2U) == 0) {
            if (!evo_clang_ast_relative_path_safe(argument + 2U)) {
                evo_project_release(arguments);
                return NULL;
            }
            arguments[position++] = (char *)argument;
        } else if (evo_clang_ast_safe_standard(argument) ||
                   evo_clang_ast_safe_define(argument)) {
            arguments[position++] = (char *)argument;
        } else if (evo_clang_ast_ignored_flag(argument) ||
                   strcmp(argument, unit->file) == 0 || argument[0] != '-') {
            continue;
        } else {
            evo_project_release(arguments);
            return NULL;
        }
    }
    arguments[position++] = (char *)unit->file;
    arguments[position] = NULL;
    *argument_count_out = position;
    return arguments;
}

static evo_project_transformation_status_t evo_clang_ast_dump(
    const evo_project_transformation_request_t *request,
    const evo_project_clang_ast_context_t *context,
    const evo_project_compilation_record_t *unit,
    evo_project_sandbox_result_t *sandbox)
{
    static const char *const environment[] = {"LANG=C", "LC_ALL=C"};
    char **arguments;
    size_t argument_count = 0U;
    evo_project_sandbox_command_t command;
    evo_project_sandbox_status_t status;

    arguments = evo_clang_ast_arguments(unit, &argument_count);
    if (arguments == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER;
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
            .cpu_time_ms = context->timeout_ms,
            .address_space_bytes = context->max_memory_bytes,
            .descendant_process_count = context->max_processes,
            .storage_bytes = context->max_storage_bytes,
            .output_bytes = context->max_output_bytes,
            .wall_timeout_ms = context->timeout_ms,
            .network_access = false,
        },
    };
    status = evo_project_sandbox_run(&command, sandbox);
    evo_project_release(arguments);
    if (status == EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    if (status != EVO_PROJECT_SANDBOX_SUCCESS || !sandbox->completed ||
        sandbox->exit_code != 0 || sandbox->resource_exhausted || sandbox->signaled ||
        sandbox->stdout_text == NULL || sandbox->stdout_bytes == 0U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER;
    }
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static evo_project_transformation_status_t evo_clang_ast_assignment(
    const evo_project_transformation_request_t *request,
    const unsigned char *source,
    evo_project_transformation_byte_range_t target,
    const char *ast,
    size_t ast_size,
    evo_project_clang_ast_context_t *context,
    evo_project_transformation_ast_result_t *result)
{
    evo_project_transformation_byte_range_t primary = {0};
    evo_project_transformation_byte_range_t duplicate = {0};
    evo_project_transformation_byte_range_t operand = {0};
    evo_project_clang_ast_authority_t authority = {0};
    evo_project_transformation_status_t status;
    evo_project_transformation_operator_t operator_kind;
    size_t position;
    char operator_char;
    bool compound = false;

    if (!evo_clang_ast_identifier_range(source, target.start, target.end, &primary)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    position = evo_clang_ast_skip_space(source, primary.end, target.end);
    if (position >= target.end) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    operator_char = (char)source[position];
    if (operator_char == '=') {
        position = evo_clang_ast_skip_space(source, position + 1U, target.end);
        if (!evo_clang_ast_identifier_range(source, position, target.end, &duplicate) ||
            !evo_clang_ast_bytes_equal(source, primary, duplicate)) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
        }
        position = evo_clang_ast_skip_space(source, duplicate.end, target.end);
        if (position >= target.end) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
        }
        operator_char = (char)source[position++];
        operand.start = evo_clang_ast_skip_space(source, position, target.end);
        operand.end = evo_clang_ast_trim_end(source, operand.start, target.end);
    } else {
        if (position + 1U >= target.end || source[position + 1U] != '=') {
            return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
        }
        compound = true;
        operand.start = evo_clang_ast_skip_space(source, position + 2U, target.end);
        operand.end = evo_clang_ast_trim_end(source, operand.start, target.end);
    }
    operator_kind = evo_clang_ast_operator(operator_char);
    if (operand.start >= operand.end ||
        operator_kind == EVO_PROJECT_TRANSFORMATION_OPERATOR_NONE) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    status = evo_project_clang_ast_authorize_assignment(
        ast,
        ast_size,
        target,
        primary,
        duplicate,
        operand,
        compound,
        operator_kind,
        &request->limits,
        &authority);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    if (!authority.primary_reference_resolved ||
        (!compound && !authority.duplicate_reference_matches)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    context->primary_declaration_identity =
        evo_clang_ast_declaration_identity(request, source, primary);
    if (context->primary_declaration_identity == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    if (!compound) {
        context->duplicate_declaration_identity =
            evo_clang_ast_duplicate(context->primary_declaration_identity);
        if (context->duplicate_declaration_identity == NULL) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
        }
    }
    result->form = compound ? EVO_PROJECT_AST_ASSIGNMENT_COMPOUND
                            : EVO_PROJECT_AST_ASSIGNMENT_BINARY;
    result->operator_kind = operator_kind;
    result->primary = primary;
    result->duplicate_primary = duplicate;
    result->operand = operand;
    result->primary_declaration_identity = context->primary_declaration_identity;
    result->duplicate_declaration_identity = context->duplicate_declaration_identity;
    result->primary_plain_identifier = true;
    result->volatile_access = authority.volatile_access;
    result->contains_macro = authority.contains_macro;
    result->language_extension = authority.language_extension;
    result->ambiguous_target = authority.ambiguous_target;
    result->result_type_matches_primary = authority.result_type_matches_primary;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static evo_project_transformation_status_t evo_clang_ast_condition(
    const evo_project_transformation_request_t *request,
    const unsigned char *source,
    evo_project_transformation_byte_range_t target,
    const char *ast,
    size_t ast_size,
    evo_project_transformation_ast_result_t *result)
{
    evo_project_clang_ast_authority_t authority = {0};
    evo_project_transformation_status_t status;
    size_t position = evo_clang_ast_skip_space(source, target.start, target.end);
    size_t end = evo_clang_ast_trim_end(source, position, target.end);
    bool double_negated = false;

    if (position + 2U <= end && source[position] == '!' &&
        source[position + 1U] == '!') {
        double_negated = true;
        result->form = EVO_PROJECT_AST_DOUBLE_NEGATED_CONDITION;
        result->operand.start = evo_clang_ast_skip_space(source, position + 2U, end);
        result->operand.end = end;
    } else {
        result->form = EVO_PROJECT_AST_SCALAR_CONDITION;
        result->operand.start = position;
        result->operand.end = end;
    }
    if (result->operand.start >= result->operand.end) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    status = evo_project_clang_ast_authorize_condition(
        ast,
        ast_size,
        target,
        double_negated,
        &request->limits,
        &authority);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    if (!authority.scalar_operand ||
        authority.condition_context == EVO_PROJECT_TRANSFORMATION_CONDITION_NONE) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    result->condition_context = authority.condition_context;
    result->scalar_operand = authority.scalar_operand;
    result->volatile_access = authority.volatile_access;
    result->contains_macro = authority.contains_macro;
    result->language_extension = authority.language_extension;
    result->ambiguous_target = authority.ambiguous_target;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static evo_project_transformation_status_t evo_clang_ast_shift(
    const evo_project_transformation_request_t *request,
    const unsigned char *source,
    evo_project_transformation_byte_range_t target,
    const char *ast,
    size_t ast_size,
    evo_project_clang_ast_context_t *context,
    evo_project_transformation_ast_result_t *result)
{
    evo_project_clang_ast_authority_t authority = {0};
    evo_project_transformation_status_t status;
    evo_project_transformation_byte_range_t expression;
    size_t content_start = target.start;
    size_t content_end = target.end;
    size_t position;
    bool shift_form = false;

    if (content_end - content_start >= 2U && source[content_start] == '(' &&
        source[content_end - 1U] == ')') {
        content_start += 1U;
        content_end -= 1U;
        shift_form = true;
    }
    expression.start = content_start;
    expression.end = content_end;
    if (!evo_clang_ast_identifier_range(
            source, content_start, content_end, &result->primary)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    position = evo_clang_ast_skip_space(source, result->primary.end, content_end);
    if (shift_form) {
        if (position + 1U >= content_end || source[position] != '<' ||
            source[position + 1U] != '<') {
            return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
        }
        position += 2U;
    } else {
        if (position >= content_end || source[position] != '*') {
            return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
        }
        position += 1U;
    }
    result->literal.start = evo_clang_ast_skip_space(source, position, content_end);
    result->literal.end =
        evo_clang_ast_trim_end(source, result->literal.start, content_end);
    if (result->literal.start >= result->literal.end ||
        !evo_clang_ast_parse_u64(
            source, result->literal, &result->literal_value)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    status = evo_project_clang_ast_authorize_shift(
        ast,
        ast_size,
        expression,
        result->primary,
        result->literal,
        shift_form,
        &request->limits,
        &authority);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    if (!authority.primary_reference_resolved ||
        !authority.result_unsigned_integer) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    context->primary_declaration_identity =
        evo_clang_ast_declaration_identity(request, source, result->primary);
    if (context->primary_declaration_identity == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    result->primary_declaration_identity = context->primary_declaration_identity;
    result->form = shift_form ? EVO_PROJECT_AST_UNSIGNED_SHIFT_POWER_OF_TWO
                              : EVO_PROJECT_AST_UNSIGNED_MULTIPLY_POWER_OF_TWO;
    result->operator_kind = shift_form
                                ? EVO_PROJECT_TRANSFORMATION_OPERATOR_SHIFT_LEFT
                                : EVO_PROJECT_TRANSFORMATION_OPERATOR_MULTIPLY;
    result->volatile_access = authority.volatile_access;
    result->contains_macro = authority.contains_macro;
    result->language_extension = authority.language_extension;
    result->ambiguous_target = authority.ambiguous_target;
    result->result_width_bits = authority.result_width_bits;
    result->result_unsigned_integer = authority.result_unsigned_integer;
    result->result_type_matches_primary = authority.result_type_matches_primary;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static bool evo_clang_ast_source_matches_request(
    const evo_project_transformation_request_t *request,
    const unsigned char *source,
    size_t source_size)
{
    evo_project_fingerprint_t fingerprint;
    char formatted[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];

    if (request == NULL || source == NULL || request->source_fingerprint == NULL ||
        source_size != request->source_size) {
        return false;
    }
    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(&fingerprint, source, source_size);
    evo_project_fingerprint_format(fingerprint.value, formatted);
    return strcmp(formatted, request->source_fingerprint) == 0;
}

evo_project_transformation_status_t evo_project_clang_ast_provider(
    const evo_project_transformation_request_t *request,
    void *opaque,
    evo_project_transformation_ast_result_t *result)
{
    evo_project_clang_ast_context_t *context = opaque;
    const evo_project_compilation_record_t *unit;
    const evo_project_provider_record_t *provider;
    evo_project_sandbox_result_t sandbox = {0};
    unsigned char *source = NULL;
    size_t source_size = 0U;
    size_t target_start;
    size_t target_end;
    evo_project_transformation_byte_range_t target;
    bool comment = false;
    bool preprocessor = false;
    evo_project_transformation_status_t status;

    if (request == NULL || context == NULL || result == NULL || request->target == NULL ||
        request->snapshot_path == NULL || request->transformation_identity == NULL ||
        request->transformation_version != 1U || request->network_access ||
        context->timeout_ms == 0U || context->max_memory_bytes == 0U ||
        context->max_processes == 0U || context->max_storage_bytes == 0U ||
        context->max_output_bytes == 0U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_INVALID_ARGUMENT;
    }
    provider = evo_project_provider_find(EVO_PROJECT_PROVIDER_CLANG_AST_ID);
    if (provider == NULL || !evo_project_provider_available(provider)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER;
    }
    unit = evo_clang_ast_compilation_unit(context, request->target->file);
    if (unit == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER;
    }
    evo_project_clang_ast_context_destroy(context);
    if (!evo_clang_ast_read_source(request, &source, &source_size)) {
        evo_project_release(source);
        return EVO_PROJECT_TRANSFORMATION_ERROR_SOURCE_IO;
    }
    if (!evo_clang_ast_source_matches_request(request, source, source_size)) {
        evo_project_release(source);
        return EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_CHANGED;
    }
    if (!evo_clang_ast_offset(
            source,
            source_size,
            request->target->line,
            request->target->column,
            &target_start) ||
        !evo_clang_ast_offset(
            source,
            source_size,
            request->target->end_line,
            request->target->end_column,
            &target_end) ||
        target_start >= target_end || target_end > source_size) {
        evo_project_release(source);
        return EVO_PROJECT_TRANSFORMATION_ERROR_SOURCE_IO;
    }
    target.start = target_start;
    target.end = target_end;
    status = evo_clang_ast_dump(request, context, unit, &sandbox);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        evo_project_release(source);
        evo_project_sandbox_result_destroy(&sandbox);
        return status;
    }
    context->location_identity = evo_clang_ast_duplicate(request->target->location_identity);
    context->file = evo_clang_ast_duplicate(request->target->file);
    if (context->location_identity == NULL || context->file == NULL) {
        status = EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    *result = (evo_project_transformation_ast_result_t){0};
    result->schema_version = EVO_PROJECT_TRANSFORMATION_AST_SCHEMA_VERSION;
    result->completed = true;
    result->location_identity = context->location_identity;
    result->file = context->file;
    result->target = target;
    result->volatile_access = false;
    result->contains_macro = false;
    result->language_extension = false;
    result->ambiguous_target = false;
    result->alias_assumption_required = false;
    (void)evo_clang_ast_contains_unsafe_text(source, target, &comment, &preprocessor);
    result->contains_comment = comment;
    result->contains_preprocessor = preprocessor;

    if (strcmp(request->transformation_identity, "catalyst.evo.c.assignment-to-compound") == 0) {
        status = evo_clang_ast_assignment(
            request,
            source,
            target,
            sandbox.stdout_text,
            sandbox.stdout_bytes,
            context,
            result);
    } else if (strcmp(request->transformation_identity, "catalyst.evo.c.double-negation-condition") == 0) {
        status = evo_clang_ast_condition(
            request,
            source,
            target,
            sandbox.stdout_text,
            sandbox.stdout_bytes,
            result);
    } else if (strcmp(request->transformation_identity, "catalyst.evo.c.unsigned-multiply-to-shift") == 0) {
        status = evo_clang_ast_shift(
            request,
            source,
            target,
            sandbox.stdout_text,
            sandbox.stdout_bytes,
            context,
            result);
    } else {
        status = EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }

cleanup:
    evo_project_release(source);
    evo_project_sandbox_result_destroy(&sandbox);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        evo_project_clang_ast_context_destroy(context);
        *result = (evo_project_transformation_ast_result_t){0};
    }
    return status;
}
