#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_AST_AUTHORITY_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_AST_AUTHORITY_H

#include "internal/project_transformation.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct evo_project_clang_ast_authority {
    bool contains_macro;
    bool volatile_access;
    bool language_extension;
    bool ambiguous_target;
    bool primary_reference_resolved;
    bool duplicate_reference_matches;
    bool result_type_matches_primary;
    bool scalar_operand;
    bool result_unsigned_integer;
    uint32_t result_width_bits;
    evo_project_transformation_condition_context_t condition_context;
} evo_project_clang_ast_authority_t;

evo_project_transformation_status_t evo_project_clang_ast_authorize_assignment(
    const char *json,
    size_t json_size,
    evo_project_transformation_byte_range_t target,
    evo_project_transformation_byte_range_t primary,
    evo_project_transformation_byte_range_t duplicate_primary,
    evo_project_transformation_byte_range_t operand,
    bool compound,
    evo_project_transformation_operator_t operator_kind,
    const evo_project_transformation_limits_t *limits,
    evo_project_clang_ast_authority_t *authority);

evo_project_transformation_status_t evo_project_clang_ast_authorize_condition(
    const char *json,
    size_t json_size,
    evo_project_transformation_byte_range_t target,
    bool double_negated,
    const evo_project_transformation_limits_t *limits,
    evo_project_clang_ast_authority_t *authority);

evo_project_transformation_status_t evo_project_clang_ast_authorize_shift(
    const char *json,
    size_t json_size,
    evo_project_transformation_byte_range_t expression,
    evo_project_transformation_byte_range_t primary,
    evo_project_transformation_byte_range_t literal,
    bool shift,
    const evo_project_transformation_limits_t *limits,
    evo_project_clang_ast_authority_t *authority);

#endif
