#include "internal/population_storage.h"

#include <assert.h>

enum {
    TEST_POPULATION_SIZE = 3,
    TEST_GENOME_SIZE = 4,
    TEST_STORAGE_BYTES = TEST_POPULATION_SIZE * TEST_GENOME_SIZE
};

typedef struct parent_snapshot {
    evo_population_t metadata;
    unsigned char genomes[TEST_STORAGE_BYTES];
    evo_candidate_evaluation_t evaluations[TEST_POPULATION_SIZE];
} parent_snapshot_t;

static evo_fitness_t test_evaluator(const void *genome, void *context)
{
    const unsigned char *bytes = genome;

    (void)context;
    return (evo_fitness_t){
        .correctness = (double)bytes[0],
        .total = (double)bytes[0],
    };
}

static bool test_validator(const void *genome, void *context)
{
    const bool *reject_all = context;

    (void)genome;
    return reject_all == NULL || !*reject_all;
}

static evo_problem_t test_problem(void)
{
    return (evo_problem_t){
        .genome_size = TEST_GENOME_SIZE,
        .evaluate = test_evaluator,
        .is_valid = test_validator,
    };
}

static evo_config_t test_config(void)
{
    return (evo_config_t){
        .population_size = TEST_POPULATION_SIZE,
        .random_seed = 42,
        .max_genome_bytes = TEST_GENOME_SIZE,
        .max_population_bytes = TEST_STORAGE_BYTES,
        .max_evaluation_bytes =
            TEST_POPULATION_SIZE * sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes = TEST_STORAGE_BYTES,
        .max_diversity_work = SIZE_MAX,
    };
}

static void assert_fitness_equal(const evo_fitness_t *left,
                                 const evo_fitness_t *right)
{
    assert(left->correctness == right->correctness);
    assert(left->performance == right->performance);
    assert(left->memory_use == right->memory_use);
    assert(left->reliability == right->reliability);
    assert(left->maintainability == right->maintainability);
    assert(left->constraint_penalty == right->constraint_penalty);
    assert(left->total == right->total);
}

static void assert_population_empty(const evo_population_t *population)
{
    assert(population->genomes == NULL);
    assert(population->evaluations == NULL);
    assert(population->population_size == 0);
    assert(population->genome_size == 0);
    assert(population->storage_bytes == 0);
    assert(population->evaluation_bytes == 0);
    assert(population->valid_count == 0);
    assert(population->best_index == 0);
    assert(population->produced_count == 0);
    assert(population->elite_count == 0);
    assert(population->elite_source_valid_count == 0);
    assert(population->initialization_seed == 0);
    assert(population->source_generation == 0);
    assert(population->rng_algorithm_version == 0);
    assert(population->operator_seed_schedule_version == 0);
    assert(population->selection_policy_version == 0);
    assert(population->selection_policy == EVO_SELECTION_TOURNAMENT);
    assert(population->odd_child_policy_version == 0);
    assert(population->elite_policy_version == 0);
    assert(population->singleton_child_policy_version == 0);
    assert(population->fitness_comparison_policy_version == 0);
    assert(!population->initialized);
    assert(!population->has_best);
    assert(!population->evaluated);
    assert(!population->elite_count_explicit);
}

static void assert_child_storage(const evo_population_t *children)
{
    assert(children->genomes != NULL);
    assert(children->evaluations == NULL);
    assert(children->population_size == TEST_POPULATION_SIZE);
    assert(children->genome_size == TEST_GENOME_SIZE);
    assert(children->storage_bytes == TEST_STORAGE_BYTES);
    assert(children->evaluation_bytes == 0);
    assert(children->valid_count == 0);
    assert(children->best_index == 0);
    assert(children->produced_count == 0);
    assert(children->elite_count == 0);
    assert(children->elite_source_valid_count == 0);
    assert(children->initialization_seed == 0);
    assert(children->source_generation == 0);
    assert(children->rng_algorithm_version == 0);
    assert(children->operator_seed_schedule_version == 0);
    assert(children->selection_policy_version == 0);
    assert(children->selection_policy == EVO_SELECTION_TOURNAMENT);
    assert(children->odd_child_policy_version == 0);
    assert(children->elite_policy_version == 0);
    assert(children->singleton_child_policy_version == 0);
    assert(children->fitness_comparison_policy_version == 0);
    assert(!children->initialized);
    assert(!children->has_best);
    assert(!children->evaluated);
    assert(!children->elite_count_explicit);
}

static void create_completed_parent(const evo_problem_t *problem,
                                    const evo_config_t *config,
                                    void *context,
                                    evo_population_t *parents)
{
    assert(evo_population_create(problem, config, parents) == EVO_SUCCESS);
    assert(evo_population_initialize(problem, config, context, parents) ==
           EVO_SUCCESS);
    assert(evo_population_evaluate(problem, config, context, parents) ==
           EVO_SUCCESS);
}

static void snapshot_parent(const evo_population_t *parents,
                            parent_snapshot_t *snapshot)
{
    snapshot->metadata = *parents;
    for (size_t index = 0; index < TEST_STORAGE_BYTES; ++index) {
        snapshot->genomes[index] = parents->genomes[index];
    }
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        snapshot->evaluations[index] = parents->evaluations[index];
    }
}

static void assert_parent_unchanged(const evo_population_t *parents,
                                    const parent_snapshot_t *snapshot)
{
    const evo_population_t *before = &snapshot->metadata;

    assert(parents->genomes == before->genomes);
    assert(parents->evaluations == before->evaluations);
    assert(parents->population_size == before->population_size);
    assert(parents->genome_size == before->genome_size);
    assert(parents->storage_bytes == before->storage_bytes);
    assert(parents->evaluation_bytes == before->evaluation_bytes);
    assert(parents->valid_count == before->valid_count);
    assert(parents->best_index == before->best_index);
    assert(parents->produced_count == before->produced_count);
    assert(parents->elite_count == before->elite_count);
    assert(parents->elite_source_valid_count ==
           before->elite_source_valid_count);
    assert(parents->initialization_seed == before->initialization_seed);
    assert(parents->source_generation == before->source_generation);
    assert(parents->rng_algorithm_version == before->rng_algorithm_version);
    assert(parents->operator_seed_schedule_version ==
           before->operator_seed_schedule_version);
    assert(parents->selection_policy_version ==
           before->selection_policy_version);
    assert(parents->selection_policy == before->selection_policy);
    assert(parents->odd_child_policy_version ==
           before->odd_child_policy_version);
    assert(parents->elite_policy_version ==
           before->elite_policy_version);
    assert(parents->singleton_child_policy_version ==
           before->singleton_child_policy_version);
    assert(parents->fitness_comparison_policy_version ==
           before->fitness_comparison_policy_version);
    assert(parents->initialized == before->initialized);
    assert(parents->has_best == before->has_best);
    assert(parents->evaluated == before->evaluated);
    assert(parents->elite_count_explicit ==
           before->elite_count_explicit);

    for (size_t index = 0; index < TEST_STORAGE_BYTES; ++index) {
        assert(parents->genomes[index] == snapshot->genomes[index]);
    }
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        assert(parents->evaluations[index].valid ==
               snapshot->evaluations[index].valid);
        assert(parents->evaluations[index].evaluated ==
               snapshot->evaluations[index].evaluated);
        assert_fitness_equal(&parents->evaluations[index].fitness,
                             &snapshot->evaluations[index].fitness);
    }
}

static void test_invalid_and_incomplete_inputs(void)
{
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config();
    evo_population_t parents = {0};
    evo_population_t children = {0};
    parent_snapshot_t snapshot = {0};

    assert(evo_child_population_create(
               &problem, &config, &parents, NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_population_create(
               NULL, &config, &parents, &children) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_population_empty(&children);
    assert(evo_child_population_create(
               &problem, NULL, &parents, &children) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_population_empty(&children);
    assert(evo_child_population_create(
               &problem, &config, NULL, &children) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_population_empty(&children);
    assert(evo_child_population_create(
               &problem, &config, &parents, &children) ==
           EVO_ERROR_STATE);
    assert_population_empty(&children);

    create_completed_parent(&problem, &config, NULL, &parents);
    snapshot_parent(&parents, &snapshot);

    assert(evo_child_population_create(
               &problem, &config, &parents, &parents) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_parent_unchanged(&parents, &snapshot);

    problem.genome_size = TEST_GENOME_SIZE + 1;
    assert(evo_child_population_create(
               &problem, &config, &parents, &children) ==
           EVO_ERROR_STATE);
    assert_population_empty(&children);
    problem.genome_size = TEST_GENOME_SIZE;

    config.population_size = TEST_POPULATION_SIZE + 1;
    assert(evo_child_population_create(
               &problem, &config, &parents, &children) ==
           EVO_ERROR_STATE);
    assert_population_empty(&children);
    config.population_size = TEST_POPULATION_SIZE;

    --parents.storage_bytes;
    assert(evo_child_population_create(
               &problem, &config, &parents, &children) ==
           EVO_ERROR_STATE);
    assert_population_empty(&children);
    ++parents.storage_bytes;

    assert(!evo_population_validate_completed(&config, &parents, NULL));
    assert_parent_unchanged(&parents, &snapshot);
    evo_population_destroy(&parents);
}

static void test_child_budget_rejections_preserve_parent(void)
{
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config();
    evo_population_t parents = {0};
    evo_population_t children = {0};
    parent_snapshot_t snapshot = {0};

    create_completed_parent(&problem, &config, NULL, &parents);
    snapshot_parent(&parents, &snapshot);

    config.max_child_population_bytes = 0;
    assert(evo_child_population_create(
               &problem, &config, &parents, &children) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_population_empty(&children);
    assert_parent_unchanged(&parents, &snapshot);

    config.max_child_population_bytes = TEST_STORAGE_BYTES - 1;
    assert(evo_child_population_create(
               &problem, &config, &parents, &children) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_population_empty(&children);
    assert_parent_unchanged(&parents, &snapshot);

    evo_population_destroy(&parents);
}

static void test_independent_child_ownership_and_active_rejection(void)
{
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config();
    evo_population_t parents = {0};
    evo_population_t children = {0};
    parent_snapshot_t parent_before = {0};

    create_completed_parent(&problem, &config, NULL, &parents);
    snapshot_parent(&parents, &parent_before);

    assert(evo_child_population_create(
               &problem, &config, &parents, &children) == EVO_SUCCESS);
    assert_child_storage(&children);
    assert(children.genomes != parents.genomes);

    for (size_t index = 0; index < TEST_STORAGE_BYTES; ++index) {
        assert(children.genomes[index] == 0);
        children.genomes[index] = (unsigned char)(index + 1);
    }
    assert_parent_unchanged(&parents, &parent_before);

    const evo_population_t active_children = children;
    assert(evo_child_population_create(
               &problem, &config, &parents, &children) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(children.genomes == active_children.genomes);
    assert(children.population_size == active_children.population_size);
    assert(children.genome_size == active_children.genome_size);
    assert(children.storage_bytes == active_children.storage_bytes);
    for (size_t index = 0; index < TEST_STORAGE_BYTES; ++index) {
        assert(children.genomes[index] == (unsigned char)(index + 1));
    }

    evo_population_destroy(&parents);
    assert_population_empty(&parents);
    assert_child_storage(&children);
    for (size_t index = 0; index < TEST_STORAGE_BYTES; ++index) {
        assert(children.genomes[index] == (unsigned char)(index + 1));
    }

    evo_population_destroy(&children);
    assert_population_empty(&children);
    evo_population_destroy(&children);
    assert_population_empty(&children);
}

static void test_all_invalid_parent_still_owns_child_storage(void)
{
    bool reject_all = true;
    evo_problem_t problem = test_problem();
    evo_config_t config = test_config();
    evo_population_t parents = {0};
    evo_population_t children = {0};

    create_completed_parent(&problem, &config, &reject_all, &parents);
    assert(parents.valid_count == 0);
    assert(!parents.has_best);

    assert(evo_child_population_create(
               &problem, &config, &parents, &children) == EVO_SUCCESS);
    assert_child_storage(&children);

    evo_population_destroy(&children);
    assert_population_empty(&children);
    assert(parents.genomes != NULL);
    assert(parents.evaluations != NULL);
    assert(parents.initialized);
    assert(parents.evaluated);
    evo_population_destroy(&parents);
}

int main(void)
{
    test_invalid_and_incomplete_inputs();
    test_child_budget_rejections_preserve_parent();
    test_independent_child_ownership_and_active_rejection();
    test_all_invalid_parent_still_owns_child_storage();
    return 0;
}
