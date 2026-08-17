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

typedef struct evo_project_search_orchestration_batch_record {
    uint64_t generation;
    size_t external_worker_count;
    size_t completion_count;
    size_t committed_count;
    size_t first_hard_failure_index;
    bool has_hard_failure;
    bool generation_committed;
    bool cleanup_complete;
    size_t job_count;
    const evo_project_orchestration_job_record_t *jobs;
} evo_project_search_orchestration_batch_record_t;

typedef struct evo_project_search_orchestration_trace {
    uint32_t schema_version;
    const char *policy_identity;
    const char *provider_identity;
    size_t batch_count;
    const evo_project_search_orchestration_batch_record_t *batches;
    size_t job_count;
    bool run_complete;
    bool projection_complete;
    bool probabilistic_authority;
    void *private_owner;
} evo_project_search_orchestration_trace_t;

void evo_project_search_orchestration_trace_destroy(
    evo_project_search_orchestration_trace_t *trace);

evo_project_search_status_t evo_project_search_run_orchestrated(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search);

evo_project_search_status_t evo_project_search_run_orchestrated_with_trace(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search,
    evo_project_search_orchestration_trace_t *trace);

#endif
