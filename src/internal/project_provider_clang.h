#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_H

#include "internal/project_analysis.h"
#include "internal/project_provider_sandbox.h"
#include "internal/project_transformation.h"

#include <stddef.h>

typedef struct evo_project_clang_analysis_context {
    evo_project_source_location_record_t *source_locations;
    size_t source_location_count;
    size_t source_location_capacity;
    evo_project_declaration_record_t *declarations;
    size_t declaration_count;
    size_t declaration_capacity;
} evo_project_clang_analysis_context_t;

/*
 * Private product wiring for AST inspection. Compilation records are borrowed
 * immutable baseline evidence. The provider reconstructs its own conservative
 * Clang argv; it never executes the compiler command captured in the database.
 * Stable declaration identities are retained in the context so the borrowed
 * provider result remains valid until evo_project_transformation_apply copies it.
 */
typedef struct evo_project_clang_ast_context {
    const char *clang_program;
    size_t compilation_unit_count;
    const evo_project_compilation_record_t *compilation_units;
    evo_project_sandbox_limits_t sandbox_limits;
    size_t max_json_tokens;
    size_t max_json_depth;
    char primary_declaration_identity[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char duplicate_declaration_identity[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
} evo_project_clang_ast_context_t;

evo_project_analysis_status_t evo_project_clang_analysis_provider(
    const evo_project_analysis_request_t *request,
    void *context,
    evo_project_analysis_provider_result_t *result);

void evo_project_clang_analysis_context_destroy(
    evo_project_clang_analysis_context_t *context);

evo_project_transformation_status_t evo_project_clang_ast_provider(
    const evo_project_transformation_request_t *request,
    void *context,
    evo_project_transformation_ast_result_t *result);

#endif
