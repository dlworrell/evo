#ifndef CATALYST_EVO_INTERNAL_PROJECT_MANIFEST_H
#define CATALYST_EVO_INTERNAL_PROJECT_MANIFEST_H

#include "internal/project_ingestion.h"
#include "internal/project_json.h"

typedef struct evo_project_named_identity {
    char *name;
    char *identity;
} evo_project_named_identity_t;

typedef struct evo_project_environment_entry {
    char *name;
    char *value;
} evo_project_environment_entry_t;

typedef struct evo_project_manifest_command {
    evo_project_command_stage_t stage;
    const char *stage_id;
    size_t argument_count;
    char **arguments;
} evo_project_manifest_command_t;

typedef struct evo_project_manifest_budget {
    size_t max_files;
    size_t max_file_bytes;
    size_t max_total_bytes;
    size_t max_path_bytes;
    size_t max_compilation_database_bytes;
    size_t max_command_output_bytes;
    size_t max_evidence_bytes;
    uint64_t command_timeout_ms;
    uint64_t max_memory_bytes;
    size_t max_processes;
    uint64_t max_storage_bytes;
    bool network_access;
} evo_project_manifest_budget_t;

typedef struct evo_project_manifest_search {
    uint64_t seed;
    size_t population;
    size_t generations;
    size_t workers;
} evo_project_manifest_search_t;

typedef struct evo_project_manifest {
    char *schema;
    char *manifest_id;
    char *source_identity;
    char **permitted_roots;
    size_t permitted_root_count;
    char *compilation_database;
    char *generated_source_policy;
    char *build_frontend;
    evo_project_manifest_command_t commands[EVO_PROJECT_COMMAND_COUNT];
    bool benchmark_required;
    char *language;
    char **targets;
    size_t target_count;
    evo_project_named_identity_t *dependencies;
    size_t dependency_count;
    evo_project_named_identity_t *toolchains;
    size_t toolchain_count;
    evo_project_environment_entry_t *environment;
    size_t environment_count;
    char **workloads;
    size_t workload_count;
    char **constraints;
    size_t constraint_count;
    evo_project_manifest_search_t search;
    evo_project_manifest_budget_t budget;
    char *artifact_retention;
    char *cleanup_policy;
    uint64_t fingerprint;
} evo_project_manifest_t;

evo_project_status_t evo_project_manifest_parse(
    const char *text,
    size_t text_size,
    const evo_project_ingest_limits_t *limits,
    evo_project_manifest_t *manifest);

void evo_project_manifest_destroy(evo_project_manifest_t *manifest);

bool evo_project_relative_path_valid(
    const char *path,
    size_t maximum_bytes);

#endif
