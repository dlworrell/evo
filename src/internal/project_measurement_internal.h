#ifndef CATALYST_EVO_INTERNAL_PROJECT_MEASUREMENT_INTERNAL_H
#define CATALYST_EVO_INTERNAL_PROJECT_MEASUREMENT_INTERNAL_H

#include "internal/project_candidate_internal.h"
#include "internal/project_measurement.h"
#include "internal/project_measurement_owner.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool evo_measurement_config_valid(const evo_project_measurement_config_t *config);
char *evo_measurement_duplicate(const char *value);
uint64_t evo_measurement_policy_fingerprint(const evo_project_measurement_config_t *config);
uint64_t evo_measurement_condition_fingerprint(const evo_project_measurement_config_t *config);
uint64_t evo_measurement_result_fingerprint(const evo_project_measurement_owner_t *owner);

bool evo_measurement_prepare_owner(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner);
void evo_measurement_release_owner(evo_project_measurement_owner_t *owner);

bool evo_measurement_record_sample(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner,
    const evo_project_measurement_workload_policy_t *policy,
    evo_project_measurement_subject_t subject,
    evo_project_measurement_phase_t phase,
    size_t pair_index,
    size_t sequence_index,
    const evo_project_measurement_outcome_t *outcome);

bool evo_measurement_finalize_workloads(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner);
bool evo_measurement_build_evidence(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_owner_t *owner);
evo_project_measurement_status_t evo_measurement_publish_evidence(
    const evo_project_measurement_owner_t *owner);

#endif
