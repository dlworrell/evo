#include "internal/child_tail.h"

#include <assert.h>

enum {
    TEST_MAX_POPULATION_SIZE = 5,
    TEST_GENOME_SIZE = 4,
    TEST_MAX_STORAGE_BYTES =
        TEST_MAX_POPULATION_SIZE * TEST_GENOME_SIZE
};

typedef struct operator_evidence {
    size_t crossover_calls;
    size_t mutation_calls;
} operator_evidence_t;

typedef struct tail_fixture {
    unsigned char genomes[TEST_MAX_STORAGE_BYTES];
    evo_candidate_evaluation_t
        evaluations[TEST_MAX_POPULATION_SIZE];
    evo_population_t parents;
    evo_problem_t problem;
    evo_config_t config;
} tail_fixture_t;

typedef struct population_snapshot {
    evo_population_t metadata;
    unsigned char genomes[TEST_MAX_STORAGE_BYTES];
    evo_candidate_evaluation_t
        evaluations[TEST_MAX_POPULATION_SIZE];
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
    assert(mutation_rate == 1.0);
    ++operator_evidence->mutation_calls;
    for (size_t index = 0; index < TEST_GENOME_SIZE; ++index) {
        bytes[index] ^=
            (unsigned char)(0x80u + (unsigned int)index);
    }
}

static void fixture_initialize(tail_fixture_t *fixture,
                               size_t population_size)
{
    const size_t storage_bytes = population_size * TEST_GENOME_SIZE;

    assert(population_size != 0);
    assert(population_size <= TEST_MAX_POPULATION_SIZE);
    *fixture = (tail_fixture_t){0};

    fixture->problem.genome_size = TEST_GENOME_SIZE;
    fixture->problem.crossover = test_crossover;
    fixture->problem.mutate = test_mutation;

    fixture->config.population_size = population_size;
    fixture->config.tournament_size = 1;
    fixture->config.crossover_rate = 1.0;
    fixture->config.mutation_rate = 1.0;
    fixture->config.random_seed = 42;
    fixture->config.max_genome_bytes = TEST_GENOME_SIZE;
    fixture->config.max_population_bytes = storage_bytes;
    fixture->config.max_evaluation_bytes =
        population_size * sizeof(evo_candidate_evaluation_t);
    fixture->config.max_child_population_bytes = storage_bytes;

    fixture->parents.genomes = fixture->genomes;
    fixture->parents.evaluations = fixture->evaluations;
    fixture->parents.population_size = population_size;
    fixture->parents.genome_size = TEST_GENOME_SIZE;
    fixture->parents.storage_bytes = storage_bytes;
    fixture->parents.evaluation_bytes =
        fixture->config.max_evaluation_bytes;
    fixture->parents.valid_count = population_size;
    fixture->parents.best_index = population_size - 1;
    fixture->parents.initialization_seed = fixture->config.random_seed;
    fixture->parents.rng_algorithm_version = EVO_RNG_ALGORITHM_VERSION;
    fixture->parents.fitness_comparison_policy_version =
        EVO_FITNESS_COMPARISON_POLICY_VERSION;
    fixture->parents.initialized = true;
    fixture->parents.has_best = true;
    fixture->parents.evaluated = true;

    for (size_t candidate = 0; candidate < population_size; ++candidate) {
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

static void create_children(const tail_fixture_t *fixture,
                            evo_population_t *children)
{
    assert(evo_child_population_create(&fixture->problem,
                                       &fixture->config,
                                       &fixture->parents,
                                       children) == EVO_SUCCESS);
}

static void produce_complete_pairs(const tail_fixture_t *fixture,
                                   uint64_t source_generation,
                                   operator_evidence_t *callbacks,
                                   evo_population_t *children)
{
    const size_t pair_count = fixture->config.population_size / 2;

    for (size_t pair_index = 0; pair_index < pair_count; ++pair_index) {
        evo_child_pair_evidence_t evidence = {0};

        assert(evo_child_pair_produce(&fixture->problem,
                                      &fixture->config,
                                      callbacks,
                                      &fixture->parents,
                                      source_generation,
                                      pair_index,
                                      children,
                                      &evidence) == EVO_SUCCESS);
        assert(evidence.complete);
        assert(evidence.produced_count == (pair_index + 1) * 2);
    }
}

static void snapshot_population(const evo_population_t *population,
                                population_snapshot_t *snapshot)
{
    assert(population->storage_bytes <= TEST_MAX_STORAGE_BYTES);
    assert(population->population_size <= TEST_MAX_POPULATION_SIZE);
    *snapshot = (population_snapshot_t){0};
    snapshot->metadata = *population;

    for (size_t index = 0;
         index < population->storage_bytes &&
         index < TEST_MAX_STORAGE_BYTES;
         ++index) {
        snapshot->genomes[index] = population->genomes[index];
    }
    if (population->evaluations != NULL) {
        for (size_t index = 0;
             index < population->population_size &&
             index < TEST_MAX_POPULATION_SIZE;
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
    assert(left->initialization_seed == right->initialization_seed);
    assert(left->source_generation == right->source_generation);
    assert(left->rng_algorithm_version == right->rng_algorithm_version);
    assert(left->operator_seed_schedule_version ==
           right->operator_seed_schedule_version);
    assert(left->odd_child_policy_version ==
           right->odd_child_policy_version);
    assert(left->fitness_comparison_policy_version ==
           right->fitness_comparison_policy_version);
    assert(left->initialized == right->initialized);
    assert(left->has_best == right->has_best);
    assert(left->evaluated == right->evaluated);
}

static void assert_population_unchanged(
    const evo_population_t *population,
    const population_snapshot_t *snapshot)
{
    assert_population_metadata_equal(population, &snapshot->metadata);
    for (size_t index = 0;
         index < population->storage_bytes &&
         index < TEST_MAX_STORAGE_BYTES;
         ++index) {
        assert(population->genomes[index] == snapshot->genomes[index]);
    }
    if (population->evaluations != NULL) {
        for (size_t index = 0;
             index < population->population_size &&
             index < TEST_MAX_POPULATION_SIZE;
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

static void assert_bytes_equal(const unsigned char *left,
                               const unsigned char *right,
                               size_t byte_count)
{
    for (size_t index = 0; index < byte_count; ++index) {
        assert(left[index] == right[index]);
    }
}

static evo_child_tail_evidence_t sentinel_evidence(void)
{
    return (evo_child_tail_evidence_t){
        .parent_index = 11,
        .child_index = 13,
        .produced_count = 17,
        .source_generation = 19,
        .operator_seed_schedule_version = 23,
        .policy_version = 29,
        .complete = true,
    };
}

static void assert_evidence_equal(
    const evo_child_tail_evidence_t *left,
    const evo_child_tail_evidence_t *right)
{
    assert(left->parent_index == right->parent_index);
    assert(left->child_index == right->child_index);
    assert(left->produced_count == right->produced_count);
    assert(left->source_generation == right->source_generation);
    assert(left->operator_seed_schedule_version ==
           right->operator_seed_schedule_version);
    assert(left->policy_version == right->policy_version);
    assert(left->complete == right->complete);
}

static void assert_completed_child(const evo_population_t *children,
                                   size_t population_size,
                                   uint64_t source_generation)
{
    assert(children->genomes != NULL);
    assert(children->evaluations == NULL);
    assert(children->population_size == population_size);
    assert(children->genome_size == TEST_GENOME_SIZE);
    assert(children->storage_bytes ==
           population_size * TEST_GENOME_SIZE);
    assert(children->evaluation_bytes == 0);
    assert(children->valid_count == 0);
    assert(children->best_index == 0);
    assert(children->produced_count == population_size);
    assert(children->initialization_seed == 0);
    assert(children->source_generation == source_generation);
    assert(children->rng_algorithm_version == 0);
    assert(children->operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(children->odd_child_policy_version ==
           EVO_ODD_CHILD_POLICY_VERSION);
    assert(children->fitness_comparison_policy_version == 0);
    assert(!children->initialized);
    assert(!children->has_best);
    assert(!children->evaluated);
}

static void test_complete_odd_population_clones_stable_best(void)
{
    static const unsigned char expected_best[TEST_GENOME_SIZE] = {
        5, 6, 7, 8};
    tail_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t parent_before = {0};
    population_snapshot_t pair_prefix = {0};
    operator_evidence_t callbacks = {0};
    evo_child_tail_evidence_t evidence = {0};
    size_t validated_count = TEST_MAX_POPULATION_SIZE;
    size_t crossover_calls = 0;
    size_t mutation_calls = 0;

    fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
    create_children(&fixture, &children);
    snapshot_population(&fixture.parents, &parent_before);
    produce_complete_pairs(&fixture, 7, &callbacks, &children);
    snapshot_population(&children, &pair_prefix);
    crossover_calls = callbacks.crossover_calls;
    mutation_calls = callbacks.mutation_calls;

    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  &evidence) == EVO_SUCCESS);
    assert(callbacks.crossover_calls == crossover_calls);
    assert(callbacks.mutation_calls == mutation_calls);
    assert_bytes_equal(children.genomes,
                       pair_prefix.genomes,
                       4 * TEST_GENOME_SIZE);
    assert_bytes_equal(children.genomes + 4 * TEST_GENOME_SIZE,
                       expected_best,
                       TEST_GENOME_SIZE);
    assert_completed_child(&children, TEST_MAX_POPULATION_SIZE, 7);
    assert(evidence.parent_index == 4);
    assert(evidence.child_index == 4);
    assert(evidence.produced_count == TEST_MAX_POPULATION_SIZE);
    assert(evidence.source_generation == 7);
    assert(evidence.operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(evidence.policy_version == EVO_ODD_CHILD_POLICY_VERSION);
    assert(evidence.complete);
    assert_population_unchanged(&fixture.parents, &parent_before);
    assert(!evo_population_validate_completed(
        &fixture.config, &children, &validated_count));
    assert(validated_count == TEST_MAX_POPULATION_SIZE);

    evo_population_destroy(&children);
}

static void test_replay_is_byte_and_evidence_identical(void)
{
    tail_fixture_t fixture = {0};
    evo_population_t first = {0};
    evo_population_t replay = {0};
    operator_evidence_t first_callbacks = {0};
    operator_evidence_t replay_callbacks = {0};
    evo_child_tail_evidence_t first_evidence = {0};
    evo_child_tail_evidence_t replay_evidence = {0};

    fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
    create_children(&fixture, &first);
    create_children(&fixture, &replay);
    produce_complete_pairs(&fixture, 31, &first_callbacks, &first);
    produce_complete_pairs(&fixture, 31, &replay_callbacks, &replay);

    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  31,
                                  &first,
                                  &first_evidence) == EVO_SUCCESS);
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  31,
                                  &replay,
                                  &replay_evidence) == EVO_SUCCESS);
    assert_bytes_equal(first.genomes,
                       replay.genomes,
                       TEST_MAX_STORAGE_BYTES);
    assert_evidence_equal(&first_evidence, &replay_evidence);
    assert(first_callbacks.crossover_calls ==
           replay_callbacks.crossover_calls);
    assert(first_callbacks.mutation_calls ==
           replay_callbacks.mutation_calls);
    assert_completed_child(&first, TEST_MAX_POPULATION_SIZE, 31);
    assert_completed_child(&replay, TEST_MAX_POPULATION_SIZE, 31);

    evo_population_destroy(&first);
    evo_population_destroy(&replay);
}

static void test_one_member_population_is_supported(void)
{
    static const unsigned char expected[TEST_GENOME_SIZE] = {1, 2, 3, 4};
    tail_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t parent_before = {0};
    evo_child_tail_evidence_t evidence = {0};

    fixture_initialize(&fixture, 1);
    create_children(&fixture, &children);
    snapshot_population(&fixture.parents, &parent_before);

    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  9,
                                  &children,
                                  &evidence) == EVO_SUCCESS);
    assert_bytes_equal(children.genomes, expected, TEST_GENOME_SIZE);
    assert_completed_child(&children, 1, 9);
    assert(evidence.parent_index == 0);
    assert(evidence.child_index == 0);
    assert(evidence.produced_count == 1);
    assert_population_unchanged(&fixture.parents, &parent_before);

    evo_population_destroy(&children);
}

static void test_tied_best_uses_stable_first_index(void)
{
    static const unsigned char expected[TEST_GENOME_SIZE] = {2, 3, 4, 5};
    tail_fixture_t fixture = {0};
    evo_population_t children = {0};
    operator_evidence_t callbacks = {0};
    evo_child_tail_evidence_t evidence = {0};

    fixture_initialize(&fixture, 3);
    fixture.evaluations[1].fitness = fitness_with_total(20.0);
    fixture.evaluations[2].fitness = fitness_with_total(20.0);
    fixture.parents.best_index = 1;
    create_children(&fixture, &children);
    produce_complete_pairs(&fixture, 4, &callbacks, &children);

    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  4,
                                  &children,
                                  &evidence) == EVO_SUCCESS);
    assert(evidence.parent_index == 1);
    assert_bytes_equal(children.genomes + 2 * TEST_GENOME_SIZE,
                       expected,
                       TEST_GENOME_SIZE);
    evo_population_destroy(&children);
}

static void test_invalid_preflight_preserves_every_object(void)
{
    tail_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t parent_before = {0};
    population_snapshot_t child_before = {0};
    operator_evidence_t callbacks = {0};
    evo_child_tail_evidence_t evidence = sentinel_evidence();
    const evo_child_tail_evidence_t evidence_before = evidence;

    fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
    create_children(&fixture, &children);
    produce_complete_pairs(&fixture, 7, &callbacks, &children);
    snapshot_population(&fixture.parents, &parent_before);
    snapshot_population(&children, &child_before);

    assert(evo_child_tail_produce(NULL,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_tail_produce(&fixture.problem,
                                  NULL,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  NULL,
                                  7,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  NULL,
                                  &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  NULL) == EVO_ERROR_INVALID_ARGUMENT);

    children.storage_bytes -= 1;
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    children.storage_bytes += 1;

    children.odd_child_policy_version = EVO_ODD_CHILD_POLICY_VERSION;
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    children.odd_child_policy_version = 0;

    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  8,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);

    fixture.parents.evaluated = false;
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    fixture.parents.evaluated = true;

    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &fixture.parents,
                                  &evidence) == EVO_ERROR_STATE);

    children.genomes = fixture.parents.genomes;
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    children.genomes = child_before.metadata.genomes;

    children.genomes = fixture.parents.genomes + 1;
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    children.genomes = child_before.metadata.genomes;

    assert_evidence_equal(&evidence, &evidence_before);
    assert_population_unchanged(&fixture.parents, &parent_before);
    assert_population_unchanged(&children, &child_before);
    evo_population_destroy(&children);
}

static void test_incomplete_pair_prefix_is_rejected(void)
{
    tail_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t child_before = {0};
    operator_evidence_t callbacks = {0};
    evo_child_pair_evidence_t pair = {0};
    evo_child_tail_evidence_t evidence = sentinel_evidence();
    const evo_child_tail_evidence_t evidence_before = evidence;

    fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
    create_children(&fixture, &children);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  7,
                                  0,
                                  &children,
                                  &pair) == EVO_SUCCESS);
    snapshot_population(&children, &child_before);
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  7,
                                  &children,
                                  &evidence) == EVO_ERROR_STATE);
    assert_evidence_equal(&evidence, &evidence_before);
    assert_population_unchanged(&children, &child_before);
    evo_population_destroy(&children);
}

static void test_even_population_is_rejected(void)
{
    tail_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t child_before = {0};
    evo_child_tail_evidence_t evidence = sentinel_evidence();
    const evo_child_tail_evidence_t evidence_before = evidence;

    fixture_initialize(&fixture, 4);
    create_children(&fixture, &children);
    snapshot_population(&children, &child_before);
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  1,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_evidence_equal(&evidence, &evidence_before);
    assert_population_unchanged(&children, &child_before);
    evo_population_destroy(&children);
}

static void test_all_invalid_parent_is_rejected(void)
{
    tail_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t child_before = {0};
    evo_child_tail_evidence_t evidence = sentinel_evidence();
    const evo_child_tail_evidence_t evidence_before = evidence;

    fixture_initialize(&fixture, 1);
    fixture.evaluations[0] = (evo_candidate_evaluation_t){0};
    fixture.parents.valid_count = 0;
    fixture.parents.best_index = 0;
    fixture.parents.has_best = false;
    create_children(&fixture, &children);
    snapshot_population(&children, &child_before);

    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  3,
                                  &children,
                                  &evidence) ==
           EVO_ERROR_NO_VALID_CANDIDATE);
    assert_evidence_equal(&evidence, &evidence_before);
    assert_population_unchanged(&children, &child_before);
    evo_population_destroy(&children);
}

static void test_completed_tail_cannot_be_replayed_or_extended(void)
{
    tail_fixture_t fixture = {0};
    evo_population_t children = {0};
    population_snapshot_t completed = {0};
    operator_evidence_t callbacks = {0};
    evo_child_tail_evidence_t first = {0};
    evo_child_tail_evidence_t tail_output = sentinel_evidence();
    const evo_child_tail_evidence_t tail_output_before = tail_output;
    evo_child_pair_evidence_t pair_output = {
        .produced_count = 47,
        .rng_algorithm_version = 53,
        .complete = true,
    };
    const evo_child_pair_evidence_t pair_output_before = pair_output;

    fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
    create_children(&fixture, &children);
    produce_complete_pairs(&fixture, 12, &callbacks, &children);
    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  12,
                                  &children,
                                  &first) == EVO_SUCCESS);
    snapshot_population(&children, &completed);

    assert(evo_child_tail_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture.parents,
                                  12,
                                  &children,
                                  &tail_output) == EVO_ERROR_STATE);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &callbacks,
                                  &fixture.parents,
                                  12,
                                  0,
                                  &children,
                                  &pair_output) == EVO_ERROR_STATE);
    assert_evidence_equal(&tail_output, &tail_output_before);
    assert(pair_output.plan.parent_a_index ==
           pair_output_before.plan.parent_a_index);
    assert(pair_output.plan.parent_b_index ==
           pair_output_before.plan.parent_b_index);
    assert(pair_output.plan.child_a_index ==
           pair_output_before.plan.child_a_index);
    assert(pair_output.plan.child_b_index ==
           pair_output_before.plan.child_b_index);
    assert(pair_output.plan.pair_index ==
           pair_output_before.plan.pair_index);
    assert(pair_output.plan.source_generation ==
           pair_output_before.plan.source_generation);
    assert(pair_output.plan.seed_schedule_version ==
           pair_output_before.plan.seed_schedule_version);
    assert(pair_output.produced_count ==
           pair_output_before.produced_count);
    assert(pair_output.rng_algorithm_version ==
           pair_output_before.rng_algorithm_version);
    assert(pair_output.complete == pair_output_before.complete);
    assert_population_unchanged(&children, &completed);
    evo_population_destroy(&children);
}

int main(void)
{
    test_complete_odd_population_clones_stable_best();
    test_replay_is_byte_and_evidence_identical();
    test_one_member_population_is_supported();
    test_tied_best_uses_stable_first_index();
    test_invalid_preflight_preserves_every_object();
    test_incomplete_pair_prefix_is_rejected();
    test_even_population_is_rejected();
    test_all_invalid_parent_is_rejected();
    test_completed_tail_cannot_be_replayed_or_extended();
    return 0;
}
