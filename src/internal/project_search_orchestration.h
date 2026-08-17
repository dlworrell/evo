#ifndef CATALYST_EVO_INTERNAL_PROJECT_SEARCH_ORCHESTRATION_H
#define CATALYST_EVO_INTERNAL_PROJECT_SEARCH_ORCHESTRATION_H

#include "internal/project_orchestration.h"
#include "internal/project_search.h"

typedef struct evo_project_search_orchestration_policy {
    uint32_t schema_version;
    const char *identity;
    evo_project_orchestration_resource_policy_t resources;
    evo_project_orchestration_provider_t provider;
    evo_project_orchestration_limits_t limits;
} evo_project_search_orchestration_policy_t;

evo_project_search_status_t evo_project_search_run_orchestrated(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search);

#endif
