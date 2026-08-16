#ifndef CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_CATALOGUE_H
#define CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_CATALOGUE_H

#include "internal/project_transformation_owner.h"

const evo_project_transformation_catalogue_t *
evo_project_transformation_builtin_recipe_catalogue(void);

const evo_project_transformation_capability_t *
evo_project_transformation_builtin_capabilities(size_t *count);

const evo_project_transformation_capability_t *
evo_project_transformation_find_capability(
    const char *identity,
    uint32_t implementation_version);

bool evo_project_transformation_registry_is_builtin(
    const evo_project_transformation_registry_t *registry);

evo_project_transformation_status_t
evo_project_transformation_catalogue_generate_evidence(
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_registry_owner_t *owner);

#endif
