#ifndef CATALYST_EVO_INTERNAL_PROJECT_CANDIDATE_OWNER_H
#define CATALYST_EVO_INTERNAL_PROJECT_CANDIDATE_OWNER_H

#include "internal/project_candidate.h"

typedef struct evo_project_candidate_owner {
    char *baseline_fingerprint;
    char *recipe_fingerprint;
    char *output_path;
    char *workspace_path;
    evo_project_candidate_changed_file_t *changed_files;
    char **changed_paths;
    size_t changed_file_count;
    char *patch;
    size_t patch_size;
    char *canonical_json;
    size_t canonical_json_size;
    char *audit_markdown;
    size_t audit_markdown_size;
    uint64_t candidate_fingerprint;
    evo_project_candidate_t view;
} evo_project_candidate_owner_t;

#endif
