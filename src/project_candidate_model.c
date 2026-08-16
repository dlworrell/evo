#include "internal/project_candidate_internal.h"

#include "internal/project_fingerprint.h"
#include "internal/project_recipe_owner.h"
#include "internal/project_runtime.h"
#include "internal/project_snapshot.h"
#include "internal/project_transformation_owner.h"

#include <string.h>

static const evo_project_transformation_application_t *evo_candidate_find_application(
    const evo_project_candidate_config_t *config,
    const char *identity,
    bool *duplicate)
{
    const evo_project_transformation_application_t *found = NULL;
    size_t index;

    *duplicate = false;
    for (index = 0U; index < config->application_count; index += 1U) {
        const evo_project_transformation_application_t *application =
            &config->applications[index];

        if (application->record_identity != NULL &&
            strcmp(application->record_identity, identity) == 0) {
            if (found != NULL) {
                *duplicate = true;
                return NULL;
            }
            found = application;
        }
    }
    return found;
}

static const evo_project_file_record_t *evo_candidate_find_baseline_file(
    const evo_project_baseline_owner_t *owner,
    const char *path)
{
    size_t index;

    for (index = 0U; index < owner->file_count; index += 1U) {
        if (owner->files[index].path != NULL &&
            strcmp(owner->files[index].path, path) == 0) {
            return &owner->files[index];
        }
    }
    return NULL;
}

static bool evo_candidate_recipe_current(
    const evo_project_candidate_config_t *config,
    const evo_project_recipe_owner_t *owner)
{
    char fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];

    if (owner == NULL || config->recipe->schema_version != EVO_PROJECT_RECIPE_SCHEMA_VERSION ||
        config->recipe->records != owner->records ||
        config->recipe->record_count != owner->record_count ||
        config->recipe->record_count == 0U || !config->recipe->projection_complete ||
        config->recipe->probabilistic_authority || config->recipe->raw_source_bytes ||
        strcmp(config->recipe->baseline_fingerprint,
               config->baseline->baseline_fingerprint) != 0) {
        return false;
    }
    evo_project_fingerprint_format(owner->recipe_fingerprint, fingerprint);
    return strcmp(config->recipe->recipe_fingerprint, fingerprint) == 0;
}

static bool evo_candidate_application_owner_current(
    const evo_project_transformation_application_t *application)
{
    const evo_project_transformation_application_owner_t *owner = application->private_owner;
    char fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];

    if (owner == NULL) {
        return false;
    }
    evo_project_fingerprint_format(owner->application_fingerprint, fingerprint);
    return strcmp(application->application_fingerprint, fingerprint) == 0;
}

static evo_project_candidate_status_t evo_candidate_validate_application(
    const evo_project_candidate_config_t *config,
    const evo_project_recipe_record_t *record,
    const evo_project_transformation_application_t *application)
{
    if (application == NULL) {
        return EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_MISSING;
    }
    if (!evo_candidate_application_owner_current(application) ||
        application->schema_version != EVO_PROJECT_TRANSFORMATION_APPLICATION_SCHEMA_VERSION ||
        !application->projection_complete || application->probabilistic_authority ||
        application->snapshot_modified || application->candidate_materialized ||
        application->baseline_fingerprint == NULL || application->recipe_fingerprint == NULL ||
        application->record_identity == NULL || application->transformation_identity == NULL ||
        application->target.file == NULL || application->target.location_identity == NULL ||
        strcmp(application->baseline_fingerprint,
               config->baseline->baseline_fingerprint) != 0 ||
        strcmp(application->recipe_fingerprint, config->recipe->recipe_fingerprint) != 0 ||
        strcmp(application->record_identity, record->identity) != 0 ||
        strcmp(application->transformation_identity, record->transformation_identity) != 0 ||
        application->transformation_version != record->transformation_version ||
        strcmp(application->target.file, record->target.file) != 0 ||
        strcmp(application->target.location_identity, record->target.location_identity) != 0) {
        return EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_STALE;
    }
    if (application->disposition != EVO_PROJECT_TRANSFORMATION_EDIT &&
        application->disposition != EVO_PROJECT_TRANSFORMATION_ALREADY_SATISFIED) {
        return EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_STALE;
    }
    if (!evo_candidate_relative_path_valid(record->target.file, config->limits.max_path_bytes)) {
        return EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID;
    }
    if (application->disposition == EVO_PROJECT_TRANSFORMATION_EDIT) {
        if (application->edit.before_start >= application->edit.before_end ||
            application->edit.before_size !=
                application->edit.before_end - application->edit.before_start ||
            application->edit.before_text == NULL ||
            (application->edit.replacement_size > 0U &&
             application->edit.replacement_text == NULL) ||
            application->edit.after_start != application->edit.before_start ||
            application->edit.after_end !=
                application->edit.after_start + application->edit.replacement_size) {
            return EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_STALE;
        }
    }
    return EVO_PROJECT_CANDIDATE_SUCCESS;
}

evo_project_candidate_status_t evo_candidate_preflight(
    const evo_project_candidate_config_t *config,
    const evo_project_candidate_t *candidate,
    const evo_project_baseline_owner_t **baseline_owner,
    const evo_project_recipe_owner_t **recipe_owner,
    char **normalized_output)
{
    size_t index;
    size_t edit_count = 0U;
    evo_project_status_t snapshot_status;

    if (config == NULL || candidate == NULL || config->baseline == NULL ||
        config->recipe == NULL || config->applications == NULL ||
        config->application_count == 0U || !evo_candidate_limits_valid(&config->limits) ||
        (config->workspace_policy != EVO_PROJECT_CANDIDATE_WORKSPACE_DISCARD &&
         config->workspace_policy != EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN) ||
        (const void *)config == (const void *)candidate ||
        (const void *)config->baseline == (const void *)candidate ||
        (const void *)config->recipe == (const void *)candidate ||
        (const void *)config->applications == (const void *)candidate) {
        return EVO_PROJECT_CANDIDATE_ERROR_INVALID_ARGUMENT;
    }
    if (candidate->private_owner != NULL || candidate->schema_version != 0U) {
        return EVO_PROJECT_CANDIDATE_ERROR_RESULT_ACTIVE;
    }
    *baseline_owner = config->baseline->private_owner;
    *recipe_owner = config->recipe->private_owner;
    if (*baseline_owner == NULL || !(*baseline_owner)->committed ||
        config->baseline->schema_version != EVO_PROJECT_BASELINE_SCHEMA_VERSION ||
        config->baseline->state != EVO_PROJECT_BASELINE_ELIGIBLE ||
        (*baseline_owner)->state != EVO_PROJECT_BASELINE_ELIGIBLE ||
        config->baseline->files != (*baseline_owner)->files ||
        config->baseline->file_count != (*baseline_owner)->file_count) {
        return EVO_PROJECT_CANDIDATE_ERROR_BASELINE_INELIGIBLE;
    }
    if (config->baseline->file_count == 0U ||
        config->baseline->file_count > config->limits.max_files ||
        config->baseline->total_file_bytes > (uint64_t)config->limits.max_total_file_bytes) {
        return EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
    }
    if (!evo_candidate_recipe_current(config, *recipe_owner) ||
        config->application_count != config->recipe->record_count) {
        return EVO_PROJECT_CANDIDATE_ERROR_RECIPE_STALE;
    }
    for (index = 0U; index < config->recipe->record_count; index += 1U) {
        const evo_project_recipe_record_t *record = &config->recipe->records[index];
        const evo_project_transformation_application_t *application;
        bool duplicate;
        evo_project_candidate_status_t status;
        size_t previous;

        if (record->identity == NULL || record->target.file == NULL ||
            record->transformation_identity == NULL) {
            return EVO_PROJECT_CANDIDATE_ERROR_RECIPE_STALE;
        }
        if (!evo_candidate_relative_path_valid(
                record->target.file, config->limits.max_path_bytes)) {
            return EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID;
        }
        if (evo_candidate_find_baseline_file(*baseline_owner, record->target.file) == NULL) {
            return EVO_PROJECT_CANDIDATE_ERROR_RECIPE_STALE;
        }
        for (previous = 0U; previous < index; previous += 1U) {
            if (config->recipe->records[previous].identity != NULL &&
                strcmp(config->recipe->records[previous].identity, record->identity) == 0) {
                return EVO_PROJECT_CANDIDATE_ERROR_RECIPE_STALE;
            }
        }
        application = evo_candidate_find_application(config, record->identity, &duplicate);
        if (duplicate) {
            return EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_DUPLICATE;
        }
        status = evo_candidate_validate_application(config, record, application);
        if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
            return status;
        }
        if (application->disposition == EVO_PROJECT_TRANSFORMATION_EDIT) {
            edit_count += 1U;
            if (edit_count > config->limits.max_edits) {
                return EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
            }
        }
    }
    snapshot_status = evo_project_snapshot_verify_baseline(*baseline_owner);
    if (snapshot_status != EVO_PROJECT_SUCCESS) {
        return EVO_PROJECT_CANDIDATE_ERROR_BASELINE_CHANGED;
    }
    return evo_candidate_validate_output_path(config, *baseline_owner, normalized_output);
}

size_t evo_candidate_collect_file_edits(
    const evo_project_candidate_config_t *config,
    const char *path,
    evo_candidate_edit_ref_t *edits)
{
    size_t count = 0U;
    size_t index;

    for (index = 0U; index < config->application_count; index += 1U) {
        const evo_project_transformation_application_t *application =
            &config->applications[index];

        if (application->disposition == EVO_PROJECT_TRANSFORMATION_EDIT &&
            strcmp(application->target.file, path) == 0) {
            edits[count++].application = application;
        }
    }
    return count;
}

static void evo_candidate_sort_edits(evo_candidate_edit_ref_t *edits, size_t count)
{
    size_t left;

    for (left = 0U; left < count; left += 1U) {
        size_t right;
        for (right = left + 1U; right < count; right += 1U) {
            const evo_project_transformation_edit_t *left_edit =
                &edits[left].application->edit;
            const evo_project_transformation_edit_t *right_edit =
                &edits[right].application->edit;

            if (right_edit->before_start < left_edit->before_start ||
                (right_edit->before_start == left_edit->before_start &&
                 strcmp(edits[right].application->record_identity,
                        edits[left].application->record_identity) < 0)) {
                const evo_candidate_edit_ref_t temporary = edits[left];
                edits[left] = edits[right];
                edits[right] = temporary;
            }
        }
    }
}

static bool evo_candidate_fingerprint_text_matches(
    const unsigned char *bytes,
    size_t size,
    const char *expected)
{
    evo_project_fingerprint_t fingerprint;
    char formatted[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(&fingerprint, bytes, size);
    evo_project_fingerprint_format(fingerprint.value, formatted);
    return expected != NULL && strcmp(formatted, expected) == 0;
}

evo_project_candidate_status_t evo_candidate_apply_edits(
    const evo_project_candidate_config_t *config,
    const unsigned char *source,
    size_t source_size,
    evo_candidate_edit_ref_t *edits,
    size_t edit_count,
    unsigned char **candidate_bytes,
    size_t *candidate_size)
{
    size_t index;
    size_t output_size = source_size;
    size_t source_position = 0U;
    size_t output_position = 0U;

    evo_candidate_sort_edits(edits, edit_count);
    for (index = 0U; index < edit_count; index += 1U) {
        const evo_project_transformation_edit_t *edit = &edits[index].application->edit;

        if (edit->before_end > source_size ||
            (index > 0U && edit->before_start <
                               edits[index - 1U].application->edit.before_end)) {
            return EVO_PROJECT_CANDIDATE_ERROR_CONFLICT;
        }
        if (edit->before_size != edit->before_end - edit->before_start ||
            !evo_candidate_fingerprint_text_matches(
                source + edit->before_start, edit->before_size, edit->before_fingerprint) ||
            strlen(edit->before_text) != edit->before_size) {
            return EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_STALE;
        }
        {
            size_t before_index;
            for (before_index = 0U; before_index < edit->before_size; before_index += 1U) {
                if ((unsigned char)edit->before_text[before_index] !=
                    source[edit->before_start + before_index]) {
                    return EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_STALE;
                }
            }
        }
        if (!evo_candidate_fingerprint_text_matches(
                edit->replacement_size == 0U
                    ? (const unsigned char *)""
                    : (const unsigned char *)edit->replacement_text,
                edit->replacement_size,
                edit->replacement_fingerprint)) {
            return EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_STALE;
        }
        if (edit->replacement_size >= edit->before_size) {
            const size_t growth = edit->replacement_size - edit->before_size;
            if (growth > SIZE_MAX - output_size) {
                return EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
            }
            output_size += growth;
        } else {
            output_size -= edit->before_size - edit->replacement_size;
        }
        if (output_size > config->limits.max_file_bytes) {
            return EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
        }
    }
    if (output_size == SIZE_MAX) {
        return EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
    }
    *candidate_bytes = evo_project_allocate_zeroed(output_size + 1U, sizeof(**candidate_bytes));
    if (*candidate_bytes == NULL) {
        return EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < edit_count; index += 1U) {
        const evo_project_transformation_edit_t *edit = &edits[index].application->edit;
        size_t copy_index;

        for (copy_index = source_position; copy_index < edit->before_start; copy_index += 1U) {
            (*candidate_bytes)[output_position++] = source[copy_index];
        }
        for (copy_index = 0U; copy_index < edit->replacement_size; copy_index += 1U) {
            (*candidate_bytes)[output_position++] =
                (unsigned char)edit->replacement_text[copy_index];
        }
        source_position = edit->before_end;
    }
    while (source_position < source_size) {
        (*candidate_bytes)[output_position++] = source[source_position++];
    }
    (*candidate_bytes)[output_position] = '\0';
    *candidate_size = output_position;
    return output_position == output_size ? EVO_PROJECT_CANDIDATE_SUCCESS
                                          : EVO_PROJECT_CANDIDATE_ERROR_STATE;
}
