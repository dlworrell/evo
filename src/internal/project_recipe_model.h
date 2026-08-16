#ifndef CATALYST_EVO_INTERNAL_PROJECT_RECIPE_MODEL_H
#define CATALYST_EVO_INTERNAL_PROJECT_RECIPE_MODEL_H

#include "internal/project_baseline_owner.h"
#include "internal/project_recipe_owner.h"

evo_project_recipe_status_t evo_project_recipe_model_build(
    const evo_project_recipe_context_t *context,
    const evo_project_baseline_owner_t *baseline_owner,
    const evo_project_recipe_proposal_record_t *proposals,
    size_t proposal_count,
    evo_project_recipe_owner_t *owner);

void evo_project_recipe_model_destroy(evo_project_recipe_owner_t *owner);

#endif
