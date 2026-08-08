#include "catalyst/evo/evo.h"
#include "internal/population_storage.h"
#include "internal/secure_erasure.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

enum {
    TRACKED_ALLOCATION_CAPACITY = 16,
    TEST_GENOME_SIZE = 8,
    TEST_POPULATION_SIZE = 2
};

typedef enum release_expectation {
    EXPECT_ORDINARY_RELEASE = 0,
    EXPECT_SECURE_RELEASE = 1
} release_expectation_t;

typedef struct tracked_allocation {
    void *allocation;
    size_t byte_count;
    size_t erase_calls;
    size_t erase_sequence;
    bool released;
} tracked_allocation_t;

typedef struct evaluation_context {
    size_t evaluation_calls;
} evaluation_context_t;

void *__real_calloc(size_t count, size_t size);
void __real_free(void *allocation);
void __real_evo_secure_erase(void *allocation, size_t byte_count);

static tracked_allocation_t tracked[TRACKED_ALLOCATION_CAPACITY];
static size_t allocation_attempts;
static size_t erasure_calls;
static size_t fail_allocation_attempt;
static size_t lifecycle_sequence;
static size_t release_calls;
static size_t tracked_count;
static release_expectation_t release_expectation;

static tracked_allocation_t *active_record(void *allocation)
{
    for (size_t index = tracked_count; index != 0; --index) {
        tracked_allocation_t *record = &tracked[index - 1];

        if (record->allocation == allocation && !record->released) {
            return record;
        }
    }

    return NULL;
}

static bool bytes_are_zero(const void *allocation, size_t byte_count)
{
    const unsigned char *bytes = allocation;

    for (size_t index = 0; index < byte_count; ++index) {
        if (bytes[index] != 0) {
            return false;
        }
    }

    return true;
}

void *__wrap_calloc(size_t count, size_t size)
{
    void *allocation = NULL;

    ++allocation_attempts;
    if (fail_allocation_attempt != 0 &&
        allocation_attempts == fail_allocation_attempt) {
        return NULL;
    }

    assert(count == 0 || size <= SIZE_MAX / count);
    allocation = __real_calloc(count, size);
    if (allocation != NULL) {
        assert(tracked_count < TRACKED_ALLOCATION_CAPACITY);
        tracked[tracked_count] = (tracked_allocation_t){
            .allocation = allocation,
            .byte_count = count * size,
        };
        ++tracked_count;
    }
    return allocation;
}

void __wrap_evo_secure_erase(void *allocation, size_t byte_count)
{
    tracked_allocation_t *record = active_record(allocation);

    assert(release_expectation == EXPECT_SECURE_RELEASE);
    assert(record != NULL);
    assert(byte_count != 0);
    assert(byte_count == record->byte_count);
    assert(record->erase_calls == 0);
    ++record->erase_calls;
    record->erase_sequence = ++lifecycle_sequence;
    ++erasure_calls;

    __real_evo_secure_erase(allocation, byte_count);
    assert(bytes_are_zero(allocation, byte_count));
}

void __wrap_free(void *allocation)
{
    if (allocation != NULL) {
        tracked_allocation_t *record = active_record(allocation);

        assert(record != NULL);
        if (release_expectation == EXPECT_SECURE_RELEASE) {
            assert(record->erase_calls == 1);
            assert(record->erase_sequence == lifecycle_sequence);
        } else {
            assert(record->erase_calls == 0);
        }
        ++lifecycle_sequence;
        record->released = true;
        ++release_calls;
    }
    __real_free(allocation);
}

static void begin_tracking(release_expectation_t expectation,
                           size_t failure_attempt)
{
    for (size_t index = 0; index < tracked_count; ++index) {
        assert(tracked[index].released);
    }

    allocation_attempts = 0;
    erasure_calls = 0;
    fail_allocation_attempt = failure_attempt;
    lifecycle_sequence = 0;
    release_calls = 0;
    tracked_count = 0;
    release_expectation = expectation;
}

static void assert_tracking_complete(size_t expected_attempts,
                                     size_t expected_allocations,
                                     size_t expected_erasures)
{
    assert(allocation_attempts == expected_attempts);
    assert(tracked_count == expected_allocations);
    assert(release_calls == expected_allocations);
    assert(erasure_calls == expected_erasures);
    for (size_t index = 0; index < tracked_count; ++index) {
        assert(tracked[index].released);
        assert(tracked[index].erase_calls ==
               (expected_erasures == 0 ? 0 : 1));
    }
}

static void initialize_secret_genome(void *genome, void *context)
{
    unsigned char *bytes = genome;

    (void)context;
    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        bytes[index] = (unsigned char)(0xa0U + index);
    }
}

static evo_fitness_t evaluate_secret_genome(const void *genome,
                                            void *context)
{
    const unsigned char *bytes = genome;

    (void)context;
    return (evo_fitness_t){
        .correctness = (double)bytes[0],
        .total = (double)bytes[0],
    };
}

static evo_fitness_t evaluate_non_finite(const void *genome,
                                         void *context)
{
    (void)genome;
    (void)context;
    return (evo_fitness_t){.total = NAN};
}

static evo_fitness_t evaluate_child_non_finite(const void *genome,
                                               void *context)
{
    evaluation_context_t *evaluation_context = context;
    const unsigned char *bytes = genome;

    ++evaluation_context->evaluation_calls;
    if (evaluation_context->evaluation_calls > TEST_POPULATION_SIZE) {
        return (evo_fitness_t){.total = NAN};
    }

    return (evo_fitness_t){
        .correctness = (double)bytes[0],
        .total = (double)bytes[0],
    };
}

static bool reject_all_genomes(const void *genome, void *context)
{
    (void)genome;
    (void)context;
    return false;
}

static double invalid_distance(const void *left,
                               const void *right,
                               size_t genome_size,
                               void *context)
{
    (void)left;
    (void)right;
    (void)genome_size;
    (void)context;
    return NAN;
}

static evo_problem_t make_problem(void)
{
    return (evo_problem_t){
        .genome_size = TEST_GENOME_SIZE,
        .initialize = initialize_secret_genome,
        .evaluate = evaluate_secret_genome,
    };
}

static evo_config_t make_config(bool secure_erasure_enabled,
                                size_t generation_limit)
{
    return (evo_config_t){
        .population_size = TEST_POPULATION_SIZE,
        .generation_limit = generation_limit,
        .tournament_size = 2,
        .crossover_rate = 0.0,
        .mutation_rate = 0.0,
        .random_seed = UINT64_C(0x5028),
        .max_genome_bytes = TEST_GENOME_SIZE,
        .max_population_bytes =
            TEST_POPULATION_SIZE * TEST_GENOME_SIZE,
        .max_evaluation_bytes =
            TEST_POPULATION_SIZE *
            sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes =
            TEST_POPULATION_SIZE * TEST_GENOME_SIZE,
        .max_diversity_work = SIZE_MAX,
        .secure_erasure_enabled = secure_erasure_enabled,
    };
}

static void assert_result_empty(const evo_result_t *result)
{
    assert(result->best_genome == NULL);
    assert(result->best_genome_size == 0);
    assert(result->secure_erasure_policy_version == 0);
    assert(result->secure_erasure_backend ==
           EVO_SECURE_ERASURE_BACKEND_NONE);
    assert(!result->secure_erasure_enabled);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(result->generation_statistics.version == 0);
}

static void assert_live_result_audit(const evo_result_t *result,
                                     bool enabled)
{
    assert(result->best_genome != NULL);
    assert(result->best_genome_size == TEST_GENOME_SIZE);
    assert(result->secure_erasure_policy_version ==
           EVO_SECURE_ERASURE_POLICY_VERSION);
    assert(result->secure_erasure_enabled == enabled);
    assert(result->secure_erasure_backend ==
           (enabled ? evo_secure_erasure_selected_backend()
                    : EVO_SECURE_ERASURE_BACKEND_NONE));
}

static void test_disabled_policy_uses_ordinary_release(void)
{
    const evo_problem_t problem = make_problem();
    const evo_config_t config = make_config(false, 0);
    evo_result_t result = {0};

    begin_tracking(EXPECT_ORDINARY_RELEASE, 0);
    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    assert_live_result_audit(&result, false);
    assert(allocation_attempts == 3);
    assert(release_calls == 2);
    assert(erasure_calls == 0);
    evo_result_destroy(&result);
    assert_result_empty(&result);
    evo_result_destroy(&result);
    assert_tracking_complete(3, 3, 0);
}

static void test_enabled_policy_erases_success_owners(void)
{
    const evo_problem_t problem = make_problem();
    const evo_config_t config = make_config(true, 0);
    evo_result_t result = {0};

    begin_tracking(EXPECT_SECURE_RELEASE, 0);
    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    assert_live_result_audit(&result, true);
    assert(allocation_attempts == 3);
    assert(release_calls == 2);
    assert(erasure_calls == 2);
    evo_result_destroy(&result);
    assert_result_empty(&result);
    evo_result_destroy(&result);
    assert_tracking_complete(3, 3, 3);
}

static void test_population_owner_registry_is_exact(void)
{
    const evo_problem_t problem = make_problem();
    const evo_config_t config = make_config(true, 0);
    evo_population_t population = {0};

    begin_tracking(EXPECT_SECURE_RELEASE, 0);
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_SUCCESS);
    assert(population.storage_bytes ==
           TEST_POPULATION_SIZE * TEST_GENOME_SIZE);
    assert(population.evaluation_bytes == 0);
    assert(population.secure_erasure_policy_version ==
           EVO_SECURE_ERASURE_POLICY_VERSION);
    assert(population.secure_erasure_backend ==
           evo_secure_erasure_selected_backend());
    assert(population.secure_erasure_enabled);
    assert(evo_population_initialize(
               &problem, &config, NULL, &population) == EVO_SUCCESS);
    assert(evo_population_evaluate(
               &problem, &config, NULL, &population) == EVO_SUCCESS);
    assert(population.evaluation_bytes ==
           TEST_POPULATION_SIZE *
               sizeof(evo_candidate_evaluation_t));
    evo_population_destroy(&population);
    assert(population.genomes == NULL);
    assert(population.evaluations == NULL);
    assert(population.storage_bytes == 0);
    assert(population.evaluation_bytes == 0);
    assert(population.secure_erasure_policy_version == 0);
    assert(population.secure_erasure_backend ==
           EVO_SECURE_ERASURE_BACKEND_NONE);
    assert(!population.secure_erasure_enabled);
    assert_tracking_complete(2, 2, 2);
}

static void test_enabled_policy_survives_owner_promotion(void)
{
    const evo_problem_t problem = make_problem();
    const evo_config_t config = make_config(true, 1);
    evo_result_t result = {0};

    begin_tracking(EXPECT_SECURE_RELEASE, 0);
    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    assert_live_result_audit(&result, true);
    assert(result.generations_completed == 1);
    assert(allocation_attempts == 5);
    assert(release_calls == 4);
    assert(erasure_calls == 4);
    evo_result_destroy(&result);
    assert_tracking_complete(5, 5, 5);
}

static void test_every_allocation_failure_erases_prior_owners(void)
{
    const evo_problem_t problem = make_problem();
    const evo_config_t config = make_config(true, 1);

    for (size_t failure_attempt = 1;
         failure_attempt <= 5;
         ++failure_attempt) {
        evo_result_t result = {0};

        begin_tracking(EXPECT_SECURE_RELEASE, failure_attempt);
        assert(evo_run(&problem, &config, NULL, &result) ==
               EVO_ERROR_OUT_OF_MEMORY);
        assert_result_empty(&result);
        assert_tracking_complete(failure_attempt,
                                 failure_attempt - 1,
                                 failure_attempt - 1);
    }
}

static void test_provisional_evaluation_failure_is_erased(void)
{
    evo_problem_t problem = make_problem();
    const evo_config_t config = make_config(true, 0);
    evo_result_t result = {0};

    problem.evaluate = evaluate_non_finite;
    begin_tracking(EXPECT_SECURE_RELEASE, 0);
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_EVALUATION);
    assert_result_empty(&result);
    assert_tracking_complete(2, 2, 2);
}

static void test_all_invalid_transfer_failure_is_erased(void)
{
    evo_problem_t problem = make_problem();
    const evo_config_t config = make_config(true, 0);
    evo_result_t result = {0};

    problem.is_valid = reject_all_genomes;
    begin_tracking(EXPECT_SECURE_RELEASE, 0);
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_NO_VALID_CANDIDATE);
    assert_result_empty(&result);
    assert_tracking_complete(2, 2, 2);
}

static void test_child_evaluation_failure_erases_every_live_owner(void)
{
    evo_problem_t problem = make_problem();
    const evo_config_t config = make_config(true, 1);
    evaluation_context_t context = {0};
    evo_result_t result = {0};

    problem.evaluate = evaluate_child_non_finite;
    begin_tracking(EXPECT_SECURE_RELEASE, 0);
    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_EVALUATION);
    assert(context.evaluation_calls == TEST_POPULATION_SIZE + 1);
    assert_result_empty(&result);
    assert_tracking_complete(5, 5, 5);
}

static void test_attached_evaluation_rollback_is_erased(void)
{
    evo_problem_t problem = make_problem();
    const evo_config_t config = make_config(true, 0);
    evo_result_t result = {0};

    problem.genome_distance = invalid_distance;
    problem.genome_distance_version = UINT32_C(1);
    begin_tracking(EXPECT_SECURE_RELEASE, 0);
    assert(evo_run(&problem, &config, NULL, &result) ==
           EVO_ERROR_EVALUATION);
    assert_result_empty(&result);
    assert_tracking_complete(2, 2, 2);
}

int main(void)
{
    _Static_assert(EVO_SECURE_ERASURE_POLICY_VERSION == UINT32_C(1),
                   "the initial secure-erasure policy must remain stable");
    _Static_assert(EVO_SECURE_ERASURE_BACKEND_NONE == 0,
                   "disabled secure erasure must remain the zero value");

    test_disabled_policy_uses_ordinary_release();
    test_enabled_policy_erases_success_owners();
    test_population_owner_registry_is_exact();
    test_enabled_policy_survives_owner_promotion();
    test_every_allocation_failure_erases_prior_owners();
    test_provisional_evaluation_failure_is_erased();
    test_all_invalid_transfer_failure_is_erased();
    test_child_evaluation_failure_erases_every_live_owner();
    test_attached_evaluation_rollback_is_erased();
    return 0;
}
