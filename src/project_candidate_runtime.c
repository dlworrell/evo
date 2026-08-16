#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "internal/project_candidate.h"
#include "internal/project_candidate_internal.h"

#include "internal/project_baseline_owner.h"
#include "internal/project_candidate_owner.h"
#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_recipe_owner.h"
#include "internal/project_runtime.h"
#include "internal/project_snapshot.h"
#include "internal/project_transformation_owner.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

bool evo_candidate_limits_valid(const evo_project_candidate_limits_t *limits)
{
    return limits != NULL && limits->max_string_bytes > 0U &&
           limits->max_path_bytes > 0U && limits->max_files > 0U &&
           limits->max_file_bytes > 0U && limits->max_total_file_bytes > 0U &&
           limits->max_edits > 0U && limits->max_patch_bytes > 0U &&
           limits->max_evidence_bytes > 0U;
}

char *evo_candidate_duplicate(const char *value)
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

bool evo_candidate_buffer_open(evo_candidate_buffer_t *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0U || capacity == SIZE_MAX) {
        return false;
    }
    buffer->bytes = evo_project_allocate_zeroed(capacity + 1U, sizeof(*buffer->bytes));
    if (buffer->bytes == NULL) {
        return false;
    }
    buffer->capacity = capacity;
    return true;
}

void evo_candidate_buffer_close(evo_candidate_buffer_t *buffer)
{
    if (buffer == NULL) {
        return;
    }
    evo_project_release(buffer->bytes);
    *buffer = (evo_candidate_buffer_t){0};
}

bool evo_candidate_buffer_append_bytes(
    evo_candidate_buffer_t *buffer,
    const void *bytes,
    size_t count)
{
    const unsigned char *source = bytes;
    size_t index;

    if (buffer == NULL || (bytes == NULL && count > 0U) ||
        count > buffer->capacity - buffer->size) {
        return false;
    }
    for (index = 0U; index < count; index += 1U) {
        buffer->bytes[buffer->size + index] = (char)source[index];
    }
    buffer->size += count;
    buffer->bytes[buffer->size] = '\0';
    return true;
}

bool evo_candidate_buffer_append_text(
    evo_candidate_buffer_t *buffer,
    const char *text)
{
    return text != NULL &&
           evo_candidate_buffer_append_bytes(buffer, text, strlen(text));
}

bool evo_candidate_buffer_append_u64(
    evo_candidate_buffer_t *buffer,
    uint64_t value)
{
    char text[32];
    const int written = evo_project_format(text, sizeof(text), "%llu", (unsigned long long)value);

    return written > 0 && (size_t)written < sizeof(text) &&
           evo_candidate_buffer_append_bytes(buffer, text, (size_t)written);
}

bool evo_candidate_buffer_append_size(
    evo_candidate_buffer_t *buffer,
    size_t value)
{
    char text[32];
    const int written = evo_project_format(text, sizeof(text), "%zu", value);

    return written > 0 && (size_t)written < sizeof(text) &&
           evo_candidate_buffer_append_bytes(buffer, text, (size_t)written);
}

bool evo_candidate_buffer_append_json_string(
    evo_candidate_buffer_t *buffer,
    const char *text)
{
    size_t index;

    if (text == NULL || !evo_candidate_buffer_append_text(buffer, "\"")) {
        return false;
    }
    for (index = 0U; text[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)text[index];
        char escape[7];
        int written;

        if (byte == (unsigned char)'\"') {
            if (!evo_candidate_buffer_append_text(buffer, "\\\"")) {
                return false;
            }
        } else if (byte == (unsigned char)'\\') {
            if (!evo_candidate_buffer_append_text(buffer, "\\\\")) {
                return false;
            }
        } else if (byte == (unsigned char)'\b') {
            if (!evo_candidate_buffer_append_text(buffer, "\\b")) {
                return false;
            }
        } else if (byte == (unsigned char)'\f') {
            if (!evo_candidate_buffer_append_text(buffer, "\\f")) {
                return false;
            }
        } else if (byte == (unsigned char)'\n') {
            if (!evo_candidate_buffer_append_text(buffer, "\\n")) {
                return false;
            }
        } else if (byte == (unsigned char)'\r') {
            if (!evo_candidate_buffer_append_text(buffer, "\\r")) {
                return false;
            }
        } else if (byte == (unsigned char)'\t') {
            if (!evo_candidate_buffer_append_text(buffer, "\\t")) {
                return false;
            }
        } else if (byte < 0x20U) {
            written = evo_project_format(escape, sizeof(escape), "\\u%04x", (unsigned int)byte);
            if (written != 6 || !evo_candidate_buffer_append_bytes(buffer, escape, 6U)) {
                return false;
            }
        } else if (!evo_candidate_buffer_append_bytes(buffer, &byte, 1U)) {
            return false;
        }
    }
    return evo_candidate_buffer_append_text(buffer, "\"");
}

static bool evo_candidate_path_component_valid(const char *component, size_t size)
{
    size_t index;

    if (size == 0U || (size == 1U && component[0] == '.') ||
        (size == 2U && component[0] == '.' && component[1] == '.')) {
        return false;
    }
    for (index = 0U; index < size; index += 1U) {
        const unsigned char byte = (unsigned char)component[index];

        if (byte < 0x20U || byte == 0x7fU || component[index] == '\\') {
            return false;
        }
    }
    return true;
}

bool evo_candidate_relative_path_valid(const char *path, size_t maximum_bytes)
{
    const size_t size = path == NULL ? 0U : strlen(path);
    size_t start = 0U;
    size_t index;

    if (size == 0U || size > maximum_bytes || path[0] == '/' || path[size - 1U] == '/') {
        return false;
    }
    for (index = 0U; index <= size; index += 1U) {
        if (index == size || path[index] == '/') {
            if (!evo_candidate_path_component_valid(path + start, index - start)) {
                return false;
            }
            start = index + 1U;
        }
    }
    return true;
}

static bool evo_candidate_output_name_valid(const char *name)
{
    size_t index;

    if (name == NULL || name[0] == '\0' || strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) {
        return false;
    }
    for (index = 0U; name[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)name[index];

        if (byte < 0x20U || byte == 0x7fU || name[index] == '/' ||
            name[index] == '\\') {
            return false;
        }
    }
    return true;
}

char *evo_candidate_join_path(const char *left, const char *right)
{
    const size_t left_size = left == NULL ? 0U : strlen(left);
    const size_t right_size = right == NULL ? 0U : strlen(right);
    const bool separator = left_size > 0U && left[left_size - 1U] != '/';
    size_t total;
    size_t position = 0U;
    size_t index;
    char *joined;

    if (left == NULL || right == NULL || left_size > SIZE_MAX - right_size ||
        left_size + right_size > SIZE_MAX - 2U) {
        return NULL;
    }
    total = left_size + right_size + (separator ? 1U : 0U);
    joined = evo_project_allocate_zeroed(total + 1U, sizeof(*joined));
    if (joined == NULL) {
        return NULL;
    }
    for (index = 0U; index < left_size; index += 1U) {
        joined[position++] = left[index];
    }
    if (separator) {
        joined[position++] = '/';
    }
    for (index = 0U; index < right_size; index += 1U) {
        joined[position++] = right[index];
    }
    joined[position] = '\0';
    return joined;
}

static bool evo_candidate_path_within(const char *parent, const char *candidate)
{
    const size_t parent_size = parent == NULL ? 0U : strlen(parent);

    if (parent == NULL || candidate == NULL || parent_size == 0U) {
        return false;
    }
    if (parent_size == 1U && parent[0] == '/') {
        return candidate[0] == '/';
    }
    return strcmp(parent, candidate) == 0 ||
           (strncmp(parent, candidate, parent_size) == 0 &&
            candidate[parent_size] == '/');
}

evo_project_candidate_status_t evo_candidate_validate_output_path(
    const evo_project_candidate_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    char **normalized_output)
{
    const char *last_slash;
    const char *base_name;
    size_t parent_size;
    char *parent_input = NULL;
    char *parent_real = NULL;
    char *candidate_path = NULL;
    struct stat metadata;
    size_t index;

    if (config->output_path == NULL || config->output_path[0] != '/' ||
        strlen(config->output_path) > config->limits.max_path_bytes) {
        return EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID;
    }
    last_slash = strrchr(config->output_path, '/');
    if (last_slash == NULL || last_slash[1] == '\0' ||
        !evo_candidate_output_name_valid(last_slash + 1)) {
        return EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID;
    }
    base_name = last_slash + 1;
    parent_size = (size_t)(last_slash - config->output_path);
    if (parent_size == 0U) {
        parent_input = evo_candidate_duplicate("/");
    } else {
        parent_input = evo_project_allocate_zeroed(parent_size + 1U, sizeof(*parent_input));
        if (parent_input != NULL) {
            for (index = 0U; index < parent_size; index += 1U) {
                parent_input[index] = config->output_path[index];
            }
            parent_input[parent_size] = '\0';
        }
    }
    if (parent_input == NULL) {
        return EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
    }
    parent_real = realpath(parent_input, NULL);
    evo_project_release(parent_input);
    if (parent_real == NULL) {
        return EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID;
    }
    candidate_path = evo_candidate_join_path(parent_real, base_name);
    evo_project_release(parent_real);
    if (candidate_path == NULL) {
        return EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
    }
    if (strlen(candidate_path) > config->limits.max_path_bytes) {
        evo_project_release(candidate_path);
        return EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
    }
    if (lstat(candidate_path, &metadata) == 0) {
        evo_project_release(candidate_path);
        return EVO_PROJECT_CANDIDATE_ERROR_OUTPUT_EXISTS;
    }
    if (errno != ENOENT) {
        evo_project_release(candidate_path);
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    if (evo_candidate_path_within(baseline_owner->snapshot_path, candidate_path) ||
        evo_candidate_path_within(baseline_owner->output_path, candidate_path) ||
        evo_candidate_path_within(baseline_owner->authorized_root, candidate_path)) {
        evo_project_release(candidate_path);
        return EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID;
    }
    *normalized_output = candidate_path;
    return EVO_PROJECT_CANDIDATE_SUCCESS;
}

static int evo_candidate_open_relative_file(const char *root, const char *path)
{
    const size_t path_size = strlen(path);
    char *component;
    size_t position = 0U;
    int directory_fd;

    if (!evo_candidate_relative_path_valid(path, path_size)) {
        errno = EINVAL;
        return -1;
    }
    component = evo_project_allocate_zeroed(path_size + 1U, sizeof(*component));
    if (component == NULL) {
        errno = ENOMEM;
        return -1;
    }
    directory_fd = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory_fd < 0) {
        evo_project_release(component);
        return -1;
    }
    while (position < path_size) {
        size_t component_size = 0U;
        bool final_component;
        int next_fd;

        while (position < path_size && path[position] != '/') {
            component[component_size++] = path[position++];
        }
        component[component_size] = '\0';
        final_component = position == path_size;
        next_fd = openat(
            directory_fd,
            component,
            final_component ? O_RDONLY | O_NOFOLLOW | O_CLOEXEC
                            : O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
            0);
        (void)close(directory_fd);
        if (next_fd < 0) {
            evo_project_release(component);
            return -1;
        }
        directory_fd = next_fd;
        if (!final_component) {
            position += 1U;
        }
    }
    evo_project_release(component);
    return directory_fd;
}

evo_project_candidate_status_t evo_candidate_read_snapshot_file(
    const evo_project_baseline_owner_t *baseline_owner,
    const evo_project_file_record_t *record,
    const evo_project_candidate_limits_t *limits,
    unsigned char **bytes,
    size_t *size)
{
    struct stat metadata;
    evo_project_fingerprint_t fingerprint;
    size_t position = 0U;
    int file_fd;

    if (record->size > (uint64_t)SIZE_MAX ||
        record->size > (uint64_t)limits->max_file_bytes) {
        return EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
    }
    *size = (size_t)record->size;
    file_fd = evo_candidate_open_relative_file(baseline_owner->snapshot_path, record->path);
    if (file_fd < 0 || fstat(file_fd, &metadata) != 0 ||
        !S_ISREG(metadata.st_mode) || metadata.st_size < 0 ||
        (uintmax_t)metadata.st_size != (uintmax_t)record->size) {
        if (file_fd >= 0) {
            (void)close(file_fd);
        }
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    if (*size == SIZE_MAX) {
        (void)close(file_fd);
        return EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
    }
    *bytes = evo_project_allocate_zeroed(*size + 1U, sizeof(**bytes));
    if (*bytes == NULL) {
        (void)close(file_fd);
        return EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
    }
    while (position < *size) {
        const ssize_t count = read(file_fd, *bytes + position, *size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            evo_project_release(*bytes);
            *bytes = NULL;
            (void)close(file_fd);
            return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
        }
        position += (size_t)count;
    }
    if (close(file_fd) != 0) {
        evo_project_release(*bytes);
        *bytes = NULL;
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    (*bytes)[*size] = '\0';
    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(&fingerprint, *bytes, *size);
    if (fingerprint.value != record->content_fingerprint) {
        evo_project_release(*bytes);
        *bytes = NULL;
        return EVO_PROJECT_CANDIDATE_ERROR_BASELINE_CHANGED;
    }
    return EVO_PROJECT_CANDIDATE_SUCCESS;
}

evo_project_candidate_status_t evo_candidate_write_all(
    int file_fd,
    const unsigned char *bytes,
    size_t size)
{
    size_t position = 0U;

    while (position < size) {
        const ssize_t count = write(file_fd, bytes + position, size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
        }
        position += (size_t)count;
    }
    return EVO_PROJECT_CANDIDATE_SUCCESS;
}

evo_project_candidate_status_t evo_candidate_open_output_file(
    int workspace_fd,
    const char *path,
    mode_t mode,
    int *file_fd)
{
    const size_t path_size = strlen(path);
    char *component = NULL;
    size_t position = 0U;
    int directory_fd = -1;
    evo_project_candidate_status_t status = EVO_PROJECT_CANDIDATE_SUCCESS;

    component = evo_project_allocate_zeroed(path_size + 1U, sizeof(*component));
    if (component == NULL) {
        return EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
    }
    directory_fd = dup(workspace_fd);
    if (directory_fd < 0) {
        evo_project_release(component);
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    while (position < path_size) {
        size_t component_size = 0U;
        bool final_component;

        while (position < path_size && path[position] != '/') {
            component[component_size++] = path[position++];
        }
        component[component_size] = '\0';
        final_component = position == path_size;
        if (final_component) {
            *file_fd = openat(
                directory_fd,
                component,
                O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                mode);
            if (*file_fd < 0) {
                status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
            }
        } else {
            int next_fd;
            struct stat metadata;

            if (mkdirat(directory_fd, component, 0700) != 0 && errno != EEXIST) {
                status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
                break;
            }
            if (fstatat(directory_fd, component, &metadata, AT_SYMLINK_NOFOLLOW) != 0 ||
                !S_ISDIR(metadata.st_mode)) {
                status = EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID;
                break;
            }
            next_fd = openat(
                directory_fd,
                component,
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
                0);
            if (next_fd < 0) {
                status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
                break;
            }
            (void)close(directory_fd);
            directory_fd = next_fd;
            position += 1U;
        }
    }
    (void)close(directory_fd);
    evo_project_release(component);
    return status;
}

static evo_project_candidate_status_t evo_candidate_remove_directory_contents(int directory_fd)
{
    DIR *directory;
    struct dirent *entry;
    int iteration_fd;

    if (fchmod(directory_fd, (mode_t)0700) != 0) {
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    iteration_fd = dup(directory_fd);
    if (iteration_fd < 0) {
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    directory = fdopendir(iteration_fd);
    if (directory == NULL) {
        (void)close(iteration_fd);
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        struct stat metadata;
        evo_project_candidate_status_t status;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            errno = 0;
            continue;
        }
        if (fstatat(directory_fd, entry->d_name, &metadata, AT_SYMLINK_NOFOLLOW) != 0) {
            (void)closedir(directory);
            return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
        }
        if (S_ISDIR(metadata.st_mode)) {
            const int child_fd = openat(
                directory_fd,
                entry->d_name,
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
                0);

            if (child_fd < 0) {
                (void)closedir(directory);
                return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
            }
            status = evo_candidate_remove_directory_contents(child_fd);
            (void)close(child_fd);
            if (status == EVO_PROJECT_CANDIDATE_SUCCESS &&
                unlinkat(directory_fd, entry->d_name, AT_REMOVEDIR) != 0) {
                status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
            }
        } else {
            status = unlinkat(directory_fd, entry->d_name, 0) == 0
                         ? EVO_PROJECT_CANDIDATE_SUCCESS
                         : EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
        }
        if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
            (void)closedir(directory);
            return status;
        }
        errno = 0;
    }
    if (errno != 0 || closedir(directory) != 0) {
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    return EVO_PROJECT_CANDIDATE_SUCCESS;
}

evo_project_candidate_status_t evo_candidate_remove_tree(const char *path)
{
    int directory_fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    evo_project_candidate_status_t status;

    if (directory_fd < 0) {
        return errno == ENOENT ? EVO_PROJECT_CANDIDATE_SUCCESS
                               : EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    status = evo_candidate_remove_directory_contents(directory_fd);
    if (close(directory_fd) != 0 && status == EVO_PROJECT_CANDIDATE_SUCCESS) {
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    if (status == EVO_PROJECT_CANDIDATE_SUCCESS && rmdir(path) != 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    return status;
}
