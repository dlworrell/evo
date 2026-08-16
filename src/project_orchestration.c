#include "internal/project_orchestration.h"

#include "internal/project_orchestration_internal.h"

evo_project_orchestration_status_t evo_project_orchestration_run_batch(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_t *orchestration)
{
    evo_project_orchestration_owner_t *owner = NULL;
    evo_project_orchestration_status_t status;

    if (config == NULL || orchestration == NULL ||
        !evo_project_orchestration_config_valid(config, orchestration)) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_INVALID_ARGUMENT;
    }
    if (orchestration->private_owner != NULL ||
        orchestration->schema_version != 0U) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_RESULT_ACTIVE;
    }
    if (!evo_project_orchestration_allocate_owner(config, &owner)) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_OUT_OF_MEMORY;
    }
    status = evo_project_orchestration_runtime_run(config, owner);
    if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        evo_project_orchestration_owner_destroy(owner);
        return status;
    }
    owner->view.private_owner = owner;
    *orchestration = owner->view;
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

void evo_project_orchestration_destroy(
    evo_project_orchestration_t *orchestration)
{
    evo_project_orchestration_owner_t *owner;

    if (orchestration == NULL) {
        return;
    }
    owner = orchestration->private_owner;
    if (owner != NULL) {
        evo_project_orchestration_owner_destroy(owner);
    }
    *orchestration = (evo_project_orchestration_t){0};
}
