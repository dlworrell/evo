#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_ADAPTERS_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_ADAPTERS_H

#include "internal/project_assurance.h"
#include "internal/project_ingestion.h"
#include "internal/project_measurement.h"
#include "internal/project_orchestration.h"
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
    evo_project_sandbox_result_t last_result;
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

/*
 * One caller-owned provider handle. Slots are intentionally explicit rather
 * than allocator-backed because the orchestration callback ABI has no
 * post-join handle destructor. A joined slot remains stable for the duration
 * of the join return and may be reused by a later dispatch wave.
 */
typedef struct evo_project_local_evaluation_slot {
    bool active;
    bool canceled;
    bool joined;
    evo_project_orchestration_terminal_reason_t terminal_reason;
    evo_project_orchestration_provider_capabilities_t capabilities;
    evo_project_search_evaluation_outcome_t evaluation;
    char candidate_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
} evo_project_local_evaluation_slot_t;

/*
 * Adapter from the synchronous source-evaluation contract to the product
 * orchestration start/poll/cancel/join contract. The evaluator is private
 * product plumbing, not a public provider ABI. Its returned result must be
 * fully owned/settled before the callback returns; this adapter deep-copies
 * all committed fingerprint identities into the acquired slot.
 *
 * capabilities are evidence supplied by the concrete evaluation composition.
 * The adapter will not invent missing capabilities and will return a terminal
 * CAPABILITY_UNAVAILABLE result without invoking the evaluator when the
 * requested orchestration policy requires an unattested capability.
 */
typedef struct evo_project_local_evaluation_context {
    evo_project_search_evaluation_provider_fn evaluator;
    void *evaluator_context;
    evo_project_orchestration_provider_capabilities_t capabilities;
    size_t slot_count;
    evo_project_local_evaluation_slot_t *slots;
} evo_project_local_evaluation_context_t;

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

void evo_project_sandbox_assurance_context_destroy(
    evo_project_sandbox_assurance_context_t *context);

evo_project_measurement_status_t evo_project_sandbox_measurement_provider(
    const evo_project_measurement_request_t *request,
    void *context,
    evo_project_measurement_outcome_t *outcome);

bool evo_project_local_evaluation_provider_init(
    evo_project_local_evaluation_context_t *context,
    evo_project_orchestration_provider_t *provider);

#endif
