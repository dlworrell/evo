#include "internal/project_json.h"

#include "internal/project_runtime.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define EVO_PROJECT_JSON_NO_PARENT SIZE_MAX

typedef struct evo_project_json_parser {
    const char *text;
    size_t text_size;
    size_t position;
    evo_project_json_token_t *tokens;
    size_t token_capacity;
    size_t token_count;
    size_t maximum_depth;
} evo_project_json_parser_t;

static void evo_project_json_skip_space(evo_project_json_parser_t *parser)
{
    while (parser->position < parser->text_size) {
        const char value = parser->text[parser->position];

        if (value != ' ' && value != '\t' && value != '\n' && value != '\r') {
            break;
        }
        parser->position += 1U;
    }
}

static evo_project_json_status_t evo_project_json_add_token(
    evo_project_json_parser_t *parser,
    evo_project_json_type_t type,
    size_t start,
    size_t parent,
    size_t *token_index)
{
    evo_project_json_token_t *token;

    if (parser->token_count >= parser->token_capacity) {
        return EVO_PROJECT_JSON_RESOURCE_LIMIT;
    }

    *token_index = parser->token_count;
    token = &parser->tokens[parser->token_count];
    parser->token_count += 1U;
    token->type = type;
    token->start = start;
    token->end = start;
    token->child_count = 0U;
    token->parent = parent;
    if (parent != EVO_PROJECT_JSON_NO_PARENT) {
        parser->tokens[parent].child_count += 1U;
    }
    return EVO_PROJECT_JSON_SUCCESS;
}

static bool evo_project_json_is_hex(char value)
{
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

static evo_project_json_status_t evo_project_json_parse_string_token(
    evo_project_json_parser_t *parser,
    size_t parent,
    size_t *token_index)
{
    evo_project_json_status_t status;
    size_t index;

    if (parser->position >= parser->text_size ||
        parser->text[parser->position] != '"') {
        return EVO_PROJECT_JSON_INVALID;
    }

    parser->position += 1U;
    status = evo_project_json_add_token(
        parser,
        EVO_PROJECT_JSON_STRING,
        parser->position,
        parent,
        &index);
    if (status != EVO_PROJECT_JSON_SUCCESS) {
        return status;
    }

    while (parser->position < parser->text_size) {
        const unsigned char value =
            (unsigned char)parser->text[parser->position];

        if (value == (unsigned char)'"') {
            parser->tokens[index].end = parser->position;
            parser->position += 1U;
            *token_index = index;
            return EVO_PROJECT_JSON_SUCCESS;
        }
        if (value < 0x20U) {
            return EVO_PROJECT_JSON_INVALID;
        }
        if (value == (unsigned char)'\\') {
            size_t escape_index;
            char escape;

            parser->position += 1U;
            if (parser->position >= parser->text_size) {
                return EVO_PROJECT_JSON_INVALID;
            }
            escape = parser->text[parser->position];
            if (escape == '"' || escape == '\\' || escape == '/' ||
                escape == 'b' || escape == 'f' || escape == 'n' ||
                escape == 'r' || escape == 't') {
                parser->position += 1U;
                continue;
            }
            if (escape != 'u') {
                return EVO_PROJECT_JSON_INVALID;
            }
            if (parser->text_size - parser->position <= 4U) {
                return EVO_PROJECT_JSON_INVALID;
            }
            for (escape_index = 1U; escape_index <= 4U; escape_index += 1U) {
                if (!evo_project_json_is_hex(
                        parser->text[parser->position + escape_index])) {
                    return EVO_PROJECT_JSON_INVALID;
                }
            }
            parser->position += 5U;
            continue;
        }
        parser->position += 1U;
    }

    return EVO_PROJECT_JSON_INVALID;
}

static bool evo_project_json_is_digit(char value)
{
    return value >= '0' && value <= '9';
}

static evo_project_json_status_t evo_project_json_parse_number_token(
    evo_project_json_parser_t *parser,
    size_t parent,
    size_t *token_index)
{
    evo_project_json_status_t status;
    size_t index;
    const size_t start = parser->position;

    if (parser->text[parser->position] == '-') {
        parser->position += 1U;
        if (parser->position >= parser->text_size) {
            return EVO_PROJECT_JSON_INVALID;
        }
    }

    if (parser->text[parser->position] == '0') {
        parser->position += 1U;
        if (parser->position < parser->text_size &&
            evo_project_json_is_digit(parser->text[parser->position])) {
            return EVO_PROJECT_JSON_INVALID;
        }
    } else {
        if (parser->text[parser->position] < '1' ||
            parser->text[parser->position] > '9') {
            return EVO_PROJECT_JSON_INVALID;
        }
        while (parser->position < parser->text_size &&
               evo_project_json_is_digit(parser->text[parser->position])) {
            parser->position += 1U;
        }
    }

    if (parser->position < parser->text_size &&
        parser->text[parser->position] == '.') {
        parser->position += 1U;
        if (parser->position >= parser->text_size ||
            !evo_project_json_is_digit(parser->text[parser->position])) {
            return EVO_PROJECT_JSON_INVALID;
        }
        while (parser->position < parser->text_size &&
               evo_project_json_is_digit(parser->text[parser->position])) {
            parser->position += 1U;
        }
    }

    if (parser->position < parser->text_size &&
        (parser->text[parser->position] == 'e' ||
         parser->text[parser->position] == 'E')) {
        parser->position += 1U;
        if (parser->position < parser->text_size &&
            (parser->text[parser->position] == '+' ||
             parser->text[parser->position] == '-')) {
            parser->position += 1U;
        }
        if (parser->position >= parser->text_size ||
            !evo_project_json_is_digit(parser->text[parser->position])) {
            return EVO_PROJECT_JSON_INVALID;
        }
        while (parser->position < parser->text_size &&
               evo_project_json_is_digit(parser->text[parser->position])) {
            parser->position += 1U;
        }
    }

    status = evo_project_json_add_token(
        parser, EVO_PROJECT_JSON_NUMBER, start, parent, &index);
    if (status != EVO_PROJECT_JSON_SUCCESS) {
        return status;
    }
    parser->tokens[index].end = parser->position;
    *token_index = index;
    return EVO_PROJECT_JSON_SUCCESS;
}

static bool evo_project_json_match_literal(
    evo_project_json_parser_t *parser,
    const char *literal,
    size_t literal_size)
{
    size_t index;

    if (parser->text_size - parser->position < literal_size) {
        return false;
    }
    for (index = 0U; index < literal_size; index += 1U) {
        if (parser->text[parser->position + index] != literal[index]) {
            return false;
        }
    }
    parser->position += literal_size;
    return true;
}

static evo_project_json_status_t evo_project_json_parse_value(
    evo_project_json_parser_t *parser,
    size_t parent,
    size_t depth,
    size_t *token_index);

static evo_project_json_status_t evo_project_json_parse_object(
    evo_project_json_parser_t *parser,
    size_t parent,
    size_t depth,
    size_t *token_index)
{
    evo_project_json_status_t status;
    size_t object_index;

    if (depth > parser->maximum_depth) {
        return EVO_PROJECT_JSON_RESOURCE_LIMIT;
    }
    status = evo_project_json_add_token(
        parser,
        EVO_PROJECT_JSON_OBJECT,
        parser->position,
        parent,
        &object_index);
    if (status != EVO_PROJECT_JSON_SUCCESS) {
        return status;
    }
    parser->position += 1U;
    evo_project_json_skip_space(parser);
    if (parser->position < parser->text_size &&
        parser->text[parser->position] == '}') {
        parser->position += 1U;
        parser->tokens[object_index].end = parser->position;
        *token_index = object_index;
        return EVO_PROJECT_JSON_SUCCESS;
    }

    for (;;) {
        size_t ignored_index;

        status = evo_project_json_parse_string_token(
            parser, object_index, &ignored_index);
        if (status != EVO_PROJECT_JSON_SUCCESS) {
            return status;
        }
        evo_project_json_skip_space(parser);
        if (parser->position >= parser->text_size ||
            parser->text[parser->position] != ':') {
            return EVO_PROJECT_JSON_INVALID;
        }
        parser->position += 1U;
        evo_project_json_skip_space(parser);
        status = evo_project_json_parse_value(
            parser, object_index, depth + 1U, &ignored_index);
        if (status != EVO_PROJECT_JSON_SUCCESS) {
            return status;
        }
        evo_project_json_skip_space(parser);
        if (parser->position >= parser->text_size) {
            return EVO_PROJECT_JSON_INVALID;
        }
        if (parser->text[parser->position] == '}') {
            parser->position += 1U;
            parser->tokens[object_index].end = parser->position;
            *token_index = object_index;
            return EVO_PROJECT_JSON_SUCCESS;
        }
        if (parser->text[parser->position] != ',') {
            return EVO_PROJECT_JSON_INVALID;
        }
        parser->position += 1U;
        evo_project_json_skip_space(parser);
    }
}

static evo_project_json_status_t evo_project_json_parse_array(
    evo_project_json_parser_t *parser,
    size_t parent,
    size_t depth,
    size_t *token_index)
{
    evo_project_json_status_t status;
    size_t array_index;

    if (depth > parser->maximum_depth) {
        return EVO_PROJECT_JSON_RESOURCE_LIMIT;
    }
    status = evo_project_json_add_token(
        parser,
        EVO_PROJECT_JSON_ARRAY,
        parser->position,
        parent,
        &array_index);
    if (status != EVO_PROJECT_JSON_SUCCESS) {
        return status;
    }
    parser->position += 1U;
    evo_project_json_skip_space(parser);
    if (parser->position < parser->text_size &&
        parser->text[parser->position] == ']') {
        parser->position += 1U;
        parser->tokens[array_index].end = parser->position;
        *token_index = array_index;
        return EVO_PROJECT_JSON_SUCCESS;
    }

    for (;;) {
        size_t ignored_index;

        status = evo_project_json_parse_value(
            parser, array_index, depth + 1U, &ignored_index);
        if (status != EVO_PROJECT_JSON_SUCCESS) {
            return status;
        }
        evo_project_json_skip_space(parser);
        if (parser->position >= parser->text_size) {
            return EVO_PROJECT_JSON_INVALID;
        }
        if (parser->text[parser->position] == ']') {
            parser->position += 1U;
            parser->tokens[array_index].end = parser->position;
            *token_index = array_index;
            return EVO_PROJECT_JSON_SUCCESS;
        }
        if (parser->text[parser->position] != ',') {
            return EVO_PROJECT_JSON_INVALID;
        }
        parser->position += 1U;
        evo_project_json_skip_space(parser);
    }
}

static evo_project_json_status_t evo_project_json_parse_value(
    evo_project_json_parser_t *parser,
    size_t parent,
    size_t depth,
    size_t *token_index)
{
    evo_project_json_status_t status;
    size_t index;

    if (parser->position >= parser->text_size) {
        return EVO_PROJECT_JSON_INVALID;
    }
    if (parser->text[parser->position] == '{') {
        return evo_project_json_parse_object(
            parser, parent, depth, token_index);
    }
    if (parser->text[parser->position] == '[') {
        return evo_project_json_parse_array(
            parser, parent, depth, token_index);
    }
    if (parser->text[parser->position] == '"') {
        return evo_project_json_parse_string_token(
            parser, parent, token_index);
    }
    if (parser->text[parser->position] == '-' ||
        evo_project_json_is_digit(parser->text[parser->position])) {
        return evo_project_json_parse_number_token(
            parser, parent, token_index);
    }

    if (evo_project_json_match_literal(parser, "true", 4U)) {
        status = evo_project_json_add_token(
            parser, EVO_PROJECT_JSON_TRUE, parser->position - 4U, parent, &index);
    } else if (evo_project_json_match_literal(parser, "false", 5U)) {
        status = evo_project_json_add_token(
            parser, EVO_PROJECT_JSON_FALSE, parser->position - 5U, parent, &index);
    } else if (evo_project_json_match_literal(parser, "null", 4U)) {
        status = evo_project_json_add_token(
            parser, EVO_PROJECT_JSON_NULL, parser->position - 4U, parent, &index);
    } else {
        return EVO_PROJECT_JSON_INVALID;
    }
    if (status != EVO_PROJECT_JSON_SUCCESS) {
        return status;
    }
    parser->tokens[index].end = parser->position;
    *token_index = index;
    return EVO_PROJECT_JSON_SUCCESS;
}

evo_project_json_status_t evo_project_json_parse(
    const char *text,
    size_t text_size,
    evo_project_json_token_t *tokens,
    size_t token_capacity,
    size_t maximum_depth,
    size_t *token_count)
{
    evo_project_json_parser_t parser;
    evo_project_json_status_t status;
    size_t root_index;

    if (text == NULL || tokens == NULL || token_count == NULL ||
        text_size == 0U || token_capacity == 0U || maximum_depth == 0U) {
        return EVO_PROJECT_JSON_INVALID;
    }

    parser.text = text;
    parser.text_size = text_size;
    parser.position = 0U;
    parser.tokens = tokens;
    parser.token_capacity = token_capacity;
    parser.token_count = 0U;
    parser.maximum_depth = maximum_depth;
    evo_project_json_skip_space(&parser);
    status = evo_project_json_parse_value(
        &parser, EVO_PROJECT_JSON_NO_PARENT, 1U, &root_index);
    if (status != EVO_PROJECT_JSON_SUCCESS) {
        return status;
    }
    evo_project_json_skip_space(&parser);
    if (parser.position != parser.text_size || root_index != 0U) {
        return EVO_PROJECT_JSON_INVALID;
    }
    *token_count = parser.token_count;
    return EVO_PROJECT_JSON_SUCCESS;
}

size_t evo_project_json_next(
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t token_index)
{
    size_t index;

    if (tokens == NULL || token_index >= token_count) {
        return token_count;
    }
    index = token_index + 1U;
    while (index < token_count &&
           tokens[index].start < tokens[token_index].end) {
        index += 1U;
    }
    return index;
}

static bool evo_project_json_key_equals(
    const char *text,
    const evo_project_json_token_t *token,
    const char *key)
{
    size_t token_size;
    size_t key_size;
    size_t index;

    if (token->type != EVO_PROJECT_JSON_STRING) {
        return false;
    }
    token_size = token->end - token->start;
    key_size = strlen(key);
    if (token_size != key_size) {
        return false;
    }
    for (index = 0U; index < token_size; index += 1U) {
        if (text[token->start + index] != key[index]) {
            return false;
        }
    }
    return true;
}

int evo_project_json_object_get(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *key,
    size_t *value_index)
{
    size_t index;
    size_t found_count = 0U;
    size_t found_index = 0U;

    if (text == NULL || tokens == NULL || key == NULL || value_index == NULL ||
        object_index >= token_count ||
        tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        (tokens[object_index].child_count % 2U) != 0U) {
        return -1;
    }

    index = object_index + 1U;
    while (index < token_count &&
           tokens[index].parent == object_index) {
        const size_t value = index + 1U;

        if (value >= token_count || tokens[value].parent != object_index) {
            return -1;
        }
        if (evo_project_json_key_equals(text, &tokens[index], key)) {
            found_count += 1U;
            found_index = value;
        }
        index = evo_project_json_next(tokens, token_count, value);
    }
    if (found_count > 1U) {
        return -1;
    }
    if (found_count == 0U) {
        return 0;
    }
    *value_index = found_index;
    return 1;
}

bool evo_project_json_object_has_only(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *const *allowed_keys,
    size_t allowed_key_count)
{
    size_t index;

    if (text == NULL || tokens == NULL || allowed_keys == NULL ||
        object_index >= token_count ||
        tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        (tokens[object_index].child_count % 2U) != 0U) {
        return false;
    }
    index = object_index + 1U;
    while (index < token_count &&
           tokens[index].parent == object_index) {
        const size_t value = index + 1U;
        size_t allowed_index;
        bool allowed = false;

        if (value >= token_count || tokens[value].parent != object_index) {
            return false;
        }
        for (allowed_index = 0U; allowed_index < allowed_key_count;
             allowed_index += 1U) {
            if (evo_project_json_key_equals(
                    text, &tokens[index], allowed_keys[allowed_index])) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            return false;
        }
        index = evo_project_json_next(tokens, token_count, value);
    }
    return true;
}

static unsigned int evo_project_json_hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return (unsigned int)(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return (unsigned int)(value - 'a') + 10U;
    }
    return (unsigned int)(value - 'A') + 10U;
}

static bool evo_project_json_append_utf8(
    uint32_t code_point,
    char *output,
    size_t capacity,
    size_t *position)
{
    unsigned char bytes[4];
    size_t count;
    size_t index;

    if (code_point == 0U || code_point > 0x10ffffU ||
        (code_point >= 0xd800U && code_point <= 0xdfffU)) {
        return false;
    }
    if (code_point <= 0x7fU) {
        bytes[0] = (unsigned char)code_point;
        count = 1U;
    } else if (code_point <= 0x7ffU) {
        bytes[0] = (unsigned char)(0xc0U | (code_point >> 6U));
        bytes[1] = (unsigned char)(0x80U | (code_point & 0x3fU));
        count = 2U;
    } else if (code_point <= 0xffffU) {
        bytes[0] = (unsigned char)(0xe0U | (code_point >> 12U));
        bytes[1] = (unsigned char)(0x80U | ((code_point >> 6U) & 0x3fU));
        bytes[2] = (unsigned char)(0x80U | (code_point & 0x3fU));
        count = 3U;
    } else {
        bytes[0] = (unsigned char)(0xf0U | (code_point >> 18U));
        bytes[1] = (unsigned char)(0x80U | ((code_point >> 12U) & 0x3fU));
        bytes[2] = (unsigned char)(0x80U | ((code_point >> 6U) & 0x3fU));
        bytes[3] = (unsigned char)(0x80U | (code_point & 0x3fU));
        count = 4U;
    }
    if (capacity - *position < count) {
        return false;
    }
    for (index = 0U; index < count; index += 1U) {
        output[*position] = (char)bytes[index];
        *position += 1U;
    }
    return true;
}

static bool evo_project_json_validate_utf8(const char *value, size_t size)
{
    size_t index = 0U;

    while (index < size) {
        const unsigned char first = (unsigned char)value[index];
        uint32_t code_point;
        size_t count;
        size_t continuation;

        if (first <= 0x7fU) {
            if (first == 0U) {
                return false;
            }
            index += 1U;
            continue;
        }
        if (first >= 0xc2U && first <= 0xdfU) {
            code_point = (uint32_t)(first & 0x1fU);
            count = 2U;
        } else if (first >= 0xe0U && first <= 0xefU) {
            code_point = (uint32_t)(first & 0x0fU);
            count = 3U;
        } else if (first >= 0xf0U && first <= 0xf4U) {
            code_point = (uint32_t)(first & 0x07U);
            count = 4U;
        } else {
            return false;
        }
        if (size - index < count) {
            return false;
        }
        for (continuation = 1U; continuation < count; continuation += 1U) {
            const unsigned char next =
                (unsigned char)value[index + continuation];
            if ((next & 0xc0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (uint32_t)(next & 0x3fU);
        }
        if ((count == 3U && code_point < 0x800U) ||
            (count == 4U && code_point < 0x10000U) ||
            code_point > 0x10ffffU ||
            (code_point >= 0xd800U && code_point <= 0xdfffU)) {
            return false;
        }
        index += count;
    }
    return true;
}

bool evo_project_json_text_valid(
    const char *value,
    size_t maximum_bytes,
    bool allow_empty)
{
    size_t size = 0U;

    if (value == NULL || maximum_bytes == 0U) {
        return false;
    }
    while (value[size] != '\0') {
        const unsigned char byte = (unsigned char)value[size];

        if (size >= maximum_bytes || byte < 0x20U || byte == 0x7fU) {
            return false;
        }
        size += 1U;
    }
    return (allow_empty || size > 0U) &&
           evo_project_json_validate_utf8(value, size);
}

static bool evo_project_json_decode_unicode(
    const char *text,
    size_t end,
    size_t *input,
    uint32_t *code_point)
{
    uint32_t first = 0U;
    size_t index;

    if (end - *input < 5U || text[*input] != 'u') {
        return false;
    }
    for (index = 1U; index <= 4U; index += 1U) {
        first = (first << 4U) |
                (uint32_t)evo_project_json_hex_value(text[*input + index]);
    }
    *input += 5U;
    if (first >= 0xd800U && first <= 0xdbffU) {
        uint32_t second = 0U;

        if (end - *input < 6U || text[*input] != '\\' ||
            text[*input + 1U] != 'u') {
            return false;
        }
        *input += 2U;
        for (index = 0U; index < 4U; index += 1U) {
            second = (second << 4U) |
                     (uint32_t)evo_project_json_hex_value(text[*input + index]);
        }
        *input += 4U;
        if (second < 0xdc00U || second > 0xdfffU) {
            return false;
        }
        *code_point = 0x10000U + ((first - 0xd800U) << 10U) +
                      (second - 0xdc00U);
        return true;
    }
    if (first >= 0xdc00U && first <= 0xdfffU) {
        return false;
    }
    *code_point = first;
    return true;
}

evo_project_json_status_t evo_project_json_decode_string(
    const char *text,
    const evo_project_json_token_t *token,
    size_t maximum_bytes,
    char **decoded)
{
    char *output;
    size_t input;
    size_t output_position = 0U;

    if (text == NULL || token == NULL || decoded == NULL ||
        token->type != EVO_PROJECT_JSON_STRING || maximum_bytes == 0U ||
        token->end < token->start ||
        token->end - token->start > maximum_bytes) {
        return EVO_PROJECT_JSON_INVALID;
    }
    if (maximum_bytes == SIZE_MAX) {
        return EVO_PROJECT_JSON_RESOURCE_LIMIT;
    }
    output = evo_project_allocate_zeroed(maximum_bytes + 1U, sizeof(*output));
    if (output == NULL) {
        return EVO_PROJECT_JSON_OUT_OF_MEMORY;
    }

    input = token->start;
    while (input < token->end) {
        const unsigned char value = (unsigned char)text[input];

        if (value != (unsigned char)'\\') {
            if (output_position >= maximum_bytes) {
                evo_project_release(output);
                return EVO_PROJECT_JSON_RESOURCE_LIMIT;
            }
            output[output_position] = (char)value;
            output_position += 1U;
            input += 1U;
            continue;
        }

        input += 1U;
        if (input >= token->end) {
            evo_project_release(output);
            return EVO_PROJECT_JSON_INVALID;
        }
        if (text[input] == 'u') {
            uint32_t code_point;

            if (!evo_project_json_decode_unicode(
                    text, token->end, &input, &code_point) ||
                !evo_project_json_append_utf8(
                    code_point,
                    output,
                    maximum_bytes,
                    &output_position)) {
                evo_project_release(output);
                return EVO_PROJECT_JSON_INVALID;
            }
            continue;
        }
        if (output_position >= maximum_bytes) {
            evo_project_release(output);
            return EVO_PROJECT_JSON_RESOURCE_LIMIT;
        }
        switch (text[input]) {
        case '"':
        case '\\':
        case '/':
            output[output_position] = text[input];
            break;
        case 'b':
            output[output_position] = '\b';
            break;
        case 'f':
            output[output_position] = '\f';
            break;
        case 'n':
            output[output_position] = '\n';
            break;
        case 'r':
            output[output_position] = '\r';
            break;
        case 't':
            output[output_position] = '\t';
            break;
        default:
            evo_project_release(output);
            return EVO_PROJECT_JSON_INVALID;
        }
        output_position += 1U;
        input += 1U;
    }
    if (!evo_project_json_validate_utf8(output, output_position)) {
        evo_project_release(output);
        return EVO_PROJECT_JSON_INVALID;
    }
    output[output_position] = '\0';
    *decoded = output;
    return EVO_PROJECT_JSON_SUCCESS;
}

bool evo_project_json_parse_u64(
    const char *text,
    const evo_project_json_token_t *token,
    uint64_t *value)
{
    uint64_t parsed = 0U;
    size_t index;

    if (text == NULL || token == NULL || value == NULL ||
        token->type != EVO_PROJECT_JSON_NUMBER || token->end <= token->start) {
        return false;
    }
    for (index = token->start; index < token->end; index += 1U) {
        const unsigned int digit = (unsigned int)(text[index] - '0');

        if (!evo_project_json_is_digit(text[index]) ||
            parsed > (UINT64_MAX - (uint64_t)digit) / 10U) {
            return false;
        }
        parsed = (parsed * 10U) + (uint64_t)digit;
    }
    *value = parsed;
    return true;
}

bool evo_project_json_parse_i64(
    const char *text,
    const evo_project_json_token_t *token,
    int64_t *value)
{
    const uint64_t negative_limit = (uint64_t)INT64_MAX + UINT64_C(1);
    uint64_t parsed = 0U;
    uint64_t limit;
    size_t index;
    bool negative;

    if (text == NULL || token == NULL || value == NULL ||
        token->type != EVO_PROJECT_JSON_NUMBER || token->end <= token->start) {
        return false;
    }
    negative = text[token->start] == '-';
    index = token->start + (negative ? 1U : 0U);
    if (index >= token->end) {
        return false;
    }
    limit = negative ? negative_limit : (uint64_t)INT64_MAX;
    for (; index < token->end; index += 1U) {
        const unsigned int digit = (unsigned int)(text[index] - '0');

        if (!evo_project_json_is_digit(text[index]) ||
            parsed > (limit - (uint64_t)digit) / UINT64_C(10)) {
            return false;
        }
        parsed = (parsed * UINT64_C(10)) + (uint64_t)digit;
    }
    if (negative) {
        *value = parsed == negative_limit
                     ? INT64_MIN
                     : -(int64_t)parsed;
    } else {
        *value = (int64_t)parsed;
    }
    return true;
}

bool evo_project_json_parse_bool(
    const evo_project_json_token_t *token,
    bool *value)
{
    if (token == NULL || value == NULL) {
        return false;
    }
    if (token->type == EVO_PROJECT_JSON_TRUE) {
        *value = true;
        return true;
    }
    if (token->type == EVO_PROJECT_JSON_FALSE) {
        *value = false;
        return true;
    }
    return false;
}
