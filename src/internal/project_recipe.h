#ifndef CATALYST_EVO_INTERNAL_PROJECT_RECIPE_H
#define CATALYST_EVO_INTERNAL_PROJECT_RECIPE_H

#include "internal/project_analysis.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_RECIPE_SCHEMA_VERSION 1U
#define EVO_PROJECT_TRANSFORMATION_CATALOGUE_SCHEMA_VERSION 1U
#define EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE 16U

#define EVO_PROJECT_RECIPE_LOCATION_SPELLING UINT32_C(1)
#define EVO_PROJECT_RECIPE_LOCATION_MACRO_EXPANSION UINT32_C(2)
#define EVO_PROJECT_RECIPE_LOCATION_ALL     \
    (EVO_PROJECT_RECIPE_LOCATION_SPELLING | \
     EVO_PROJECT_RECIPE_LOCATION_MACRO_EXPANSION)

typedef enum evo_project_recipe_status {
    EVO_PROJECT_RECIPE_SUCCESS = 0,
    EVO_PROJECT_RECIPE_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_RECIPE_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_RECIPE_ERROR_BASELINE_INELIGIBLE = 3,
    EVO_PROJECT_RECIPE_ERROR_ANALYSIS_STALE = 4,
    EVO_PROJECT_RECIPE_ERROR_CATALOGUE_INVALID = 5,
    EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT = 6,
    EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY = 7,
    EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID = 8,
    EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION = 9,
    EVO_PROJECT_RECIPE_ERROR_STALE_TARGET = 10,
    EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER = 11,
    EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_MISSING = 12,
    EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_AMBIGUOUS = 13,
    EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_CYCLE = 14,
    EVO_PROJECT_RECIPE_ERROR_CONFLICT = 15,
    EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT = 16,
    EVO_PROJECT_RECIPE_ERROR_GENOME_NONCANONICAL = 17,
    EVO_PROJECT_RECIPE_ERROR_BASELINE_CHANGED = 18,
    EVO_PROJECT_RECIPE_ERROR_STATE = 19
} evo_project_recipe_status_t;

typedef enum evo_project_recipe_parameter_kind {
    EVO_PROJECT_RECIPE_PARAMETER_INTEGER = 1,
    EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN = 2,
    EVO_PROJECT_RECIPE_PARAMETER_CHOICE = 3
} evo_project_recipe_parameter_kind_t;

typedef struct evo_project_transformation_reference {
    const char *identity;
    uint32_t implementation_version;
} evo_project_transformation_reference_t;

typedef struct evo_project_transformation_parameter_schema {
    const char *identity;
    evo_project_recipe_parameter_kind_t kind;
    bool required;
    int64_t minimum_integer;
    int64_t maximum_integer;
    size_t choice_count;
    const char *const *choices;
} evo_project_transformation_parameter_schema_t;

typedef struct evo_project_transformation_catalogue_entry {
    const char *identity;
    uint32_t implementation_version;
    uint32_t allowed_location_kinds;
    size_t parameter_schema_count;
    const evo_project_transformation_parameter_schema_t *parameter_schemas;
    size_t precondition_count;
    const char *const *preconditions;
    size_t dependency_count;
    const evo_project_transformation_reference_t *dependencies;
    size_t conflict_count;
    const evo_project_transformation_reference_t *conflicts;
} evo_project_transformation_catalogue_entry_t;

typedef struct evo_project_transformation_catalogue {
    uint32_t schema_version;
    const char *identity;
    uint32_t catalogue_version;
    size_t entry_count;
    const evo_project_transformation_catalogue_entry_t *entries;
} evo_project_transformation_catalogue_t;

typedef struct evo_project_recipe_limits {
    size_t max_string_bytes;
    size_t max_path_bytes;
    size_t max_catalogue_entries;
    size_t max_parameter_schemas;
    size_t max_choices;
    size_t max_records;
    size_t max_parameters_per_record;
    size_t max_preconditions_per_record;
    size_t max_dependencies_per_record;
    size_t max_conflicts_per_record;
    size_t max_provenance_records_per_record;
    size_t max_json_tokens;
    size_t max_json_depth;
    size_t max_genome_bytes;
    size_t max_audit_bytes;
    size_t max_total_bytes;
} evo_project_recipe_limits_t;

typedef struct evo_project_recipe_parameter_value {
    const char *identity;
    evo_project_recipe_parameter_kind_t kind;
    int64_t integer_value;
    bool boolean_value;
    const char *choice_value;
} evo_project_recipe_parameter_value_t;

typedef struct evo_project_recipe_proposal_record {
    const char *identity;
    const char *target_location_identity;
    const char *transformation_identity;
    uint32_t transformation_version;
    size_t parameter_count;
    const evo_project_recipe_parameter_value_t *parameters;
} evo_project_recipe_proposal_record_t;

typedef struct evo_project_recipe_context {
    const evo_project_baseline_t *baseline;
    const evo_project_analysis_t *analysis;
    const evo_project_transformation_catalogue_t *catalogue;
    evo_project_recipe_limits_t limits;
} evo_project_recipe_context_t;

typedef struct evo_project_recipe_build_config {
    evo_project_recipe_context_t context;
    size_t record_count;
    const evo_project_recipe_proposal_record_t *records;
    size_t genome_size;
} evo_project_recipe_build_config_t;

typedef struct evo_project_recipe_target {
    const char *location_identity;
    const char *file;
    uint32_t line;
    uint32_t column;
    uint32_t end_line;
    uint32_t end_column;
    evo_project_source_location_kind_t kind;
    const char *spelling_identity;
} evo_project_recipe_target_t;

typedef struct evo_project_recipe_dependency {
    const char *record_identity;
    const char *transformation_identity;
    uint32_t transformation_version;
} evo_project_recipe_dependency_t;

typedef struct evo_project_recipe_record {
    const char *identity;
    const char *baseline_fingerprint;
    const char *analysis_fingerprint;
    const char *catalogue_identity;
    uint32_t catalogue_version;
    evo_project_recipe_target_t target;
    const char *transformation_identity;
    uint32_t transformation_version;
    size_t parameter_count;
    const evo_project_recipe_parameter_value_t *parameters;
    size_t precondition_count;
    const char *const *preconditions;
    size_t dependency_count;
    const evo_project_recipe_dependency_t *dependencies;
    size_t conflict_count;
    const evo_project_transformation_reference_t *conflicts;
    size_t opportunity_rank;
    size_t compiler_record_count;
    const char *const *compiler_record_identities;
    size_t runtime_record_count;
    const char *const *runtime_record_identities;
} evo_project_recipe_record_t;

typedef struct evo_project_recipe {
    uint32_t schema_version;
    const char *baseline_fingerprint;
    const char *analysis_fingerprint;
    const char *catalogue_identity;
    uint32_t catalogue_version;
    char recipe_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t record_count;
    const evo_project_recipe_record_t *records;
    size_t genome_size;
    const unsigned char *genome;
    size_t canonical_json_size;
    const char *canonical_json;
    size_t audit_markdown_size;
    const char *audit_markdown;
    bool projection_complete;
    bool probabilistic_authority;
    bool raw_source_bytes;
    void *private_owner;
} evo_project_recipe_t;

evo_project_recipe_status_t evo_project_recipe_build(
    const evo_project_recipe_build_config_t *config,
    evo_project_recipe_t *recipe);

evo_project_recipe_status_t evo_project_recipe_decode(
    const evo_project_recipe_context_t *context,
    const unsigned char *genome,
    size_t genome_size,
    evo_project_recipe_t *recipe);

bool evo_project_recipe_equal(
    const evo_project_recipe_t *left,
    const evo_project_recipe_t *right);

void evo_project_recipe_destroy(evo_project_recipe_t *recipe);

const char *evo_project_recipe_status_name(
    evo_project_recipe_status_t status);

#endif
