#include "internal/child_pair.h"

#include <assert.h>
#include <math.h>

enum {
    TEST_POPULATION_SIZE = 5,
    TEST_GENOME_SIZE = 4,
    TEST_STORAGE_BYTES = TEST_POPULATION_SIZE * TEST_GENOME_SIZE
};

typedef struct operator_evidence {
    size_t crossover_calls;
    size_t mutation_calls;
} operator_evidence_t;

typedef struct pair_fixture {
    unsigned char genomes[TEST_STORAGE_BYTES];
    evo_candidate_evaluation_t evaluations[TEST_POPULATION_SIZE];
    evo_population_t parents;
    evo_problem_t problem;
    evo_config_t config;
} pair_fixture_t;

typedef struct population_snapshot {
    evo_population_t metadata;
    unsigned char genomes[TEST_STORAGE_BYTES];
    evo_candidate_evaluation_t evaluations[TEST_POPULATION_SIZE];
} population_snapshot_t;

static evo_fitness_t fitness_with_total(double total)
{
    return (evo_fitness_t){
        .correctness = total + 1.0,
        .performance = total + 2.0,
        .memory_use = total + 3.0,
        .reliability = total + 4.0,
        .maintainability = total + 5.0,
        .constraint_penalty = total + 6.0,
        .total = total,
    };
}

static void test_crossover(const void *parent_a,
                           const void *parent_b,
                           void *child_a,
                           void *child_b,
                           void *context)
{
    const unsigned char *left = parent_a;
    const unsigned char *right = parent_b;
    unsigned char *first = child_a;
    unsigned char *second = child_b;
    operator_evidence_t *operator_evidence = context;

    assert(operator_evidence != NULL);
    ++operator_evidence->crossover_calls;

    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        first[index] =
            (unsigned char)(left[index] ^ right[index] ^ 0x3cu);
        second[index] = (unsigned char)((unsigned int)left[index] +
                                        (unsigned int)right[index] +
                                        (unsigned int)index);
    }
}

static void test_mutation(void *genome,
                          double mutation_rate,
                          void *context)
{
    unsigned char *bytes = genome;
    operator_evidence_t *operator_evidence = context;

    assert(operator_evidence != NULL);
    assert(mutation_rate == 0.5);
    ++operator_evidence->mutation_calls;

    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        bytes[index] ^=
            (unsigned char)(0x80u + (unsigned int)index);
    }
}

static void fixture_initialize(pair_fixture_t *fixture)
{
    *fixture = (pair_fixture_t){0};
    fixture->problem.genome_size = TEST_GENOME_SIZE;
    fixture->problem.crossover = test_crossover;
    fixture->problem.mutate = test_mutation;

    fixture->config.population_size = TEST_POPULATION_SIZE;
    fixture->config.tournament_size = 3;
    fixture->config.crossover_rate = 0.75;
    fixture->config.mutation_rate = 0.5;
    fixture->config.random_seed = 42;
    fixture->config.max_genome_bytes = TEST_GENOME_SIZE;
    fixture->config.max_population_bytes = TEST_STORAGE_BYTES;
    fixture->config.max_evaluation_bytes =
        TEST_POPULATION_SIZE * sizeof(evo_candidate_evaluation_t);
    fixture->config.max_child_population_bytes = TEST_STORAGE_BYTES;
    fixture->config.max_diversity_work = SIZE_MAX;

    fixture->parents.genomes = fixture->genomes;
    fixture->parents.evaluations = fixture->evaluations;
    fixture->parents.population_size = TEST_POPULATION_SIZE;
    fixture->parents.genome_size = TEST_GENOME_SIZE;
    fixture->parents.storage_bytes = TEST_STORAGE_BYTES;
    fixture->parents.evaluation_bytes =
        fixture->config.max_evaluation_bytes;
    fixture->parents.valid_count = TEST_POPULATION_SIZE;
    fixture->parents.best_index = TEST_POPULATION_SIZE - 1;
    fixture->parents.initialization_seed = fixture->config.random_seed;
    fixture->parents.rng_algorithm_version = EVO_RNG_ALGORITHM_VERSION;
    fixture->parents.fitness_comparison_policy_version =
        EVO_FITNESS_COMPARISON_POLICY_VERSION;
    fixture->parents.diversity_policy_version =
        EVO_DIVERSITY_POLICY_VERSION;
    fixture->parents.diversity_metric_version =
        EVO_BYTE_DIVERSITY_METRIC_VERSION;
    fixture->parents.diversity_pair_count =
        TEST_POPULATION_SIZE * (TEST_POPULATION_SIZE - 1) / 2;
    fixture->parents.diversity_work_units =
        fixture->parents.diversity_pair_count * TEST_GENOME_SIZE;
    fixture->parents.initialized = true;
    fixture->parents.has_best = true;
    fixture->parents.evaluated = true;

    for (size_t candidate = 0;
         candidate < TEST_POPULATION_SIZE;
         ++candidate) {
        for (size_t byte_index = 0;
             byte_index < TEST_GENOME_SIZE;
             ++byte_index) {
            fixture->genomes[candidate * TEST_GENOME_SIZE + byte_index] =
                (unsigned char)(candidate + byte_index + 1);
        }
        fixture->evaluations[candidate].fitness =
            fitness_with_total((double)candidate);
        fixture->evaluations[candidate].valid = true;
        fixture->evaluations[candidate].evaluated = true;
    }
}

static void snapshot_population(const evo_population_t *population,
                                population_snapshot_t *snapshot)
{
    snapshot->metadata = *population;
    for (size_t index = 0; index < TEST_STORAGE_BYTES; ++index) {
        snapshot->genomes[index] = population->genomes[index];
    }

    if (population->evaluations != NULL) {
        for (size_t index = 0;
             index < TEST_POPULATION_SIZE;
             ++index) {
            snapshot->evaluations[index] = population->evaluations[index];
        }
    }
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

static void assert_population_metadata_equal(
    const evo_population_t *left,
    const evo_population_t *right)
{
    assert(left->genomes == right->genomes);
    assert(left->evaluations == right->evaluations);
    assert(left->population_size == right->population_size);
    assert(left->genome_size == right->genome_size);
    assert(left->storage_bytes == right->storage_bytes);
    assert(left->evaluation_bytes == right->evaluation_bytes);
    assert(left->valid_count == right->valid_count);
    assert(left->best_index == right->best_index);
    assert(left->produced_count == right->produced_count);
    assert(left->elite_count == right->elite_count);
    assert(left->elite_source_valid_count ==
           right->elite_source_valid_count);
    assert(left->initialization_seed == right->initialization_seed);
    assert(left->source_generation == right->source_generation);
    assert(left->rng_algorithm_version == right->rng_algorithm_version);
    assert(left->operator_seed_schedule_version ==
           right->operator_seed_schedule_version);
    assert(left->odd_child_policy_version ==
           right->odd_child_policy_version);
    assert(left->elite_policy_version == right->elite_policy_version);
    assert(left->singleton_child_policy_version ==
           right->singleton_child_policy_version);
    assert(left->fitness_comparison_policy_version ==
           right->fitness_comparison_policy_version);
    assert(left->diversity_policy_version ==
           right->diversity_policy_version);
    assert(left->diversity_metric_version ==
           right->diversity_metric_version);
    assert(left->diversity_pair_count == right->diversity_pair_count);
    assert(left->diversity_work_units == right->diversity_work_units);
    assert(left->diversity == right->diversity);
    assert(left->diversity_uses_domain_distance ==
           right->diversity_uses_domain_distance);
    assert(left->initialized == right->initialized);
    assert(left->has_best == right->has_best);
    assert(left->evaluated == right->evaluated);
    assert(left->elite_count_explicit ==
           right->elite_count_explicit);
}

static void assert_population_unchanged(
    const evo_population_t *population,
    const population_snapshot_t *snapshot)
{
    assert_population_metadata_equal(population, &snapshot->metadata);
    for (size_t index = 0; index < TEST_STORAGE_BYTES; ++index) {
        assert(population->genomes[index] == snapshot->genomes[index]);
    }

    if (population->evaluations != NULL) {
        for (size_t index = 0;
             index < TEST_POPULATION_SIZE;
             ++index) {
            assert(population->evaluations[index].valid ==
                   snapshot->evaluations[index].valid);
            assert(population->evaluations[index].evaluated ==
                   snapshot->evaluations[index].evaluated);
            assert_fitness_equal(
                &population->evaluations[index].fitness,
                &snapshot->evaluations[index].fitness);
        }
    }
}

static evo_child_pair_evidence_t sentinel_evidence(void)
{
    return (evo_child_pair_evidence_t){
        .plan = {
            .parent_a_index = 11,
            .parent_b_index = 13,
            .child_a_index = 17,
            .child_b_index = 19,
            .pair_index = 23,
            .source_generation = 29,
            .seed_schedule_version = 31,
        },
        .produced_count = 37,
        .rng_algorithm_version = 41,
        .complete = true,
    };
}

static void assert_evidence_equal(
    const evo_child_pair_evidence_t *left,
    const evo_child_pair_evidence_t *right)
{
    assert(left->plan.parent_a_index == right->plan.parent_a_index);
    assert(left->plan.parent_b_index == right->plan.parent_b_index);
    assert(left->plan.child_a_index == right->plan.child_a_index);
    assert(left->plan.child_b_index == right->plan.child_b_index);
    assert(left->plan.pair_index == right->plan.pair_index);
    assert(left->plan.source_generation == right->plan.source_generation);
    assert(left->plan.seed_schedule_version ==
           right->plan.seed_schedule_version);
    assert(left->produced_count == right->produced_count);
    assert(left->rng_algorithm_version == right->rng_algorithm_version);
    assert(left->complete == right->complete);
}

static void assert_bytes_equal(const unsigned char *actual,
                               const unsigned char *expected,
                               size_t byte_count)
{
    for (size_t index = 0; index < byte_count; ++index) {
        assert(actual[index] == expected[index]);
    }
}

static void create_children(const pair_fixture_t *fixture,
                            evo_population_t *children)
{
    assert(evo_child_population_create(&fixture->problem,
                                       &fixture->config,
                                       &fixture->parents,
                                       children) == EVO_SUCCESS);
}

static void assert_child_progress(const evo_population_t *children,
                                  size_t produced_count,
                                  uint64_t source_generation)
{
    assert(children->genomes != NULL);
    assert(children->evaluations == NULL);
    assert(children->population_size == TEST_POPULATION_SIZE);
    assert(children->genome_size == TEST_GENOME_SIZE);
    assert(children->storage_bytes == TEST_STORAGE_BYTES);
    assert(children->evaluation_bytes == 0);
    assert(children->valid_count == 0);
    assert(children->best_index == 0);
    assert(children->produced_count == produced_count);
    assert(children->elite_count == 0);
    assert(children->elite_source_valid_count == 0);
    assert(children->initialization_seed == 0);
    assert(children->source_generation == source_generation);
    assert(children->rng_algorithm_version == 0);
    assert(children->operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(children->odd_child_policy_version == 0);
    assert(children->elite_policy_version == 0);
    assert(children->singleton_child_policy_version == 0);
    assert(children->fitness_comparison_policy_version == 0);
    assert(!children->initialized);
    assert(!children->has_best);
    assert(!children->evaluated);
    assert(!children->elite_count_explicit);
}

static void test_invalid_preflight_preserves_every_object(void)
{
    pair_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t parent_before = {0};
    population_snapshot_t child_before = {0};
    operator_evidence_t callbacks = {0};
    evo_child_pair_evidence_t evidence = sentinel_evidence();
    const evo_child_pair_evidence_t evidence_before = evidence;

    fixture_initialize(&fixture);
    create_children(&fixture, &children);
    snapshot_population(&fixture.parents, &parent_before);
    snapshot_population(&children, &child_before);

    assert(evo_child_pair_produce(NULL,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_pair_produce(&fixture.problem,
                                  NULL,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  NULL,
                                  7,
                                  0,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  NULL,
                                  &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);

    fixture.config.crossover_rate = NAN;
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_RESOURCE_LIMIT);
    fixture.config.crossover_rate = 0.75;
    fixture.config.mutation_rate = INFINITY;
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_RESOURCE_LIMIT);
    fixture.config.mutation_rate = 0.5;
    fixture.config.tournament_size = 0;
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_RESOURCE_LIMIT);
    fixture.config.tournament_size = 3;

    fixture.config.max_child_population_bytes = TEST_STORAGE_BYTES - 1;
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    fixture.config.max_child_population_bytes = TEST_STORAGE_BYTES;

    children.evaluation_bytes = 1;
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    children.evaluation_bytes = 0;

    children.odd_child_policy_version = 1;
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    children.odd_child_policy_version = 0;

    children.source_generation = 1;
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    children.source_generation = 0;

    fixture.parents.initialized = false;
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    fixture.parents.initialized = true;

    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &fixture.parents,
                                  &evidence) == EVO_ERROR_STATE);

    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  1,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);

    assert(callbacks.crossover_calls == 0);
    assert(callbacks.mutation_calls == 0);
    assert_evidence_equal(&evidence, &evidence_before);
    assert_population_unchanged(&fixture.parents, &parent_before);
    assert_population_unchanged(&children, &child_before);

    evo_population_destroy(&children);
}

static void test_all_invalid_parent_preserves_child(void)
{
    pair_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t child_before = {0};
    operator_evidence_t callbacks = {0};
    evo_child_pair_evidence_t evidence = sentinel_evidence();
    const evo_child_pair_evidence_t evidence_before = evidence;

    fixture_initialize(&fixture);
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        fixture.evaluations[index] =
            (evo_candidate_evaluation_t){0};
    }
    fixture.parents.valid_count = 0;
    fixture.parents.best_index = 0;
    fixture.parents.has_best = false;
    fixture.parents.diversity_pair_count = 0;
    fixture.parents.diversity_work_units = 0;

    create_children(&fixture, &children);
    snapshot_population(&children, &child_before);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_NO_VALID_CANDIDATE);
    assert(callbacks.crossover_calls == 0);
    assert(callbacks.mutation_calls == 0);
    assert_evidence_equal(&evidence, &evidence_before);
    assert_population_unchanged(&children, &child_before);
    evo_population_destroy(&children);
}

static void test_fixed_pairs_replay_and_odd_tail(void)
{
    static const unsigned char expected_children[TEST_STORAGE_BYTES] = {
        0xbc,
        0xbd,
        0xbe,
        0xbf,
        0x8a,
        0x8c,
        0x92,
        0x90,
        0x85,
        0x87,
        0x85,
        0x8b,
        0x83,
        0x85,
        0x87,
        0x85,
        0x00,
        0x00,
        0x00,
        0x00,
    };
    pair_fixture_t fixture = {0};
    evo_population_t first = {0};
    evo_population_t replay = {0};
    population_snapshot_t parent_before = {0};
    operator_evidence_t first_callbacks = {0};
    operator_evidence_t replay_callbacks = {0};
    evo_child_pair_evidence_t first_pair = {0};
    evo_child_pair_evidence_t first_pair_replay = {0};
    evo_child_pair_evidence_t second_pair = {0};
    evo_child_pair_evidence_t second_pair_replay = {0};
    size_t validated_count = TEST_POPULATION_SIZE;

    fixture_initialize(&fixture);
    create_children(&fixture, &first);
    create_children(&fixture, &replay);
    snapshot_population(&fixture.parents, &parent_before);

    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &first_callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &first,
                                  &first_pair) == EVO_SUCCESS);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &replay_callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &replay,
                                  &first_pair_replay) == EVO_SUCCESS);
    assert_evidence_equal(&first_pair, &first_pair_replay);
    assert(first_pair.plan.parent_a_index == 4);
    assert(first_pair.plan.parent_b_index == 4);
    assert(first_pair.plan.child_a_index == 0);
    assert(first_pair.plan.child_b_index == 1);
    assert(first_pair.plan.pair_index == 0);
    assert(first_pair.plan.source_generation == 7);
    assert(first_pair.plan.seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(first_pair.produced_count == 2);
    assert(first_pair.rng_algorithm_version ==
           EVO_RNG_ALGORITHM_VERSION);
    assert(first_pair.complete);
    assert_child_progress(&first, 2, 7);
    assert_child_progress(&replay, 2, 7);

    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &first_callbacks,
                                  &fixture.parents,
                                  7,
                                  1,
                                  &first,
                                  &second_pair) == EVO_SUCCESS);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &replay_callbacks,
                                  &fixture.parents,
                                  7,
                                  1,
                                  &replay,
                                  &second_pair_replay) == EVO_SUCCESS);
    assert_evidence_equal(&second_pair, &second_pair_replay);
    assert(second_pair.plan.parent_a_index == 4);
    assert(second_pair.plan.parent_b_index == 2);
    assert(second_pair.plan.child_a_index == 2);
    assert(second_pair.plan.child_b_index == 3);
    assert(second_pair.produced_count == 4);
    assert_child_progress(&first, 4, 7);
    assert_child_progress(&replay, 4, 7);
    assert_bytes_equal(first.genomes,
                       expected_children,
                       TEST_STORAGE_BYTES);
    assert_bytes_equal(replay.genomes,
                       expected_children,
                       TEST_STORAGE_BYTES);
    assert(first_callbacks.crossover_calls == 1);
    assert(first_callbacks.mutation_calls == 4);
    assert(replay_callbacks.crossover_calls == 1);
    assert(replay_callbacks.mutation_calls == 4);
    assert_population_unchanged(&fixture.parents, &parent_before);
    assert(!evo_population_validate_completed(
        &fixture.config, &first, &validated_count));
    assert(validated_count == TEST_POPULATION_SIZE);

    evo_population_destroy(&first);
    evo_population_destroy(&replay);
}

static void test_order_generation_and_progress_rejections(void)
{
    pair_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t child_before = {0};
    operator_evidence_t callbacks = {0};
    evo_child_pair_evidence_t first_pair = {0};
    evo_child_pair_evidence_t output = sentinel_evidence();
    const evo_child_pair_evidence_t output_before = output;

    fixture_initialize(&fixture);
    create_children(&fixture, &children);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &first_pair) == EVO_SUCCESS);
    snapshot_population(&children, &child_before);

    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &output) == EVO_ERROR_STATE);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  8,
                                  1,
                                  &children,
                                  &output) == EVO_ERROR_STATE);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  2,
                                  &children,
                                  &output) == EVO_ERROR_STATE);
    assert_evidence_equal(&output, &output_before);
    assert_population_unchanged(&children, &child_before);

    assert(evo_population_initialize(&fixture.problem,
                                     &fixture.config,
                                     &callbacks,
                                     &children) == EVO_ERROR_STATE);
    assert_population_unchanged(&children, &child_before);
    evo_population_destroy(&children);
}

static void test_source_generation_separates_child_output(void)
{
    pair_fixture_t fixture = {0};
    evo_population_t generation_seven = {0};
    evo_population_t generation_eight = {0};
    operator_evidence_t seven_callbacks = {0};
    operator_evidence_t eight_callbacks = {0};
    evo_child_pair_evidence_t seven = {0};
    evo_child_pair_evidence_t eight = {0};
    bool differs = false;

    fixture_initialize(&fixture);
    create_children(&fixture, &generation_seven);
    create_children(&fixture, &generation_eight);

    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &seven_callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &generation_seven,
                                  &seven) == EVO_SUCCESS);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &eight_callbacks,
                                  &fixture.parents,
                                  8,
                                  0,
                                  &generation_eight,
                                  &eight) == EVO_SUCCESS);

    for (size_t index = 0; index < 2 * TEST_GENOME_SIZE; ++index) {
        if (generation_seven.genomes[index] !=
            generation_eight.genomes[index]) {
            differs = true;
        }
    }
    assert(differs);
    assert(seven.plan.source_generation == 7);
    assert(eight.plan.source_generation == 8);
    assert(generation_seven.source_generation == 7);
    assert(generation_eight.source_generation == 8);

    evo_population_destroy(&generation_seven);
    evo_population_destroy(&generation_eight);
}

static void test_absent_callbacks_clone_selected_parents(void)
{
    static const unsigned char expected_parent[TEST_GENOME_SIZE] = {
        5, 6, 7, 8};
    pair_fixture_t fixture = {0};
    evo_population_t children = {0};
    evo_child_pair_evidence_t evidence = {0};

    fixture_initialize(&fixture);
    fixture.problem.crossover = NULL;
    fixture.problem.mutate = NULL;
    fixture.config.crossover_rate = 1.0;
    fixture.config.mutation_rate = 1.0;
    create_children(&fixture, &children);

    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  NULL,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &evidence) == EVO_SUCCESS);
    assert_bytes_equal(children.genomes,
                       expected_parent,
                       TEST_GENOME_SIZE);
    assert_bytes_equal(children.genomes + TEST_GENOME_SIZE,
                       expected_parent,
                       TEST_GENOME_SIZE);
    assert(evidence.complete);
    evo_population_destroy(&children);
}

int main(void)
{
    test_invalid_preflight_preserves_every_object();
    test_all_invalid_parent_preserves_child();
    test_fixed_pairs_replay_and_odd_tail();
    test_order_generation_and_progress_rejections();
    test_source_generation_separates_child_output();
    test_absent_callbacks_clone_selected_parents();
    return 0;
}
