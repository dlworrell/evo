#ifndef CATALYST_EVO_INTERNAL_PROJECT_ASSURANCE_H
#define CATALYST_EVO_INTERNAL_PROJECT_ASSURANCE_H

#include "internal/project_candidate.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_ASSURANCE_SCHEMA_VERSION 1U

typedef enum evo_project_assurance_status {
    EVO_PROJECT_ASSURANCE_SUCCESS = 0,
    EVO_PROJECT_ASSURANCE_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_ASSURANCE_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_ASSURANCE_ERROR_CANDIDATE_INELIGIBLE = 3,
    EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID = 4,
    EVO_PROJECT_ASSURANCE_ERROR_PATH_INVALID = 5,
    EVO_PROJECT_ASSURANCE_ERROR_OUTPUT_EXISTS = 6,
    EVO_PROJECT_ASSURANCE_ERROR_RESOURCE_LIMIT = 7,
    EVO_PROJECT_ASSURANCE_ERROR_OUT_OF_MEMORY = 8,
    EVO_PROJECT_ASSURANCE_ERROR_EXECUTION_PROVIDER = 9,
    EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE = 10,
    EVO_PROJECT_ASSURANCE_ERROR_STATE = 11
} evo_project_assurance_status_t;

typedef enum evo_project_assurance_stage {
    EVO_PROJECT_ASSURANCE_STAGE_FAST = 1,
    EVO_PROJECT_ASSURANCE_STAGE_FINALIST = 2
} evo_project_assurance_stage_t;

typedef enum evo_project_assurance_disposition {
    EVO_PROJECT_ASSURANCE_GATE_NOT_RUN = 0,
    EVO_PROJECT_ASSURANCE_GATE_PASSED = 1,
    EVO_PROJECT_ASSURANCE_GATE_FAILED = 2,
    EVO_PROJECT_ASSURANCE_GATE_TIMED_OUT = 3,
    EVO_PROJECT_ASSURANCE_GATE_SIGNALED = 4,
    EVO_PROJECT_ASSURANCE_GATE_RESOURCE_EXHAUSTED = 5,
    EVO_PROJECT_ASSURANCE_GATE_UNAVAILABLE = 6,
    EVO_PROJECT_ASSURANCE_GATE_POLICY_FAILED = 7,
    EVO_PROJECT_ASSURANCE_GATE_CLEANUP_FAILED = 8
} evo_project_assurance_disposition_t;

typedef struct evo_project_assurance_limits {
    size_t max_string_bytes;
    size_t max_path_bytes;
    size_t max_gates;
    size_t max_arguments;
    size_t max_environment_entries;
    size_t max_command_bytes;
    size_t max_output_bytes;
    size_t max_diagnostic_bytes;
    size_t max_evidence_bytes;
    uint64_t max_timeout_ms;
    uint64_t max_memory_bytes;
    size_t max_processes;
    uint64_t max_storage_bytes;
} evo_project_assurance_limits_t;

typedef struct evo_project_assurance_gate {
    const char *gate_id;
    const char *profile_id;
    evo_project_assurance_stage_t stage;
    bool required;
    size_t argument_count;
    const char *const *arguments;
    size_t environment_count;
    const char *const *environment;
    const char *working_directory;
    uint64_t timeout_ms;
    uint64_t max_memory_bytes;
    size_t max_processes;
    uint64_t max_storage_bytes;
    size_t max_output_bytes;
    bool network_access;
} evo_project_assurance_gate_t;

typedef struct evo_project_assurance_gate_view {
    const char *gate_id;
    const char *profile_id;
    evo_project_assurance_stage_t stage;
    bool required;
    size_t argument_count;
    const char *const *arguments;
    size_t environment_count;
    const char *const *environment;
    const char *working_directory;
    uint64_t timeout_ms;
    uint64_t max_memory_bytes;
    size_t max_processes;
    uint64_t max_storage_bytes;
    size_t max_output_bytes;
    bool network_access;
    bool workspace_only_filesystem;
    bool shell_interpretation;
} evo_project_assurance_gate_view_t;

typedef struct evo_project_assurance_gate_outcome {
    uint32_t schema_version;
    bool completed;
    bool available;
    bool timed_out;
    bool signaled;
    bool resource_exhausted;
    bool filesystem_policy_enforced;
    bool network_policy_enforced;
    bool process_group_clean;
    bool source_modified;
    bool snapshot_modified;
    int exit_code;
    int signal_number;
    size_t stdout_bytes;
    uint64_t stdout_fingerprint;
    size_t stderr_bytes;
    uint64_t stderr_fingerprint;
    uint64_t toolchain_fingerprint;
    const char *diagnostic_excerpt;
    size_t diagnostic_excerpt_bytes;
} evo_project_assurance_gate_outcome_t;

typedef evo_project_assurance_status_t (*evo_project_assurance_runner_fn)(
    const evo_project_assurance_gate_view_t *gate,
    const char *candidate_workspace_path,
    void *context,
    evo_project_assurance_gate_outcome_t *outcome);

typedef struct evo_project_assurance_config {
    const evo_project_candidate_t *candidate;
    const char *policy_id;
    const char *execution_provider_identity;
    evo_project_assurance_stage_t stage;
    size_t required_profile_count;
    const char *const *required_profiles;
    size_t gate_count;
    const evo_project_assurance_gate_t *gates;
    const char *output_path;
    bool allow_network_gates;
    evo_project_assurance_limits_t limits;
    evo_project_assurance_runner_fn runner;
    void *runner_context;
} evo_project_assurance_config_t;

typedef struct evo_project_assurance_gate_result {
    const char *gate_id;
    const char *profile_id;
    evo_project_assurance_stage_t stage;
    bool required;
    evo_project_assurance_disposition_t disposition;
    int exit_code;
    int signal_number;
    size_t stdout_bytes;
    char stdout_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t stderr_bytes;
    char stderr_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char toolchain_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    const char *diagnostic_excerpt;
    bool filesystem_policy_enforced;
    bool network_policy_enforced;
    bool process_group_clean;
    bool source_modified;
    bool snapshot_modified;
} evo_project_assurance_gate_result_t;

typedef struct evo_project_assurance {
    uint32_t schema_version;
    const char *candidate_fingerprint;
    const char *policy_id;
    char policy_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    const char *execution_provider_identity;
    evo_project_assurance_stage_t stage;
    size_t gate_count;
    const evo_project_assurance_gate_result_t *gates;
    bool performance_eligible;
    bool champion_eligible;
    bool projection_complete;
    bool probabilistic_authority;
    bool source_modified;
    bool snapshot_modified;
    char assurance_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    const char *output_path;
    size_t canonical_json_size;
    const char *canonical_json;
    size_t audit_markdown_size;
    const char *audit_markdown;
    void *private_owner;
} evo_project_assurance_t;

evo_project_assurance_status_t evo_project_candidate_assure(
    const evo_project_assurance_config_t *config,
    evo_project_assurance_t *assurance);

void evo_project_assurance_destroy(evo_project_assurance_t *assurance);

const char *evo_project_assurance_status_name(
    evo_project_assurance_status_t status);

const char *evo_project_assurance_stage_name(
    evo_project_assurance_stage_t stage);

const char *evo_project_assurance_disposition_name(
    evo_project_assurance_disposition_t disposition);

#endif
