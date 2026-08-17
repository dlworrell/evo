#ifndef CATALYST_EVO_INTERNAL_PROJECT_ORCHESTRATION_OWNER_H
#define CATALYST_EVO_INTERNAL_PROJECT_ORCHESTRATION_OWNER_H

#include "internal/project_orchestration.h"

typedef struct evo_project_orchestration_runtime_job {
    void *provider_handle;
    bool active;
} evo_project_orchestration_runtime_job_t;

typedef struct evo_project_orchestration_owner {
    char *policy_identity;
    char *provider_identity;
    evo_project_orchestration_job_record_t *jobs;
    evo_project_orchestration_runtime_job_t *runtime_jobs;
    size_t job_count;
    evo_project_orchestration_t view;
} evo_project_orchestration_owner_t;

#endif
