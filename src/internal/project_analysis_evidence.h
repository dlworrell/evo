#ifndef CATALYST_EVO_INTERNAL_PROJECT_ANALYSIS_EVIDENCE_H
#define CATALYST_EVO_INTERNAL_PROJECT_ANALYSIS_EVIDENCE_H

#include "internal/project_analysis_owner.h"
#include "internal/project_baseline_owner.h"

evo_project_analysis_status_t evo_project_analysis_evidence_preflight(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner);

evo_project_analysis_status_t evo_project_analysis_evidence_commit(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    evo_project_analysis_owner_t *owner);

void evo_project_analysis_evidence_discard(
    evo_project_analysis_owner_t *owner);

#endif
