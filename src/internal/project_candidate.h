#ifndef CATALYST_EVO_INTERNAL_PROJECT_CANDIDATE_H
#define CATALYST_EVO_INTERNAL_PROJECT_CANDIDATE_H

#include "internal/project_transformation.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_CANDIDATE_SCHEMA_VERSION 1U

typedef enum evo_project_candidate_status {
    EVO_PROJECT_CANDIDATE_SUCCESS = 0,
    EVO_PROJECT_CANDIDATE_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_CANDIDATE_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_CANDIDATE_ERROR_BASELINE_INELIGIBLE = 3,
    EVO_PROJECT_CANDIDATE_ERROR_BASELINE_CHANGED = 4,
    EVO_PROJECT_CANDIDATE_ERROR_RECIPE_STALE = 5,
    EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_STALE = 6,
    EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_MISSING = 7,
    EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_DUPLICATE = 8,
    EVO_PROJECT_CANDIDATE_ERROR_CONFLICT = 9,
    EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID = 10,
    EVO_PROJECT_CANDIDATE_ERROR_OUTPUT_EXISTS = 11,
    EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT = 12,
    EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY = 13,
    EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO = 14,
    EVO_PROJECT_CANDIDATE_ERROR_EVIDENCE = 15,
    EVO_PROJECT_CANDIDATE_ERROR_STATE = 16
} evo_project_candidate_status_t;

typedef enum evo_project_candidate_workspace_policy {
    EVO_PROJECT_CANDIDATE_WORKSPACE_DISCARD = 1,
    EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN = 2
} evo_project_candidate_workspace_policy_t;

typedef struct evo_project_candidate_limits {
    size_t max_string_bytes;
    size_t max_path_bytes;
    size_t max_files;
    size_t max_file_bytes;
    size_t max_total_file_bytes;
    size_t max_edits;
    size_t max_patch_bytes;
    size_t max_evidence_bytes;
} evo_project_candidate_limits_t;

typedef struct evo_project_candidate_changed_file {
    const char *path;
    uint64_t before_size;
    uint64_t after_size;
    char before_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char after_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t edit_count;
} evo_project_candidate_changed_file_t;

typedef struct evo_project_candidate_config {
    const evo_project_baseline_t *baseline;
    const evo_project_recipe_t *recipe;
    size_t application_count;
    const evo_project_transformation_application_t *applications;
    const char *output_path;
    evo_project_candidate_workspace_policy_t workspace_policy;
    evo_project_candidate_limits_t limits;
} evo_project_candidate_config_t;

typedef struct evo_project_candidate {
    uint32_t schema_version;
    const char *baseline_fingerprint;
    const char *recipe_fingerprint;
    char candidate_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    const char *output_path;
    const char *workspace_path;
    evo_project_candidate_workspace_policy_t workspace_policy;
    size_t file_count;
    size_t changed_file_count;
    const evo_project_candidate_changed_file_t *changed_files;
    size_t patch_size;
    const char *patch;
    size_t canonical_json_size;
    const char *canonical_json;
    size_t audit_markdown_size;
    const char *audit_markdown;
    bool projection_complete;
    bool probabilistic_authority;
    bool source_modified;
    bool snapshot_modified;
    void *private_owner;
} evo_project_candidate_t;

evo_project_candidate_status_t evo_project_candidate_materialize(
    const evo_project_candidate_config_t *config,
    evo_project_candidate_t *candidate);

void evo_project_candidate_destroy(evo_project_candidate_t *candidate);

const char *evo_project_candidate_status_name(
    evo_project_candidate_status_t status);

const char *evo_project_candidate_workspace_policy_name(
    evo_project_candidate_workspace_policy_t policy);

#endif
