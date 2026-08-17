#include "internal/project_orchestration_internal.h"

#include "internal/project_search_internal.h"

static void evo_orchestration_latch_failure(
    evo_project_orchestration_owner_t *owner,
    size_t index)
{
    if (!owner->view.has_hard_failure) {
        owner->view.has_hard_failure = true;
        owner->view.first_hard_failure_index = index;
    }
}

static void evo_orchestration_mark_terminal(
    evo_project_orchestration_owner_t *owner,
    size_t index,
    evo_project_orchestration_terminal_reason_t reason)
{
    evo_project_orchestration_job_record_t *job = &owner->jobs[index];

    if (job->terminal) {
        return;
    }
    job->terminal = true;
    job->state = EVO_PROJECT_ORCHESTRATION_JOB_TERMINAL;
    job->terminal_reason = reason;
    job->completion_ordinal = owner->view.completion_count;
    owner->view.completion_count += 1U;
    if (evo_project_orchestration_terminal_is_hard_failure(reason)) {
        evo_orchestration_latch_failure(owner, index);
    }
}

static bool evo_orchestration_copy_outcome_identity(
    char destination[EVO_PROJECT_FINGERPRINT_TEXT_SIZE],
    const char *source)
{
    if (source == NULL) {
        destination[0] = '\0';
        return false;
    }
    return evo_search_copy_text(
        destination, EVO_PROJECT_FINGERPRINT_TEXT_SIZE, source);
}

static bool evo_orchestration_copy_evaluation(
    const evo_project_search_evaluation_outcome_t *source,
    evo_project_orchestration_job_record_t *job)
{
    if (source == NULL ||
        source->schema_version != EVO_PROJECT_SEARCH_SCHEMA_VERSION ||
        !source->accepted || !source->correctness_preserved ||
        !source->performance_eligible || !source->fitness_available ||
        !evo_search_fitness_valid(&source->fitness) ||
        !evo_orchestration_copy_outcome_identity(
            job->candidate_fingerprint, source->candidate_fingerprint) ||
        !evo_orchestration_copy_outcome_identity(
            job->assurance_fingerprint, source->assurance_fingerprint) ||
        !evo_orchestration_copy_outcome_identity(
            job->measurement_fingerprint,
            source->measurement_fingerprint)) {
        return false;
    }
    job->evaluation = *source;
    job->evaluation.candidate_fingerprint = job->candidate_fingerprint;
    job->evaluation.assurance_fingerprint = job->assurance_fingerprint;
    job->evaluation.measurement_fingerprint = job->measurement_fingerprint;
    return true;
}

static void evo_orchestration_prepare_job(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t *owner,
    size_t index)
{
    evo_project_orchestration_job_record_t *job = &owner->jobs[index];

    *job = (evo_project_orchestration_job_record_t){0};
    job->generation = config->candidates[index].generation;
    job->population_index = config->candidates[index].population_index;
    job->logical_worker_identity =
        index % config->resources.external_worker_count + 1U;
    job->dispatch_wave =
        index / config->resources.external_worker_count;
}

static evo_project_orchestration_status_t evo_orchestration_start_job(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t *owner,
    size_t index)
{
    evo_project_orchestration_job_record_t *job = &owner->jobs[index];
    evo_project_orchestration_runtime_job_t *runtime =
        &owner->runtime_jobs[index];
    evo_project_orchestration_provider_request_t request = {0};
    evo_project_orchestration_status_t status;

    job->admitted = true;
    job->state = EVO_PROJECT_ORCHESTRATION_JOB_ADMITTED;
    request.schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    request.provider_identity = config->provider.identity;
    request.policy_identity = config->policy_identity;
    request.logical_worker_identity = job->logical_worker_identity;
    request.dispatch_wave = job->dispatch_wave;
    request.resources = config->resources;
    request.candidate = config->candidates[index];
    status = config->provider.start(
        &request, config->provider.context, &runtime->provider_handle);
    if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS ||
        runtime->provider_handle == NULL) {
        runtime->provider_handle = NULL;
        runtime->active = false;
        evo_orchestration_mark_terminal(
            owner,
            index,
            EVO_PROJECT_ORCHESTRATION_TERMINAL_START_FAILED);
        return EVO_PROJECT_ORCHESTRATION_SUCCESS;
    }
    runtime->active = true;
    job->started = true;
    job->state = EVO_PROJECT_ORCHESTRATION_JOB_STARTED;
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t evo_orchestration_cancel_active(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t *owner,
    size_t wave_start,
    size_t wave_end,
    size_t failure_index)
{
    size_t index;

    for (index = wave_start; index < wave_end; index += 1U) {
        evo_project_orchestration_job_record_t *job = &owner->jobs[index];
        evo_project_orchestration_runtime_job_t *runtime =
            &owner->runtime_jobs[index];

        if (index == failure_index || !runtime->active || job->terminal ||
            job->cancel_requested) {
            continue;
        }
        if (config->provider.cancel(
                runtime->provider_handle,
                config->provider.context) !=
            EVO_PROJECT_ORCHESTRATION_SUCCESS) {
            return EVO_PROJECT_ORCHESTRATION_ERROR_CLEANUP;
        }
        job->cancel_requested = true;
        job->state = EVO_PROJECT_ORCHESTRATION_JOB_CANCEL_REQUESTED;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static bool evo_orchestration_wave_terminal(
    const evo_project_orchestration_owner_t *owner,
    size_t wave_start,
    size_t wave_end)
{
    size_t index;

    for (index = wave_start; index < wave_end; index += 1U) {
        const evo_project_orchestration_job_record_t *job =
            &owner->jobs[index];

        if (job->started && !job->terminal) {
            return false;
        }
    }
    return true;
}

static evo_project_orchestration_status_t evo_orchestration_poll_wave(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t *owner,
    size_t wave_start,
    size_t wave_end)
{
    size_t round = 0U;
    bool cancellation_issued = false;

    while (!evo_orchestration_wave_terminal(owner, wave_start, wave_end)) {
        size_t index;

        if (round >= config->limits.max_poll_rounds) {
            for (index = wave_start; index < wave_end; index += 1U) {
                if (owner->jobs[index].started &&
                    !owner->jobs[index].terminal) {
                    evo_orchestration_mark_terminal(
                        owner,
                        index,
                        EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL);
                }
            }
            break;
        }
        round += 1U;
        for (index = wave_start; index < wave_end; index += 1U) {
            evo_project_orchestration_job_record_t *job = &owner->jobs[index];
            evo_project_orchestration_runtime_job_t *runtime =
                &owner->runtime_jobs[index];
            evo_project_orchestration_provider_poll_t poll = {0};
            evo_project_orchestration_status_t status;

            if (!runtime->active || job->terminal) {
                continue;
            }
            status = config->provider.poll(
                runtime->provider_handle,
                config->provider.context,
                &poll);
            if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS ||
                poll.schema_version !=
                    EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION) {
                evo_orchestration_mark_terminal(
                    owner,
                    index,
                    EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL);
                continue;
            }
            if (poll.terminal) {
                if (poll.terminal_reason ==
                    EVO_PROJECT_ORCHESTRATION_TERMINAL_NONE) {
                    evo_orchestration_mark_terminal(
                        owner,
                        index,
                        EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL);
                } else {
                    evo_orchestration_mark_terminal(
                        owner, index, poll.terminal_reason);
                }
            } else if (poll.terminal_reason !=
                       EVO_PROJECT_ORCHESTRATION_TERMINAL_NONE) {
                evo_orchestration_mark_terminal(
                    owner,
                    index,
                    EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL);
            }
        }
        if (owner->view.has_hard_failure && !cancellation_issued) {
            const evo_project_orchestration_status_t status =
                evo_orchestration_cancel_active(
                    config,
                    owner,
                    wave_start,
                    wave_end,
                    owner->view.first_hard_failure_index);

            if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
                return status;
            }
            cancellation_issued = true;
        }
    }
    if (owner->view.has_hard_failure && !cancellation_issued) {
        return evo_orchestration_cancel_active(
            config,
            owner,
            wave_start,
            wave_end,
            owner->view.first_hard_failure_index);
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t evo_orchestration_join_job(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t *owner,
    size_t index)
{
    evo_project_orchestration_job_record_t *job = &owner->jobs[index];
    evo_project_orchestration_runtime_job_t *runtime =
        &owner->runtime_jobs[index];
    evo_project_orchestration_provider_join_t join = {0};
    evo_project_orchestration_status_t status;

    if (!job->started) {
        return EVO_PROJECT_ORCHESTRATION_SUCCESS;
    }
    status = config->provider.join(
        runtime->provider_handle, config->provider.context, &join);
    runtime->provider_handle = NULL;
    runtime->active = false;
    if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS ||
        join.schema_version != EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION ||
        !join.cleanup_complete) {
        job->terminal_reason =
            EVO_PROJECT_ORCHESTRATION_TERMINAL_CLEANUP_FAILED;
        job->cleanup_complete = false;
        owner->view.cleanup_complete = false;
        return EVO_PROJECT_ORCHESTRATION_ERROR_CLEANUP;
    }
    if (!job->terminal) {
        evo_orchestration_mark_terminal(owner, index, join.terminal_reason);
    } else if (join.terminal_reason != job->terminal_reason &&
               !(job->cancel_requested &&
                 join.terminal_reason ==
                     EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED)) {
        job->terminal_reason =
            EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL;
        evo_orchestration_latch_failure(owner, index);
    }
    if (!evo_project_orchestration_capabilities_satisfy(
            &config->resources, &join.capabilities)) {
        job->terminal_reason =
            EVO_PROJECT_ORCHESTRATION_TERMINAL_CAPABILITY_UNAVAILABLE;
        evo_orchestration_latch_failure(owner, index);
    }
    if (job->terminal_reason ==
        EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS) {
        if (!evo_orchestration_copy_evaluation(&join.evaluation, job)) {
            job->terminal_reason =
                EVO_PROJECT_ORCHESTRATION_TERMINAL_PROVIDER_PROTOCOL;
            evo_orchestration_latch_failure(owner, index);
        }
    }
    job->cleanup_complete = true;
    job->joined = true;
    job->state = EVO_PROJECT_ORCHESTRATION_JOB_JOINED;
    if (!evo_project_orchestration_terminal_is_hard_failure(
            job->terminal_reason)) {
        job->staged = true;
        job->state = EVO_PROJECT_ORCHESTRATION_JOB_STAGED;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t evo_orchestration_join_wave(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t *owner,
    size_t wave_start,
    size_t wave_end)
{
    size_t index;

    for (index = wave_start; index < wave_end; index += 1U) {
        const evo_project_orchestration_status_t status =
            evo_orchestration_join_job(config, owner, index);

        if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
            size_t recovery_index;

            for (recovery_index = index + 1U; recovery_index < wave_end;
                 recovery_index += 1U) {
                evo_project_orchestration_runtime_job_t *runtime =
                    &owner->runtime_jobs[recovery_index];
                evo_project_orchestration_job_record_t *job =
                    &owner->jobs[recovery_index];

                if (runtime->active && !job->cancel_requested) {
                    (void)config->provider.cancel(
                        runtime->provider_handle,
                        config->provider.context);
                    job->cancel_requested = true;
                }
                if (runtime->active) {
                    evo_project_orchestration_provider_join_t recovery = {0};

                    (void)config->provider.join(
                        runtime->provider_handle,
                        config->provider.context,
                        &recovery);
                    runtime->provider_handle = NULL;
                    runtime->active = false;
                }
            }
            return status;
        }
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

evo_project_orchestration_status_t evo_project_orchestration_runtime_run(
    const evo_project_orchestration_config_t *config,
    evo_project_orchestration_owner_t *owner)
{
    size_t index;
    size_t wave_start = 0U;

    owner->view.cleanup_complete = true;
    for (index = 0U; index < owner->job_count; index += 1U) {
        evo_orchestration_prepare_job(config, owner, index);
    }

    while (wave_start < owner->job_count && !owner->view.has_hard_failure) {
        size_t wave_end = wave_start + config->resources.external_worker_count;

        if (wave_end < wave_start || wave_end > owner->job_count) {
            wave_end = owner->job_count;
        }
        for (index = wave_start; index < wave_end; index += 1U) {
            const evo_project_orchestration_status_t status =
                evo_orchestration_start_job(config, owner, index);

            if (status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
                return status;
            }
            if (owner->view.has_hard_failure) {
                break;
            }
        }
        if (owner->view.has_hard_failure) {
            const evo_project_orchestration_status_t cancel_status =
                evo_orchestration_cancel_active(
                    config,
                    owner,
                    wave_start,
                    wave_end,
                    owner->view.first_hard_failure_index);

            if (cancel_status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
                return cancel_status;
            }
        } else {
            const evo_project_orchestration_status_t poll_status =
                evo_orchestration_poll_wave(
                    config, owner, wave_start, wave_end);

            if (poll_status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
                return poll_status;
            }
        }
        {
            const evo_project_orchestration_status_t join_status =
                evo_orchestration_join_wave(
                    config, owner, wave_start, wave_end);

            if (join_status != EVO_PROJECT_ORCHESTRATION_SUCCESS) {
                return join_status;
            }
        }
        wave_start = wave_end;
    }

    if (!owner->view.has_hard_failure) {
        for (index = 0U; index < owner->job_count; index += 1U) {
            evo_project_orchestration_job_record_t *job = &owner->jobs[index];

            if (!job->staged || !job->joined || !job->cleanup_complete) {
                return EVO_PROJECT_ORCHESTRATION_ERROR_STATE;
            }
            job->commit_ordinal = index;
            job->committed = true;
            job->state = EVO_PROJECT_ORCHESTRATION_JOB_COMMITTED;
            owner->view.committed_count += 1U;
        }
        owner->view.generation_committed = true;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}
