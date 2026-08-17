#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_PROVIDER_REGISTRY_SCHEMA_VERSION 1U
#define EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID \
    "catalyst.evo.provider.clang-analysis.v1"
#define EVO_PROJECT_PROVIDER_CLANG_AST_ID \
    "catalyst.evo.provider.clang-ast.v1"
#define EVO_PROJECT_PROVIDER_LINUX_BWRAP_ID \
    "catalyst.evo.provider.linux-bwrap.v1"
#define EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID \
    "catalyst.evo.provider.local-evaluation.v1"

typedef enum evo_project_provider_kind {
    EVO_PROJECT_PROVIDER_ANALYSIS = 1,
    EVO_PROJECT_PROVIDER_TRANSFORMATION_AST = 2,
    EVO_PROJECT_PROVIDER_EXECUTION = 3,
    EVO_PROJECT_PROVIDER_EVALUATION = 4
} evo_project_provider_kind_t;

typedef enum evo_project_provider_capability {
    EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST = UINT64_C(1) << 0,
    EVO_PROJECT_PROVIDER_CAPABILITY_COMPILATION_DATABASE = UINT64_C(1) << 1,
    EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV = UINT64_C(1) << 2,
    EVO_PROJECT_PROVIDER_CAPABILITY_CPU_LIMIT = UINT64_C(1) << 3,
    EVO_PROJECT_PROVIDER_CAPABILITY_ADDRESS_SPACE_LIMIT = UINT64_C(1) << 4,
    EVO_PROJECT_PROVIDER_CAPABILITY_PROCESS_LIMIT = UINT64_C(1) << 5,
    EVO_PROJECT_PROVIDER_CAPABILITY_STORAGE_LIMIT = UINT64_C(1) << 6,
    EVO_PROJECT_PROVIDER_CAPABILITY_OUTPUT_LIMIT = UINT64_C(1) << 7,
    EVO_PROJECT_PROVIDER_CAPABILITY_WALL_TIMEOUT = UINT64_C(1) << 8,
    EVO_PROJECT_PROVIDER_CAPABILITY_FILESYSTEM_ISOLATION = UINT64_C(1) << 9,
    EVO_PROJECT_PROVIDER_CAPABILITY_NETWORK_ISOLATION = UINT64_C(1) << 10,
    EVO_PROJECT_PROVIDER_CAPABILITY_DESCENDANT_CLEANUP = UINT64_C(1) << 11,
    EVO_PROJECT_PROVIDER_CAPABILITY_ASYNC_START_POLL_CANCEL_JOIN =
        UINT64_C(1) << 12,
    EVO_PROJECT_PROVIDER_CAPABILITY_MEASUREMENT = UINT64_C(1) << 13
} evo_project_provider_capability_t;

typedef struct evo_project_provider_record {
    uint32_t schema_version;
    const char *identity;
    uint32_t implementation_version;
    evo_project_provider_kind_t kind;
    const char *platform;
    const char *runtime_requirement;
    uint64_t capabilities;
} evo_project_provider_record_t;

size_t evo_project_provider_registry_count(void);

const evo_project_provider_record_t *evo_project_provider_registry_at(
    size_t index);

const evo_project_provider_record_t *evo_project_provider_find(
    const char *identity);

bool evo_project_provider_available(
    const evo_project_provider_record_t *provider);

const char *evo_project_provider_kind_name(evo_project_provider_kind_t kind);

#endif
