#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_ASYNC_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_ASYNC_H

#include "internal/project_orchestration.h"
#include "internal/project_search.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_ASYNC_EVALUATION_SCHEMA_VERSION 1U

typedef struct evo_project_async_evaluation_slot {
    bool active;
    bool joined;
    bool cancel_requested;
    bool reaped;
    bool result_loaded;
    int process_id;
    int result_descriptor;
    int wait_status;
    evo_project_orchestration_terminal_reason_t terminal_reason;
    char candidate_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    evo_project_orchestration_provider_join_t join;
} evo_project_async_evaluation_slot_t;

typedef struct evo_project_async_evaluation_context {
    evo_project_search_evaluation_provider_fn evaluator;
    void *evaluator_context;
    evo_project_orchestration_provider_capabilities_t capabilities;
    size_t slot_count;
    evo_project_async_evaluation_slot_t *slots;
} evo_project_async_evaluation_context_t;

bool evo_project_async_local_evaluation_provider_init(
    evo_project_async_evaluation_context_t *context,
    evo_project_orchestration_provider_t *provider);

void evo_project_async_local_evaluation_context_destroy(
    evo_project_async_evaluation_context_t *context);

#endif
