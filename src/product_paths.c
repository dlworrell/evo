#define _GNU_SOURCE

#include "internal/product_cli.h"
#include "internal/project_runtime.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#endif

static bool evo_product_component(const char *start, size_t length)
{
    size_t index;

    if (length == 0U || length >= 256U ||
        (length == 2U && start[0] == '.' && start[1] == '.')) {
        return false;
    }
    for (index = 0U; index < length; index += 1U) {
        const unsigned char byte = (unsigned char)start[index];
        if (byte < 0x20U || byte == 0x7fU) {
            return false;
        }
    }
    return true;
}

static int evo_product_walk(int initial, const char *path, bool directory)
{
    int current = dup(initial);
    const char *cursor = path;

    if (current < 0) {
        return -1;
    }
    while (*cursor != '\0') {
        char component[256];
        const char *end;
        size_t length;
        int next;
        bool last;

        while (*cursor == '/') {
            cursor += 1;
        }
        if (*cursor == '\0') {
            break;
        }
        end = strchr(cursor, '/');
        length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        if (!evo_product_component(cursor, length)) {
            (void)close(current);
            errno = EINVAL;
            return -1;
        }
        for (size_t index = 0U; index < length; index += 1U) {
            component[index] = cursor[index];
        }
        component[length] = '\0';
        last = end == NULL || end[1] == '\0';
        if (strcmp(component, ".") == 0) {
            cursor += length;
            continue;
        }
        next = openat(current, component,
                      O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK |
                          ((!last || directory) ? O_DIRECTORY : 0));
        (void)close(current);
        if (next < 0) {
            return -1;
        }
        current = next;
        cursor += length;
    }
    struct stat metadata;
    if (fstat(current, &metadata) != 0 ||
        (directory ? !S_ISDIR(metadata.st_mode) : !S_ISREG(metadata.st_mode))) {
        (void)close(current);
        errno = EINVAL;
        return -1;
    }
    return current;
}

int evo_product_open_relative(int directory_fd, const char *path, bool directory)
{
    if (path == NULL || path[0] == '\0' || path[0] == '/') {
        errno = EINVAL;
        return -1;
    }
    return evo_product_walk(directory_fd, path, directory);
}

int evo_product_open_path(const char *argument, bool directory,
                          char absolute[EVO_PRODUCT_PATH_BYTES])
{
    char joined[EVO_PRODUCT_PATH_BYTES];
    char cwd[EVO_PRODUCT_PATH_BYTES];
    const char *cursor;
    size_t used = 0U;
    int descriptor;
    int root;
    int written;

    if (argument == NULL || argument[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    if (argument[0] == '/') {
        written = evo_project_format(joined, sizeof(joined), "%s", argument);
    } else {
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            return -1;
        }
        written = evo_project_format(joined, sizeof(joined), "%s/%s", cwd, argument);
    }
    if (written < 0 || (size_t)written >= sizeof(joined)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    root = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (root < 0) {
        return -1;
    }
    descriptor = evo_product_walk(root, joined, directory);
    (void)close(root);
    if (descriptor < 0) {
        return -1;
    }
    /* Normalize only separators and dot components, after no-follow walking. */
    cursor = joined;
    while (*cursor != '\0') {
        const char *end;
        size_t length;
        while (*cursor == '/') {
            cursor += 1;
        }
        if (*cursor == '\0') {
            break;
        }
        end = strchr(cursor, '/');
        length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        if (!(length == 1U && cursor[0] == '.')) {
            absolute[used++] = '/';
            for (size_t index = 0U; index < length; index += 1U) {
                absolute[used++] = cursor[index];
            }
        }
        cursor += length;
    }
    if (used == 0U) {
        absolute[used++] = '/';
    }
    absolute[used] = '\0';
    return descriptor;
}

bool evo_product_read(int descriptor, size_t maximum, char **bytes, size_t *size)
{
    struct stat before;
    struct stat after;
    size_t position = 0U;
    char *value;

    *bytes = NULL;
    *size = 0U;
    if (fstat(descriptor, &before) != 0 || !S_ISREG(before.st_mode) ||
        before.st_size < 0 || (uintmax_t)before.st_size > (uintmax_t)maximum ||
        maximum == SIZE_MAX) {
        errno = EFBIG;
        return false;
    }
    *size = (size_t)before.st_size;
    value = evo_project_allocate_zeroed(*size + 1U, sizeof(*value));
    if (value == NULL) {
        errno = ENOMEM;
        return false;
    }
    while (position < *size) {
        const ssize_t count = read(descriptor, value + position, *size - position);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            evo_project_release(value);
            return false;
        }
        position += (size_t)count;
    }
    char extra;
    if (read(descriptor, &extra, 1U) != 0 || fstat(descriptor, &after) != 0 ||
        before.st_dev != after.st_dev || before.st_ino != after.st_ino ||
        before.st_size != after.st_size || before.st_mtime != after.st_mtime ||
        before.st_ctime != after.st_ctime
#if defined(__APPLE__)
        || before.st_mtimespec.tv_nsec != after.st_mtimespec.tv_nsec ||
        before.st_ctimespec.tv_nsec != after.st_ctimespec.tv_nsec
#else
        || before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        before.st_ctim.tv_nsec != after.st_ctim.tv_nsec
#endif
    ) {
        evo_project_release(value);
        errno = EIO;
        return false;
    }
    *bytes = value;
    return true;
}

bool evo_product_write(int directory_fd, const char *name, const char *bytes,
                       size_t size)
{
    size_t position = 0U;
    const int descriptor = openat(directory_fd, name,
                                  O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                      O_NOFOLLOW,
                                  0600);
    bool success = descriptor >= 0;

    while (success && position < size) {
        const ssize_t count = write(descriptor, bytes + position, size - position);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            success = false;
        } else {
            position += (size_t)count;
        }
    }
    if (descriptor >= 0) {
        success = fsync(descriptor) == 0 && success;
        success = close(descriptor) == 0 && success;
    }
    return success;
}

bool evo_product_path_contains(const char *root, const char *path)
{
    const size_t length = strlen(root);
    return strncmp(root, path, length) == 0 &&
           (length == 1U || path[length] == '/' || path[length] == '\0');
}

bool evo_product_stage_prepare(const char *output, evo_product_stage_t *stage)
{
    char argument[EVO_PRODUCT_PATH_BYTES];
    char parent[EVO_PRODUCT_PATH_BYTES];
    struct stat metadata;
    char *slash;
    int written;

    *stage = (evo_product_stage_t){.parent_fd = -1, .directory_fd = -1};
    written = evo_project_format(argument, sizeof(argument), "%s", output);
    if (written <= 0 || (size_t)written >= sizeof(argument)) {
        errno = ENAMETOOLONG;
        return false;
    }
    slash = strrchr(argument, '/');
    const char *name = slash == NULL ? argument : slash + 1;
    if (!evo_product_component(name, strlen(name)) || strcmp(name, ".") == 0) {
        errno = EINVAL;
        return false;
    }
    written = evo_project_format(stage->output_name, sizeof(stage->output_name), "%s", name);
    if (written <= 0 || (size_t)written >= sizeof(stage->output_name)) {
        return false;
    }
    if (slash == NULL) {
        written = evo_project_format(parent, sizeof(parent), ".");
    } else {
        *slash = '\0';
        written = evo_project_format(parent, sizeof(parent), "%s", slash == argument ? "/" : argument);
    }
    if (written <= 0 || (size_t)written >= sizeof(parent)) {
        return false;
    }
    stage->parent_fd = evo_product_open_path(parent, true, stage->parent_path);
    if (stage->parent_fd < 0) {
        return false;
    }
    if (fstat(stage->parent_fd, &metadata) != 0 || metadata.st_uid != geteuid() ||
        (metadata.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        errno = EACCES;
        return false;
    }
    if (fstatat(stage->parent_fd, stage->output_name, &metadata, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT) {
        errno = EEXIST;
        return false;
    }
    written = evo_project_format(stage->output_path, sizeof(stage->output_path), "%s/%s", stage->parent_path, stage->output_name);
    if (written <= 0 || (size_t)written >= sizeof(stage->output_path)) {
        errno = ENAMETOOLONG;
        return false;
    }
    return true;
}

bool evo_product_stage_begin(evo_product_stage_t *stage)
{
    struct stat metadata;
    const char *name;
    int written;

    written = evo_project_format(stage->stage_path, sizeof(stage->stage_path), "%s/.evo-run-XXXXXX", stage->parent_path);
    if (written <= 0 || (size_t)written >= sizeof(stage->stage_path) || mkdtemp(stage->stage_path) == NULL) {
        return false;
    }
    stage->created = true;
    name = strrchr(stage->stage_path, '/');
    if (name == NULL || evo_project_format(stage->stage_name, sizeof(stage->stage_name), "%s", name + 1) <= 0) {
        return false;
    }
    if (fstatat(stage->parent_fd, stage->stage_name, &metadata, AT_SYMLINK_NOFOLLOW) != 0) {
        return false;
    }
    stage->device = metadata.st_dev;
    stage->inode = metadata.st_ino;
    stage->directory_fd = openat(stage->parent_fd, stage->stage_name,
                                 O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (stage->directory_fd < 0 || fstat(stage->directory_fd, &metadata) != 0 ||
        metadata.st_dev != stage->device || metadata.st_ino != stage->inode) {
        return false;
    }
    return true;
}

static bool evo_product_remove_children(int directory_fd, size_t depth, size_t *remaining)
{
    DIR *directory;
    struct dirent *entry;
    int copy;
    bool success = true;

    if (depth > 128U || *remaining == 0U) {
        return false;
    }
    copy = dup(directory_fd);
    directory = copy < 0 ? NULL : fdopendir(copy);
    if (directory == NULL) {
        if (copy >= 0) {
            (void)close(copy);
        }
        return false;
    }
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        struct stat metadata;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (*remaining == 0U || fstatat(directory_fd, entry->d_name, &metadata, AT_SYMLINK_NOFOLLOW) != 0) {
            success = false;
            break;
        }
        *remaining -= 1U;
        if (S_ISDIR(metadata.st_mode)) {
            const int child = openat(directory_fd, entry->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (child < 0 || fchmod(child, 0700) != 0) {
                if (child >= 0) {
                    (void)close(child);
                }
                success = false;
                break;
            }
            const bool removed = evo_product_remove_children(child, depth + 1U, remaining);
            (void)close(child);
            if (!removed || unlinkat(directory_fd, entry->d_name, AT_REMOVEDIR) != 0) {
                success = false;
                break;
            }
        } else if (unlinkat(directory_fd, entry->d_name, 0) != 0) {
            success = false;
            break;
        }
        errno = 0;
    }
    if (errno != 0) {
        success = false;
    }
    return closedir(directory) == 0 && success;
}

bool evo_product_stage_destroy(evo_product_stage_t *stage)
{
    bool success = true;
    struct stat metadata;
    size_t remaining = 1000000U;

    if (!stage->published && stage->created) {
        success = fstatat(stage->parent_fd, stage->stage_name, &metadata, AT_SYMLINK_NOFOLLOW) == 0 &&
                  metadata.st_dev == stage->device && metadata.st_ino == stage->inode &&
                  (stage->directory_fd < 0 || evo_product_remove_children(stage->directory_fd, 0U, &remaining)) &&
                  unlinkat(stage->parent_fd, stage->stage_name, AT_REMOVEDIR) == 0;
    }
    if (stage->directory_fd >= 0) {
        success = close(stage->directory_fd) == 0 && success;
    }
    if (stage->parent_fd >= 0) {
        success = close(stage->parent_fd) == 0 && success;
    }
    stage->directory_fd = -1;
    stage->parent_fd = -1;
    return success;
}

bool evo_product_stage_publish(evo_product_stage_t *stage)
{
    if (fsync(stage->directory_fd) != 0) {
        return false;
    }
#if defined(__linux__) && defined(SYS_renameat2)
    /* RENAME_NOREPLACE: an output created concurrently can never be replaced. */
    if (syscall(SYS_renameat2, stage->parent_fd, stage->stage_name,
                stage->parent_fd, stage->output_name, 1U) != 0) {
        return false;
    }
    stage->published = true;
    return fsync(stage->parent_fd) == 0;
#else
    errno = ENOTSUP;
    return false;
#endif
}
