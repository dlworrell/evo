#include "internal/selection.h"

#include <assert.h>
#include <math.h>

enum {
    TEST_POPULATION_CAPACITY = 5
};

typedef struct selection_fixture {
    unsigned char genomes[TEST_POPULATION_CAPACITY];
    evo_candidate_evaluation_t
        evaluations[TEST_POPULATION_CAPACITY];
    evo_population_t population;
    evo_config_t config;
} selection_fixture_t;

static evo_fitness_t fitness_with_total(double total)
{
    evo_fitness_t fitness = {
        .correctness = total + 1.0,
        .performance = total + 2.0,
        .memory_use = total + 3.0,
        .reliability = total + 4.0,
        .maintainability = total + 5.0,
        .constraint_penalty = total + 6.0,
        .total = total,
    };
    return fitness;
}

static void fixture_set_candidate(selection_fixture_t *fixture,
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

static void fixture_finalize(selection_fixture_t *fixture)
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
    fixture->population.diversity_pair_count =
        valid_count * (valid_count - (valid_count != 0 ? 1 : 0)) / 2;
    fixture->population.diversity_work_units =
        fixture->population.diversity_pair_count;
}

static void fixture_initialize(selection_fixture_t *fixture,
                               size_t population_size,
                               size_t tournament_size,
                               uint64_t seed)
{
    assert(population_size > 0);
    assert(population_size <= TEST_POPULATION_CAPACITY);

    *fixture = (selection_fixture_t){0};
    fixture->config.population_size = population_size;
    fixture->config.tournament_size = tournament_size;
    fixture->config.random_seed = seed;
    fixture->config.max_genome_bytes = 1;
    fixture->config.max_population_bytes = population_size;
    fixture->config.max_evaluation_bytes =
        population_size * sizeof(evo_candidate_evaluation_t);
    fixture->config.max_diversity_work = SIZE_MAX;

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
    fixture->population.fitness_comparison_policy_version =
        EVO_FITNESS_COMPARISON_POLICY_VERSION;
    fixture->population.diversity_policy_version =
        EVO_DIVERSITY_POLICY_VERSION;
    fixture->population.diversity_metric_version =
        EVO_BYTE_DIVERSITY_METRIC_VERSION;
    fixture->population.initialized = true;
    fixture->population.evaluated = true;

    for (size_t index = 0; index < population_size; ++index) {
        fixture->genomes[index] = (unsigned char)index;
        fixture_set_candidate(
            fixture, index, true, (double)index);
    }
    fixture_finalize(fixture);
}

static void assert_rng_equal(const evo_rng_t *left,
                             const evo_rng_t *right)
{
    assert(left->state == right->state);
    assert(left->increment == right->increment);
    assert(left->seeded == right->seeded);
}

static void assert_fixture_core_unchanged(
    const selection_fixture_t *fixture,
    const evo_population_t *before)
{
    assert(fixture->population.genomes == before->genomes);
    assert(fixture->population.evaluations == before->evaluations);
    assert(fixture->population.population_size ==
           before->population_size);
    assert(fixture->population.genome_size == before->genome_size);
    assert(fixture->population.storage_bytes ==
           before->storage_bytes);
    assert(fixture->population.evaluation_bytes ==
           before->evaluation_bytes);
    assert(fixture->population.valid_count == before->valid_count);
    assert(fixture->population.best_index == before->best_index);
    assert(fixture->population.produced_count == before->produced_count);
    assert(fixture->population.elite_count == before->elite_count);
    assert(fixture->population.elite_source_valid_count ==
           before->elite_source_valid_count);
    assert(fixture->population.initialization_seed ==
           before->initialization_seed);
    assert(fixture->population.source_generation ==
           before->source_generation);
    assert(fixture->population.rng_algorithm_version ==
           before->rng_algorithm_version);
    assert(fixture->population.operator_seed_schedule_version ==
           before->operator_seed_schedule_version);
    assert(fixture->population.odd_child_policy_version ==
           before->odd_child_policy_version);
    assert(fixture->population.elite_policy_version ==
           before->elite_policy_version);
    assert(fixture->population.singleton_child_policy_version ==
           before->singleton_child_policy_version);
    assert(fixture->population.fitness_comparison_policy_version ==
           before->fitness_comparison_policy_version);
    assert(fixture->population.initialized == before->initialized);
    assert(fixture->population.has_best == before->has_best);
    assert(fixture->population.evaluated == before->evaluated);
    assert(fixture->population.elite_count_explicit ==
           before->elite_count_explicit);
}

static void test_invalid_arguments_preserve_state(void)
{
    selection_fixture_t fixture = {0};
    evo_rng_t rng = {0};
    size_t selected_index = 41;

    fixture_initialize(&fixture, 5, 3, 17);
    assert(evo_rng_seed(&rng, 29));
    const evo_rng_t before_rng = rng;
    const evo_population_t before_population = fixture.population;

    assert(evo_population_select_tournament(
               NULL,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_select_tournament(
               &fixture.config,
               NULL,
               &rng,
               &selected_index) == EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               NULL,
               &selected_index) == EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               NULL) == EVO_ERROR_INVALID_ARGUMENT);

    assert(selected_index == 41);
    assert_rng_equal(&rng, &before_rng);
    assert_fixture_core_unchanged(
        &fixture, &before_population);
}

static void test_tournament_bounds_and_unseeded_rng(void)
{
    selection_fixture_t fixture = {0};
    evo_rng_t rng = {0};
    size_t selected_index = 43;

    fixture_initialize(&fixture, 5, 3, 31);
    const evo_population_t before_population = fixture.population;

    fixture.config.tournament_size = 0;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_RESOURCE_LIMIT);

    fixture.config.tournament_size = 6;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_RESOURCE_LIMIT);

    fixture.config.tournament_size = 3;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_STATE);

    assert(selected_index == 43);
    assert(!rng.seeded);
    assert_fixture_core_unchanged(
        &fixture, &before_population);
}

static void test_inconsistent_population_rejection(void)
{
    selection_fixture_t fixture = {0};
    evo_rng_t rng = {0};
    size_t selected_index = 47;

    fixture_initialize(&fixture, 5, 3, 37);
    assert(evo_rng_seed(&rng, 41));

    const evo_rng_t before_rng = rng;

    fixture.population.initialized = false;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_STATE);
    fixture.population.initialized = true;

    --fixture.population.valid_count;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_STATE);
    ++fixture.population.valid_count;

    fixture.population.best_index = 0;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_STATE);
    fixture.population.best_index = 4;

    fixture.population.fitness_comparison_policy_version = 0;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_STATE);
    fixture.population.fitness_comparison_policy_version =
        EVO_FITNESS_COMPARISON_POLICY_VERSION;

    fixture.evaluations[0].fitness.total = NAN;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_STATE);
    fixture.evaluations[0].fitness =
        fitness_with_total(0.0);

    fixture_set_candidate(&fixture, 2, false, 0.0);
    fixture_finalize(&fixture);
    fixture.evaluations[2].evaluated = true;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_ERROR_STATE);

    assert(selected_index == 47);
    assert_rng_equal(&rng, &before_rng);
}

static void test_all_invalid_population(void)
{
    selection_fixture_t fixture = {0};
    evo_rng_t rng = {0};
    size_t selected_index = 53;

    fixture_initialize(&fixture, 5, 3, 43);
    for (size_t index = 0;
         index < fixture.population.population_size;
         ++index) {
        fixture_set_candidate(&fixture, index, false, 0.0);
    }
    fixture_finalize(&fixture);

    assert(evo_rng_seed(&rng, 47));
    const evo_rng_t before_rng = rng;
    const evo_population_t before_population = fixture.population;

    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) ==
           EVO_ERROR_NO_VALID_CANDIDATE);
    assert(selected_index == 53);
    assert_rng_equal(&rng, &before_rng);
    assert_fixture_core_unchanged(
        &fixture, &before_population);
}

static void test_valid_only_selection_and_replay(void)
{
    selection_fixture_t fixture = {0};
    evo_rng_t first = {0};
    evo_rng_t replay = {0};
    size_t first_index = SIZE_MAX;
    size_t replay_index = SIZE_MAX;

    fixture_initialize(&fixture, 5, 3, 59);
    fixture_set_candidate(&fixture, 0, false, 0.0);
    fixture_set_candidate(&fixture, 2, false, 0.0);
    fixture_set_candidate(&fixture, 3, false, 0.0);
    fixture_set_candidate(&fixture, 1, true, 10.0);
    fixture_set_candidate(&fixture, 4, true, 40.0);
    fixture_finalize(&fixture);

    assert(evo_rng_seed(&first, 42));
    assert(evo_rng_seed(&replay, 42));
    for (size_t tournament = 0; tournament < 32; ++tournament) {
        assert(evo_population_select_tournament(
                   &fixture.config,
                   &fixture.population,
                   &first,
                   &first_index) == EVO_SUCCESS);
        assert(evo_population_select_tournament(
                   &fixture.config,
                   &fixture.population,
                   &replay,
                   &replay_index) == EVO_SUCCESS);
        assert(first_index == replay_index);
        assert(first_index == 1 || first_index == 4);
    }
    assert_rng_equal(&first, &replay);
}

static void test_fixed_vector_and_single_draw(void)
{
    selection_fixture_t fixture = {0};
    evo_rng_t rng = {0};
    size_t selected_index = SIZE_MAX;

    fixture_initialize(&fixture, 5, 3, 61);
    assert(evo_rng_seed(&rng, 42));
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_SUCCESS);
    assert(selected_index == 3);

    fixture.config.tournament_size = 1;
    assert(evo_rng_seed(&rng, 42));
    selected_index = SIZE_MAX;
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_SUCCESS);
    assert(selected_index == 0);
}

static void test_exact_tie_uses_lower_population_index(void)
{
    selection_fixture_t fixture = {0};
    evo_rng_t probe = {0};
    evo_rng_t rng = {0};
    bool saw_lower = false;
    bool saw_upper = false;
    size_t selected_index = SIZE_MAX;

    fixture_initialize(&fixture, 5, 5, 67);
    for (size_t index = 0;
         index < fixture.population.population_size;
         ++index) {
        fixture_set_candidate(&fixture, index, false, 0.0);
    }
    fixture_set_candidate(&fixture, 1, true, 9.0);
    fixture_set_candidate(&fixture, 3, true, 9.0);
    fixture_finalize(&fixture);

    assert(evo_rng_seed(&probe, 42));
    for (size_t draw = 0;
         draw < fixture.config.tournament_size;
         ++draw) {
        size_t ordinal = SIZE_MAX;

        assert(evo_rng_uniform_index(&probe, 2, &ordinal));
        saw_lower = saw_lower || ordinal == 0;
        saw_upper = saw_upper || ordinal == 1;
    }
    assert(saw_lower);
    assert(saw_upper);

    assert(evo_rng_seed(&rng, 42));
    assert(evo_population_select_tournament(
               &fixture.config,
               &fixture.population,
               &rng,
               &selected_index) == EVO_SUCCESS);
    assert(selected_index == 1);
}

int main(void)
{
    test_invalid_arguments_preserve_state();
    test_tournament_bounds_and_unseeded_rng();
    test_inconsistent_population_rejection();
    test_all_invalid_population();
    test_valid_only_selection_and_replay();
    test_fixed_vector_and_single_draw();
    test_exact_tie_uses_lower_population_index();
    return 0;
}
