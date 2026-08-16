#ifndef CATALYST_EVO_INTERNAL_PROJECT_RECIPE_ENCODING_H
#define CATALYST_EVO_INTERNAL_PROJECT_RECIPE_ENCODING_H

#include "internal/project_recipe_model.h"

evo_project_recipe_status_t evo_project_recipe_encoding_finish(
    const evo_project_recipe_context_t *context,
    const evo_project_baseline_owner_t *baseline_owner,
    size_t genome_size,
    evo_project_recipe_owner_t *owner);

evo_project_recipe_status_t evo_project_recipe_encoding_decode(
    const evo_project_recipe_context_t *context,
    const evo_project_baseline_owner_t *baseline_owner,
    const unsigned char *genome,
    size_t genome_size,
    evo_project_recipe_owner_t *owner);

#endif
