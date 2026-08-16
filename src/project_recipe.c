#include "internal/project_recipe.h"

#include "internal/project_analysis_owner.h"
#include "internal/project_baseline_owner.h"
#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_recipe_encoding.h"
#include "internal/project_recipe_model.h"
#include "internal/project_recipe_owner.h"
#include "internal/project_runtime.h"
#include "internal/project_snapshot.h"

#include <string.h>

static bool evo_project_recipe_limits_valid(
    const evo_project_recipe_limits_t *limits)
{
    return limits->max_string_bytes > 0U && limits->max_path_bytes > 0U &&
           limits->max_catalogue_entries > 0U &&
           limits->max_parameter_schemas > 0U && limits->max_choices > 0U &&
           limits->max_records > 0U &&
           limits->max_parameters_per_record > 0U &&
           limits->max_preconditions_per_record > 0U &&
           limits->max_dependencies_per_record > 0U &&
           limits->max_conflicts_per_record > 0U &&
           limits->max_provenance_records_per_record > 0U &&
           limits->max_json_tokens > 0U && limits->max_json_depth > 0U &&
           limits->max_genome_bytes > EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE &&
           limits->max_audit_bytes > 0U && limits->max_total_bytes > 0U;
}

static bool evo_project_recipe_result_independent(
    const evo_project_recipe_context_t *context,
    const evo_project_recipe_t *recipe)
{
    const evo_project_baseline_owner_t *baseline_owner =
        context->baseline->private_owner;
    const evo_project_analysis_owner_t *analysis_owner =
        context->analysis->private_owner;
    size_t index;

    if ((const void *)context == (const void *)recipe ||
        (const void *)context->baseline == (const void *)recipe ||
        (const void *)baseline_owner == (const void *)recipe ||
        (const void *)context->analysis == (const void *)recipe ||
        (const void *)analysis_owner == (const void *)recipe ||
        (const void *)context->catalogue == (const void *)recipe ||
        (const void *)context->catalogue->entries == (const void *)recipe ||
        (const void *)context->analysis->source_locations ==
            (const void *)recipe ||
        (const void *)context->analysis->opportunities ==
            (const void *)recipe) {
        return false;
    }
    for (index = 0U; index < context->catalogue->entry_count; index += 1U) {
        const evo_project_transformation_catalogue_entry_t *entry =
            &context->catalogue->entries[index];

        if ((const void *)entry->parameter_schemas == (const void *)recipe ||
            (const void *)entry->preconditions == (const void *)recipe ||
            (const void *)entry->dependencies == (const void *)recipe ||
            (const void *)entry->conflicts == (const void *)recipe) {
            return false;
        }
    }
    return true;
}

static bool evo_project_recipe_context_valid(
    const evo_project_recipe_context_t *context,
    const evo_project_recipe_t *recipe)
{
    const evo_project_baseline_owner_t *baseline_owner;
    const evo_project_analysis_owner_t *analysis_owner;

    if (context == NULL || recipe == NULL || context->baseline == NULL ||
        context->analysis == NULL || context->catalogue == NULL ||
        !evo_project_recipe_limits_valid(&context->limits)) {
        return false;
    }
    baseline_owner = context->baseline->private_owner;
    analysis_owner = context->analysis->private_owner;
    if (context->baseline->schema_version !=
            EVO_PROJECT_BASELINE_SCHEMA_VERSION ||
        baseline_owner == NULL || !baseline_owner->committed ||
        context->analysis->schema_version !=
            EVO_PROJECT_ANALYSIS_SCHEMA_VERSION ||
        analysis_owner == NULL || !analysis_owner->committed ||
        context->catalogue->entries == NULL ||
        context->catalogue->entry_count == 0U ||
        context->catalogue->entry_count >
            context->limits.max_catalogue_entries) {
        return false;
    }
    return evo_project_recipe_result_independent(context, recipe);
}

static bool evo_project_recipe_analysis_current(
    const evo_project_recipe_context_t *context)
{
    const evo_project_baseline_owner_t *baseline_owner =
        context->baseline->private_owner;
    const evo_project_analysis_owner_t *analysis_owner =
        context->analysis->private_owner;
    char baseline_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char analysis_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];

    evo_project_fingerprint_format(
        baseline_owner->baseline_fingerprint, baseline_fingerprint);
    evo_project_fingerprint_format(
        analysis_owner->analysis_fingerprint, analysis_fingerprint);
    return strcmp(
               context->baseline->baseline_fingerprint,
               baseline_fingerprint) == 0 &&
           strcmp(
               context->analysis->baseline_fingerprint,
               baseline_fingerprint) == 0 &&
           strcmp(analysis_owner->baseline_fingerprint,
                  baseline_fingerprint) == 0 &&
           strcmp(
               context->analysis->analysis_fingerprint,
               analysis_fingerprint) == 0 &&
           context->analysis->source_location_count ==
               analysis_owner->source_location_count &&
           context->analysis->source_locations ==
               analysis_owner->source_locations &&
           context->analysis->optimization_record_count ==
               analysis_owner->optimization_record_count &&
           context->analysis->optimization_records ==
               analysis_owner->optimization_records &&
           context->analysis->runtime_record_count ==
               analysis_owner->runtime_record_count &&
           context->analysis->runtime_records ==
               analysis_owner->runtime_records &&
           context->analysis->opportunity_count ==
               analysis_owner->opportunity_count &&
           context->analysis->opportunities ==
               analysis_owner->opportunities &&
           context->analysis->projection_complete &&
           !context->analysis->probabilistic_authority;
}

static evo_project_recipe_status_t evo_project_recipe_map_snapshot_status(
    evo_project_status_t status)
{
    if (status == EVO_PROJECT_ERROR_OUT_OF_MEMORY) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    if (status == EVO_PROJECT_ERROR_RESOURCE_LIMIT) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    return status == EVO_PROJECT_SUCCESS
               ? EVO_PROJECT_RECIPE_SUCCESS
               : EVO_PROJECT_RECIPE_ERROR_BASELINE_CHANGED;
}

static void evo_project_recipe_publish(
    evo_project_recipe_owner_t *owner,
    evo_project_recipe_t *recipe)
{
    recipe->schema_version = EVO_PROJECT_RECIPE_SCHEMA_VERSION;
    recipe->baseline_fingerprint = owner->baseline_fingerprint;
    recipe->analysis_fingerprint = owner->analysis_fingerprint;
    recipe->catalogue_identity = owner->catalogue_identity;
    recipe->catalogue_version = owner->catalogue_version;
    evo_project_fingerprint_format(
        owner->recipe_fingerprint, recipe->recipe_fingerprint);
    recipe->record_count = owner->record_count;
    recipe->records = owner->records;
    recipe->genome_size = owner->genome_size;
    recipe->genome = owner->genome;
    recipe->canonical_json_size = owner->canonical_json_size;
    recipe->canonical_json =
        (const char *)owner->genome + EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE;
    recipe->audit_markdown_size = owner->audit_markdown_size;
    recipe->audit_markdown = owner->audit_markdown;
    recipe->projection_complete = true;
    recipe->probabilistic_authority = false;
    recipe->raw_source_bytes = false;
    recipe->private_owner = owner;
}

static evo_project_recipe_status_t evo_project_recipe_preflight(
    const evo_project_recipe_context_t *context,
    const evo_project_recipe_t *recipe)
{
    evo_project_status_t snapshot_status;

    if (!evo_project_recipe_context_valid(context, recipe)) {
        return EVO_PROJECT_RECIPE_ERROR_INVALID_ARGUMENT;
    }
    if (recipe->private_owner != NULL || recipe->schema_version != 0U) {
        return EVO_PROJECT_RECIPE_ERROR_RESULT_ACTIVE;
    }
    if (context->baseline->state != EVO_PROJECT_BASELINE_ELIGIBLE ||
        ((const evo_project_baseline_owner_t *)
             context->baseline->private_owner)
                ->state != EVO_PROJECT_BASELINE_ELIGIBLE) {
        return EVO_PROJECT_RECIPE_ERROR_BASELINE_INELIGIBLE;
    }
    if (!evo_project_recipe_analysis_current(context)) {
        return EVO_PROJECT_RECIPE_ERROR_ANALYSIS_STALE;
    }
    snapshot_status = evo_project_snapshot_verify_baseline(
        context->baseline->private_owner);
    return evo_project_recipe_map_snapshot_status(snapshot_status);
}

static evo_project_recipe_status_t evo_project_recipe_finish_operation(
    const evo_project_recipe_context_t *context,
    evo_project_recipe_owner_t *owner,
    evo_project_recipe_status_t status,
    evo_project_recipe_t *recipe)
{
    const evo_project_status_t snapshot_status =
        evo_project_snapshot_verify_baseline(context->baseline->private_owner);

    if (snapshot_status != EVO_PROJECT_SUCCESS) {
        status = evo_project_recipe_map_snapshot_status(snapshot_status);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        evo_project_recipe_publish(owner, recipe);
        return status;
    }
    evo_project_recipe_model_destroy(owner);
    evo_project_release(owner);
    return status;
}

static bool evo_project_recipe_build_records_independent(
    const evo_project_recipe_build_config_t *config,
    const evo_project_recipe_t *recipe)
{
    size_t index;

    if ((const void *)config == (const void *)recipe ||
        (const void *)config->records == (const void *)recipe) {
        return false;
    }
    for (index = 0U; index < config->record_count; index += 1U) {
        if ((const void *)config->records[index].parameters ==
            (const void *)recipe) {
            return false;
        }
    }
    return true;
}

evo_project_recipe_status_t evo_project_recipe_build(
    const evo_project_recipe_build_config_t *config,
    evo_project_recipe_t *recipe)
{
    const evo_project_baseline_owner_t *baseline_owner;
    evo_project_recipe_owner_t *owner;
    evo_project_recipe_status_t status;

    if (config == NULL ||
        !evo_project_recipe_context_valid(&config->context, recipe) ||
        (config->record_count == 0U && config->records != NULL) ||
        (config->record_count > 0U && config->records == NULL)) {
        return EVO_PROJECT_RECIPE_ERROR_INVALID_ARGUMENT;
    }
    if (config->record_count > config->context.limits.max_records) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    if (!evo_project_recipe_build_records_independent(config, recipe)) {
        return EVO_PROJECT_RECIPE_ERROR_INVALID_ARGUMENT;
    }
    status = evo_project_recipe_preflight(&config->context, recipe);
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return status;
    }
    baseline_owner = config->context.baseline->private_owner;
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    status = evo_project_recipe_model_build(
        &config->context,
        baseline_owner,
        config->records,
        config->record_count,
        owner);
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_encoding_finish(
            &config->context, baseline_owner, config->genome_size, owner);
    }
    return evo_project_recipe_finish_operation(
        &config->context, owner, status, recipe);
}

evo_project_recipe_status_t evo_project_recipe_decode(
    const evo_project_recipe_context_t *context,
    const unsigned char *genome,
    size_t genome_size,
    evo_project_recipe_t *recipe)
{
    const evo_project_baseline_owner_t *baseline_owner;
    evo_project_recipe_owner_t *owner;
    evo_project_recipe_status_t status;

    if (genome == NULL || (const void *)genome == (const void *)recipe) {
        return EVO_PROJECT_RECIPE_ERROR_INVALID_ARGUMENT;
    }
    status = evo_project_recipe_preflight(context, recipe);
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return status;
    }
    baseline_owner = context->baseline->private_owner;
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    status = evo_project_recipe_encoding_decode(
        context, baseline_owner, genome, genome_size, owner);
    return evo_project_recipe_finish_operation(context, owner, status, recipe);
}

bool evo_project_recipe_equal(
    const evo_project_recipe_t *left,
    const evo_project_recipe_t *right)
{
    size_t index;

    if (left == NULL || right == NULL || left->private_owner == NULL ||
        right->private_owner == NULL ||
        left->schema_version != EVO_PROJECT_RECIPE_SCHEMA_VERSION ||
        right->schema_version != EVO_PROJECT_RECIPE_SCHEMA_VERSION ||
        left->canonical_json == NULL || right->canonical_json == NULL ||
        left->canonical_json_size != right->canonical_json_size) {
        return false;
    }
    for (index = 0U; index < left->canonical_json_size; index += 1U) {
        if (left->canonical_json[index] != right->canonical_json[index]) {
            return false;
        }
    }
    return true;
}

void evo_project_recipe_destroy(evo_project_recipe_t *recipe)
{
    evo_project_recipe_owner_t *owner;

    if (recipe == NULL) {
        return;
    }
    owner = recipe->private_owner;
    if (owner != NULL) {
        evo_project_recipe_model_destroy(owner);
        evo_project_release(owner);
    }
    *recipe = (evo_project_recipe_t){0};
}

const char *evo_project_recipe_status_name(
    evo_project_recipe_status_t status)
{
    switch (status) {
    case EVO_PROJECT_RECIPE_SUCCESS:
        return "success";
    case EVO_PROJECT_RECIPE_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_RECIPE_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_RECIPE_ERROR_BASELINE_INELIGIBLE:
        return "baseline-ineligible";
    case EVO_PROJECT_RECIPE_ERROR_ANALYSIS_STALE:
        return "analysis-stale";
    case EVO_PROJECT_RECIPE_ERROR_CATALOGUE_INVALID:
        return "catalogue-invalid";
    case EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID:
        return "recipe-invalid";
    case EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION:
        return "unknown-transformation";
    case EVO_PROJECT_RECIPE_ERROR_STALE_TARGET:
        return "stale-target";
    case EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER:
        return "invalid-parameter";
    case EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_MISSING:
        return "dependency-missing";
    case EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_AMBIGUOUS:
        return "dependency-ambiguous";
    case EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_CYCLE:
        return "dependency-cycle";
    case EVO_PROJECT_RECIPE_ERROR_CONFLICT:
        return "conflict";
    case EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT:
        return "genome-corrupt";
    case EVO_PROJECT_RECIPE_ERROR_GENOME_NONCANONICAL:
        return "genome-noncanonical";
    case EVO_PROJECT_RECIPE_ERROR_BASELINE_CHANGED:
        return "baseline-changed";
    case EVO_PROJECT_RECIPE_ERROR_STATE:
    default:
        return "state";
    }
}
