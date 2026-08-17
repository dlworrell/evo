#define _POSIX_C_SOURCE 200809L

#include "internal/project_provider_sandbox.h"

#include "internal/project_fingerprint.h"
#include "internal/project_provider.h"
#include "internal/project_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(__linux__)
#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

typedef struct evo_project_sandbox_owner {
    char *stdout_text;
    char *stderr_text;
} evo_project_sandbox_owner_t;

static bool evo_sandbox_text_valid(const char *text, bool allow_empty)
{
    size_t index = 0U;

    if (text == NULL) {
        return false;
    }
    while (index < 4096U && text[index] != '\0') {
        const unsigned char value = (unsigned char)text[index];

        if (value < 0x20U || value == 0x7fU) {
            return false;
        }
        index += 1U;
    }
    return index < 4096U && (allow_empty || index > 0U);
}

static bool evo_sandbox_limits_valid(const evo_project_sandbox_limits_t *limits)
{
    return limits != NULL && limits->cpu_time_ms > 0U &&
           limits->address_space_bytes > 0U &&
           limits->descendant_process_count > 0U && limits->storage_bytes > 0U &&
           limits->output_bytes > 0U && limits->wall_timeout_ms > 0U;
}

static bool evo_sandbox_command_valid(const evo_project_sandbox_command_t *command)
{
    size_t index;

    if (command == NULL ||
        command->schema_version != EVO_PROJECT_SANDBOX_SCHEMA_VERSION ||
        !evo_sandbox_text_valid(command->workspace_path, false) ||
        (command->working_directory != NULL &&
         !evo_sandbox_text_valid(command->working_directory, false)) ||
        command->argument_count == 0U || command->arguments == NULL ||
        !evo_sandbox_limits_valid(&command->limits) ||
        command->environment_count > 128U ||
        (command->environment_count > 0U && command->environment == NULL)) {
        return false;
    }
    for (index = 0U; index < command->argument_count; index += 1U) {
        if (!evo_sandbox_text_valid(command->arguments[index], false)) {
            return false;
        }
    }
    for (index = 0U; index < command->environment_count; index += 1U) {
        const char *entry = command->environment[index];
        const char *equal;

        if (!evo_sandbox_text_valid(entry, false)) {
            return false;
        }
        equal = strchr(entry, '=');
        if (equal == NULL || equal == entry || equal[1] == '\0') {
            return false;
        }
    }
    return true;
}

#if defined(__linux__)

static bool evo_sandbox_program_available(const char *program)
{
    const char *path = getenv("PATH");
    const char *cursor;

    if (program == NULL || path == NULL || path[0] == '\0') {
        return false;
    }
    cursor = path;
    while (true) {
        const char *end = strchr(cursor, ':');
        const size_t directory_length =
            end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        const size_t program_length = strlen(program);
        char candidate[4096];
        size_t position = 0U;

        if (directory_length == 0U) {
            candidate[position++] = '.';
        } else if (directory_length < sizeof(candidate)) {
            (void)memcpy(candidate, cursor, directory_length);
            position = directory_length;
        }
        if (position > 0U && position + 1U < sizeof(candidate)) {
            candidate[position++] = '/';
        }
        if (position > 0U && program_length < sizeof(candidate) - position) {
            (void)memcpy(candidate + position, program, program_length);
            position += program_length;
            candidate[position] = '\0';
            if (access(candidate, X_OK) == 0) {
                return true;
            }
        }
        if (end == NULL) {
            break;
        }
        cursor = end + 1;
    }
    return false;
}

static bool evo_sandbox_path_within(const char *root, const char *path)
{
    const size_t root_length = strlen(root);

    if (strncmp(root, path, root_length) != 0) {
        return false;
    }
    return path[root_length] == '\0' || path[root_length] == '/';
}

static bool evo_sandbox_resolve_paths(
    const evo_project_sandbox_command_t *command,
    char workspace[PATH_MAX],
    char working_directory[PATH_MAX])
{
    char requested[PATH_MAX];
    const char *working = command->working_directory;
    int written;

    if (realpath(command->workspace_path, workspace) == NULL ||
        workspace[0] != '/') {
        return false;
    }
    if (working == NULL || working[0] == '\0') {
        written = evo_project_format(
            requested, sizeof(requested), "%s", workspace);
    } else if (working[0] == '/') {
        written = evo_project_format(
            requested, sizeof(requested), "%s", working);
    } else {
        written = evo_project_format(
            requested, sizeof(requested), "%s/%s", workspace, working);
    }
    if (written <= 0 || (size_t)written >= sizeof(requested) ||
        realpath(requested, working_directory) == NULL ||
        !evo_sandbox_path_within(workspace, working_directory)) {
        return false;
    }
    return true;
}

static bool evo_sandbox_set_nonblocking(int descriptor)
{
    const int flags = fcntl(descriptor, F_GETFL, 0);

    return flags >= 0 && fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0;
}

static bool evo_sandbox_clock_ns(uint64_t *value)
{
    struct timespec now;
    uint64_t seconds;
    uint64_t nanoseconds;

    if (value == NULL || clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
        now.tv_sec < 0 || now.tv_nsec < 0) {
        return false;
    }
    seconds = (uint64_t)now.tv_sec;
    nanoseconds = (uint64_t)now.tv_nsec;
    if (seconds > UINT64_MAX / UINT64_C(1000000000)) {
        return false;
    }
    seconds *= UINT64_C(1000000000);
    if (nanoseconds > UINT64_MAX - seconds) {
        return false;
    }
    *value = seconds + nanoseconds;
    return true;
}

static bool evo_sandbox_sum_storage(
    const char *path,
    uint64_t limit,
    uint64_t *total)
{
    DIR *directory;
    struct dirent *entry;

    directory = opendir(path);
    if (directory == NULL) {
        return false;
    }
    while ((entry = readdir(directory)) != NULL) {
        char child[PATH_MAX];
        struct stat metadata;
        int written;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        written = evo_project_format(
            child, sizeof(child), "%s/%s", path, entry->d_name);
        if (written <= 0 || (size_t)written >= sizeof(child) ||
            lstat(child, &metadata) != 0) {
            (void)closedir(directory);
            return false;
        }
        if (S_ISLNK(metadata.st_mode)) {
            continue;
        }
        if (S_ISREG(metadata.st_mode)) {
            const uint64_t bytes = metadata.st_size < 0
                                       ? UINT64_MAX
                                       : (uint64_t)metadata.st_size;

            if (bytes > limit || *total > limit - bytes) {
                *total = limit + 1U;
                (void)closedir(directory);
                return true;
            }
            *total += bytes;
        } else if (S_ISDIR(metadata.st_mode)) {
            if (!evo_sandbox_sum_storage(child, limit, total)) {
                (void)closedir(directory);
                return false;
            }
            if (*total > limit) {
                (void)closedir(directory);
                return true;
            }
        }
    }
    return closedir(directory) == 0;
}

static bool evo_sandbox_storage_within(const char *workspace, uint64_t limit)
{
    uint64_t total = 0U;

    return evo_sandbox_sum_storage(workspace, limit, &total) && total <= limit;
}

static bool evo_sandbox_read_children(pid_t process, size_t limit, size_t *count)
{
    char path[128];
    FILE *stream;
    long child;
    int written;

    if (*count > limit) {
        return true;
    }
    written = evo_project_format(
        path,
        sizeof(path),
        "/proc/%ld/task/%ld/children",
        (long)process,
        (long)process);
    if (written <= 0 || (size_t)written >= sizeof(path)) {
        return false;
    }
    stream = fopen(path, "r");
    if (stream == NULL) {
        return errno == ENOENT;
    }
    while (fscanf(stream, "%ld", &child) == 1) {
        if (child <= 0) {
            (void)fclose(stream);
            return false;
        }
        if (*count == SIZE_MAX) {
            (void)fclose(stream);
            return false;
        }
        *count += 1U;
        if (*count > limit) {
            (void)fclose(stream);
            return true;
        }
        if (!evo_sandbox_read_children((pid_t)child, limit, count)) {
            (void)fclose(stream);
            return false;
        }
    }
    return fclose(stream) == 0;
}

static bool evo_sandbox_processes_within(pid_t process, size_t limit)
{
    size_t count = 0U;

    return evo_sandbox_read_children(process, limit, &count) && count <= limit;
}

static char *evo_sandbox_environment_name(const char *entry)
{
    const char *equal = strchr(entry, '=');
    const size_t length = equal == NULL ? 0U : (size_t)(equal - entry);
    char *name;

    if (length == 0U || length == SIZE_MAX) {
        return NULL;
    }
    name = evo_project_allocate_zeroed(length + 1U, sizeof(*name));
    if (name == NULL) {
        return NULL;
    }
    (void)memcpy(name, entry, length);
    name[length] = '\0';
    return name;
}

static void evo_sandbox_environment_names_destroy(
    char **names,
    size_t count)
{
    size_t index;

    if (names == NULL) {
        return;
    }
    for (index = 0U; index < count; index += 1U) {
        evo_project_release(names[index]);
    }
    evo_project_release(names);
}

static char **evo_sandbox_build_argv(
    const evo_project_sandbox_command_t *command,
    const char *workspace,
    const char *working_directory,
    char ***environment_names_out,
    size_t *argument_count_out)
{
    static const char *const roots[] = {
        "/usr", "/bin", "/sbin", "/lib", "/lib64", "/etc"};
    size_t root_count = 0U;
    size_t index;
    size_t capacity;
    size_t position = 0U;
    char **arguments;
    char **environment_names = NULL;

    if (command->argument_count > SIZE_MAX - 64U ||
        command->environment_count > (SIZE_MAX - command->argument_count - 64U) / 3U) {
        return NULL;
    }
    capacity = command->argument_count + 64U + command->environment_count * 3U;
    arguments = evo_project_allocate_zeroed(capacity, sizeof(*arguments));
    if (arguments == NULL) {
        return NULL;
    }
    if (command->environment_count > 0U) {
        environment_names = evo_project_allocate_zeroed(
            command->environment_count, sizeof(*environment_names));
        if (environment_names == NULL) {
            evo_project_release(arguments);
            return NULL;
        }
    }
    arguments[position++] = (char *)"bwrap";
    arguments[position++] = (char *)"--die-with-parent";
    arguments[position++] = (char *)"--new-session";
    arguments[position++] = (char *)"--unshare-user";
    arguments[position++] = (char *)"--unshare-pid";
    arguments[position++] = (char *)"--unshare-uts";
    arguments[position++] = (char *)"--unshare-ipc";
    if (!command->limits.network_access) {
        arguments[position++] = (char *)"--unshare-net";
    }
    arguments[position++] = (char *)"--proc";
    arguments[position++] = (char *)"/proc";
    arguments[position++] = (char *)"--dev";
    arguments[position++] = (char *)"/dev";
    arguments[position++] = (char *)"--tmpfs";
    arguments[position++] = (char *)"/tmp";
    for (index = 0U; index < sizeof(roots) / sizeof(roots[0]); index += 1U) {
        if (access(roots[index], F_OK) == 0) {
            arguments[position++] = (char *)"--ro-bind";
            arguments[position++] = (char *)roots[index];
            arguments[position++] = (char *)roots[index];
            root_count += 1U;
        }
    }
    if (root_count == 0U) {
        evo_project_release(arguments);
        evo_sandbox_environment_names_destroy(
            environment_names, command->environment_count);
        return NULL;
    }
    arguments[position++] = (char *)"--bind";
    arguments[position++] = (char *)workspace;
    arguments[position++] = (char *)workspace;
    arguments[position++] = (char *)"--chdir";
    arguments[position++] = (char *)working_directory;
    arguments[position++] = (char *)"--clearenv";
    arguments[position++] = (char *)"--setenv";
    arguments[position++] = (char *)"PATH";
    arguments[position++] = (char *)"/usr/local/bin:/usr/bin:/bin";
    for (index = 0U; index < command->environment_count; index += 1U) {
        const char *equal = strchr(command->environment[index], '=');

        environment_names[index] =
            evo_sandbox_environment_name(command->environment[index]);
        if (environment_names[index] == NULL || equal == NULL) {
            evo_project_release(arguments);
            evo_sandbox_environment_names_destroy(
                environment_names, command->environment_count);
            return NULL;
        }
        arguments[position++] = (char *)"--setenv";
        arguments[position++] = environment_names[index];
        arguments[position++] = (char *)(equal + 1);
    }
    arguments[position++] = (char *)"--";
    for (index = 0U; index < command->argument_count; index += 1U) {
        arguments[position++] = (char *)command->arguments[index];
    }
    arguments[position] = NULL;
    *environment_names_out = environment_names;
    *argument_count_out = position;
    return arguments;
}

static bool evo_sandbox_set_child_limits(
    const evo_project_sandbox_limits_t *limits)
{
    struct rlimit cpu;
    struct rlimit address_space;
    uint64_t cpu_seconds = limits->cpu_time_ms / UINT64_C(1000);

    if (limits->cpu_time_ms % UINT64_C(1000) != 0U) {
        cpu_seconds += 1U;
    }
    if (cpu_seconds == 0U ||
        cpu_seconds > (uint64_t)((rlim_t)-1) ||
        limits->address_space_bytes > (uint64_t)((rlim_t)-1)) {
        return false;
    }
    cpu.rlim_cur = (rlim_t)cpu_seconds;
    cpu.rlim_max = (rlim_t)cpu_seconds;
    address_space.rlim_cur = (rlim_t)limits->address_space_bytes;
    address_space.rlim_max = (rlim_t)limits->address_space_bytes;
    return setrlimit(RLIMIT_CPU, &cpu) == 0 &&
           setrlimit(RLIMIT_AS, &address_space) == 0;
}

static bool evo_sandbox_append_output(
    int descriptor,
    char *buffer,
    size_t capacity,
    size_t *length,
    bool *open_stream,
    size_t *combined,
    size_t combined_limit)
{
    while (*open_stream) {
        ssize_t count;
        size_t available;

        if (*length >= capacity) {
            return false;
        }
        available = capacity - *length;
        count = read(descriptor, buffer + *length, available);
        if (count > 0) {
            const size_t bytes = (size_t)count;

            if (bytes > combined_limit || *combined > combined_limit - bytes) {
                return false;
            }
            *length += bytes;
            *combined += bytes;
            continue;
        }
        if (count == 0) {
            *open_stream = false;
            return true;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        return false;
    }
    return true;
}

static bool evo_sandbox_terminate(pid_t process)
{
    int status = 0;
    size_t attempts;
    struct timespec pause = {0, 10000000L};

    if (kill(-process, SIGTERM) != 0 && errno != ESRCH) {
        return false;
    }
    for (attempts = 0U; attempts < 20U; attempts += 1U) {
        const pid_t waited = waitpid(process, &status, WNOHANG);

        if (waited == process) {
            return true;
        }
        if (waited < 0 && errno == ECHILD) {
            return true;
        }
        if (waited < 0 && errno != EINTR) {
            return false;
        }
        (void)nanosleep(&pause, NULL);
    }
    if (kill(-process, SIGKILL) != 0 && errno != ESRCH) {
        return false;
    }
    while (waitpid(process, &status, 0) < 0) {
        if (errno != EINTR) {
            return errno == ECHILD;
        }
    }
    return true;
}

static void evo_sandbox_fingerprint_output(
    const char *bytes,
    size_t count,
    uint64_t *value)
{
    evo_project_fingerprint_t fingerprint;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(
        &fingerprint, (const unsigned char *)bytes, count);
    *value = fingerprint.value;
}

static evo_project_sandbox_status_t evo_sandbox_run_linux(
    const evo_project_sandbox_command_t *command,
    evo_project_sandbox_result_t *result)
{
    evo_project_sandbox_owner_t *owner = NULL;
    char workspace[PATH_MAX];
    char working_directory[PATH_MAX];
    char **sandbox_argv = NULL;
    char **environment_names = NULL;
    size_t sandbox_argc = 0U;
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    pid_t process = -1;
    uint64_t start_ns = 0U;
    uint64_t now_ns = 0U;
    size_t combined_output = 0U;
    bool stdout_open = true;
    bool stderr_open = true;
    bool child_reaped = false;
    bool forced_termination = false;
    int child_status = 0;
    evo_project_sandbox_resource_t exhausted = EVO_PROJECT_SANDBOX_RESOURCE_NONE;
    evo_project_sandbox_status_t return_status = EVO_PROJECT_SANDBOX_SUCCESS;

    if (!evo_sandbox_resolve_paths(command, workspace, working_directory) ||
        !evo_sandbox_storage_within(workspace, command->limits.storage_bytes)) {
        return EVO_PROJECT_SANDBOX_ERROR_RESOURCE_LIMIT;
    }
    sandbox_argv = evo_sandbox_build_argv(
        command,
        workspace,
        working_directory,
        &environment_names,
        &sandbox_argc);
    if (sandbox_argv == NULL || sandbox_argc == 0U) {
        return EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY;
    }
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL || command->limits.output_bytes == SIZE_MAX) {
        return_status = EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    owner->stdout_text = evo_project_allocate_zeroed(
        command->limits.output_bytes + 1U, sizeof(*owner->stdout_text));
    owner->stderr_text = evo_project_allocate_zeroed(
        command->limits.output_bytes + 1U, sizeof(*owner->stderr_text));
    if (owner->stdout_text == NULL || owner->stderr_text == NULL ||
        pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0 ||
        !evo_sandbox_clock_ns(&start_ns)) {
        return_status = EVO_PROJECT_SANDBOX_ERROR_PROCESS;
        goto cleanup;
    }
    process = fork();
    if (process < 0) {
        return_status = EVO_PROJECT_SANDBOX_ERROR_PROCESS;
        goto cleanup;
    }
    if (process == 0) {
        if (setsid() < 0 ||
            dup2(stdout_pipe[1], STDOUT_FILENO) < 0 ||
            dup2(stderr_pipe[1], STDERR_FILENO) < 0 ||
            !evo_sandbox_set_child_limits(&command->limits)) {
            _exit(126);
        }
        (void)close(stdout_pipe[0]);
        (void)close(stdout_pipe[1]);
        (void)close(stderr_pipe[0]);
        (void)close(stderr_pipe[1]);
        execvp(sandbox_argv[0], sandbox_argv);
        _exit(127);
    }
    (void)close(stdout_pipe[1]);
    stdout_pipe[1] = -1;
    (void)close(stderr_pipe[1]);
    stderr_pipe[1] = -1;
    if (!evo_sandbox_set_nonblocking(stdout_pipe[0]) ||
        !evo_sandbox_set_nonblocking(stderr_pipe[0])) {
        return_status = EVO_PROJECT_SANDBOX_ERROR_PROCESS;
        forced_termination = true;
        goto supervise_done;
    }

    while (!child_reaped || stdout_open || stderr_open) {
        struct pollfd descriptors[2];
        const pid_t waited = waitpid(process, &child_status, WNOHANG);

        if (waited == process) {
            child_reaped = true;
        } else if (waited < 0 && errno != EINTR) {
            return_status = EVO_PROJECT_SANDBOX_ERROR_PROCESS;
            forced_termination = true;
            break;
        }
        descriptors[0].fd = stdout_open ? stdout_pipe[0] : -1;
        descriptors[0].events = POLLIN | POLLHUP;
        descriptors[0].revents = 0;
        descriptors[1].fd = stderr_open ? stderr_pipe[0] : -1;
        descriptors[1].events = POLLIN | POLLHUP;
        descriptors[1].revents = 0;
        (void)poll(descriptors, 2U, child_reaped ? 0 : 10);
        if (!evo_sandbox_append_output(
                stdout_pipe[0],
                owner->stdout_text,
                command->limits.output_bytes,
                &result->stdout_bytes,
                &stdout_open,
                &combined_output,
                command->limits.output_bytes) ||
            !evo_sandbox_append_output(
                stderr_pipe[0],
                owner->stderr_text,
                command->limits.output_bytes,
                &result->stderr_bytes,
                &stderr_open,
                &combined_output,
                command->limits.output_bytes)) {
            exhausted = EVO_PROJECT_SANDBOX_RESOURCE_OUTPUT;
            forced_termination = true;
            break;
        }
        if (!child_reaped) {
            if (!evo_sandbox_clock_ns(&now_ns) || now_ns < start_ns) {
                return_status = EVO_PROJECT_SANDBOX_ERROR_PROCESS;
                forced_termination = true;
                break;
            }
            if ((now_ns - start_ns) / UINT64_C(1000000) >
                command->limits.wall_timeout_ms) {
                exhausted = EVO_PROJECT_SANDBOX_RESOURCE_WALL_TIME;
                forced_termination = true;
                break;
            }
            if (!evo_sandbox_processes_within(
                    process, command->limits.descendant_process_count)) {
                exhausted = EVO_PROJECT_SANDBOX_RESOURCE_PROCESS_COUNT;
                forced_termination = true;
                break;
            }
            if (!evo_sandbox_storage_within(
                    workspace, command->limits.storage_bytes)) {
                exhausted = EVO_PROJECT_SANDBOX_RESOURCE_STORAGE;
                forced_termination = true;
                break;
            }
        }
    }

supervise_done:
    if (forced_termination && process > 0 && !child_reaped) {
        if (!evo_sandbox_terminate(process)) {
            return_status = EVO_PROJECT_SANDBOX_ERROR_CLEANUP;
        }
        child_reaped = true;
    } else if (!child_reaped && process > 0) {
        while (waitpid(process, &child_status, 0) < 0) {
            if (errno != EINTR) {
                return_status = EVO_PROJECT_SANDBOX_ERROR_CLEANUP;
                break;
            }
        }
        child_reaped = true;
    }
    if (!evo_sandbox_clock_ns(&now_ns) || now_ns < start_ns) {
        return_status = EVO_PROJECT_SANDBOX_ERROR_PROCESS;
    } else {
        result->elapsed_ns = now_ns - start_ns;
    }
    owner->stdout_text[result->stdout_bytes] = '\0';
    owner->stderr_text[result->stderr_bytes] = '\0';
    evo_sandbox_fingerprint_output(
        owner->stdout_text, result->stdout_bytes, &result->stdout_fingerprint);
    evo_sandbox_fingerprint_output(
        owner->stderr_text, result->stderr_bytes, &result->stderr_fingerprint);
    result->stdout_text = owner->stdout_text;
    result->stderr_text = owner->stderr_text;
    result->resource_exhausted = exhausted != EVO_PROJECT_SANDBOX_RESOURCE_NONE;
    result->exhausted_resource = exhausted;
    result->timed_out = exhausted == EVO_PROJECT_SANDBOX_RESOURCE_WALL_TIME;
    if (!forced_termination && child_reaped) {
        if (WIFEXITED(child_status)) {
            result->completed = true;
            result->exit_code = WEXITSTATUS(child_status);
        } else if (WIFSIGNALED(child_status)) {
            const int signal_number = WTERMSIG(child_status);

            result->signaled = true;
            result->signal_number = signal_number;
            if (signal_number == SIGXCPU) {
                result->resource_exhausted = true;
                result->exhausted_resource = EVO_PROJECT_SANDBOX_RESOURCE_CPU;
            }
        }
    }
    result->schema_version = EVO_PROJECT_SANDBOX_SCHEMA_VERSION;
    result->cpu_limit_enforced = true;
    result->address_space_limit_enforced = true;
    result->process_limit_enforced = true;
    result->storage_limit_enforced = true;
    result->output_limit_enforced = true;
    result->timeout_enforced = true;
    result->filesystem_isolation_enforced = true;
    result->network_isolation_enforced = !command->limits.network_access;
    result->descendant_cleanup_enforced = child_reaped;
    result->private_owner = owner;
    owner = NULL;

cleanup:
    if (stdout_pipe[0] >= 0) {
        (void)close(stdout_pipe[0]);
    }
    if (stdout_pipe[1] >= 0) {
        (void)close(stdout_pipe[1]);
    }
    if (stderr_pipe[0] >= 0) {
        (void)close(stderr_pipe[0]);
    }
    if (stderr_pipe[1] >= 0) {
        (void)close(stderr_pipe[1]);
    }
    evo_sandbox_environment_names_destroy(
        environment_names, command->environment_count);
    evo_project_release(sandbox_argv);
    if (owner != NULL) {
        evo_project_release(owner->stdout_text);
        evo_project_release(owner->stderr_text);
        evo_project_release(owner);
    }
    return return_status;
}

#endif

bool evo_project_sandbox_available(void)
{
#if defined(__linux__)
    const evo_project_provider_record_t *provider =
        evo_project_provider_find(EVO_PROJECT_PROVIDER_LINUX_BWRAP_ID);

    return provider != NULL && evo_project_provider_available(provider) &&
           evo_sandbox_program_available("bwrap");
#else
    return false;
#endif
}

evo_project_sandbox_status_t evo_project_sandbox_run(
    const evo_project_sandbox_command_t *command,
    evo_project_sandbox_result_t *result)
{
    if (!evo_sandbox_command_valid(command) || result == NULL ||
        result->private_owner != NULL || result->schema_version != 0U) {
        return EVO_PROJECT_SANDBOX_ERROR_INVALID_ARGUMENT;
    }
    if (!evo_project_sandbox_available()) {
        return EVO_PROJECT_SANDBOX_ERROR_UNAVAILABLE;
    }
#if defined(__linux__)
    return evo_sandbox_run_linux(command, result);
#else
    (void)command;
    (void)result;
    return EVO_PROJECT_SANDBOX_ERROR_UNAVAILABLE;
#endif
}

void evo_project_sandbox_result_destroy(evo_project_sandbox_result_t *result)
{
    evo_project_sandbox_owner_t *owner;

    if (result == NULL) {
        return;
    }
    owner = result->private_owner;
    if (owner != NULL) {
        evo_project_release(owner->stdout_text);
        evo_project_release(owner->stderr_text);
        evo_project_release(owner);
    }
    *result = (evo_project_sandbox_result_t){0};
}

const char *evo_project_sandbox_status_name(evo_project_sandbox_status_t status)
{
    switch (status) {
    case EVO_PROJECT_SANDBOX_SUCCESS:
        return "success";
    case EVO_PROJECT_SANDBOX_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_SANDBOX_ERROR_UNAVAILABLE:
        return "unavailable";
    case EVO_PROJECT_SANDBOX_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_SANDBOX_ERROR_PROCESS:
        return "process";
    case EVO_PROJECT_SANDBOX_ERROR_CLEANUP:
        return "cleanup";
    default:
        return "unknown";
    }
}

const char *evo_project_sandbox_resource_name(
    evo_project_sandbox_resource_t resource)
{
    switch (resource) {
    case EVO_PROJECT_SANDBOX_RESOURCE_NONE:
        return "none";
    case EVO_PROJECT_SANDBOX_RESOURCE_CPU:
        return "cpu";
    case EVO_PROJECT_SANDBOX_RESOURCE_ADDRESS_SPACE:
        return "address-space";
    case EVO_PROJECT_SANDBOX_RESOURCE_PROCESS_COUNT:
        return "process-count";
    case EVO_PROJECT_SANDBOX_RESOURCE_STORAGE:
        return "storage";
    case EVO_PROJECT_SANDBOX_RESOURCE_OUTPUT:
        return "output";
    case EVO_PROJECT_SANDBOX_RESOURCE_WALL_TIME:
        return "wall-time";
    default:
        return "unknown";
    }
}
