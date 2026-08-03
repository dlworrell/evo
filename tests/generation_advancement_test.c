#include "internal/generation_advancement.h"

#include "internal/child_evaluation.h"
#include "internal/child_pair.h"
#include "internal/child_single.h"
#include "internal/child_tail.h"
#include "internal/elite.h"

#include <assert.h>

enum {
    TEST_POPULATION_SIZE = 3,
    TEST_GENOME_SIZE = 2,
    TEST_STORAGE_SIZE = TEST_POPULATION_SIZE * TEST_GENOME_SIZE
};

typedef struct advancement_fixture {
    evo_problem_t problem;
    evo_config_t config;
    evo_population_t parents;
    evo_population_t children;
    bool child_validity[TEST_POPULATION_SIZE];
    evo_fitness_t child_fitness[TEST_POPULATION_SIZE];
    const unsigned char *evaluation_base;
    size_t initialization_calls;
    size_t validation_calls;
    size_t evaluation_calls;
    size_t crossover_calls;
    size_t mutation_calls;
    bool capture_child_evaluation;
} advancement_fixture_t;

typedef struct population_snapshot {
    evo_population_t metadata;
    unsigned char genomes[TEST_STORAGE_SIZE];
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

static size_t child_index(const void *genome,
                          const advancement_fixture_t *fixture)
{
    const unsigned char *bytes = genome;
    const size_t offset =
        (size_t)(bytes - fixture->evaluation_base);

    assert(fixture->evaluation_base != NULL);
    assert(offset % TEST_GENOME_SIZE == 0);
    assert(offset / TEST_GENOME_SIZE < TEST_POPULATION_SIZE);
    return offset / TEST_GENOME_SIZE;
}

static void deterministic_initializer(void *genome, void *opaque)
{
    advancement_fixture_t *fixture = opaque;

    (void)genome;
    ++fixture->initialization_calls;
}

static bool deterministic_validator(const void *genome, void *opaque)
{
    advancement_fixture_t *fixture = opaque;

    if (!fixture->capture_child_evaluation) {
        return true;
    }

    ++fixture->validation_calls;
    return fixture->child_validity[child_index(genome, fixture)];
}

static evo_fitness_t deterministic_evaluator(const void *genome,
                                             void *opaque)
{
    advancement_fixture_t *fixture = opaque;

    if (!fixture->capture_child_evaluation) {
        const unsigned char *bytes = genome;
        return fitness_with_total((double)bytes[0]);
    }

    ++fixture->evaluation_calls;
    return fixture->child_fitness[child_index(genome, fixture)];
}

static void deterministic_crossover(const void *parent_a,
                                    const void *parent_b,
                                    void *child_a,
                                    void *child_b,
                                    void *opaque)
{
    const unsigned char *source_a = parent_a;
    const unsigned char *source_b = parent_b;
    unsigned char *destination_a = child_a;
    unsigned char *destination_b = child_b;
    advancement_fixture_t *fixture = opaque;

    ++fixture->crossover_calls;
    for (size_t offset = 0; offset < TEST_GENOME_SIZE; ++offset) {
        destination_a[offset] = source_a[offset];
        destination_b[offset] = source_b[offset];
    }
}

static void deterministic_mutator(void *genome,
                                  double mutation_rate,
                                  void *opaque)
{
    unsigned char *bytes = genome;
    advancement_fixture_t *fixture = opaque;

    assert(mutation_rate == 1.0);
    ++fixture->mutation_calls;
    bytes[0] ^= UINT8_C(0x5a);
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
    assert(population->odd_child_policy_version == 0);
    assert(population->elite_policy_version == 0);
    assert(population->singleton_child_policy_version == 0);
    assert(population->fitness_comparison_policy_version == 0);
    assert(population->diversity_policy_version == 0);
    assert(population->diversity_metric_version == 0);
    assert(population->diversity_pair_count == 0);
    assert(population->diversity_work_units == 0);
    assert(population->diversity == 0.0);
    assert(!population->diversity_uses_domain_distance);
    assert(!population->initialized);
    assert(!population->has_best);
    assert(!population->evaluated);
    assert(!population->elite_count_explicit);
}

static void snapshot_population(const evo_population_t *population,
                                population_snapshot_t *snapshot)
{
    assert(population->storage_bytes <= TEST_STORAGE_SIZE);
    assert(population->population_size <= TEST_POPULATION_SIZE);
    assert(population->evaluations != NULL);
    snapshot->metadata = *population;
    for (size_t index = 0; index < TEST_STORAGE_SIZE; ++index) {
        if (index < population->storage_bytes) {
            snapshot->genomes[index] = population->genomes[index];
        }
    }
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        if (index < population->population_size) {
            snapshot->evaluations[index] = population->evaluations[index];
        }
    }
}

static void assert_population_matches_snapshot(
    const evo_population_t *population,
    const population_snapshot_t *snapshot)
{
    const evo_population_t *before = &snapshot->metadata;

    assert(population->genomes == before->genomes);
    assert(population->evaluations == before->evaluations);
    assert(population->population_size == before->population_size);
    assert(population->genome_size == before->genome_size);
    assert(population->storage_bytes == before->storage_bytes);
    assert(population->evaluation_bytes == before->evaluation_bytes);
    assert(population->valid_count == before->valid_count);
    assert(population->best_index == before->best_index);
    assert(population->produced_count == before->produced_count);
    assert(population->elite_count == before->elite_count);
    assert(population->elite_source_valid_count ==
           before->elite_source_valid_count);
    assert(population->initialization_seed == before->initialization_seed);
    assert(population->source_generation == before->source_generation);
    assert(population->rng_algorithm_version ==
           before->rng_algorithm_version);
    assert(population->operator_seed_schedule_version ==
           before->operator_seed_schedule_version);
    assert(population->odd_child_policy_version ==
           before->odd_child_policy_version);
    assert(population->elite_policy_version ==
           before->elite_policy_version);
    assert(population->singleton_child_policy_version ==
           before->singleton_child_policy_version);
    assert(population->fitness_comparison_policy_version ==
           before->fitness_comparison_policy_version);
    assert(population->diversity_policy_version ==
           before->diversity_policy_version);
    assert(population->diversity_metric_version ==
           before->diversity_metric_version);
    assert(population->diversity_pair_count ==
           before->diversity_pair_count);
    assert(population->diversity_work_units ==
           before->diversity_work_units);
    assert(population->diversity == before->diversity);
    assert(population->diversity_uses_domain_distance ==
           before->diversity_uses_domain_distance);
    assert(population->initialized == before->initialized);
    assert(population->has_best == before->has_best);
    assert(population->evaluated == before->evaluated);
    assert(population->elite_count_explicit ==
           before->elite_count_explicit);

    for (size_t index = 0; index < TEST_STORAGE_SIZE; ++index) {
        if (index < population->storage_bytes) {
            assert(population->genomes[index] == snapshot->genomes[index]);
        }
    }
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        if (index < population->population_size) {
            const evo_candidate_evaluation_t *evaluation =
                &population->evaluations[index];
            const evo_candidate_evaluation_t *expected =
                &snapshot->evaluations[index];

            assert(evaluation->valid == expected->valid);
            assert(evaluation->evaluated == expected->evaluated);
            assert_fitness_equal(&evaluation->fitness, &expected->fitness);
        }
    }
}

static evo_generation_advancement_evidence_t sentinel_evidence(void)
{
    return (evo_generation_advancement_evidence_t){
        .population_size = 29,
        .valid_count = 31,
        .best_index = 37,
        .elite_count = 39,
        .elite_source_valid_count = 40,
        .previous_generation = 41,
        .completed_generation = 43,
        .operator_seed_schedule_version = 47,
        .odd_child_policy_version = 53,
        .elite_policy_version = 55,
        .singleton_child_policy_version = 57,
        .policy_version = 59,
        .has_best = true,
        .elite_count_explicit = true,
        .complete = true,
    };
}

static void assert_evidence_equal(
    const evo_generation_advancement_evidence_t *left,
    const evo_generation_advancement_evidence_t *right)
{
    assert(left->population_size == right->population_size);
    assert(left->valid_count == right->valid_count);
    assert(left->best_index == right->best_index);
    assert(left->elite_count == right->elite_count);
    assert(left->elite_source_valid_count ==
           right->elite_source_valid_count);
    assert(left->previous_generation == right->previous_generation);
    assert(left->completed_generation == right->completed_generation);
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
    assert(left->policy_version == right->policy_version);
    assert(left->has_best == right->has_best);
    assert(left->elite_count_explicit ==
           right->elite_count_explicit);
    assert(left->complete == right->complete);
}

static void configure_child_results(advancement_fixture_t *fixture,
                                    bool all_invalid)
{
    for (size_t index = 0; index < TEST_POPULATION_SIZE; ++index) {
        fixture->child_validity[index] = !all_invalid;
        fixture->child_fitness[index] =
            fitness_with_total((double)(index + 10));
    }
}

static void create_evaluated_child(advancement_fixture_t *fixture,
                                   uint64_t source_generation,
                                   bool all_invalid)
{
    evo_child_pair_evidence_t pair = {0};
    evo_child_single_evidence_t single = {0};
    evo_elite_evidence_t elite = {0};
    evo_child_evaluation_evidence_t evaluation = {0};
    size_t requested_count = 0;
    size_t effective_count = 0;
    size_t offspring_count = 0;

    configure_child_results(fixture, all_invalid);
    fixture->capture_child_evaluation = false;
    assert(evo_child_population_create(&fixture->problem,
                                       &fixture->config,
                                       &fixture->parents,
                                       &fixture->children) == EVO_SUCCESS);
    assert(evo_elite_policy_counts(&fixture->config,
                                   fixture->parents.valid_count,
                                   &requested_count,
                                   &effective_count,
                                   &offspring_count) == EVO_SUCCESS);
    (void)requested_count;
    (void)effective_count;
    for (size_t pair_index = 0;
         pair_index < offspring_count / 2;
         ++pair_index) {
        assert(evo_child_pair_produce(&fixture->problem,
                                      &fixture->config,
                                      fixture,
                                      &fixture->parents,
                                      source_generation,
                                      pair_index,
                                      &fixture->children,
                                      &pair) == EVO_SUCCESS);
    }
    if (offspring_count % 2 != 0) {
        assert(evo_child_single_produce(&fixture->problem,
                                        &fixture->config,
                                        fixture,
                                        &fixture->parents,
                                        source_generation,
                                        &fixture->children,
                                        &single) == EVO_SUCCESS);
    }
    assert(evo_elite_population_complete(&fixture->problem,
                                         &fixture->config,
                                         &fixture->parents,
                                         source_generation,
                                         &fixture->children,
                                         &elite) == EVO_SUCCESS);

    fixture->evaluation_base = fixture->children.genomes;
    fixture->capture_child_evaluation = true;
    assert(evo_child_population_evaluate(&fixture->problem,
                                         &fixture->config,
                                         fixture,
                                         source_generation,
                                         &fixture->children,
                                         &evaluation) == EVO_SUCCESS);
    fixture->capture_child_evaluation = false;
}

static void fixture_initialize(advancement_fixture_t *fixture,
                               bool all_invalid_child)
{
    *fixture = (advancement_fixture_t){0};
    fixture->problem = (evo_problem_t){
        .genome_size = TEST_GENOME_SIZE,
        .initialize = deterministic_initializer,
        .mutate = deterministic_mutator,
        .crossover = deterministic_crossover,
        .evaluate = deterministic_evaluator,
        .is_valid = deterministic_validator,
    };
    fixture->config = (evo_config_t){
        .population_size = TEST_POPULATION_SIZE,
        .generation_limit = 8,
        .tournament_size = TEST_POPULATION_SIZE,
        .crossover_rate = 1.0,
        .mutation_rate = 1.0,
        .random_seed = 42,
        .max_genome_bytes = TEST_GENOME_SIZE,
        .max_population_bytes = TEST_STORAGE_SIZE,
        .max_evaluation_bytes =
            TEST_POPULATION_SIZE *
            sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes = TEST_STORAGE_SIZE,
        .max_diversity_work = SIZE_MAX,
    };

    assert(evo_population_create(&fixture->problem,
                                 &fixture->config,
                                 &fixture->parents) == EVO_SUCCESS);
    assert(evo_population_initialize(&fixture->problem,
                                     &fixture->config,
                                     fixture,
                                     &fixture->parents) == EVO_SUCCESS);
    assert(evo_population_evaluate(&fixture->problem,
                                   &fixture->config,
                                   fixture,
                                   &fixture->parents) == EVO_SUCCESS);
    create_evaluated_child(fixture, 0, all_invalid_child);
}

static void fixture_destroy(advancement_fixture_t *fixture)
{
    evo_population_destroy(&fixture->children);
    evo_population_destroy(&fixture->parents);
}

static void test_generation_zero_promotion_preserves_child_ownership(void)
{
    advancement_fixture_t fixture = {0};
    evo_generation_advancement_evidence_t evidence = {0};
    population_snapshot_t child_before = {0};
    unsigned char *child_genomes = NULL;
    evo_candidate_evaluation_t *child_evaluations = NULL;
    size_t valid_count = 0;

    fixture_initialize(&fixture, false);
    snapshot_population(&fixture.children, &child_before);
    child_genomes = fixture.children.genomes;
    child_evaluations = fixture.children.evaluations;

    {
        const size_t before_validation = fixture.validation_calls;
        const size_t before_evaluation = fixture.evaluation_calls;
        const size_t before_crossover = fixture.crossover_calls;
        const size_t before_mutation = fixture.mutation_calls;

        assert(evo_population_advance_generation(&fixture.problem,
                                                 &fixture.config,
                                                 0,
                                                 &fixture.parents,
                                                 &fixture.children,
                                                 &evidence) == EVO_SUCCESS);
        assert(fixture.validation_calls == before_validation);
        assert(fixture.evaluation_calls == before_evaluation);
        assert(fixture.crossover_calls == before_crossover);
        assert(fixture.mutation_calls == before_mutation);
    }

    assert_population_empty(&fixture.children);
    assert(fixture.parents.genomes == child_genomes);
    assert(fixture.parents.evaluations == child_evaluations);
    assert_population_matches_snapshot(&fixture.parents, &child_before);
    assert(evo_population_validate_completed(&fixture.config,
                                             &fixture.parents,
                                             &valid_count));
    assert(valid_count == TEST_POPULATION_SIZE);
    assert(evidence.population_size == TEST_POPULATION_SIZE);
    assert(evidence.valid_count == TEST_POPULATION_SIZE);
    assert(evidence.best_index == TEST_POPULATION_SIZE - 1);
    assert(evidence.previous_generation == 0);
    assert(evidence.completed_generation == 1);
    assert(evidence.operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(evidence.odd_child_policy_version ==
           EVO_ODD_CHILD_POLICY_VERSION);
    assert(evidence.elite_count == 1);
    assert(evidence.elite_source_valid_count == TEST_POPULATION_SIZE);
    assert(evidence.elite_policy_version == EVO_ELITE_POLICY_VERSION);
    assert(evidence.singleton_child_policy_version == 0);
    assert(!evidence.elite_count_explicit);
    assert(evidence.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert(evidence.diversity_policy_version ==
           EVO_DIVERSITY_POLICY_VERSION);
    assert(evidence.diversity_metric_version ==
           EVO_BYTE_DIVERSITY_METRIC_VERSION);
    assert(evidence.policy_version ==
           EVO_GENERATION_ADVANCEMENT_POLICY_VERSION);
    assert(evidence.has_best);
    assert(evidence.complete);

    assert(evo_child_population_create(&fixture.problem,
                                       &fixture.config,
                                       &fixture.parents,
                                       &fixture.children) == EVO_SUCCESS);
    evo_population_destroy(&fixture.children);
    fixture_destroy(&fixture);
}

static void test_later_generation_lineage_and_reuse(void)
{
    advancement_fixture_t fixture = {0};
    evo_generation_advancement_evidence_t first = {0};
    evo_generation_advancement_evidence_t second = {0};
    evo_generation_advancement_evidence_t rejected =
        sentinel_evidence();
    const evo_generation_advancement_evidence_t rejected_before =
        rejected;
    population_snapshot_t parent_before = {0};
    population_snapshot_t child_before = {0};

    fixture_initialize(&fixture, false);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             &first) == EVO_SUCCESS);
    create_evaluated_child(&fixture, 1, false);
    snapshot_population(&fixture.parents, &parent_before);
    snapshot_population(&fixture.children, &child_before);

    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             2,
                                             &fixture.parents,
                                             &fixture.children,
                                             &rejected) == EVO_ERROR_STATE);
    assert_population_matches_snapshot(&fixture.parents, &parent_before);
    assert_population_matches_snapshot(&fixture.children, &child_before);
    assert_evidence_equal(&rejected, &rejected_before);

    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             1,
                                             &fixture.parents,
                                             &fixture.children,
                                             &second) == EVO_SUCCESS);
    assert_population_empty(&fixture.children);
    assert(fixture.parents.source_generation == 1);
    assert(!fixture.parents.initialized);
    assert(second.previous_generation == 1);
    assert(second.completed_generation == 2);
    assert(second.complete);
    fixture_destroy(&fixture);
}

static void test_all_invalid_child_is_promotable(void)
{
    advancement_fixture_t fixture = {0};
    evo_generation_advancement_evidence_t evidence = {0};
    size_t valid_count = TEST_POPULATION_SIZE;

    fixture_initialize(&fixture, true);
    assert(!fixture.children.has_best);
    assert(fixture.children.valid_count == 0);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             &evidence) == EVO_SUCCESS);
    assert_population_empty(&fixture.children);
    assert(evo_population_validate_completed(&fixture.config,
                                             &fixture.parents,
                                             &valid_count));
    assert(valid_count == 0);
    assert(!fixture.parents.has_best);
    assert(fixture.parents.best_index == 0);
    assert(evidence.valid_count == 0);
    assert(evidence.best_index == 0);
    assert(!evidence.has_best);
    assert(evidence.complete);
    assert(evo_child_population_create(&fixture.problem,
                                       &fixture.config,
                                       &fixture.parents,
                                       &fixture.children) == EVO_SUCCESS);
    fixture_destroy(&fixture);
}

static void test_rejections_preserve_both_populations_and_evidence(void)
{
    advancement_fixture_t fixture = {0};
    evo_generation_advancement_evidence_t evidence =
        sentinel_evidence();
    const evo_generation_advancement_evidence_t evidence_before =
        evidence;
    population_snapshot_t parent_before = {0};
    population_snapshot_t child_before = {0};
    unsigned char *owned_child_genomes = NULL;
    uint64_t child_source_generation = 0;
    bool child_evaluated = false;

    fixture_initialize(&fixture, false);
    snapshot_population(&fixture.parents, &parent_before);
    snapshot_population(&fixture.children, &child_before);

    assert(evo_population_advance_generation(NULL,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_advance_generation(&fixture.problem,
                                             NULL,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             NULL,
                                             &fixture.children,
                                             &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             NULL,
                                             &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.parents,
                                             &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_advance_generation(
               &fixture.problem,
               &fixture.config,
               0,
               &fixture.parents,
               &fixture.children,
               (evo_generation_advancement_evidence_t *)(void *)&fixture.parents) == EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_advance_generation(
               &fixture.problem,
               &fixture.config,
               0,
               &fixture.parents,
               &fixture.children,
               (evo_generation_advancement_evidence_t *)(void *)fixture.children.genomes) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             UINT64_MAX,
                                             &fixture.parents,
                                             &fixture.children,
                                             &evidence) ==
           EVO_ERROR_RESOURCE_LIMIT);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             1,
                                             &fixture.parents,
                                             &fixture.children,
                                             &evidence) == EVO_ERROR_STATE);

    child_source_generation = fixture.children.source_generation;
    fixture.children.source_generation = 7;
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             &evidence) == EVO_ERROR_STATE);
    fixture.children.source_generation = child_source_generation;

    child_evaluated = fixture.children.evaluated;
    fixture.children.evaluated = false;
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             &evidence) == EVO_ERROR_STATE);
    fixture.children.evaluated = child_evaluated;

    owned_child_genomes = fixture.children.genomes;
    fixture.children.genomes = fixture.parents.genomes;
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             &evidence) == EVO_ERROR_STATE);
    fixture.children.genomes = owned_child_genomes;

    assert_population_matches_snapshot(&fixture.parents, &parent_before);
    assert_population_matches_snapshot(&fixture.children, &child_before);
    assert_evidence_equal(&evidence, &evidence_before);
    fixture_destroy(&fixture);
}

static void test_repeated_advancement_rejects_empty_child(void)
{
    advancement_fixture_t fixture = {0};
    evo_generation_advancement_evidence_t first = {0};
    evo_generation_advancement_evidence_t repeated =
        sentinel_evidence();
    const evo_generation_advancement_evidence_t repeated_before =
        repeated;
    population_snapshot_t parent_before = {0};

    fixture_initialize(&fixture, false);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             &first) == EVO_SUCCESS);
    snapshot_population(&fixture.parents, &parent_before);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             1,
                                             &fixture.parents,
                                             &fixture.children,
                                             &repeated) == EVO_ERROR_STATE);
    assert_population_matches_snapshot(&fixture.parents, &parent_before);
    assert_population_empty(&fixture.children);
    assert_evidence_equal(&repeated, &repeated_before);
    fixture_destroy(&fixture);
}

static void test_explicit_elite_provenance_is_promoted(void)
{
    advancement_fixture_t fixture = {0};
    evo_generation_advancement_evidence_t evidence = {0};

    fixture_initialize(&fixture, false);
    evo_population_destroy(&fixture.children);
    fixture.config.elite_count_enabled = true;
    fixture.config.elite_count = 2;
    create_evaluated_child(&fixture, 0, false);

    assert(fixture.children.elite_count == 2);
    assert(fixture.children.elite_source_valid_count ==
           TEST_POPULATION_SIZE);
    assert(fixture.children.elite_policy_version ==
           EVO_ELITE_POLICY_VERSION);
    assert(fixture.children.singleton_child_policy_version ==
           EVO_SINGLETON_CHILD_POLICY_VERSION);
    assert(fixture.children.elite_count_explicit);
    assert(evo_population_advance_generation(&fixture.problem,
                                             &fixture.config,
                                             0,
                                             &fixture.parents,
                                             &fixture.children,
                                             &evidence) == EVO_SUCCESS);
    assert(evidence.elite_count == 2);
    assert(evidence.elite_source_valid_count ==
           TEST_POPULATION_SIZE);
    assert(evidence.elite_policy_version == EVO_ELITE_POLICY_VERSION);
    assert(evidence.singleton_child_policy_version ==
           EVO_SINGLETON_CHILD_POLICY_VERSION);
    assert(evidence.elite_count_explicit);
    assert(evidence.policy_version ==
           EVO_GENERATION_ADVANCEMENT_POLICY_VERSION);
    assert_population_empty(&fixture.children);
    fixture_destroy(&fixture);
}

int main(void)
{
    test_generation_zero_promotion_preserves_child_ownership();
    test_later_generation_lineage_and_reuse();
    test_all_invalid_child_is_promotable();
    test_rejections_preserve_both_populations_and_evidence();
    test_repeated_advancement_rejects_empty_child();
    test_explicit_elite_provenance_is_promoted();
    return 0;
}
