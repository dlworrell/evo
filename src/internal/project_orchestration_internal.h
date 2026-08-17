#ifndef CATALYST_EVO_INTERNAL_PROJECT_ORCHESTRATION_INTERNAL_H
#define CATALYST_EVO_INTERNAL_PROJECT_ORCHESTRATION_INTERNAL_H

#include "internal/project_orchestration_owner.h"

bool evo_project_orchestration_config_valid(
    const evo_project_orchestration_config_t *config,
    const evo_project_orchestration_t *orchestration);

bool evo_project_orchestration_allocate_owner(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t **owner_out);

void evo_project_orchestration_owner_destroy(
    evo_project_orchestration_owner_t *owner);

bool evo_project_orchestration_terminal_is_hard_failure(
    evo_project_orchestration_terminal_reason_t reason);

bool evo_project_orchestration_capabilities_satisfy(
    const evo_project_orchestration_resource_policy_t *resources,
    const evo_project_orchestration_provider_capabilities_t *capabilities);

evo_project_orchestration_status_t evo_project_orchestration_runtime_run(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t *owner);

#endif
