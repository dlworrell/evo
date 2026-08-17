#include "internal/project_orchestration_checkpoint.h"
#include "internal/run_batch.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_POPULATION_SIZE 4U
#define TEST_GENOME_SIZE 8U
#define TEST_GENERATION_LIMIT 4U
#define TEST_RESTORE_GENERATION 2U
#define TEST_CHECKPOINT_CAPACITY 8192U
#define TEST_PRODUCT_CHECKPOINT_CAPACITY 16384U
#define TEST_SNAPSHOT_COUNT (TEST_GENERATION_LIMIT + 1U)

typedef struct test_checkpoint_log {
    unsigned char snapshots[TEST_SNAPSHOT_COUNT][TEST_CHECKPOINT_CAPACITY];
    size_t sizes[TEST_SNAPSHOT_COUNT];
    size_t count;
} test_checkpoint_log_t;

typedef struct test_provider_context {
    size_t batch_calls;
    size_t candidate_evaluations;
} test_provider_context_t;

static int test_failures = 0;

static void test_check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project orchestration resume failure: %s\n", message);
        test_failures += 1;
    }
}

static evo_fitness_t test_fitness(const void *genome)
{
    const unsigned char *bytes = genome;
    double total = 0.0;
    size_t index;

    for (index = 0U; index < TEST_GENOME_SIZE; index += 1U) {
        total += (double)bytes[index];
    }
    return (evo_fitness_t){
        .correctness = total,
        .performance = total / 2.0,
        .memory_use = total / 4.0,
        .reliability = total / 8.0,
        .maintainability = total / 16.0,
        .constraint_penalty = 0.0,
        .total = total,
    };
}

static evo_fitness_t test_evaluate(const void *genome, void *context)
{
    (void)context;
    return test_fitness(genome);
}

static bool test_valid(const void *genome, void *context)
{
    (void)genome;
    (void)context;
    return true;
}

static evo_status_t test_batch_evaluate(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    uint64_t generation,
    const evo_population_t *population,
    evo_candidate_evaluation_t *evaluations,
    size_t evaluation_count,
    void *batch_context)
{
    test_provider_context_t *provider = batch_context;
    size_t index;

    (void)context;
    (void)generation;
    if (problem == NULL || config == NULL || population == NULL ||
        evaluations == NULL || provider == NULL ||
        evaluation_count != TEST_POPULATION_SIZE ||
        evaluation_count != population->population_size ||
        problem->genome_size != TEST_GENOME_SIZE ||
        config->population_size != TEST_POPULATION_SIZE) {
        return EVO_ERROR_EVALUATION;
    }
    provider->batch_calls += 1U;
    for (index = 0U; index < evaluation_count; index += 1U) {
        const void *genome = evo_population_genome_const(population, index);

        if (genome == NULL) {
            return EVO_ERROR_EVALUATION;
        }
        evaluations[index].valid = true;
        evaluations[index].evaluated = true;
        evaluations[index].fitness = test_fitness(genome);
        provider->candidate_evaluations += 1U;
    }
    return EVO_SUCCESS;
}

static void test_checkpoint_observer(
    const void *checkpoint,
    size_t checkpoint_size,
    const evo_checkpoint_view_t *view,
    void *context)
{
    test_checkpoint_log_t *log = context;
    const unsigned char *bytes = checkpoint;
    const size_t generation = (size_t)view->current_generation;
    size_t index;

    if (log == NULL || checkpoint == NULL || view == NULL ||
        generation >= TEST_SNAPSHOT_COUNT ||
        checkpoint_size > TEST_CHECKPOINT_CAPACITY) {
        test_failures += 1;
        return;
    }
    for (index = 0U; index < checkpoint_size; index += 1U) {
        log->snapshots[generation][index] = bytes[index];
    }
    log->sizes[generation] = checkpoint_size;
    log->count += 1U;
}

static evo_problem_t test_problem(void)
{
    evo_problem_t problem = {0};

    problem.genome_size = TEST_GENOME_SIZE;
    problem.evaluate = test_evaluate;
    problem.is_valid = test_valid;
    problem.checkpoint_problem_identity = UINT64_C(0x4200660042006600);
    problem.evaluation_callback_thread_safety = EVO_EVALUATION_CALLBACK_SERIAL;
    return problem;
}

static evo_config_t test_config(
    test_checkpoint_log_t *checkpoints,
    unsigned char checkpoint_buffer[TEST_CHECKPOINT_CAPACITY])
{
    evo_config_t config = {0};

    config.population_size = TEST_POPULATION_SIZE;
    config.generation_limit = TEST_GENERATION_LIMIT;
    config.tournament_size = 0U;
    config.crossover_rate = 0.75;
    config.mutation_rate = 0.25;
    config.random_seed = UINT64_C(420066);
    config.max_genome_bytes = TEST_GENOME_SIZE;
    config.max_population_bytes = TEST_POPULATION_SIZE * TEST_GENOME_SIZE;
    config.max_evaluation_bytes =
        TEST_POPULATION_SIZE * sizeof(evo_candidate_evaluation_t);
    config.max_child_population_bytes =
        TEST_POPULATION_SIZE * TEST_GENOME_SIZE;
    config.max_diversity_work = SIZE_MAX;
    config.stagnation_enabled = true;
    config.improvement_tolerance = 0.0;
    config.stagnation_patience = 32U;
    config.elite_count_enabled = true;
    config.elite_count = 1U;
    config.selection_policy = EVO_SELECTION_RANK;
    config.rank_base_weight = 1U;
    config.rank_step_weight = 2U;
    config.crossover_operator = EVO_CROSSOVER_BYTE_UNIFORM;
    config.mutation_operator = EVO_MUTATION_BYTE_XOR;
    config.adaptive_mutation_enabled = true;
    config.adaptive_mutation_min_rate = 0.1;
    config.adaptive_mutation_max_rate = 0.9;
    config.adaptive_mutation_step = 0.1;
    config.adaptive_mutation_diversity_threshold = 0.5;
    config.adaptive_mutation_reset_on_improvement = true;
    config.max_checkpoint_bytes = TEST_CHECKPOINT_CAPACITY;
    config.checkpoint_buffer = checkpoint_buffer;
    config.checkpoint_buffer_size = TEST_CHECKPOINT_CAPACITY;
    config.checkpoint_observer = test_checkpoint_observer;
    config.checkpoint_observer_context = checkpoints;
    config.checkpoint_context_identity = UINT64_C(0x4242424266666666);
    config.evaluation_worker_count = 0U;
    return config;
}

static evo_project_orchestration_checkpoint_identity_t test_identity(void)
{
    evo_project_orchestration_checkpoint_identity_t identity = {0};

    identity.baseline_fingerprint = "fnv1a64-v1:resume-baseline";
    identity.analysis_fingerprint = "fnv1a64-v1:resume-analysis";
    identity.catalogue_identity = "resume-catalogue-v1";
    identity.catalogue_version = 1U;
    identity.recipe_schema_version = 1U;
    identity.search_schema_version = 1U;
    identity.mutation_policy_version = 1U;
    identity.crossover_policy_version = 1U;
    identity.repair_policy_version = 1U;
    identity.search_policy_identity = "resume-search-policy-v1";
    identity.evaluation_provider_identity = "resume-provider-v1";
    identity.orchestration_policy_identity = "bounded-orchestration-policy-v1";
    identity.toolchain_identity = "clang-gcc-compatible-profile-v1";
    identity.workload_identity = "resume-workload-v1";
    identity.artifact_schema_identity = "evo-artifacts-v1";
    identity.random_seed = UINT64_C(420066);
    identity.committed_generation = TEST_RESTORE_GENERATION;
    identity.committed_lineage_fingerprint = "fnv1a64-v1:resume-lineage-g2";
    return identity;
}

static evo_project_orchestration_checkpoint_limits_t test_product_limits(void)
{
    evo_project_orchestration_checkpoint_limits_t limits = {0};

    limits.max_string_bytes = 128U;
    limits.max_core_checkpoint_bytes = TEST_CHECKPOINT_CAPACITY;
    limits.max_checkpoint_bytes = TEST_PRODUCT_CHECKPOINT_CAPACITY;
    return limits;
}

static bool test_bytes_equal(
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

static bool test_fitness_equal(
    const evo_fitness_t *left,
    const evo_fitness_t *right)
{
    return left->correctness == right->correctness &&
           left->performance == right->performance &&
           left->memory_use == right->memory_use &&
           left->reliability == right->reliability &&
           left->maintainability == right->maintainability &&
           left->constraint_penalty == right->constraint_penalty &&
           left->total == right->total;
}

static bool test_results_equal(
    const evo_result_t *left,
    const evo_result_t *right)
{
    return left != NULL && right != NULL && left->best_genome != NULL &&
           right->best_genome != NULL &&
           left->best_genome_size == right->best_genome_size &&
           test_bytes_equal(
               left->best_genome, right->best_genome, left->best_genome_size) &&
           test_fitness_equal(&left->best_fitness, &right->best_fitness) &&
           left->generations_completed == right->generations_completed &&
           left->random_seed == right->random_seed &&
           left->termination_reason == right->termination_reason &&
           left->generation_statistics.generation_index ==
               right->generation_statistics.generation_index &&
           left->generation_statistics.best_index ==
               right->generation_statistics.best_index &&
           test_fitness_equal(
               &left->generation_statistics.best_fitness,
               &right->generation_statistics.best_fitness) &&
           test_fitness_equal(
               &left->generation_statistics.fitness_sums,
               &right->generation_statistics.fitness_sums) &&
           left->generation_statistics.diversity ==
               right->generation_statistics.diversity &&
           left->generation_statistics.mutation_rate_effective ==
               right->generation_statistics.mutation_rate_effective;
}

static void test_product_checkpoint_resume(void)
{
    evo_problem_t problem = test_problem();
    test_provider_context_t uninterrupted_provider = {0};
    test_checkpoint_log_t uninterrupted_checkpoints = {0};
    unsigned char uninterrupted_buffer[TEST_CHECKPOINT_CAPACITY] = {0};
    evo_config_t config =
        test_config(&uninterrupted_checkpoints, uninterrupted_buffer);
    const evo_population_batch_evaluator_t uninterrupted_batch = {
        test_batch_evaluate, &uninterrupted_provider};
    const evo_project_orchestration_checkpoint_identity_t identity =
        test_identity();
    const evo_project_orchestration_checkpoint_limits_t product_limits =
        test_product_limits();
    evo_result_t uninterrupted = {0};
    evo_project_orchestration_checkpoint_t product_checkpoint = {0};
    evo_project_orchestration_checkpoint_t validated = {0};
    evo_project_orchestration_checkpoint_identity_t stale = identity;
    test_provider_context_t preflight_provider = {0};
    test_provider_context_t resumed_provider = {0};
    test_checkpoint_log_t resumed_checkpoints = {0};
    unsigned char resumed_buffer[TEST_CHECKPOINT_CAPACITY] = {0};
    evo_config_t resumed_config =
        test_config(&resumed_checkpoints, resumed_buffer);
    const evo_population_batch_evaluator_t resumed_batch = {
        test_batch_evaluate, &resumed_provider};
    evo_result_t resumed = {0};
    evo_project_orchestration_checkpoint_status_t checkpoint_status;

    test_check(
        evo_run_with_batch_evaluator(
            &problem, &config, NULL, &uninterrupted_batch, &uninterrupted) ==
            EVO_SUCCESS,
        "uninterrupted bounded external evaluation succeeds");
    if (uninterrupted.best_genome == NULL) {
        goto finish;
    }
    test_check(
        uninterrupted_checkpoints.count == TEST_SNAPSHOT_COUNT &&
            uninterrupted_checkpoints.sizes[TEST_RESTORE_GENERATION] > 0U &&
            uninterrupted_provider.batch_calls == TEST_SNAPSHOT_COUNT,
        "uninterrupted run records every committed generation boundary");

    checkpoint_status = evo_project_orchestration_checkpoint_create(
        &identity,
        uninterrupted_checkpoints.snapshots[TEST_RESTORE_GENERATION],
        uninterrupted_checkpoints.sizes[TEST_RESTORE_GENERATION],
        &product_limits,
        &product_checkpoint);
    test_check(
        checkpoint_status == EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS,
        "product checkpoint wraps committed core resume state");
    if (checkpoint_status != EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS) {
        goto finish;
    }

    stale = identity;
    stale.toolchain_identity = "stale-toolchain-v2";
    test_check(
        evo_project_orchestration_checkpoint_validate(
            &stale,
            product_checkpoint.serialized,
            product_checkpoint.serialized_size,
            &product_limits,
            &validated) ==
                EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH &&
            validated.private_owner == NULL && preflight_provider.batch_calls == 0U,
        "stale product identity rejects before external candidate execution");

    checkpoint_status = evo_project_orchestration_checkpoint_validate(
        &identity,
        product_checkpoint.serialized,
        product_checkpoint.serialized_size,
        &product_limits,
        &validated);
    test_check(
        checkpoint_status == EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS &&
            validated.identity.committed_generation == TEST_RESTORE_GENERATION,
        "exact product identity admits nested core checkpoint");
    if (checkpoint_status != EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS) {
        goto finish;
    }

    test_check(
        evo_resume_with_batch_evaluator(
            &problem,
            &resumed_config,
            NULL,
            validated.core_checkpoint,
            validated.core_checkpoint_size,
            &resumed_batch,
            &resumed) == EVO_SUCCESS,
        "validated product checkpoint resumes bounded external evaluation");
    test_check(
        test_results_equal(&uninterrupted, &resumed),
        "resumed bounded external evaluation matches uninterrupted execution");
    test_check(
        resumed_provider.batch_calls ==
                TEST_GENERATION_LIMIT - TEST_RESTORE_GENERATION &&
            resumed_provider.candidate_evaluations ==
                (TEST_GENERATION_LIMIT - TEST_RESTORE_GENERATION) *
                    TEST_POPULATION_SIZE &&
            resumed_checkpoints.count ==
                TEST_GENERATION_LIMIT - TEST_RESTORE_GENERATION,
        "resume schedules only post-checkpoint generations");

finish:
    evo_result_destroy(&resumed);
    evo_project_orchestration_checkpoint_destroy(&validated);
    evo_project_orchestration_checkpoint_destroy(&product_checkpoint);
    evo_result_destroy(&uninterrupted);
}

int main(void)
{
    test_product_checkpoint_resume();
    if (test_failures != 0) {
        (void)fprintf(
            stderr, "project orchestration resume failures: %d\n", test_failures);
        return 1;
    }
    (void)printf("project orchestration resume tests: PASS\n");
    return 0;
}
