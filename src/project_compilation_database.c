#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "internal/project_compilation_database.h"

#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_manifest.h"
#include "internal/project_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * The compilation database is normalized into an explicit ordered registry.
 * The original JSON remains in the immutable snapshot, while this projection
 * is the stable, human-readable build description consumed by later stages.
 */

static evo_project_status_t evo_project_compdb_json_status(
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

static bool evo_project_compdb_text_valid(const char *value)
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

static char *evo_project_compdb_duplicate(const char *value)
{
    const size_t size = strlen(value);
    char *copy;
    size_t index;

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

static evo_project_status_t evo_project_compdb_decode(
    const char *text,
    const evo_project_json_token_t *token,
    size_t maximum_bytes,
    char **value)
{
    const evo_project_json_status_t json_status =
        evo_project_json_decode_string(text, token, maximum_bytes, value);
    const evo_project_status_t status =
        evo_project_compdb_json_status(json_status);

    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    if (!evo_project_compdb_text_valid(*value)) {
        evo_project_release(*value);
        *value = NULL;
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_compdb_string_member(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *name,
    size_t maximum_bytes,
    bool required,
    char **value)
{
    size_t value_index;
    const int found = evo_project_json_object_get(
        text, tokens, token_count, object_index, name, &value_index);

    if (found < 0 || (required && found == 0)) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    if (found == 0) {
        *value = NULL;
        return EVO_PROJECT_SUCCESS;
    }
    return evo_project_compdb_decode(
        text, &tokens[value_index], maximum_bytes, value);
}

static char *evo_project_compdb_join(
    const char *left,
    const char *right,
    size_t maximum_bytes)
{
    const size_t left_size = strlen(left);
    const size_t right_size = strlen(right);
    char *joined;
    size_t index;

    if (left_size > SIZE_MAX - right_size ||
        left_size + right_size > SIZE_MAX - 2U ||
        left_size + right_size + 1U > maximum_bytes) {
        return NULL;
    }
    joined = evo_project_allocate_zeroed(left_size + right_size + 2U, sizeof(*joined));
    if (joined == NULL) {
        return NULL;
    }
    for (index = 0U; index < left_size; index += 1U) {
        joined[index] = left[index];
    }
    joined[left_size] = '/';
    for (index = 0U; index < right_size; index += 1U) {
        joined[left_size + 1U + index] = right[index];
    }
    joined[left_size + right_size + 1U] = '\0';
    return joined;
}

static evo_project_status_t evo_project_compdb_normalize_directory(
    const char *value,
    const char *authorized_root,
    size_t maximum_bytes,
    char **normalized)
{
    const size_t root_size = strlen(authorized_root);
    const char *relative = value;

    if (value[0] == '/') {
        if (strncmp(value, authorized_root, root_size) != 0 ||
            (value[root_size] != '\0' && value[root_size] != '/')) {
            return EVO_PROJECT_ERROR_PATH_INVALID;
        }
        relative = value[root_size] == '/' ? value + root_size + 1U : ".";
    }
    if (strcmp(relative, ".") != 0 &&
        !evo_project_relative_path_valid(relative, maximum_bytes)) {
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    *normalized = evo_project_compdb_duplicate(relative);
    return *normalized == NULL ? EVO_PROJECT_ERROR_OUT_OF_MEMORY
                               : EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_compdb_normalize_path(
    const char *value,
    const char *directory,
    const char *authorized_root,
    size_t maximum_bytes,
    char **normalized)
{
    const size_t root_size = strlen(authorized_root);
    const char *relative = value;
    char *joined = NULL;

    if (value[0] == '/') {
        if (strncmp(value, authorized_root, root_size) != 0 ||
            value[root_size] != '/') {
            return EVO_PROJECT_ERROR_PATH_INVALID;
        }
        relative = value + root_size + 1U;
    } else if (strcmp(directory, ".") != 0) {
        const size_t directory_size = strlen(directory);
        const size_t value_size = strlen(value);

        if (directory_size > SIZE_MAX - value_size ||
            directory_size + value_size > SIZE_MAX - 1U ||
            directory_size + value_size + 1U > maximum_bytes) {
            return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
        }
        joined = evo_project_compdb_join(directory, value, maximum_bytes);
        if (joined == NULL) {
            return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
        }
        relative = joined;
    }
    if (!evo_project_relative_path_valid(relative, maximum_bytes)) {
        evo_project_release(joined);
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    *normalized = evo_project_compdb_duplicate(relative);
    evo_project_release(joined);
    return *normalized == NULL ? EVO_PROJECT_ERROR_OUT_OF_MEMORY
                               : EVO_PROJECT_SUCCESS;
}

static bool evo_project_compdb_file_captured(
    const char *path,
    const evo_project_file_record_t *files,
    size_t file_count)
{
    size_t index;

    for (index = 0U; index < file_count; index += 1U) {
        if (strcmp(path, files[index].path) == 0) {
            return true;
        }
    }
    return false;
}

static void evo_project_compdb_release_record(
    evo_project_compilation_record_t *record)
{
    size_t index;
    char **arguments = (char **)record->arguments;

    evo_project_release((char *)record->directory);
    evo_project_release((char *)record->file);
    evo_project_release((char *)record->output);
    for (index = 0U; index < record->argument_count; index += 1U) {
        evo_project_release(arguments[index]);
    }
    evo_project_release(arguments);
    evo_project_release((char *)record->command);
    *record = (evo_project_compilation_record_t){0};
}

void evo_project_compilation_database_destroy(
    evo_project_compilation_record_t *records,
    size_t record_count)
{
    size_t index;

    if (records == NULL) {
        return;
    }
    for (index = 0U; index < record_count; index += 1U) {
        evo_project_compdb_release_record(&records[index]);
    }
    evo_project_release(records);
}

static evo_project_status_t evo_project_compdb_arguments(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t array_index,
    const evo_project_ingest_limits_t *limits,
    evo_project_compilation_record_t *record)
{
    char **arguments;
    size_t token_index;
    size_t total_bytes = 0U;
    size_t item;

    if (array_index >= token_count ||
        tokens[array_index].type != EVO_PROJECT_JSON_ARRAY ||
        tokens[array_index].child_count == 0U ||
        tokens[array_index].child_count > limits->max_command_args) {
        return array_index < token_count &&
                       tokens[array_index].type == EVO_PROJECT_JSON_ARRAY &&
                       tokens[array_index].child_count > limits->max_command_args
                   ? EVO_PROJECT_ERROR_RESOURCE_LIMIT
                   : EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    if (tokens[array_index].child_count > SIZE_MAX / sizeof(*arguments)) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    arguments = evo_project_allocate_zeroed(tokens[array_index].child_count, sizeof(*arguments));
    if (arguments == NULL) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    token_index = array_index + 1U;
    for (item = 0U; item < tokens[array_index].child_count; item += 1U) {
        evo_project_status_t status;
        size_t argument_size;

        if (token_index >= token_count ||
            tokens[token_index].parent != array_index) {
            status = EVO_PROJECT_ERROR_MANIFEST_INVALID;
        } else {
            status = evo_project_compdb_decode(
                text,
                &tokens[token_index],
                limits->max_string_bytes,
                &arguments[item]);
        }
        if (status != EVO_PROJECT_SUCCESS) {
            size_t release;

            for (release = 0U; release < item; release += 1U) {
                evo_project_release(arguments[release]);
            }
            evo_project_release(arguments);
            return status;
        }
        argument_size = strlen(arguments[item]);
        if (argument_size > limits->max_command_bytes - total_bytes) {
            size_t release;

            for (release = 0U; release <= item; release += 1U) {
                evo_project_release(arguments[release]);
            }
            evo_project_release(arguments);
            return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
        }
        total_bytes += argument_size;
        token_index = evo_project_json_next(tokens, token_count, token_index);
    }
    record->command_form = EVO_PROJECT_COMPILE_COMMAND_ARGUMENTS;
    record->argument_count = tokens[array_index].child_count;
    record->arguments = (const char *const *)arguments;
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_compdb_parse_record(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *authorized_root,
    const evo_project_ingest_limits_t *limits,
    const evo_project_file_record_t *files,
    size_t file_count,
    evo_project_compilation_record_t *record)
{
    static const char *const allowed[] = {
        "directory", "file", "arguments", "command", "output"};
    char *raw_directory = NULL;
    char *raw_file = NULL;
    char *raw_output = NULL;
    size_t arguments_index = 0U;
    size_t command_index = 0U;
    const int has_arguments = evo_project_json_object_get(
        text,
        tokens,
        token_count,
        object_index,
        "arguments",
        &arguments_index);
    const int has_command = evo_project_json_object_get(
        text, tokens, token_count, object_index, "command", &command_index);
    evo_project_status_t status;

    if (object_index >= token_count ||
        tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_json_object_has_only(
            text,
            tokens,
            token_count,
            object_index,
            allowed,
            sizeof(allowed) / sizeof(allowed[0])) ||
        has_arguments < 0 || has_command < 0 ||
        (has_arguments == 1) == (has_command == 1)) {
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    status = evo_project_compdb_string_member(
        text,
        tokens,
        token_count,
        object_index,
        "directory",
        limits->max_path_bytes,
        true,
        &raw_directory);
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_compdb_string_member(
            text,
            tokens,
            token_count,
            object_index,
            "file",
            limits->max_path_bytes,
            true,
            &raw_file);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_compdb_string_member(
            text,
            tokens,
            token_count,
            object_index,
            "output",
            limits->max_path_bytes,
            false,
            &raw_output);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_compdb_normalize_directory(
            raw_directory,
            authorized_root,
            limits->max_path_bytes,
            (char **)&record->directory);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_compdb_normalize_path(
            raw_file,
            record->directory,
            authorized_root,
            limits->max_path_bytes,
            (char **)&record->file);
    }
    if (status == EVO_PROJECT_SUCCESS &&
        !evo_project_compdb_file_captured(record->file, files, file_count)) {
        status = EVO_PROJECT_ERROR_PATH_INVALID;
    }
    if (status == EVO_PROJECT_SUCCESS && raw_output != NULL) {
        status = evo_project_compdb_normalize_path(
            raw_output,
            record->directory,
            authorized_root,
            limits->max_path_bytes,
            (char **)&record->output);
    }
    if (status == EVO_PROJECT_SUCCESS && has_arguments == 1) {
        status = evo_project_compdb_arguments(
            text, tokens, token_count, arguments_index, limits, record);
    }
    if (status == EVO_PROJECT_SUCCESS && has_command == 1) {
        status = evo_project_compdb_decode(
            text,
            &tokens[command_index],
            limits->max_command_bytes,
            (char **)&record->command);
        if (status == EVO_PROJECT_SUCCESS) {
            record->command_form = EVO_PROJECT_COMPILE_COMMAND_SHELL;
        }
    }
    evo_project_release(raw_directory);
    evo_project_release(raw_file);
    evo_project_release(raw_output);
    if (status != EVO_PROJECT_SUCCESS) {
        evo_project_compdb_release_record(record);
    }
    return status;
}

static int evo_project_compdb_record_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_compilation_record_t *left = left_value;
    const evo_project_compilation_record_t *right = right_value;
    int order = strcmp(left->file, right->file);
    size_t index;

    if (order != 0) {
        return order;
    }
    order = strcmp(left->directory, right->directory);
    if (order != 0) {
        return order;
    }
    if (left->output == NULL || right->output == NULL) {
        if (left->output != right->output) {
            return left->output == NULL ? -1 : 1;
        }
    } else {
        order = strcmp(left->output, right->output);
        if (order != 0) {
            return order;
        }
    }
    if (left->command_form != right->command_form) {
        return left->command_form < right->command_form ? -1 : 1;
    }
    if (left->command_form == EVO_PROJECT_COMPILE_COMMAND_SHELL) {
        return strcmp(left->command, right->command);
    }
    if (left->argument_count != right->argument_count) {
        return left->argument_count < right->argument_count ? -1 : 1;
    }
    for (index = 0U; index < left->argument_count; index += 1U) {
        order = strcmp(left->arguments[index], right->arguments[index]);
        if (order != 0) {
            return order;
        }
    }
    return 0;
}

static uint64_t evo_project_compdb_fingerprint(
    const evo_project_compilation_record_t *records,
    size_t record_count)
{
    evo_project_fingerprint_t fingerprint;
    size_t index;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, "catalyst.evo-project-build-description.v1");
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)record_count);
    for (index = 0U; index < record_count; index += 1U) {
        size_t argument;

        evo_project_fingerprint_string(&fingerprint, records[index].file);
        evo_project_fingerprint_string(&fingerprint, records[index].directory);
        evo_project_fingerprint_string(
            &fingerprint,
            records[index].output == NULL ? "" : records[index].output);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)records[index].command_form);
        if (records[index].command_form == EVO_PROJECT_COMPILE_COMMAND_SHELL) {
            evo_project_fingerprint_string(
                &fingerprint, records[index].command);
        } else {
            evo_project_fingerprint_u64(
                &fingerprint, (uint64_t)records[index].argument_count);
            for (argument = 0U; argument < records[index].argument_count;
                 argument += 1U) {
                evo_project_fingerprint_string(
                    &fingerprint, records[index].arguments[argument]);
            }
        }
    }
    return fingerprint.value;
}

evo_project_status_t evo_project_compilation_database_parse(
    const char *text,
    size_t text_size,
    const char *authorized_root,
    const evo_project_ingest_limits_t *limits,
    const evo_project_file_record_t *files,
    size_t file_count,
    evo_project_compilation_record_t **records,
    size_t *record_count,
    uint64_t *normalized_fingerprint)
{
    evo_project_json_token_t *tokens;
    size_t token_count = 0U;
    evo_project_json_status_t json_status;
    evo_project_compilation_record_t *parsed;
    size_t entry_count;
    size_t item;
    size_t token_index;
    evo_project_status_t status = EVO_PROJECT_SUCCESS;

    if (text == NULL || text_size == 0U || authorized_root == NULL ||
        limits == NULL || files == NULL || file_count == 0U || records == NULL ||
        record_count == NULL || normalized_fingerprint == NULL ||
        limits->max_json_tokens == 0U || limits->max_json_depth == 0U) {
        return EVO_PROJECT_ERROR_INVALID_ARGUMENT;
    }
    if (text_size > limits->max_compilation_database_bytes ||
        limits->max_json_tokens > SIZE_MAX / sizeof(*tokens)) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    *records = NULL;
    *record_count = 0U;
    *normalized_fingerprint = 0U;
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
    status = evo_project_compdb_json_status(json_status);
    if (status != EVO_PROJECT_SUCCESS || token_count == 0U ||
        tokens[0].type != EVO_PROJECT_JSON_ARRAY ||
        tokens[0].child_count == 0U ||
        tokens[0].child_count > limits->max_files) {
        if (status == EVO_PROJECT_SUCCESS) {
            status = tokens[0].child_count > limits->max_files
                         ? EVO_PROJECT_ERROR_RESOURCE_LIMIT
                         : EVO_PROJECT_ERROR_MANIFEST_INVALID;
        }
        evo_project_release(tokens);
        return status;
    }
    if (tokens[0].child_count > SIZE_MAX / sizeof(*parsed)) {
        evo_project_release(tokens);
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    entry_count = tokens[0].child_count;
    parsed = evo_project_allocate_zeroed(entry_count, sizeof(*parsed));
    if (parsed == NULL) {
        evo_project_release(tokens);
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    token_index = 1U;
    for (item = 0U; item < entry_count; item += 1U) {
        if (token_index >= token_count || tokens[token_index].parent != 0U) {
            status = EVO_PROJECT_ERROR_MANIFEST_INVALID;
            break;
        }
        status = evo_project_compdb_parse_record(
            text,
            tokens,
            token_count,
            token_index,
            authorized_root,
            limits,
            files,
            file_count,
            &parsed[item]);
        if (status != EVO_PROJECT_SUCCESS) {
            break;
        }
        token_index = evo_project_json_next(tokens, token_count, token_index);
    }
    evo_project_release(tokens);
    if (status != EVO_PROJECT_SUCCESS) {
        evo_project_compilation_database_destroy(parsed, item + 1U);
        return status;
    }
    qsort(
        parsed,
        entry_count,
        sizeof(*parsed),
        evo_project_compdb_record_compare);
    for (item = 1U; item < entry_count; item += 1U) {
        if (strcmp(parsed[item - 1U].file, parsed[item].file) == 0) {
            evo_project_compilation_database_destroy(
                parsed, entry_count);
            return EVO_PROJECT_ERROR_MANIFEST_INVALID;
        }
    }
    *record_count = entry_count;
    *normalized_fingerprint =
        evo_project_compdb_fingerprint(parsed, *record_count);
    *records = parsed;
    return EVO_PROJECT_SUCCESS;
}

evo_project_status_t evo_project_compilation_database_load(
    const char *snapshot_root,
    const char *relative_path,
    const char *authorized_root,
    const evo_project_ingest_limits_t *limits,
    const evo_project_file_record_t *files,
    size_t file_count,
    evo_project_compilation_record_t **records,
    size_t *record_count,
    uint64_t *normalized_fingerprint)
{
    int root_fd;
    int file_fd;
    struct stat metadata;
    char *bytes;
    size_t size;
    size_t position = 0U;
    evo_project_status_t status;

    if (snapshot_root == NULL || relative_path == NULL ||
        authorized_root == NULL || limits == NULL || files == NULL ||
        records == NULL || record_count == NULL ||
        normalized_fingerprint == NULL) {
        return EVO_PROJECT_ERROR_INVALID_ARGUMENT;
    }
    root_fd = open(
        snapshot_root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (root_fd < 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    file_fd = openat(root_fd, relative_path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    (void)close(root_fd);
    if (file_fd < 0 || fstat(file_fd, &metadata) != 0 ||
        !S_ISREG(metadata.st_mode) || metadata.st_size <= 0) {
        if (file_fd >= 0) {
            (void)close(file_fd);
        }
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    if ((uintmax_t)metadata.st_size >
        (uintmax_t)limits->max_compilation_database_bytes) {
        (void)close(file_fd);
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    size = (size_t)metadata.st_size;
    if (size == SIZE_MAX) {
        (void)close(file_fd);
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    bytes = evo_project_allocate_zeroed(size + 1U, sizeof(*bytes));
    if (bytes == NULL) {
        (void)close(file_fd);
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    while (position < size) {
        const ssize_t read_count = read(file_fd, bytes + position, size - position);

        if (read_count < 0 && errno == EINTR) {
            continue;
        }
        if (read_count <= 0) {
            evo_project_release(bytes);
            (void)close(file_fd);
            return EVO_PROJECT_ERROR_SOURCE_IO;
        }
        position += (size_t)read_count;
    }
    if (close(file_fd) != 0) {
        evo_project_release(bytes);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    bytes[size] = '\0';
    status = evo_project_compilation_database_parse(
        bytes,
        size,
        authorized_root,
        limits,
        files,
        file_count,
        records,
        record_count,
        normalized_fingerprint);
    evo_project_release(bytes);
    return status;
}
