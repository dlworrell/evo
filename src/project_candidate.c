#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "internal/project_candidate.h"
#include "internal/project_candidate_internal.h"
#include "internal/project_candidate_owner.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"
#include "internal/project_snapshot.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static size_t evo_candidate_line_count(const unsigned char *bytes, size_t size)
{
    size_t count = 0U;
    size_t index;

    if (size == 0U) {
        return 0U;
    }
    for (index = 0U; index < size; index += 1U) {
        if (bytes[index] == (unsigned char)'\n') {
            count += 1U;
        }
    }
    if (bytes[size - 1U] != (unsigned char)'\n') {
        count += 1U;
    }
    return count;
}

static bool evo_candidate_append_diff_lines(
    evo_candidate_buffer_t *patch,
    char prefix,
    const unsigned char *bytes,
    size_t size)
{
    size_t start = 0U;

    while (start < size) {
        size_t end = start;
        bool has_newline;

        while (end < size && bytes[end] != (unsigned char)'\n') {
            end += 1U;
        }
        has_newline = end < size && bytes[end] == (unsigned char)'\n';
        if (!evo_candidate_buffer_append_bytes(patch, &prefix, 1U) ||
            !evo_candidate_buffer_append_bytes(patch, bytes + start, end - start) ||
            !evo_candidate_buffer_append_text(patch, "\n")) {
            return false;
        }
        if (!has_newline &&
            !evo_candidate_buffer_append_text(patch, "\\ No newline at end of file\n")) {
            return false;
        }
        start = has_newline ? end + 1U : end;
    }
    return true;
}

static bool evo_candidate_append_file_patch(
    evo_candidate_buffer_t *patch,
    const char *path,
    const unsigned char *before,
    size_t before_size,
    const unsigned char *after,
    size_t after_size)
{
    const size_t before_lines = evo_candidate_line_count(before, before_size);
    const size_t after_lines = evo_candidate_line_count(after, after_size);

    if (!evo_candidate_buffer_append_text(patch, "diff --git a/") ||
        !evo_candidate_buffer_append_text(patch, path) ||
        !evo_candidate_buffer_append_text(patch, " b/") ||
        !evo_candidate_buffer_append_text(patch, path) ||
        !evo_candidate_buffer_append_text(patch, "\n--- a/") ||
        !evo_candidate_buffer_append_text(patch, path) ||
        !evo_candidate_buffer_append_text(patch, "\n+++ b/") ||
        !evo_candidate_buffer_append_text(patch, path) ||
        !evo_candidate_buffer_append_text(patch, "\n@@ -")) {
        return false;
    }
    if (before_lines == 0U) {
        if (!evo_candidate_buffer_append_text(patch, "0,0")) {
            return false;
        }
    } else if (!evo_candidate_buffer_append_text(patch, "1,") ||
               !evo_candidate_buffer_append_size(patch, before_lines)) {
        return false;
    }
    if (!evo_candidate_buffer_append_text(patch, " +")) {
        return false;
    }
    if (after_lines == 0U) {
        if (!evo_candidate_buffer_append_text(patch, "0,0")) {
            return false;
        }
    } else if (!evo_candidate_buffer_append_text(patch, "1,") ||
               !evo_candidate_buffer_append_size(patch, after_lines)) {
        return false;
    }
    return evo_candidate_buffer_append_text(patch, " @@\n") &&
           evo_candidate_append_diff_lines(patch, '-', before, before_size) &&
           evo_candidate_append_diff_lines(patch, '+', after, after_size);
}

static evo_project_candidate_status_t evo_candidate_record_changed_file(
    evo_project_candidate_owner_t *owner,
    const evo_project_file_record_t *record,
    const unsigned char *candidate_bytes,
    size_t candidate_size,
    size_t edit_count)
{
    evo_project_candidate_changed_file_t *changed =
        &owner->changed_files[owner->changed_file_count];
    evo_project_fingerprint_t fingerprint;
    char *path = evo_candidate_duplicate(record->path);

    if (path == NULL) {
        return EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
    }
    owner->changed_paths[owner->changed_file_count] = path;
    changed->path = path;
    changed->before_size = record->size;
    changed->after_size = (uint64_t)candidate_size;
    evo_project_fingerprint_format(record->content_fingerprint, changed->before_fingerprint);
    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(&fingerprint, candidate_bytes, candidate_size);
    evo_project_fingerprint_format(fingerprint.value, changed->after_fingerprint);
    changed->edit_count = edit_count;
    owner->changed_file_count += 1U;
    return EVO_PROJECT_CANDIDATE_SUCCESS;
}

static evo_project_candidate_status_t evo_candidate_write_workspace_file(
    int workspace_fd,
    const evo_project_file_record_t *record,
    const unsigned char *bytes,
    size_t size)
{
    int file_fd = -1;
    evo_project_candidate_status_t status = evo_candidate_open_output_file(
        workspace_fd, record->path, (mode_t)0600, &file_fd);

    if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
        return status;
    }
    status = evo_candidate_write_all(file_fd, bytes, size);
    if (status == EVO_PROJECT_CANDIDATE_SUCCESS && fsync(file_fd) != 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    if (status == EVO_PROJECT_CANDIDATE_SUCCESS &&
        fchmod(file_fd, (mode_t)(record->source_mode & 0555U)) != 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    if (close(file_fd) != 0 && status == EVO_PROJECT_CANDIDATE_SUCCESS) {
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    return status;
}

static evo_project_candidate_status_t evo_candidate_materialize_files(
    const evo_project_candidate_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    int workspace_fd,
    evo_project_candidate_owner_t *owner,
    evo_candidate_buffer_t *patch,
    evo_project_fingerprint_t *candidate_fingerprint)
{
    evo_candidate_edit_ref_t *edits;
    uint64_t total_candidate_bytes = 0U;
    size_t file_index;
    evo_project_candidate_status_t status = EVO_PROJECT_CANDIDATE_SUCCESS;

    edits = evo_project_allocate_zeroed(config->application_count, sizeof(*edits));
    if (edits == NULL) {
        return EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
    }
    evo_project_fingerprint_begin(candidate_fingerprint);
    evo_project_fingerprint_string(candidate_fingerprint, "catalyst.evo-project-candidate.v1");
    evo_project_fingerprint_string(candidate_fingerprint, config->baseline->baseline_fingerprint);
    evo_project_fingerprint_string(candidate_fingerprint, config->recipe->recipe_fingerprint);
    evo_project_fingerprint_u64(candidate_fingerprint, (uint64_t)baseline_owner->file_count);
    for (file_index = 0U; file_index < baseline_owner->file_count; file_index += 1U) {
        const evo_project_file_record_t *record = &baseline_owner->files[file_index];
        unsigned char *source = NULL;
        unsigned char *candidate_bytes = NULL;
        size_t source_size = 0U;
        size_t candidate_size = 0U;
        size_t edit_count;
        evo_project_fingerprint_t file_fingerprint;

        if (!evo_candidate_relative_path_valid(record->path, config->limits.max_path_bytes)) {
            status = EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID;
            goto finish_file;
        }
        status = evo_candidate_read_snapshot_file(
            baseline_owner, record, &config->limits, &source, &source_size);
        if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
            goto finish_file;
        }
        edit_count = evo_candidate_collect_file_edits(config, record->path, edits);
        if (edit_count > 0U) {
            status = evo_candidate_apply_edits(
                config,
                source,
                source_size,
                edits,
                edit_count,
                &candidate_bytes,
                &candidate_size);
        } else {
            size_t index;

            candidate_bytes = evo_project_allocate_zeroed(source_size + 1U, sizeof(*candidate_bytes));
            if (candidate_bytes == NULL) {
                status = EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
                goto finish_file;
            }
            for (index = 0U; index < source_size; index += 1U) {
                candidate_bytes[index] = source[index];
            }
            candidate_bytes[source_size] = '\0';
            candidate_size = source_size;
        }
        if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
            goto finish_file;
        }
        if ((uint64_t)candidate_size > UINT64_MAX - total_candidate_bytes) {
            status = EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
            goto finish_file;
        }
        total_candidate_bytes += (uint64_t)candidate_size;
        if (total_candidate_bytes > (uint64_t)config->limits.max_total_file_bytes) {
            status = EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
            goto finish_file;
        }
        status = evo_candidate_write_workspace_file(
            workspace_fd, record, candidate_bytes, candidate_size);
        if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
            goto finish_file;
        }
        evo_project_fingerprint_begin(&file_fingerprint);
        evo_project_fingerprint_bytes(&file_fingerprint, candidate_bytes, candidate_size);
        evo_project_fingerprint_string(candidate_fingerprint, record->path);
        evo_project_fingerprint_u64(candidate_fingerprint, (uint64_t)candidate_size);
        evo_project_fingerprint_u64(candidate_fingerprint, (uint64_t)record->source_mode);
        evo_project_fingerprint_u64(candidate_fingerprint, file_fingerprint.value);
        if (edit_count > 0U) {
            status = evo_candidate_record_changed_file(
                owner, record, candidate_bytes, candidate_size, edit_count);
            if (status == EVO_PROJECT_CANDIDATE_SUCCESS) {
                const bool patch_appended = evo_candidate_append_file_patch(
                    patch,
                    record->path,
                    source,
                    source_size,
                    candidate_bytes,
                    candidate_size);

                if (!patch_appended) {
                    status = EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
                }
            }
        }

    finish_file:
        evo_project_release(candidate_bytes);
        evo_project_release(source);
        if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
            break;
        }
    }
    evo_project_release(edits);
    if (status == EVO_PROJECT_CANDIDATE_SUCCESS) {
        evo_project_fingerprint_bytes(candidate_fingerprint, patch->bytes, patch->size);
    }
    return status;
}

static bool evo_candidate_build_json(
    const evo_project_candidate_config_t *config,
    const evo_project_candidate_owner_t *owner,
    const char *candidate_fingerprint,
    evo_candidate_buffer_t *json)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(json, "{\"schema_version\":1,\"baseline_fingerprint\":") ||
        !evo_candidate_buffer_append_json_string(json, config->baseline->baseline_fingerprint) ||
        !evo_candidate_buffer_append_text(json, ",\"recipe_fingerprint\":") ||
        !evo_candidate_buffer_append_json_string(json, config->recipe->recipe_fingerprint) ||
        !evo_candidate_buffer_append_text(json, ",\"candidate_fingerprint\":") ||
        !evo_candidate_buffer_append_json_string(json, candidate_fingerprint) ||
        !evo_candidate_buffer_append_text(json, ",\"workspace_policy\":") ||
        !evo_candidate_buffer_append_json_string(
            json, evo_project_candidate_workspace_policy_name(config->workspace_policy)) ||
        !evo_candidate_buffer_append_text(json, ",\"file_count\":") ||
        !evo_candidate_buffer_append_size(json, config->baseline->file_count) ||
        !evo_candidate_buffer_append_text(json, ",\"changed_files\":[")) {
        return false;
    }
    for (index = 0U; index < owner->changed_file_count; index += 1U) {
        const evo_project_candidate_changed_file_t *changed = &owner->changed_files[index];

        if (index > 0U && !evo_candidate_buffer_append_text(json, ",")) {
            return false;
        }
        if (!evo_candidate_buffer_append_text(json, "{\"path\":") ||
            !evo_candidate_buffer_append_json_string(json, changed->path) ||
            !evo_candidate_buffer_append_text(json, ",\"before_size\":") ||
            !evo_candidate_buffer_append_u64(json, changed->before_size) ||
            !evo_candidate_buffer_append_text(json, ",\"after_size\":") ||
            !evo_candidate_buffer_append_u64(json, changed->after_size) ||
            !evo_candidate_buffer_append_text(json, ",\"before_fingerprint\":") ||
            !evo_candidate_buffer_append_json_string(json, changed->before_fingerprint) ||
            !evo_candidate_buffer_append_text(json, ",\"after_fingerprint\":") ||
            !evo_candidate_buffer_append_json_string(json, changed->after_fingerprint) ||
            !evo_candidate_buffer_append_text(json, ",\"edit_count\":") ||
            !evo_candidate_buffer_append_size(json, changed->edit_count) ||
            !evo_candidate_buffer_append_text(json, "}")) {
            return false;
        }
    }
    return evo_candidate_buffer_append_text(json, "],\"patch_bytes\":") &&
           evo_candidate_buffer_append_size(json, owner->patch_size) &&
           evo_candidate_buffer_append_text(
               json,
               ",\"projection_complete\":true,\"probabilistic_authority\":false,"
               "\"source_modified\":false,\"snapshot_modified\":false}\n");
}

static bool evo_candidate_build_markdown(
    const evo_project_candidate_config_t *config,
    const evo_project_candidate_owner_t *owner,
    const char *candidate_fingerprint,
    evo_candidate_buffer_t *markdown)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(markdown, "# EVO candidate materialization\n\n") ||
        !evo_candidate_buffer_append_text(markdown, "- Schema: `1`\n- Baseline: `") ||
        !evo_candidate_buffer_append_text(markdown, config->baseline->baseline_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Recipe: `") ||
        !evo_candidate_buffer_append_text(markdown, config->recipe->recipe_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Candidate: `") ||
        !evo_candidate_buffer_append_text(markdown, candidate_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Workspace policy: `") ||
        !evo_candidate_buffer_append_text(
            markdown, evo_project_candidate_workspace_policy_name(config->workspace_policy)) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Changed files: `") ||
        !evo_candidate_buffer_append_size(markdown, owner->changed_file_count) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Patch bytes: `") ||
        !evo_candidate_buffer_append_size(markdown, owner->patch_size) ||
        !evo_candidate_buffer_append_text(
            markdown,
            "`\n- Source modified: `false`\n- Snapshot modified: `false`\n"
            "- Probabilistic authority: `false`\n\n## Changed files\n\n")) {
        return false;
    }
    if (owner->changed_file_count == 0U &&
        !evo_candidate_buffer_append_text(markdown, "No source files changed.\n")) {
        return false;
    }
    for (index = 0U; index < owner->changed_file_count; index += 1U) {
        const evo_project_candidate_changed_file_t *changed = &owner->changed_files[index];

        if (!evo_candidate_buffer_append_text(markdown, "- `") ||
            !evo_candidate_buffer_append_text(markdown, changed->path) ||
            !evo_candidate_buffer_append_text(markdown, "`: ") ||
            !evo_candidate_buffer_append_size(markdown, changed->edit_count) ||
            !evo_candidate_buffer_append_text(markdown, " edit(s), `") ||
            !evo_candidate_buffer_append_text(markdown, changed->before_fingerprint) ||
            !evo_candidate_buffer_append_text(markdown, "` -> `") ||
            !evo_candidate_buffer_append_text(markdown, changed->after_fingerprint) ||
            !evo_candidate_buffer_append_text(markdown, "`\n")) {
            return false;
        }
    }
    return true;
}

static evo_project_candidate_status_t evo_candidate_write_named_file(
    int directory_fd,
    const char *name,
    const char *bytes,
    size_t size)
{
    int file_fd = openat(
        directory_fd,
        name,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    evo_project_candidate_status_t status;

    if (file_fd < 0) {
        return EVO_PROJECT_CANDIDATE_ERROR_EVIDENCE;
    }
    status = evo_candidate_write_all(file_fd, (const unsigned char *)bytes, size);
    if (status == EVO_PROJECT_CANDIDATE_SUCCESS && fsync(file_fd) != 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_EVIDENCE;
    }
    if (close(file_fd) != 0 && status == EVO_PROJECT_CANDIDATE_SUCCESS) {
        status = EVO_PROJECT_CANDIDATE_ERROR_EVIDENCE;
    }
    return status;
}

static evo_project_candidate_status_t evo_candidate_create_stage(
    const char *output_path,
    char **stage_path,
    char **workspace_path,
    int *output_fd,
    int *stage_fd,
    int *workspace_fd,
    bool *output_created)
{
    const char marker[] = "incomplete\n";
    int marker_fd = -1;
    evo_project_candidate_status_t status = EVO_PROJECT_CANDIDATE_SUCCESS;

    if (mkdir(output_path, 0700) != 0) {
        return errno == EEXIST ? EVO_PROJECT_CANDIDATE_ERROR_OUTPUT_EXISTS
                               : EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    *output_created = true;
    *output_fd = open(output_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (*output_fd < 0) {
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    marker_fd = openat(
        *output_fd,
        ".evo-incomplete-v1",
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    if (marker_fd < 0 ||
        evo_candidate_write_all(
            marker_fd, (const unsigned char *)marker, sizeof(marker) - 1U) !=
            EVO_PROJECT_CANDIDATE_SUCCESS ||
        fsync(marker_fd) != 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
        goto finish;
    }
    if (close(marker_fd) != 0) {
        marker_fd = -1;
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
        goto finish;
    }
    marker_fd = -1;
    if (mkdirat(*output_fd, ".evo-stage-v1", 0700) != 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
        goto finish;
    }
    *stage_fd = openat(
        *output_fd,
        ".evo-stage-v1",
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
        0);
    if (*stage_fd < 0 || mkdirat(*stage_fd, "workspace", 0700) != 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
        goto finish;
    }
    *workspace_fd = openat(
        *stage_fd,
        "workspace",
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
        0);
    if (*workspace_fd < 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
        goto finish;
    }
    *stage_path = evo_candidate_join_path(output_path, ".evo-stage-v1");
    if (*stage_path != NULL) {
        *workspace_path = evo_candidate_join_path(*stage_path, "workspace");
    }
    if (*stage_path == NULL || *workspace_path == NULL) {
        status = EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
    }

finish:
    if (marker_fd >= 0) {
        (void)close(marker_fd);
    }
    return status;
}

static evo_project_candidate_status_t evo_candidate_publish(
    const evo_project_candidate_config_t *config,
    evo_project_candidate_owner_t *owner,
    const char *stage_path,
    const char *workspace_path,
    int output_fd,
    int stage_fd,
    int workspace_fd)
{
    char *final_workspace = NULL;
    char *stage_patch = NULL;
    char *stage_json = NULL;
    char *stage_markdown = NULL;
    char *final_patch = NULL;
    char *final_json = NULL;
    char *final_markdown = NULL;
    evo_project_candidate_status_t status = EVO_PROJECT_CANDIDATE_SUCCESS;

    if (fsync(workspace_fd) != 0 || fsync(stage_fd) != 0) {
        return EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
    }
    stage_patch = evo_candidate_join_path(stage_path, "candidate.patch");
    stage_json = evo_candidate_join_path(stage_path, "candidate.json");
    stage_markdown = evo_candidate_join_path(stage_path, "candidate.md");
    final_patch = evo_candidate_join_path(owner->output_path, "candidate.patch");
    final_json = evo_candidate_join_path(owner->output_path, "candidate.json");
    final_markdown = evo_candidate_join_path(owner->output_path, "candidate.md");
    if (stage_patch == NULL || stage_json == NULL || stage_markdown == NULL ||
        final_patch == NULL || final_json == NULL || final_markdown == NULL) {
        status = EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
        goto finish;
    }
    if (config->workspace_policy == EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN) {
        final_workspace = evo_candidate_join_path(owner->output_path, "candidate");
        if (final_workspace == NULL) {
            status = EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
            goto finish;
        }
        if (rename(workspace_path, final_workspace) != 0) {
            status = EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO;
            goto finish;
        }
    } else {
        status = evo_candidate_remove_tree(workspace_path);
        if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
            goto finish;
        }
    }
    if (rename(stage_patch, final_patch) != 0 || rename(stage_json, final_json) != 0 ||
        rename(stage_markdown, final_markdown) != 0 || rmdir(stage_path) != 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_EVIDENCE;
        goto finish;
    }
    if (fsync(output_fd) != 0 || unlinkat(output_fd, ".evo-incomplete-v1", 0) != 0 ||
        fsync(output_fd) != 0) {
        status = EVO_PROJECT_CANDIDATE_ERROR_EVIDENCE;
        goto finish;
    }
    if (config->workspace_policy == EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN) {
        owner->workspace_path = final_workspace;
        final_workspace = NULL;
    }

finish:
    evo_project_release(final_workspace);
    evo_project_release(stage_patch);
    evo_project_release(stage_json);
    evo_project_release(stage_markdown);
    evo_project_release(final_patch);
    evo_project_release(final_json);
    evo_project_release(final_markdown);
    return status;
}

static void evo_candidate_owner_destroy(evo_project_candidate_owner_t *owner)
{
    size_t index;

    if (owner == NULL) {
        return;
    }
    for (index = 0U; index < owner->changed_file_count; index += 1U) {
        evo_project_release(owner->changed_paths[index]);
    }
    evo_project_release(owner->changed_paths);
    evo_project_release(owner->changed_files);
    evo_project_release(owner->audit_markdown);
    evo_project_release(owner->canonical_json);
    evo_project_release(owner->patch);
    evo_project_release(owner->workspace_path);
    evo_project_release(owner->output_path);
    evo_project_release(owner->recipe_fingerprint);
    evo_project_release(owner->baseline_fingerprint);
}

evo_project_candidate_status_t evo_project_candidate_materialize(
    const evo_project_candidate_config_t *config,
    evo_project_candidate_t *candidate)
{
    const evo_project_baseline_owner_t *baseline_owner = NULL;
    const evo_project_recipe_owner_t *recipe_owner = NULL;
    evo_project_candidate_owner_t *owner = NULL;
    evo_candidate_buffer_t patch = {0};
    evo_candidate_buffer_t json = {0};
    evo_candidate_buffer_t markdown = {0};
    evo_project_fingerprint_t candidate_fingerprint;
    char candidate_fingerprint_text[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char *normalized_output = NULL;
    char *stage_path = NULL;
    char *workspace_path = NULL;
    int output_fd = -1;
    int stage_fd = -1;
    int workspace_fd = -1;
    bool output_created = false;
    evo_project_candidate_status_t status;
    evo_project_status_t snapshot_status;

    status = evo_candidate_preflight(
        config,
        candidate,
        &baseline_owner,
        &recipe_owner,
        &normalized_output);
    (void)recipe_owner;
    if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
        return status;
    }
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        evo_project_release(normalized_output);
        return EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
    }
    owner->baseline_fingerprint = evo_candidate_duplicate(config->baseline->baseline_fingerprint);
    owner->recipe_fingerprint = evo_candidate_duplicate(config->recipe->recipe_fingerprint);
    owner->output_path = normalized_output;
    normalized_output = NULL;
    owner->changed_files = evo_project_allocate_zeroed(
        config->baseline->file_count, sizeof(*owner->changed_files));
    owner->changed_paths = evo_project_allocate_zeroed(
        config->baseline->file_count, sizeof(*owner->changed_paths));
    {
        const size_t baseline_evidence_limit =
            baseline_owner->manifest.budget.max_evidence_bytes;
        const size_t evidence_limit =
            config->limits.max_evidence_bytes < baseline_evidence_limit
                ? config->limits.max_evidence_bytes
                : baseline_evidence_limit;

        if (owner->baseline_fingerprint == NULL || owner->recipe_fingerprint == NULL ||
            owner->changed_files == NULL || owner->changed_paths == NULL ||
            !evo_candidate_buffer_open(&patch, config->limits.max_patch_bytes) ||
            !evo_candidate_buffer_open(&json, evidence_limit) ||
            !evo_candidate_buffer_open(&markdown, evidence_limit)) {
            status = EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY;
            goto finish;
        }
    }
    status = evo_candidate_create_stage(
        owner->output_path,
        &stage_path,
        &workspace_path,
        &output_fd,
        &stage_fd,
        &workspace_fd,
        &output_created);
    if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
        goto finish;
    }
    status = evo_candidate_materialize_files(
        config,
        baseline_owner,
        workspace_fd,
        owner,
        &patch,
        &candidate_fingerprint);
    if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
        goto finish;
    }
    snapshot_status = evo_project_snapshot_verify_baseline(baseline_owner);
    if (snapshot_status != EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_CANDIDATE_ERROR_BASELINE_CHANGED;
        goto finish;
    }
    evo_project_fingerprint_format(candidate_fingerprint.value, candidate_fingerprint_text);
    owner->candidate_fingerprint = candidate_fingerprint.value;
    owner->patch = patch.bytes;
    owner->patch_size = patch.size;
    patch.bytes = NULL;
    patch.size = 0U;
    patch.capacity = 0U;
    if (!evo_candidate_build_json(config, owner, candidate_fingerprint_text, &json) ||
        !evo_candidate_build_markdown(config, owner, candidate_fingerprint_text, &markdown)) {
        status = EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT;
        goto finish;
    }
    owner->canonical_json = json.bytes;
    owner->canonical_json_size = json.size;
    json.bytes = NULL;
    json.size = 0U;
    json.capacity = 0U;
    owner->audit_markdown = markdown.bytes;
    owner->audit_markdown_size = markdown.size;
    markdown.bytes = NULL;
    markdown.size = 0U;
    markdown.capacity = 0U;
    if (evo_candidate_write_named_file(
            stage_fd, "candidate.patch", owner->patch, owner->patch_size) !=
            EVO_PROJECT_CANDIDATE_SUCCESS ||
        evo_candidate_write_named_file(
            stage_fd,
            "candidate.json",
            owner->canonical_json,
            owner->canonical_json_size) != EVO_PROJECT_CANDIDATE_SUCCESS ||
        evo_candidate_write_named_file(
            stage_fd,
            "candidate.md",
            owner->audit_markdown,
            owner->audit_markdown_size) != EVO_PROJECT_CANDIDATE_SUCCESS) {
        status = EVO_PROJECT_CANDIDATE_ERROR_EVIDENCE;
        goto finish;
    }
    status = evo_candidate_publish(
        config,
        owner,
        stage_path,
        workspace_path,
        output_fd,
        stage_fd,
        workspace_fd);
    if (status != EVO_PROJECT_CANDIDATE_SUCCESS) {
        goto finish;
    }
    snapshot_status = evo_project_snapshot_verify_baseline(baseline_owner);
    if (snapshot_status != EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_CANDIDATE_ERROR_BASELINE_CHANGED;
        goto finish;
    }
    owner->view.schema_version = EVO_PROJECT_CANDIDATE_SCHEMA_VERSION;
    owner->view.baseline_fingerprint = owner->baseline_fingerprint;
    owner->view.recipe_fingerprint = owner->recipe_fingerprint;
    evo_project_fingerprint_format(owner->candidate_fingerprint, owner->view.candidate_fingerprint);
    owner->view.output_path = owner->output_path;
    owner->view.workspace_path = owner->workspace_path;
    owner->view.workspace_policy = config->workspace_policy;
    owner->view.file_count = config->baseline->file_count;
    owner->view.changed_file_count = owner->changed_file_count;
    owner->view.changed_files = owner->changed_files;
    owner->view.patch_size = owner->patch_size;
    owner->view.patch = owner->patch;
    owner->view.canonical_json_size = owner->canonical_json_size;
    owner->view.canonical_json = owner->canonical_json;
    owner->view.audit_markdown_size = owner->audit_markdown_size;
    owner->view.audit_markdown = owner->audit_markdown;
    owner->view.projection_complete = true;
    owner->view.probabilistic_authority = false;
    owner->view.source_modified = false;
    owner->view.snapshot_modified = false;
    *candidate = owner->view;
    candidate->private_owner = owner;
    owner = NULL;
    status = EVO_PROJECT_CANDIDATE_SUCCESS;

finish:
    evo_candidate_buffer_close(&markdown);
    evo_candidate_buffer_close(&json);
    evo_candidate_buffer_close(&patch);
    if (workspace_fd >= 0) {
        (void)close(workspace_fd);
    }
    if (stage_fd >= 0) {
        (void)close(stage_fd);
    }
    if (output_fd >= 0) {
        (void)close(output_fd);
    }
    evo_project_release(workspace_path);
    evo_project_release(stage_path);
    evo_project_release(normalized_output);
    if (status != EVO_PROJECT_CANDIDATE_SUCCESS && output_created && owner != NULL &&
        owner->output_path != NULL) {
        (void)evo_candidate_remove_tree(owner->output_path);
    }
    if (owner != NULL) {
        evo_candidate_owner_destroy(owner);
        evo_project_release(owner);
    }
    return status;
}

void evo_project_candidate_destroy(evo_project_candidate_t *candidate)
{
    evo_project_candidate_owner_t *owner;

    if (candidate == NULL) {
        return;
    }
    owner = candidate->private_owner;
    if (owner != NULL) {
        evo_candidate_owner_destroy(owner);
        evo_project_release(owner);
    }
    *candidate = (evo_project_candidate_t){0};
}

const char *evo_project_candidate_status_name(evo_project_candidate_status_t status)
{
    switch (status) {
    case EVO_PROJECT_CANDIDATE_SUCCESS:
        return "success";
    case EVO_PROJECT_CANDIDATE_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_CANDIDATE_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_CANDIDATE_ERROR_BASELINE_INELIGIBLE:
        return "baseline-ineligible";
    case EVO_PROJECT_CANDIDATE_ERROR_BASELINE_CHANGED:
        return "baseline-changed";
    case EVO_PROJECT_CANDIDATE_ERROR_RECIPE_STALE:
        return "recipe-stale";
    case EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_STALE:
        return "application-stale";
    case EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_MISSING:
        return "application-missing";
    case EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_DUPLICATE:
        return "application-duplicate";
    case EVO_PROJECT_CANDIDATE_ERROR_CONFLICT:
        return "conflict";
    case EVO_PROJECT_CANDIDATE_ERROR_PATH_INVALID:
        return "path-invalid";
    case EVO_PROJECT_CANDIDATE_ERROR_OUTPUT_EXISTS:
        return "output-exists";
    case EVO_PROJECT_CANDIDATE_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_CANDIDATE_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_CANDIDATE_ERROR_SOURCE_IO:
        return "source-io";
    case EVO_PROJECT_CANDIDATE_ERROR_EVIDENCE:
        return "evidence";
    case EVO_PROJECT_CANDIDATE_ERROR_STATE:
    default:
        return "state";
    }
}

const char *evo_project_candidate_workspace_policy_name(
    evo_project_candidate_workspace_policy_t policy)
{
    switch (policy) {
    case EVO_PROJECT_CANDIDATE_WORKSPACE_DISCARD:
        return "discard";
    case EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN:
        return "retain";
    default:
        return "invalid";
    }
}
