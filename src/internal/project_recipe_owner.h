#ifndef CATALYST_EVO_INTERNAL_PROJECT_RECIPE_OWNER_H
#define CATALYST_EVO_INTERNAL_PROJECT_RECIPE_OWNER_H

#include "internal/project_recipe.h"

typedef struct evo_project_recipe_record_owner {
    evo_project_recipe_record_t view;
    char *identity;
    char *target_location_identity;
    char *target_file;
    char *target_spelling_identity;
    char *transformation_identity;
    evo_project_recipe_parameter_value_t *parameters;
    char **preconditions;
    evo_project_recipe_dependency_t *dependencies;
    evo_project_transformation_reference_t *conflicts;
    char **compiler_record_identities;
    char **runtime_record_identities;
    size_t source_index;
} evo_project_recipe_record_owner_t;

typedef struct evo_project_recipe_owner {
    char *baseline_fingerprint;
    char *analysis_fingerprint;
    char *catalogue_identity;
    uint32_t catalogue_version;
    evo_project_recipe_record_owner_t *record_owners;
    evo_project_recipe_record_t *records;
    size_t record_count;
    unsigned char *genome;
    size_t genome_size;
    size_t canonical_json_size;
    char *audit_markdown;
    size_t audit_markdown_size;
    uint64_t recipe_fingerprint;
} evo_project_recipe_owner_t;

#endif
