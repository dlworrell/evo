#include "internal/project_provider.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project provider test failure: %s\n", message);
        failures += 1;
    }
}

static void check_provider(
    size_t index,
    const char *identity,
    evo_project_provider_kind_t kind,
    uint64_t required_capabilities)
{
    const evo_project_provider_record_t *provider =
        evo_project_provider_registry_at(index);

    check(provider != NULL, "provider registry entry exists");
    if (provider == NULL) {
        return;
    }
    check(
        provider->schema_version == EVO_PROJECT_PROVIDER_REGISTRY_SCHEMA_VERSION,
        "provider registry schema");
    check(strcmp(provider->identity, identity) == 0, "provider identity order");
    check(provider->implementation_version == 1U, "provider version");
    check(provider->kind == kind, "provider kind");
    check(provider->platform != NULL, "provider platform");
    check(provider->runtime_requirement != NULL, "provider requirement");
    check(
        (provider->capabilities & required_capabilities) ==
            required_capabilities,
        "provider capabilities");
    check(evo_project_provider_find(identity) == provider, "provider lookup");
}

int main(void)
{
    size_t left;
    size_t right;

    check(evo_project_provider_registry_count() == 4U, "registry count");
    check_provider(
        0U,
        EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID,
        EVO_PROJECT_PROVIDER_ANALYSIS,
        EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST |
            EVO_PROJECT_PROVIDER_CAPABILITY_COMPILATION_DATABASE |
            EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV);
    check_provider(
        1U,
        EVO_PROJECT_PROVIDER_CLANG_AST_ID,
        EVO_PROJECT_PROVIDER_TRANSFORMATION_AST,
        EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST |
            EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV);
    check_provider(
        2U,
        EVO_PROJECT_PROVIDER_LINUX_BWRAP_ID,
        EVO_PROJECT_PROVIDER_EXECUTION,
        EVO_PROJECT_PROVIDER_CAPABILITY_DIRECT_ARGV |
            EVO_PROJECT_PROVIDER_CAPABILITY_CPU_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_ADDRESS_SPACE_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_PROCESS_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_STORAGE_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_OUTPUT_LIMIT |
            EVO_PROJECT_PROVIDER_CAPABILITY_WALL_TIMEOUT |
            EVO_PROJECT_PROVIDER_CAPABILITY_FILESYSTEM_ISOLATION |
            EVO_PROJECT_PROVIDER_CAPABILITY_NETWORK_ISOLATION |
            EVO_PROJECT_PROVIDER_CAPABILITY_DESCENDANT_CLEANUP |
            EVO_PROJECT_PROVIDER_CAPABILITY_MEASUREMENT);
    check_provider(
        3U,
        EVO_PROJECT_PROVIDER_LOCAL_EVALUATION_ID,
        EVO_PROJECT_PROVIDER_EVALUATION,
        EVO_PROJECT_PROVIDER_CAPABILITY_ASYNC_START_POLL_CANCEL_JOIN |
            EVO_PROJECT_PROVIDER_CAPABILITY_CLANG_AST |
            EVO_PROJECT_PROVIDER_CAPABILITY_MEASUREMENT);

    check(evo_project_provider_registry_at(4U) == NULL, "registry bound");
    check(evo_project_provider_find(NULL) == NULL, "null lookup");
    check(evo_project_provider_find("unknown-provider") == NULL, "unknown lookup");
    check(
        strcmp(evo_project_provider_kind_name(EVO_PROJECT_PROVIDER_ANALYSIS),
               "analysis") == 0,
        "analysis kind name");
    check(
        strcmp(evo_project_provider_kind_name(EVO_PROJECT_PROVIDER_EXECUTION),
               "execution") == 0,
        "execution kind name");

    for (left = 0U; left < evo_project_provider_registry_count(); left += 1U) {
        const evo_project_provider_record_t *left_provider =
            evo_project_provider_registry_at(left);

        for (right = left + 1U;
             right < evo_project_provider_registry_count();
             right += 1U) {
            const evo_project_provider_record_t *right_provider =
                evo_project_provider_registry_at(right);

            check(
                strcmp(left_provider->identity, right_provider->identity) != 0,
                "provider identities unique");
        }
    }

#if !defined(__linux__)
    check(
        !evo_project_provider_available(
            evo_project_provider_find(EVO_PROJECT_PROVIDER_LINUX_BWRAP_ID)),
        "linux provider unavailable off linux");
#endif

    if (failures != 0) {
        (void)fprintf(stderr, "%d project provider tests failed\n", failures);
        return 1;
    }
    (void)printf("project provider registry tests passed\n");
    return 0;
}
