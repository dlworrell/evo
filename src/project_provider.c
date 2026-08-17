#include "internal/project_provider.h"

#include "internal/project_provider_probe.h"

#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <unistd.h>
#endif

#define EVO_PROJECT_PROVIDER_EXEC_CAPABILITIES              \
    (EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV |          \
     EVO_PROJECT_PROVIDER_CAPABILITY_CPU_LIMIT |            \
     EVO_PROJECT_PROVIDER_CAPABILITY_ADDRESS_SPACE_LIMIT |  \
     EVO_PROJECT_PROVIDER_CAPABILITY_PROCESS_LIMIT |        \
     EVO_PROJECT_PROVIDER_CAPABILITY_STORAGE_LIMIT |        \
     EVO_PROJECT_PROVIDER_CAPABILITY_OUTPUT_LIMIT |         \
     EVO_PROJECT_PROVIDER_CAPABILITY_WALL_TIMEOUT |         \
     EVO_PROJECT_PROVIDER_CAPABILITY_FILESYSTEM_ISOLATION | \
     EVO_PROJECT_PROVIDER_CAPABILITY_NETWORK_ISOLATION |    \
     EVO_PROJECT_PROVIDER_CAPABILITY_DESCENDANT_CLEANUP |   \
     EVO_PROJECT_PROVIDER_CAPABILITY_MEASUREMENT)

static const evo_project_provider_record_t evo_project_providers[] = {
    {EVO_PROJECT_PROVIDER_REGISTRY_SCHEMA_VERSION,
     EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID,
     1U,
     EVO_PROJECT_PROVIDER_ANALYSIS,
     "linux",
     "clang,bwrap",
     EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST |
         EVO_PROJECT_PROVIDER_CAPABILITY_COMPILATION_DATABASE |
         EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV |
         EVO_PROJECT_PROVIDER_CAPABILITY_FILESYSTEM_ISOLATION |
         EVO_PROJECT_PROVIDER_CAPABILITY_NETWORK_ISOLATION |
         EVO_PROJECT_PROVIDER_CAPABILITY_DESCENDANT_CLEANUP},
    {EVO_PROJECT_PROVIDER_REGISTRY_SCHEMA_VERSION,
     EVO_PROJECT_PROVIDER_CLANG_AST_ID,
     1U,
     EVO_PROJECT_PROVIDER_TRANSFORMATION_AST,
     "linux",
     "clang,bwrap",
     EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST |
         EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV |
         EVO_PROJECT_PROVIDER_CAPABILITY_FILESYSTEM_ISOLATION |
         EVO_PROJECT_PROVIDER_CAPABILITY_NETWORK_ISOLATION |
         EVO_PROJECT_PROVIDER_CAPABILITY_DESCENDANT_CLEANUP},
    {EVO_PROJECT_PROVIDER_REGISTRY_SCHEMA_VERSION,
     EVO_PROJECT_PROVIDER_LINUX_BWRAP_ID,
     1U,
     EVO_PROJECT_PROVIDER_EXECUTION,
     "linux",
     "bwrap",
     EVO_PROJECT_PROVIDER_EXEC_CAPABILITIES},
    {EVO_PROJECT_PROVIDER_REGISTRY_SCHEMA_VERSION,
     EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID,
     1U,
     EVO_PROJECT_PROVIDER_EVALUATION,
     "linux",
     "bwrap,clang",
     EVO_PROJECT_PROVIDER_EXEC_CAPABILITIES |
         EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST |
         EVO_PROJECT_PROVIDER_CAPABILITY_COMPILATION_DATABASE |
         EVO_PROJECT_PROVIDER_CAPABILITY_ASYNC_START_POLL_CANCEL_JOIN}};

#if defined(__linux__)
static bool evo_project_provider_program_available(const char *program)
{
    const char *path;
    const char *cursor;

    if (program == NULL || program[0] == '\0' || strchr(program, '/') != NULL) {
        return program != NULL && access(program, X_OK) == 0;
    }
    path = getenv("PATH");
    if (path == NULL || path[0] == '\0') {
        return false;
    }
    cursor = path;
    while (true) {
        const char *end = strchr(cursor, ':');
        const size_t directory_length =
            end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        const size_t program_length = strlen(program);
        char candidate[4096];
        size_t position = 0U;
        size_t index;

        if (directory_length == 0U) {
            if (program_length + 2U <= sizeof(candidate)) {
                candidate[position++] = '.';
                candidate[position++] = '/';
            } else {
                return false;
            }
        } else if (directory_length + 1U < sizeof(candidate)) {
            for (index = 0U; index < directory_length; index += 1U) {
                candidate[position + index] = cursor[index];
            }
            position += directory_length;
            candidate[position++] = '/';
        }
        if (position > 0U && program_length < sizeof(candidate) - position) {
            for (index = 0U; index < program_length; index += 1U) {
                candidate[position + index] = program[index];
            }
            position += program_length;
            candidate[position] = '\0';
            if (access(candidate, X_OK) == 0) {
                return true;
            }
        }
        if (end == NULL) {
            break;
        }
        cursor = end + 1;
    }
    return false;
}
#endif

size_t evo_project_provider_registry_count(void)
{
    return sizeof(evo_project_providers) / sizeof(evo_project_providers[0]);
}

const evo_project_provider_record_t *evo_project_provider_registry_at(
    size_t index)
{
    if (index >= evo_project_provider_registry_count()) {
        return NULL;
    }
    return &evo_project_providers[index];
}

const evo_project_provider_record_t *evo_project_provider_find(
    const char *identity)
{
    size_t index;

    if (identity == NULL) {
        return NULL;
    }
    for (index = 0U; index < evo_project_provider_registry_count(); index += 1U) {
        if (strcmp(evo_project_providers[index].identity, identity) == 0) {
            return &evo_project_providers[index];
        }
    }
    return NULL;
}

bool evo_project_provider_available(
    const evo_project_provider_record_t *provider)
{
    if (provider == NULL ||
        provider->schema_version != EVO_PROJECT_PROVIDER_REGISTRY_SCHEMA_VERSION) {
        return false;
    }
#if !defined(__linux__)
    return false;
#else
    const bool sandbox_available =
        evo_project_provider_program_available("bwrap") &&
        evo_project_provider_probe_bwrap();

    switch (provider->kind) {
    case EVO_PROJECT_PROVIDER_ANALYSIS:
    case EVO_PROJECT_PROVIDER_TRANSFORMATION_AST:
        return sandbox_available && evo_project_provider_program_available("clang");
    case EVO_PROJECT_PROVIDER_EXECUTION:
        return sandbox_available;
    case EVO_PROJECT_PROVIDER_EVALUATION:
        return sandbox_available && evo_project_provider_program_available("clang");
    default:
        return false;
    }
#endif
}

const char *evo_project_provider_kind_name(evo_project_provider_kind_t kind)
{
    switch (kind) {
    case EVO_PROJECT_PROVIDER_ANALYSIS:
        return "analysis";
    case EVO_PROJECT_PROVIDER_TRANSFORMATION_AST:
        return "transformation-ast";
    case EVO_PROJECT_PROVIDER_EXECUTION:
        return "execution";
    case EVO_PROJECT_PROVIDER_EVALUATION:
        return "evaluation";
    default:
        return "unknown";
    }
}
