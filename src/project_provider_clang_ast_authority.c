#include "internal/project_provider_clang_ast_authority.h"

#include "internal/project_json.h"
#include "internal/project_runtime.h"

#include <stdint.h>
#include <string.h>

#define EVO_CLANG_AST_JSON_MAX_DEPTH 64U

typedef struct evo_clang_ast_json {
    const char *text;
    const evo_project_transformation_limits_t *limits;
    evo_project_json_token_t *tokens;
    size_t token_count;
} evo_clang_ast_json_t;

typedef struct evo_clang_ast_declref {
    char *id;
    char *name;
    char *type;
} evo_clang_ast_declref_t;

static void evo_ast_declref_destroy(evo_clang_ast_declref_t *reference)
{
    if (reference == NULL) {
        return;
    }
    evo_project_release(reference->id);
    evo_project_release(reference->name);
    evo_project_release(reference->type);
    *reference = (evo_clang_ast_declref_t){0};
}

static evo_project_transformation_status_t evo_ast_json_open(
    const char *text,
    size_t text_size,
    const evo_project_transformation_limits_t *limits,
    evo_clang_ast_json_t *json)
{
    size_t token_capacity;
    evo_project_json_status_t status;

    if (text == NULL || text_size == 0U || limits == NULL || json == NULL ||
        limits->max_total_bytes < sizeof(evo_project_json_token_t)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
    }
    token_capacity = limits->max_total_bytes / sizeof(evo_project_json_token_t);
    if (token_capacity > text_size + 1U) {
        token_capacity = text_size + 1U;
    }
    if (token_capacity == 0U ||
        token_capacity > SIZE_MAX / sizeof(evo_project_json_token_t)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    json->tokens = evo_project_allocate_zeroed(
        token_capacity, sizeof(*json->tokens));
    if (json->tokens == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    json->text = text;
    json->limits = limits;
    status = evo_project_json_parse(
        text,
        text_size,
        json->tokens,
        token_capacity,
        EVO_CLANG_AST_JSON_MAX_DEPTH,
        &json->token_count);
    if (status != EVO_PROJECT_JSON_SUCCESS) {
        evo_project_release(json->tokens);
        *json = (evo_clang_ast_json_t){0};
        if (status == EVO_PROJECT_JSON_OUT_OF_MEMORY) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
        }
        if (status == EVO_PROJECT_JSON_RESOURCE_LIMIT) {
            return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
        }
        return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
    }
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static void evo_ast_json_close(evo_clang_ast_json_t *json)
{
    if (json == NULL) {
        return;
    }
    evo_project_release(json->tokens);
    *json = (evo_clang_ast_json_t){0};
}

static int evo_ast_member(
    const evo_clang_ast_json_t *json,
    size_t object,
    const char *name,
    size_t *value)
{
    if (json == NULL || value == NULL || object >= json->token_count ||
        json->tokens[object].type != EVO_PROJECT_JSON_OBJECT) {
        return -1;
    }
    return evo_project_json_object_get(
        json->text,
        json->tokens,
        json->token_count,
        object,
        name,
        value);
}

static char *evo_ast_string_member(
    const evo_clang_ast_json_t *json,
    size_t object,
    const char *name)
{
    size_t value;
    char *decoded = NULL;

    if (evo_ast_member(json, object, name, &value) != 1 ||
        json->tokens[value].type != EVO_PROJECT_JSON_STRING ||
        evo_project_json_decode_string(
            json->text,
            &json->tokens[value],
            json->limits->max_string_bytes,
            &decoded) != EVO_PROJECT_JSON_SUCCESS) {
        return NULL;
    }
    return decoded;
}

static bool evo_ast_u64_member(
    const evo_clang_ast_json_t *json,
    size_t object,
    const char *name,
    uint64_t *value)
{
    size_t member;

    return evo_ast_member(json, object, name, &member) == 1 &&
           evo_project_json_parse_u64(
               json->text, &json->tokens[member], value);
}

static bool evo_ast_endpoint(
    const evo_clang_ast_json_t *json,
    size_t object,
    size_t *offset,
    size_t *token_length,
    bool *macro)
{
    uint64_t parsed_offset;
    uint64_t parsed_length;
    size_t nested;

    if (evo_ast_u64_member(json, object, "offset", &parsed_offset) &&
        evo_ast_u64_member(json, object, "tokLen", &parsed_length) &&
        parsed_offset <= SIZE_MAX && parsed_length > 0U &&
        parsed_length <= SIZE_MAX - (size_t)parsed_offset) {
        *offset = (size_t)parsed_offset;
        *token_length = (size_t)parsed_length;
        return true;
    }
    if (evo_ast_member(json, object, "expansionLoc", &nested) == 1 &&
        json->tokens[nested].type == EVO_PROJECT_JSON_OBJECT) {
        *macro = true;
        return evo_ast_endpoint(json, nested, offset, token_length, macro);
    }
    if (evo_ast_member(json, object, "spellingLoc", &nested) == 1 &&
        json->tokens[nested].type == EVO_PROJECT_JSON_OBJECT) {
        *macro = true;
        return evo_ast_endpoint(json, nested, offset, token_length, macro);
    }
    return false;
}

static bool evo_ast_object_range(
    const evo_clang_ast_json_t *json,
    size_t object,
    evo_project_transformation_byte_range_t *range,
    bool *macro)
{
    size_t range_object;
    size_t begin;
    size_t end;
    size_t begin_offset;
    size_t begin_length;
    size_t end_offset;
    size_t end_length;

    if (evo_ast_member(json, object, "range", &range_object) != 1 ||
        json->tokens[range_object].type != EVO_PROJECT_JSON_OBJECT ||
        evo_ast_member(json, range_object, "begin", &begin) != 1 ||
        evo_ast_member(json, range_object, "end", &end) != 1 ||
        json->tokens[begin].type != EVO_PROJECT_JSON_OBJECT ||
        json->tokens[end].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_ast_endpoint(
            json, begin, &begin_offset, &begin_length, macro) ||
        !evo_ast_endpoint(
            json, end, &end_offset, &end_length, macro) ||
        end_offset > SIZE_MAX - end_length) {
        return false;
    }
    range->start = begin_offset;
    range->end = end_offset + end_length;
    return range->start < range->end;
}

static bool evo_ast_range_equal(
    evo_project_transformation_byte_range_t left,
    evo_project_transformation_byte_range_t right)
{
    return left.start == right.start && left.end == right.end;
}

static bool evo_ast_token_contains(
    const evo_clang_ast_json_t *json,
    size_t token,
    const char *needle)
{
    const size_t needle_size = strlen(needle);
    size_t index;
    size_t offset;

    if (needle_size == 0U || token >= json->token_count ||
        json->tokens[token].end < json->tokens[token].start ||
        json->tokens[token].end - json->tokens[token].start < needle_size) {
        return false;
    }
    for (index = json->tokens[token].start;
         index + needle_size <= json->tokens[token].end;
         index += 1U) {
        bool equal = true;

        for (offset = 0U; offset < needle_size; offset += 1U) {
            if (json->text[index + offset] != needle[offset]) {
                equal = false;
                break;
            }
        }
        if (equal) {
            return true;
        }
    }
    return false;
}

static bool evo_ast_kind_opcode(
    const evo_clang_ast_json_t *json,
    size_t object,
    const char *kind,
    const char *opcode)
{
    char *observed_kind = evo_ast_string_member(json, object, "kind");
    char *observed_opcode = NULL;
    bool matched;

    if (observed_kind == NULL) {
        return false;
    }
    matched = strcmp(observed_kind, kind) == 0;
    if (matched && opcode != NULL) {
        observed_opcode = evo_ast_string_member(json, object, "opcode");
        matched = observed_opcode != NULL && strcmp(observed_opcode, opcode) == 0;
    }
    evo_project_release(observed_opcode);
    evo_project_release(observed_kind);
    return matched;
}

static evo_project_transformation_status_t evo_ast_find_exact(
    const evo_clang_ast_json_t *json,
    evo_project_transformation_byte_range_t target,
    const char *kind,
    const char *opcode,
    size_t *object,
    bool *contains_macro)
{
    size_t index;
    size_t match_count = 0U;
    size_t selected = 0U;
    bool selected_macro = false;

    for (index = 0U; index < json->token_count; index += 1U) {
        evo_project_transformation_byte_range_t range;
        bool macro = false;

        if (json->tokens[index].type != EVO_PROJECT_JSON_OBJECT ||
            !evo_ast_kind_opcode(json, index, kind, opcode) ||
            !evo_ast_object_range(json, index, &range, &macro) ||
            !evo_ast_range_equal(range, target)) {
            continue;
        }
        selected = index;
        selected_macro = macro;
        match_count += 1U;
    }
    if (match_count == 0U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    if (match_count != 1U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET;
    }
    *object = selected;
    *contains_macro = selected_macro ||
                      evo_ast_token_contains(json, selected, "\"expansionLoc\"") ||
                      evo_ast_token_contains(json, selected, "\"spellingLoc\"");
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static char *evo_ast_type_member(
    const evo_clang_ast_json_t *json,
    size_t object,
    const char *member_name)
{
    size_t type_object;
    char *desugared;
    char *qualified;

    if (evo_ast_member(json, object, member_name, &type_object) != 1 ||
        json->tokens[type_object].type != EVO_PROJECT_JSON_OBJECT) {
        return NULL;
    }
    desugared = evo_ast_string_member(json, type_object, "desugaredQualType");
    if (desugared != NULL) {
        return desugared;
    }
    qualified = evo_ast_string_member(json, type_object, "qualType");
    return qualified;
}

static char *evo_ast_expression_type(
    const evo_clang_ast_json_t *json,
    size_t object)
{
    return evo_ast_type_member(json, object, "type");
}

static bool evo_ast_declref_at(
    const evo_clang_ast_json_t *json,
    size_t root,
    evo_project_transformation_byte_range_t expected,
    evo_clang_ast_declref_t *reference)
{
    size_t index;
    size_t match_count = 0U;
    evo_clang_ast_declref_t selected = {0};

    for (index = root; index < json->token_count; index += 1U) {
        evo_project_transformation_byte_range_t range;
        bool macro = false;
        size_t referenced;
        char *id;
        char *name;
        char *type;

        if (json->tokens[index].start >= json->tokens[root].end) {
            break;
        }
        if (json->tokens[index].type != EVO_PROJECT_JSON_OBJECT ||
            !evo_ast_kind_opcode(json, index, "DeclRefExpr", NULL) ||
            !evo_ast_object_range(json, index, &range, &macro) || macro ||
            !evo_ast_range_equal(range, expected) ||
            evo_ast_member(json, index, "referencedDecl", &referenced) != 1 ||
            json->tokens[referenced].type != EVO_PROJECT_JSON_OBJECT) {
            continue;
        }
        id = evo_ast_string_member(json, referenced, "id");
        name = evo_ast_string_member(json, referenced, "name");
        type = evo_ast_type_member(json, referenced, "type");
        if (id == NULL || name == NULL || type == NULL) {
            evo_project_release(id);
            evo_project_release(name);
            evo_project_release(type);
            evo_ast_declref_destroy(&selected);
            return false;
        }
        evo_ast_declref_destroy(&selected);
        selected.id = id;
        selected.name = name;
        selected.type = type;
        match_count += 1U;
    }
    if (match_count != 1U) {
        evo_ast_declref_destroy(&selected);
        return false;
    }
    *reference = selected;
    return true;
}

static uint32_t evo_ast_unsigned_width(const char *type)
{
    if (type == NULL) {
        return 0U;
    }
    if (strcmp(type, "unsigned char") == 0) {
        return 8U;
    }
    if (strcmp(type, "unsigned short") == 0 ||
        strcmp(type, "unsigned short int") == 0 ||
        strcmp(type, "unsigned int") == 0) {
        return 16U;
    }
    if (strcmp(type, "unsigned long") == 0 ||
        strcmp(type, "unsigned long int") == 0) {
        return 32U;
    }
    if (strcmp(type, "unsigned long long") == 0 ||
        strcmp(type, "unsigned long long int") == 0) {
        return 64U;
    }
    return 0U;
}

static const char *evo_ast_binary_operator_text(
    evo_project_transformation_operator_t operator_kind)
{
    switch (operator_kind) {
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_ADD:
        return "+";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_AND:
        return "&";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_OR:
        return "|";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_BITWISE_XOR:
        return "^";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_MULTIPLY:
        return "*";
    case EVO_PROJECT_TRANSFORMATION_OPERATOR_SUBTRACT:
        return "-";
    default:
        return NULL;
    }
}

static bool evo_ast_compound_operator_text(
    evo_project_transformation_operator_t operator_kind,
    char output[3])
{
    const char *binary = evo_ast_binary_operator_text(operator_kind);

    if (binary == NULL || binary[0] == '\0' || binary[1] != '\0') {
        return false;
    }
    output[0] = binary[0];
    output[1] = '=';
    output[2] = '\0';
    return true;
}

static void evo_ast_target_flags(
    const evo_clang_ast_json_t *json,
    size_t root,
    bool range_macro,
    evo_project_clang_ast_authority_t *authority)
{
    authority->contains_macro =
        range_macro ||
        evo_ast_token_contains(json, root, "\"expansionLoc\"") ||
        evo_ast_token_contains(json, root, "\"spellingLoc\"");
    authority->volatile_access =
        evo_ast_token_contains(json, root, "volatile");
    authority->language_extension =
        evo_ast_token_contains(json, root, "__int128") ||
        evo_ast_token_contains(json, root, "\"kind\": \"StmtExpr\"") ||
        evo_ast_token_contains(json, root, "\"kind\":\"StmtExpr\"");
}

static bool evo_ast_direct_child_exact(
    const evo_clang_ast_json_t *json,
    size_t statement,
    size_t ordinal,
    evo_project_transformation_byte_range_t target,
    size_t *child)
{
    size_t inner;
    size_t index;
    size_t observed = 0U;

    if (evo_ast_member(json, statement, "inner", &inner) != 1 ||
        json->tokens[inner].type != EVO_PROJECT_JSON_ARRAY) {
        return false;
    }
    for (index = inner + 1U; index < json->token_count; index += 1U) {
        evo_project_transformation_byte_range_t range;
        bool macro = false;

        if (json->tokens[index].start >= json->tokens[inner].end) {
            break;
        }
        if (json->tokens[index].parent != inner) {
            continue;
        }
        if (observed == ordinal &&
            json->tokens[index].type == EVO_PROJECT_JSON_OBJECT &&
            evo_ast_object_range(json, index, &range, &macro) &&
            evo_ast_range_equal(range, target)) {
            *child = index;
            return true;
        }
        observed += 1U;
    }
    return false;
}

static evo_project_transformation_status_t evo_ast_condition_node(
    const evo_clang_ast_json_t *json,
    evo_project_transformation_byte_range_t target,
    size_t *condition,
    evo_project_transformation_condition_context_t *context)
{
    size_t index;
    size_t match_count = 0U;
    size_t selected = 0U;
    evo_project_transformation_condition_context_t selected_context =
        EVO_PROJECT_TRANSFORMATION_CONDITION_NONE;

    for (index = 0U; index < json->token_count; index += 1U) {
        char *kind;
        size_t ordinal;
        evo_project_transformation_condition_context_t candidate_context;
        size_t child;

        if (json->tokens[index].type != EVO_PROJECT_JSON_OBJECT) {
            continue;
        }
        kind = evo_ast_string_member(json, index, "kind");
        if (kind == NULL) {
            continue;
        }
        if (strcmp(kind, "IfStmt") == 0) {
            ordinal = 0U;
            candidate_context = EVO_PROJECT_TRANSFORMATION_CONDITION_IF;
        } else if (strcmp(kind, "WhileStmt") == 0) {
            ordinal = 0U;
            candidate_context = EVO_PROJECT_TRANSFORMATION_CONDITION_WHILE;
        } else if (strcmp(kind, "DoStmt") == 0) {
            ordinal = 1U;
            candidate_context = EVO_PROJECT_TRANSFORMATION_CONDITION_DO_WHILE;
        } else if (strcmp(kind, "ForStmt") == 0) {
            ordinal = 2U;
            candidate_context = EVO_PROJECT_TRANSFORMATION_CONDITION_FOR;
        } else {
            evo_project_release(kind);
            continue;
        }
        evo_project_release(kind);
        if (evo_ast_direct_child_exact(
                json, index, ordinal, target, &child)) {
            selected = child;
            selected_context = candidate_context;
            match_count += 1U;
        }
    }
    if (match_count == 0U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
    }
    if (match_count != 1U) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET;
    }
    *condition = selected;
    *context = selected_context;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static bool evo_ast_direct_child_kind_opcode(
    const evo_clang_ast_json_t *json,
    size_t object,
    size_t ordinal,
    const char *kind,
    const char *opcode)
{
    size_t inner;
    size_t index;
    size_t observed = 0U;

    if (evo_ast_member(json, object, "inner", &inner) != 1 ||
        json->tokens[inner].type != EVO_PROJECT_JSON_ARRAY) {
        return false;
    }
    for (index = inner + 1U; index < json->token_count; index += 1U) {
        if (json->tokens[index].start >= json->tokens[inner].end) {
            break;
        }
        if (json->tokens[index].parent != inner) {
            continue;
        }
        if (observed == ordinal) {
            return json->tokens[index].type == EVO_PROJECT_JSON_OBJECT &&
                   evo_ast_kind_opcode(json, index, kind, opcode);
        }
        observed += 1U;
    }
    return false;
}

evo_project_transformation_status_t evo_project_clang_ast_authorize_assignment(
    const char *json_text,
    size_t json_size,
    evo_project_transformation_byte_range_t target,
    evo_project_transformation_byte_range_t primary,
    evo_project_transformation_byte_range_t duplicate_primary,
    evo_project_transformation_byte_range_t operand,
    bool compound,
    evo_project_transformation_operator_t operator_kind,
    const evo_project_transformation_limits_t *limits,
    evo_project_clang_ast_authority_t *authority)
{
    evo_clang_ast_json_t json = {0};
    evo_clang_ast_declref_t primary_reference = {0};
    evo_clang_ast_declref_t duplicate_reference = {0};
    evo_project_transformation_status_t status;
    size_t root = 0U;
    size_t rhs = 0U;
    bool macro = false;
    char compound_opcode[3] = {0};
    const char *binary_opcode = evo_ast_binary_operator_text(operator_kind);
    const char *root_kind = compound ? "CompoundAssignOperator" : "BinaryOperator";
    const char *root_opcode = compound ? compound_opcode : "=";
    char *root_type = NULL;
    char *rhs_type = NULL;
    char *compute_lhs = NULL;
    char *compute_result = NULL;

    if (authority == NULL || binary_opcode == NULL ||
        (compound && !evo_ast_compound_operator_text(operator_kind, compound_opcode))) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
    }
    *authority = (evo_project_clang_ast_authority_t){0};
    status = evo_ast_json_open(json_text, json_size, limits, &json);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    status = evo_ast_find_exact(
        &json, target, root_kind, root_opcode, &root, &macro);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        authority->ambiguous_target =
            status == EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET;
        goto finish;
    }
    evo_ast_target_flags(&json, root, macro, authority);
    if (!evo_ast_declref_at(&json, root, primary, &primary_reference)) {
        status = EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
        goto finish;
    }
    authority->primary_reference_resolved = true;
    root_type = evo_ast_expression_type(&json, root);
    if (root_type == NULL) {
        status = EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
        goto finish;
    }
    if (compound) {
        compute_lhs = evo_ast_type_member(&json, root, "computeLHSType");
        compute_result = evo_ast_type_member(&json, root, "computeResultType");
        authority->result_type_matches_primary =
            strcmp(root_type, primary_reference.type) == 0 &&
            compute_lhs != NULL && compute_result != NULL &&
            strcmp(compute_lhs, primary_reference.type) == 0 &&
            strcmp(compute_result, primary_reference.type) == 0;
    } else {
        evo_project_transformation_byte_range_t rhs_range = {
            duplicate_primary.start, operand.end};

        if (!evo_ast_declref_at(
                &json, root, duplicate_primary, &duplicate_reference)) {
            status = EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
            goto finish;
        }
        authority->duplicate_reference_matches =
            strcmp(primary_reference.id, duplicate_reference.id) == 0;
        status = evo_ast_find_exact(
            &json, rhs_range, "BinaryOperator", binary_opcode, &rhs, &macro);
        if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
            authority->ambiguous_target =
                status == EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET;
            goto finish;
        }
        rhs_type = evo_ast_expression_type(&json, rhs);
        if (rhs_type == NULL) {
            status = EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
            goto finish;
        }
        authority->result_type_matches_primary =
            strcmp(root_type, primary_reference.type) == 0 &&
            strcmp(rhs_type, primary_reference.type) == 0 &&
            strcmp(duplicate_reference.type, primary_reference.type) == 0;
    }
    status = EVO_PROJECT_TRANSFORMATION_SUCCESS;

finish:
    evo_project_release(compute_result);
    evo_project_release(compute_lhs);
    evo_project_release(rhs_type);
    evo_project_release(root_type);
    evo_ast_declref_destroy(&duplicate_reference);
    evo_ast_declref_destroy(&primary_reference);
    evo_ast_json_close(&json);
    return status;
}

evo_project_transformation_status_t evo_project_clang_ast_authorize_condition(
    const char *json_text,
    size_t json_size,
    evo_project_transformation_byte_range_t target,
    bool double_negated,
    const evo_project_transformation_limits_t *limits,
    evo_project_clang_ast_authority_t *authority)
{
    evo_clang_ast_json_t json = {0};
    evo_project_transformation_status_t status;
    size_t condition = 0U;
    bool macro = false;

    if (authority == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
    }
    *authority = (evo_project_clang_ast_authority_t){0};
    status = evo_ast_json_open(json_text, json_size, limits, &json);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    status = evo_ast_condition_node(
        &json, target, &condition, &authority->condition_context);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        authority->ambiguous_target =
            status == EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET;
        goto finish;
    }
    {
        evo_project_transformation_byte_range_t range;

        if (!evo_ast_object_range(&json, condition, &range, &macro)) {
            status = EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
            goto finish;
        }
    }
    evo_ast_target_flags(&json, condition, macro, authority);
    authority->scalar_operand = true;
    if (double_negated &&
        (!evo_ast_kind_opcode(&json, condition, "UnaryOperator", "!") ||
         !evo_ast_direct_child_kind_opcode(
             &json, condition, 0U, "UnaryOperator", "!"))) {
        status = EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE;
        goto finish;
    }
    status = EVO_PROJECT_TRANSFORMATION_SUCCESS;

finish:
    evo_ast_json_close(&json);
    return status;
}

evo_project_transformation_status_t evo_project_clang_ast_authorize_shift(
    const char *json_text,
    size_t json_size,
    evo_project_transformation_byte_range_t expression,
    evo_project_transformation_byte_range_t primary,
    evo_project_transformation_byte_range_t literal,
    bool shift,
    const evo_project_transformation_limits_t *limits,
    evo_project_clang_ast_authority_t *authority)
{
    evo_clang_ast_json_t json = {0};
    evo_clang_ast_declref_t primary_reference = {0};
    evo_project_transformation_status_t status;
    size_t root = 0U;
    size_t literal_node = 0U;
    bool macro = false;
    char *root_type = NULL;

    if (authority == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
    }
    *authority = (evo_project_clang_ast_authority_t){0};
    status = evo_ast_json_open(json_text, json_size, limits, &json);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    status = evo_ast_find_exact(
        &json,
        expression,
        "BinaryOperator",
        shift ? "<<" : "*",
        &root,
        &macro);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        authority->ambiguous_target =
            status == EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET;
        goto finish;
    }
    evo_ast_target_flags(&json, root, macro, authority);
    if (!evo_ast_declref_at(&json, root, primary, &primary_reference)) {
        status = EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
        goto finish;
    }
    authority->primary_reference_resolved = true;
    status = evo_ast_find_exact(
        &json, literal, "IntegerLiteral", NULL, &literal_node, &macro);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        authority->ambiguous_target =
            status == EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET;
        goto finish;
    }
    root_type = evo_ast_expression_type(&json, root);
    if (root_type == NULL) {
        status = EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED;
        goto finish;
    }
    authority->result_width_bits = evo_ast_unsigned_width(root_type);
    authority->result_unsigned_integer = authority->result_width_bits > 0U;
    authority->result_type_matches_primary =
        strcmp(root_type, primary_reference.type) == 0;
    status = EVO_PROJECT_TRANSFORMATION_SUCCESS;

finish:
    evo_project_release(root_type);
    evo_ast_declref_destroy(&primary_reference);
    evo_ast_json_close(&json);
    return status;
}
