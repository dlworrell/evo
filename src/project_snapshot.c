#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "internal/project_snapshot.h"

#include "internal/project_compilation_database.h"
#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct evo_project_collection {
    evo_project_file_record_t *files;
    size_t count;
    size_t capacity;
    uint64_t total_bytes;
    const evo_project_manifest_budget_t *budget;
} evo_project_collection_t;

static evo_project_status_t evo_project_write_all(
    int file_fd,
    const unsigned char *bytes,
    size_t byte_count);

static char *evo_project_duplicate_text(const char *value)
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

static char *evo_project_join_path(const char *left, const char *right)
{
    const size_t left_size = strlen(left);
    const size_t right_size = strlen(right);
    const bool needs_separator = left_size > 0U && left[left_size - 1U] != '/';
    size_t size;
    char *joined;
    size_t index;
    size_t position = 0U;

    if (left_size > SIZE_MAX - right_size ||
        left_size + right_size > SIZE_MAX - 2U) {
        return NULL;
    }
    size = left_size + right_size + (needs_separator ? 1U : 0U);
    joined = evo_project_allocate_zeroed(size + 1U, sizeof(*joined));
    if (joined == NULL) {
        return NULL;
    }
    for (index = 0U; index < left_size; index += 1U) {
        joined[position] = left[index];
        position += 1U;
    }
    if (needs_separator) {
        joined[position] = '/';
        position += 1U;
    }
    for (index = 0U; index < right_size; index += 1U) {
        joined[position] = right[index];
        position += 1U;
    }
    joined[position] = '\0';
    return joined;
}

static bool evo_project_path_is_within(
    const char *parent,
    const char *candidate)
{
    const size_t parent_size = strlen(parent);

    if (parent_size == 1U && parent[0] == '/') {
        return candidate[0] == '/';
    }
    if (strcmp(parent, candidate) == 0) {
        return true;
    }
    return strncmp(parent, candidate, parent_size) == 0 &&
           candidate[parent_size] == '/';
}

static bool evo_project_output_name_valid(const char *name)
{
    size_t index;

    if (name == NULL || name[0] == '\0' || strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) {
        return false;
    }
    for (index = 0U; name[index] != '\0'; index += 1U) {
        const unsigned char value = (unsigned char)name[index];

        if (value < 0x20U || value == 0x7fU || name[index] == '/' ||
            name[index] == '\\') {
            return false;
        }
    }
    return true;
}

static evo_project_status_t evo_project_prepare_output_paths(
    const evo_project_capture_config_t *config,
    evo_project_baseline_owner_t *owner)
{
    const char *last_slash;
    const char *base_name;
    char *parent_input = NULL;
    char *parent_real = NULL;
    char *output_real = NULL;
    char *stage = NULL;
    char *snapshot = NULL;
    char *workspace = NULL;
    size_t parent_size;
    size_t index;
    int output_fd = -1;
    int marker_fd = -1;
    const char marker[] = "incomplete\n";

    if (config->output_path[0] != '/') {
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    last_slash = strrchr(config->output_path, '/');
    if (last_slash == NULL || last_slash[1] == '\0') {
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    base_name = last_slash + 1;
    if (!evo_project_output_name_valid(base_name)) {
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    parent_size = (size_t)(last_slash - config->output_path);
    if (parent_size == 0U) {
        parent_input = evo_project_duplicate_text("/");
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
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    parent_real = realpath(parent_input, NULL);
    evo_project_release(parent_input);
    if (parent_real == NULL) {
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    if (evo_project_path_is_within(owner->authorized_root, parent_real)) {
        evo_project_release(parent_real);
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    output_real = evo_project_join_path(parent_real, base_name);
    evo_project_release(parent_real);
    if (output_real == NULL) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    if (strlen(output_real) > config->limits.max_path_bytes) {
        evo_project_release(output_real);
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    if (mkdir(output_real, 0700) != 0) {
        const int saved_errno = errno;

        evo_project_release(output_real);
        return saved_errno == EEXIST ? EVO_PROJECT_ERROR_OUTPUT_EXISTS
                                     : EVO_PROJECT_ERROR_SOURCE_IO;
    }
    owner->output_reserved = true;
    owner->output_path = output_real;
    output_fd = open(output_real, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (output_fd < 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    marker_fd = openat(
        output_fd,
        ".evo-incomplete-v1",
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    if (marker_fd < 0 ||
        evo_project_write_all(
            marker_fd,
            (const unsigned char *)marker,
            sizeof(marker) - 1U) != EVO_PROJECT_SUCCESS ||
        fsync(marker_fd) != 0) {
        if (marker_fd >= 0) {
            (void)close(marker_fd);
        }
        (void)close(output_fd);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    if (close(marker_fd) != 0) {
        marker_fd = -1;
        (void)close(output_fd);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    marker_fd = -1;
    if (mkdirat(output_fd, ".evo-stage-v1", 0700) != 0) {
        (void)close(output_fd);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    if (fsync(output_fd) != 0) {
        (void)close(output_fd);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    if (close(output_fd) != 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    output_fd = -1;
    stage = evo_project_join_path(output_real, ".evo-stage-v1");
    if (stage != NULL) {
        snapshot = evo_project_join_path(stage, "snapshot");
    }
    if (stage != NULL) {
        workspace = evo_project_join_path(stage, "workspace");
    }
    if (stage == NULL || snapshot == NULL || workspace == NULL) {
        evo_project_release(stage);
        evo_project_release(snapshot);
        evo_project_release(workspace);
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    owner->stage_path = stage;
    owner->snapshot_path = snapshot;
    owner->workspace_path = workspace;
    return EVO_PROJECT_SUCCESS;
}

static int evo_project_open_relative(
    int root_fd,
    const char *path,
    int final_flags,
    mode_t create_mode)
{
    const size_t path_size = strlen(path);
    char *component;
    size_t position = 0U;
    int directory_fd;

    if (path_size == SIZE_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    component = evo_project_allocate_zeroed(path_size + 1U, sizeof(*component));
    if (component == NULL) {
        errno = ENOMEM;
        return -1;
    }
    directory_fd = dup(root_fd);
    if (directory_fd < 0) {
        evo_project_release(component);
        return -1;
    }
    while (position < path_size) {
        size_t component_size = 0U;
        bool final_component;
        int next_fd;

        while (position < path_size && path[position] != '/') {
            component[component_size] = path[position];
            component_size += 1U;
            position += 1U;
        }
        component[component_size] = '\0';
        final_component = position == path_size;
        if (final_component) {
            next_fd = openat(
                directory_fd,
                component,
                final_flags | O_NOFOLLOW | O_CLOEXEC,
                create_mode);
        } else {
            next_fd = openat(
                directory_fd,
                component,
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
                0);
        }
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

static char *evo_project_relative_child(
    const char *prefix,
    const char *name,
    size_t maximum_bytes)
{
    const size_t prefix_size = strlen(prefix);
    const size_t name_size = strlen(name);
    const bool separator = prefix_size > 0U;
    size_t size;
    char *path;
    size_t index;
    size_t position = 0U;

    if (prefix_size > SIZE_MAX - name_size ||
        prefix_size + name_size > SIZE_MAX - 2U) {
        return NULL;
    }
    size = prefix_size + name_size + (separator ? 1U : 0U);
    if (size > maximum_bytes) {
        return NULL;
    }
    path = evo_project_allocate_zeroed(size + 1U, sizeof(*path));
    if (path == NULL) {
        return NULL;
    }
    for (index = 0U; index < prefix_size; index += 1U) {
        path[position] = prefix[index];
        position += 1U;
    }
    if (separator) {
        path[position] = '/';
        position += 1U;
    }
    for (index = 0U; index < name_size; index += 1U) {
        path[position] = name[index];
        position += 1U;
    }
    path[position] = '\0';
    return path;
}

static evo_project_status_t evo_project_collection_add(
    evo_project_collection_t *collection,
    const char *path,
    const struct stat *metadata)
{
    uint64_t size;
    char *path_copy;

    if (metadata->st_size < 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    size = (uint64_t)metadata->st_size;
    if (collection->count >= collection->capacity ||
        size > (uint64_t)collection->budget->max_file_bytes ||
        collection->total_bytes >
            (uint64_t)collection->budget->max_total_bytes - size) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    path_copy = evo_project_duplicate_text(path);
    if (path_copy == NULL) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    collection->files[collection->count].path = path_copy;
    collection->files[collection->count].size = size;
    collection->files[collection->count].source_mode =
        (unsigned int)(metadata->st_mode & (mode_t)07777);
    collection->files[collection->count].content_fingerprint = 0U;
    collection->count += 1U;
    collection->total_bytes += size;
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_collect_directory(
    int directory_fd,
    const char *relative_prefix,
    evo_project_collection_t *collection)
{
    DIR *directory;
    struct dirent *entry;
    int iteration_fd = dup(directory_fd);

    if (iteration_fd < 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    directory = fdopendir(iteration_fd);
    if (directory == NULL) {
        (void)close(iteration_fd);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        struct stat metadata;
        char *relative_path;
        evo_project_status_t status;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            errno = 0;
            continue;
        }
        relative_path = evo_project_relative_child(
            relative_prefix,
            entry->d_name,
            collection->budget->max_path_bytes);
        if (relative_path == NULL) {
            (void)closedir(directory);
            return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
        }
        if (fstatat(
                directory_fd,
                entry->d_name,
                &metadata,
                AT_SYMLINK_NOFOLLOW) != 0) {
            evo_project_release(relative_path);
            (void)closedir(directory);
            return EVO_PROJECT_ERROR_SOURCE_IO;
        }
        if (S_ISREG(metadata.st_mode)) {
            status = evo_project_collection_add(
                collection, relative_path, &metadata);
        } else if (S_ISDIR(metadata.st_mode)) {
            const int child_fd = openat(
                directory_fd,
                entry->d_name,
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
                0);

            if (child_fd < 0) {
                evo_project_release(relative_path);
                (void)closedir(directory);
                return EVO_PROJECT_ERROR_SOURCE_IO;
            }
            status = evo_project_collect_directory(
                child_fd, relative_path, collection);
            (void)close(child_fd);
        } else {
            status = EVO_PROJECT_ERROR_PATH_INVALID;
        }
        evo_project_release(relative_path);
        if (status != EVO_PROJECT_SUCCESS) {
            (void)closedir(directory);
            return status;
        }
        errno = 0;
    }
    if (errno != 0 || closedir(directory) != 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_collect_root(
    int source_root_fd,
    const char *relative_root,
    evo_project_collection_t *collection)
{
    int root_fd;
    struct stat metadata;
    evo_project_status_t status;

    root_fd = evo_project_open_relative(
        source_root_fd,
        relative_root,
        O_RDONLY | O_NONBLOCK,
        0);
    if (root_fd < 0 || fstat(root_fd, &metadata) != 0) {
        if (root_fd >= 0) {
            (void)close(root_fd);
        }
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    if (S_ISREG(metadata.st_mode)) {
        status = evo_project_collection_add(
            collection, relative_root, &metadata);
    } else if (S_ISDIR(metadata.st_mode)) {
        status = evo_project_collect_directory(
            root_fd, relative_root, collection);
    } else {
        status = EVO_PROJECT_ERROR_PATH_INVALID;
    }
    (void)close(root_fd);
    return status;
}

static int evo_project_file_record_compare(
    const void *left_value,
    const void *right_value)
{
    const evo_project_file_record_t *left = left_value;
    const evo_project_file_record_t *right = right_value;

    return strcmp(left->path, right->path);
}

static void evo_project_release_files(
    evo_project_file_record_t *files,
    size_t count)
{
    size_t index;

    if (files == NULL) {
        return;
    }
    for (index = 0U; index < count; index += 1U) {
        evo_project_release((void *)files[index].path);
    }
    evo_project_release(files);
}

static evo_project_status_t evo_project_collect_source(
    const char *authorized_root,
    const evo_project_manifest_t *manifest,
    evo_project_file_record_t **files,
    size_t *file_count,
    uint64_t *total_bytes)
{
    evo_project_collection_t collection;
    int source_fd;
    size_t root;

    if (manifest->budget.max_files > SIZE_MAX / sizeof(*collection.files)) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    collection.files = evo_project_allocate_zeroed(
        manifest->budget.max_files, sizeof(*collection.files));
    if (collection.files == NULL) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    collection.count = 0U;
    collection.capacity = manifest->budget.max_files;
    collection.total_bytes = 0U;
    collection.budget = &manifest->budget;

    source_fd = open(
        authorized_root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (source_fd < 0) {
        evo_project_release(collection.files);
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    for (root = 0U; root < manifest->permitted_root_count; root += 1U) {
        const evo_project_status_t status = evo_project_collect_root(
            source_fd, manifest->permitted_roots[root], &collection);

        if (status != EVO_PROJECT_SUCCESS) {
            (void)close(source_fd);
            evo_project_release_files(collection.files, collection.count);
            return status;
        }
    }
    if (close(source_fd) != 0) {
        evo_project_release_files(collection.files, collection.count);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    if (collection.count == 0U) {
        evo_project_release_files(collection.files, collection.count);
        return EVO_PROJECT_ERROR_MANIFEST_INVALID;
    }
    qsort(
        collection.files,
        collection.count,
        sizeof(*collection.files),
        evo_project_file_record_compare);
    for (root = 1U; root < collection.count; root += 1U) {
        if (strcmp(
                collection.files[root - 1U].path,
                collection.files[root].path) == 0) {
            evo_project_release_files(collection.files, collection.count);
            return EVO_PROJECT_ERROR_MANIFEST_INVALID;
        }
    }
    *files = collection.files;
    *file_count = collection.count;
    *total_bytes = collection.total_bytes;
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_create_parent_directories(
    int root_fd,
    const char *path)
{
    const size_t path_size = strlen(path);
    char *component;
    size_t position = 0U;
    int directory_fd;

    if (path_size == SIZE_MAX) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    component = evo_project_allocate_zeroed(path_size + 1U, sizeof(*component));
    if (component == NULL) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    directory_fd = dup(root_fd);
    if (directory_fd < 0) {
        evo_project_release(component);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    while (position < path_size) {
        size_t component_size = 0U;
        int child_fd;

        while (position < path_size && path[position] != '/') {
            component[component_size] = path[position];
            component_size += 1U;
            position += 1U;
        }
        component[component_size] = '\0';
        if (position == path_size) {
            break;
        }
        if (mkdirat(directory_fd, component, 0700) != 0 && errno != EEXIST) {
            (void)close(directory_fd);
            evo_project_release(component);
            return EVO_PROJECT_ERROR_SOURCE_IO;
        }
        child_fd = openat(
            directory_fd,
            component,
            O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
            0);
        (void)close(directory_fd);
        if (child_fd < 0) {
            evo_project_release(component);
            return EVO_PROJECT_ERROR_PATH_INVALID;
        }
        directory_fd = child_fd;
        position += 1U;
    }
    (void)close(directory_fd);
    evo_project_release(component);
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_write_all(
    int file_fd,
    const unsigned char *bytes,
    size_t byte_count)
{
    size_t written_total = 0U;

    while (written_total < byte_count) {
        const ssize_t written = write(
            file_fd, bytes + written_total, byte_count - written_total);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return EVO_PROJECT_ERROR_SOURCE_IO;
        }
        written_total += (size_t)written;
    }
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_copy_file(
    int source_root_fd,
    int snapshot_root_fd,
    int workspace_root_fd,
    evo_project_file_record_t *record)
{
    unsigned char buffer[8192];
    evo_project_fingerprint_t fingerprint;
    struct stat metadata;
    int source_fd = -1;
    int snapshot_fd = -1;
    int workspace_fd = -1;
    uint64_t copied = 0U;
    evo_project_status_t status;
    mode_t snapshot_mode;
    mode_t workspace_mode;

    status = evo_project_create_parent_directories(
        snapshot_root_fd, record->path);
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_create_parent_directories(
            workspace_root_fd, record->path);
    }
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    source_fd = evo_project_open_relative(
        source_root_fd, record->path, O_RDONLY | O_NONBLOCK, 0);
    snapshot_fd = evo_project_open_relative(
        snapshot_root_fd,
        record->path,
        O_WRONLY | O_CREAT | O_EXCL,
        0600);
    workspace_fd = evo_project_open_relative(
        workspace_root_fd,
        record->path,
        O_WRONLY | O_CREAT | O_EXCL,
        0600);
    if (source_fd < 0 || snapshot_fd < 0 || workspace_fd < 0 ||
        fstat(source_fd, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        metadata.st_size < 0 || (uint64_t)metadata.st_size != record->size ||
        (unsigned int)(metadata.st_mode & (mode_t)07777) !=
            record->source_mode) {
        status = EVO_PROJECT_ERROR_SOURCE_CHANGED;
        goto close_files;
    }
    evo_project_fingerprint_begin(&fingerprint);
    for (;;) {
        ssize_t read_count = read(source_fd, buffer, sizeof(buffer));

        if (read_count < 0 && errno == EINTR) {
            continue;
        }
        if (read_count < 0) {
            status = EVO_PROJECT_ERROR_SOURCE_IO;
            goto close_files;
        }
        if (read_count == 0) {
            break;
        }
        if (copied > UINT64_MAX - (uint64_t)read_count ||
            copied + (uint64_t)read_count > record->size) {
            status = EVO_PROJECT_ERROR_SOURCE_CHANGED;
            goto close_files;
        }
        evo_project_fingerprint_bytes(
            &fingerprint, buffer, (size_t)read_count);
        status = evo_project_write_all(
            snapshot_fd, buffer, (size_t)read_count);
        if (status == EVO_PROJECT_SUCCESS) {
            status = evo_project_write_all(
                workspace_fd, buffer, (size_t)read_count);
        }
        if (status != EVO_PROJECT_SUCCESS) {
            goto close_files;
        }
        copied += (uint64_t)read_count;
    }
    if (copied != record->size) {
        status = EVO_PROJECT_ERROR_SOURCE_CHANGED;
        goto close_files;
    }
    snapshot_mode = (mode_t)(record->source_mode & 0555U);
    snapshot_mode |= (mode_t)0400;
    workspace_mode = (mode_t)(record->source_mode & 0777U);
    workspace_mode |= (mode_t)0600;
    if (fchmod(snapshot_fd, snapshot_mode) != 0 ||
        fchmod(workspace_fd, workspace_mode) != 0 ||
        fsync(snapshot_fd) != 0 || fsync(workspace_fd) != 0) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
        goto close_files;
    }
    record->content_fingerprint = fingerprint.value;
    status = EVO_PROJECT_SUCCESS;

close_files:
    if (source_fd >= 0 && close(source_fd) != 0 &&
        status == EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
    }
    if (snapshot_fd >= 0 && close(snapshot_fd) != 0 &&
        status == EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
    }
    if (workspace_fd >= 0 && close(workspace_fd) != 0 &&
        status == EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
    }
    return status;
}

static evo_project_status_t evo_project_make_tree_readonly(int directory_fd)
{
    DIR *directory;
    struct dirent *entry;
    int iteration_fd = dup(directory_fd);

    if (iteration_fd < 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    directory = fdopendir(iteration_fd);
    if (directory == NULL) {
        (void)close(iteration_fd);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        struct stat metadata;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            errno = 0;
            continue;
        }
        if (fstatat(
                directory_fd,
                entry->d_name,
                &metadata,
                AT_SYMLINK_NOFOLLOW) != 0) {
            (void)closedir(directory);
            return EVO_PROJECT_ERROR_SOURCE_IO;
        }
        if (S_ISDIR(metadata.st_mode)) {
            const int child_fd = openat(
                directory_fd,
                entry->d_name,
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
                0);
            evo_project_status_t status;

            if (child_fd < 0) {
                (void)closedir(directory);
                return EVO_PROJECT_ERROR_SOURCE_IO;
            }
            status = evo_project_make_tree_readonly(child_fd);
            (void)close(child_fd);
            if (status != EVO_PROJECT_SUCCESS) {
                (void)closedir(directory);
                return status;
            }
        } else if (!S_ISREG(metadata.st_mode)) {
            (void)closedir(directory);
            return EVO_PROJECT_ERROR_PATH_INVALID;
        }
        errno = 0;
    }
    if (errno != 0 || closedir(directory) != 0 ||
        fchmod(directory_fd, (mode_t)0500) != 0 || fsync(directory_fd) != 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    return EVO_PROJECT_SUCCESS;
}

static uint64_t evo_project_compute_baseline_fingerprint(
    const evo_project_baseline_owner_t *owner)
{
    evo_project_fingerprint_t fingerprint;
    size_t index;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, "catalyst.evo-project-baseline.v1");
    evo_project_fingerprint_u64(&fingerprint, owner->manifest.fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, owner->execution_provider_identity);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)owner->file_count);
    for (index = 0U; index < owner->file_count; index += 1U) {
        evo_project_fingerprint_string(&fingerprint, owner->files[index].path);
        evo_project_fingerprint_u64(&fingerprint, owner->files[index].size);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)owner->files[index].source_mode);
        evo_project_fingerprint_u64(
            &fingerprint, owner->files[index].content_fingerprint);
    }
    evo_project_fingerprint_u64(
        &fingerprint, owner->normalized_build_fingerprint);
    return fingerprint.value;
}

static evo_project_status_t evo_project_validate_compilation_database(
    const evo_project_baseline_owner_t *owner)
{
    size_t index;

    for (index = 0U; index < owner->file_count; index += 1U) {
        if (strcmp(
                owner->files[index].path,
                owner->manifest.compilation_database) == 0) {
            return owner->files[index].size <=
                           (uint64_t)owner->manifest.budget
                               .max_compilation_database_bytes
                       ? EVO_PROJECT_SUCCESS
                       : EVO_PROJECT_ERROR_RESOURCE_LIMIT;
        }
    }
    return EVO_PROJECT_ERROR_MANIFEST_INVALID;
}

evo_project_status_t evo_project_snapshot_prepare(
    const evo_project_capture_config_t *config,
    evo_project_baseline_owner_t *owner)
{
    evo_project_status_t status;
    int source_fd = -1;
    int stage_fd = -1;
    int snapshot_fd = -1;
    int workspace_fd = -1;
    size_t index;

    owner->authorized_root = realpath(config->authorized_project_root, NULL);
    if (owner->authorized_root == NULL) {
        return EVO_PROJECT_ERROR_PATH_INVALID;
    }
    if (strlen(owner->authorized_root) > config->limits.max_path_bytes) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    owner->execution_provider_identity =
        evo_project_duplicate_text(config->execution_provider_identity);
    if (owner->execution_provider_identity == NULL) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    status = evo_project_prepare_output_paths(config, owner);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    status = evo_project_collect_source(
        owner->authorized_root,
        &owner->manifest,
        &owner->files,
        &owner->file_count,
        &owner->total_file_bytes);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    status = evo_project_validate_compilation_database(owner);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    stage_fd = open(
        owner->stage_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (stage_fd < 0 || mkdirat(stage_fd, "snapshot", 0700) != 0 ||
        mkdirat(stage_fd, "workspace", 0700) != 0) {
        if (stage_fd >= 0) {
            (void)close(stage_fd);
        }
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    snapshot_fd = openat(
        stage_fd,
        "snapshot",
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
        0);
    workspace_fd = openat(
        stage_fd,
        "workspace",
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
        0);
    source_fd = open(
        owner->authorized_root,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (snapshot_fd < 0 || workspace_fd < 0 || source_fd < 0) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
        goto close_directories;
    }
    for (index = 0U; index < owner->file_count; index += 1U) {
        status = evo_project_copy_file(
            source_fd, snapshot_fd, workspace_fd, &owner->files[index]);
        if (status != EVO_PROJECT_SUCCESS) {
            goto close_directories;
        }
    }
    status = evo_project_compilation_database_load(
        owner->snapshot_path,
        owner->manifest.compilation_database,
        owner->authorized_root,
        &config->limits,
        owner->files,
        owner->file_count,
        &owner->compilation_units,
        &owner->compilation_unit_count,
        &owner->normalized_build_fingerprint);
    if (status != EVO_PROJECT_SUCCESS) {
        goto close_directories;
    }
    status = evo_project_make_tree_readonly(snapshot_fd);
    if (status != EVO_PROJECT_SUCCESS) {
        goto close_directories;
    }
    if (fsync(workspace_fd) != 0 || fsync(stage_fd) != 0) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
        goto close_directories;
    }
    owner->baseline_fingerprint =
        evo_project_compute_baseline_fingerprint(owner);
    status = EVO_PROJECT_SUCCESS;

close_directories:
    if (source_fd >= 0) {
        (void)close(source_fd);
    }
    if (snapshot_fd >= 0) {
        (void)close(snapshot_fd);
    }
    if (workspace_fd >= 0) {
        (void)close(workspace_fd);
    }
    if (stage_fd >= 0) {
        (void)close(stage_fd);
    }
    return status;
}

static evo_project_status_t evo_project_compare_file_bytes(
    int source_root_fd,
    int snapshot_root_fd,
    const evo_project_file_record_t *record)
{
    unsigned char source_buffer[8192];
    unsigned char snapshot_buffer[8192];
    evo_project_fingerprint_t fingerprint;
    int source_fd = evo_project_open_relative(
        source_root_fd, record->path, O_RDONLY | O_NONBLOCK, 0);
    int snapshot_fd = evo_project_open_relative(
        snapshot_root_fd, record->path, O_RDONLY | O_NONBLOCK, 0);
    evo_project_status_t status = EVO_PROJECT_SUCCESS;

    if (source_fd < 0 || snapshot_fd < 0) {
        status = EVO_PROJECT_ERROR_SOURCE_CHANGED;
        goto close_files;
    }
    evo_project_fingerprint_begin(&fingerprint);
    for (;;) {
        ssize_t source_count;
        ssize_t snapshot_count;
        size_t index;

        do {
            source_count = read(source_fd, source_buffer, sizeof(source_buffer));
        } while (source_count < 0 && errno == EINTR);
        do {
            snapshot_count =
                read(snapshot_fd, snapshot_buffer, sizeof(snapshot_buffer));
        } while (snapshot_count < 0 && errno == EINTR);
        if (source_count < 0 || snapshot_count < 0) {
            status = EVO_PROJECT_ERROR_SOURCE_IO;
            break;
        }
        if (source_count != snapshot_count) {
            status = EVO_PROJECT_ERROR_SOURCE_CHANGED;
            break;
        }
        if (source_count == 0) {
            break;
        }
        for (index = 0U; index < (size_t)source_count; index += 1U) {
            if (source_buffer[index] != snapshot_buffer[index]) {
                status = EVO_PROJECT_ERROR_SOURCE_CHANGED;
                break;
            }
        }
        if (status != EVO_PROJECT_SUCCESS) {
            break;
        }
        evo_project_fingerprint_bytes(
            &fingerprint, source_buffer, (size_t)source_count);
    }
    if (status == EVO_PROJECT_SUCCESS &&
        fingerprint.value != record->content_fingerprint) {
        status = EVO_PROJECT_ERROR_SOURCE_CHANGED;
    }

close_files:
    if (source_fd >= 0) {
        (void)close(source_fd);
    }
    if (snapshot_fd >= 0) {
        (void)close(snapshot_fd);
    }
    return status;
}

evo_project_status_t evo_project_snapshot_verify_source(
    const evo_project_baseline_owner_t *owner)
{
    evo_project_file_record_t *current_files = NULL;
    size_t current_count = 0U;
    uint64_t current_bytes = 0U;
    evo_project_status_t status;
    int source_fd = -1;
    int snapshot_fd = -1;
    size_t index;

    status = evo_project_collect_source(
        owner->authorized_root,
        &owner->manifest,
        &current_files,
        &current_count,
        &current_bytes);
    if (status != EVO_PROJECT_SUCCESS) {
        return status == EVO_PROJECT_ERROR_OUT_OF_MEMORY ||
                       status == EVO_PROJECT_ERROR_RESOURCE_LIMIT
                   ? status
                   : EVO_PROJECT_ERROR_SOURCE_CHANGED;
    }
    if (current_count != owner->file_count ||
        current_bytes != owner->total_file_bytes) {
        evo_project_release_files(current_files, current_count);
        return EVO_PROJECT_ERROR_SOURCE_CHANGED;
    }
    for (index = 0U; index < current_count; index += 1U) {
        if (strcmp(current_files[index].path, owner->files[index].path) != 0 ||
            current_files[index].size != owner->files[index].size ||
            current_files[index].source_mode !=
                owner->files[index].source_mode) {
            evo_project_release_files(current_files, current_count);
            return EVO_PROJECT_ERROR_SOURCE_CHANGED;
        }
    }
    evo_project_release_files(current_files, current_count);
    source_fd = open(
        owner->authorized_root,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    snapshot_fd = open(
        owner->snapshot_path,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (source_fd < 0 || snapshot_fd < 0) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
        goto close_directories;
    }
    for (index = 0U; index < owner->file_count; index += 1U) {
        status = evo_project_compare_file_bytes(
            source_fd, snapshot_fd, &owner->files[index]);
        if (status != EVO_PROJECT_SUCCESS) {
            goto close_directories;
        }
    }
    status = EVO_PROJECT_SUCCESS;

close_directories:
    if (source_fd >= 0) {
        (void)close(source_fd);
    }
    if (snapshot_fd >= 0) {
        (void)close(snapshot_fd);
    }
    return status;
}

static evo_project_status_t evo_project_verify_snapshot_file(
    int snapshot_root_fd,
    const evo_project_file_record_t *record)
{
    unsigned char buffer[8192];
    evo_project_fingerprint_t fingerprint;
    struct stat metadata;
    int file_fd = evo_project_open_relative(
        snapshot_root_fd, record->path, O_RDONLY | O_NONBLOCK, 0);
    evo_project_status_t status = EVO_PROJECT_SUCCESS;
    uint64_t byte_count = 0U;

    if (file_fd < 0 || fstat(file_fd, &metadata) != 0 ||
        !S_ISREG(metadata.st_mode) || metadata.st_size < 0 ||
        (uint64_t)metadata.st_size != record->size ||
        (unsigned int)(metadata.st_mode & (mode_t)07777) !=
            (record->source_mode & 0555U)) {
        if (file_fd >= 0) {
            (void)close(file_fd);
        }
        return EVO_PROJECT_ERROR_SOURCE_CHANGED;
    }
    evo_project_fingerprint_begin(&fingerprint);
    for (;;) {
        ssize_t read_count;

        do {
            read_count = read(file_fd, buffer, sizeof(buffer));
        } while (read_count < 0 && errno == EINTR);
        if (read_count < 0) {
            status = EVO_PROJECT_ERROR_SOURCE_IO;
            break;
        }
        if (read_count == 0) {
            break;
        }
        if (byte_count > UINT64_MAX - (uint64_t)read_count) {
            status = EVO_PROJECT_ERROR_RESOURCE_LIMIT;
            break;
        }
        byte_count += (uint64_t)read_count;
        evo_project_fingerprint_bytes(
            &fingerprint, buffer, (size_t)read_count);
    }
    if (status == EVO_PROJECT_SUCCESS &&
        (byte_count != record->size ||
         fingerprint.value != record->content_fingerprint)) {
        status = EVO_PROJECT_ERROR_SOURCE_CHANGED;
    }
    if (close(file_fd) != 0 && status == EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
    }
    return status;
}

static bool evo_project_snapshot_directory_expected(
    const evo_project_baseline_owner_t *owner,
    const char *relative_path)
{
    const size_t path_size = strlen(relative_path);
    size_t index;

    if (path_size == 0U) {
        return true;
    }
    for (index = 0U; index < owner->file_count; index += 1U) {
        if (strncmp(owner->files[index].path, relative_path, path_size) == 0 &&
            owner->files[index].path[path_size] == '/') {
            return true;
        }
    }
    return false;
}

static evo_project_status_t evo_project_verify_snapshot_directories(
    const evo_project_baseline_owner_t *owner,
    int directory_fd,
    const char *relative_path)
{
    struct stat directory_metadata;
    DIR *directory;
    struct dirent *entry;
    int iteration_fd;

    if (fstat(directory_fd, &directory_metadata) != 0 ||
        !S_ISDIR(directory_metadata.st_mode) ||
        (unsigned int)(directory_metadata.st_mode & (mode_t)07777) != 0500U) {
        return EVO_PROJECT_ERROR_SOURCE_CHANGED;
    }
    iteration_fd = dup(directory_fd);
    if (iteration_fd < 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    directory = fdopendir(iteration_fd);
    if (directory == NULL) {
        (void)close(iteration_fd);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        struct stat metadata;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            errno = 0;
            continue;
        }
        if (fstatat(
                directory_fd,
                entry->d_name,
                &metadata,
                AT_SYMLINK_NOFOLLOW) != 0) {
            (void)closedir(directory);
            return EVO_PROJECT_ERROR_SOURCE_CHANGED;
        }
        if (S_ISDIR(metadata.st_mode)) {
            const size_t prefix_size = strlen(relative_path);
            const size_t name_size = strlen(entry->d_name);
            char *child_path;
            int child_fd;
            evo_project_status_t status;

            if (prefix_size > SIZE_MAX - name_size ||
                prefix_size + name_size > SIZE_MAX - 2U ||
                prefix_size + name_size + (prefix_size > 0U ? 1U : 0U) >
                    owner->manifest.budget.max_path_bytes) {
                (void)closedir(directory);
                return EVO_PROJECT_ERROR_SOURCE_CHANGED;
            }
            child_path = evo_project_relative_child(
                relative_path,
                entry->d_name,
                owner->manifest.budget.max_path_bytes);
            if (child_path == NULL) {
                (void)closedir(directory);
                return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
            }
            if (!evo_project_snapshot_directory_expected(owner, child_path)) {
                evo_project_release(child_path);
                (void)closedir(directory);
                return EVO_PROJECT_ERROR_SOURCE_CHANGED;
            }
            child_fd = openat(
                directory_fd,
                entry->d_name,
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
                0);
            if (child_fd < 0) {
                evo_project_release(child_path);
                (void)closedir(directory);
                return EVO_PROJECT_ERROR_SOURCE_CHANGED;
            }
            status = evo_project_verify_snapshot_directories(
                owner, child_fd, child_path);
            evo_project_release(child_path);
            if (close(child_fd) != 0 && status == EVO_PROJECT_SUCCESS) {
                status = EVO_PROJECT_ERROR_SOURCE_IO;
            }
            if (status != EVO_PROJECT_SUCCESS) {
                (void)closedir(directory);
                return status;
            }
        } else if (!S_ISREG(metadata.st_mode)) {
            (void)closedir(directory);
            return EVO_PROJECT_ERROR_SOURCE_CHANGED;
        }
        errno = 0;
    }
    if (errno != 0 || closedir(directory) != 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    return EVO_PROJECT_SUCCESS;
}

evo_project_status_t evo_project_snapshot_verify_baseline(
    const evo_project_baseline_owner_t *owner)
{
    evo_project_file_record_t *current_files = NULL;
    size_t current_count = 0U;
    uint64_t current_bytes = 0U;
    evo_project_status_t status;
    int snapshot_fd = -1;
    size_t index;

    if (owner == NULL || owner->snapshot_path == NULL || !owner->committed) {
        return EVO_PROJECT_ERROR_INVALID_ARGUMENT;
    }
    status = evo_project_collect_source(
        owner->snapshot_path,
        &owner->manifest,
        &current_files,
        &current_count,
        &current_bytes);
    if (status != EVO_PROJECT_SUCCESS) {
        return status == EVO_PROJECT_ERROR_OUT_OF_MEMORY ||
                       status == EVO_PROJECT_ERROR_RESOURCE_LIMIT
                   ? status
                   : EVO_PROJECT_ERROR_SOURCE_CHANGED;
    }
    if (current_count != owner->file_count ||
        current_bytes != owner->total_file_bytes) {
        evo_project_release_files(current_files, current_count);
        return EVO_PROJECT_ERROR_SOURCE_CHANGED;
    }
    for (index = 0U; index < current_count; index += 1U) {
        if (strcmp(current_files[index].path, owner->files[index].path) != 0 ||
            current_files[index].size != owner->files[index].size ||
            current_files[index].source_mode !=
                (owner->files[index].source_mode & 0555U)) {
            evo_project_release_files(current_files, current_count);
            return EVO_PROJECT_ERROR_SOURCE_CHANGED;
        }
    }
    evo_project_release_files(current_files, current_count);
    snapshot_fd = open(
        owner->snapshot_path,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (snapshot_fd < 0) {
        return EVO_PROJECT_ERROR_SOURCE_CHANGED;
    }
    status = evo_project_verify_snapshot_directories(owner, snapshot_fd, "");
    if (status == EVO_PROJECT_SUCCESS) {
        for (index = 0U; index < owner->file_count; index += 1U) {
            status = evo_project_verify_snapshot_file(
                snapshot_fd, &owner->files[index]);
            if (status != EVO_PROJECT_SUCCESS) {
                break;
            }
        }
    }
    if (close(snapshot_fd) != 0 && status == EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
    }
    return status;
}

static evo_project_status_t evo_project_remove_directory_contents(int directory_fd)
{
    DIR *directory;
    struct dirent *entry;
    int iteration_fd;

    if (fchmod(directory_fd, (mode_t)0700) != 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    iteration_fd = dup(directory_fd);
    if (iteration_fd < 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    directory = fdopendir(iteration_fd);
    if (directory == NULL) {
        (void)close(iteration_fd);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        struct stat metadata;
        evo_project_status_t status;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            errno = 0;
            continue;
        }
        if (fstatat(
                directory_fd,
                entry->d_name,
                &metadata,
                AT_SYMLINK_NOFOLLOW) != 0) {
            (void)closedir(directory);
            return EVO_PROJECT_ERROR_SOURCE_IO;
        }
        if (S_ISDIR(metadata.st_mode)) {
            const int child_fd = openat(
                directory_fd,
                entry->d_name,
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
                0);

            if (child_fd < 0) {
                (void)closedir(directory);
                return EVO_PROJECT_ERROR_SOURCE_IO;
            }
            status = evo_project_remove_directory_contents(child_fd);
            (void)close(child_fd);
            if (status == EVO_PROJECT_SUCCESS &&
                unlinkat(directory_fd, entry->d_name, AT_REMOVEDIR) != 0) {
                status = EVO_PROJECT_ERROR_SOURCE_IO;
            }
        } else {
            status = unlinkat(directory_fd, entry->d_name, 0) == 0
                         ? EVO_PROJECT_SUCCESS
                         : EVO_PROJECT_ERROR_SOURCE_IO;
        }
        if (status != EVO_PROJECT_SUCCESS) {
            (void)closedir(directory);
            return status;
        }
        errno = 0;
    }
    if (errno != 0 || closedir(directory) != 0) {
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_remove_tree(const char *path)
{
    int directory_fd = open(
        path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    evo_project_status_t status;

    if (directory_fd < 0) {
        return errno == ENOENT ? EVO_PROJECT_SUCCESS
                               : EVO_PROJECT_ERROR_SOURCE_IO;
    }
    if (fchmod(directory_fd, (mode_t)0700) != 0) {
        (void)close(directory_fd);
        return EVO_PROJECT_ERROR_SOURCE_IO;
    }
    status = evo_project_remove_directory_contents(directory_fd);
    if (close(directory_fd) != 0 && status == EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
    }
    if (status == EVO_PROJECT_SUCCESS && rmdir(path) != 0) {
        status = EVO_PROJECT_ERROR_SOURCE_IO;
    }
    return status;
}

evo_project_status_t evo_project_snapshot_remove_workspace(
    evo_project_baseline_owner_t *owner)
{
    evo_project_status_t status;

    if (owner == NULL || owner->workspace_path == NULL) {
        return EVO_PROJECT_ERROR_INVALID_ARGUMENT;
    }
    status = evo_project_remove_tree(owner->workspace_path);
    if (status == EVO_PROJECT_SUCCESS) {
        evo_project_release(owner->workspace_path);
        owner->workspace_path = NULL;
    }
    return status;
}

evo_project_status_t evo_project_snapshot_commit(
    evo_project_baseline_owner_t *owner)
{
    char *final_snapshot;
    char *stage_json;
    char *final_json;
    char *stage_markdown;
    char *final_markdown;
    int output_fd = -1;
    int snapshot_fd = -1;
    evo_project_status_t status = EVO_PROJECT_SUCCESS;

    if (owner == NULL || !owner->output_reserved || owner->committed ||
        owner->workspace_path != NULL) {
        return EVO_PROJECT_ERROR_STATE;
    }
    final_snapshot = evo_project_join_path(owner->output_path, "snapshot");
    stage_json = evo_project_join_path(owner->stage_path, "baseline.json");
    final_json = evo_project_join_path(owner->output_path, "baseline.json");
    stage_markdown = evo_project_join_path(owner->stage_path, "baseline.md");
    final_markdown = evo_project_join_path(owner->output_path, "baseline.md");
    if (final_snapshot == NULL || stage_json == NULL || final_json == NULL ||
        stage_markdown == NULL || final_markdown == NULL) {
        status = EVO_PROJECT_ERROR_OUT_OF_MEMORY;
        goto release_paths;
    }
    snapshot_fd = open(
        owner->snapshot_path,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (snapshot_fd < 0 || fchmod(snapshot_fd, (mode_t)0700) != 0) {
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
        goto release_paths;
    }
    /*
     * Moving a directory to a new parent requires owner-write permission on
     * the directory so that its parent entry can change. No consumer callback
     * remains reachable here, the incomplete marker is still present, and the
     * root is re-hardened before any evidence is published.
     */
    if (rename(owner->snapshot_path, final_snapshot) != 0) {
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
        goto release_paths;
    }
    if (fchmod(snapshot_fd, (mode_t)0500) != 0 || fsync(snapshot_fd) != 0) {
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
        goto release_paths;
    }
    if (close(snapshot_fd) != 0) {
        snapshot_fd = -1;
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
        goto release_paths;
    }
    snapshot_fd = -1;
    if (rename(stage_json, final_json) != 0 ||
        rename(stage_markdown, final_markdown) != 0 ||
        rmdir(owner->stage_path) != 0) {
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
        goto release_paths;
    }
    output_fd = open(
        owner->output_path,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (output_fd < 0) {
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
        goto release_paths;
    }
    if (fsync(output_fd) != 0 ||
        unlinkat(output_fd, ".evo-incomplete-v1", 0) != 0 ||
        fsync(output_fd) != 0 || fchmod(output_fd, (mode_t)0500) != 0) {
        (void)close(output_fd);
        output_fd = -1;
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
        goto release_paths;
    }
    if (close(output_fd) != 0) {
        output_fd = -1;
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
        goto release_paths;
    }
    output_fd = -1;
    evo_project_release(owner->snapshot_path);
    owner->snapshot_path = final_snapshot;
    final_snapshot = NULL;
    owner->committed = true;

release_paths:
    if (snapshot_fd >= 0) {
        (void)fchmod(snapshot_fd, (mode_t)0500);
        (void)close(snapshot_fd);
    }
    if (output_fd >= 0) {
        (void)close(output_fd);
    }
    evo_project_release(final_snapshot);
    evo_project_release(stage_json);
    evo_project_release(final_json);
    evo_project_release(stage_markdown);
    evo_project_release(final_markdown);
    return status;
}

void evo_project_snapshot_discard(evo_project_baseline_owner_t *owner)
{
    if (owner == NULL) {
        return;
    }
    if (owner->output_reserved && !owner->committed &&
        owner->output_path != NULL) {
        (void)evo_project_remove_tree(owner->output_path);
    }
    evo_project_release_files(owner->files, owner->file_count);
    owner->files = NULL;
    owner->file_count = 0U;
    owner->total_file_bytes = 0U;
    evo_project_compilation_database_destroy(
        owner->compilation_units, owner->compilation_unit_count);
    owner->compilation_units = NULL;
    owner->compilation_unit_count = 0U;
    owner->normalized_build_fingerprint = 0U;
    evo_project_release(owner->authorized_root);
    owner->authorized_root = NULL;
    evo_project_release(owner->output_path);
    owner->output_path = NULL;
    evo_project_release(owner->stage_path);
    owner->stage_path = NULL;
    evo_project_release(owner->snapshot_path);
    owner->snapshot_path = NULL;
    evo_project_release(owner->workspace_path);
    owner->workspace_path = NULL;
    evo_project_release(owner->execution_provider_identity);
    owner->execution_provider_identity = NULL;
}
