#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_AST_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_AST_H

#include "internal/project_analysis.h"
#include "internal/project_transformation.h"

#include <stddef.h>
#include <stdint.h>

typedef struct evo_project_clang_ast_context {
    size_t compilation_unit_count;
    const evo_project_compilation_record_t *compilation_units;
    uint64_t timeout_ms;
    uint64_t max_memory_bytes;
    size_t max_processes;
    uint64_t max_storage_bytes;
    size_t max_output_bytes;
    char *location_identity;
    char *file;
    char *primary_declaration_identity;
    char *duplicate_declaration_identity;
} evo_project_clang_ast_context_t;

evo_project_transformation_status_t evo_project_clang_ast_provider(
    const evo_project_transformation_request_t *request,
    void *context,
    evo_project_transformation_ast_result_t *result);

void evo_project_clang_ast_context_destroy(
    evo_project_clang_ast_context_t *context);

#endif
