#define _POSIX_C_SOURCE 200809L

#include "internal/project_provider.h"
#include "internal/project_provider_async.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "async provider test failure: %s\n", message);
        failures += 1;
    }
}

static evo_project_orchestration_provider_capabilities_t capabilities(void)
{
    return (evo_project_orchestration_provider_capabilities_t){
        .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
        .cpu_limit_enforced = true,
        .address_space_limit_enforced = true,
        .process_limit_enforced = true,
        .storage_limit_enforced = true,
        .output_limit_enforced = true,
        .timeout_enforced = true,
        .filesystem_isolation_enforced = true,
        .network_isolation_enforced = true,
        .descendant_cleanup_enforced = true,
    };
}

static evo_project_orchestration_resource_policy_t resources(void)
{
    return (evo_project_orchestration_resource_policy_t){
        .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
        .external_worker_count = 1U,
        .cpu_time_ms = 1000U,
        .address_space_bytes = 268435456U,
        .descendant_process_count = 8U,
        .storage_bytes = 1048576U,
        .output_bytes = 65536U,
        .wall_timeout_ms = 5000U,
        .workspace_bytes = 1048576U,
        .require_filesystem_isolation = true,
        .require_network_isolation = true,
        .require_descendant_cleanup = true,
    };
}

static evo_project_recipe_t recipe(void)
{
    return (evo_project_recipe_t){
        .schema_version = EVO_PROJECT_RECIPE_SCHEMA_VERSION,
        .record_count = 0U,
        .records = NULL,
    };
}

static evo_project_orchestration_provider_request_t request_for(
    const evo_project_recipe_t *candidate_recipe)
{
    return (evo_project_orchestration_provider_request_t){
        .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
        .provider_identity = EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID,
        .policy_identity = "async-provider-policy-v1",
        .logical_worker_identity = 0U,
        .dispatch_wave = 0U,
        .resources = resources(),
        .candidate = {
            .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
            .generation = 2U,
            .population_index = 3U,
            .recipe_fingerprint = "fnv1a64-v1:0000000000000001",
            .workspace_identity = "async-provider-workspace-0",
            .random_seed = UINT64_C(42),
            .recipe = candidate_recipe,
        },
    };
}

static evo_project_search_status_t immediate_evaluator(
    const evo_project_search_evaluation_request_t *request,
    void *context,
    evo_project_search_evaluation_outcome_t *outcome)
{
    (void)context;
    if (request == NULL || outcome == NULL ||
        request->schema_version != EVO_PROJECT_SEARCH_SCHEMA_VERSION ||
        request->provider_identity == NULL ||
        strcmp(request->provider_identity, EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID) != 0 ||
        request->generation != 2U || request->population_index != 3U ||
        request->random_seed != UINT64_C(42)) {
        return EVO_PROJECT_SEARCH_ERROR_PROVIDER;
    }
    *outcome = (evo_project_search_evaluation_outcome_t){
        .schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION,
        .accepted = true,
        .correctness_preserved = true,
        .performance_eligible = true,
        .fitness_available = true,
        .candidate_fingerprint = "fnv1a64-v1:1111111111111111",
        .assurance_fingerprint = "fnv1a64-v1:2222222222222222",
        .measurement_fingerprint = "fnv1a64-v1:3333333333333333",
    };
    return EVO_PROJECT_SEARCH_SUCCESS;
}

static evo_project_search_status_t blocking_evaluator(
    const evo_project_search_evaluation_request_t *request,
    void *context,
    evo_project_search_evaluation_outcome_t *outcome)
{
    const struct timespec pause = {5, 0};

    (void)request;
    (void)context;
    (void)outcome;
    (void)nanosleep(&pause, NULL);
    return EVO_PROJECT_SEARCH_ERROR_PROVIDER;
}

static bool wait_terminal(
    evo_project_orchestration_provider_t *provider,
    void *handle,
    evo_project_orchestration_provider_poll_t *poll)
{
    const struct timespec pause = {0, 10000000L};
    size_t attempt;

    for (attempt = 0U; attempt < 200U; attempt += 1U) {
        if (provider->poll(handle, provider->context, poll) !=
            EVO_PROJECT_ORCHESTRATION_SUCCESS) {
            return false;
        }
        if (poll->terminal) {
            return true;
        }
        (void)nanosleep(&pause, NULL);
    }
    return false;
}

static void test_success(void)
{
    evo_project_async_evaluation_slot_t slots[2] = {0};
    evo_project_async_evaluation_context_t context = {
        .evaluator = immediate_evaluator,
        .evaluator_context = NULL,
        .capabilities = {0},
        .slot_count = 2U,
        .slots = slots,
    };
    evo_project_orchestration_provider_t provider = {0};
    evo_project_recipe_t candidate_recipe = recipe();
    evo_project_orchestration_provider_request_t request =
        request_for(&candidate_recipe);
    evo_project_orchestration_provider_poll_t poll = {0};
    evo_project_orchestration_provider_join_t join = {0};
    void *handle = NULL;

    context.capabilities = capabilities();
    check(
        evo_project_async_local_evaluation_provider_init(&context, &provider),
        "async provider initializes");
    check(
        provider.start(&request, provider.context, &handle) ==
            EVO_PROJECT_ORCHESTRATION_SUCCESS,
        "async start succeeds");
    check(handle != NULL, "async handle returned");
    check(wait_terminal(&provider, handle, &poll), "successful evaluator becomes terminal");
    check(
        poll.terminal_reason == EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS,
        "successful terminal reason");
    check(
        provider.join(handle, provider.context, &join) ==
            EVO_PROJECT_ORCHESTRATION_SUCCESS,
        "successful join succeeds");
    check(join.cleanup_complete, "successful join cleanup complete");
    check(join.evaluation.accepted, "accepted outcome preserved");
    check(
        join.evaluation.candidate_fingerprint != NULL &&
            strcmp(join.evaluation.candidate_fingerprint,
                   "fnv1a64-v1:1111111111111111") == 0,
        "candidate fingerprint copied into parent ownership");
    evo_project_async_local_evaluation_context_destroy(&context);
}

static void test_running_cancel(void)
{
    evo_project_async_evaluation_slot_t slots[1] = {0};
    evo_project_async_evaluation_context_t context = {
        .evaluator = blocking_evaluator,
        .evaluator_context = NULL,
        .capabilities = {0},
        .slot_count = 1U,
        .slots = slots,
    };
    evo_project_orchestration_provider_t provider = {0};
    evo_project_recipe_t candidate_recipe = recipe();
    evo_project_orchestration_provider_request_t request =
        request_for(&candidate_recipe);
    evo_project_orchestration_provider_poll_t poll = {0};
    evo_project_orchestration_provider_join_t join = {0};
    void *handle = NULL;

    context.capabilities = capabilities();
    check(
        evo_project_async_local_evaluation_provider_init(&context, &provider),
        "cancel provider initializes");
    check(
        provider.start(&request, provider.context, &handle) ==
            EVO_PROJECT_ORCHESTRATION_SUCCESS,
        "blocking evaluator starts asynchronously");
    check(
        provider.poll(handle, provider.context, &poll) ==
            EVO_PROJECT_ORCHESTRATION_SUCCESS,
        "blocking evaluator poll succeeds");
    check(!poll.terminal, "blocking evaluator is live after start returns");
    check(
        provider.cancel(handle, provider.context) ==
            EVO_PROJECT_ORCHESTRATION_SUCCESS,
        "cancel terminates running evaluator process group");
    check(wait_terminal(&provider, handle, &poll), "canceled evaluator becomes terminal");
    check(
        poll.terminal_reason == EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED,
        "cancel terminal reason");
    check(
        provider.join(handle, provider.context, &join) ==
            EVO_PROJECT_ORCHESTRATION_SUCCESS,
        "canceled evaluator joins");
    check(join.cleanup_complete, "canceled evaluator cleanup complete");
    check(
        join.terminal_reason == EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED,
        "canceled join reason");
    evo_project_async_local_evaluation_context_destroy(&context);
}

int main(void)
{
    test_success();
    test_running_cancel();

    if (failures != 0) {
        (void)fprintf(stderr, "%d async provider tests failed\n", failures);
        return 1;
    }
    (void)printf("asynchronous local evaluation provider tests passed\n");
    return 0;
}
