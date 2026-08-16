#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "internal/project_transformation.h"

#include "internal/project_analysis_owner.h"
#include "internal/project_baseline_owner.h"
#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_recipe_owner.h"
#include "internal/project_runtime.h"
#include "internal/project_snapshot.h"
#include "internal/project_transformation_catalogue.h"
#include "internal/project_transformation_evidence.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static bool evo_transform_limits_valid(
    const evo_project_transformation_limits_t *limits)
{
    return limits != NULL && limits->max_string_bytes > 0U &&
           limits->max_path_bytes > 0U && limits->max_source_bytes > 0U &&
           limits->max_replacement_bytes > 0U &&
           limits->max_parameters > 0U &&
           limits->max_registry_bytes > 0U &&
           limits->max_application_bytes > 0U &&
           limits->max_audit_bytes > 0U && limits->max_total_bytes > 0U;
}

static char *evo_transform_duplicate(const char *value)
{
    size_t size;
    char *copy;
    size_t index;

    if (value == NULL) {
        return NULL;
    }
    size = strlen(value);
    if (size == SIZE_MAX) {
        return NULL;
    }
    copy = evo_project_allocate_zeroed(size + 1U, sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }
    for (index = 0U; index < size; index += 1U) {
        copy[index] = value[index];
    }
    copy[size] = '\0';
    return copy;
}

static bool evo_transform_text_valid(
    const char *value,
    size_t maximum_bytes)
{
    return evo_project_json_text_valid(value, maximum_bytes, false);
}

evo_project_transformation_status_t evo_project_transformation_registry_open(
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_registry_t *registry)
{
    evo_project_transformation_registry_owner_t *owner;
    evo_project_transformation_status_t status;
    size_t capability_count;

    if (!evo_transform_limits_valid(limits) || registry == NULL ||
        (const void *)limits == (const void *)registry) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_INVALID_ARGUMENT;
    }
    if (registry->private_owner != NULL || registry->schema_version != 0U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESULT_ACTIVE;
    }
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    status = evo_project_transformation_catalogue_generate_evidence(
        limits, owner);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        evo_project_release(owner);
        return status;
    }
    registry->schema_version =
        EVO_PROJECT_TRANSFORMATION_REGISTRY_SCHEMA_VERSION;
    registry->recipe_catalogue =
        evo_project_transformation_builtin_recipe_catalogue();
    registry->capabilities =
        evo_project_transformation_builtin_capabilities(&capability_count);
    registry->capability_count = capability_count;
    registry->canonical_json_size = owner->canonical_json_size;
    registry->canonical_json = owner->canonical_json;
    registry->audit_markdown_size = owner->audit_markdown_size;
    registry->audit_markdown = owner->audit_markdown;
    registry->projection_complete = true;
    registry->probabilistic_authority = false;
    registry->private_owner = owner;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

void evo_project_transformation_registry_destroy(
    evo_project_transformation_registry_t *registry)
{
    evo_project_transformation_registry_owner_t *owner;

    if (registry == NULL) {
        return;
    }
    owner = registry->private_owner;
    if (owner != NULL) {
        evo_project_release(owner->audit_markdown);
        evo_project_release(owner->canonical_json);
        evo_project_release(owner);
    }
    *registry = (evo_project_transformation_registry_t){0};
}

static bool evo_transform_result_independent(
    const evo_project_transformation_apply_config_t *config,
    const evo_project_transformation_application_t *application)
{
    return (const void *)config != (const void *)application &&
           (const void *)config->baseline != (const void *)application &&
           (const void *)config->baseline->private_owner !=
               (const void *)application &&
           (const void *)config->analysis != (const void *)application &&
           (const void *)config->analysis->private_owner !=
               (const void *)application &&
           (const void *)config->recipe != (const void *)application &&
           (const void *)config->recipe->private_owner !=
               (const void *)application &&
           (const void *)config->registry != (const void *)application &&
           (const void *)config->registry->private_owner !=
               (const void *)application &&
           config->provider_context != (void *)application;
}

static bool evo_transform_authority_current(
    const evo_project_transformation_apply_config_t *config)
{
    const evo_project_baseline_owner_t *baseline_owner =
        config->baseline->private_owner;
    const evo_project_analysis_owner_t *analysis_owner =
        config->analysis->private_owner;
    const evo_project_recipe_owner_t *recipe_owner =
        config->recipe->private_owner;
    char baseline_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char analysis_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char recipe_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];

    if (baseline_owner == NULL || analysis_owner == NULL ||
        recipe_owner == NULL || !baseline_owner->committed ||
        !analysis_owner->committed ||
        config->baseline->schema_version !=
            EVO_PROJECT_BASELINE_SCHEMA_VERSION ||
        config->analysis->schema_version !=
            EVO_PROJECT_ANALYSIS_SCHEMA_VERSION ||
        config->recipe->schema_version != EVO_PROJECT_RECIPE_SCHEMA_VERSION ||
        config->analysis->source_locations !=
            analysis_owner->source_locations ||
        config->analysis->source_location_count !=
            analysis_owner->source_location_count ||
        config->analysis->opportunities != analysis_owner->opportunities ||
        config->analysis->opportunity_count !=
            analysis_owner->opportunity_count ||
        config->recipe->records != recipe_owner->records ||
        config->recipe->record_count != recipe_owner->record_count ||
        !config->analysis->projection_complete ||
        config->analysis->probabilistic_authority ||
        !config->recipe->projection_complete ||
        config->recipe->probabilistic_authority ||
        config->recipe->raw_source_bytes ||
        !evo_project_transformation_registry_is_builtin(config->registry)) {
        return false;
    }
    evo_project_fingerprint_format(
        baseline_owner->baseline_fingerprint, baseline_fingerprint);
    evo_project_fingerprint_format(
        analysis_owner->analysis_fingerprint, analysis_fingerprint);
    evo_project_fingerprint_format(
        recipe_owner->recipe_fingerprint, recipe_fingerprint);
    return strcmp(
               config->baseline->baseline_fingerprint,
               baseline_fingerprint) == 0 &&
           strcmp(
               config->analysis->baseline_fingerprint,
               baseline_fingerprint) == 0 &&
           strcmp(
               config->analysis->analysis_fingerprint,
               analysis_fingerprint) == 0 &&
           strcmp(
               config->recipe->baseline_fingerprint,
               baseline_fingerprint) == 0 &&
           strcmp(
               config->recipe->analysis_fingerprint,
               analysis_fingerprint) == 0 &&
           strcmp(config->recipe->recipe_fingerprint, recipe_fingerprint) == 0 &&
           strcmp(
               config->recipe->catalogue_identity,
               config->registry->recipe_catalogue->identity) == 0 &&
           config->recipe->catalogue_version ==
               config->registry->recipe_catalogue->catalogue_version;
}

static evo_project_transformation_status_t evo_transform_preflight(
    const evo_project_transformation_apply_config_t *config,
    const evo_project_transformation_application_t *application)
{
    evo_project_status_t snapshot_status;

    if (config == NULL || application == NULL || config->baseline == NULL ||
        config->analysis == NULL || config->recipe == NULL ||
        config->registry == NULL || config->record_identity == NULL ||
        config->provider_identity == NULL || config->provider_version == 0U ||
        config->clang_identity == NULL || config->provider == NULL ||
        !evo_transform_limits_valid(&config->limits) ||
        !evo_transform_text_valid(
            config->record_identity, config->limits.max_string_bytes) ||
        !evo_transform_text_valid(
            config->provider_identity, config->limits.max_string_bytes) ||
        !evo_transform_text_valid(
            config->clang_identity, config->limits.max_string_bytes)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_INVALID_ARGUMENT;
    }
    if (!evo_transform_result_independent(config, application)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_INVALID_ARGUMENT;
    }
    if (application->private_owner != NULL || application->schema_version != 0U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESULT_ACTIVE;
    }
    if (!evo_transform_authority_current(config)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RECIPE_STALE;
    }
    if (config->baseline->state != EVO_PROJECT_BASELINE_ELIGIBLE ||
        ((const evo_project_baseline_owner_t *)
             config->baseline->private_owner)
                ->state != EVO_PROJECT_BASELINE_ELIGIBLE) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_INELIGIBLE;
    }
    snapshot_status = evo_project_snapshot_verify_baseline(
        config->baseline->private_owner);
    return snapshot_status == EVO_PROJECT_SUCCESS
               ? EVO_PROJECT_TRANSFORMATION_SUCCESS
               : EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_CHANGED;
}

static const evo_project_recipe_record_t *evo_transform_find_record(
    const evo_project_recipe_t *recipe,
    const char *identity)
{
    const evo_project_recipe_record_t *found = NULL;
    size_t index;

    for (index = 0U; index < recipe->record_count; index += 1U) {
        if (recipe->records[index].identity != NULL &&
            strcmp(recipe->records[index].identity, identity) == 0) {
            if (found != NULL) {
                return NULL;
            }
            found = &recipe->records[index];
        }
    }
    return found;
}

static const evo_project_file_record_t *evo_transform_find_file(
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

static bool evo_transform_path_component_valid(
    const char *component,
    size_t size)
{
    return size > 0U && !(size == 1U && component[0] == '.') &&
           !(size == 2U && component[0] == '.' && component[1] == '.');
}

static int evo_transform_open_relative_file(
    const char *root,
    const char *path)
{
    const size_t path_size = strlen(path);
    char *component;
    size_t position = 0U;
    int directory_fd;

    if (path_size == 0U || path[0] == '/' || path_size == SIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    component = evo_project_allocate_zeroed(path_size + 1U, sizeof(*component));
    if (component == NULL) {
        errno = ENOMEM;
        return -1;
    }
    directory_fd = open(
        root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory_fd < 0) {
        evo_project_release(component);
        return -1;
    }
    while (position < path_size) {
        size_t component_size = 0U;
        bool final_component;
        int next_fd;

        while (position < path_size && path[position] != '/') {
            component[component_size] = path[position];
            component_size += 1U;
            position += 1U;
        }
        component[component_size] = '\0';
        if (!evo_transform_path_component_valid(component, component_size)) {
            (void)close(directory_fd);
            evo_project_release(component);
            errno = EINVAL;
            return -1;
        }
        final_component = position == path_size;
        next_fd = openat(
            directory_fd,
            component,
            final_component
                ? O_RDONLY | O_NOFOLLOW | O_CLOEXEC
                : O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        (void)close(directory_fd);
        if (next_fd < 0) {
            evo_project_release(component);
            return -1;
        }
        directory_fd = next_fd;
        if (!final_component) {
            position += 1U;
        }
    }
    evo_project_release(component);
    return directory_fd;
}

static evo_project_transformation_status_t evo_transform_read_source(
    const evo_project_baseline_owner_t *owner,
    const evo_project_file_record_t *file,
    const evo_project_transformation_limits_t *limits,
    unsigned char **bytes,
    size_t *size)
{
    struct stat metadata;
    evo_project_fingerprint_t fingerprint;
    size_t position = 0U;
    int file_fd;

    if (file->size > (uint64_t)SIZE_MAX ||
        file->size > (uint64_t)limits->max_source_bytes ||
        file->size > (uint64_t)owner->manifest.budget.max_file_bytes) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    *size = (size_t)file->size;
    file_fd = evo_transform_open_relative_file(owner->snapshot_path, file->path);
    if (file_fd < 0 || fstat(file_fd, &metadata) != 0 ||
        !S_ISREG(metadata.st_mode) || metadata.st_size < 0 ||
        (uintmax_t)metadata.st_size != (uintmax_t)file->size ||
        (unsigned int)(metadata.st_mode & (mode_t)07777) !=
            (file->source_mode & 0555U)) {
        if (file_fd >= 0) {
            (void)close(file_fd);
        }
        return EVO_PROJECT_TRANSFORMATION_ERROR_SOURCE_IO;
    }
    if (*size == SIZE_MAX) {
        (void)close(file_fd);
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    *bytes = evo_project_allocate_zeroed(*size + 1U, sizeof(**bytes));
    if (*bytes == NULL) {
        (void)close(file_fd);
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    while (position < *size) {
        const ssize_t count = read(file_fd, *bytes + position, *size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            evo_project_release(*bytes);
            *bytes = NULL;
            (void)close(file_fd);
            return EVO_PROJECT_TRANSFORMATION_ERROR_SOURCE_IO;
        }
        position += (size_t)count;
    }
    if (close(file_fd) != 0) {
        evo_project_release(*bytes);
        *bytes = NULL;
        return EVO_PROJECT_TRANSFORMATION_ERROR_SOURCE_IO;
    }
    (*bytes)[*size] = '\0';
    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(&fingerprint, *bytes, *size);
    if (fingerprint.value != file->content_fingerprint) {
        evo_project_release(*bytes);
        *bytes = NULL;
        return EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_CHANGED;
    }
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static bool evo_transform_position_offset(
    const unsigned char *source,
    size_t source_size,
    uint32_t wanted_line,
    uint32_t wanted_column,
    size_t *offset)
{
    uint32_t line = 1U;
    uint32_t column = 1U;
    size_t index;

    if (wanted_line == 0U || wanted_column == 0U) {
        return false;
    }
    for (index = 0U; index <= source_size; index += 1U) {
        if (line == wanted_line && column == wanted_column) {
            *offset = index;
            return true;
        }
        if (index == source_size) {
            break;
        }
        if (source[index] == (unsigned char)'\n') {
            line += 1U;
            column = 1U;
        } else {
            column += 1U;
        }
    }
    return false;
}

static bool evo_transform_ast_matches_recipe_range(
    const unsigned char *source,
    size_t source_size,
    const evo_project_recipe_target_t *target,
    const evo_project_transformation_ast_result_t *ast)
{
    size_t start;
    size_t end;

    return evo_transform_position_offset(
               source,
               source_size,
               target->line,
               target->column,
               &start) &&
           evo_transform_position_offset(
               source,
               source_size,
               target->end_line,
               target->end_column,
               &end) &&
           start < end && ast->target.start == start && ast->target.end == end;
}

static evo_project_transformation_status_t evo_transform_copy_common(
    const evo_project_transformation_apply_config_t *config,
    const evo_project_recipe_record_t *record,
    evo_project_transformation_application_owner_t *owner)
{
#define EVO_DUPLICATE_FIELD(member_name, source_value)                    \
    do {                                                                  \
        owner->member_name = evo_transform_duplicate(source_value);       \
        if (owner->member_name == NULL) {                                 \
            return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;        \
        }                                                                 \
    } while (false)

    EVO_DUPLICATE_FIELD(
        baseline_fingerprint, config->baseline->baseline_fingerprint);
    EVO_DUPLICATE_FIELD(
        analysis_fingerprint, config->analysis->analysis_fingerprint);
    EVO_DUPLICATE_FIELD(recipe_fingerprint, config->recipe->recipe_fingerprint);
    EVO_DUPLICATE_FIELD(
        catalogue_identity, config->recipe->catalogue_identity);
    EVO_DUPLICATE_FIELD(record_identity, record->identity);
    EVO_DUPLICATE_FIELD(
        transformation_identity, record->transformation_identity);
    EVO_DUPLICATE_FIELD(provider_identity, config->provider_identity);
    EVO_DUPLICATE_FIELD(clang_identity, config->clang_identity);
    EVO_DUPLICATE_FIELD(
        target_location_identity, record->target.location_identity);
    EVO_DUPLICATE_FIELD(target_file, record->target.file);
    if (record->target.spelling_identity != NULL) {
        EVO_DUPLICATE_FIELD(
            target_spelling_identity, record->target.spelling_identity);
    }
#undef EVO_DUPLICATE_FIELD

    owner->view.schema_version =
        EVO_PROJECT_TRANSFORMATION_APPLICATION_SCHEMA_VERSION;
    owner->view.baseline_fingerprint = owner->baseline_fingerprint;
    owner->view.analysis_fingerprint = owner->analysis_fingerprint;
    owner->view.recipe_fingerprint = owner->recipe_fingerprint;
    owner->view.catalogue_identity = owner->catalogue_identity;
    owner->view.catalogue_version = config->recipe->catalogue_version;
    owner->view.record_identity = owner->record_identity;
    owner->view.transformation_identity = owner->transformation_identity;
    owner->view.transformation_version = record->transformation_version;
    owner->view.provider_identity = owner->provider_identity;
    owner->view.provider_version = config->provider_version;
    owner->view.clang_identity = owner->clang_identity;
    owner->view.target = record->target;
    owner->view.target.location_identity = owner->target_location_identity;
    owner->view.target.file = owner->target_file;
    owner->view.target.spelling_identity = owner->target_spelling_identity;
    owner->view.projection_complete = true;
    owner->view.probabilistic_authority = false;
    owner->view.snapshot_modified = false;
    owner->view.candidate_materialized = false;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static evo_project_transformation_status_t evo_transform_finish(
    const evo_project_transformation_apply_config_t *config,
    evo_project_transformation_application_owner_t *owner,
    evo_project_transformation_status_t status,
    evo_project_transformation_application_t *application)
{
    const evo_project_status_t snapshot_status =
        evo_project_snapshot_verify_baseline(config->baseline->private_owner);

    if (snapshot_status != EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_CHANGED;
    }
    if (status == EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        *application = owner->view;
        application->private_owner = owner;
        return status;
    }
    evo_project_transformation_application_owner_destroy(owner);
    evo_project_release(owner);
    return status;
}

evo_project_transformation_status_t evo_project_transformation_apply(
    const evo_project_transformation_apply_config_t *config,
    evo_project_transformation_application_t *application)
{
    const evo_project_baseline_owner_t *baseline_owner;
    const evo_project_file_record_t *file;
    const evo_project_recipe_record_t *record;
    const evo_project_transformation_capability_t *capability;
    evo_project_transformation_application_owner_t *owner;
    evo_project_transformation_ast_result_t ast = {0};
    evo_project_transformation_request_t request = {0};
    evo_project_transformation_status_t status;
    evo_project_status_t snapshot_status;
    evo_project_fingerprint_t source_fingerprint;
    char source_fingerprint_text[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    unsigned char *source = NULL;
    size_t source_size = 0U;

    status = evo_transform_preflight(config, application);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    baseline_owner = config->baseline->private_owner;
    record = evo_transform_find_record(config->recipe, config->record_identity);
    if (record == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RECORD_NOT_FOUND;
    }
    capability = evo_project_transformation_find_capability(
        record->transformation_identity, record->transformation_version);
    if (capability == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_CATALOGUE_INVALID;
    }
    if (record->parameter_count > config->limits.max_parameters ||
        !evo_transform_text_valid(
            record->identity, config->limits.max_string_bytes) ||
        !evo_transform_text_valid(
            record->transformation_identity,
            config->limits.max_string_bytes) ||
        !evo_transform_text_valid(
            record->target.location_identity,
            config->limits.max_string_bytes) ||
        !evo_transform_text_valid(
            record->target.file, config->limits.max_path_bytes) ||
        (record->target.spelling_identity != NULL &&
         !evo_transform_text_valid(
             record->target.spelling_identity,
             config->limits.max_string_bytes))) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    file = evo_transform_find_file(baseline_owner, record->target.file);
    if (file == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RECIPE_STALE;
    }
    status = evo_transform_read_source(
        baseline_owner, file, &config->limits, &source, &source_size);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    evo_project_fingerprint_begin(&source_fingerprint);
    evo_project_fingerprint_bytes(&source_fingerprint, source, source_size);
    evo_project_fingerprint_format(
        source_fingerprint.value, source_fingerprint_text);
    request.schema_version =
        EVO_PROJECT_TRANSFORMATION_PROVIDER_CONTRACT_VERSION;
    request.baseline_fingerprint = config->baseline->baseline_fingerprint;
    request.analysis_fingerprint = config->analysis->analysis_fingerprint;
    request.recipe_fingerprint = config->recipe->recipe_fingerprint;
    request.snapshot_path = baseline_owner->snapshot_path;
    request.record_identity = record->identity;
    request.target = &record->target;
    request.transformation_identity = record->transformation_identity;
    request.transformation_version = record->transformation_version;
    request.parameter_count = record->parameter_count;
    request.parameters = record->parameters;
    request.source_size = source_size;
    request.source_fingerprint = source_fingerprint_text;
    request.limits = config->limits;
    request.network_access = false;
    status = config->provider(&request, config->provider_context, &ast);
    snapshot_status = evo_project_snapshot_verify_baseline(baseline_owner);
    if (snapshot_status != EVO_PROJECT_SUCCESS) {
        evo_project_release(source);
        return EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_CHANGED;
    }
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        evo_project_release(source);
        return EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER;
    }
    if (!evo_transform_authority_current(config)) {
        evo_project_release(source);
        return EVO_PROJECT_TRANSFORMATION_ERROR_RECIPE_STALE;
    }
    if (!evo_transform_ast_matches_recipe_range(
            source, source_size, &record->target, &ast)) {
        evo_project_release(source);
        return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
    }
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        evo_project_release(source);
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    status = evo_transform_copy_common(config, record, owner);
    if (status == EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        status = evo_project_transformation_model_apply(
            record,
            capability,
            source,
            source_size,
            &ast,
            &config->limits,
            owner);
    }
    evo_project_release(source);
    if (status == EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        status = evo_project_transformation_application_generate_evidence(
            &config->limits,
            baseline_owner->manifest.budget.max_evidence_bytes,
            owner);
    }
    return evo_transform_finish(config, owner, status, application);
}

void evo_project_transformation_application_destroy(
    evo_project_transformation_application_t *application)
{
    evo_project_transformation_application_owner_t *owner;

    if (application == NULL) {
        return;
    }
    owner = application->private_owner;
    if (owner != NULL) {
        evo_project_transformation_application_owner_destroy(owner);
        evo_project_release(owner);
    }
    *application = (evo_project_transformation_application_t){0};
}

const char *evo_project_transformation_status_name(
    evo_project_transformation_status_t status)
{
    switch (status) {
    case EVO_PROJECT_TRANSFORMATION_SUCCESS:
        return "success";
    case EVO_PROJECT_TRANSFORMATION_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_TRANSFORMATION_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_INELIGIBLE:
        return "baseline-ineligible";
    case EVO_PROJECT_TRANSFORMATION_ERROR_ANALYSIS_STALE:
        return "analysis-stale";
    case EVO_PROJECT_TRANSFORMATION_ERROR_RECIPE_STALE:
        return "recipe-stale";
    case EVO_PROJECT_TRANSFORMATION_ERROR_CATALOGUE_INVALID:
        return "catalogue-invalid";
    case EVO_PROJECT_TRANSFORMATION_ERROR_RECORD_NOT_FOUND:
        return "record-not-found";
    case EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER:
        return "provider";
    case EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED:
        return "ast-malformed";
    case EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE:
        return "not-applicable";
    case EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_MACRO:
        return "unsupported-macro";
    case EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_COMMENT:
        return "unsupported-comment";
    case EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_PREPROCESSOR:
        return "unsupported-preprocessor";
    case EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_EXTENSION:
        return "unsupported-extension";
    case EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_ALIAS_ASSUMPTION:
        return "unsupported-alias-assumption";
    case EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET:
        return "ambiguous-target";
    case EVO_PROJECT_TRANSFORMATION_ERROR_SOURCE_IO:
        return "source-io";
    case EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_CHANGED:
        return "baseline-changed";
    case EVO_PROJECT_TRANSFORMATION_ERROR_EVIDENCE:
        return "evidence";
    case EVO_PROJECT_TRANSFORMATION_ERROR_STATE:
    default:
        return "state";
    }
}

const char *evo_project_transformation_ast_form_name(
    evo_project_transformation_ast_form_t form)
{
    switch (form) {
    case EVO_PROJECT_AST_ASSIGNMENT_BINARY:
        return "assignment-binary";
    case EVO_PROJECT_AST_ASSIGNMENT_COMPOUND:
        return "assignment-compound";
    case EVO_PROJECT_AST_UNSIGNED_MULTIPLY_POWER_OF_TWO:
        return "unsigned-multiply-power-of-two";
    case EVO_PROJECT_AST_UNSIGNED_SHIFT_POWER_OF_TWO:
        return "unsigned-shift-power-of-two";
    case EVO_PROJECT_AST_DOUBLE_NEGATED_CONDITION:
        return "double-negated-condition";
    case EVO_PROJECT_AST_SCALAR_CONDITION:
    default:
        return "scalar-condition";
    }
}

const char *evo_project_transformation_operator_name(
    evo_project_transformation_operator_t operator_kind)
{
    switch (operator_kind) {
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_ADD:
        return "add";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_AND:
        return "bitwise-and";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_OR:
        return "bitwise-or";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_XOR:
        return "bitwise-xor";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_MULTIPLY:
        return "multiply";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_SUBTRACT:
        return "subtract";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_SHIFT_LEFT:
        return "shift-left";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_NONE:
    default:
        return "none";
    }
}

const char *evo_project_transformation_condition_context_name(
    evo_project_transformation_condition_context_t context)
{
    switch (context) {
    case EVO_PROJECT_TRANSFORMATION_CONDITION_DO_WHILE:
        return "do-while";
    case EVO_PROJECT_TRANSFORMATION_CONDITION_FOR:
        return "for";
    case EVO_PROJECT_TRANSFORMATION_CONDITION_IF:
        return "if";
    case EVO_PROJECT_TRANSFORMATION_CONDITION_WHILE:
        return "while";
    case EVO_PROJECT_TRANSFORMATION_CONDITION_NONE:
    default:
        return "none";
    }
}
