#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_H

#include "internal/project_analysis.h"
#include "internal/project_transformation.h"

#include <stddef.h>
#include <stdint.h>

typedef struct evo_project_clang_analysis_context {
    evo_project_source_location_record_t *source_locations;
    size_t source_location_count;
    size_t source_location_capacity;
    evo_project_declaration_record_t *declarations;
    size_t declaration_count;
    size_t declaration_capacity;
} evo_project_clang_analysis_context_t;

/*
 * Private product wiring for transformation AST inspection. Compilation records
 * are borrowed immutable baseline evidence. The provider reconstructs its own
 * conservative Clang argv and never executes a captured compiler command.
 * Strings retained here keep borrowed result identities valid until the
 * transformation layer copies the provider result.
 */
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

void evo_project_clang_ast_context_destroy(
    evo_project_clang_ast_context_t *context);

#endif
