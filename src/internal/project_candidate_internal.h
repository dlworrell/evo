#ifndef CATALYST_EVO_INTERNAL_PROJECT_CANDIDATE_INTERNAL_H
#define CATALYST_EVO_INTERNAL_PROJECT_CANDIDATE_INTERNAL_H

#include "internal/project_baseline_owner.h"
#include "internal/project_candidate.h"
#include "internal/project_recipe_owner.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct evo_candidate_buffer {
    char *bytes;
    size_t size;
    size_t capacity;
} evo_candidate_buffer_t;

typedef struct evo_candidate_edit_ref {
    const evo_project_transformation_application_t *application;
} evo_candidate_edit_ref_t;

bool evo_candidate_limits_valid(const evo_project_candidate_limits_t *limits);
char *evo_candidate_duplicate(const char *value);
bool evo_candidate_buffer_open(evo_candidate_buffer_t *buffer, size_t capacity);
void evo_candidate_buffer_close(evo_candidate_buffer_t *buffer);
bool evo_candidate_buffer_append_bytes(
    evo_candidate_buffer_t *buffer,
    const void *bytes,
    size_t count);
bool evo_candidate_buffer_append_text(
    evo_candidate_buffer_t *buffer,
    const char *text);
bool evo_candidate_buffer_append_u64(
    evo_candidate_buffer_t *buffer,
    uint64_t value);
bool evo_candidate_buffer_append_size(
    evo_candidate_buffer_t *buffer,
    size_t value);
bool evo_candidate_buffer_append_json_string(
    evo_candidate_buffer_t *buffer,
    const char *text);
bool evo_candidate_relative_path_valid(const char *path, size_t maximum_bytes);
char *evo_candidate_join_path(const char *left, const char *right);
evo_project_candidate_status_t evo_candidate_validate_output_path(
    const evo_project_candidate_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    char **normalized_output);
evo_project_candidate_status_t evo_candidate_read_snapshot_file(
    const evo_project_baseline_owner_t *baseline_owner,
    const evo_project_file_record_t *record,
    const evo_project_candidate_limits_t *limits,
    unsigned char **bytes,
    size_t *size);
evo_project_candidate_status_t evo_candidate_write_all(
    int file_fd,
    const unsigned char *bytes,
    size_t size);
evo_project_candidate_status_t evo_candidate_open_output_file(
    int workspace_fd,
    const char *path,
    mode_t mode,
    int *file_fd);
evo_project_candidate_status_t evo_candidate_remove_tree(const char *path);

evo_project_candidate_status_t evo_candidate_preflight(
    const evo_project_candidate_config_t *config,
    const evo_project_candidate_t *candidate,
    const evo_project_baseline_owner_t **baseline_owner,
    const evo_project_recipe_owner_t **recipe_owner,
    char **normalized_output);
size_t evo_candidate_collect_file_edits(
    const evo_project_candidate_config_t *config,
    const char *path,
    evo_candidate_edit_ref_t *edits);
evo_project_candidate_status_t evo_candidate_apply_edits(
    const evo_project_candidate_config_t *config,
    const unsigned char *source,
    size_t source_size,
    evo_candidate_edit_ref_t *edits,
    size_t edit_count,
    unsigned char **candidate_bytes,
    size_t *candidate_size);

#endif
