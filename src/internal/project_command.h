#ifndef CATALYST_EVO_INTERNAL_PROJECT_COMMAND_H
#define CATALYST_EVO_INTERNAL_PROJECT_COMMAND_H

#include "internal/project_manifest.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_COMMAND_SCHEMA_VERSION 1U
#define EVO_PROJECT_COMMAND_INTERFACE_VERSION 1U
#define EVO_PROJECT_PRODUCT_COMMAND_COUNT 4U
#define EVO_PROJECT_COMMAND_PROVIDER_POLICY_ID \
    "catalyst.evo.provider-policy.v1"

#define EVO_PROJECT_COMMAND_PROVIDER_CLANG_ANALYSIS_ID \
    "catalyst.evo.provider.clang-analysis.v1"
#define EVO_PROJECT_COMMAND_PROVIDER_CLANG_AST_ID \
    "catalyst.evo.provider.clang-ast.v1"
#define EVO_PROJECT_COMMAND_PROVIDER_LINUX_BWRAP_ID \
    "catalyst.evo.provider.linux-bwrap.v1"
#define EVO_PROJECT_COMMAND_PROVIDER_LOCAL_EVALUATION_ID \
    "catalyst.evo.provider.local-evaluation.v1"

#define EVO_PROJECT_COMMAND_PROVIDER_IMPLEMENTATION_VERSION 1U

typedef enum evo_project_command_operation {
    EVO_PROJECT_COMMAND_ANALYZE = 0,
    EVO_PROJECT_COMMAND_EVOLVE = 1,
    EVO_PROJECT_COMMAND_REPLAY = 2,
    EVO_PROJECT_COMMAND_REPORT = 3
} evo_project_command_operation_t;

typedef enum evo_project_command_status {
    EVO_PROJECT_COMMAND_SUCCESS = 0,
    EVO_PROJECT_COMMAND_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_COMMAND_ERROR_SCHEMA = 2,
    EVO_PROJECT_COMMAND_ERROR_OPERATION = 3,
    EVO_PROJECT_COMMAND_ERROR_PATH = 4,
    EVO_PROJECT_COMMAND_ERROR_MANIFEST = 5,
    EVO_PROJECT_COMMAND_ERROR_PROVIDER_POLICY = 6,
    EVO_PROJECT_COMMAND_ERROR_PROVIDER_IDENTITY = 7,
    EVO_PROJECT_COMMAND_ERROR_PROVIDER_VERSION = 8,
    EVO_PROJECT_COMMAND_ERROR_PROVIDER_UNAVAILABLE = 9,
    EVO_PROJECT_COMMAND_ERROR_PROVIDER_CAPABILITY = 10,
    EVO_PROJECT_COMMAND_ERROR_REPLAY_IDENTITY = 11,
    EVO_PROJECT_COMMAND_ERROR_CHECKPOINT = 12,
    EVO_PROJECT_COMMAND_ERROR_OUTPUT_POLICY = 13
} evo_project_command_status_t;

typedef enum evo_project_command_exit_status {
    EVO_PROJECT_COMMAND_EXIT_SUCCESS = 0,
    EVO_PROJECT_COMMAND_EXIT_INTERNAL = 1,
    EVO_PROJECT_COMMAND_EXIT_USAGE = 2,
    EVO_PROJECT_COMMAND_EXIT_CONFIGURATION = 3,
    EVO_PROJECT_COMMAND_EXIT_INGESTION = 10,
    EVO_PROJECT_COMMAND_EXIT_BASELINE_BUILD = 11,
    EVO_PROJECT_COMMAND_EXIT_CORRECTNESS = 12,
    EVO_PROJECT_COMMAND_EXIT_BENCHMARK_INELIGIBLE = 13,
    EVO_PROJECT_COMMAND_EXIT_ANALYSIS = 20,
    EVO_PROJECT_COMMAND_EXIT_MATERIALIZATION = 21,
    EVO_PROJECT_COMMAND_EXIT_CANDIDATE = 22,
    EVO_PROJECT_COMMAND_EXIT_NO_ELIGIBLE_CHAMPION = 23,
    EVO_PROJECT_COMMAND_EXIT_REPLAY_MISMATCH = 24,
    EVO_PROJECT_COMMAND_EXIT_RESOURCE = 70,
    EVO_PROJECT_COMMAND_EXIT_INTERRUPTED = 130
} evo_project_command_exit_status_t;

typedef enum evo_project_command_terminal_class {
    EVO_PROJECT_COMMAND_TERMINAL_SUCCESS = 0,
    EVO_PROJECT_COMMAND_TERMINAL_INVALID_INPUT = 1,
    EVO_PROJECT_COMMAND_TERMINAL_INGESTION_FAILURE = 2,
    EVO_PROJECT_COMMAND_TERMINAL_BASELINE_BUILD_FAILURE = 3,
    EVO_PROJECT_COMMAND_TERMINAL_CORRECTNESS_FAILURE = 4,
    EVO_PROJECT_COMMAND_TERMINAL_BENCHMARK_INELIGIBLE = 5,
    EVO_PROJECT_COMMAND_TERMINAL_ANALYSIS_FAILURE = 6,
    EVO_PROJECT_COMMAND_TERMINAL_MATERIALIZATION_FAILURE = 7,
    EVO_PROJECT_COMMAND_TERMINAL_CANDIDATE_FAILURE = 8,
    EVO_PROJECT_COMMAND_TERMINAL_NO_ELIGIBLE_CHAMPION = 9,
    EVO_PROJECT_COMMAND_TERMINAL_REPLAY_MISMATCH = 10,
    EVO_PROJECT_COMMAND_TERMINAL_RESOURCE_FAILURE = 11,
    EVO_PROJECT_COMMAND_TERMINAL_INTERRUPTED = 12,
    EVO_PROJECT_COMMAND_TERMINAL_INTERNAL_FAILURE = 13
} evo_project_command_terminal_class_t;

typedef enum evo_project_command_provider_slot {
    EVO_PROJECT_COMMAND_PROVIDER_ANALYSIS = 0,
    EVO_PROJECT_COMMAND_PROVIDER_TRANSFORMATION_AST = 1,
    EVO_PROJECT_COMMAND_PROVIDER_EXECUTION = 2,
    EVO_PROJECT_COMMAND_PROVIDER_EVALUATION = 3,
    EVO_PROJECT_COMMAND_PROVIDER_COUNT = 4
} evo_project_command_provider_slot_t;

typedef enum evo_project_command_provider_capability {
    EVO_PROJECT_COMMAND_CAPABILITY_CLANG_AST = UINT64_C(1) << 0,
    EVO_PROJECT_COMMAND_CAPABILITY_COMPILATION_DATABASE = UINT64_C(1) << 1,
    EVO_PROJECT_COMMAND_CAPABILITY_DIRECT_ARGV = UINT64_C(1) << 2,
    EVO_PROJECT_COMMAND_CAPABILITY_CPU_LIMIT = UINT64_C(1) << 3,
    EVO_PROJECT_COMMAND_CAPABILITY_ADDRESS_SPACE_LIMIT = UINT64_C(1) << 4,
    EVO_PROJECT_COMMAND_CAPABILITY_PROCESS_LIMIT = UINT64_C(1) << 5,
    EVO_PROJECT_COMMAND_CAPABILITY_STORAGE_LIMIT = UINT64_C(1) << 6,
    EVO_PROJECT_COMMAND_CAPABILITY_OUTPUT_LIMIT = UINT64_C(1) << 7,
    EVO_PROJECT_COMMAND_CAPABILITY_WALL_TIMEOUT = UINT64_C(1) << 8,
    EVO_PROJECT_COMMAND_CAPABILITY_FILESYSTEM_ISOLATION = UINT64_C(1) << 9,
    EVO_PROJECT_COMMAND_CAPABILITY_NETWORK_ISOLATION = UINT64_C(1) << 10,
    EVO_PROJECT_COMMAND_CAPABILITY_DESCENDANT_CLEANUP = UINT64_C(1) << 11,
    EVO_PROJECT_COMMAND_CAPABILITY_ASYNC_START_POLL_CANCEL_JOIN = UINT64_C(1) << 12,
    EVO_PROJECT_COMMAND_CAPABILITY_MEASUREMENT = UINT64_C(1) << 13
} evo_project_command_provider_capability_t;

typedef struct evo_project_command_descriptor {
    uint32_t schema_version;
    evo_project_command_operation_t operation;
    const char *name;
    const char *request_schema;
    const char *help;
    bool requires_manifest;
    bool requires_input;
    bool requires_evidence;
    bool allows_checkpoint;
} evo_project_command_descriptor_t;

typedef struct evo_project_command_provider_selection {
    const char *identity;
    uint32_t implementation_version;
    uint64_t capabilities;
    bool available;
} evo_project_command_provider_selection_t;

typedef struct evo_project_command_request {
    uint32_t schema_version;
    evo_project_command_operation_t operation;
    const char *manifest_path;
    const evo_project_manifest_t *manifest;
    const char *input_path;
    const char *output_path;
    const char *evidence_path;
    const char *checkpoint_path;
    const char *provider_policy_identity;
    evo_project_command_provider_selection_t providers[EVO_PROJECT_COMMAND_PROVIDER_COUNT];
    bool resume;
    bool overwrite_output;
    bool replay_identity_complete;
    bool external_inputs_declared;
} evo_project_command_request_t;

typedef struct evo_project_command_provider_requirement {
    const char *identity;
    uint32_t implementation_version;
    uint64_t required_capabilities;
    bool required;
} evo_project_command_provider_requirement_t;

typedef struct evo_project_command_plan {
    uint32_t schema_version;
    uint32_t interface_version;
    evo_project_command_operation_t operation;
    const char *name;
    const char *request_schema;
    const char *manifest_path;
    const char *input_path;
    const char *output_path;
    const char *evidence_path;
    const char *checkpoint_path;
    const char *provider_policy_identity;
    evo_project_command_provider_requirement_t providers[EVO_PROJECT_COMMAND_PROVIDER_COUNT];
    size_t required_provider_count;
    bool resume;
    bool execution_permitted;
    bool input_repository_read_only;
    bool output_atomic;
    bool existing_output_rejected;
    bool network_access_implicit;
    bool repository_mutation_permitted;
    bool machine_evidence_authoritative;
    bool human_summary_is_projection;
    bool stdout_machine_output_only;
    bool stderr_diagnostics_only;
} evo_project_command_plan_t;

size_t evo_project_command_registry_count(void);

const evo_project_command_descriptor_t *evo_project_command_registry_at(
    size_t index);

const evo_project_command_descriptor_t *evo_project_command_find(
    const char *name);

const evo_project_command_provider_requirement_t *evo_project_command_provider_requirement(
    evo_project_command_provider_slot_t slot);

evo_project_command_status_t evo_project_command_plan_build(
    const evo_project_command_request_t *request,
    evo_project_command_plan_t *plan);

evo_project_command_exit_status_t evo_project_command_exit_for_terminal(
    evo_project_command_operation_t operation,
    evo_project_command_terminal_class_t terminal_class);

const char *evo_project_command_operation_name(
    evo_project_command_operation_t operation);

const char *evo_project_command_status_name(
    evo_project_command_status_t status);

const char *evo_project_command_exit_status_name(
    evo_project_command_exit_status_t status);

#endif
