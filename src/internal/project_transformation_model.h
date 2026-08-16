#ifndef CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_MODEL_H
#define CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_MODEL_H

#include "internal/project_transformation_catalogue.h"

evo_project_transformation_status_t evo_project_transformation_model_apply(
    const evo_project_recipe_record_t *record,
    const evo_project_transformation_capability_t *capability,
    const unsigned char *source,
    size_t source_size,
    const evo_project_transformation_ast_result_t *ast,
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_application_owner_t *owner);

void evo_project_transformation_application_owner_destroy(
    evo_project_transformation_application_owner_t *owner);

#endif
