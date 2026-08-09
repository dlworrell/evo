#ifndef CATALYST_EVO_INTERNAL_PROJECT_BASELINE_OWNER_H
#define CATALYST_EVO_INTERNAL_PROJECT_BASELINE_OWNER_H

#include "internal/project_ingestion.h"
#include "internal/project_manifest.h"

typedef struct evo_project_baseline_owner {
    evo_project_manifest_t manifest;
    char *authorized_root;
    char *output_path;
    char *stage_path;
    char *snapshot_path;
    char *workspace_path;
    char *execution_provider_identity;
    evo_project_file_record_t *files;
    size_t file_count;
    uint64_t total_file_bytes;
    evo_project_compilation_record_t *compilation_units;
    size_t compilation_unit_count;
    uint64_t normalized_build_fingerprint;
    uint64_t baseline_fingerprint;
    evo_project_command_record_t commands[EVO_PROJECT_COMMAND_COUNT];
    evo_project_baseline_state_t state;
    bool output_reserved;
    bool committed;
} evo_project_baseline_owner_t;

#endif
