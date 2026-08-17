from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'{label}: expected text not found')
    return text.replace(old, new, 1)


path = Path('tests/project_search_test.c')
text = path.read_text()
if '#include "internal/project_search_orchestration.h"' not in text:
    text = replace_once(
        text,
        '#include "internal/project_search_internal.h"\n',
        '#include "internal/project_search_internal.h"\n#include "internal/project_search_orchestration.h"\n',
        'orchestration test include')

provider_marker = 'static evo_project_search_config_t test_search_config(\n'
if 'typedef struct test_async_handle' not in text:
    if provider_marker not in text:
        raise SystemExit('async provider insertion marker missing')
    block = r'''typedef struct test_async_handle {
    size_t generation;
    size_t population_index;
    size_t polls_remaining;
    evo_project_orchestration_terminal_reason_t planned_reason;
    evo_project_search_evaluation_outcome_t outcome;
    char candidate[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    bool canceled;
    bool joined;
} test_async_handle_t;

typedef struct test_async_provider_context {
    test_provider_context_t evaluator;
    test_async_handle_t handles[8];
    size_t start_count;
    size_t cancel_count;
    size_t join_count;
    bool saw_live_recipe;
    bool force_timeout;
    size_t timeout_generation;
    size_t timeout_population_index;
} test_async_provider_context_t;

static bool test_copy_fingerprint(
    char output[EVO_PROJECT_FINGERPRINT_TEXT_SIZE],
    const char *input)
{
    size_t index = 0U;

    if (input == NULL) {
        return false;
    }
    while (index < EVO_PROJECT_FINGERPRINT_TEXT_SIZE && input[index] != '\0') {
        output[index] = input[index];
        index += 1U;
    }
    if (index == 0U || index >= EVO_PROJECT_FINGERPRINT_TEXT_SIZE) {
        return false;
    }
    output[index] = '\0';
    return true;
}

static evo_project_orchestration_status_t test_async_start(
    const evo_project_orchestration_provider_request_t *request,
    void *opaque,
    void **handle)
{
    test_async_provider_context_t *context = opaque;
    test_async_handle_t *job;
    evo_project_search_evaluation_request_t evaluation_request = {0};
    evo_project_search_evaluation_outcome_t outcome = {0};

    if (request == NULL || context == NULL || handle == NULL ||
        request->candidate.recipe == NULL ||
        request->candidate.recipe->private_owner == NULL ||
        request->candidate.population_index >= 8U ||
        strcmp(request->candidate.recipe_fingerprint,
               request->candidate.recipe->recipe_fingerprint) != 0) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    context->saw_live_recipe = true;
    job = &context->handles[request->candidate.population_index];
    *job = (test_async_handle_t){0};
    job->generation = request->candidate.generation;
    job->population_index = request->candidate.population_index;
    job->polls_remaining =
        1U + ((request->candidate.population_index + request->candidate.generation) % 3U);
    job->planned_reason =
        context->force_timeout &&
                request->candidate.generation == context->timeout_generation &&
                request->candidate.population_index ==
                    context->timeout_population_index
            ? EVO_PROJECT_ORCHESTRATION_TERMINAL_TIMEOUT
            : EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS;
    evaluation_request.schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION;
    evaluation_request.random_seed = request->candidate.random_seed;
    evaluation_request.generation = request->candidate.generation;
    evaluation_request.population_index = request->candidate.population_index;
    evaluation_request.provider_identity = "deterministic-provider-v1";
    evaluation_request.recipe = request->candidate.recipe;
    if (job->planned_reason == EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS) {
        if (test_provider(
                &evaluation_request, &context->evaluator, &outcome) !=
                EVO_PROJECT_SEARCH_SUCCESS ||
            !test_copy_fingerprint(job->candidate, outcome.candidate_fingerprint) ||
            !test_copy_fingerprint(job->assurance, outcome.assurance_fingerprint) ||
            !test_copy_fingerprint(
                job->measurement, outcome.measurement_fingerprint)) {
            return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
        }
        job->outcome = outcome;
        job->outcome.candidate_fingerprint = job->candidate;
        job->outcome.assurance_fingerprint = job->assurance;
        job->outcome.measurement_fingerprint = job->measurement;
    }
    context->start_count += 1U;
    *handle = job;
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t test_async_poll(
    void *handle,
    void *opaque,
    evo_project_orchestration_provider_poll_t *poll)
{
    test_async_handle_t *job = handle;

    (void)opaque;
    if (job == NULL || poll == NULL) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    *poll = (evo_project_orchestration_provider_poll_t){0};
    poll->schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    if (job->canceled) {
        poll->terminal = true;
        poll->terminal_reason = EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED;
        return EVO_PROJECT_ORCHESTRATION_SUCCESS;
    }
    if (job->polls_remaining > 0U) {
        job->polls_remaining -= 1U;
    }
    if (job->polls_remaining == 0U) {
        poll->terminal = true;
        poll->terminal_reason = job->planned_reason;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t test_async_cancel(
    void *handle,
    void *opaque)
{
    test_async_handle_t *job = handle;
    test_async_provider_context_t *context = opaque;

    if (job == NULL || context == NULL) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    if (!job->canceled) {
        job->canceled = true;
        context->cancel_count += 1U;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t test_async_join(
    void *handle,
    void *opaque,
    evo_project_orchestration_provider_join_t *join)
{
    test_async_handle_t *job = handle;
    test_async_provider_context_t *context = opaque;

    if (job == NULL || context == NULL || join == NULL || job->joined) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    job->joined = true;
    context->join_count += 1U;
    *join = (evo_project_orchestration_provider_join_t){0};
    join->schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    join->terminal_reason =
        job->canceled ? EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED
                      : job->planned_reason;
    join->capabilities.schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    join->capabilities.cpu_limit_enforced = true;
    join->capabilities.address_space_limit_enforced = true;
    join->capabilities.process_limit_enforced = true;
    join->capabilities.storage_limit_enforced = true;
    join->capabilities.output_limit_enforced = true;
    join->capabilities.timeout_enforced = true;
    join->capabilities.filesystem_isolation_enforced = true;
    join->capabilities.network_isolation_enforced = true;
    join->capabilities.descendant_cleanup_enforced = true;
    join->cleanup_complete = true;
    if (join->terminal_reason ==
        EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS) {
        join->evaluation = job->outcome;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_search_orchestration_policy_t test_orchestration_policy(
    size_t worker_count,
    test_async_provider_context_t *provider)
{
    return (evo_project_search_orchestration_policy_t){
        .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
        .identity = "structured-search-orchestration-v1",
        .resources = {
            .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
            .external_worker_count = worker_count,
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
        .provider = {
            .identity = "structured-search-async-provider-v1",
            .start = test_async_start,
            .poll = test_async_poll,
            .cancel = test_async_cancel,
            .join = test_async_join,
            .context = provider,
        },
        .limits = {
            .max_string_bytes = 256U,
            .max_candidates = 8U,
            .max_external_workers = 4U,
            .max_poll_rounds = 32U,
            .max_cpu_time_ms = UINT64_C(10000),
            .max_address_space_bytes = UINT64_C(1073741824),
            .max_descendant_process_count = 16U,
            .max_storage_bytes = UINT64_C(1073741824),
            .max_output_bytes = UINT64_C(1048576),
            .max_wall_timeout_ms = UINT64_C(10000),
            .max_workspace_bytes = UINT64_C(1073741824),
            .max_total_bytes = 1048576U,
        },
    };
}

'''
    text = text.replace(provider_marker, block + provider_marker, 1)

main_marker = 'int main(void)\n'
if 'static void test_orchestrated_search_equivalence(' not in text:
    if main_marker not in text:
        raise SystemExit('main insertion marker missing')
    tests = r'''static void test_orchestrated_search_equivalence(test_fixture_t *fixture)
{
    test_provider_context_t serial_placeholder = {0};
    test_provider_context_t parallel_placeholder = {0};
    test_async_provider_context_t serial_provider = {0};
    test_async_provider_context_t parallel_provider = {0};
    evo_project_search_config_t serial_config =
        test_search_config(fixture, &serial_placeholder);
    evo_project_search_config_t parallel_config =
        test_search_config(fixture, &parallel_placeholder);
    evo_project_search_orchestration_policy_t serial_policy =
        test_orchestration_policy(1U, &serial_provider);
    evo_project_search_orchestration_policy_t parallel_policy =
        test_orchestration_policy(4U, &parallel_provider);
    evo_project_search_t serial = {0};
    evo_project_search_t parallel = {0};

    test_check(
        evo_project_search_run_orchestrated(
            &serial_config, &serial_policy, &serial) ==
            EVO_PROJECT_SEARCH_SUCCESS,
        "serial orchestrated structured search succeeds");
    test_check(
        evo_project_search_run_orchestrated(
            &parallel_config, &parallel_policy, &parallel) ==
            EVO_PROJECT_SEARCH_SUCCESS,
        "parallel orchestrated structured search succeeds");
    if (serial.private_owner == NULL || parallel.private_owner == NULL) {
        evo_project_search_destroy(&parallel);
        evo_project_search_destroy(&serial);
        return;
    }
    test_check(
        serial_provider.saw_live_recipe && parallel_provider.saw_live_recipe &&
            serial_provider.start_count == serial_provider.join_count &&
            parallel_provider.start_count == parallel_provider.join_count &&
            serial_provider.cancel_count == 0U &&
            parallel_provider.cancel_count == 0U,
        "orchestration provider sees live recipes and joins every start");
    test_check(
        strcmp(serial.search_fingerprint, parallel.search_fingerprint) == 0 &&
            strcmp(serial.best_recipe_fingerprint,
                   parallel.best_recipe_fingerprint) == 0 &&
            serial.termination_reason == parallel.termination_reason &&
            serial.best_fitness.total == parallel.best_fitness.total &&
            serial.lineage_count == parallel.lineage_count &&
            serial.operator_event_count == parallel.operator_event_count &&
            strcmp(serial.canonical_json, parallel.canonical_json) == 0,
        "external worker count preserves complete logical search authority");
    evo_project_search_destroy(&parallel);
    evo_project_search_destroy(&serial);
}

static void test_orchestrated_failure_is_atomic(test_fixture_t *fixture)
{
    test_provider_context_t placeholder = {0};
    test_async_provider_context_t provider = {
        .force_timeout = true,
        .timeout_generation = 0U,
        .timeout_population_index = 0U,
    };
    evo_project_search_config_t config =
        test_search_config(fixture, &placeholder);
    evo_project_search_orchestration_policy_t policy =
        test_orchestration_policy(2U, &provider);
    evo_project_search_t search = {0};

    test_check(
        evo_project_search_run_orchestrated(&config, &policy, &search) !=
                EVO_PROJECT_SEARCH_SUCCESS &&
            search.private_owner == NULL,
        "hard external worker failure publishes no partial search result");
    test_check(
        provider.start_count > 0U &&
            provider.start_count == provider.join_count &&
            provider.cancel_count > 0U,
        "hard external worker failure cancels and joins started siblings");
}

'''
    text = text.replace(main_marker, tests + main_marker, 1)

old_main_calls = '''        test_structured_operators(&fixture);\n        test_search_replay_and_improvement(&fixture);\n        test_exact_tie_is_stable(&fixture);\n'''
new_main_calls = '''        test_structured_operators(&fixture);\n        test_search_replay_and_improvement(&fixture);\n        test_exact_tie_is_stable(&fixture);\n        test_orchestrated_search_equivalence(&fixture);\n        test_orchestrated_failure_is_atomic(&fixture);\n'''
if 'test_orchestrated_search_equivalence(&fixture);' not in text:
    text = replace_once(text, old_main_calls, new_main_calls, 'main orchestrated calls')

path.write_text(text)
