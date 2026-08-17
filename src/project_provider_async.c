#define _POSIX_C_SOURCE 200809L

#include "internal/project_provider_async.h"

#include "internal/project_provider.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct evo_project_async_wire_result {
    uint32_t schema_version;
    int32_t evaluator_status;
    uint32_t outcome_schema_version;
    bool accepted;
    bool correctness_preserved;
    bool performance_eligible;
    bool fitness_available;
    evo_fitness_t fitness;
    char candidate_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
} evo_project_async_wire_result_t;

static bool evo_async_fingerprint_valid(const char *fingerprint)
{
    static const char prefix[] = "evo-fnv1a64:";
    size_t index;

    if (fingerprint == NULL) {
        return false;
    }
    for (index = 0U; index + 1U < sizeof(prefix); index += 1U) {
        if (fingerprint[index] != prefix[index]) {
            return false;
        }
    }
    for (index = sizeof(prefix) - 1U;
         index < EVO_PROJECT_FINGERPRINT_TEXT_SIZE - 1U;
         index += 1U) {
        const char byte = fingerprint[index];
        const bool decimal = byte >= '0' && byte <= '9';
        const bool lower_hex = byte >= 'a' && byte <= 'f';

        if (!decimal && !lower_hex) {
            return false;
        }
    }
    return fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE - 1U] == '\0';
}

static bool evo_async_copy_fingerprint(
    char destination[EVO_PROJECT_FINGERPRINT_TEXT_SIZE],
    const char *source)
{
    size_t index;

    if (!evo_async_fingerprint_valid(source)) {
        return false;
    }
    for (index = 0U; index < EVO_PROJECT_FINGERPRINT_TEXT_SIZE; index += 1U) {
        destination[index] = source[index];
    }
    return true;
}

static bool evo_async_capabilities_satisfy(
    const evo_project_orchestration_provider_capabilities_t *capabilities,
    const evo_project_orchestration_resource_policy_t *resources)
{
    if (capabilities == NULL || resources == NULL ||
        capabilities->schema_version != EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION ||
        resources->schema_version != EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION) {
        return false;
    }
    if ((resources->cpu_time_ms > 0U && !capabilities->cpu_limit_enforced) ||
        (resources->address_space_bytes > 0U && !capabilities->address_space_limit_enforced) ||
        (resources->descendant_process_count > 0U && !capabilities->process_limit_enforced) ||
        (resources->storage_bytes > 0U && !capabilities->storage_limit_enforced) ||
        (resources->output_bytes > 0U && !capabilities->output_limit_enforced) ||
        (resources->wall_timeout_ms > 0U && !capabilities->timeout_enforced) ||
        (resources->require_filesystem_isolation &&
         !capabilities->filesystem_isolation_enforced) ||
        (resources->require_network_isolation &&
         !capabilities->network_isolation_enforced) ||
        (resources->require_descendant_cleanup &&
         !capabilities->descendant_cleanup_enforced)) {
        return false;
    }
    return true;
}

static bool evo_async_request_valid(
    const evo_project_orchestration_provider_request_t *request,
    const evo_project_async_evaluation_context_t *context)
{
    return request != NULL && context != NULL && context->evaluator != NULL &&
           context->slots != NULL && context->slot_count > 0U &&
           request->schema_version == EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION &&
           request->provider_identity != NULL &&
           strcmp(request->provider_identity, EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID) == 0 &&
           request->policy_identity != NULL &&
           request->candidate.schema_version == EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION &&
           request->candidate.recipe_fingerprint != NULL &&
           request->candidate.workspace_identity != NULL &&
           request->candidate.recipe != NULL &&
           request->resources.external_worker_count > 0U &&
           request->resources.cpu_time_ms > 0U &&
           request->resources.address_space_bytes > 0U &&
           request->resources.descendant_process_count > 0U &&
           request->resources.storage_bytes > 0U &&
           request->resources.output_bytes > 0U &&
           request->resources.wall_timeout_ms > 0U &&
           request->resources.workspace_bytes > 0U;
}

static evo_project_async_evaluation_slot_t *evo_async_acquire_slot(
    evo_project_async_evaluation_context_t *context)
{
    size_t index;

    for (index = 0U; index < context->slot_count; index += 1U) {
        if (!context->slots[index].active || context->slots[index].joined) {
            return &context->slots[index];
        }
    }
    return NULL;
}

static bool evo_async_write_all(int descriptor, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;
    size_t position = 0U;

    while (position < size) {
        const ssize_t count = write(descriptor, bytes + position, size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        position += (size_t)count;
    }
    return true;
}

static bool evo_async_read_all(int descriptor, void *buffer, size_t size)
{
    unsigned char *bytes = buffer;
    size_t position = 0U;

    while (position < size) {
        const ssize_t count = read(descriptor, bytes + position, size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        position += (size_t)count;
    }
    return true;
}

static bool evo_async_wire_from_outcome(
    evo_project_search_status_t evaluator_status,
    const evo_project_search_evaluation_outcome_t *outcome,
    evo_project_async_wire_result_t *wire)
{
    *wire = (evo_project_async_wire_result_t){0};
    wire->schema_version = EVO_PROJECT_ASYNC_EVALUATION_SCHEMA_VERSION;
    wire->evaluator_status = (int32_t)evaluator_status;
    if (evaluator_status != EVO_PROJECT_SEARCH_SUCCESS || outcome == NULL) {
        return true;
    }
    wire->outcome_schema_version = outcome->schema_version;
    wire->accepted = outcome->accepted;
    wire->correctness_preserved = outcome->correctness_preserved;
    wire->performance_eligible = outcome->performance_eligible;
    wire->fitness_available = outcome->fitness_available;
    wire->fitness = outcome->fitness;
    return evo_async_copy_fingerprint(
               wire->candidate_fingerprint, outcome->candidate_fingerprint) &&
           evo_async_copy_fingerprint(
               wire->assurance_fingerprint, outcome->assurance_fingerprint) &&
           evo_async_copy_fingerprint(
               wire->measurement_fingerprint, outcome->measurement_fingerprint);
}

static void evo_async_child_evaluate(
    int descriptor,
    const evo_project_orchestration_provider_request_t *request,
    evo_project_async_evaluation_context_t *context)
{
    const evo_project_search_evaluation_request_t evaluation_request = {
        .schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION,
        .random_seed = request->candidate.random_seed,
        .generation = request->candidate.generation,
        .population_index = request->candidate.population_index,
        .provider_identity = EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID,
        .recipe = request->candidate.recipe,
    };
    evo_project_search_evaluation_outcome_t evaluation = {0};
    evo_project_async_wire_result_t wire = {0};
    evo_project_search_status_t status;

    (void)setpgid(0, 0);
    status = context->evaluator(
        &evaluation_request, context->evaluator_context, &evaluation);
    if (!evo_async_wire_from_outcome(status, &evaluation, &wire)) {
        wire = (evo_project_async_wire_result_t){0};
        wire.schema_version = EVO_PROJECT_ASYNC_EVALUATION_SCHEMA_VERSION;
        wire.evaluator_status = (int32_t)EVO_PROJECT_SEARCH_ERROR_PROVIDER;
    }
    if (!evo_async_write_all(descriptor, &wire, sizeof(wire))) {
        (void)close(descriptor);
        _exit(126);
    }
    (void)close(descriptor);
    _exit(0);
}

static evo_project_orchestration_status_t evo_async_start(
    const evo_project_orchestration_provider_request_t *request,
    void *opaque,
    void **handle)
{
    evo_project_async_evaluation_context_t *context = opaque;
    evo_project_async_evaluation_slot_t *slot;
    int descriptors[2] = {-1, -1};
    pid_t process;

    if (handle == NULL || !evo_async_request_valid(request, context)) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    slot = evo_async_acquire_slot(context);
    if (slot == NULL) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_RESOURCE_LIMIT;
    }
    *slot = (evo_project_async_evaluation_slot_t){0};
    slot->result_descriptor = -1;
    slot->active = true;
    slot->join.schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    slot->join.capabilities = context->capabilities;
    if (!evo_async_capabilities_satisfy(&context->capabilities, &request->resources)) {
        slot->reaped = true;
        slot->result_loaded = true;
        slot->terminal_reason =
            EVO_PROJECT_ORCHESTRATION_TERMINAL_CAPABILITY_UNAVAILABLE;
        slot->join.terminal_reason = slot->terminal_reason;
        slot->join.cleanup_complete = true;
        *handle = slot;
        return EVO_PROJECT_ORCHESTRATION_SUCCESS;
    }
    if (pipe(descriptors) != 0) {
        slot->active = false;
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    process = fork();
    if (process < 0) {
        (void)close(descriptors[0]);
        (void)close(descriptors[1]);
        slot->active = false;
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    if (process == 0) {
        (void)close(descriptors[0]);
        evo_async_child_evaluate(descriptors[1], request, context);
    }
    (void)close(descriptors[1]);
    (void)setpgid(process, process);
    slot->process_id = (int)process;
    slot->result_descriptor = descriptors[0];
    slot->terminal_reason = EVO_PROJECT_ORCHESTRATION_TERMINAL_NONE;
    *handle = slot;
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static bool evo_async_slot_belongs(
    const evo_project_async_evaluation_context_t *context,
    const evo_project_async_evaluation_slot_t *slot)
{
    size_t index;

    if (context == NULL || slot == NULL || context->slots == NULL) {
        return false;
    }
    for (index = 0U; index < context->slot_count; index += 1U) {
        if (&context->slots[index] == slot) {
            return true;
        }
    }
    return false;
}

static bool evo_async_load_result(evo_project_async_evaluation_slot_t *slot)
{
    evo_project_async_wire_result_t wire = {0};

    if (slot->result_loaded) {
        return true;
    }
    if (slot->cancel_requested) {
        slot->terminal_reason = EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED;
        slot->join.terminal_reason = slot->terminal_reason;
        slot->join.cleanup_complete = slot->reaped;
        slot->result_loaded = true;
        if (slot->result_descriptor >= 0) {
            (void)close(slot->result_descriptor);
            slot->result_descriptor = -1;
        }
        return true;
    }
    if (slot->result_descriptor < 0 ||
        !evo_async_read_all(slot->result_descriptor, &wire, sizeof(wire))) {
        slot->terminal_reason = EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL;
        slot->join.terminal_reason = slot->terminal_reason;
        slot->join.cleanup_complete = slot->reaped;
        slot->result_loaded = true;
        if (slot->result_descriptor >= 0) {
            (void)close(slot->result_descriptor);
            slot->result_descriptor = -1;
        }
        return true;
    }
    (void)close(slot->result_descriptor);
    slot->result_descriptor = -1;
    if (wire.schema_version != EVO_PROJECT_ASYNC_EVALUATION_SCHEMA_VERSION ||
        wire.evaluator_status != (int32_t)EVO_PROJECT_SEARCH_SUCCESS ||
        wire.outcome_schema_version != EVO_PROJECT_SEARCH_SCHEMA_VERSION ||
        !evo_async_copy_fingerprint(
            slot->candidate_fingerprint, wire.candidate_fingerprint) ||
        !evo_async_copy_fingerprint(
            slot->assurance_fingerprint, wire.assurance_fingerprint) ||
        !evo_async_copy_fingerprint(
            slot->measurement_fingerprint, wire.measurement_fingerprint)) {
        slot->terminal_reason = EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL;
    } else {
        slot->join.evaluation = (evo_project_search_evaluation_outcome_t){
            .schema_version = wire.outcome_schema_version,
            .accepted = wire.accepted,
            .correctness_preserved = wire.correctness_preserved,
            .performance_eligible = wire.performance_eligible,
            .fitness_available = wire.fitness_available,
            .candidate_fingerprint = slot->candidate_fingerprint,
            .assurance_fingerprint = slot->assurance_fingerprint,
            .measurement_fingerprint = slot->measurement_fingerprint,
            .fitness = wire.fitness,
        };
        slot->terminal_reason = wire.accepted
                                    ? EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS
                                    : EVO_PROJECT_ORCHESTRATION_TERMINAL_CANDIDATE_REJECTED;
    }
    slot->join.terminal_reason = slot->terminal_reason;
    slot->join.cleanup_complete = slot->reaped;
    slot->result_loaded = true;
    return true;
}

static evo_project_orchestration_status_t evo_async_reap(
    evo_project_async_evaluation_slot_t *slot,
    bool block)
{
    pid_t process;
    int status = 0;
    int options = block ? 0 : WNOHANG;

    if (slot->reaped || slot->process_id <= 0) {
        return EVO_PROJECT_ORCHESTRATION_SUCCESS;
    }
    do {
        process = waitpid((pid_t)slot->process_id, &status, options);
    } while (process < 0 && errno == EINTR);
    if (process == 0) {
        return EVO_PROJECT_ORCHESTRATION_SUCCESS;
    }
    if (process < 0) {
        if (errno == ECHILD) {
            slot->reaped = true;
            slot->wait_status = 0;
            return EVO_PROJECT_ORCHESTRATION_SUCCESS;
        }
        return EVO_PROJECT_ORCHESTRATION_ERROR_CLEANUP;
    }
    slot->reaped = true;
    slot->wait_status = status;
    if (!slot->cancel_requested &&
        (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
        slot->terminal_reason = EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t evo_async_poll(
    void *handle,
    void *opaque,
    evo_project_orchestration_provider_poll_t *poll)
{
    evo_project_async_evaluation_context_t *context = opaque;
    evo_project_async_evaluation_slot_t *slot = handle;
    evo_project_orchestration_status_t status;

    if (!evo_async_slot_belongs(context, slot) || poll == NULL || !slot->active) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    status = evo_async_reap(slot, false);
    if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        return status;
    }
    if (slot->reaped && !slot->result_loaded) {
        (void)evo_async_load_result(slot);
    }
    *poll = (evo_project_orchestration_provider_poll_t){
        .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
        .terminal = slot->reaped && slot->result_loaded,
        .terminal_reason = slot->reaped && slot->result_loaded
                               ? slot->terminal_reason
                               : EVO_PROJECT_ORCHESTRATION_TERMINAL_NONE,
    };
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t evo_async_cancel(
    void *handle,
    void *opaque)
{
    evo_project_async_evaluation_context_t *context = opaque;
    evo_project_async_evaluation_slot_t *slot = handle;

    if (!evo_async_slot_belongs(context, slot) || !slot->active || slot->joined) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    slot->cancel_requested = true;
    if (!slot->reaped && slot->process_id > 0) {
        if (kill(-(pid_t)slot->process_id, SIGKILL) != 0 && errno != ESRCH) {
            return EVO_PROJECT_ORCHESTRATION_ERROR_CLEANUP;
        }
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t evo_async_join(
    void *handle,
    void *opaque,
    evo_project_orchestration_provider_join_t *join)
{
    evo_project_async_evaluation_context_t *context = opaque;
    evo_project_async_evaluation_slot_t *slot = handle;
    evo_project_orchestration_status_t status;

    if (!evo_async_slot_belongs(context, slot) || join == NULL ||
        !slot->active || slot->joined) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    status = evo_async_reap(slot, true);
    if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        return status;
    }
    if (!slot->result_loaded) {
        (void)evo_async_load_result(slot);
    }
    slot->join.cleanup_complete = slot->reaped;
    *join = slot->join;
    slot->joined = true;
    slot->active = false;
    return join->cleanup_complete
               ? EVO_PROJECT_ORCHESTRATION_SUCCESS
               : EVO_PROJECT_ORCHESTRATION_ERROR_CLEANUP;
}

bool evo_project_async_local_evaluation_provider_init(
    evo_project_async_evaluation_context_t *context,
    evo_project_orchestration_provider_t *provider)
{
    size_t index;

    if (context == NULL || provider == NULL || context->evaluator == NULL ||
        context->slots == NULL || context->slot_count == 0U ||
        context->capabilities.schema_version != EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION) {
        return false;
    }
    for (index = 0U; index < context->slot_count; index += 1U) {
        context->slots[index] = (evo_project_async_evaluation_slot_t){0};
        context->slots[index].result_descriptor = -1;
        context->slots[index].joined = true;
    }
    *provider = (evo_project_orchestration_provider_t){
        .identity = EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID,
        .start = evo_async_start,
        .poll = evo_async_poll,
        .cancel = evo_async_cancel,
        .join = evo_async_join,
        .context = context,
    };
    return true;
}

void evo_project_async_local_evaluation_context_destroy(
    evo_project_async_evaluation_context_t *context)
{
    size_t index;

    if (context == NULL || context->slots == NULL) {
        return;
    }
    for (index = 0U; index < context->slot_count; index += 1U) {
        evo_project_async_evaluation_slot_t *slot = &context->slots[index];

        if (slot->active && !slot->reaped && slot->process_id > 0) {
            (void)kill(-(pid_t)slot->process_id, SIGKILL);
            (void)evo_async_reap(slot, true);
        }
        if (slot->result_descriptor >= 0) {
            (void)close(slot->result_descriptor);
            slot->result_descriptor = -1;
        }
        slot->active = false;
        slot->joined = true;
    }
}
