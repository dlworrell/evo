#ifndef CATALYST_EVO_INTERNAL_PROJECT_ANALYSIS_H
#define CATALYST_EVO_INTERNAL_PROJECT_ANALYSIS_H

#include "internal/project_ingestion.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_ANALYSIS_SCHEMA_VERSION 1U

typedef enum evo_project_analysis_status {
    EVO_PROJECT_ANALYSIS_SUCCESS = 0,
    EVO_PROJECT_ANALYSIS_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_ANALYSIS_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_ANALYSIS_ERROR_BASELINE_INELIGIBLE = 3,
    EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT = 4,
    EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY = 5,
    EVO_PROJECT_ANALYSIS_ERROR_PROVIDER = 6,
    EVO_PROJECT_ANALYSIS_ERROR_INCONSISTENT_EVIDENCE = 7,
    EVO_PROJECT_ANALYSIS_ERROR_BASELINE_CHANGED = 8,
    EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID = 9,
    EVO_PROJECT_ANALYSIS_ERROR_OUTPUT_EXISTS = 10,
    EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO = 11,
    EVO_PROJECT_ANALYSIS_ERROR_STATE = 12,
    EVO_PROJECT_ANALYSIS_ERROR_UNSUPPORTED_EVIDENCE = 13
} evo_project_analysis_status_t;

typedef enum evo_project_source_location_kind {
    EVO_PROJECT_LOCATION_SPELLING = 1,
    EVO_PROJECT_LOCATION_MACRO_EXPANSION = 2,
    EVO_PROJECT_LOCATION_GENERATED = 3
} evo_project_source_location_kind_t;

typedef enum evo_project_declaration_kind {
    EVO_PROJECT_DECLARATION_FUNCTION = 1,
    EVO_PROJECT_DECLARATION_VARIABLE = 2,
    EVO_PROJECT_DECLARATION_TYPE = 3
} evo_project_declaration_kind_t;

typedef enum evo_project_linkage {
    EVO_PROJECT_LINKAGE_NONE = 1,
    EVO_PROJECT_LINKAGE_INTERNAL = 2,
    EVO_PROJECT_LINKAGE_EXTERNAL = 3
} evo_project_linkage_t;

typedef enum evo_project_call_kind {
    EVO_PROJECT_CALL_DIRECT = 1,
    EVO_PROJECT_CALL_INDIRECT = 2
} evo_project_call_kind_t;

typedef enum evo_project_control_flow_kind {
    EVO_PROJECT_CONTROL_FALLTHROUGH = 1,
    EVO_PROJECT_CONTROL_BRANCH_TRUE = 2,
    EVO_PROJECT_CONTROL_BRANCH_FALSE = 3,
    EVO_PROJECT_CONTROL_BACK_EDGE = 4,
    EVO_PROJECT_CONTROL_RETURN = 5
} evo_project_control_flow_kind_t;

typedef enum evo_project_data_flow_kind {
    EVO_PROJECT_DATA_READ = 1,
    EVO_PROJECT_DATA_WRITE = 2,
    EVO_PROJECT_DATA_ADDRESS = 3,
    EVO_PROJECT_DATA_ESCAPE = 4
} evo_project_data_flow_kind_t;

typedef enum evo_project_optimization_disposition {
    EVO_PROJECT_OPTIMIZATION_PASSED = 1,
    EVO_PROJECT_OPTIMIZATION_MISSED = 2,
    EVO_PROJECT_OPTIMIZATION_ANALYSIS = 3
} evo_project_optimization_disposition_t;

typedef enum evo_project_runtime_profile_state {
    EVO_PROJECT_RUNTIME_NOT_CONFIGURED = 1,
    EVO_PROJECT_RUNTIME_UNAVAILABLE = 2,
    EVO_PROJECT_RUNTIME_AVAILABLE = 3
} evo_project_runtime_profile_state_t;

typedef enum evo_project_runtime_metric {
    EVO_PROJECT_RUNTIME_SAMPLE_COUNT = 1
} evo_project_runtime_metric_t;

typedef struct evo_project_analysis_limits {
    size_t max_string_bytes;
    size_t max_path_bytes;
    size_t max_translation_units;
    size_t max_source_locations;
    size_t max_declarations;
    size_t max_calls;
    size_t max_control_flows;
    size_t max_data_flows;
    size_t max_optimization_records;
    size_t max_runtime_records;
    size_t max_opportunities;
    size_t max_evidence_bytes;
} evo_project_analysis_limits_t;

typedef struct evo_project_source_location_record {
    const char *identity;
    const char *file;
    uint32_t line;
    uint32_t column;
    uint32_t end_line;
    uint32_t end_column;
    evo_project_source_location_kind_t kind;
    const char *spelling_identity;
} evo_project_source_location_record_t;

typedef struct evo_project_declaration_record {
    const char *identity;
    const char *name;
    const char *translation_unit;
    const char *location_identity;
    evo_project_declaration_kind_t kind;
    evo_project_linkage_t linkage;
    bool definition;
} evo_project_declaration_record_t;

typedef struct evo_project_call_record {
    const char *identity;
    const char *caller_identity;
    const char *callee_identity;
    const char *location_identity;
    evo_project_call_kind_t kind;
} evo_project_call_record_t;

typedef struct evo_project_control_flow_record {
    const char *identity;
    const char *function_identity;
    const char *from_block_identity;
    const char *to_block_identity;
    const char *location_identity;
    evo_project_control_flow_kind_t kind;
} evo_project_control_flow_record_t;

typedef struct evo_project_data_flow_record {
    const char *identity;
    const char *function_identity;
    const char *declaration_identity;
    const char *location_identity;
    evo_project_data_flow_kind_t kind;
} evo_project_data_flow_record_t;

typedef struct evo_project_optimization_record {
    const char *identity;
    const char *pass_name;
    const char *function_identity;
    const char *location_identity;
    const char *message;
    evo_project_optimization_disposition_t disposition;
} evo_project_optimization_record_t;

typedef struct evo_project_runtime_record {
    const char *identity;
    const char *workload_identity;
    const char *function_identity;
    const char *location_identity;
    evo_project_runtime_metric_t metric;
    uint64_t value;
} evo_project_runtime_record_t;

typedef struct evo_project_analysis_provider_result {
    uint32_t schema_version;
    bool completed;
    size_t source_location_count;
    const evo_project_source_location_record_t *source_locations;
    size_t declaration_count;
    const evo_project_declaration_record_t *declarations;
    size_t call_count;
    const evo_project_call_record_t *calls;
    size_t control_flow_count;
    const evo_project_control_flow_record_t *control_flows;
    size_t data_flow_count;
    const evo_project_data_flow_record_t *data_flows;
    size_t optimization_record_count;
    const evo_project_optimization_record_t *optimization_records;
    size_t runtime_record_count;
    const evo_project_runtime_record_t *runtime_records;
} evo_project_analysis_provider_result_t;

typedef struct evo_project_analysis_request {
    uint32_t schema_version;
    const char *baseline_fingerprint;
    const char *snapshot_path;
    size_t compilation_unit_count;
    const evo_project_compilation_record_t *compilation_units;
    const char *provider_identity;
    const char *clang_identity;
    const char *llvm_identity;
    const char *target_identity;
    const char *flags_identity;
    evo_project_runtime_profile_state_t runtime_profile_state;
    const char *runtime_profile_identity;
    evo_project_analysis_limits_t limits;
    uint64_t timeout_ms;
    uint64_t max_memory_bytes;
    size_t max_processes;
    uint64_t max_storage_bytes;
    size_t max_output_bytes;
    bool network_access;
} evo_project_analysis_request_t;

typedef evo_project_analysis_status_t (*evo_project_analysis_provider_fn)(
    const evo_project_analysis_request_t *request,
    void *context,
    evo_project_analysis_provider_result_t *result);

typedef struct evo_project_analysis_config {
    const evo_project_baseline_t *baseline;
    const char *output_path;
    const char *provider_identity;
    const char *clang_identity;
    const char *llvm_identity;
    const char *target_identity;
    const char *flags_identity;
    evo_project_runtime_profile_state_t runtime_profile_state;
    const char *runtime_profile_identity;
    evo_project_analysis_limits_t limits;
    evo_project_analysis_provider_fn provider;
    void *provider_context;
} evo_project_analysis_config_t;

typedef struct evo_project_opportunity_record {
    size_t rank;
    const char *location_identity;
    size_t missed_optimization_count;
    bool runtime_evidence_present;
    uint64_t runtime_sample_count;
} evo_project_opportunity_record_t;

typedef struct evo_project_analysis {
    uint32_t schema_version;
    const char *baseline_fingerprint;
    char analysis_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    const char *provider_identity;
    const char *clang_identity;
    const char *llvm_identity;
    const char *target_identity;
    const char *flags_identity;
    evo_project_runtime_profile_state_t runtime_profile_state;
    const char *runtime_profile_identity;
    const char *output_path;
    size_t translation_unit_count;
    const char *const *translation_units;
    size_t source_location_count;
    const evo_project_source_location_record_t *source_locations;
    size_t declaration_count;
    const evo_project_declaration_record_t *declarations;
    size_t call_count;
    const evo_project_call_record_t *calls;
    size_t control_flow_count;
    const evo_project_control_flow_record_t *control_flows;
    size_t data_flow_count;
    const evo_project_data_flow_record_t *data_flows;
    size_t optimization_record_count;
    const evo_project_optimization_record_t *optimization_records;
    size_t runtime_record_count;
    const evo_project_runtime_record_t *runtime_records;
    size_t opportunity_count;
    const evo_project_opportunity_record_t *opportunities;
    bool projection_complete;
    bool probabilistic_authority;
    void *private_owner;
} evo_project_analysis_t;

evo_project_analysis_status_t evo_project_analyze(
    const evo_project_analysis_config_t *config,
    evo_project_analysis_t *analysis);

void evo_project_analysis_destroy(evo_project_analysis_t *analysis);

const char *evo_project_analysis_status_name(
    evo_project_analysis_status_t status);

const char *evo_project_runtime_profile_state_name(
    evo_project_runtime_profile_state_t state);

#endif
