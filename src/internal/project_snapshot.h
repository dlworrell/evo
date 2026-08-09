#ifndef CATALYST_EVO_INTERNAL_PROJECT_SNAPSHOT_H
#define CATALYST_EVO_INTERNAL_PROJECT_SNAPSHOT_H

#include "internal/project_baseline_owner.h"

evo_project_status_t evo_project_snapshot_prepare(
    const evo_project_capture_config_t *config,
    evo_project_baseline_owner_t *owner);

evo_project_status_t evo_project_snapshot_verify_source(
    const evo_project_baseline_owner_t *owner);

evo_project_status_t evo_project_snapshot_verify_baseline(
    const evo_project_baseline_owner_t *owner);

evo_project_status_t evo_project_snapshot_remove_workspace(
    evo_project_baseline_owner_t *owner);

evo_project_status_t evo_project_snapshot_commit(
    evo_project_baseline_owner_t *owner);

void evo_project_snapshot_discard(evo_project_baseline_owner_t *owner);

#endif
