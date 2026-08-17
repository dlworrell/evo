#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_SANDBOX_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_SANDBOX_H

#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define EVO_PROJECT_SANDBOX_SCHEMA_VERSION 1U

typedef enum evo_project_sandbox_status {
    EVO_PROJECT_SANDBOX_SUCCESS = 0,
    EVO_PROJECT_SANDBOX_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_SANDBOX_ERROR_UNAVAILABLE = 2,
    EVO_PROJECT_SANDBOX_ERROR_RESOURCE_LIMIT = 3,
    EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY = 4,
    EVO_PROJECT_SANDBOX_ERROR_PROCESS = 5,
    EVO_PROJECT_SANDBOX_ERROR_CLEANUP = 6
} evo_project_sandbox_status_t;

typedef enum evo_project_sandbox_resource {
    EVO_PROJECT_SANDBOX_RESOURCE_NONE = 0,
    EVO_PROJECT_SANDBOX_RESOURCE_CPU = 1,
    EVO_PROJECT_SANDBOX_RESOURCE_ADDRESS_SPACE = 2,
    EVO_PROJECT_SANDBOX_RESOURCE_PROCESS_COUNT = 3,
    EVO_PROJECT_SANDBOX_RESOURCE_STORAGE = 4,
    EVO_PROJECT_SANDBOX_RESOURCE_OUTPUT = 5,
    EVO_PROJECT_SANDBOX_RESOURCE_WALL_TIME = 6
} evo_project_sandbox_resource_t;

typedef struct evo_project_sandbox_limits {
    uint64_t cpu_time_ms;
    uint64_t address_space_bytes;
    size_t descendant_process_count;
    uint64_t storage_bytes;
    size_t output_bytes;
    uint64_t wall_timeout_ms;
    bool network_access;
} evo_project_sandbox_limits_t;

typedef struct evo_project_sandbox_command {
    uint32_t schema_version;
    const char *workspace_path;
    const char *working_directory;
    size_t argument_count;
    const char *const *arguments;
    size_t environment_count;
    const char *const *environment;
    evo_project_sandbox_limits_t limits;
} evo_project_sandbox_command_t;

typedef struct evo_project_sandbox_result {
    uint32_t schema_version;
    bool completed;
    bool timed_out;
    bool signaled;
    bool resource_exhausted;
    evo_project_sandbox_resource_t exhausted_resource;
    int exit_code;
    int signal_number;
    uint64_t elapsed_ns;
    size_t stdout_bytes;
    uint64_t stdout_fingerprint;
    size_t stderr_bytes;
    uint64_t stderr_fingerprint;
    const char *stdout_text;
    const char *stderr_text;
    bool cpu_limit_enforced;
    bool address_space_limit_enforced;
    bool process_limit_enforced;
    bool storage_limit_enforced;
    bool output_limit_enforced;
    bool timeout_enforced;
    bool filesystem_isolation_enforced;
    bool network_isolation_enforced;
    bool descendant_cleanup_enforced;
    void *private_owner;
} evo_project_sandbox_result_t;

bool evo_project_sandbox_available(void);

evo_project_sandbox_status_t evo_project_sandbox_run(
    const evo_project_sandbox_command_t *command,
    evo_project_sandbox_result_t *result);

void evo_project_sandbox_result_destroy(evo_project_sandbox_result_t *result);

const char *evo_project_sandbox_status_name(evo_project_sandbox_status_t status);

const char *evo_project_sandbox_resource_name(
    evo_project_sandbox_resource_t resource);

#endif
