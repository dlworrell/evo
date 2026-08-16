#include "internal/project_orchestration_checkpoint.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_failures = 0;

static const unsigned char test_core_checkpoint[] = {
    0x45U, 0x56U, 0x4fU, 0x43U, 0x4fU, 0x52U, 0x45U, 0x33U,
    0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U,
    0x10U, 0x20U, 0x30U, 0x40U, 0x50U, 0x60U, 0x70U, 0x80U,
    0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U};

static void test_check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(
            stderr,
            "project orchestration checkpoint test failure: %s\n",
            message);
        test_failures += 1;
    }
}

static evo_project_orchestration_checkpoint_limits_t test_limits(void)
{
    evo_project_orchestration_checkpoint_limits_t limits = {0};

    limits.max_string_bytes = 128U;
    limits.max_core_checkpoint_bytes = 4096U;
    limits.max_checkpoint_bytes = 16384U;
    return limits;
}

static evo_project_orchestration_checkpoint_identity_t test_identity(void)
{
    evo_project_orchestration_checkpoint_identity_t identity = {0};

    identity.baseline_fingerprint = "fnv1a64-v1:baseline";
    identity.analysis_fingerprint = "fnv1a64-v1:analysis";
    identity.catalogue_identity = "c17-transformations-v3";
    identity.catalogue_version = 3U;
    identity.recipe_schema_version = 1U;
    identity.search_schema_version = 1U;
    identity.mutation_policy_version = 1U;
    identity.crossover_policy_version = 1U;
    identity.repair_policy_version = 1U;
    identity.search_policy_identity = "structured-search-policy-v1";
    identity.evaluation_provider_identity = "project-evaluation-provider-v1";
    identity.orchestration_policy_identity = "bounded-orchestration-policy-v1";
    identity.toolchain_identity = "clang-18-cmake-release";
    identity.workload_identity = "fixture-workloads-v1";
    identity.artifact_schema_identity = "evo-artifacts-v1";
    identity.random_seed = UINT64_C(0x1122334455667788);
    identity.committed_generation = UINT64_C(7);
    identity.committed_lineage_fingerprint = "fnv1a64-v1:lineage";
    return identity;
}

static bool bytes_equal(
    const unsigned char *left,
    const unsigned char *right,
    size_t size)
{
    size_t index;

    for (index = 0U; index < size; index += 1U) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static void copy_bytes(
    unsigned char *destination,
    const unsigned char *source,
    size_t size)
{
    size_t index;

    for (index = 0U; index < size; index += 1U) {
        destination[index] = source[index];
    }
}

static void test_create_validate_and_replay(void)
{
    const evo_project_orchestration_checkpoint_limits_t limits = test_limits();
    const evo_project_orchestration_checkpoint_identity_t identity =
        test_identity();
    evo_project_orchestration_checkpoint_t first = {0};
    evo_project_orchestration_checkpoint_t replay = {0};
    evo_project_orchestration_checkpoint_t validated = {0};
    evo_project_orchestration_checkpoint_status_t status;

    status = evo_project_orchestration_checkpoint_create(
        &identity,
        test_core_checkpoint,
        sizeof(test_core_checkpoint),
        &limits,
        &first);
    test_check(
        status == EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS,
        "product checkpoint creation succeeds");
    if (status != EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS) {
        return;
    }
    test_check(
        first.format_version ==
                EVO_PROJECT_ORCHESTRATION_CHECKPOINT_FORMAT_VERSION &&
            first.integrity_algorithm ==
                EVO_PROJECT_ORCHESTRATION_CHECKPOINT_INTEGRITY_FNV1A64 &&
            first.core_checkpoint_size == sizeof(test_core_checkpoint) &&
            bytes_equal(
                first.core_checkpoint,
                test_core_checkpoint,
                sizeof(test_core_checkpoint)) &&
            strcmp(
                first.identity.toolchain_identity,
                identity.toolchain_identity) == 0 &&
            first.identity.committed_generation ==
                identity.committed_generation,
        "checkpoint retains exact product and nested core authority");

    status = evo_project_orchestration_checkpoint_validate(
        &identity,
        first.serialized,
        first.serialized_size,
        &limits,
        &validated);
    test_check(
        status == EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS &&
            validated.serialized_size == first.serialized_size &&
            strcmp(
                validated.checkpoint_fingerprint,
                first.checkpoint_fingerprint) == 0 &&
            bytes_equal(
                validated.core_checkpoint,
                test_core_checkpoint,
                sizeof(test_core_checkpoint)),
        "exact checkpoint validates before resume");

    status = evo_project_orchestration_checkpoint_create(
        &identity,
        test_core_checkpoint,
        sizeof(test_core_checkpoint),
        &limits,
        &replay);
    test_check(
        status == EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS &&
            replay.serialized_size == first.serialized_size &&
            bytes_equal(
                replay.serialized,
                first.serialized,
                first.serialized_size),
        "same committed boundary replays byte-identical checkpoint evidence");

    test_check(
        evo_project_orchestration_checkpoint_validate(
            &identity,
            first.serialized,
            first.serialized_size,
            &limits,
            &validated) ==
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_RESULT_ACTIVE,
        "active decoded checkpoint rejects overwrite");

    evo_project_orchestration_checkpoint_destroy(&replay);
    evo_project_orchestration_checkpoint_destroy(&validated);
    evo_project_orchestration_checkpoint_destroy(&first);
}

static void test_stale_identity_rejected(void)
{
    const evo_project_orchestration_checkpoint_limits_t limits = test_limits();
    const evo_project_orchestration_checkpoint_identity_t identity =
        test_identity();
    evo_project_orchestration_checkpoint_identity_t stale = identity;
    evo_project_orchestration_checkpoint_t checkpoint = {0};
    evo_project_orchestration_checkpoint_t validated = {0};
    evo_project_orchestration_checkpoint_status_t status;

    status = evo_project_orchestration_checkpoint_create(
        &identity,
        test_core_checkpoint,
        sizeof(test_core_checkpoint),
        &limits,
        &checkpoint);
    test_check(
        status == EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS,
        "stale-identity fixture checkpoint builds");
    if (status != EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS) {
        return;
    }

    stale.toolchain_identity = "gcc-15-autotools-release";
    test_check(
        evo_project_orchestration_checkpoint_validate(
            &stale,
            checkpoint.serialized,
            checkpoint.serialized_size,
            &limits,
            &validated) ==
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH &&
            validated.private_owner == NULL,
        "stale toolchain rejects before resume state is published");

    stale = identity;
    stale.baseline_fingerprint = "fnv1a64-v1:stale-baseline";
    test_check(
        evo_project_orchestration_checkpoint_validate(
            &stale,
            checkpoint.serialized,
            checkpoint.serialized_size,
            &limits,
            &validated) ==
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH,
        "stale baseline rejects");

    stale = identity;
    stale.catalogue_version += 1U;
    test_check(
        evo_project_orchestration_checkpoint_validate(
            &stale,
            checkpoint.serialized,
            checkpoint.serialized_size,
            &limits,
            &validated) ==
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH,
        "stale catalogue rejects");

    stale = identity;
    stale.workload_identity = "fixture-workloads-v2";
    test_check(
        evo_project_orchestration_checkpoint_validate(
            &stale,
            checkpoint.serialized,
            checkpoint.serialized_size,
            &limits,
            &validated) ==
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH,
        "stale workload rejects");

    stale = identity;
    stale.orchestration_policy_identity = "bounded-orchestration-policy-v2";
    test_check(
        evo_project_orchestration_checkpoint_validate(
            &stale,
            checkpoint.serialized,
            checkpoint.serialized_size,
            &limits,
            &validated) ==
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH,
        "stale orchestration policy rejects");

    evo_project_orchestration_checkpoint_destroy(&checkpoint);
}

static void test_corruption_and_format_rejected(void)
{
    const evo_project_orchestration_checkpoint_limits_t limits = test_limits();
    const evo_project_orchestration_checkpoint_identity_t identity =
        test_identity();
    evo_project_orchestration_checkpoint_t checkpoint = {0};
    evo_project_orchestration_checkpoint_t validated = {0};
    unsigned char mutated[2048];
    evo_project_orchestration_checkpoint_status_t status;

    status = evo_project_orchestration_checkpoint_create(
        &identity,
        test_core_checkpoint,
        sizeof(test_core_checkpoint),
        &limits,
        &checkpoint);
    test_check(
        status == EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS &&
            checkpoint.serialized_size <= sizeof(mutated),
        "corruption fixture checkpoint fits local buffer");
    if (status != EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS ||
        checkpoint.serialized_size > sizeof(mutated)) {
        evo_project_orchestration_checkpoint_destroy(&checkpoint);
        return;
    }

    copy_bytes(mutated, checkpoint.serialized, checkpoint.serialized_size);
    mutated[checkpoint.serialized_size - 1U] ^= 0x01U;
    test_check(
        evo_project_orchestration_checkpoint_validate(
            &identity,
            mutated,
            checkpoint.serialized_size,
            &limits,
            &validated) ==
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_INTEGRITY,
        "nested checkpoint corruption fails integrity before resume");

    copy_bytes(mutated, checkpoint.serialized, checkpoint.serialized_size);
    mutated[8] = 2U;
    test_check(
        evo_project_orchestration_checkpoint_validate(
            &identity,
            mutated,
            checkpoint.serialized_size,
            &limits,
            &validated) ==
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_FORMAT,
        "unsupported product checkpoint version rejects");

    test_check(
        evo_project_orchestration_checkpoint_validate(
            &identity,
            checkpoint.serialized,
            checkpoint.serialized_size - 1U,
            &limits,
            &validated) ==
            EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_FORMAT,
        "truncated product checkpoint rejects");

    evo_project_orchestration_checkpoint_destroy(&checkpoint);
}

int main(void)
{
    test_create_validate_and_replay();
    test_stale_identity_rejected();
    test_corruption_and_format_rejected();
    if (test_failures != 0) {
        (void)fprintf(
            stderr,
            "project orchestration checkpoint failures: %d\n",
            test_failures);
        return 1;
    }
    (void)printf("project orchestration checkpoint tests: PASS\n");
    return 0;
}
