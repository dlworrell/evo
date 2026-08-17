#ifndef CATALYST_EVO_INTERNAL_PROJECT_ORCHESTRATION_H
#define CATALYST_EVO_INTERNAL_PROJECT_ORCHESTRATION_H

#include "internal/project_search.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION 1U
#define EVO_PROJECT_ORCHESTRATION_POLICY_VERSION 1U

typedef enum evo_project_orchestration_status {
    EVO_PROJECT_ORCHESTRATION_SUCCESS = 0,
    EVO_PROJECT_ORCHESTRATION_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_ORCHESTRATION_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_ORCHESTRATION_ERROR_POLICY_INVALID = 3,
    EVO_PROJECT_ORCHESTRATION_ERROR_RESOURCE_LIMIT = 4,
    EVO_PROJECT_ORCHESTRATION_ERROR_OUT_OF_MEMORY = 5,
    EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER = 6,
    EVO_PROJECT_ORCHESTRATION_ERROR_CLEANUP = 7,
    EVO_PROJECT_ORCHESTRATION_ERROR_EVIDENCE = 8,
    EVO_PROJECT_ORCHESTRATION_ERROR_STATE = 9
} evo_project_orchestration_status_t;

typedef enum evo_project_orchestration_job_state {
    EVO_PROJECT_ORCHESTRATION_JOB_UNASSIGNED = 0,
    EVO_PROJECT_ORCHESTRATION_JOB_ADMITTED = 1,
    EVO_PROJECT_ORCHESTRATION_JOB_STARTED = 2,
    EVO_PROJECT_ORCHESTRATION_JOB_CANCEL_REQUESTED = 3,
    EVO_PROJECT_ORCHESTRATION_JOB_TERMINAL = 4,
    EVO_PROJECT_ORCHESTRATION_JOB_JOINED = 5,
    EVO_PROJECT_ORCHESTRATION_JOB_STAGED = 6,
    EVO_PROJECT_ORCHESTRATION_JOB_COMMITTED = 7
} evo_project_orchestration_job_state_t;

typedef enum evo_project_orchestration_terminal_reason {
    EVO_PROJECT_ORCHESTRATION_TERMINAL_NONE = 0,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS = 1,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_CANDIDATE_REJECTED = 2,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_START_FAILED = 3,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_TIMEOUT = 4,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_SIGNAL = 5,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_CPU_LIMIT = 6,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_MEMORY_LIMIT = 7,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_PROCESS_LIMIT = 8,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_STORAGE_LIMIT = 9,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_OUTPUT_LIMIT = 10,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED = 11,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_CAPABILITY_UNAVAILABLE = 12,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL = 13,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_JOIN_FAILED = 14,
    EVO_PROJECT_ORCHESTRATION_TERMINAL_CLEANUP_FAILED = 15
} evo_project_orchestration_terminal_reason_t;

typedef struct evo_project_orchestration_resource_policy {
    uint32_t schema_version;
    size_t external_worker_count;
    uint64_t cpu_time_ms;
    uint64_t address_space_bytes;
    size_t descendant_process_count;
    uint64_t storage_bytes;
    uint64_t output_bytes;
    uint64_t wall_timeout_ms;
    uint64_t workspace_bytes;
    bool require_filesystem_isolation;
    bool require_network_isolation;
    bool require_descendant_cleanup;
} evo_project_orchestration_resource_policy_t;

typedef struct evo_project_orchestration_limits {
    size_t max_string_bytes;
    size_t max_candidates;
    size_t max_external_workers;
    size_t max_poll_rounds;
    uint64_t max_cpu_time_ms;
    uint64_t max_address_space_bytes;
    size_t max_descendant_process_count;
    uint64_t max_storage_bytes;
    uint64_t max_output_bytes;
    uint64_t max_wall_timeout_ms;
    uint64_t max_workspace_bytes;
    size_t max_total_bytes;
} evo_project_orchestration_limits_t;

typedef struct evo_project_orchestration_candidate_request {
    uint32_t schema_version;
    size_t generation;
    size_t population_index;
    const char *recipe_fingerprint;
    const char *workspace_identity;
    uint64_t random_seed;
    const evo_project_recipe_t *recipe;
} evo_project_orchestration_candidate_request_t;

typedef struct evo_project_orchestration_provider_request {
    uint32_t schema_version;
    const char *provider_identity;
    const char *policy_identity;
    size_t logical_worker_identity;
    size_t dispatch_wave;
    evo_project_orchestration_resource_policy_t resources;
    evo_project_orchestration_candidate_request_t candidate;
} evo_project_orchestration_provider_request_t;

typedef struct evo_project_orchestration_provider_poll {
    uint32_t schema_version;
    bool terminal;
    evo_project_orchestration_terminal_reason_t terminal_reason;
} evo_project_orchestration_provider_poll_t;

typedef struct evo_project_orchestration_provider_capabilities {
    uint32_t schema_version;
    bool cpu_limit_enforced;
    bool address_space_limit_enforced;
    bool process_limit_enforced;
    bool storage_limit_enforced;
    bool output_limit_enforced;
    bool timeout_enforced;
    bool filesystem_isolation_enforced;
    bool network_isolation_enforced;
    bool descendant_cleanup_enforced;
} evo_project_orchestration_provider_capabilities_t;

typedef struct evo_project_orchestration_provider_join {
    uint32_t schema_version;
    evo_project_orchestration_terminal_reason_t terminal_reason;
    evo_project_orchestration_provider_capabilities_t capabilities;
    evo_project_search_evaluation_outcome_t evaluation;
    bool cleanup_complete;
} evo_project_orchestration_provider_join_t;

typedef evo_project_orchestration_status_t (*evo_project_orchestration_provider_start_fn)(
    const evo_project_orchestration_provider_request_t *request,
    void *context,
    void **handle);

typedef evo_project_orchestration_status_t (*evo_project_orchestration_provider_poll_fn)(
    void *handle,
    void *context,
    evo_project_orchestration_provider_poll_t *poll);

typedef evo_project_orchestration_status_t (*evo_project_orchestration_provider_cancel_fn)(
    void *handle,
    void *context);

typedef evo_project_orchestration_status_t (*evo_project_orchestration_provider_join_fn)(
    void *handle,
    void *context,
    evo_project_orchestration_provider_join_t *join);

typedef struct evo_project_orchestration_provider {
    const char *identity;
    evo_project_orchestration_provider_start_fn start;
    evo_project_orchestration_provider_poll_fn poll;
    evo_project_orchestration_provider_cancel_fn cancel;
    evo_project_orchestration_provider_join_fn join;
    void *context;
} evo_project_orchestration_provider_t;

typedef struct evo_project_orchestration_config {
    const char *policy_identity;
    evo_project_orchestration_resource_policy_t resources;
    size_t candidate_count;
    const evo_project_orchestration_candidate_request_t *candidates;
    evo_project_orchestration_provider_t provider;
    evo_project_orchestration_limits_t limits;
} evo_project_orchestration_config_t;

typedef struct evo_project_orchestration_job_record {
    size_t generation;
    size_t population_index;
    size_t logical_worker_identity;
    size_t dispatch_wave;
    size_t completion_ordinal;
    size_t commit_ordinal;
    evo_project_orchestration_job_state_t state;
    evo_project_orchestration_terminal_reason_t terminal_reason;
    char candidate_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    evo_project_search_evaluation_outcome_t evaluation;
    bool admitted;
    bool started;
    bool cancel_requested;
    bool terminal;
    bool joined;
    bool staged;
    bool committed;
    bool cleanup_complete;
} evo_project_orchestration_job_record_t;

typedef struct evo_project_orchestration {
    uint32_t schema_version;
    const char *policy_identity;
    const char *provider_identity;
    size_t generation;
    size_t candidate_count;
    size_t external_worker_count;
    size_t completion_count;
    size_t committed_count;
    size_t first_hard_failure_index;
    bool has_hard_failure;
    bool generation_committed;
    bool cleanup_complete;
    size_t job_count;
    const evo_project_orchestration_job_record_t *jobs;
    bool projection_complete;
    bool probabilistic_authority;
    void *private_owner;
} evo_project_orchestration_t;

evo_project_orchestration_status_t evo_project_orchestration_run_batch(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_t *orchestration);

void evo_project_orchestration_destroy(
    evo_project_orchestration_t *orchestration);

const char *evo_project_orchestration_status_name(
    evo_project_orchestration_status_t status);
const char *evo_project_orchestration_job_state_name(
    evo_project_orchestration_job_state_t state);
const char *evo_project_orchestration_terminal_reason_name(
    evo_project_orchestration_terminal_reason_t reason);

#endif
