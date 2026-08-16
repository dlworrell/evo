#include "internal/project_orchestration.h"
#include "internal/project_runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct fake_handle {
    size_t population_index;
    size_t logical_worker_identity;
    size_t dispatch_wave;
    size_t polls_remaining;
    evo_project_orchestration_terminal_reason_t planned_reason;
    char candidate_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    bool canceled;
    bool joined;
} fake_handle_t;

typedef struct fake_provider {
    fake_handle_t handles[8];
    size_t polls[8];
    evo_project_orchestration_terminal_reason_t reasons[8];
    size_t start_count;
    size_t cancel_count;
    size_t join_count;
    bool network_capability;
} fake_provider_t;

static int test_failures = 0;

static void test_check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(
            stderr, "project orchestration test failure: %s\n", message);
        test_failures += 1;
    }
}

static evo_project_orchestration_status_t fake_start(
    const evo_project_orchestration_provider_request_t *request,
    void *context,
    void **handle)
{
    fake_provider_t *provider = context;
    fake_handle_t *job;
    int written;

    if (request == NULL || provider == NULL || handle == NULL ||
        request->candidate.population_index >= 8U) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    job = &provider->handles[request->candidate.population_index];
    *job = (fake_handle_t){0};
    job->population_index = request->candidate.population_index;
    job->logical_worker_identity = request->logical_worker_identity;
    job->dispatch_wave = request->dispatch_wave;
    job->polls_remaining = provider->polls[job->population_index];
    job->planned_reason = provider->reasons[job->population_index];
    written = evo_project_format(
        job->candidate_fingerprint,
        sizeof(job->candidate_fingerprint),
        "fnv1a64-v1:candidate-%zu",
        job->population_index);
    if (written <= 0 ||
        (size_t)written >= sizeof(job->candidate_fingerprint)) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    written = evo_project_format(
        job->assurance_fingerprint,
        sizeof(job->assurance_fingerprint),
        "fnv1a64-v1:assurance-%zu",
        job->population_index);
    if (written <= 0 ||
        (size_t)written >= sizeof(job->assurance_fingerprint)) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    written = evo_project_format(
        job->measurement_fingerprint,
        sizeof(job->measurement_fingerprint),
        "fnv1a64-v1:measurement-%zu",
        job->population_index);
    if (written <= 0 ||
        (size_t)written >= sizeof(job->measurement_fingerprint)) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    provider->start_count += 1U;
    *handle = job;
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t fake_poll(
    void *handle,
    void *context,
    evo_project_orchestration_provider_poll_t *poll)
{
    fake_handle_t *job = handle;

    (void)context;
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

static evo_project_orchestration_status_t fake_cancel(
    void *handle,
    void *context)
{
    fake_handle_t *job = handle;
    fake_provider_t *provider = context;

    if (job == NULL || provider == NULL) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    if (!job->canceled) {
        job->canceled = true;
        provider->cancel_count += 1U;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t fake_join(
    void *handle,
    void *context,
    evo_project_orchestration_provider_join_t *join)
{
    fake_handle_t *job = handle;
    fake_provider_t *provider = context;

    if (job == NULL || provider == NULL || join == NULL || job->joined) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    job->joined = true;
    provider->join_count += 1U;
    *join = (evo_project_orchestration_provider_join_t){0};
    join->schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    join->terminal_reason =
        job->canceled ? EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED
                      : job->planned_reason;
    join->capabilities.schema_version =
        EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    join->capabilities.cpu_limit_enforced = true;
    join->capabilities.address_space_limit_enforced = true;
    join->capabilities.process_limit_enforced = true;
    join->capabilities.storage_limit_enforced = true;
    join->capabilities.output_limit_enforced = true;
    join->capabilities.timeout_enforced = true;
    join->capabilities.filesystem_isolation_enforced = true;
    join->capabilities.network_isolation_enforced =
        provider->network_capability;
    join->capabilities.descendant_cleanup_enforced = true;
    join->cleanup_complete = true;
    join->evaluation.schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION;
    if (join->terminal_reason ==
        EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS) {
        join->evaluation.accepted = true;
        join->evaluation.correctness_preserved = true;
        join->evaluation.performance_eligible = true;
        join->evaluation.fitness_available = true;
        join->evaluation.candidate_fingerprint = job->candidate_fingerprint;
        join->evaluation.assurance_fingerprint = job->assurance_fingerprint;
        join->evaluation.measurement_fingerprint =
            job->measurement_fingerprint;
        join->evaluation.fitness.correctness = 1.0;
        join->evaluation.fitness.performance =
            4.0 - (double)job->population_index;
        join->evaluation.fitness.reliability = 1.0;
        join->evaluation.fitness.maintainability = 1.0;
        join->evaluation.fitness.total =
            100.0 - (double)job->population_index;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_limits_t test_limits(void)
{
    evo_project_orchestration_limits_t limits = {0};

    limits.max_string_bytes = 128U;
    limits.max_candidates = 8U;
    limits.max_external_workers = 4U;
    limits.max_poll_rounds = 32U;
    limits.max_cpu_time_ms = UINT64_C(10000);
    limits.max_address_space_bytes = UINT64_C(1073741824);
    limits.max_descendant_process_count = 16U;
    limits.max_storage_bytes = UINT64_C(1073741824);
    limits.max_output_bytes = UINT64_C(1048576);
    limits.max_wall_timeout_ms = UINT64_C(10000);
    limits.max_workspace_bytes = UINT64_C(1073741824);
    limits.max_total_bytes = 1048576U;
    return limits;
}

static evo_project_orchestration_resource_policy_t test_resources(
    size_t worker_count)
{
    evo_project_orchestration_resource_policy_t resources = {0};

    resources.schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    resources.external_worker_count = worker_count;
    resources.cpu_time_ms = UINT64_C(1000);
    resources.address_space_bytes = UINT64_C(268435456);
    resources.descendant_process_count = 4U;
    resources.storage_bytes = UINT64_C(1048576);
    resources.output_bytes = UINT64_C(65536);
    resources.wall_timeout_ms = UINT64_C(1000);
    resources.workspace_bytes = UINT64_C(1048576);
    resources.require_filesystem_isolation = true;
    resources.require_network_isolation = true;
    resources.require_descendant_cleanup = true;
    return resources;
}

static evo_project_orchestration_config_t test_config(
    const evo_project_orchestration_candidate_request_t *candidates,
    size_t candidate_count,
    size_t worker_count,
    fake_provider_t *provider)
{
    evo_project_orchestration_config_t config = {0};

    config.policy_identity = "fixture-orchestration-policy-v1";
    config.resources = test_resources(worker_count);
    config.candidate_count = candidate_count;
    config.candidates = candidates;
    config.provider.identity = "fixture-async-provider-v1";
    config.provider.start = fake_start;
    config.provider.poll = fake_poll;
    config.provider.cancel = fake_cancel;
    config.provider.join = fake_join;
    config.provider.context = provider;
    config.limits = test_limits();
    return config;
}

static const evo_project_orchestration_candidate_request_t test_candidates[] = {
    {EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
     3U,
     0U,
     "fnv1a64-v1:recipe-0",
     "generation-3-candidate-0"},
    {EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
     3U,
     1U,
     "fnv1a64-v1:recipe-1",
     "generation-3-candidate-1"},
    {EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
     3U,
     2U,
     "fnv1a64-v1:recipe-2",
     "generation-3-candidate-2"},
    {EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
     3U,
     3U,
     "fnv1a64-v1:recipe-3",
     "generation-3-candidate-3"}};

static void fake_provider_prepare_success(fake_provider_t *provider)
{
    *provider = (fake_provider_t){0};
    provider->polls[0] = 3U;
    provider->polls[1] = 1U;
    provider->polls[2] = 2U;
    provider->polls[3] = 1U;
    provider->reasons[0] = EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS;
    provider->reasons[1] = EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS;
    provider->reasons[2] = EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS;
    provider->reasons[3] = EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS;
    provider->network_capability = true;
}

static void test_parallel_commit_order(void)
{
    fake_provider_t provider;
    evo_project_orchestration_t orchestration = {0};
    evo_project_orchestration_config_t config;
    evo_project_orchestration_status_t status;
    size_t index;

    fake_provider_prepare_success(&provider);
    config = test_config(test_candidates, 4U, 2U, &provider);
    status = evo_project_orchestration_run_batch(&config, &orchestration);
    test_check(status == EVO_PROJECT_ORCHESTRATION_SUCCESS,
               "parallel batch succeeds");
    if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        return;
    }
    test_check(orchestration.generation_committed &&
                   !orchestration.has_hard_failure &&
                   orchestration.committed_count == 4U &&
                   orchestration.completion_count == 4U &&
                   orchestration.cleanup_complete,
               "parallel generation commits atomically");
    test_check(orchestration.jobs[1].completion_ordinal == 0U &&
                   orchestration.jobs[0].completion_ordinal == 1U &&
                   orchestration.jobs[3].completion_ordinal == 2U &&
                   orchestration.jobs[2].completion_ordinal == 3U,
               "runtime completion order is retained diagnostically");
    for (index = 0U; index < orchestration.job_count; index += 1U) {
        const evo_project_orchestration_job_record_t *job =
            &orchestration.jobs[index];

        test_check(job->committed && job->commit_ordinal == index &&
                       job->population_index == index &&
                       job->logical_worker_identity == index % 2U + 1U &&
                       job->dispatch_wave == index / 2U && job->joined &&
                       job->cleanup_complete &&
                       job->terminal_reason ==
                           EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS &&
                       job->evaluation.accepted &&
                       job->evaluation.candidate_fingerprint ==
                           job->candidate_fingerprint,
                   "commit order and copied provider evidence are stable");
    }
    test_check(provider.start_count == 4U && provider.cancel_count == 0U &&
                   provider.join_count == 4U,
               "bounded provider lifecycle completes");
    evo_project_orchestration_destroy(&orchestration);
}

static void test_serial_parallel_logical_equivalence(void)
{
    fake_provider_t serial_provider;
    fake_provider_t parallel_provider;
    evo_project_orchestration_t serial = {0};
    evo_project_orchestration_t parallel = {0};
    evo_project_orchestration_config_t serial_config;
    evo_project_orchestration_config_t parallel_config;
    size_t index;

    fake_provider_prepare_success(&serial_provider);
    fake_provider_prepare_success(&parallel_provider);
    serial_config = test_config(test_candidates, 4U, 1U, &serial_provider);
    parallel_config = test_config(test_candidates, 4U, 4U, &parallel_provider);
    test_check(evo_project_orchestration_run_batch(&serial_config, &serial) ==
                   EVO_PROJECT_ORCHESTRATION_SUCCESS,
               "serial batch succeeds");
    test_check(evo_project_orchestration_run_batch(
                   &parallel_config, &parallel) ==
                   EVO_PROJECT_ORCHESTRATION_SUCCESS,
               "wide parallel batch succeeds");
    if (serial.private_owner == NULL || parallel.private_owner == NULL) {
        evo_project_orchestration_destroy(&parallel);
        evo_project_orchestration_destroy(&serial);
        return;
    }
    test_check(serial.generation_committed && parallel.generation_committed,
               "serial and parallel generations commit");
    for (index = 0U; index < 4U; index += 1U) {
        test_check(serial.jobs[index].population_index ==
                           parallel.jobs[index].population_index &&
                       serial.jobs[index].terminal_reason ==
                           parallel.jobs[index].terminal_reason &&
                       serial.jobs[index].evaluation.fitness.total ==
                           parallel.jobs[index].evaluation.fitness.total &&
                       strcmp(serial.jobs[index].candidate_fingerprint,
                              parallel.jobs[index].candidate_fingerprint) == 0,
                   "worker count does not alter logical candidate outcome");
    }
    evo_project_orchestration_destroy(&parallel);
    evo_project_orchestration_destroy(&serial);
}

static void test_hard_failure_cancels_wave(void)
{
    fake_provider_t provider;
    evo_project_orchestration_t orchestration = {0};
    evo_project_orchestration_config_t config;
    evo_project_orchestration_status_t status;

    fake_provider_prepare_success(&provider);
    provider.polls[0] = 1U;
    provider.polls[1] = 5U;
    provider.reasons[0] = EVO_PROJECT_ORCHESTRATION_TERMINAL_TIMEOUT;
    config = test_config(test_candidates, 4U, 2U, &provider);
    status = evo_project_orchestration_run_batch(&config, &orchestration);
    test_check(status == EVO_PROJECT_ORCHESTRATION_SUCCESS,
               "trustworthy failure produces scheduler evidence");
    if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        return;
    }
    test_check(orchestration.has_hard_failure &&
                   orchestration.first_hard_failure_index == 0U &&
                   !orchestration.generation_committed &&
                   orchestration.committed_count == 0U,
               "hard failure publishes no partial generation");
    test_check(orchestration.jobs[0].terminal_reason ==
                       EVO_PROJECT_ORCHESTRATION_TERMINAL_TIMEOUT &&
                   orchestration.jobs[0].joined &&
                   orchestration.jobs[1].cancel_requested &&
                   orchestration.jobs[1].terminal_reason ==
                       EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED &&
                   orchestration.jobs[1].joined &&
                   !orchestration.jobs[2].started &&
                   !orchestration.jobs[3].started,
               "failure cancels and joins siblings before later starts");
    test_check(provider.start_count == 2U && provider.cancel_count == 1U &&
                   provider.join_count == 2U,
               "provider lifecycle is bounded after failure");
    evo_project_orchestration_destroy(&orchestration);
}

static void test_candidate_rejection_is_committed(void)
{
    fake_provider_t provider;
    evo_project_orchestration_t orchestration = {0};
    evo_project_orchestration_config_t config;

    fake_provider_prepare_success(&provider);
    provider.reasons[2] =
        EVO_PROJECT_ORCHESTRATION_TERMINAL_CANDIDATE_REJECTED;
    config = test_config(test_candidates, 4U, 2U, &provider);
    test_check(evo_project_orchestration_run_batch(&config, &orchestration) ==
                   EVO_PROJECT_ORCHESTRATION_SUCCESS,
               "candidate rejection batch succeeds");
    if (orchestration.private_owner == NULL) {
        return;
    }
    test_check(orchestration.generation_committed &&
                   !orchestration.has_hard_failure &&
                   orchestration.jobs[2].committed &&
                   orchestration.jobs[2].terminal_reason ==
                       EVO_PROJECT_ORCHESTRATION_TERMINAL_CANDIDATE_REJECTED &&
                   !orchestration.jobs[2].evaluation.accepted,
               "candidate rejection remains an exact committed exclusion");
    evo_project_orchestration_destroy(&orchestration);
}

static void test_capability_failure_fails_closed(void)
{
    fake_provider_t provider;
    evo_project_orchestration_t orchestration = {0};
    evo_project_orchestration_config_t config;

    fake_provider_prepare_success(&provider);
    provider.network_capability = false;
    config = test_config(test_candidates, 2U, 2U, &provider);
    test_check(evo_project_orchestration_run_batch(&config, &orchestration) ==
                   EVO_PROJECT_ORCHESTRATION_SUCCESS,
               "capability failure retains trustworthy evidence");
    if (orchestration.private_owner == NULL) {
        return;
    }
    test_check(orchestration.has_hard_failure &&
                   !orchestration.generation_committed &&
                   orchestration.jobs[0].terminal_reason ==
                       EVO_PROJECT_ORCHESTRATION_TERMINAL_CAPABILITY_UNAVAILABLE,
               "required isolation capability fails closed");
    evo_project_orchestration_destroy(&orchestration);
}

static void test_invalid_policy_rejected(void)
{
    fake_provider_t provider;
    evo_project_orchestration_t orchestration = {0};
    evo_project_orchestration_config_t config;

    fake_provider_prepare_success(&provider);
    config = test_config(test_candidates, 4U, 2U, &provider);
    config.resources.external_worker_count = 0U;
    test_check(evo_project_orchestration_run_batch(&config, &orchestration) ==
                   EVO_PROJECT_ORCHESTRATION_ERROR_INVALID_ARGUMENT &&
                   orchestration.private_owner == NULL,
               "zero external worker policy rejects atomically");
}

int main(void)
{
    test_parallel_commit_order();
    test_serial_parallel_logical_equivalence();
    test_hard_failure_cancels_wave();
    test_candidate_rejection_is_committed();
    test_capability_failure_fails_closed();
    test_invalid_policy_rejected();
    if (test_failures != 0) {
        (void)fprintf(stderr,
                      "project orchestration failures: %d\n",
                      test_failures);
        return 1;
    }
    (void)printf("project orchestration tests: PASS\n");
    return 0;
}
