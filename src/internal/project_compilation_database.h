#ifndef CATALYST_EVO_INTERNAL_PROJECT_COMPILATION_DATABASE_H
#define CATALYST_EVO_INTERNAL_PROJECT_COMPILATION_DATABASE_H

#include "internal/project_ingestion.h"

evo_project_status_t evo_project_compilation_database_load(
    const char *snapshot_root,
    const char *relative_path,
    const char *authorized_root,
    const evo_project_ingest_limits_t *limits,
    const evo_project_file_record_t *files,
    size_t file_count,
    evo_project_compilation_record_t **records,
    size_t *record_count,
    uint64_t *normalized_fingerprint);

evo_project_status_t evo_project_compilation_database_parse(
    const char *text,
    size_t text_size,
    const char *authorized_root,
    const evo_project_ingest_limits_t *limits,
    const evo_project_file_record_t *files,
    size_t file_count,
    evo_project_compilation_record_t **records,
    size_t *record_count,
    uint64_t *normalized_fingerprint);

void evo_project_compilation_database_destroy(
    evo_project_compilation_record_t *records,
    size_t record_count);

#endif
