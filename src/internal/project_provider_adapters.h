#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_ADAPTERS_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_ADAPTERS_H

#include "internal/project_assurance.h"
#include "internal/project_ingestion.h"
#include "internal/project_measurement.h"
#include "internal/project_provider_sandbox.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct evo_project_sandbox_command_context {
    size_t environment_count;
    const char *const *environment;
} evo_project_sandbox_command_context_t;

typedef struct evo_project_sandbox_assurance_context {
    const char *toolchain_identity;
    size_t max_diagnostic_bytes;
} evo_project_sandbox_assurance_context_t;

typedef struct evo_project_measurement_command {
    const char *workload_id;
    const char *baseline_workspace_path;
    const char *candidate_workspace_path;
    const char *working_directory;
    size_t baseline_argument_count;
    const char *const *baseline_arguments;
    size_t candidate_argument_count;
    const char *const *candidate_arguments;
    size_t environment_count;
    const char *const *environment;
    const char *baseline_binary_path;
    const char *candidate_binary_path;
    uint64_t max_memory_bytes;
    size_t max_processes;
    uint64_t max_storage_bytes;
    size_t max_output_bytes;
    bool network_access;
    uint32_t reliability_ppm;
    uint32_t maintainability_ppm;
} evo_project_measurement_command_t;

typedef struct evo_project_sandbox_measurement_context {
    size_t workload_count;
    const evo_project_measurement_command_t *workloads;
} evo_project_sandbox_measurement_context_t;

evo_project_status_t evo_project_sandbox_command_runner(
    const evo_project_command_view_t *command,
    const char *workspace_path,
    void *context,
    evo_project_command_outcome_t *outcome);

evo_project_assurance_status_t evo_project_sandbox_assurance_runner(
    const evo_project_assurance_gate_view_t *gate,
    const char *candidate_workspace_path,
    void *context,
    evo_project_assurance_gate_outcome_t *outcome);

evo_project_measurement_status_t evo_project_sandbox_measurement_provider(
    const evo_project_measurement_request_t *request,
    void *context,
    evo_project_measurement_outcome_t *outcome);

#endif
