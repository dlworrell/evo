#ifndef CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_OWNER_H
#define CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_OWNER_H

#include "internal/project_transformation.h"

typedef struct evo_project_transformation_registry_owner {
    char *canonical_json;
    size_t canonical_json_size;
    char *audit_markdown;
    size_t audit_markdown_size;
} evo_project_transformation_registry_owner_t;

typedef struct evo_project_transformation_application_owner {
    char *baseline_fingerprint;
    char *analysis_fingerprint;
    char *recipe_fingerprint;
    char *catalogue_identity;
    char *record_identity;
    char *transformation_identity;
    evo_project_recipe_parameter_value_t *parameters;
    size_t parameter_count;
    char *provider_identity;
    char *clang_identity;
    char *target_location_identity;
    char *target_file;
    char *target_spelling_identity;
    char *primary_declaration_identity;
    char *duplicate_declaration_identity;
    char *before_text;
    char *replacement_text;
    char *formatting_policy;
    char *idempotence_policy;
    char **semantic_assumptions;
    size_t semantic_assumption_count;
    char **validation_obligations;
    size_t validation_obligation_count;
    char *canonical_json;
    size_t canonical_json_size;
    char *audit_markdown;
    size_t audit_markdown_size;
    uint64_t application_fingerprint;
    evo_project_transformation_application_t view;
} evo_project_transformation_application_owner_t;

#endif
