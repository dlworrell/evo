#include "internal/project_provider.h"
#include "internal/project_provider_adapters.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

typedef enum test_evaluator_mode {
    TEST_EVALUATOR_ACCEPT = 0,
    TEST_EVALUATOR_REJECT = 1,
    TEST_EVALUATOR_BAD_SCHEMA = 2
} test_evaluator_mode_t;

typedef struct test_evaluator_context {
    test_evaluator_mode_t mode;
    size_t calls;
    bool request_valid;
    char candidate[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
} test_evaluator_context_t;

static void check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project provider test failure: %s\n", message);
        failures += 1;
    }
}

static void check_provider(
    size_t index,
    const char *identity,
    evo_project_provider_kind_t kind,
    uint64_t required_capabilities)
{
    const evo_project_provider_record_t *provider =
        evo_project_provider_registry_at(index);

    check(provider != NULL, "provider registry entry exists");
    if (provider == NULL) {
        return;
    }
    check(
        provider->schema_version == EVO_PROJECT_PROVIDER_REGISTRY_SCHEMA_VERSION,
        "provider registry schema");
    check(strcmp(provider->identity, identity) == 0, "provider identity order");
    check(provider->implementation_version == 1U, "provider version");
    check(provider->kind == kind, "provider kind");
    check(provider->platform != NULL, "provider platform");
    check(provider->runtime_requirement != NULL, "provider requirement");
    check(
        (provider->capabilities & required_capabilities) ==
            required_capabilities,
        "provider capabilities");
    check(evo_project_provider_find(identity) == provider, "provider lookup");
}

static evo_project_search_status_t test_evaluator(
    const evo_project_search_evaluation_request_t *request,
    void *opaque,
    evo_project_search_evaluation_outcome_t *outcome)
{
    test_evaluator_context_t *context = opaque;

    if (request == NULL || context == NULL || outcome == NULL) {
        return EVO_PROJECT_SEARCH_ERROR_PROVIDER;
    }
    context->calls += 1U;
    context->request_valid =
        request->schema_version == EVO_PROJECT_SEARCH_SCHEMA_VERSION &&
        request->generation == 7U && request->population_index == 3U &&
        request->random_seed == UINT64_C(0x1122334455667788) &&
        request->provider_identity != NULL &&
        strcmp(request->provider_identity,
               EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID) == 0 &&
        request->recipe != NULL && request->recipe->private_owner != NULL;

    *outcome = (evo_project_search_evaluation_outcome_t){0};
    outcome->schema_version = context->mode == TEST_EVALUATOR_BAD_SCHEMA
                                  ? EVO_PROJECT_SEARCH_SCHEMA_VERSION + 1U
                                  : EVO_PROJECT_SEARCH_SCHEMA_VERSION;
    if (context->mode == TEST_EVALUATOR_REJECT) {
        return EVO_PROJECT_SEARCH_SUCCESS;
    }
    outcome->accepted = true;
    outcome->correctness_preserved = true;
    outcome->performance_eligible = true;
    outcome->fitness_available = true;
    outcome->candidate_fingerprint = context->candidate;
    outcome->assurance_fingerprint = context->assurance;
    outcome->measurement_fingerprint = context->measurement;
    outcome->fitness.correctness = 1.0;
    outcome->fitness.performance = 0.5;
    outcome->fitness.memory_use = 0.25;
    outcome->fitness.reliability = 1.0;
    outcome->fitness.maintainability = 1.0;
    outcome->fitness.constraint_penalty = 0.0;
    outcome->fitness.total = 3.75;
    return EVO_PROJECT_SEARCH_SUCCESS;
}

static evo_project_orchestration_provider_capabilities_t test_capabilities(void)
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

static evo_project_orchestration_provider_request_t test_provider_request(
    const evo_project_orchestration_provider_t *provider,
    const evo_project_recipe_t *recipe)
{
    return (evo_project_orchestration_provider_request_t){
        .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
        .provider_identity = provider->identity,
        .policy_identity = "production-provider-test-policy-v1",
        .logical_worker_identity = 1U,
        .dispatch_wave = 0U,
        .resources = {
            .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
            .external_worker_count = 1U,
            .cpu_time_ms = UINT64_C(1000),
            .address_space_bytes = UINT64_C(268435456),
            .descendant_process_count = 4U,
            .storage_bytes = UINT64_C(1048576),
            .output_bytes = UINT64_C(65536),
            .wall_timeout_ms = UINT64_C(1000),
            .workspace_bytes = UINT64_C(1048576),
            .require_filesystem_isolation = true,
            .require_network_isolation = true,
            .require_descendant_cleanup = true,
        },
        .candidate = {
            .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
            .generation = 7U,
            .population_index = 3U,
            .recipe_fingerprint = recipe->recipe_fingerprint,
            .workspace_identity = "provider-workspace-7-3",
            .random_seed = UINT64_C(0x1122334455667788),
            .recipe = recipe,
        },
    };
}

static void test_local_evaluation_lifecycle(void)
{
    evo_project_recipe_t recipe = {0};
    int recipe_owner = 1;
    evo_project_local_evaluation_slot_t slots[1] = {{0}};
    test_evaluator_context_t evaluator = {
        .mode = TEST_EVALUATOR_ACCEPT,
        .candidate = "candidate-provider-v1",
        .assurance = "assurance-provider-v1",
        .measurement = "measure-provider-v1",
    };
    evo_project_local_evaluation_context_t context = {
        .evaluator = test_evaluator,
        .evaluator_context = &evaluator,
        .capabilities = test_capabilities(),
        .slot_count = 1U,
        .slots = slots,
    };
    evo_project_orchestration_provider_t provider = {0};
    evo_project_orchestration_provider_request_t request;
    evo_project_orchestration_provider_poll_t poll = {0};
    evo_project_orchestration_provider_join_t join = {0};
    void *handle = NULL;
    void *second_handle = NULL;
    evo_project_orchestration_status_t status;

    recipe.schema_version = EVO_PROJECT_RECIPE_SCHEMA_VERSION;
    recipe.private_owner = &recipe_owner;
    (void)evo_project_format(
        recipe.recipe_fingerprint,
        sizeof(recipe.recipe_fingerprint),
        "%s",
        "recipe-provider-v1");

    check(
        evo_project_local_evaluation_provider_init(&context, &provider),
        "local evaluation provider initializes");
    check(
        provider.identity != NULL &&
            strcmp(provider.identity, EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID) ==
                0 &&
            provider.start != NULL && provider.poll != NULL &&
            provider.cancel != NULL && provider.join != NULL &&
            provider.context == &context,
        "local evaluation provider publishes complete lifecycle callbacks");

    request = test_provider_request(&provider, &recipe);
    status = provider.start(&request, provider.context, &handle);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS && handle != NULL &&
            evaluator.calls == 1U && evaluator.request_valid,
        "local evaluation start invokes the configured evaluator once");
    check(slots[0].active, "local evaluation owns a bounded active slot");

    status = provider.start(&request, provider.context, &second_handle);
    check(
        status == EVO_PROJECT_ORCHESTRATION_ERROR_RESOURCE_LIMIT &&
            second_handle == NULL && evaluator.calls == 1U,
        "local evaluation refuses dispatch beyond its explicit slot bound");

    evaluator.candidate[0] = 'X';
    evaluator.assurance[0] = 'X';
    evaluator.measurement[0] = 'X';
    status = provider.poll(handle, provider.context, &poll);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS && poll.terminal &&
            poll.terminal_reason == EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS,
        "local evaluation poll deterministically publishes success");
    status = provider.join(handle, provider.context, &join);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS && join.cleanup_complete &&
            join.terminal_reason == EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS &&
            strcmp(join.evaluation.candidate_fingerprint,
                   "candidate-provider-v1") == 0 &&
            strcmp(join.evaluation.assurance_fingerprint,
                   "assurance-provider-v1") == 0 &&
            strcmp(join.evaluation.measurement_fingerprint,
                   "measure-provider-v1") == 0 &&
            join.capabilities.filesystem_isolation_enforced &&
            join.capabilities.network_isolation_enforced &&
            join.capabilities.descendant_cleanup_enforced,
        "join publishes deep-copied identities and attested capabilities");
    check(!slots[0].active && slots[0].joined, "joined slot is reusable");

    evaluator.candidate[0] = 'c';
    evaluator.assurance[0] = 'a';
    evaluator.measurement[0] = 'm';
    handle = NULL;
    status = provider.start(&request, provider.context, &handle);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS && handle != NULL,
        "joined slot can be reused by a later dispatch");
    status = provider.cancel(handle, provider.context);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS,
        "local evaluation cancellation latches successfully");
    poll = (evo_project_orchestration_provider_poll_t){0};
    status = provider.poll(handle, provider.context, &poll);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS && poll.terminal &&
            poll.terminal_reason == EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED,
        "canceled local evaluation polls as canceled");
    join = (evo_project_orchestration_provider_join_t){0};
    status = provider.join(handle, provider.context, &join);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS && join.cleanup_complete &&
            join.terminal_reason == EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED,
        "canceled local evaluation joins cleanly without publishing success");

    context.capabilities.network_isolation_enforced = false;
    handle = NULL;
    {
        const size_t calls_before = evaluator.calls;

        status = provider.start(&request, provider.context, &handle);
        check(
            status == EVO_PROJECT_ORCHESTRATION_SUCCESS && handle != NULL &&
                evaluator.calls == calls_before,
            "missing required capability prevents evaluator execution");
    }
    poll = (evo_project_orchestration_provider_poll_t){0};
    status = provider.poll(handle, provider.context, &poll);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS && poll.terminal &&
            poll.terminal_reason ==
                EVO_PROJECT_ORCHESTRATION_TERMINAL_CAPABILITY_UNAVAILABLE,
        "missing required capability has an explicit terminal reason");
    join = (evo_project_orchestration_provider_join_t){0};
    (void)provider.join(handle, provider.context, &join);
    context.capabilities.network_isolation_enforced = true;

    evaluator.mode = TEST_EVALUATOR_REJECT;
    handle = NULL;
    status = provider.start(&request, provider.context, &handle);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS && handle != NULL,
        "rejected candidate still produces a joinable provider handle");
    poll = (evo_project_orchestration_provider_poll_t){0};
    (void)provider.poll(handle, provider.context, &poll);
    check(
        poll.terminal_reason ==
            EVO_PROJECT_ORCHESTRATION_TERMINAL_CANDIDATE_REJECTED,
        "rejected candidate is not misreported as provider failure");
    join = (evo_project_orchestration_provider_join_t){0};
    (void)provider.join(handle, provider.context, &join);

    evaluator.mode = TEST_EVALUATOR_BAD_SCHEMA;
    handle = NULL;
    status = provider.start(&request, provider.context, &handle);
    check(
        status == EVO_PROJECT_ORCHESTRATION_SUCCESS && handle != NULL,
        "malformed evaluator output remains joinable for protocol evidence");
    poll = (evo_project_orchestration_provider_poll_t){0};
    (void)provider.poll(handle, provider.context, &poll);
    check(
        poll.terminal_reason ==
            EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL,
        "malformed evaluator output is a provider-protocol failure");
    join = (evo_project_orchestration_provider_join_t){0};
    (void)provider.join(handle, provider.context, &join);
}

int main(void)
{
    size_t left;
    size_t right;

    check(evo_project_provider_registry_count() == 4U, "registry count");
    check_provider(
        0U,
        EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID,
        EVO_PROJECT_PROVIDER_ANALYSIS,
        EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST |
            EVO_PROJECT_PROVIDER_CAPABILITY_COMPILATION_DATABASE |
            EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV);
    check_provider(
        1U,
        EVO_PROJECT_PROVIDER_CLANG_AST_ID,
        EVO_PROJECT_PROVIDER_TRANSFORMATION_AST,
        EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST |
            EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV);
    check_provider(
        2U,
        EVO_PROJECT_PROVIDER_LINUX_BWRAP_ID,
        EVO_PROJECT_PROVIDER_EXECUTION,
        EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV |
            EVO_PROJECT_PROVIDER_CAPABILITY_CPU_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_ADDRESS_SPACE_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_PROCESS_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_STORAGE_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_OUTPUT_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_WALL_TIMEOUT |
            EVO_PROJECT_PROVIDER_CAPABILITY_FILESYSTEM_ISOLATION |
            EVO_PROJECT_PROVIDER_CAPABILITY_NETWORK_ISOLATION |
            EVO_PROJECT_PROVIDER_CAPABILITY_DESCENDANT_CLEANUP |
            EVO_PROJECT_PROVIDER_CAPABILITY_MEASUREMENT);
    check_provider(
        3U,
        EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID,
        EVO_PROJECT_PROVIDER_EVALUATION,
        EVO_PROJECT_PROVIDER_CAPABILITY_ASYNC_START_POLL_CANCEL_JOIN |
            EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST |
            EVO_PROJECT_PROVIDER_CAPABILITY_MEASUREMENT);

    check(evo_project_provider_registry_at(4U) == NULL, "registry bound");
    check(evo_project_provider_find(NULL) == NULL, "null lookup");
    check(evo_project_provider_find("unknown-provider") == NULL, "unknown lookup");
    check(
        strcmp(evo_project_provider_kind_name(EVO_PROJECT_PROVIDER_ANALYSIS),
               "analysis") == 0,
        "analysis kind name");
    check(
        strcmp(evo_project_provider_kind_name(EVO_PROJECT_PROVIDER_EXECUTION),
               "execution") == 0,
        "execution kind name");

    for (left = 0U; left < evo_project_provider_registry_count(); left += 1U) {
        const evo_project_provider_record_t *left_provider =
            evo_project_provider_registry_at(left);

        for (right = left + 1U;
             right < evo_project_provider_registry_count();
             right += 1U) {
            const evo_project_provider_record_t *right_provider =
                evo_project_provider_registry_at(right);

            check(
                strcmp(left_provider->identity, right_provider->identity) != 0,
                "provider identities unique");
        }
    }

#if !defined(__linux__)
    check(
        !evo_project_provider_available(
            evo_project_provider_find(EVO_PROJECT_PROVIDER_LINUX_BWRAP_ID)),
        "linux provider unavailable off linux");
#endif

    test_local_evaluation_lifecycle();

    if (failures != 0) {
        (void)fprintf(stderr, "%d project provider tests failed\n", failures);
        return 1;
    }
    (void)printf("project provider registry tests passed\n");
    return 0;
}
