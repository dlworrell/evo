from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'{label}: expected text not found')
    return text.replace(old, new, 1)


# Extend the external scheduler request with the live recipe authority needed by
# the caller-supplied provider during synchronous start admission. Existing
# scheduler-only fixtures may leave these appended fields zero/null.
header = Path('src/internal/project_orchestration.h')
text = header.read_text()
old = '''typedef struct evo_project_orchestration_candidate_request {\n    uint32_t schema_version;\n    size_t generation;\n    size_t population_index;\n    const char *recipe_fingerprint;\n    const char *workspace_identity;\n} evo_project_orchestration_candidate_request_t;\n'''
new = '''typedef struct evo_project_orchestration_candidate_request {\n    uint32_t schema_version;\n    size_t generation;\n    size_t population_index;\n    const char *recipe_fingerprint;\n    const char *workspace_identity;\n    uint64_t random_seed;\n    const evo_project_recipe_t *recipe;\n} evo_project_orchestration_candidate_request_t;\n'''
if 'const evo_project_recipe_t *recipe;' not in text:
    text = replace_once(text, old, new, 'candidate request extension')
header.write_text(text)


path = Path('src/project_search.c')
text = path.read_text()
if '#include "internal/project_search_orchestration.h"' not in text:
    text = replace_once(
        text,
        '#include "internal/project_search_owner.h"\n',
        '#include "internal/project_search_owner.h"\n#include "internal/project_search_orchestration.h"\n#include "internal/run_batch.h"\n',
        'orchestrated search includes')

old_context = '''typedef struct evo_search_run_context {\n    const evo_project_search_config_t *config;\n    evo_project_search_owner_t *owner;\n    bool fatal_state;\n} evo_search_run_context_t;\n'''
new_context = '''typedef struct evo_search_run_context {\n    const evo_project_search_config_t *config;\n    const evo_project_search_orchestration_policy_t *orchestration_policy;\n    evo_project_search_owner_t *owner;\n    bool fatal_state;\n} evo_search_run_context_t;\n'''
if 'const evo_project_search_orchestration_policy_t *orchestration_policy;' not in text:
    text = replace_once(text, old_context, new_context, 'search run context')

start = text.find('static bool evo_search_validate_callback(')
end = text.find('static evo_fitness_t evo_search_evaluate_callback(', start)
if start < 0 or end < 0:
    raise SystemExit('search validation callback boundaries missing')
if 'evo_search_prepare_structural_record(' not in text:
    replacement = r'''static bool evo_search_prepare_structural_record(
    evo_search_run_context_t *context,
    const void *genome,
    evo_project_recipe_t *recipe,
    evo_project_search_lineage_record_t **record_out)
{
    evo_project_search_owner_t *owner = context->owner;
    evo_project_search_birth_event_t *birth =
        evo_search_find_birth(owner, genome);
    evo_project_search_lineage_record_t *record;
    evo_project_recipe_status_t recipe_status;
    const size_t ordinal = owner->validation_ordinal;

    if (record_out == NULL || recipe == NULL || recipe->private_owner != NULL) {
        context->fatal_state = true;
        return false;
    }
    *record_out = NULL;
    owner->validation_ordinal += 1U;
    if (owner->lineage_count >= owner->lineage_capacity ||
        context->config->population_size == 0U) {
        context->fatal_state = true;
        return false;
    }
    record = &owner->lineage[owner->lineage_count];
    *record = (evo_project_search_lineage_record_t){0};
    record->generation = ordinal / context->config->population_size;
    record->population_index = ordinal % context->config->population_size;
    owner->lineage_genome_addresses[owner->lineage_count] = genome;
    owner->lineage_count += 1U;
    *record_out = record;

    if (birth != NULL) {
        birth->consumed = true;
        record->rejection_reason = birth->rejection_reason;
        record->recipe_status = birth->recipe_status;
        (void)evo_search_copy_fingerprint(
            record->parent_a_recipe_fingerprint,
            birth->parent_a_recipe_fingerprint);
        (void)evo_search_copy_fingerprint(
            record->parent_b_recipe_fingerprint,
            birth->parent_b_recipe_fingerprint);
        if (evo_search_bind_operator_events(owner, genome, record) == 0U) {
            record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_STATE;
            context->fatal_state = true;
            return false;
        }
        if (birth->rejection_reason != EVO_PROJECT_SEARCH_REJECTION_NONE) {
            return false;
        }
    }

    recipe_status = evo_project_recipe_decode(
        &context->config->recipe_context,
        genome,
        context->config->genome_size,
        recipe);
    record->recipe_status = recipe_status;
    if (birth == NULL) {
        if (evo_search_record_operator_event(
                context,
                genome,
                EVO_PROJECT_SEARCH_OPERATOR_CLONE,
                recipe_status == EVO_PROJECT_RECIPE_SUCCESS
                    ? recipe->recipe_fingerprint
                    : NULL,
                NULL,
                recipe_status == EVO_PROJECT_RECIPE_SUCCESS
                    ? recipe->recipe_fingerprint
                    : NULL,
                recipe_status) == NULL ||
            evo_search_bind_operator_events(owner, genome, record) == 0U) {
            record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_STATE;
            context->fatal_state = true;
            evo_project_recipe_destroy(recipe);
            return false;
        }
    }
    if (recipe_status != EVO_PROJECT_RECIPE_SUCCESS) {
        record->rejection_reason =
            evo_search_rejection_from_recipe(recipe_status);
        evo_project_recipe_destroy(recipe);
        return false;
    }
    if (!evo_search_copy_fingerprint(
            record->recipe_fingerprint, recipe->recipe_fingerprint)) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_STATE;
        context->fatal_state = true;
        evo_project_recipe_destroy(recipe);
        return false;
    }
    if (birth == NULL) {
        (void)evo_search_copy_fingerprint(
            record->parent_a_recipe_fingerprint, recipe->recipe_fingerprint);
    }
    return true;
}

static bool evo_search_validate_callback(const void *genome, void *opaque)
{
    evo_search_run_context_t *context = opaque;
    evo_project_search_lineage_record_t *record = NULL;
    evo_project_recipe_t recipe = {0};
    evo_project_search_evaluation_request_t request = {0};
    evo_project_search_evaluation_outcome_t outcome = {0};
    evo_project_search_status_t provider_status;

    if (!evo_search_prepare_structural_record(
            context, genome, &recipe, &record)) {
        return false;
    }
    request.schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION;
    request.random_seed = context->config->random_seed;
    request.generation = record->generation;
    request.population_index = record->population_index;
    request.provider_identity = context->config->evaluation_provider_identity;
    request.recipe = &recipe;
    outcome.schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION;
    provider_status = context->config->evaluation_provider(
        &request,
        context->config->evaluation_provider_context,
        &outcome);
    if (provider_status != EVO_PROJECT_SEARCH_SUCCESS ||
        outcome.schema_version != EVO_PROJECT_SEARCH_SCHEMA_VERSION ||
        !outcome.accepted) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_PROVIDER;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    if (!outcome.correctness_preserved) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_CORRECTNESS;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    if (!outcome.performance_eligible) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_ASSURANCE;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    if (!outcome.fitness_available) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_MEASUREMENT;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    if (!evo_search_copy_outcome(context->config, &outcome, record)) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_PROVIDER;
        context->fatal_state = true;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    record->valid = true;
    record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_NONE;
    evo_project_recipe_destroy(&recipe);
    return true;
}

'''
    text = text[:start] + replacement + text[end:]

batch_marker = 'static bool evo_search_allocate_owner(\n'
if 'static evo_status_t evo_search_batch_evaluation_callback(' not in text:
    if batch_marker not in text:
        raise SystemExit('batch callback insertion marker missing')
    batch_code = r'''#define EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES 96U

static bool evo_search_orchestration_policy_valid(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *policy)
{
    return config != NULL && policy != NULL &&
           policy->schema_version == EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION &&
           policy->identity != NULL && policy->identity[0] != '\0' &&
           policy->provider.identity != NULL &&
           policy->provider.identity[0] != '\0' &&
           policy->provider.start != NULL && policy->provider.poll != NULL &&
           policy->provider.cancel != NULL && policy->provider.join != NULL &&
           policy->resources.schema_version ==
               EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION &&
           policy->resources.external_worker_count > 0U &&
           policy->limits.max_candidates >= config->population_size &&
           policy->limits.max_string_bytes >=
               EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES &&
           policy->limits.max_external_workers >=
               policy->resources.external_worker_count;
}

static void evo_search_destroy_recipe_array(
    evo_project_recipe_t *recipes,
    size_t count)
{
    size_t index;

    if (recipes == NULL) {
        return;
    }
    for (index = 0U; index < count; index += 1U) {
        evo_project_recipe_destroy(&recipes[index]);
    }
    evo_project_release(recipes);
}

static evo_status_t evo_search_batch_evaluation_callback(
    const evo_problem_t *problem,
    const evo_config_t *core_config,
    void *opaque,
    uint64_t generation,
    const evo_population_t *population,
    evo_candidate_evaluation_t *evaluations,
    size_t evaluation_count,
    void *batch_context)
{
    evo_search_run_context_t *context = batch_context;
    const evo_project_search_orchestration_policy_t *policy;
    evo_project_recipe_t *recipes = NULL;
    evo_project_search_lineage_record_t **records = NULL;
    evo_project_orchestration_candidate_request_t *requests = NULL;
    char *workspace_storage = NULL;
    evo_project_orchestration_t orchestration = {0};
    evo_project_orchestration_config_t orchestration_config = {0};
    size_t scheduled_count = 0U;
    size_t index;
    evo_status_t status = EVO_SUCCESS;

    (void)problem;
    (void)core_config;
    (void)opaque;
    if (context == NULL || context->config == NULL ||
        context->owner == NULL || context->orchestration_policy == NULL ||
        population == NULL || evaluations == NULL ||
        evaluation_count != population->population_size ||
        population->population_size != context->config->population_size ||
        population->genome_size != context->config->genome_size ||
        generation > SIZE_MAX ||
        context->owner->validation_ordinal / context->config->population_size !=
            (size_t)generation) {
        return EVO_ERROR_EVALUATION;
    }
    policy = context->orchestration_policy;
    if (evaluation_count >
            SIZE_MAX / sizeof(*recipes) ||
        evaluation_count > SIZE_MAX / sizeof(*records) ||
        evaluation_count > SIZE_MAX / sizeof(*requests) ||
        evaluation_count >
            SIZE_MAX / EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES) {
        context->fatal_state = true;
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    recipes = evo_project_allocate_zeroed(evaluation_count, sizeof(*recipes));
    records = evo_project_allocate_zeroed(evaluation_count, sizeof(*records));
    requests = evo_project_allocate_zeroed(evaluation_count, sizeof(*requests));
    workspace_storage = evo_project_allocate_zeroed(
        evaluation_count, EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES);
    if (recipes == NULL || records == NULL || requests == NULL ||
        workspace_storage == NULL) {
        status = EVO_ERROR_OUT_OF_MEMORY;
        goto finish;
    }

    for (index = 0U; index < evaluation_count; index += 1U) {
        const unsigned char *genome =
            (const unsigned char *)population->genomes +
            index * context->config->genome_size;
        evo_project_search_lineage_record_t *record = NULL;
        char *workspace =
            workspace_storage +
            scheduled_count * EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES;
        int written;

        if (!evo_search_prepare_structural_record(
                context, genome, &recipes[index], &record)) {
            if (context->fatal_state) {
                status = EVO_ERROR_EVALUATION;
                goto finish;
            }
            continue;
        }
        if (record == NULL || record->generation != (size_t)generation ||
            record->population_index != index) {
            context->fatal_state = true;
            status = EVO_ERROR_EVALUATION;
            goto finish;
        }
        records[index] = record;
        written = evo_project_format(
            workspace,
            EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES,
            "generation-%llu-candidate-%zu",
            (unsigned long long)generation,
            index);
        if (written <= 0 ||
            (size_t)written >= EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES) {
            context->fatal_state = true;
            status = EVO_ERROR_RESOURCE_LIMIT;
            goto finish;
        }
        requests[scheduled_count].schema_version =
            EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
        requests[scheduled_count].generation = (size_t)generation;
        requests[scheduled_count].population_index = index;
        requests[scheduled_count].recipe_fingerprint =
            recipes[index].recipe_fingerprint;
        requests[scheduled_count].workspace_identity = workspace;
        requests[scheduled_count].random_seed = context->config->random_seed;
        requests[scheduled_count].recipe = &recipes[index];
        scheduled_count += 1U;
    }

    if (scheduled_count == 0U) {
        goto finish;
    }
    orchestration_config.policy_identity = policy->identity;
    orchestration_config.resources = policy->resources;
    orchestration_config.candidate_count = scheduled_count;
    orchestration_config.candidates = requests;
    orchestration_config.provider = policy->provider;
    orchestration_config.limits = policy->limits;
    if (evo_project_orchestration_run_batch(
            &orchestration_config, &orchestration) !=
        EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
    if (orchestration.has_hard_failure ||
        !orchestration.cleanup_complete ||
        !orchestration.generation_committed ||
        orchestration.committed_count != scheduled_count) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
    for (index = 0U; index < orchestration.job_count; index += 1U) {
        const evo_project_orchestration_job_record_t *job =
            &orchestration.jobs[index];
        evo_project_search_lineage_record_t *record;
        const size_t population_index = job->population_index;

        if (population_index >= evaluation_count ||
            records[population_index] == NULL || !job->committed) {
            context->fatal_state = true;
            status = EVO_ERROR_EVALUATION;
            goto finish;
        }
        record = records[population_index];
        if (job->terminal_reason ==
            EVO_PROJECT_ORCHESTRATION_TERMINAL_CANDIDATE_REJECTED) {
            record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_PROVIDER;
            continue;
        }
        if (job->terminal_reason !=
                EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS ||
            !evo_search_copy_outcome(
                context->config, &job->evaluation, record)) {
            record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_PROVIDER;
            context->fatal_state = true;
            status = EVO_ERROR_EVALUATION;
            goto finish;
        }
        record->valid = true;
        record->evaluated = true;
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_NONE;
        evaluations[population_index].valid = true;
        evaluations[population_index].evaluated = true;
        evaluations[population_index].fitness = record->fitness;
    }

finish:
    evo_project_orchestration_destroy(&orchestration);
    evo_search_destroy_recipe_array(recipes, evaluation_count);
    evo_project_release(records);
    evo_project_release(requests);
    evo_project_release(workspace_storage);
    return status;
}

'''
    text = replace_once(text, batch_marker, batch_code + batch_marker, 'batch callback marker')

# Convert the current public implementation into a common helper and dispatch
# either through the ordinary synchronous validation path or the private batch
# evaluator path. The installed public function remains unchanged.
old_run = 'evo_project_search_status_t evo_project_search_run(\n    const evo_project_search_config_t *config,\n    evo_project_search_t *search)\n'
if 'static evo_project_search_status_t evo_project_search_run_common(' not in text:
    new_run = '''static evo_project_search_status_t evo_project_search_run_common(\n    const evo_project_search_config_t *config,\n    const evo_project_search_orchestration_policy_t *orchestration_policy,\n    evo_project_search_t *search)\n'''
    text = replace_once(text, old_run, new_run, 'project search run signature')
    text = replace_once(
        text,
        '''    if (config == NULL || search == NULL || !evo_search_config_valid(config)) {\n        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;\n    }\n''',
        '''    if (config == NULL || search == NULL || !evo_search_config_valid(config) ||\n        (orchestration_policy != NULL &&\n         !evo_search_orchestration_policy_valid(config, orchestration_policy))) {\n        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;\n    }\n''',
        'run preflight')
    text = replace_once(
        text,
        '''    run_context.config = config;\n    run_context.owner = owner;\n''',
        '''    run_context.config = config;\n    run_context.orchestration_policy = orchestration_policy;\n    run_context.owner = owner;\n''',
        'run context orchestration')
    old_core = '    core_status = evo_run(&problem, &core_config, &run_context, &core_result);\n'
    new_core = '''    if (orchestration_policy == NULL) {\n        core_status = evo_run(\n            &problem, &core_config, &run_context, &core_result);\n    } else {\n        const evo_population_batch_evaluator_t batch_evaluator = {\n            evo_search_batch_evaluation_callback, &run_context};\n\n        core_status = evo_run_with_batch_evaluator(\n            &problem,\n            &core_config,\n            &run_context,\n            &batch_evaluator,\n            &core_result);\n    }\n'''
    text = replace_once(text, old_core, new_core, 'core run dispatch')
    destroy_marker = 'void evo_project_search_destroy(evo_project_search_t *search)\n'
    wrappers = '''evo_project_search_status_t evo_project_search_run(\n    const evo_project_search_config_t *config,\n    evo_project_search_t *search)\n{\n    return evo_project_search_run_common(config, NULL, search);\n}\n\nevo_project_search_status_t evo_project_search_run_orchestrated(\n    const evo_project_search_config_t *config,\n    const evo_project_search_orchestration_policy_t *orchestration_policy,\n    evo_project_search_t *search)\n{\n    if (orchestration_policy == NULL) {\n        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;\n    }\n    return evo_project_search_run_common(\n        config, orchestration_policy, search);\n}\n\n'''
    text = replace_once(text, destroy_marker, wrappers + destroy_marker, 'search destroy marker')

path.write_text(text)
