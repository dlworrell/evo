#ifndef CATALYST_EVO_INTERNAL_PROJECT_SEARCH_ORCHESTRATION_TRACE_H
#define CATALYST_EVO_INTERNAL_PROJECT_SEARCH_ORCHESTRATION_TRACE_H

#include "internal/project_search_orchestration.h"

typedef struct evo_project_search_orchestration_trace_owner
    evo_project_search_orchestration_trace_owner_t;

bool evo_project_search_orchestration_trace_owner_create(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *policy,
    evo_project_search_orchestration_trace_owner_t **owner_out);

bool evo_project_search_orchestration_trace_append(
    evo_project_search_orchestration_trace_owner_t *owner,
    const evo_project_orchestration_t *batch);

void evo_project_search_orchestration_trace_publish(
    evo_project_search_orchestration_trace_owner_t *owner,
    bool run_complete,
    evo_project_search_orchestration_trace_t *trace);

void evo_project_search_orchestration_trace_owner_destroy(
    evo_project_search_orchestration_trace_owner_t *owner);

#endif
