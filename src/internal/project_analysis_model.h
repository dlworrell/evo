#ifndef CATALYST_EVO_INTERNAL_PROJECT_ANALYSIS_MODEL_H
#define CATALYST_EVO_INTERNAL_PROJECT_ANALYSIS_MODEL_H

#include "internal/project_analysis_owner.h"
#include "internal/project_baseline_owner.h"

evo_project_analysis_status_t evo_project_analysis_model_build(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    const evo_project_analysis_provider_result_t *provider_result,
    evo_project_analysis_owner_t *owner);

void evo_project_analysis_model_destroy(
    evo_project_analysis_owner_t *owner);

#endif
