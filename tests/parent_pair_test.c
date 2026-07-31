#include "internal/parent_pair.h"

#include <assert.h>

enum {
    TEST_POPULATION_CAPACITY = 5
};

typedef struct pair_fixture {
    unsigned char genomes[TEST_POPULATION_CAPACITY];
    evo_candidate_evaluation_t
        evaluations[TEST_POPULATION_CAPACITY];
    evo_population_t population;
    evo_config_t config;
} pair_fixture_t;

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

static void fixture_set_candidate(pair_fixture_t *fixture,
                                  size_t index,
                                  bool valid,
                                  double total)
{
    assert(index < fixture->population.population_size);
    fixture->evaluations[index] =
        (evo_candidate_evaluation_t){0};
    if (valid) {
        fixture->evaluations[index].fitness =
            fitness_with_total(total);
        fixture->evaluations[index].valid = true;
        fixture->evaluations[index].evaluated = true;
    }
}

static void fixture_finalize(pair_fixture_t *fixture)
{
    size_t valid_count = 0;
    size_t best_index = 0;
    bool has_best = false;

    for (size_t index = 0;
         index < fixture->population.population_size;
         ++index) {
        const evo_candidate_evaluation_t *evaluation =
            &fixture->evaluations[index];

        if (!evaluation->valid) {
            continue;
        }

        ++valid_count;
        if (!has_best ||
            evaluation->fitness.total >
                fixture->evaluations[best_index].fitness.total) {
            best_index = index;
            has_best = true;
        }
    }

    fixture->population.valid_count = valid_count;
    fixture->population.best_index = best_index;
    fixture->population.has_best = has_best;
}

static void fixture_initialize(pair_fixture_t *fixture,
                               size_t population_size,
                               size_t tournament_size,
                               uint64_t seed)
{
    assert(population_size > 0);
    assert(population_size <= TEST_POPULATION_CAPACITY);

    *fixture = (pair_fixture_t){0};
    fixture->config.population_size = population_size;
    fixture->config.tournament_size = tournament_size;
    fixture->config.random_seed = seed;
    fixture->config.max_genome_bytes = 1;
    fixture->config.max_population_bytes = population_size;
    fixture->config.max_evaluation_bytes =
        population_size * sizeof(evo_candidate_evaluation_t);

    fixture->population.genomes = fixture->genomes;
    fixture->population.evaluations = fixture->evaluations;
    fixture->population.population_size = population_size;
    fixture->population.genome_size = 1;
    fixture->population.storage_bytes = population_size;
    fixture->population.evaluation_bytes =
        fixture->config.max_evaluation_bytes;
    fixture->population.initialization_seed = seed;
    fixture->population.rng_algorithm_version =
        EVO_RNG_ALGORITHM_VERSION;
    fixture->population.initialized = true;
    fixture->population.evaluated = true;

    for (size_t index = 0; index < population_size; ++index) {
        fixture->genomes[index] = (unsigned char)(index + 1);
        fixture_set_candidate(
            fixture, index, true, (double)index);
    }
    fixture_finalize(fixture);
}

static evo_parent_pair_t sentinel_pair(void)
{
    return (evo_parent_pair_t){
        .parent_a_index = 31,
        .parent_b_index = 37,
        .child_a_index = 41,
        .child_b_index = 43,
        .pair_index = 47,
        .source_generation = 53,
        .seed_schedule_version = 59,
    };
}

static void assert_pair_equal(const evo_parent_pair_t *left,
                              const evo_parent_pair_t *right)
{
    assert(left->parent_a_index == right->parent_a_index);
    assert(left->parent_b_index == right->parent_b_index);
    assert(left->child_a_index == right->child_a_index);
    assert(left->child_b_index == right->child_b_index);
    assert(left->pair_index == right->pair_index);
    assert(left->source_generation == right->source_generation);
    assert(left->seed_schedule_version ==
           right->seed_schedule_version);
}

static void assert_parent_unchanged(const pair_fixture_t *fixture,
                                    const evo_population_t *before,
                                    const unsigned char *genomes_before,
                                    const evo_candidate_evaluation_t
                                        *evaluations_before)
{
    assert(fixture->population.genomes == before->genomes);
    assert(fixture->population.evaluations == before->evaluations);
    assert(fixture->population.population_size ==
           before->population_size);
    assert(fixture->population.genome_size == before->genome_size);
    assert(fixture->population.storage_bytes == before->storage_bytes);
    assert(fixture->population.evaluation_bytes ==
           before->evaluation_bytes);
    assert(fixture->population.valid_count == before->valid_count);
    assert(fixture->population.best_index == before->best_index);
    assert(fixture->population.initialization_seed ==
           before->initialization_seed);
    assert(fixture->population.rng_algorithm_version ==
           before->rng_algorithm_version);
    assert(fixture->population.initialized == before->initialized);
    assert(fixture->population.has_best == before->has_best);
    assert(fixture->population.evaluated == before->evaluated);

    for (size_t index = 0;
         index < fixture->population.population_size;
         ++index) {
        assert(fixture->genomes[index] == genomes_before[index]);
        assert(fixture->evaluations[index].valid ==
               evaluations_before[index].valid);
        assert(fixture->evaluations[index].evaluated ==
               evaluations_before[index].evaluated);
        assert(fixture->evaluations[index].fitness.total ==
               evaluations_before[index].fitness.total);
    }
}

static void snapshot_parent(
    const pair_fixture_t *fixture,
    evo_population_t *population,
    unsigned char *genomes,
    evo_candidate_evaluation_t *evaluations)
{
    *population = fixture->population;
    for (size_t index = 0;
         index < fixture->population.population_size;
         ++index) {
        genomes[index] = fixture->genomes[index];
        evaluations[index] = fixture->evaluations[index];
    }
}

static void test_invalid_arguments_and_bounds_preserve_output(void)
{
    pair_fixture_t fixture = {0};
    evo_parent_pair_t output = sentinel_pair();
    const evo_parent_pair_t before_output = output;

    fixture_initialize(&fixture, 5, 3, 42);

    assert(evo_parent_pair_plan(
               NULL, &fixture.population, 7, 0, &output) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_parent_pair_plan(
               &fixture.config, NULL, 7, 0, &output) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_parent_pair_plan(
               &fixture.config, &fixture.population, 7, 0, NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert_pair_equal(&output, &before_output);

    fixture.config.tournament_size = 0;
    assert(evo_parent_pair_plan(
               &fixture.config, &fixture.population, 7, 0, &output) ==
           EVO_ERROR_RESOURCE_LIMIT);
    fixture.config.tournament_size = 6;
    assert(evo_parent_pair_plan(
               &fixture.config, &fixture.population, 7, 0, &output) ==
           EVO_ERROR_RESOURCE_LIMIT);
    fixture.config.tournament_size = 3;

    assert(evo_parent_pair_plan(
               &fixture.config, &fixture.population, 7, 2, &output) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert_pair_equal(&output, &before_output);
}

static void test_incomplete_and_all_invalid_parent_rejection(void)
{
    pair_fixture_t fixture = {0};
    evo_parent_pair_t output = sentinel_pair();
    const evo_parent_pair_t before_output = output;

    fixture_initialize(&fixture, 5, 3, 42);
    fixture.population.initialized = false;
    assert(evo_parent_pair_plan(
               &fixture.config, &fixture.population, 7, 0, &output) ==
           EVO_ERROR_STATE);
    assert_pair_equal(&output, &before_output);
    fixture.population.initialized = true;

    for (size_t index = 0;
         index < fixture.population.population_size;
         ++index) {
        fixture_set_candidate(&fixture, index, false, 0.0);
    }
    fixture_finalize(&fixture);
    assert(evo_parent_pair_plan(
               &fixture.config, &fixture.population, 7, 0, &output) ==
           EVO_ERROR_NO_VALID_CANDIDATE);
    assert_pair_equal(&output, &before_output);
}

static void test_fixed_complete_pair_vector_and_replay(void)
{
    pair_fixture_t fixture = {0};
    evo_parent_pair_t first = {0};
    evo_parent_pair_t replay = {0};
    evo_parent_pair_t second_pair = {0};
    evo_population_t before_population = {0};
    unsigned char genomes_before[TEST_POPULATION_CAPACITY] = {0};
    evo_candidate_evaluation_t
        evaluations_before[TEST_POPULATION_CAPACITY] = {0};

    fixture_initialize(&fixture, 5, 3, 42);
    snapshot_parent(&fixture,
                    &before_population,
                    genomes_before,
                    evaluations_before);

    assert(evo_parent_pair_plan(
               &fixture.config, &fixture.population, 7, 0, &first) ==
           EVO_SUCCESS);
    assert(evo_parent_pair_plan(
               &fixture.config, &fixture.population, 7, 0, &replay) ==
           EVO_SUCCESS);
    assert_pair_equal(&first, &replay);

    assert(first.parent_a_index == 4);
    assert(first.parent_b_index == 4);
    assert(first.child_a_index == 0);
    assert(first.child_b_index == 1);
    assert(first.pair_index == 0);
    assert(first.source_generation == 7);
    assert(first.seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);

    assert(evo_parent_pair_plan(
               &fixture.config,
               &fixture.population,
               7,
               1,
               &second_pair) == EVO_SUCCESS);
    assert(second_pair.parent_a_index == 4);
    assert(second_pair.parent_b_index == 2);
    assert(second_pair.child_a_index == 2);
    assert(second_pair.child_b_index == 3);
    assert(second_pair.pair_index == 1);
    assert(second_pair.source_generation == 7);

    assert_parent_unchanged(&fixture,
                            &before_population,
                            genomes_before,
                            evaluations_before);

    const evo_parent_pair_t before_rejection = second_pair;
    assert(evo_parent_pair_plan(
               &fixture.config,
               &fixture.population,
               7,
               2,
               &second_pair) == EVO_ERROR_RESOURCE_LIMIT);
    assert_pair_equal(&second_pair, &before_rejection);
}

static void test_valid_only_parent_domain(void)
{
    pair_fixture_t fixture = {0};

    fixture_initialize(&fixture, 5, 3, 73);
    for (size_t index = 0;
         index < fixture.population.population_size;
         ++index) {
        fixture_set_candidate(&fixture, index, false, 0.0);
    }
    fixture_set_candidate(&fixture, 1, true, 10.0);
    fixture_set_candidate(&fixture, 4, true, 40.0);
    fixture_finalize(&fixture);

    for (size_t pair_index = 0; pair_index < 2; ++pair_index) {
        evo_parent_pair_t pair = {0};

        assert(evo_parent_pair_plan(
                   &fixture.config,
                   &fixture.population,
                   11,
                   pair_index,
                   &pair) == EVO_SUCCESS);
        assert(pair.parent_a_index == 1 ||
               pair.parent_a_index == 4);
        assert(pair.parent_b_index == 1 ||
               pair.parent_b_index == 4);
    }
}

int main(void)
{
    test_invalid_arguments_and_bounds_preserve_output();
    test_incomplete_and_all_invalid_parent_rejection();
    test_fixed_complete_pair_vector_and_replay();
    test_valid_only_parent_domain();
    return 0;
}
