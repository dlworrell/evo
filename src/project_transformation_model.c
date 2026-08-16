#include "internal/project_transformation_model.h"

#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_runtime.h"

#include <string.h>

static char *evo_transform_duplicate_text(const char *value)
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

static char *evo_transform_duplicate_bytes(
    const unsigned char *bytes,
    size_t count)
{
    char *copy;
    size_t index;

    if (count == SIZE_MAX) {
        return NULL;
    }
    copy = evo_project_allocate_zeroed(count + 1U, sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }
    for (index = 0U; index < count; index += 1U) {
        copy[index] = (char)bytes[index];
    }
    copy[count] = '\0';
    return copy;
}

static bool evo_transform_range_valid(
    evo_project_transformation_byte_range_t range,
    evo_project_transformation_byte_range_t target,
    bool required)
{
    if (range.start > range.end) {
        return false;
    }
    if (!required && range.start == 0U && range.end == 0U) {
        return true;
    }
    return range.start < range.end && range.start >= target.start &&
           range.end <= target.end;
}

static bool evo_transform_range_empty(
    evo_project_transformation_byte_range_t range)
{
    return range.start == 0U && range.end == 0U;
}

static bool evo_transform_bytes_equal(
    const unsigned char *source,
    evo_project_transformation_byte_range_t left,
    evo_project_transformation_byte_range_t right)
{
    const size_t left_size = left.end - left.start;
    const size_t right_size = right.end - right.start;
    size_t index;

    if (left_size != right_size) {
        return false;
    }
    for (index = 0U; index < left_size; index += 1U) {
        if (source[left.start + index] != source[right.start + index]) {
            return false;
        }
    }
    return true;
}

static bool evo_transform_basic_identifier(
    const unsigned char *source,
    evo_project_transformation_byte_range_t range)
{
    size_t index;

    if (range.start >= range.end ||
        !((source[range.start] >= (unsigned char)'A' &&
           source[range.start] <= (unsigned char)'Z') ||
          (source[range.start] >= (unsigned char)'a' &&
           source[range.start] <= (unsigned char)'z') ||
          source[range.start] == (unsigned char)'_')) {
        return false;
    }
    for (index = range.start + 1U; index < range.end; index += 1U) {
        if (!((source[index] >= (unsigned char)'A' &&
               source[index] <= (unsigned char)'Z') ||
              (source[index] >= (unsigned char)'a' &&
               source[index] <= (unsigned char)'z') ||
              (source[index] >= (unsigned char)'0' &&
               source[index] <= (unsigned char)'9') ||
              source[index] == (unsigned char)'_')) {
            return false;
        }
    }
    return true;
}

static bool evo_transform_interstitial_token(
    const unsigned char *source,
    size_t start,
    size_t end,
    const char *token)
{
    size_t token_index = 0U;
    size_t index;

    if (start > end) {
        return false;
    }
    for (index = start; index < end; index += 1U) {
        const unsigned char byte = source[index];

        if (byte == (unsigned char)' ' || byte == (unsigned char)'\t' ||
            byte == (unsigned char)'\n' || byte == (unsigned char)'\r' ||
            byte == (unsigned char)'(' || byte == (unsigned char)')') {
            continue;
        }
        if (token[token_index] == '\0' ||
            byte != (unsigned char)token[token_index]) {
            return false;
        }
        token_index += 1U;
    }
    return token[token_index] == '\0';
}

static evo_project_transformation_status_t evo_transform_scan_target(
    const unsigned char *source,
    evo_project_transformation_byte_range_t target)
{
    size_t index;

    for (index = target.start; index < target.end; index += 1U) {
        const unsigned char byte = source[index];

        if (byte == (unsigned char)'#') {
            return EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_PREPROCESSOR;
        }
        if (byte == (unsigned char)'/' && index + 1U < target.end &&
            (source[index + 1U] == (unsigned char)'/' ||
             source[index + 1U] == (unsigned char)'*')) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_COMMENT;
        }
        if (byte >= 0x80U ||
            (byte < 0x20U && byte != (unsigned char)'\t' &&
             byte != (unsigned char)'\n' && byte != (unsigned char)'\r')) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_EXTENSION;
        }
    }
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static evo_project_transformation_status_t evo_transform_common_ast_valid(
    const evo_project_recipe_record_t *record,
    const evo_project_transformation_capability_t *capability,
    const unsigned char *source,
    size_t source_size,
    const evo_project_transformation_ast_result_t *ast,
    const evo_project_transformation_limits_t *limits)
{
    size_t index;
    bool supported_form = false;

    if (ast->schema_version != EVO_PROJECT_TRANSFORMATION_AST_SCHEMA_VERSION ||
        !ast->completed ||
        !evo_project_json_text_valid(
            ast->location_identity, limits->max_string_bytes, false) ||
        !evo_project_json_text_valid(
            ast->file, limits->max_path_bytes, false) ||
        strcmp(ast->location_identity, record->target.location_identity) != 0 ||
        strcmp(ast->file, record->target.file) != 0 ||
        ast->target.start >= ast->target.end || ast->target.end > source_size) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
    }
    for (index = 0U; index < capability->ast_form_count; index += 1U) {
        if (capability->ast_forms[index] == ast->form) {
            supported_form = true;
            break;
        }
    }
    if (!supported_form) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    if (record->target.kind != EVO_PROJECT_LOCATION_SPELLING ||
        ast->contains_macro) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_MACRO;
    }
    if (ast->contains_comment) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_COMMENT;
    }
    if (ast->contains_preprocessor) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_PREPROCESSOR;
    }
    if (ast->language_extension) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_EXTENSION;
    }
    if (ast->alias_assumption_required) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_ALIAS_ASSUMPTION;
    }
    if (ast->ambiguous_target) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET;
    }
    return evo_transform_scan_target(source, ast->target);
}

static const evo_project_recipe_parameter_value_t *evo_transform_parameter(
    const evo_project_recipe_record_t *record,
    const char *identity)
{
    size_t index;

    for (index = 0U; index < record->parameter_count; index += 1U) {
        if (record->parameters[index].identity != NULL &&
            strcmp(record->parameters[index].identity, identity) == 0) {
            return &record->parameters[index];
        }
    }
    return NULL;
}

static evo_project_transformation_operator_t evo_assignment_operator(
    const evo_project_recipe_record_t *record,
    const char **token)
{
    const evo_project_recipe_parameter_value_t *parameter =
        evo_transform_parameter(record, "operator");

    if (parameter == NULL ||
        parameter->kind != EVO_PROJECT_RECIPE_PARAMETER_CHOICE ||
        parameter->choice_value == NULL) {
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_NONE;
    }
    if (strcmp(parameter->choice_value, "add") == 0) {
        *token = "+=";
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_ADD;
    }
    if (strcmp(parameter->choice_value, "bitwise-and") == 0) {
        *token = "&=";
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_AND;
    }
    if (strcmp(parameter->choice_value, "bitwise-or") == 0) {
        *token = "|=";
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_OR;
    }
    if (strcmp(parameter->choice_value, "bitwise-xor") == 0) {
        *token = "^=";
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_XOR;
    }
    if (strcmp(parameter->choice_value, "multiply") == 0) {
        *token = "*=";
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_MULTIPLY;
    }
    if (strcmp(parameter->choice_value, "subtract") == 0) {
        *token = "-=";
        return EVO_PROJECT_TRANSFORMATION_OPERATOR_SUBTRACT;
    }
    return EVO_PROJECT_TRANSFORMATION_OPERATOR_NONE;
}

static evo_project_transformation_condition_context_t evo_condition_context(
    const evo_project_recipe_record_t *record)
{
    const evo_project_recipe_parameter_value_t *parameter =
        evo_transform_parameter(record, "context");

    if (parameter == NULL ||
        parameter->kind != EVO_PROJECT_RECIPE_PARAMETER_CHOICE ||
        parameter->choice_value == NULL) {
        return EVO_PROJECT_TRANSFORMATION_CONDITION_NONE;
    }
    if (strcmp(parameter->choice_value, "do-while") == 0) {
        return EVO_PROJECT_TRANSFORMATION_CONDITION_DO_WHILE;
    }
    if (strcmp(parameter->choice_value, "for") == 0) {
        return EVO_PROJECT_TRANSFORMATION_CONDITION_FOR;
    }
    if (strcmp(parameter->choice_value, "if") == 0) {
        return EVO_PROJECT_TRANSFORMATION_CONDITION_IF;
    }
    if (strcmp(parameter->choice_value, "while") == 0) {
        return EVO_PROJECT_TRANSFORMATION_CONDITION_WHILE;
    }
    return EVO_PROJECT_TRANSFORMATION_CONDITION_NONE;
}

static bool evo_transform_prefix_is_double_not(
    const unsigned char *source,
    size_t start,
    size_t end)
{
    size_t index;
    size_t bang_count = 0U;

    for (index = start; index < end; index += 1U) {
        const unsigned char byte = source[index];

        if (byte == (unsigned char)'!') {
            bang_count += 1U;
        } else if (byte != (unsigned char)' ' && byte != (unsigned char)'\t' &&
                   byte != (unsigned char)'\n' &&
                   byte != (unsigned char)'\r') {
            return false;
        }
    }
    return bang_count == 2U;
}

static bool evo_transform_power_of_two(uint64_t value, uint32_t *shift)
{
    uint32_t count = 0U;

    if (value < UINT64_C(2) || (value & (value - UINT64_C(1))) != 0U) {
        return false;
    }
    while (value > UINT64_C(1)) {
        value >>= 1U;
        count += 1U;
    }
    *shift = count;
    return true;
}

static bool evo_transform_decimal_suffix_valid(
    const unsigned char *suffix,
    size_t size)
{
    const bool first_unsigned =
        size > 0U &&
        (suffix[0] == (unsigned char)'u' || suffix[0] == (unsigned char)'U');
    const bool last_unsigned =
        size > 0U &&
        (suffix[size - 1U] == (unsigned char)'u' ||
         suffix[size - 1U] == (unsigned char)'U');

    if (size == 0U) {
        return true;
    }
    if (size == 1U) {
        return first_unsigned || suffix[0] == (unsigned char)'l' ||
               suffix[0] == (unsigned char)'L';
    }
    if (size == 2U) {
        return (first_unsigned &&
                (suffix[1] == (unsigned char)'l' ||
                 suffix[1] == (unsigned char)'L')) ||
               (last_unsigned &&
                (suffix[0] == (unsigned char)'l' ||
                 suffix[0] == (unsigned char)'L')) ||
               (suffix[0] == suffix[1] &&
                (suffix[0] == (unsigned char)'l' ||
                 suffix[0] == (unsigned char)'L'));
    }
    if (size == 3U) {
        return (first_unsigned && suffix[1] == suffix[2] &&
                (suffix[1] == (unsigned char)'l' ||
                 suffix[1] == (unsigned char)'L')) ||
               (last_unsigned && suffix[0] == suffix[1] &&
                (suffix[0] == (unsigned char)'l' ||
                 suffix[0] == (unsigned char)'L'));
    }
    return false;
}

static bool evo_transform_decimal_literal_matches(
    const unsigned char *source,
    evo_project_transformation_byte_range_t range,
    uint64_t expected)
{
    uint64_t value = 0U;
    size_t position = range.start;
    size_t digit_count = 0U;

    while (position < range.end &&
           source[position] >= (unsigned char)'0' &&
           source[position] <= (unsigned char)'9') {
        const uint64_t digit =
            (uint64_t)(source[position] - (unsigned char)'0');

        if (value > (UINT64_MAX - digit) / UINT64_C(10)) {
            return false;
        }
        value = value * UINT64_C(10) + digit;
        digit_count += 1U;
        position += 1U;
    }
    return digit_count > 0U && value == expected &&
           evo_transform_decimal_suffix_valid(
               source + position, range.end - position);
}

static evo_project_transformation_status_t evo_transform_prepare_edit(
    const unsigned char *source,
    evo_project_transformation_byte_range_t target,
    const unsigned char *replacement,
    size_t replacement_size,
    evo_project_transformation_disposition_t disposition,
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_application_owner_t *owner)
{
    evo_project_fingerprint_t fingerprint;
    const size_t before_size = target.end - target.start;

    if (replacement_size > limits->max_replacement_bytes) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    owner->before_text = evo_transform_duplicate_bytes(
        source + target.start, before_size);
    if (owner->before_text == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    if (replacement_size > 0U) {
        owner->replacement_text = evo_transform_duplicate_bytes(
            replacement, replacement_size);
        if (owner->replacement_text == NULL) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
        }
    }
    owner->view.disposition = disposition;
    owner->view.edit.before_start = target.start;
    owner->view.edit.before_end = target.end;
    owner->view.edit.before_size = before_size;
    owner->view.edit.before_text = owner->before_text;
    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(
        &fingerprint, source + target.start, before_size);
    evo_project_fingerprint_format(
        fingerprint.value, owner->view.edit.before_fingerprint);
    owner->view.edit.after_start = target.start;
    owner->view.edit.after_end =
        disposition == EVO_PROJECT_TRANSFORMATION_EDIT
            ? target.start + replacement_size
            : target.end;
    owner->view.edit.replacement_size = replacement_size;
    owner->view.edit.replacement_text = owner->replacement_text;
    evo_project_fingerprint_begin(&fingerprint);
    if (replacement_size > 0U) {
        evo_project_fingerprint_bytes(
            &fingerprint, replacement, replacement_size);
    }
    evo_project_fingerprint_format(
        fingerprint.value, owner->view.edit.replacement_fingerprint);
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static evo_project_transformation_status_t evo_transform_assignment(
    const evo_project_recipe_record_t *record,
    const unsigned char *source,
    const evo_project_transformation_ast_result_t *ast,
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_application_owner_t *owner)
{
    const char *operator_token = NULL;
    const evo_project_transformation_operator_t expected =
        evo_assignment_operator(record, &operator_token);
    const bool already = ast->form == EVO_PROJECT_AST_ASSIGNMENT_COMPOUND;
    const size_t primary_size = ast->primary.end - ast->primary.start;
    const size_t operand_size = ast->operand.end - ast->operand.start;
    size_t replacement_size;
    unsigned char *replacement;
    size_t position;
    evo_project_transformation_status_t status;

    if (expected == EVO_PROJECT_TRANSFORMATION_OPERATOR_NONE ||
        ast->operator_kind != expected ||
        ast->condition_context != EVO_PROJECT_TRANSFORMATION_CONDITION_NONE ||
        !ast->primary_plain_identifier || ast->volatile_access ||
        !evo_transform_range_valid(ast->primary, ast->target, true) ||
        !evo_transform_range_valid(ast->operand, ast->target, true) ||
        !evo_transform_range_empty(ast->literal) ||
        ast->primary.start != ast->target.start ||
        ast->operand.end != ast->target.end ||
        ast->primary.start >= ast->operand.start ||
        !evo_transform_basic_identifier(source, ast->primary) ||
        !ast->result_type_matches_primary ||
        !evo_project_json_text_valid(
            ast->primary_declaration_identity,
            limits->max_string_bytes,
            false)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    if (already) {
        if (!evo_transform_range_empty(ast->duplicate_primary) ||
            ast->duplicate_declaration_identity != NULL ||
            !evo_transform_interstitial_token(
                source,
                ast->primary.end,
                ast->operand.start,
                operator_token)) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
        }
        return evo_transform_prepare_edit(
            source,
            ast->target,
            NULL,
            0U,
            EVO_PROJECT_TRANSFORMATION_ALREADY_SATISFIED,
            limits,
            owner);
    }
    if (!evo_transform_range_valid(
            ast->duplicate_primary, ast->target, true) ||
        !evo_project_json_text_valid(
            ast->duplicate_declaration_identity,
            limits->max_string_bytes,
            false) ||
        strcmp(
            ast->primary_declaration_identity,
            ast->duplicate_declaration_identity) != 0 ||
        ast->primary.end > ast->duplicate_primary.start ||
        ast->duplicate_primary.end > ast->operand.start ||
        !evo_transform_interstitial_token(
            source,
            ast->primary.end,
            ast->duplicate_primary.start,
            "=") ||
        !evo_transform_interstitial_token(
            source,
            ast->duplicate_primary.end,
            ast->operand.start,
            (char[2]){operator_token[0], '\0'}) ||
        !evo_transform_basic_identifier(source, ast->duplicate_primary) ||
        !evo_transform_bytes_equal(
            source, ast->primary, ast->duplicate_primary)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    if (primary_size > SIZE_MAX - operand_size ||
        primary_size + operand_size > SIZE_MAX - 4U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    replacement_size = primary_size + operand_size + 4U;
    if (replacement_size > limits->max_replacement_bytes) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    replacement = evo_project_allocate_zeroed(
        replacement_size, sizeof(*replacement));
    if (replacement == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    position = 0U;
    (void)memcpy(
        replacement + position, source + ast->primary.start, primary_size);
    position += primary_size;
    replacement[position] = (unsigned char)' ';
    position += 1U;
    replacement[position] = (unsigned char)operator_token[0];
    replacement[position + 1U] = (unsigned char)operator_token[1];
    position += 2U;
    replacement[position] = (unsigned char)' ';
    position += 1U;
    (void)memcpy(
        replacement + position, source + ast->operand.start, operand_size);
    status = evo_transform_prepare_edit(
        source,
        ast->target,
        replacement,
        replacement_size,
        EVO_PROJECT_TRANSFORMATION_EDIT,
        limits,
        owner);
    evo_project_release(replacement);
    return status;
}

static evo_project_transformation_status_t evo_transform_condition(
    const evo_project_recipe_record_t *record,
    const unsigned char *source,
    const evo_project_transformation_ast_result_t *ast,
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_application_owner_t *owner)
{
    const evo_project_transformation_condition_context_t expected =
        evo_condition_context(record);
    const bool already = ast->form == EVO_PROJECT_AST_SCALAR_CONDITION;
    const size_t operand_size = ast->operand.end - ast->operand.start;

    if (expected == EVO_PROJECT_TRANSFORMATION_CONDITION_NONE ||
        ast->condition_context != expected ||
        ast->operator_kind != EVO_PROJECT_TRANSFORMATION_OPERATOR_NONE ||
        !ast->scalar_operand || ast->volatile_access ||
        !evo_transform_range_valid(ast->operand, ast->target, true) ||
        !evo_transform_range_empty(ast->primary) ||
        !evo_transform_range_empty(ast->duplicate_primary) ||
        !evo_transform_range_empty(ast->literal) ||
        ast->primary_declaration_identity != NULL ||
        ast->duplicate_declaration_identity != NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    if (already) {
        if (ast->operand.start != ast->target.start ||
            ast->operand.end != ast->target.end) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
        }
        return evo_transform_prepare_edit(
            source,
            ast->target,
            NULL,
            0U,
            EVO_PROJECT_TRANSFORMATION_ALREADY_SATISFIED,
            limits,
            owner);
    }
    if (ast->operand.end != ast->target.end ||
        ast->target.start >= ast->operand.start ||
        !evo_transform_prefix_is_double_not(
            source, ast->target.start, ast->operand.start)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
    }
    return evo_transform_prepare_edit(
        source,
        ast->target,
        source + ast->operand.start,
        operand_size,
        EVO_PROJECT_TRANSFORMATION_EDIT,
        limits,
        owner);
}

static evo_project_transformation_status_t evo_transform_shift(
    const evo_project_recipe_record_t *record,
    const unsigned char *source,
    const evo_project_transformation_ast_result_t *ast,
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_application_owner_t *owner)
{
    const evo_project_recipe_parameter_value_t *maximum_parameter =
        evo_transform_parameter(record, "maximum-shift");
    const bool already =
        ast->form == EVO_PROJECT_AST_UNSIGNED_SHIFT_POWER_OF_TWO;
    uint32_t shift = 0U;
    const size_t primary_size = ast->primary.end - ast->primary.start;
    char shift_text[32];
    int written;
    size_t shift_size;
    size_t replacement_size;
    unsigned char *replacement;
    size_t position;
    evo_project_transformation_status_t status;

    if (maximum_parameter == NULL ||
        maximum_parameter->kind != EVO_PROJECT_RECIPE_PARAMETER_INTEGER ||
        maximum_parameter->integer_value < 1 ||
        maximum_parameter->integer_value > 63 ||
        ast->condition_context != EVO_PROJECT_TRANSFORMATION_CONDITION_NONE ||
        !evo_transform_range_valid(ast->primary, ast->target, true) ||
        !evo_transform_range_valid(ast->literal, ast->target, true) ||
        !evo_transform_range_empty(ast->duplicate_primary) ||
        !evo_transform_range_empty(ast->operand) ||
        ast->primary.start >= ast->literal.start || ast->volatile_access ||
        !ast->result_unsigned_integer || !ast->result_type_matches_primary ||
        ast->result_width_bits == 0U || ast->result_width_bits > 64U ||
        !evo_transform_decimal_literal_matches(
            source, ast->literal, ast->literal_value)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    if (already) {
        if (ast->operator_kind !=
                EVO_PROJECT_TRANSFORMATION_OPERATOR_SHIFT_LEFT ||
            ast->target.end - ast->target.start < 2U ||
            source[ast->target.start] != (unsigned char)'(' ||
            source[ast->target.end - 1U] != (unsigned char)')' ||
            ast->primary.start != ast->target.start + 1U ||
            ast->literal.end != ast->target.end - 1U ||
            ast->literal_value > UINT64_C(63) ||
            !evo_transform_interstitial_token(
                source, ast->primary.end, ast->literal.start, "<<")) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
        }
        shift = (uint32_t)ast->literal_value;
    } else {
        if (ast->operator_kind !=
                EVO_PROJECT_TRANSFORMATION_OPERATOR_MULTIPLY ||
            ast->primary.start != ast->target.start ||
            ast->literal.end != ast->target.end ||
            !evo_transform_interstitial_token(
                source, ast->primary.end, ast->literal.start, "*") ||
            !evo_transform_power_of_two(ast->literal_value, &shift)) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
        }
    }
    if (shift == 0U || shift >= ast->result_width_bits ||
        (int64_t)shift > maximum_parameter->integer_value) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    if (already) {
        return evo_transform_prepare_edit(
            source,
            ast->target,
            NULL,
            0U,
            EVO_PROJECT_TRANSFORMATION_ALREADY_SATISFIED,
            limits,
            owner);
    }
    written = evo_project_format(
        shift_text, sizeof(shift_text), "%u", (unsigned int)shift);
    if (written <= 0 || (size_t)written >= sizeof(shift_text)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_STATE;
    }
    shift_size = (size_t)written;
    if (primary_size > SIZE_MAX - shift_size ||
        primary_size + shift_size > SIZE_MAX - 6U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    replacement_size = primary_size + shift_size + 6U;
    if (replacement_size > limits->max_replacement_bytes) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    replacement = evo_project_allocate_zeroed(
        replacement_size, sizeof(*replacement));
    if (replacement == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    position = 0U;
    replacement[position] = (unsigned char)'(';
    position += 1U;
    (void)memcpy(
        replacement + position, source + ast->primary.start, primary_size);
    position += primary_size;
    (void)memcpy(replacement + position, " << ", 4U);
    position += 4U;
    (void)memcpy(replacement + position, shift_text, shift_size);
    position += shift_size;
    replacement[position] = (unsigned char)')';
    status = evo_transform_prepare_edit(
        source,
        ast->target,
        replacement,
        replacement_size,
        EVO_PROJECT_TRANSFORMATION_EDIT,
        limits,
        owner);
    evo_project_release(replacement);
    return status;
}

static evo_project_transformation_status_t evo_transform_copy_parameters(
    const evo_project_recipe_record_t *record,
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_application_owner_t *owner)
{
    size_t index;

    if (record->parameter_count > limits->max_parameters) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    owner->parameter_count = record->parameter_count;
    if (record->parameter_count == 0U) {
        owner->view.parameter_count = 0U;
        owner->view.parameters = NULL;
        return EVO_PROJECT_TRANSFORMATION_SUCCESS;
    }
    owner->parameters = evo_project_allocate_zeroed(
        record->parameter_count, sizeof(*owner->parameters));
    if (owner->parameters == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < record->parameter_count; index += 1U) {
        const evo_project_recipe_parameter_value_t *source =
            &record->parameters[index];
        evo_project_recipe_parameter_value_t *destination =
            &owner->parameters[index];

        if (!evo_project_json_text_valid(
                source->identity, limits->max_string_bytes, false) ||
            (source->kind != EVO_PROJECT_RECIPE_PARAMETER_INTEGER &&
             source->kind != EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN &&
             source->kind != EVO_PROJECT_RECIPE_PARAMETER_CHOICE) ||
            (source->kind == EVO_PROJECT_RECIPE_PARAMETER_CHOICE &&
             !evo_project_json_text_valid(
                 source->choice_value,
                 limits->max_string_bytes,
                 false))) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
        }
        *destination = *source;
        destination->identity = evo_transform_duplicate_text(source->identity);
        if (destination->identity == NULL) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
        }
        destination->choice_value = NULL;
        if (source->kind == EVO_PROJECT_RECIPE_PARAMETER_CHOICE) {
            destination->choice_value =
                evo_transform_duplicate_text(source->choice_value);
            if (destination->choice_value == NULL) {
                return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
            }
        }
    }
    owner->view.parameter_count = owner->parameter_count;
    owner->view.parameters = owner->parameters;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static evo_project_transformation_status_t evo_transform_copy_capability(
    const evo_project_recipe_record_t *record,
    const evo_project_transformation_capability_t *capability,
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_application_owner_t *owner)
{
    size_t index;
    evo_project_transformation_status_t status;

    if (!evo_project_json_text_valid(
            capability->formatting_policy,
            limits->max_string_bytes,
            false) ||
        !evo_project_json_text_valid(
            capability->idempotence_policy,
            limits->max_string_bytes,
            false)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    for (index = 0U; index < capability->semantic_assumption_count;
         index += 1U) {
        if (!evo_project_json_text_valid(
                capability->semantic_assumptions[index],
                limits->max_string_bytes,
                false)) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
        }
    }
    for (index = 0U; index < capability->validation_obligation_count;
         index += 1U) {
        if (!evo_project_json_text_valid(
                capability->validation_obligations[index],
                limits->max_string_bytes,
                false)) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
        }
    }
    status = evo_transform_copy_parameters(record, limits, owner);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }

    owner->formatting_policy =
        evo_transform_duplicate_text(capability->formatting_policy);
    owner->idempotence_policy =
        evo_transform_duplicate_text(capability->idempotence_policy);
    if (owner->formatting_policy == NULL || owner->idempotence_policy == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    owner->semantic_assumption_count = capability->semantic_assumption_count;
    owner->semantic_assumptions = evo_project_allocate_zeroed(
        capability->semantic_assumption_count,
        sizeof(*owner->semantic_assumptions));
    owner->validation_obligation_count =
        capability->validation_obligation_count;
    owner->validation_obligations = evo_project_allocate_zeroed(
        capability->validation_obligation_count,
        sizeof(*owner->validation_obligations));
    if (owner->semantic_assumptions == NULL ||
        owner->validation_obligations == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < capability->semantic_assumption_count;
         index += 1U) {
        owner->semantic_assumptions[index] = evo_transform_duplicate_text(
            capability->semantic_assumptions[index]);
        if (owner->semantic_assumptions[index] == NULL) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
        }
    }
    for (index = 0U; index < capability->validation_obligation_count;
         index += 1U) {
        owner->validation_obligations[index] = evo_transform_duplicate_text(
            capability->validation_obligations[index]);
        if (owner->validation_obligations[index] == NULL) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
        }
    }
    owner->view.formatting_policy = owner->formatting_policy;
    owner->view.idempotence_policy = owner->idempotence_policy;
    owner->view.semantic_assumption_count = owner->semantic_assumption_count;
    owner->view.semantic_assumptions =
        (const char *const *)owner->semantic_assumptions;
    owner->view.validation_obligation_count =
        owner->validation_obligation_count;
    owner->view.validation_obligations =
        (const char *const *)owner->validation_obligations;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static evo_project_transformation_status_t evo_transform_copy_ast_evidence(
    const evo_project_transformation_ast_result_t *ast,
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_application_owner_t *owner)
{
    if (ast->primary_declaration_identity != NULL) {
        if (!evo_project_json_text_valid(
                ast->primary_declaration_identity,
                limits->max_string_bytes,
                false)) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
        }
        owner->primary_declaration_identity =
            evo_transform_duplicate_text(ast->primary_declaration_identity);
        if (owner->primary_declaration_identity == NULL) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
        }
    }
    if (ast->duplicate_declaration_identity != NULL) {
        if (!evo_project_json_text_valid(
                ast->duplicate_declaration_identity,
                limits->max_string_bytes,
                false)) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
        }
        owner->duplicate_declaration_identity =
            evo_transform_duplicate_text(ast->duplicate_declaration_identity);
        if (owner->duplicate_declaration_identity == NULL) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
        }
    }
    owner->view.ast_primary = ast->primary;
    owner->view.ast_duplicate_primary = ast->duplicate_primary;
    owner->view.ast_operand = ast->operand;
    owner->view.ast_literal = ast->literal;
    owner->view.primary_declaration_identity =
        owner->primary_declaration_identity;
    owner->view.duplicate_declaration_identity =
        owner->duplicate_declaration_identity;
    owner->view.literal_value = ast->literal_value;
    owner->view.result_width_bits = ast->result_width_bits;
    owner->view.primary_plain_identifier = ast->primary_plain_identifier;
    owner->view.volatile_access = ast->volatile_access;
    owner->view.result_unsigned_integer = ast->result_unsigned_integer;
    owner->view.result_type_matches_primary = ast->result_type_matches_primary;
    owner->view.scalar_operand = ast->scalar_operand;
    owner->view.contains_macro = ast->contains_macro;
    owner->view.contains_comment = ast->contains_comment;
    owner->view.contains_preprocessor = ast->contains_preprocessor;
    owner->view.language_extension = ast->language_extension;
    owner->view.ambiguous_target = ast->ambiguous_target;
    owner->view.alias_assumption_required = ast->alias_assumption_required;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

evo_project_transformation_status_t evo_project_transformation_model_apply(
    const evo_project_recipe_record_t *record,
    const evo_project_transformation_capability_t *capability,
    const unsigned char *source,
    size_t source_size,
    const evo_project_transformation_ast_result_t *ast,
    const evo_project_transformation_limits_t *limits,
    evo_project_transformation_application_owner_t *owner)
{
    evo_project_transformation_status_t status =
        evo_transform_common_ast_valid(
            record, capability, source, source_size, ast, limits);

    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    if (strcmp(
            record->transformation_identity,
            "catalyst.evo.c.assignment-to-compound") == 0) {
        status = evo_transform_assignment(
            record, source, ast, limits, owner);
    } else if (strcmp(
                   record->transformation_identity,
                   "catalyst.evo.c.double-negation-condition") == 0) {
        status = evo_transform_condition(
            record, source, ast, limits, owner);
    } else if (strcmp(
                   record->transformation_identity,
                   "catalyst.evo.c.unsigned-multiply-to-shift") == 0) {
        status = evo_transform_shift(record, source, ast, limits, owner);
    } else {
        return EVO_PROJECT_TRANSFORMATION_ERROR_CATALOGUE_INVALID;
    }
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    owner->view.ast_form = ast->form;
    owner->view.operator_kind = ast->operator_kind;
    owner->view.condition_context = ast->condition_context;
    status = evo_transform_copy_ast_evidence(ast, limits, owner);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    return evo_transform_copy_capability(record, capability, limits, owner);
}

void evo_project_transformation_application_owner_destroy(
    evo_project_transformation_application_owner_t *owner)
{
    size_t index;

    if (owner == NULL) {
        return;
    }
    for (index = 0U; index < owner->parameter_count; index += 1U) {
        evo_project_release((void *)owner->parameters[index].choice_value);
        evo_project_release((void *)owner->parameters[index].identity);
    }
    for (index = 0U; index < owner->semantic_assumption_count; index += 1U) {
        evo_project_release(owner->semantic_assumptions[index]);
    }
    for (index = 0U; index < owner->validation_obligation_count; index += 1U) {
        evo_project_release(owner->validation_obligations[index]);
    }
    evo_project_release(owner->validation_obligations);
    evo_project_release(owner->semantic_assumptions);
    evo_project_release(owner->parameters);
    evo_project_release(owner->idempotence_policy);
    evo_project_release(owner->formatting_policy);
    evo_project_release(owner->audit_markdown);
    evo_project_release(owner->canonical_json);
    evo_project_release(owner->replacement_text);
    evo_project_release(owner->before_text);
    evo_project_release(owner->duplicate_declaration_identity);
    evo_project_release(owner->primary_declaration_identity);
    evo_project_release(owner->target_spelling_identity);
    evo_project_release(owner->target_file);
    evo_project_release(owner->target_location_identity);
    evo_project_release(owner->clang_identity);
    evo_project_release(owner->provider_identity);
    evo_project_release(owner->transformation_identity);
    evo_project_release(owner->record_identity);
    evo_project_release(owner->catalogue_identity);
    evo_project_release(owner->recipe_fingerprint);
    evo_project_release(owner->analysis_fingerprint);
    evo_project_release(owner->baseline_fingerprint);
    *owner = (evo_project_transformation_application_owner_t){0};
}
