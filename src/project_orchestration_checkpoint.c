#include "internal/project_orchestration_checkpoint.h"

#include "internal/project_runtime.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define EVO_ORCHESTRATION_CHECKPOINT_MAGIC_SIZE 8U
#define EVO_ORCHESTRATION_CHECKPOINT_CHECKSUM_OFFSET 24U
#define EVO_ORCHESTRATION_CHECKPOINT_CHECKSUM_SIZE 8U
#define EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT 10U

static const unsigned char evo_orchestration_checkpoint_magic[] = {
    (unsigned char)'E',
    (unsigned char)'V',
    (unsigned char)'O',
    (unsigned char)'O',
    (unsigned char)'R',
    (unsigned char)'C',
    (unsigned char)'1',
    0U};

typedef struct evo_orchestration_checkpoint_owner {
    unsigned char *bytes;
    size_t byte_count;
    evo_project_orchestration_checkpoint_t view;
} evo_orchestration_checkpoint_owner_t;

typedef struct evo_orchestration_checkpoint_cursor {
    unsigned char *bytes;
    size_t size;
    size_t position;
} evo_orchestration_checkpoint_cursor_t;

static bool evo_orchestration_checkpoint_text_valid(
    const char *text,
    size_t maximum,
    size_t *length_out)
{
    size_t length = 0U;

    if (text == NULL || maximum == 0U || length_out == NULL) {
        return false;
    }
    while (length <= maximum && text[length] != '\0') {
        const unsigned char value = (unsigned char)text[length];

        if (value < 0x20U || value == 0x7fU) {
            return false;
        }
        length += 1U;
    }
    if (length == 0U || length > maximum || length > UINT32_MAX) {
        return false;
    }
    *length_out = length;
    return true;
}

static bool evo_orchestration_checkpoint_limits_valid(
    const evo_project_orchestration_checkpoint_limits_t *limits)
{
    return limits != NULL && limits->max_string_bytes > 0U &&
           limits->max_core_checkpoint_bytes > 0U &&
           limits->max_checkpoint_bytes > 0U &&
           limits->max_core_checkpoint_bytes <= limits->max_checkpoint_bytes;
}

static bool evo_orchestration_checkpoint_identity_valid(
    const evo_project_orchestration_checkpoint_identity_t *identity,
    const evo_project_orchestration_checkpoint_limits_t *limits,
    size_t lengths[EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT])
{
    const char *values[EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT];
    size_t index;

    if (identity == NULL || limits == NULL || lengths == NULL ||
        identity->catalogue_version == 0U ||
        identity->recipe_schema_version == 0U ||
        identity->search_schema_version == 0U ||
        identity->mutation_policy_version == 0U ||
        identity->crossover_policy_version == 0U ||
        identity->repair_policy_version == 0U) {
        return false;
    }
    values[0] = identity->baseline_fingerprint;
    values[1] = identity->analysis_fingerprint;
    values[2] = identity->catalogue_identity;
    values[3] = identity->search_policy_identity;
    values[4] = identity->evaluation_provider_identity;
    values[5] = identity->orchestration_policy_identity;
    values[6] = identity->toolchain_identity;
    values[7] = identity->workload_identity;
    values[8] = identity->artifact_schema_identity;
    values[9] = identity->committed_lineage_fingerprint;
    for (index = 0U; index < EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT;
         index += 1U) {
        if (!evo_orchestration_checkpoint_text_valid(
                values[index], limits->max_string_bytes, &lengths[index])) {
            return false;
        }
    }
    return true;
}

static bool evo_orchestration_checked_add(
    size_t left,
    size_t right,
    size_t *sum)
{
    if (sum == NULL || right > SIZE_MAX - left) {
        return false;
    }
    *sum = left + right;
    return true;
}

static bool evo_orchestration_checkpoint_serialized_size(
    const size_t lengths[EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT],
    size_t core_checkpoint_size,
    const evo_project_orchestration_checkpoint_limits_t *limits,
    size_t *size_out)
{
    size_t size = 80U;
    size_t index;

    for (index = 0U; index < EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT;
         index += 1U) {
        size_t field_size;

        if (!evo_orchestration_checked_add(lengths[index], 1U, &field_size) ||
            !evo_orchestration_checked_add(field_size, 4U, &field_size) ||
            !evo_orchestration_checked_add(size, field_size, &size)) {
            return false;
        }
    }
    if (!evo_orchestration_checked_add(size, core_checkpoint_size, &size) ||
        size > limits->max_checkpoint_bytes) {
        return false;
    }
    *size_out = size;
    return true;
}

static bool evo_orchestration_write_bytes(
    evo_orchestration_checkpoint_cursor_t *cursor,
    const void *source,
    size_t count)
{
    const unsigned char *bytes = source;
    size_t index;

    if (cursor == NULL || source == NULL || count > cursor->size - cursor->position) {
        return false;
    }
    for (index = 0U; index < count; index += 1U) {
        cursor->bytes[cursor->position + index] = bytes[index];
    }
    cursor->position += count;
    return true;
}

static bool evo_orchestration_write_u32(
    evo_orchestration_checkpoint_cursor_t *cursor,
    uint32_t value)
{
    unsigned char bytes[4];

    bytes[0] = (unsigned char)(value & UINT32_C(0xff));
    bytes[1] = (unsigned char)((value >> 8U) & UINT32_C(0xff));
    bytes[2] = (unsigned char)((value >> 16U) & UINT32_C(0xff));
    bytes[3] = (unsigned char)((value >> 24U) & UINT32_C(0xff));
    return evo_orchestration_write_bytes(cursor, bytes, sizeof(bytes));
}

static bool evo_orchestration_write_u64(
    evo_orchestration_checkpoint_cursor_t *cursor,
    uint64_t value)
{
    unsigned char bytes[8];
    size_t index;

    for (index = 0U; index < sizeof(bytes); index += 1U) {
        bytes[index] = (unsigned char)(value >> (index * 8U));
    }
    return evo_orchestration_write_bytes(cursor, bytes, sizeof(bytes));
}

static bool evo_orchestration_write_string(
    evo_orchestration_checkpoint_cursor_t *cursor,
    const char *value,
    size_t length)
{
    unsigned char terminator = 0U;

    return evo_orchestration_write_u32(cursor, (uint32_t)length) &&
           evo_orchestration_write_bytes(cursor, value, length) &&
           evo_orchestration_write_bytes(cursor, &terminator, 1U);
}

static uint32_t evo_orchestration_read_u32_at(
    const unsigned char *bytes,
    size_t offset)
{
    return (uint32_t)bytes[offset] |
           ((uint32_t)bytes[offset + 1U] << 8U) |
           ((uint32_t)bytes[offset + 2U] << 16U) |
           ((uint32_t)bytes[offset + 3U] << 24U);
}

static uint64_t evo_orchestration_read_u64_at(
    const unsigned char *bytes,
    size_t offset)
{
    uint64_t value = 0U;
    size_t index;

    for (index = 0U; index < 8U; index += 1U) {
        value |= (uint64_t)bytes[offset + index] << (index * 8U);
    }
    return value;
}

static void evo_orchestration_write_u64_at(
    unsigned char *bytes,
    size_t offset,
    uint64_t value)
{
    size_t index;

    for (index = 0U; index < 8U; index += 1U) {
        bytes[offset + index] =
            (unsigned char)(value >> (index * 8U));
    }
}

static uint64_t evo_orchestration_checkpoint_integrity(
    const unsigned char *bytes,
    size_t size)
{
    evo_project_fingerprint_t fingerprint;
    unsigned char zeros[EVO_ORCHESTRATION_CHECKPOINT_CHECKSUM_SIZE] = {0};
    const size_t suffix_offset =
        EVO_ORCHESTRATION_CHECKPOINT_CHECKSUM_OFFSET +
        EVO_ORCHESTRATION_CHECKPOINT_CHECKSUM_SIZE;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(
        &fingerprint,
        bytes,
        EVO_ORCHESTRATION_CHECKPOINT_CHECKSUM_OFFSET);
    evo_project_fingerprint_bytes(&fingerprint, zeros, sizeof(zeros));
    if (size > suffix_offset) {
        evo_project_fingerprint_bytes(
            &fingerprint,
            bytes + suffix_offset,
            size - suffix_offset);
    }
    return fingerprint.value;
}

static bool evo_orchestration_checkpoint_magic_valid(
    const unsigned char *bytes,
    size_t size)
{
    size_t index;

    if (bytes == NULL || size < EVO_ORCHESTRATION_CHECKPOINT_MAGIC_SIZE) {
        return false;
    }
    for (index = 0U; index < EVO_ORCHESTRATION_CHECKPOINT_MAGIC_SIZE;
         index += 1U) {
        if (bytes[index] != evo_orchestration_checkpoint_magic[index]) {
            return false;
        }
    }
    return true;
}

static bool evo_orchestration_checkpoint_read_string(
    unsigned char *bytes,
    size_t size,
    size_t maximum,
    size_t *position,
    const char **value_out)
{
    uint32_t length;
    size_t end;

    if (bytes == NULL || position == NULL || value_out == NULL ||
        *position > size || size - *position < 4U) {
        return false;
    }
    length = evo_orchestration_read_u32_at(bytes, *position);
    *position += 4U;
    if (length == 0U || (size_t)length > maximum ||
        !evo_orchestration_checked_add(*position, (size_t)length, &end) ||
        !evo_orchestration_checked_add(end, 1U, &end) || end > size ||
        bytes[end - 1U] != 0U) {
        return false;
    }
    {
        size_t index;

        for (index = 0U; index < (size_t)length; index += 1U) {
            const unsigned char value = bytes[*position + index];

            if (value < 0x20U || value == 0x7fU || value == 0U) {
                return false;
            }
        }
    }
    *value_out = (const char *)(bytes + *position);
    *position = end;
    return true;
}

static bool evo_orchestration_checkpoint_parse(
    evo_orchestration_checkpoint_owner_t *owner,
    const evo_project_orchestration_checkpoint_limits_t *limits)
{
    unsigned char *bytes = owner->bytes;
    const size_t size = owner->byte_count;
    evo_project_orchestration_checkpoint_t *view = &owner->view;
    size_t position = 32U;
    uint64_t total_size;
    uint64_t core_size;

    if (size < 80U || !evo_orchestration_checkpoint_magic_valid(bytes, size)) {
        return false;
    }
    view->format_version = evo_orchestration_read_u32_at(bytes, 8U);
    view->integrity_algorithm = evo_orchestration_read_u32_at(bytes, 12U);
    total_size = evo_orchestration_read_u64_at(bytes, 16U);
    if (view->format_version !=
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_FORMAT_VERSION ||
        view->integrity_algorithm !=
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_INTEGRITY_FNV1A64 ||
        total_size != size) {
        return false;
    }
    view->identity.catalogue_version =
        evo_orchestration_read_u32_at(bytes, position);
    position += 4U;
    view->identity.recipe_schema_version =
        evo_orchestration_read_u32_at(bytes, position);
    position += 4U;
    view->identity.search_schema_version =
        evo_orchestration_read_u32_at(bytes, position);
    position += 4U;
    view->identity.mutation_policy_version =
        evo_orchestration_read_u32_at(bytes, position);
    position += 4U;
    view->identity.crossover_policy_version =
        evo_orchestration_read_u32_at(bytes, position);
    position += 4U;
    view->identity.repair_policy_version =
        evo_orchestration_read_u32_at(bytes, position);
    position += 4U;
    view->identity.random_seed = evo_orchestration_read_u64_at(bytes, position);
    position += 8U;
    view->identity.committed_generation =
        evo_orchestration_read_u64_at(bytes, position);
    position += 8U;
    core_size = evo_orchestration_read_u64_at(bytes, position);
    position += 8U;
    if (core_size == 0U || core_size > limits->max_core_checkpoint_bytes ||
        core_size > SIZE_MAX) {
        return false;
    }
    if (!evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.baseline_fingerprint) ||
        !evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.analysis_fingerprint) ||
        !evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.catalogue_identity) ||
        !evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.search_policy_identity) ||
        !evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.evaluation_provider_identity) ||
        !evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.orchestration_policy_identity) ||
        !evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.toolchain_identity) ||
        !evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.workload_identity) ||
        !evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.artifact_schema_identity) ||
        !evo_orchestration_checkpoint_read_string(
            bytes,
            size,
            limits->max_string_bytes,
            &position,
            &view->identity.committed_lineage_fingerprint) ||
        core_size > size - position ||
        position + (size_t)core_size != size) {
        return false;
    }
    view->core_checkpoint_size = (size_t)core_size;
    view->core_checkpoint = bytes + position;
    view->serialized_size = size;
    view->serialized = bytes;
    view->private_owner = owner;
    evo_project_fingerprint_format(
        evo_orchestration_read_u64_at(
            bytes, EVO_ORCHESTRATION_CHECKPOINT_CHECKSUM_OFFSET),
        view->checkpoint_fingerprint);
    return true;
}

static bool evo_orchestration_checkpoint_identity_equal(
    const evo_project_orchestration_checkpoint_identity_t *left,
    const evo_project_orchestration_checkpoint_identity_t *right)
{
    return left->catalogue_version == right->catalogue_version &&
           left->recipe_schema_version == right->recipe_schema_version &&
           left->search_schema_version == right->search_schema_version &&
           left->mutation_policy_version == right->mutation_policy_version &&
           left->crossover_policy_version == right->crossover_policy_version &&
           left->repair_policy_version == right->repair_policy_version &&
           left->random_seed == right->random_seed &&
           left->committed_generation == right->committed_generation &&
           strcmp(left->baseline_fingerprint, right->baseline_fingerprint) == 0 &&
           strcmp(left->analysis_fingerprint, right->analysis_fingerprint) == 0 &&
           strcmp(left->catalogue_identity, right->catalogue_identity) == 0 &&
           strcmp(left->search_policy_identity, right->search_policy_identity) == 0 &&
           strcmp(left->evaluation_provider_identity,
                  right->evaluation_provider_identity) == 0 &&
           strcmp(left->orchestration_policy_identity,
                  right->orchestration_policy_identity) == 0 &&
           strcmp(left->toolchain_identity, right->toolchain_identity) == 0 &&
           strcmp(left->workload_identity, right->workload_identity) == 0 &&
           strcmp(left->artifact_schema_identity,
                  right->artifact_schema_identity) == 0 &&
           strcmp(left->committed_lineage_fingerprint,
                  right->committed_lineage_fingerprint) == 0;
}

static evo_orchestration_checkpoint_owner_t *
evo_orchestration_checkpoint_owner_allocate(size_t size)
{
    evo_orchestration_checkpoint_owner_t *owner =
        evo_project_allocate_zeroed(1U, sizeof(*owner));

    if (owner == NULL) {
        return NULL;
    }
    owner->bytes = evo_project_allocate_zeroed(size, sizeof(*owner->bytes));
    if (owner->bytes == NULL) {
        evo_project_release(owner);
        return NULL;
    }
    owner->byte_count = size;
    return owner;
}

static void evo_orchestration_checkpoint_owner_destroy(
    evo_orchestration_checkpoint_owner_t *owner)
{
    if (owner == NULL) {
        return;
    }
    evo_project_release(owner->bytes);
    evo_project_release(owner);
}

evo_project_orchestration_checkpoint_status_t
evo_project_orchestration_checkpoint_create(
    const evo_project_orchestration_checkpoint_identity_t *identity,
    const void *core_checkpoint,
    size_t core_checkpoint_size,
    const evo_project_orchestration_checkpoint_limits_t *limits,
    evo_project_orchestration_checkpoint_t *checkpoint)
{
    size_t lengths[EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT] = {0};
    const char *strings[EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT];
    size_t serialized_size;
    evo_orchestration_checkpoint_owner_t *owner;
    evo_orchestration_checkpoint_cursor_t cursor;
    uint64_t integrity;
    size_t index;

    if (checkpoint == NULL || identity == NULL || core_checkpoint == NULL ||
        !evo_orchestration_checkpoint_limits_valid(limits) ||
        !evo_orchestration_checkpoint_identity_valid(identity, limits, lengths)) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_INVALID_ARGUMENT;
    }
    if (checkpoint->private_owner != NULL || checkpoint->format_version != 0U) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_RESULT_ACTIVE;
    }
    if (core_checkpoint_size == 0U ||
        core_checkpoint_size > limits->max_core_checkpoint_bytes ||
        !evo_orchestration_checkpoint_serialized_size(
            lengths,
            core_checkpoint_size,
            limits,
            &serialized_size)) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_RESOURCE_LIMIT;
    }
    owner = evo_orchestration_checkpoint_owner_allocate(serialized_size);
    if (owner == NULL) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_OUT_OF_MEMORY;
    }
    cursor.bytes = owner->bytes;
    cursor.size = serialized_size;
    cursor.position = 0U;
    strings[0] = identity->baseline_fingerprint;
    strings[1] = identity->analysis_fingerprint;
    strings[2] = identity->catalogue_identity;
    strings[3] = identity->search_policy_identity;
    strings[4] = identity->evaluation_provider_identity;
    strings[5] = identity->orchestration_policy_identity;
    strings[6] = identity->toolchain_identity;
    strings[7] = identity->workload_identity;
    strings[8] = identity->artifact_schema_identity;
    strings[9] = identity->committed_lineage_fingerprint;

    if (!evo_orchestration_write_bytes(
            &cursor,
            evo_orchestration_checkpoint_magic,
            sizeof(evo_orchestration_checkpoint_magic)) ||
        !evo_orchestration_write_u32(
            &cursor, EVO_PROJECT_ORCHESTRATION_CHECKPOINT_FORMAT_VERSION) ||
        !evo_orchestration_write_u32(
            &cursor,
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_INTEGRITY_FNV1A64) ||
        !evo_orchestration_write_u64(&cursor, serialized_size) ||
        !evo_orchestration_write_u64(&cursor, UINT64_C(0)) ||
        !evo_orchestration_write_u32(&cursor, identity->catalogue_version) ||
        !evo_orchestration_write_u32(&cursor, identity->recipe_schema_version) ||
        !evo_orchestration_write_u32(&cursor, identity->search_schema_version) ||
        !evo_orchestration_write_u32(&cursor, identity->mutation_policy_version) ||
        !evo_orchestration_write_u32(&cursor, identity->crossover_policy_version) ||
        !evo_orchestration_write_u32(&cursor, identity->repair_policy_version) ||
        !evo_orchestration_write_u64(&cursor, identity->random_seed) ||
        !evo_orchestration_write_u64(&cursor, identity->committed_generation) ||
        !evo_orchestration_write_u64(&cursor, core_checkpoint_size)) {
        evo_orchestration_checkpoint_owner_destroy(owner);
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_STATE;
    }
    for (index = 0U; index < EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT;
         index += 1U) {
        if (!evo_orchestration_write_string(
                &cursor, strings[index], lengths[index])) {
            evo_orchestration_checkpoint_owner_destroy(owner);
            return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_STATE;
        }
    }
    if (!evo_orchestration_write_bytes(
            &cursor, core_checkpoint, core_checkpoint_size) ||
        cursor.position != serialized_size) {
        evo_orchestration_checkpoint_owner_destroy(owner);
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_STATE;
    }
    integrity = evo_orchestration_checkpoint_integrity(
        owner->bytes, owner->byte_count);
    evo_orchestration_write_u64_at(
        owner->bytes,
        EVO_ORCHESTRATION_CHECKPOINT_CHECKSUM_OFFSET,
        integrity);
    if (!evo_orchestration_checkpoint_parse(owner, limits)) {
        evo_orchestration_checkpoint_owner_destroy(owner);
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_STATE;
    }
    *checkpoint = owner->view;
    return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS;
}

evo_project_orchestration_checkpoint_status_t
evo_project_orchestration_checkpoint_validate(
    const evo_project_orchestration_checkpoint_identity_t *expected_identity,
    const void *serialized,
    size_t serialized_size,
    const evo_project_orchestration_checkpoint_limits_t *limits,
    evo_project_orchestration_checkpoint_t *checkpoint)
{
    const unsigned char *input = serialized;
    size_t expected_lengths[EVO_ORCHESTRATION_CHECKPOINT_STRING_COUNT] = {0};
    evo_orchestration_checkpoint_owner_t *owner;
    uint64_t expected_integrity;
    uint64_t actual_integrity;
    size_t index;

    if (checkpoint == NULL || expected_identity == NULL || serialized == NULL ||
        !evo_orchestration_checkpoint_limits_valid(limits) ||
        !evo_orchestration_checkpoint_identity_valid(
            expected_identity, limits, expected_lengths)) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_INVALID_ARGUMENT;
    }
    if (checkpoint->private_owner != NULL || checkpoint->format_version != 0U) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_RESULT_ACTIVE;
    }
    if (serialized_size < 80U || serialized_size > limits->max_checkpoint_bytes) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_RESOURCE_LIMIT;
    }
    if (!evo_orchestration_checkpoint_magic_valid(input, serialized_size) ||
        evo_orchestration_read_u32_at(input, 8U) !=
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_FORMAT_VERSION ||
        evo_orchestration_read_u32_at(input, 12U) !=
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_INTEGRITY_FNV1A64 ||
        evo_orchestration_read_u64_at(input, 16U) != serialized_size) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_FORMAT;
    }
    expected_integrity = evo_orchestration_read_u64_at(
        input, EVO_ORCHESTRATION_CHECKPOINT_CHECKSUM_OFFSET);
    actual_integrity = evo_orchestration_checkpoint_integrity(
        input, serialized_size);
    if (expected_integrity != actual_integrity) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_INTEGRITY;
    }
    owner = evo_orchestration_checkpoint_owner_allocate(serialized_size);
    if (owner == NULL) {
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < serialized_size; index += 1U) {
        owner->bytes[index] = input[index];
    }
    if (!evo_orchestration_checkpoint_parse(owner, limits)) {
        evo_orchestration_checkpoint_owner_destroy(owner);
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_FORMAT;
    }
    if (!evo_orchestration_checkpoint_identity_equal(
            expected_identity, &owner->view.identity)) {
        evo_orchestration_checkpoint_owner_destroy(owner);
        return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH;
    }
    *checkpoint = owner->view;
    return EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS;
}

void evo_project_orchestration_checkpoint_destroy(
    evo_project_orchestration_checkpoint_t *checkpoint)
{
    evo_orchestration_checkpoint_owner_t *owner;

    if (checkpoint == NULL) {
        return;
    }
    owner = checkpoint->private_owner;
    if (owner != NULL) {
        evo_orchestration_checkpoint_owner_destroy(owner);
    }
    *checkpoint = (evo_project_orchestration_checkpoint_t){0};
}

const char *evo_project_orchestration_checkpoint_status_name(
    evo_project_orchestration_checkpoint_status_t status)
{
    switch (status) {
    case EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS:
        return "success";
    case EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_FORMAT:
        return "format";
    case EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_INTEGRITY:
        return "integrity";
    case EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH:
        return "identity-mismatch";
    case EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_STATE:
    default:
        return "state";
    }
}
