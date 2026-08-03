#include "internal/child_pair.h"
#include "internal/child_single.h"
#include "internal/child_tail.h"
#include "internal/elite.h"

#include <assert.h>
#include <math.h>
#include <string.h>

enum {
    TEST_MAX_POPULATION_SIZE = 6,
    TEST_GENOME_SIZE = 4,
    TEST_MAX_STORAGE_BYTES =
        TEST_MAX_POPULATION_SIZE * TEST_GENOME_SIZE
};

typedef struct operator_evidence {
    size_t crossover_calls;
    size_t mutation_calls;
} operator_evidence_t;

typedef struct elite_fixture {
    unsigned char genomes[TEST_MAX_STORAGE_BYTES];
    evo_candidate_evaluation_t
        evaluations[TEST_MAX_POPULATION_SIZE];
    evo_population_t parents;
    evo_problem_t problem;
    evo_config_t config;
} elite_fixture_t;

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
        .constraint_penalty = 0.0,
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
    operator_evidence_t *evidence = context;

    assert(evidence != NULL);
    ++evidence->crossover_calls;
    for (size_t offset = 0; offset < TEST_GENOME_SIZE; ++offset) {
        first[offset] = (unsigned char)(left[offset] ^
                                        right[offset] ^ 0x5au);
        second[offset] =
            (unsigned char)((unsigned int)left[offset] +
                            (unsigned int)right[offset] +
                            (unsigned int)offset);
    }
}

static void test_mutation(void *genome,
                          double mutation_rate,
                          void *context)
{
    unsigned char *bytes = genome;
    operator_evidence_t *evidence = context;

    assert(evidence != NULL);
    assert(mutation_rate == 1.0);
    ++evidence->mutation_calls;
    for (size_t offset = 0; offset < TEST_GENOME_SIZE; ++offset) {
        bytes[offset] ^=
            (unsigned char)(0xa0u + (unsigned int)offset);
    }
}

static void fixture_set_evaluations(elite_fixture_t *fixture,
                                    const double *totals,
                                    const bool *valid)
{
    size_t valid_count = 0;
    size_t best_index = 0;
    bool has_best = false;

    fixture->parents.evaluations = fixture->evaluations;
    for (size_t index = 0;
         index < fixture->config.population_size;
         ++index) {
        fixture->evaluations[index] =
            (evo_candidate_evaluation_t){0};
        if (!valid[index]) {
            continue;
        }

        fixture->evaluations[index].fitness =
            fitness_with_total(totals[index]);
        fixture->evaluations[index].valid = true;
        fixture->evaluations[index].evaluated = true;
        ++valid_count;
        if (!has_best || totals[index] > totals[best_index]) {
            best_index = index;
            has_best = true;
        }
    }

    fixture->parents.valid_count = valid_count;
    fixture->parents.best_index = has_best ? best_index : 0;
    fixture->parents.has_best = has_best;
    fixture->parents.diversity_pair_count =
        valid_count * (valid_count - 1) / 2;
    fixture->parents.diversity_work_units =
        fixture->parents.diversity_pair_count * TEST_GENOME_SIZE;
    fixture->parents.diversity =
        fixture->parents.diversity_pair_count == 0 ? 0.0 : 0.5;
}

static void fixture_initialize(elite_fixture_t *fixture,
                               size_t population_size)
{
    double totals[TEST_MAX_POPULATION_SIZE] = {0};
    bool valid[TEST_MAX_POPULATION_SIZE] = {0};
    const size_t storage_bytes =
        population_size * TEST_GENOME_SIZE;

    assert(population_size != 0);
    assert(population_size <= TEST_MAX_POPULATION_SIZE);
    *fixture = (elite_fixture_t){0};

    fixture->problem.genome_size = TEST_GENOME_SIZE;
    fixture->problem.crossover = test_crossover;
    fixture->problem.mutate = test_mutation;

    fixture->config.population_size = population_size;
    fixture->config.tournament_size = 2;
    fixture->config.crossover_rate = 1.0;
    fixture->config.mutation_rate = 1.0;
    fixture->config.random_seed = UINT64_C(0x1020304050607080);
    fixture->config.max_genome_bytes = TEST_GENOME_SIZE;
    fixture->config.max_population_bytes = storage_bytes;
    fixture->config.max_evaluation_bytes =
        population_size * sizeof(evo_candidate_evaluation_t);
    fixture->config.max_child_population_bytes = storage_bytes;
    fixture->config.max_diversity_work = SIZE_MAX;

    fixture->parents.genomes = fixture->genomes;
    fixture->parents.evaluations = fixture->evaluations;
    fixture->parents.population_size = population_size;
    fixture->parents.genome_size = TEST_GENOME_SIZE;
    fixture->parents.storage_bytes = storage_bytes;
    fixture->parents.evaluation_bytes =
        fixture->config.max_evaluation_bytes;
    fixture->parents.initialization_seed =
        fixture->config.random_seed;
    fixture->parents.rng_algorithm_version =
        EVO_RNG_ALGORITHM_VERSION;
    fixture->parents.fitness_comparison_policy_version =
        EVO_FITNESS_COMPARISON_POLICY_VERSION;
    fixture->parents.diversity_policy_version =
        EVO_DIVERSITY_POLICY_VERSION;
    fixture->parents.diversity_metric_version =
        EVO_BYTE_DIVERSITY_METRIC_VERSION;
    fixture->parents.initialized = true;
    fixture->parents.evaluated = true;

    for (size_t candidate = 0;
         candidate < population_size;
         ++candidate) {
        totals[candidate] =
            (double)(TEST_MAX_POPULATION_SIZE - candidate);
        valid[candidate] = true;
        for (size_t offset = 0;
             offset < TEST_GENOME_SIZE;
             ++offset) {
            fixture->genomes[candidate * TEST_GENOME_SIZE + offset] =
                (unsigned char)(0x10u +
                                (unsigned int)(candidate * 7) +
                                (unsigned int)offset);
        }
    }
    fixture_set_evaluations(fixture, totals, valid);
}

static void snapshot_population(const evo_population_t *population,
                                population_snapshot_t *snapshot)
{
    assert(population != NULL);
    assert(snapshot != NULL);
    assert(population->storage_bytes <= TEST_MAX_STORAGE_BYTES);
    assert(population->population_size <=
           TEST_MAX_POPULATION_SIZE);

    *snapshot = (population_snapshot_t){0};
    snapshot->metadata = *population;
    if (population->genomes != NULL) {
        for (size_t offset = 0;
             offset < population->storage_bytes;
             ++offset) {
            snapshot->genomes[offset] = population->genomes[offset];
        }
    }
    if (population->evaluations != NULL) {
        unsigned char *const snapshot_bytes =
            (unsigned char *)(void *)snapshot->evaluations;
        const unsigned char *const population_bytes =
            (const unsigned char *)(const void *)population->evaluations;

        for (size_t offset = 0;
             offset < population->evaluation_bytes;
             ++offset) {
            snapshot_bytes[offset] = population_bytes[offset];
        }
    }
}

static void assert_population_unchanged(
    const evo_population_t *population,
    const population_snapshot_t *snapshot)
{
    assert(memcmp(population,
                  &snapshot->metadata,
                  sizeof(*population)) == 0);
    if (population->genomes != NULL) {
        assert(memcmp(population->genomes,
                      snapshot->genomes,
                      population->storage_bytes) == 0);
    }
    if (population->evaluations != NULL) {
        assert(memcmp(population->evaluations,
                      snapshot->evaluations,
                      population->evaluation_bytes) == 0);
    }
}

static void assert_genomes_equal(const evo_population_t *left,
                                 size_t left_index,
                                 const evo_population_t *right,
                                 size_t right_index)
{
    const void *left_genome =
        evo_population_genome_const(left, left_index);
    const void *right_genome =
        evo_population_genome_const(right, right_index);

    assert(left_genome != NULL);
    assert(right_genome != NULL);
    assert(memcmp(left_genome,
                  right_genome,
                  TEST_GENOME_SIZE) == 0);
}

static void create_children(const elite_fixture_t *fixture,
                            evo_population_t *children)
{
    assert(evo_child_population_create(&fixture->problem,
                                       &fixture->config,
                                       &fixture->parents,
                                       children) == EVO_SUCCESS);
}

static void produce_prefix(const elite_fixture_t *fixture,
                           uint64_t source_generation,
                           size_t offspring_count,
                           operator_evidence_t *callbacks,
                           evo_population_t *children,
                           evo_child_single_evidence_t *single_evidence)
{
    for (size_t pair_index = 0;
         pair_index < offspring_count / 2;
         ++pair_index) {
        evo_child_pair_evidence_t pair_evidence = {0};

        assert(evo_child_pair_produce(&fixture->problem,
                                      &fixture->config,
                                      callbacks,
                                      &fixture->parents,
                                      source_generation,
                                      pair_index,
                                      children,
                                      &pair_evidence) == EVO_SUCCESS);
        assert(pair_evidence.complete);
    }

    if (offspring_count % 2 != 0) {
        assert(single_evidence != NULL);
        assert(evo_child_single_produce(&fixture->problem,
                                        &fixture->config,
                                        callbacks,
                                        &fixture->parents,
                                        source_generation,
                                        children,
                                        single_evidence) == EVO_SUCCESS);
        assert(single_evidence->complete);
    }
}

static void complete_population(
    const elite_fixture_t *fixture,
    uint64_t source_generation,
    operator_evidence_t *callbacks,
    evo_population_t *children,
    evo_elite_evidence_t *elite_evidence,
    evo_child_single_evidence_t *single_evidence)
{
    size_t requested_count = 0;
    size_t effective_count = 0;
    size_t offspring_count = 0;
    operator_evidence_t before_elites = {0};

    assert(evo_elite_policy_counts(&fixture->config,
                                   fixture->parents.valid_count,
                                   &requested_count,
                                   &effective_count,
                                   &offspring_count) == EVO_SUCCESS);
    (void)requested_count;
    (void)effective_count;
    produce_prefix(fixture,
                   source_generation,
                   offspring_count,
                   callbacks,
                   children,
                   single_evidence);
    before_elites = *callbacks;
    assert(evo_elite_population_complete(&fixture->problem,
                                         &fixture->config,
                                         &fixture->parents,
                                         source_generation,
                                         children,
                                         elite_evidence) == EVO_SUCCESS);
    assert(callbacks->crossover_calls ==
           before_elites.crossover_calls);
    assert(callbacks->mutation_calls ==
           before_elites.mutation_calls);
}

static void test_config_validation(void)
{
    elite_fixture_t fixture = {0};
    size_t requested = 17;
    size_t effective = 19;
    size_t offspring = 23;

    fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
    assert(evo_elite_validate_config(NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_elite_validate_config(&fixture.config) == EVO_SUCCESS);

    fixture.config.elite_count = 1;
    assert(evo_elite_validate_config(&fixture.config) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert(evo_elite_policy_counts(&fixture.config,
                                   fixture.parents.valid_count,
                                   &requested,
                                   &effective,
                                   &offspring) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert(requested == 17);
    assert(effective == 19);
    assert(offspring == 23);

    fixture.config.elite_count_enabled = true;
    fixture.config.elite_count =
        fixture.config.population_size + 1;
    assert(evo_elite_validate_config(&fixture.config) ==
           EVO_ERROR_RESOURCE_LIMIT);

    fixture.config.elite_count = fixture.config.population_size;
    assert(evo_elite_validate_config(&fixture.config) == EVO_SUCCESS);
    assert(evo_elite_policy_counts(&fixture.config,
                                   fixture.parents.valid_count,
                                   NULL,
                                   &effective,
                                   &offspring) ==
           EVO_ERROR_INVALID_ARGUMENT);
}

static void assert_explicit_policy(size_t population_size,
                                   size_t requested_count)
{
    elite_fixture_t fixture = {0};
    population_snapshot_t parent_snapshot = {0};
    operator_evidence_t callbacks = {0};
    evo_population_t children = {0};
    evo_elite_evidence_t elite_evidence = {0};
    evo_child_single_evidence_t single_evidence = {0};
    const size_t expected_offspring =
        population_size - requested_count;

    fixture_initialize(&fixture, population_size);
    fixture.config.elite_count_enabled = true;
    fixture.config.elite_count = requested_count;
    snapshot_population(&fixture.parents, &parent_snapshot);
    create_children(&fixture, &children);
    complete_population(&fixture,
                        UINT64_C(9),
                        &callbacks,
                        &children,
                        &elite_evidence,
                        &single_evidence);

    assert(elite_evidence.requested_count == requested_count);
    assert(elite_evidence.effective_count == requested_count);
    assert(elite_evidence.source_valid_count == population_size);
    assert(elite_evidence.offspring_count == expected_offspring);
    assert(elite_evidence.source_generation == UINT64_C(9));
    assert(elite_evidence.operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(elite_evidence.odd_child_policy_version == 0);
    assert(elite_evidence.elite_policy_version ==
           EVO_ELITE_POLICY_VERSION);
    assert(elite_evidence.singleton_child_policy_version ==
           (expected_offspring % 2 != 0
                ? EVO_SINGLETON_CHILD_POLICY_VERSION
                : 0));
    assert(elite_evidence.elite_count_explicit);
    assert(elite_evidence.complete);
    assert(children.produced_count == population_size);
    assert(children.elite_count == requested_count);
    assert(children.elite_source_valid_count == population_size);
    assert(children.elite_policy_version == EVO_ELITE_POLICY_VERSION);
    assert(children.singleton_child_policy_version ==
           elite_evidence.singleton_child_policy_version);
    assert(children.elite_count_explicit);
    assert(callbacks.crossover_calls == expected_offspring / 2);
    assert(callbacks.mutation_calls == expected_offspring);

    for (size_t rank = 0; rank < requested_count; ++rank) {
        assert_genomes_equal(&children,
                             expected_offspring + rank,
                             &fixture.parents,
                             rank);
    }
    if (requested_count != 0) {
        assert(elite_evidence.best_parent_index == 0);
        assert(elite_evidence.worst_elite_parent_index ==
               requested_count - 1);
    }
    assert_population_unchanged(&fixture.parents, &parent_snapshot);
    evo_population_destroy(&children);
}

static void test_even_and_odd_boundaries(void)
{
    const size_t even_counts[] = {0, 1, 5, 6};
    const size_t odd_counts[] = {0, 1, 4, 5};

    for (size_t index = 0;
         index < sizeof(even_counts) / sizeof(even_counts[0]);
         ++index) {
        assert_explicit_policy(6, even_counts[index]);
    }
    for (size_t index = 0;
         index < sizeof(odd_counts) / sizeof(odd_counts[0]);
         ++index) {
        assert_explicit_policy(5, odd_counts[index]);
    }
}

static void test_ties_and_invalid_candidates(void)
{
    const double totals[TEST_MAX_POPULATION_SIZE] = {
        10.0,
        0.0,
        10.0,
        5.0,
        7.0,
        0.0,
    };
    const bool valid[TEST_MAX_POPULATION_SIZE] = {
        true,
        false,
        true,
        true,
        true,
        false,
    };
    const size_t expected_ranking[] = {0, 2, 4, 3};
    const size_t requested_counts[] = {
        0,
        1,
        TEST_MAX_POPULATION_SIZE - 1,
        TEST_MAX_POPULATION_SIZE,
    };

    for (size_t count_index = 0;
         count_index < sizeof(requested_counts) /
                           sizeof(requested_counts[0]);
         ++count_index) {
        elite_fixture_t fixture = {0};
        population_snapshot_t parent_snapshot = {0};
        operator_evidence_t callbacks = {0};
        evo_population_t children = {0};
        evo_elite_evidence_t evidence = {0};
        evo_child_single_evidence_t single = {0};
        const size_t requested_count =
            requested_counts[count_index];
        const size_t effective_count =
            requested_count < 4 ? requested_count : 4;
        const size_t offspring_count =
            TEST_MAX_POPULATION_SIZE - effective_count;

        fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
        fixture_set_evaluations(&fixture, totals, valid);
        fixture.config.elite_count_enabled = true;
        fixture.config.elite_count = requested_count;
        snapshot_population(&fixture.parents, &parent_snapshot);
        create_children(&fixture, &children);
        complete_population(&fixture,
                            UINT64_C(11),
                            &callbacks,
                            &children,
                            &evidence,
                            &single);

        assert(evidence.requested_count == requested_count);
        assert(evidence.effective_count == effective_count);
        assert(evidence.source_valid_count == 4);
        assert(evidence.offspring_count == offspring_count);
        assert(callbacks.crossover_calls == offspring_count / 2);
        assert(callbacks.mutation_calls == offspring_count);
        for (size_t rank = 0; rank < effective_count; ++rank) {
            assert_genomes_equal(&children,
                                 offspring_count + rank,
                                 &fixture.parents,
                                 expected_ranking[rank]);
        }
        if (effective_count != 0) {
            assert(evidence.best_parent_index == expected_ranking[0]);
            assert(evidence.worst_elite_parent_index ==
                   expected_ranking[effective_count - 1]);
        }
        assert_population_unchanged(&fixture.parents,
                                    &parent_snapshot);
        evo_population_destroy(&children);
    }
}

static void test_singleton_schedule_and_replay(void)
{
    elite_fixture_t fixture = {0};
    population_snapshot_t parent_snapshot = {0};
    operator_evidence_t first_callbacks = {0};
    operator_evidence_t second_callbacks = {0};
    evo_population_t first = {0};
    evo_population_t second = {0};
    evo_elite_evidence_t first_elite = {0};
    evo_elite_evidence_t second_elite = {0};
    evo_child_single_evidence_t first_single = {0};
    evo_child_single_evidence_t second_single = {0};

    fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
    fixture.config.elite_count_enabled = true;
    fixture.config.elite_count = 1;
    snapshot_population(&fixture.parents, &parent_snapshot);
    create_children(&fixture, &first);
    create_children(&fixture, &second);
    complete_population(&fixture,
                        UINT64_C(13),
                        &first_callbacks,
                        &first,
                        &first_elite,
                        &first_single);
    complete_population(&fixture,
                        UINT64_C(13),
                        &second_callbacks,
                        &second,
                        &second_elite,
                        &second_single);

    assert(first_single.child_index == 4);
    assert(first_single.selection_stream_index == 2);
    assert(first_single.parent_index < fixture.parents.population_size);
    assert(fixture.parents.evaluations[first_single.parent_index].valid);
    assert(first_single.produced_count == 5);
    assert(first_single.source_generation == UINT64_C(13));
    assert(first_single.rng_algorithm_version ==
           EVO_RNG_ALGORITHM_VERSION);
    assert(first_single.operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(first_single.policy_version ==
           EVO_SINGLETON_CHILD_POLICY_VERSION);
    assert(first_single.complete);
    assert(first_callbacks.crossover_calls == 2);
    assert(first_callbacks.mutation_calls == 5);
    assert(memcmp(first.genomes,
                  second.genomes,
                  first.storage_bytes) == 0);
    assert(memcmp(&first_single,
                  &second_single,
                  sizeof(first_single)) == 0);
    assert(memcmp(&first_elite,
                  &second_elite,
                  sizeof(first_elite)) == 0);
    assert(memcmp(&first_callbacks,
                  &second_callbacks,
                  sizeof(first_callbacks)) == 0);
    assert_population_unchanged(&fixture.parents, &parent_snapshot);

    evo_population_destroy(&second);
    evo_population_destroy(&first);
}

static void test_compatibility_mode(void)
{
    elite_fixture_t odd_fixture = {0};
    elite_fixture_t even_fixture = {0};
    operator_evidence_t tail_callbacks = {0};
    operator_evidence_t elite_callbacks = {0};
    operator_evidence_t even_callbacks = {0};
    evo_population_t tail_children = {0};
    evo_population_t elite_children = {0};
    evo_population_t even_children = {0};
    evo_child_tail_evidence_t tail_evidence = {0};
    evo_elite_evidence_t elite_evidence = {0};
    evo_elite_evidence_t even_evidence = {0};
    evo_child_single_evidence_t unused_single = {0};

    fixture_initialize(&odd_fixture, 5);
    create_children(&odd_fixture, &tail_children);
    create_children(&odd_fixture, &elite_children);
    produce_prefix(&odd_fixture,
                   UINT64_C(17),
                   4,
                   &tail_callbacks,
                   &tail_children,
                   &unused_single);
    unused_single = (evo_child_single_evidence_t){0};
    produce_prefix(&odd_fixture,
                   UINT64_C(17),
                   4,
                   &elite_callbacks,
                   &elite_children,
                   &unused_single);
    assert(evo_child_tail_produce(&odd_fixture.problem,
                                  &odd_fixture.config,
                                  &odd_fixture.parents,
                                  UINT64_C(17),
                                  &tail_children,
                                  &tail_evidence) == EVO_SUCCESS);
    assert(evo_elite_population_complete(&odd_fixture.problem,
                                         &odd_fixture.config,
                                         &odd_fixture.parents,
                                         UINT64_C(17),
                                         &elite_children,
                                         &elite_evidence) == EVO_SUCCESS);
    assert(memcmp(tail_children.genomes,
                  elite_children.genomes,
                  tail_children.storage_bytes) == 0);
    assert(memcmp(&tail_callbacks,
                  &elite_callbacks,
                  sizeof(tail_callbacks)) == 0);
    assert(tail_callbacks.crossover_calls == 2);
    assert(tail_callbacks.mutation_calls == 4);
    assert(tail_evidence.parent_index ==
           elite_evidence.best_parent_index);
    assert(tail_evidence.child_index == 4);
    assert(tail_evidence.policy_version == EVO_ODD_CHILD_POLICY_VERSION);
    assert(elite_evidence.requested_count == 1);
    assert(elite_evidence.effective_count == 1);
    assert(elite_evidence.offspring_count == 4);
    assert(elite_evidence.odd_child_policy_version ==
           EVO_ODD_CHILD_POLICY_VERSION);
    assert(elite_evidence.singleton_child_policy_version == 0);
    assert(!elite_evidence.elite_count_explicit);
    assert_genomes_equal(&elite_children,
                         4,
                         &odd_fixture.parents,
                         odd_fixture.parents.best_index);

    fixture_initialize(&even_fixture, 6);
    create_children(&even_fixture, &even_children);
    complete_population(&even_fixture,
                        UINT64_C(19),
                        &even_callbacks,
                        &even_children,
                        &even_evidence,
                        &unused_single);
    assert(even_evidence.requested_count == 0);
    assert(even_evidence.effective_count == 0);
    assert(even_evidence.offspring_count == 6);
    assert(even_evidence.odd_child_policy_version == 0);
    assert(even_evidence.singleton_child_policy_version == 0);
    assert(!even_evidence.elite_count_explicit);
    assert(even_callbacks.crossover_calls == 3);
    assert(even_callbacks.mutation_calls == 6);

    evo_population_destroy(&even_children);
    evo_population_destroy(&elite_children);
    evo_population_destroy(&tail_children);
}

static evo_elite_evidence_t sentinel_elite_evidence(void)
{
    return (evo_elite_evidence_t){
        .requested_count = 3,
        .effective_count = 5,
        .source_valid_count = 7,
        .offspring_count = 11,
        .best_parent_index = 13,
        .worst_elite_parent_index = 17,
        .source_generation = 19,
        .operator_seed_schedule_version = 23,
        .odd_child_policy_version = 29,
        .elite_policy_version = 31,
        .singleton_child_policy_version = 37,
        .elite_count_explicit = true,
        .complete = true,
    };
}

static void test_elite_rejection_is_atomic(void)
{
    elite_fixture_t fixture = {0};
    population_snapshot_t parent_snapshot = {0};
    population_snapshot_t child_snapshot = {0};
    operator_evidence_t callbacks = {0};
    evo_population_t children = {0};
    evo_elite_evidence_t evidence = sentinel_elite_evidence();
    const evo_elite_evidence_t evidence_snapshot = evidence;
    evo_child_single_evidence_t unused_single = {0};
    unsigned char *owned_child_genomes = NULL;

    fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
    fixture.config.elite_count_enabled = true;
    fixture.config.elite_count = 2;
    create_children(&fixture, &children);
    produce_prefix(&fixture,
                   UINT64_C(23),
                   4,
                   &callbacks,
                   &children,
                   &unused_single);
    snapshot_population(&fixture.parents, &parent_snapshot);
    snapshot_population(&children, &child_snapshot);

    assert(evo_elite_population_complete(
               &fixture.problem,
               &fixture.config,
               &fixture.parents,
               UINT64_C(23),
               &children,
               (evo_elite_evidence_t *)(void *)&children) ==
           EVO_ERROR_STATE);
    assert_population_unchanged(&fixture.parents, &parent_snapshot);
    assert_population_unchanged(&children, &child_snapshot);

    owned_child_genomes = children.genomes;
    children.genomes = fixture.parents.genomes;
    snapshot_population(&children, &child_snapshot);
    assert(evo_elite_population_complete(&fixture.problem,
                                         &fixture.config,
                                         &fixture.parents,
                                         UINT64_C(23),
                                         &children,
                                         &evidence) ==
           EVO_ERROR_STATE);
    assert_population_unchanged(&fixture.parents, &parent_snapshot);
    assert_population_unchanged(&children, &child_snapshot);
    assert(memcmp(&evidence,
                  &evidence_snapshot,
                  sizeof(evidence)) == 0);
    children.genomes = owned_child_genomes;

    snapshot_population(&children, &child_snapshot);
    fixture.evaluations[0].fitness.total = NAN;
    assert(evo_elite_population_complete(&fixture.problem,
                                         &fixture.config,
                                         &fixture.parents,
                                         UINT64_C(23),
                                         &children,
                                         &evidence) ==
           EVO_ERROR_STATE);
    assert_population_unchanged(&children, &child_snapshot);
    assert(memcmp(&evidence,
                  &evidence_snapshot,
                  sizeof(evidence)) == 0);

    evo_population_destroy(&children);
}

static void test_singleton_alias_rejection_is_atomic(void)
{
    elite_fixture_t fixture = {0};
    population_snapshot_t parent_snapshot = {0};
    population_snapshot_t child_snapshot = {0};
    operator_evidence_t callbacks = {0};
    evo_population_t children = {0};
    evo_child_single_evidence_t evidence = {0};

    fixture_initialize(&fixture, TEST_MAX_POPULATION_SIZE);
    fixture.config.elite_count_enabled = true;
    fixture.config.elite_count = 1;
    create_children(&fixture, &children);
    produce_prefix(&fixture,
                   UINT64_C(29),
                   4,
                   &callbacks,
                   &children,
                   &evidence);
    snapshot_population(&fixture.parents, &parent_snapshot);
    snapshot_population(&children, &child_snapshot);

    assert(evo_child_single_produce(
               &fixture.problem,
               &fixture.config,
               &callbacks,
               &fixture.parents,
               UINT64_C(29),
               &children,
               (evo_child_single_evidence_t *)(void *)&children) ==
           EVO_ERROR_STATE);
    assert(callbacks.crossover_calls == 2);
    assert(callbacks.mutation_calls == 4);
    assert_population_unchanged(&fixture.parents, &parent_snapshot);
    assert_population_unchanged(&children, &child_snapshot);

    evo_population_destroy(&children);
}

int main(void)
{
    test_config_validation();
    test_even_and_odd_boundaries();
    test_ties_and_invalid_candidates();
    test_singleton_schedule_and_replay();
    test_compatibility_mode();
    test_elite_rejection_is_atomic();
    test_singleton_alias_rejection_is_atomic();
    return 0;
}
