#ifndef CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_H
#define CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_H

#include "internal/project_recipe.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_TRANSFORMATION_REGISTRY_SCHEMA_VERSION 1U
#define EVO_PROJECT_TRANSFORMATION_AST_SCHEMA_VERSION 1U
#define EVO_PROJECT_TRANSFORMATION_APPLICATION_SCHEMA_VERSION 1U
#define EVO_PROJECT_TRANSFORMATION_PROVIDER_CONTRACT_VERSION 1U

typedef enum evo_project_transformation_status {
    EVO_PROJECT_TRANSFORMATION_SUCCESS = 0,
    EVO_PROJECT_TRANSFORMATION_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_TRANSFORMATION_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_INELIGIBLE = 3,
    EVO_PROJECT_TRANSFORMATION_ERROR_ANALYSIS_STALE = 4,
    EVO_PROJECT_TRANSFORMATION_ERROR_RECIPE_STALE = 5,
    EVO_PROJECT_TRANSFORMATION_ERROR_CATALOGUE_INVALID = 6,
    EVO_PROJECT_TRANSFORMATION_ERROR_RECORD_NOT_FOUND = 7,
    EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT = 8,
    EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY = 9,
    EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER = 10,
    EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED = 11,
    EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE = 12,
    EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_MACRO = 13,
    EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_COMMENT = 14,
    EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_PREPROCESSOR = 15,
    EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_EXTENSION = 16,
    EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_ALIAS_ASSUMPTION = 17,
    EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET = 18,
    EVO_PROJECT_TRANSFORMATION_ERROR_SOURCE_IO = 19,
    EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_CHANGED = 20,
    EVO_PROJECT_TRANSFORMATION_ERROR_EVIDENCE = 21,
    EVO_PROJECT_TRANSFORMATION_ERROR_STATE = 22
} evo_project_transformation_status_t;

typedef enum evo_project_transformation_disposition {
    EVO_PROJECT_TRANSFORMATION_EDIT = 1,
    EVO_PROJECT_TRANSFORMATION_ALREADY_SATISFIED = 2
} evo_project_transformation_disposition_t;

typedef enum evo_project_transformation_ast_form {
    EVO_PROJECT_AST_ASSIGNMENT_BINARY = 1,
    EVO_PROJECT_AST_ASSIGNMENT_COMPOUND = 2,
    EVO_PROJECT_AST_UNSIGNED_MULTIPLY_POWER_OF_TWO = 3,
    EVO_PROJECT_AST_UNSIGNED_SHIFT_POWER_OF_TWO = 4,
    EVO_PROJECT_AST_DOUBLE_NEGATED_CONDITION = 5,
    EVO_PROJECT_AST_SCALAR_CONDITION = 6
} evo_project_transformation_ast_form_t;

typedef enum evo_project_transformation_operator {
    EVO_PROJECT_TRANSFORMATION_OPERATOR_NONE = 0,
    EVO_PROJECT_TRANSFORMATION_OPERATOR_ADD = 1,
    EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_AND = 2,
    EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_OR = 3,
    EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_XOR = 4,
    EVO_PROJECT_TRANSFORMATION_OPERATOR_MULTIPLY = 5,
    EVO_PROJECT_TRANSFORMATION_OPERATOR_SUBTRACT = 6,
    EVO_PROJECT_TRANSFORMATION_OPERATOR_SHIFT_LEFT = 7
} evo_project_transformation_operator_t;

typedef enum evo_project_transformation_condition_context {
    EVO_PROJECT_TRANSFORMATION_CONDITION_NONE = 0,
    EVO_PROJECT_TRANSFORMATION_CONDITION_DO_WHILE = 1,
    EVO_PROJECT_TRANSFORMATION_CONDITION_FOR = 2,
    EVO_PROJECT_TRANSFORMATION_CONDITION_IF = 3,
    EVO_PROJECT_TRANSFORMATION_CONDITION_WHILE = 4
} evo_project_transformation_condition_context_t;

typedef struct evo_project_transformation_byte_range {
    size_t start;
    size_t end;
} evo_project_transformation_byte_range_t;

typedef struct evo_project_transformation_limits {
    size_t max_string_bytes;
    size_t max_path_bytes;
    size_t max_source_bytes;
    size_t max_replacement_bytes;
    size_t max_parameters;
    size_t max_registry_bytes;
    size_t max_application_bytes;
    size_t max_audit_bytes;
    size_t max_total_bytes;
} evo_project_transformation_limits_t;

typedef struct evo_project_transformation_capability {
    const char *identity;
    uint32_t implementation_version;
    uint32_t provider_contract_version;
    size_t ast_form_count;
    const evo_project_transformation_ast_form_t *ast_forms;
    const char *formatting_policy;
    const char *idempotence_policy;
    size_t semantic_assumption_count;
    const char *const *semantic_assumptions;
    size_t validation_obligation_count;
    const char *const *validation_obligations;
    bool comments_supported;
    bool macros_supported;
    bool language_extensions_supported;
    bool alias_assumptions_supported;
} evo_project_transformation_capability_t;

typedef struct evo_project_transformation_registry {
    uint32_t schema_version;
    const evo_project_transformation_catalogue_t *recipe_catalogue;
    size_t capability_count;
    const evo_project_transformation_capability_t *capabilities;
    size_t canonical_json_size;
    const char *canonical_json;
    size_t audit_markdown_size;
    const char *audit_markdown;
    bool projection_complete;
    bool probabilistic_authority;
    void *private_owner;
} evo_project_transformation_registry_t;

typedef struct evo_project_transformation_ast_result {
    uint32_t schema_version;
    bool completed;
    const char *location_identity;
    const char *file;
    evo_project_transformation_ast_form_t form;
    evo_project_transformation_operator_t operator_kind;
    evo_project_transformation_condition_context_t condition_context;
    evo_project_transformation_byte_range_t target;
    evo_project_transformation_byte_range_t primary;
    evo_project_transformation_byte_range_t duplicate_primary;
    evo_project_transformation_byte_range_t operand;
    evo_project_transformation_byte_range_t literal;
    const char *primary_declaration_identity;
    const char *duplicate_declaration_identity;
    uint64_t literal_value;
    uint32_t result_width_bits;
    bool primary_plain_identifier;
    bool volatile_access;
    bool result_unsigned_integer;
    bool result_type_matches_primary;
    bool scalar_operand;
    bool contains_macro;
    bool contains_comment;
    bool contains_preprocessor;
    bool language_extension;
    bool ambiguous_target;
    bool alias_assumption_required;
} evo_project_transformation_ast_result_t;

typedef struct evo_project_transformation_request {
    uint32_t schema_version;
    const char *baseline_fingerprint;
    const char *analysis_fingerprint;
    const char *recipe_fingerprint;
    const char *snapshot_path;
    const char *record_identity;
    const evo_project_recipe_target_t *target;
    const char *transformation_identity;
    uint32_t transformation_version;
    size_t parameter_count;
    const evo_project_recipe_parameter_value_t *parameters;
    size_t source_size;
    const char *source_fingerprint;
    evo_project_transformation_limits_t limits;
    bool network_access;
} evo_project_transformation_request_t;

typedef evo_project_transformation_status_t (*evo_project_transformation_ast_provider_fn)(
    const evo_project_transformation_request_t *request,
    void *context,
    evo_project_transformation_ast_result_t *result);

typedef struct evo_project_transformation_apply_config {
    const evo_project_baseline_t *baseline;
    const evo_project_analysis_t *analysis;
    const evo_project_recipe_t *recipe;
    const evo_project_transformation_registry_t *registry;
    const char *record_identity;
    const char *provider_identity;
    uint32_t provider_version;
    const char *clang_identity;
    evo_project_transformation_limits_t limits;
    evo_project_transformation_ast_provider_fn provider;
    void *provider_context;
} evo_project_transformation_apply_config_t;

typedef struct evo_project_transformation_edit {
    size_t before_start;
    size_t before_end;
    size_t before_size;
    const char *before_text;
    char before_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t after_start;
    size_t after_end;
    size_t replacement_size;
    const char *replacement_text;
    char replacement_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
} evo_project_transformation_edit_t;

typedef struct evo_project_transformation_application {
    uint32_t schema_version;
    const char *baseline_fingerprint;
    const char *analysis_fingerprint;
    const char *recipe_fingerprint;
    const char *catalogue_identity;
    uint32_t catalogue_version;
    const char *record_identity;
    const char *transformation_identity;
    uint32_t transformation_version;
    size_t parameter_count;
    const evo_project_recipe_parameter_value_t *parameters;
    const char *provider_identity;
    uint32_t provider_version;
    const char *clang_identity;
    evo_project_recipe_target_t target;
    evo_project_transformation_ast_form_t ast_form;
    evo_project_transformation_operator_t operator_kind;
    evo_project_transformation_condition_context_t condition_context;
    evo_project_transformation_byte_range_t ast_primary;
    evo_project_transformation_byte_range_t ast_duplicate_primary;
    evo_project_transformation_byte_range_t ast_operand;
    evo_project_transformation_byte_range_t ast_literal;
    const char *primary_declaration_identity;
    const char *duplicate_declaration_identity;
    uint64_t literal_value;
    uint32_t result_width_bits;
    bool primary_plain_identifier;
    bool volatile_access;
    bool result_unsigned_integer;
    bool result_type_matches_primary;
    bool scalar_operand;
    bool contains_macro;
    bool contains_comment;
    bool contains_preprocessor;
    bool language_extension;
    bool ambiguous_target;
    bool alias_assumption_required;
    evo_project_transformation_disposition_t disposition;
    evo_project_transformation_edit_t edit;
    const char *formatting_policy;
    const char *idempotence_policy;
    size_t semantic_assumption_count;
    const char *const *semantic_assumptions;
    size_t validation_obligation_count;
    const char *const *validation_obligations;
    char application_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t canonical_json_size;
    const char *canonical_json;
    size_t audit_markdown_size;
    const char *audit_markdown;
    bool projection_complete;
    bool probabilistic_authority;
    bool snapshot_modified;
    bool candidate_materialized;
    void *private_owner;
} evo_project_transformation_application_t;

evo_project_transformation_status_t evo_project_transformation_registry_open(
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_registry_t *registry);

void evo_project_transformation_registry_destroy(
    evo_project_transformation_registry_t *registry);

evo_project_transformation_status_t evo_project_transformation_apply(
    const evo_project_transformation_apply_config_t *config,
    evo_project_transformation_application_t *application);

void evo_project_transformation_application_destroy(
    evo_project_transformation_application_t *application);

const char *evo_project_transformation_status_name(
    evo_project_transformation_status_t status);

const char *evo_project_transformation_ast_form_name(
    evo_project_transformation_ast_form_t form);

const char *evo_project_transformation_operator_name(
    evo_project_transformation_operator_t operator_kind);

const char *evo_project_transformation_condition_context_name(
    evo_project_transformation_condition_context_t context);

#endif
