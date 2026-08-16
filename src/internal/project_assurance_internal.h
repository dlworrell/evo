#ifndef CATALYST_EVO_INTERNAL_PROJECT_ASSURANCE_INTERNAL_H
#define CATALYST_EVO_INTERNAL_PROJECT_ASSURANCE_INTERNAL_H

#include "internal/project_assurance.h"
#include "internal/project_assurance_owner.h"
#include "internal/project_candidate_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

bool evo_assurance_limits_valid(const evo_project_assurance_limits_t *limits);
bool evo_assurance_text_valid(const char *value, size_t maximum_bytes);
bool evo_assurance_diagnostic_valid(
    const char *value,
    size_t byte_count,
    size_t maximum_bytes);
bool evo_assurance_relative_path_valid(const char *path, size_t maximum_bytes);
bool evo_assurance_environment_valid(
    const char *entry,
    size_t maximum_bytes);
bool evo_assurance_executable_valid(
    const char *path,
    size_t maximum_bytes);
char *evo_assurance_duplicate(const char *value);
char *evo_assurance_duplicate_n(const char *value, size_t byte_count);

evo_project_assurance_status_t evo_assurance_validate_output_path(
    const evo_project_assurance_config_t *config,
    char **normalized_output);

evo_project_assurance_status_t evo_assurance_validate_policy(
    const evo_project_assurance_config_t *config,
    uint64_t *policy_fingerprint);

evo_project_assurance_disposition_t evo_assurance_outcome_disposition(
    const evo_project_assurance_gate_outcome_t *outcome);

bool evo_assurance_gate_selected(
    evo_project_assurance_stage_t requested_stage,
    evo_project_assurance_stage_t gate_stage);

bool evo_assurance_build_json(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_owner_t *owner,
    const char *policy_fingerprint,
    const char *assurance_fingerprint,
    evo_candidate_buffer_t *json);

bool evo_assurance_build_markdown(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_owner_t *owner,
    const char *policy_fingerprint,
    const char *assurance_fingerprint,
    evo_candidate_buffer_t *markdown);

#endif
