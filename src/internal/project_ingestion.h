#ifndef CATALYST_EVO_INTERNAL_PROJECT_INGESTION_H
#define CATALYST_EVO_INTERNAL_PROJECT_INGESTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_BASELINE_SCHEMA_VERSION 1U
#define EVO_PROJECT_COMMAND_COUNT 4U
#define EVO_PROJECT_FINGERPRINT_TEXT_SIZE 28U

typedef enum evo_project_status {
    EVO_PROJECT_SUCCESS = 0,
    EVO_PROJECT_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_ERROR_MANIFEST_IO = 3,
    EVO_PROJECT_ERROR_MANIFEST_INVALID = 4,
    EVO_PROJECT_ERROR_RESOURCE_LIMIT = 5,
    EVO_PROJECT_ERROR_OUT_OF_MEMORY = 6,
    EVO_PROJECT_ERROR_PATH_INVALID = 7,
    EVO_PROJECT_ERROR_SOURCE_IO = 8,
    EVO_PROJECT_ERROR_SOURCE_CHANGED = 9,
    EVO_PROJECT_ERROR_OUTPUT_EXISTS = 10,
    EVO_PROJECT_ERROR_EXECUTION_PROVIDER = 11,
    EVO_PROJECT_ERROR_EVIDENCE_IO = 12,
    EVO_PROJECT_ERROR_STATE = 13
} evo_project_status_t;

typedef enum evo_project_baseline_state {
    EVO_PROJECT_BASELINE_NONE = 0,
    EVO_PROJECT_BASELINE_ELIGIBLE = 1,
    EVO_PROJECT_BASELINE_BUILD_FAILED = 2,
    EVO_PROJECT_BASELINE_CORRECTNESS_FAILED = 3,
    EVO_PROJECT_BASELINE_BENCHMARK_INELIGIBLE = 4
} evo_project_baseline_state_t;

typedef enum evo_project_command_stage {
    EVO_PROJECT_COMMAND_CONFIGURE = 1,
    EVO_PROJECT_COMMAND_COMPILE = 2,
    EVO_PROJECT_COMMAND_CORRECTNESS = 3,
    EVO_PROJECT_COMMAND_BENCHMARK = 4
} evo_project_command_stage_t;

typedef enum evo_project_command_disposition {
    EVO_PROJECT_COMMAND_NOT_RUN = 0,
    EVO_PROJECT_COMMAND_PASSED = 1,
    EVO_PROJECT_COMMAND_FAILED = 2,
    EVO_PROJECT_COMMAND_TIMED_OUT = 3
} evo_project_command_disposition_t;

typedef struct evo_project_ingest_limits {
    size_t max_manifest_bytes;
    size_t max_json_tokens;
    size_t max_json_depth;
    size_t max_string_bytes;
    size_t max_path_bytes;
    size_t max_files;
    size_t max_file_bytes;
    size_t max_total_bytes;
    size_t max_compilation_database_bytes;
    size_t max_permitted_roots;
    size_t max_dependencies;
    size_t max_toolchains;
    size_t max_environment_entries;
    size_t max_targets;
    size_t max_workloads;
    size_t max_constraints;
    size_t max_command_args;
    size_t max_command_bytes;
    size_t max_command_output_bytes;
    size_t max_evidence_bytes;
    uint64_t max_command_timeout_ms;
    uint64_t max_memory_bytes;
    size_t max_processes;
    uint64_t max_storage_bytes;
} evo_project_ingest_limits_t;

typedef struct evo_project_command_view {
    evo_project_command_stage_t stage;
    const char *stage_id;
    size_t argument_count;
    const char *const *arguments;
    uint64_t timeout_ms;
    uint64_t max_memory_bytes;
    size_t max_processes;
    uint64_t max_storage_bytes;
    size_t max_output_bytes;
    bool network_access;
} evo_project_command_view_t;

typedef struct evo_project_command_outcome {
    uint32_t schema_version;
    bool completed;
    bool timed_out;
    int exit_code;
    size_t output_bytes;
    uint64_t output_fingerprint;
} evo_project_command_outcome_t;

typedef evo_project_status_t (*evo_project_command_runner_fn)(
    const evo_project_command_view_t *command,
    const char *workspace_path,
    void *context,
    evo_project_command_outcome_t *outcome);

typedef struct evo_project_capture_config {
    const char *manifest_path;
    const char *authorized_project_root;
    const char *output_path;
    const char *execution_provider_identity;
    evo_project_ingest_limits_t limits;
    evo_project_command_runner_fn command_runner;
    void *command_runner_context;
} evo_project_capture_config_t;

typedef struct evo_project_file_record {
    const char *path;
    uint64_t size;
    unsigned int source_mode;
    uint64_t content_fingerprint;
} evo_project_file_record_t;

typedef enum evo_project_compile_command_form {
    EVO_PROJECT_COMPILE_COMMAND_ARGUMENTS = 1,
    EVO_PROJECT_COMPILE_COMMAND_SHELL = 2
} evo_project_compile_command_form_t;

typedef struct evo_project_compilation_record {
    const char *directory;
    const char *file;
    const char *output;
    evo_project_compile_command_form_t command_form;
    size_t argument_count;
    const char *const *arguments;
    const char *command;
} evo_project_compilation_record_t;

typedef struct evo_project_command_record {
    evo_project_command_stage_t stage;
    const char *stage_id;
    evo_project_command_disposition_t disposition;
    int exit_code;
    size_t output_bytes;
    uint64_t output_fingerprint;
} evo_project_command_record_t;

typedef struct evo_project_baseline {
    uint32_t schema_version;
    evo_project_baseline_state_t state;
    const char *manifest_id;
    const char *source_declared_identity;
    const char *build_frontend;
    const char *language;
    const char *compilation_database;
    const char *execution_provider_identity;
    const char *output_path;
    char manifest_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char baseline_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t file_count;
    uint64_t total_file_bytes;
    const evo_project_file_record_t *files;
    size_t compilation_unit_count;
    const evo_project_compilation_record_t *compilation_units;
    char normalized_build_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t command_count;
    const evo_project_command_record_t *commands;
    bool projection_complete;
    bool probabilistic_authority;
    void *private_owner;
} evo_project_baseline_t;

evo_project_status_t evo_project_capture_baseline(
    const evo_project_capture_config_t *config,
    evo_project_baseline_t *baseline);

void evo_project_baseline_destroy(evo_project_baseline_t *baseline);

const char *evo_project_status_name(evo_project_status_t status);

const char *evo_project_baseline_state_name(
    evo_project_baseline_state_t state);

#endif
