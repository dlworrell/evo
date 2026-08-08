#include "internal/checkpoint.h"

#include "internal/bounded_run.h"
#include "internal/child_single.h"
#include "internal/child_tail.h"
#include "internal/crossover.h"
#include "internal/diversity.h"
#include "internal/elite.h"
#include "internal/fitness.h"
#include "internal/mutation.h"
#include "internal/population_evaluation.h"
#include "internal/result_storage.h"
#include "internal/rng.h"
#include "internal/selection.h"
#include "internal/secure_erasure.h"
#include "internal/statistics.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

enum {
    EVO_CHECKPOINT_HEADER_SIZE = 144,
    EVO_CHECKPOINT_SECTION_COUNT = 6,
    EVO_CHECKPOINT_CHECKSUM_OFFSET = 132,
    EVO_CHECKPOINT_EVALUATION_RECORD_SIZE = 58,
    EVO_CHECKPOINT_CONFIGURATION_BUFFER_SIZE = 512
};

static const unsigned char checkpoint_magic[8] = {
    'E', 'V', 'O', 'C', 'K', 'P', 'T', '1'};

_Static_assert(sizeof(double) == sizeof(uint64_t),
               "checkpoint format requires binary64-sized double");
_Static_assert(DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
               "checkpoint format requires IEEE-754 binary64 double");

typedef union checkpoint_double_bits {
    double value;
    uint64_t bits;
} checkpoint_double_bits_t;

typedef struct checkpoint_writer {
    unsigned char *bytes;
    size_t capacity;
    size_t offset;
    bool valid;
} checkpoint_writer_t;

typedef struct checkpoint_reader {
    const unsigned char *bytes;
    size_t size;
    size_t offset;
    bool valid;
} checkpoint_reader_t;

typedef struct checkpoint_layout {
    size_t total_size;
    size_t configuration_offset;
    size_t configuration_size;
    size_t state_offset;
    size_t state_size;
    size_t statistics_offset;
    size_t statistics_size;
    size_t evaluations_offset;
    size_t evaluations_size;
    size_t genomes_offset;
    size_t genomes_size;
    size_t best_genome_offset;
    size_t best_genome_size;
    uint64_t configuration_fingerprint;
    uint32_t integrity_algorithm;
    uint32_t integrity_value;
} checkpoint_layout_t;

typedef struct checkpoint_state_projection {
    evo_run_state_t run;
    uint32_t checkpoint_rng_algorithm_version;
    uint32_t checkpoint_operator_seed_schedule_version;
    uint32_t checkpoint_selection_policy_version;
    uint32_t checkpoint_byte_operator_policy_version;
    uint32_t checkpoint_bounded_run_policy_version;
    size_t population_size;
    size_t genome_size;
    size_t storage_bytes;
    size_t evaluation_bytes;
    size_t valid_count;
    size_t best_index;
    size_t produced_count;
    size_t elite_count;
    size_t elite_source_valid_count;
    uint64_t initialization_seed;
    uint64_t source_generation;
    uint32_t secure_erasure_policy_version;
    evo_secure_erasure_backend_t secure_erasure_backend;
    uint32_t rng_algorithm_version;
    uint32_t operator_seed_schedule_version;
    uint32_t selection_policy_version;
    evo_selection_policy_t selection_policy;
    uint32_t byte_operator_policy_version;
    evo_crossover_operator_t crossover_operator;
    evo_mutation_operator_t mutation_operator;
    double mutation_rate_used;
    uint32_t odd_child_policy_version;
    uint32_t elite_policy_version;
    uint32_t singleton_child_policy_version;
    uint32_t fitness_comparison_policy_version;
    uint32_t diversity_policy_version;
    uint32_t diversity_metric_version;
    size_t diversity_pair_count;
    size_t diversity_work_units;
    double diversity;
    bool population_initialized;
    bool has_best;
    bool evaluated;
    bool elite_count_explicit;
    bool diversity_uses_domain_distance;
    bool secure_erasure_enabled;
    evo_fitness_t global_best_fitness;
    size_t result_best_genome_size;
    uint32_t result_secure_erasure_policy_version;
    evo_secure_erasure_backend_t result_secure_erasure_backend;
    bool result_secure_erasure_enabled;
    uint64_t result_random_seed;
} checkpoint_state_projection_t;

static bool checked_size_add(size_t left, size_t right, size_t *sum)
{
    if (sum == NULL || right > SIZE_MAX - left) {
        return false;
    }
    *sum = left + right;
    return true;
}

static bool checked_size_multiply(size_t left,
                                  size_t right,
                                  size_t *product)
{
    if (product == NULL || (left != 0 && right > SIZE_MAX / left)) {
        return false;
    }
    *product = left * right;
    return true;
}

static bool u64_to_size(uint64_t value, size_t *converted)
{
    if (converted == NULL) {
        return false;
    }
#if SIZE_MAX < UINT64_MAX
    if (value > (uint64_t)SIZE_MAX) {
        return false;
    }
#endif
    *converted = (size_t)value;
    return true;
}

static bool size_to_u64(size_t value, uint64_t *converted)
{
    if (converted == NULL) {
        return false;
    }
#if SIZE_MAX > UINT64_MAX
    if (value > (size_t)UINT64_MAX) {
        return false;
    }
#endif
    *converted = (uint64_t)value;
    return true;
}

static bool writer_reserve(checkpoint_writer_t *writer, size_t count)
{
    size_t end = 0;

    if (writer == NULL || !writer->valid ||
        !checked_size_add(writer->offset, count, &end) ||
        (writer->bytes != NULL && end > writer->capacity)) {
        if (writer != NULL) {
            writer->valid = false;
        }
        return false;
    }
    return true;
}

static void write_u8(checkpoint_writer_t *writer, uint8_t value)
{
    if (!writer_reserve(writer, 1)) {
        return;
    }
    if (writer->bytes != NULL) {
        writer->bytes[writer->offset] = value;
    }
    ++writer->offset;
}

static void write_u32(checkpoint_writer_t *writer, uint32_t value)
{
    if (!writer_reserve(writer, 4)) {
        return;
    }
    if (writer->bytes != NULL) {
        for (size_t index = 0; index < 4; ++index) {
            writer->bytes[writer->offset + index] =
                (unsigned char)(value >> (index * 8));
        }
    }
    writer->offset += 4;
}

static void write_u64(checkpoint_writer_t *writer, uint64_t value)
{
    if (!writer_reserve(writer, 8)) {
        return;
    }
    if (writer->bytes != NULL) {
        for (size_t index = 0; index < 8; ++index) {
            writer->bytes[writer->offset + index] =
                (unsigned char)(value >> (index * 8));
        }
    }
    writer->offset += 8;
}

static void write_size(checkpoint_writer_t *writer, size_t value)
{
    uint64_t converted = 0;

    if (!size_to_u64(value, &converted)) {
        writer->valid = false;
        return;
    }
    write_u64(writer, converted);
}

static void write_bool(checkpoint_writer_t *writer, bool value)
{
    write_u8(writer, value ? UINT8_C(1) : UINT8_C(0));
}

static void write_double(checkpoint_writer_t *writer, double value)
{
    checkpoint_double_bits_t converted = {.value = value};

    write_u64(writer, converted.bits);
}

static void write_fitness(checkpoint_writer_t *writer,
                          const evo_fitness_t *fitness)
{
    write_double(writer, fitness->correctness);
    write_double(writer, fitness->performance);
    write_double(writer, fitness->memory_use);
    write_double(writer, fitness->reliability);
    write_double(writer, fitness->maintainability);
    write_double(writer, fitness->constraint_penalty);
    write_double(writer, fitness->total);
}

static bool reader_reserve(checkpoint_reader_t *reader, size_t count)
{
    size_t end = 0;

    if (reader == NULL || !reader->valid ||
        !checked_size_add(reader->offset, count, &end) || end > reader->size) {
        if (reader != NULL) {
            reader->valid = false;
        }
        return false;
    }
    return true;
}

static uint8_t read_u8(checkpoint_reader_t *reader)
{
    uint8_t value = 0;

    if (!reader_reserve(reader, 1)) {
        return 0;
    }
    value = reader->bytes[reader->offset];
    ++reader->offset;
    return value;
}

static uint32_t read_u32(checkpoint_reader_t *reader)
{
    uint32_t value = 0;

    if (!reader_reserve(reader, 4)) {
        return 0;
    }
    for (size_t index = 0; index < 4; ++index) {
        value |= (uint32_t)reader->bytes[reader->offset + index]
                 << (index * 8);
    }
    reader->offset += 4;
    return value;
}

static uint64_t read_u64(checkpoint_reader_t *reader)
{
    uint64_t value = 0;

    if (!reader_reserve(reader, 8)) {
        return 0;
    }
    for (size_t index = 0; index < 8; ++index) {
        value |= (uint64_t)reader->bytes[reader->offset + index]
                 << (index * 8);
    }
    reader->offset += 8;
    return value;
}

static size_t read_size(checkpoint_reader_t *reader)
{
    size_t value = 0;

    if (!u64_to_size(read_u64(reader), &value)) {
        reader->valid = false;
    }
    return value;
}

static bool read_bool(checkpoint_reader_t *reader)
{
    const uint8_t value = read_u8(reader);

    if (value > UINT8_C(1)) {
        reader->valid = false;
    }
    return value != 0;
}

static double read_double(checkpoint_reader_t *reader)
{
    checkpoint_double_bits_t converted = {.bits = read_u64(reader)};

    return converted.value;
}

static evo_fitness_t read_fitness(checkpoint_reader_t *reader)
{
    evo_fitness_t fitness = {0};

    fitness.correctness = read_double(reader);
    fitness.performance = read_double(reader);
    fitness.memory_use = read_double(reader);
    fitness.reliability = read_double(reader);
    fitness.maintainability = read_double(reader);
    fitness.constraint_penalty = read_double(reader);
    fitness.total = read_double(reader);
    return fitness;
}

static uint32_t read_u32_at(const unsigned char *bytes, size_t offset)
{
    uint32_t value = 0;

    for (size_t index = 0; index < 4; ++index) {
        value |= (uint32_t)bytes[offset + index] << (index * 8);
    }
    return value;
}

static uint64_t read_u64_at(const unsigned char *bytes, size_t offset)
{
    uint64_t value = 0;

    for (size_t index = 0; index < 8; ++index) {
        value |= (uint64_t)bytes[offset + index] << (index * 8);
    }
    return value;
}

static void write_u32_at(unsigned char *bytes,
                         size_t offset,
                         uint32_t value)
{
    for (size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = (unsigned char)(value >> (index * 8));
    }
}

static void write_u64_at(unsigned char *bytes,
                         size_t offset,
                         uint64_t value)
{
    for (size_t index = 0; index < 8; ++index) {
        bytes[offset + index] = (unsigned char)(value >> (index * 8));
    }
}

static uint64_t fingerprint_bytes(const unsigned char *bytes, size_t size)
{
    uint64_t fingerprint = UINT64_C(14695981039346656037);

    for (size_t index = 0; index < size; ++index) {
        fingerprint ^= (uint64_t)bytes[index];
        fingerprint *= UINT64_C(1099511628211);
    }
    return fingerprint;
}

static uint32_t checkpoint_crc32(const unsigned char *bytes, size_t size)
{
    uint32_t crc = UINT32_MAX;

    for (size_t index = 0; index < size; ++index) {
        const unsigned char byte =
            index >= EVO_CHECKPOINT_CHECKSUM_OFFSET &&
                    index < EVO_CHECKPOINT_CHECKSUM_OFFSET + 4
                ? 0
                : bytes[index];

        crc ^= (uint32_t)byte;
        for (size_t bit = 0; bit < 8; ++bit) {
            const uint32_t mask = UINT32_C(0) - (crc & UINT32_C(1));
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static void encode_configuration(checkpoint_writer_t *writer,
                                 const evo_problem_t *problem,
                                 const evo_config_t *config)
{
    write_u32(writer, EVO_CHECKPOINT_CONFIGURATION_VIEW_VERSION);
    write_size(writer, problem->genome_size);
    write_size(writer, config->population_size);
    write_size(writer, config->generation_limit);
    write_size(writer, config->tournament_size);
    write_double(writer, config->crossover_rate);
    write_double(writer, config->mutation_rate);
    write_u64(writer, config->random_seed);
    write_size(writer, config->max_genome_bytes);
    write_size(writer, config->max_population_bytes);
    write_size(writer, config->max_evaluation_bytes);
    write_size(writer, config->max_child_population_bytes);
    write_size(writer, config->max_diversity_work);
    write_bool(writer, config->fitness_target_enabled);
    write_double(writer, config->fitness_target);
    write_bool(writer, config->stagnation_enabled);
    write_double(writer, config->improvement_tolerance);
    write_size(writer, config->stagnation_patience);
    write_bool(writer, config->diversity_floor_enabled);
    write_double(writer, config->diversity_floor);
    write_bool(writer, config->elite_count_enabled);
    write_size(writer, config->elite_count);
    write_u32(writer, (uint32_t)config->selection_policy);
    write_size(writer, config->rank_base_weight);
    write_size(writer, config->rank_step_weight);
    write_u32(writer, (uint32_t)config->crossover_operator);
    write_u32(writer, (uint32_t)config->mutation_operator);
    write_bool(writer, config->adaptive_mutation_enabled);
    write_double(writer, config->adaptive_mutation_min_rate);
    write_double(writer, config->adaptive_mutation_max_rate);
    write_double(writer, config->adaptive_mutation_step);
    write_double(writer, config->adaptive_mutation_diversity_threshold);
    write_bool(writer, config->adaptive_mutation_reset_on_improvement);
    write_bool(writer, config->secure_erasure_enabled);
    write_u32(writer, problem->genome_distance_version);
    write_u64(writer, problem->checkpoint_problem_identity);
    write_u64(writer, config->checkpoint_context_identity);
    write_bool(writer, problem->initialize != NULL);
    write_bool(writer, problem->mutate != NULL);
    write_bool(writer, problem->crossover != NULL);
    write_bool(writer, problem->evaluate != NULL);
    write_bool(writer, problem->is_valid != NULL);
    write_bool(writer, problem->genome_distance != NULL);
    write_bool(writer, config->generation_observer != NULL);
    write_bool(writer, config->generation_stop != NULL);
}

static bool decode_configuration(
    checkpoint_reader_t *reader,
    evo_checkpoint_configuration_view_t *configuration)
{
    *configuration = (evo_checkpoint_configuration_view_t){0};
    configuration->version = read_u32(reader);
    configuration->genome_size = read_size(reader);
    configuration->population_size = read_size(reader);
    configuration->generation_limit = read_size(reader);
    configuration->tournament_size = read_size(reader);
    configuration->crossover_rate = read_double(reader);
    configuration->mutation_rate = read_double(reader);
    configuration->random_seed = read_u64(reader);
    configuration->max_genome_bytes = read_size(reader);
    configuration->max_population_bytes = read_size(reader);
    configuration->max_evaluation_bytes = read_size(reader);
    configuration->max_child_population_bytes = read_size(reader);
    configuration->max_diversity_work = read_size(reader);
    configuration->fitness_target_enabled = read_bool(reader);
    configuration->fitness_target = read_double(reader);
    configuration->stagnation_enabled = read_bool(reader);
    configuration->improvement_tolerance = read_double(reader);
    configuration->stagnation_patience = read_size(reader);
    configuration->diversity_floor_enabled = read_bool(reader);
    configuration->diversity_floor = read_double(reader);
    configuration->elite_count_enabled = read_bool(reader);
    configuration->elite_count = read_size(reader);
    configuration->selection_policy =
        (evo_selection_policy_t)read_u32(reader);
    configuration->rank_base_weight = read_size(reader);
    configuration->rank_step_weight = read_size(reader);
    configuration->crossover_operator =
        (evo_crossover_operator_t)read_u32(reader);
    configuration->mutation_operator =
        (evo_mutation_operator_t)read_u32(reader);
    configuration->adaptive_mutation_enabled = read_bool(reader);
    configuration->adaptive_mutation_min_rate = read_double(reader);
    configuration->adaptive_mutation_max_rate = read_double(reader);
    configuration->adaptive_mutation_step = read_double(reader);
    configuration->adaptive_mutation_diversity_threshold =
        read_double(reader);
    configuration->adaptive_mutation_reset_on_improvement =
        read_bool(reader);
    configuration->secure_erasure_enabled = read_bool(reader);
    configuration->genome_distance_version = read_u32(reader);
    configuration->checkpoint_problem_identity = read_u64(reader);
    configuration->checkpoint_context_identity = read_u64(reader);
    configuration->initialize_callback_present = read_bool(reader);
    configuration->mutate_callback_present = read_bool(reader);
    configuration->crossover_callback_present = read_bool(reader);
    configuration->evaluate_callback_present = read_bool(reader);
    configuration->validity_callback_present = read_bool(reader);
    configuration->distance_callback_present = read_bool(reader);
    configuration->generation_observer_present = read_bool(reader);
    configuration->generation_stop_present = read_bool(reader);

    return reader->valid && reader->offset == reader->size &&
           configuration->version ==
               EVO_CHECKPOINT_CONFIGURATION_VIEW_VERSION &&
           configuration->genome_size != 0 &&
           configuration->population_size != 0 &&
           configuration->checkpoint_problem_identity != 0 &&
           configuration->checkpoint_context_identity != 0 &&
           configuration->evaluate_callback_present &&
           configuration->distance_callback_present ==
               (configuration->genome_distance_version != 0) &&
           (configuration->selection_policy == EVO_SELECTION_TOURNAMENT ||
            configuration->selection_policy == EVO_SELECTION_RANK) &&
           (configuration->crossover_operator == EVO_CROSSOVER_CONSUMER ||
            configuration->crossover_operator ==
                EVO_CROSSOVER_BYTE_ONE_POINT ||
            configuration->crossover_operator ==
                EVO_CROSSOVER_BYTE_TWO_POINT ||
            configuration->crossover_operator ==
                EVO_CROSSOVER_BYTE_UNIFORM) &&
           (configuration->mutation_operator == EVO_MUTATION_CONSUMER ||
            configuration->mutation_operator == EVO_MUTATION_BYTE_XOR);
}

static double checkpoint_distance_placeholder(const void *genome_a,
                                              const void *genome_b,
                                              size_t genome_size,
                                              void *context)
{
    (void)genome_a;
    (void)genome_b;
    (void)genome_size;
    (void)context;
    return 0.0;
}

static bool decoded_configuration_is_valid(
    const evo_checkpoint_configuration_view_t *source,
    evo_problem_t *problem,
    evo_config_t *config)
{
    size_t population_bytes = 0;
    size_t evaluation_bytes = 0;

    if (source == NULL || problem == NULL || config == NULL) {
        return false;
    }
    *problem = (evo_problem_t){
        .genome_size = source->genome_size,
        .genome_distance = source->distance_callback_present
                               ? checkpoint_distance_placeholder
                               : NULL,
        .genome_distance_version = source->genome_distance_version,
        .checkpoint_problem_identity =
            source->checkpoint_problem_identity,
    };
    *config = (evo_config_t){
        .population_size = source->population_size,
        .generation_limit = source->generation_limit,
        .tournament_size = source->tournament_size,
        .crossover_rate = source->crossover_rate,
        .mutation_rate = source->mutation_rate,
        .random_seed = source->random_seed,
        .max_genome_bytes = source->max_genome_bytes,
        .max_population_bytes = source->max_population_bytes,
        .max_evaluation_bytes = source->max_evaluation_bytes,
        .max_child_population_bytes =
            source->max_child_population_bytes,
        .max_diversity_work = source->max_diversity_work,
        .fitness_target_enabled = source->fitness_target_enabled,
        .fitness_target = source->fitness_target,
        .stagnation_enabled = source->stagnation_enabled,
        .improvement_tolerance = source->improvement_tolerance,
        .stagnation_patience = source->stagnation_patience,
        .diversity_floor_enabled = source->diversity_floor_enabled,
        .diversity_floor = source->diversity_floor,
        .elite_count_enabled = source->elite_count_enabled,
        .elite_count = source->elite_count,
        .selection_policy = source->selection_policy,
        .rank_base_weight = source->rank_base_weight,
        .rank_step_weight = source->rank_step_weight,
        .crossover_operator = source->crossover_operator,
        .mutation_operator = source->mutation_operator,
        .adaptive_mutation_enabled = source->adaptive_mutation_enabled,
        .adaptive_mutation_min_rate =
            source->adaptive_mutation_min_rate,
        .adaptive_mutation_max_rate =
            source->adaptive_mutation_max_rate,
        .adaptive_mutation_step = source->adaptive_mutation_step,
        .adaptive_mutation_diversity_threshold =
            source->adaptive_mutation_diversity_threshold,
        .adaptive_mutation_reset_on_improvement =
            source->adaptive_mutation_reset_on_improvement,
        .secure_erasure_enabled = source->secure_erasure_enabled,
        .checkpoint_context_identity =
            source->checkpoint_context_identity,
    };

    return source->evaluate_callback_present &&
           source->distance_callback_present ==
               (source->genome_distance_version != 0) &&
           source->genome_size <= source->max_genome_bytes &&
           checked_size_multiply(source->population_size,
                                 source->genome_size,
                                 &population_bytes) &&
           population_bytes <= source->max_population_bytes &&
           checked_size_multiply(source->population_size,
                                 sizeof(evo_candidate_evaluation_t),
                                 &evaluation_bytes) &&
           evaluation_bytes <= source->max_evaluation_bytes &&
           evo_bounded_run_validate_config(problem, config) == EVO_SUCCESS &&
           evo_selection_validate_config(config) == EVO_SUCCESS &&
           evo_crossover_operator_is_valid(source->crossover_operator) &&
           evo_mutation_operator_is_valid(source->mutation_operator);
}

static void encode_statistics(checkpoint_writer_t *writer,
                              const evo_generation_statistics_t *statistics)
{
    write_u32(writer, statistics->version);
    write_u64(writer, statistics->generation_index);
    write_size(writer, statistics->population_size);
    write_size(writer, statistics->valid_count);
    write_size(writer, statistics->invalid_count);
    write_size(writer, statistics->best_index);
    write_fitness(writer, &statistics->best_fitness);
    write_fitness(writer, &statistics->fitness_sums);
    write_bool(writer, statistics->has_best);
    write_u32(writer, statistics->fitness_comparison_policy_version);
    write_u32(writer, statistics->diversity_policy_version);
    write_u32(writer, statistics->diversity_metric_version);
    write_size(writer, statistics->diversity_pair_count);
    write_size(writer, statistics->diversity_work_units);
    write_double(writer, statistics->diversity);
    write_bool(writer, statistics->diversity_uses_domain_distance);
    write_u32(writer, statistics->adaptive_mutation_policy_version);
    write_double(writer, statistics->mutation_rate_prior);
    write_double(writer, statistics->mutation_rate_effective);
    write_double(writer, statistics->adaptive_mutation_min_rate);
    write_double(writer, statistics->adaptive_mutation_max_rate);
    write_double(writer, statistics->adaptive_mutation_step);
    write_double(writer,
                 statistics->adaptive_mutation_diversity_threshold);
    write_size(writer,
               statistics->adaptive_mutation_stagnant_generations);
    write_u32(writer, (uint32_t)statistics->mutation_adaptation_reason);
    write_bool(writer, statistics->adaptive_mutation_enabled);
    write_bool(writer, statistics->adaptive_mutation_low_diversity);
    write_bool(writer, statistics->adaptive_mutation_global_best_improved);
    write_bool(writer, statistics->adaptive_mutation_clamped_to_min);
    write_bool(writer, statistics->adaptive_mutation_clamped_to_max);
    write_bool(writer,
               statistics->adaptive_mutation_reset_on_improvement);
}

static bool fitness_is_zero(const evo_fitness_t *fitness)
{
    return fitness->correctness == 0.0 && fitness->performance == 0.0 &&
           fitness->memory_use == 0.0 && fitness->reliability == 0.0 &&
           fitness->maintainability == 0.0 &&
           fitness->constraint_penalty == 0.0 && fitness->total == 0.0;
}

static bool adaptation_reason_is_valid(
    evo_mutation_adaptation_reason_t reason)
{
    return reason >= EVO_MUTATION_ADAPTATION_NOT_APPLICABLE &&
           reason <= EVO_MUTATION_ADAPTATION_IMPROVEMENT_HOLD;
}

static bool decode_statistics(checkpoint_reader_t *reader,
                              evo_generation_statistics_t *statistics)
{
    *statistics = (evo_generation_statistics_t){0};
    statistics->version = read_u32(reader);
    statistics->generation_index = read_u64(reader);
    statistics->population_size = read_size(reader);
    statistics->valid_count = read_size(reader);
    statistics->invalid_count = read_size(reader);
    statistics->best_index = read_size(reader);
    statistics->best_fitness = read_fitness(reader);
    statistics->fitness_sums = read_fitness(reader);
    statistics->has_best = read_bool(reader);
    statistics->fitness_comparison_policy_version = read_u32(reader);
    statistics->diversity_policy_version = read_u32(reader);
    statistics->diversity_metric_version = read_u32(reader);
    statistics->diversity_pair_count = read_size(reader);
    statistics->diversity_work_units = read_size(reader);
    statistics->diversity = read_double(reader);
    statistics->diversity_uses_domain_distance = read_bool(reader);
    statistics->adaptive_mutation_policy_version = read_u32(reader);
    statistics->mutation_rate_prior = read_double(reader);
    statistics->mutation_rate_effective = read_double(reader);
    statistics->adaptive_mutation_min_rate = read_double(reader);
    statistics->adaptive_mutation_max_rate = read_double(reader);
    statistics->adaptive_mutation_step = read_double(reader);
    statistics->adaptive_mutation_diversity_threshold =
        read_double(reader);
    statistics->adaptive_mutation_stagnant_generations = read_size(reader);
    statistics->mutation_adaptation_reason =
        (evo_mutation_adaptation_reason_t)read_u32(reader);
    statistics->adaptive_mutation_enabled = read_bool(reader);
    statistics->adaptive_mutation_low_diversity = read_bool(reader);
    statistics->adaptive_mutation_global_best_improved = read_bool(reader);
    statistics->adaptive_mutation_clamped_to_min = read_bool(reader);
    statistics->adaptive_mutation_clamped_to_max = read_bool(reader);
    statistics->adaptive_mutation_reset_on_improvement = read_bool(reader);

    return reader->valid && reader->offset == reader->size &&
           statistics->version == EVO_GENERATION_STATISTICS_VERSION &&
           statistics->population_size != 0 &&
           statistics->valid_count <= statistics->population_size &&
           statistics->invalid_count ==
               statistics->population_size - statistics->valid_count &&
           statistics->has_best == (statistics->valid_count != 0) &&
           (statistics->has_best
                ? evo_fitness_evidence_is_valid(&statistics->best_fitness)
                : fitness_is_zero(&statistics->best_fitness)) &&
           evo_fitness_evidence_is_valid(&statistics->fitness_sums) &&
           statistics->fitness_comparison_policy_version ==
               EVO_FITNESS_COMPARISON_POLICY_VERSION &&
           statistics->diversity_policy_version ==
               EVO_DIVERSITY_POLICY_VERSION &&
           statistics->diversity_metric_version != 0 &&
           isfinite(statistics->diversity) && statistics->diversity >= 0.0 &&
           statistics->diversity <= 1.0 &&
           adaptation_reason_is_valid(
               statistics->mutation_adaptation_reason);
}

static void encode_state(checkpoint_writer_t *writer,
                         const evo_population_t *population,
                         const evo_result_t *result,
                         const evo_run_state_t *state)
{
    write_u32(writer, state->version);
    write_u64(writer, state->current_generation);
    write_u64(writer, state->best_generation);
    write_size(writer, state->best_population_index);
    write_u32(writer, (uint32_t)state->termination_reason);
    write_bool(writer, state->adaptive_mutation_applicable);
    write_bool(writer, state->initialized);
    write_double(writer, state->adaptive_mutation.effective_rate);
    write_size(writer, state->adaptive_mutation.stagnant_generations);
    write_bool(writer, state->adaptive_mutation.initialized);
    write_double(writer, state->stopping.significant_best_total);
    write_size(writer, state->stopping.stagnant_generations);
    write_bool(writer, state->stopping.initialized);
    write_u32(writer, EVO_RNG_ALGORITHM_VERSION);
    write_u32(writer, EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    write_u32(writer, EVO_SELECTION_POLICY_VERSION);
    write_u32(writer, EVO_BYTE_OPERATOR_POLICY_VERSION);
    write_u32(writer, EVO_BOUNDED_RUN_POLICY_VERSION);
    write_size(writer, population->population_size);
    write_size(writer, population->genome_size);
    write_size(writer, population->storage_bytes);
    write_size(writer, population->evaluation_bytes);
    write_size(writer, population->valid_count);
    write_size(writer, population->best_index);
    write_size(writer, population->produced_count);
    write_size(writer, population->elite_count);
    write_size(writer, population->elite_source_valid_count);
    write_u64(writer, population->initialization_seed);
    write_u64(writer, population->source_generation);
    write_u32(writer, population->secure_erasure_policy_version);
    write_u32(writer, (uint32_t)population->secure_erasure_backend);
    write_u32(writer, population->rng_algorithm_version);
    write_u32(writer, population->operator_seed_schedule_version);
    write_u32(writer, population->selection_policy_version);
    write_u32(writer, (uint32_t)population->selection_policy);
    write_u32(writer, population->byte_operator_policy_version);
    write_u32(writer, (uint32_t)population->crossover_operator);
    write_u32(writer, (uint32_t)population->mutation_operator);
    write_double(writer, population->mutation_rate_used);
    write_u32(writer, population->odd_child_policy_version);
    write_u32(writer, population->elite_policy_version);
    write_u32(writer, population->singleton_child_policy_version);
    write_u32(writer, population->fitness_comparison_policy_version);
    write_u32(writer, population->diversity_policy_version);
    write_u32(writer, population->diversity_metric_version);
    write_size(writer, population->diversity_pair_count);
    write_size(writer, population->diversity_work_units);
    write_double(writer, population->diversity);
    write_bool(writer, population->initialized);
    write_bool(writer, population->has_best);
    write_bool(writer, population->evaluated);
    write_bool(writer, population->elite_count_explicit);
    write_bool(writer, population->diversity_uses_domain_distance);
    write_bool(writer, population->secure_erasure_enabled);
    write_fitness(writer, &result->best_fitness);
    write_size(writer, result->best_genome_size);
    write_u32(writer, result->secure_erasure_policy_version);
    write_u32(writer, (uint32_t)result->secure_erasure_backend);
    write_bool(writer, result->secure_erasure_enabled);
    write_u64(writer, result->random_seed);
}

static bool termination_reason_is_valid(evo_termination_reason_t reason)
{
    return reason >= EVO_TERMINATION_NONE &&
           reason <= EVO_TERMINATION_STAGNATED;
}

static bool secure_backend_is_valid(evo_secure_erasure_backend_t backend)
{
    return backend == EVO_SECURE_ERASURE_BACKEND_NONE ||
           backend == EVO_SECURE_ERASURE_BACKEND_EXPLICIT_BZERO ||
           backend == EVO_SECURE_ERASURE_BACKEND_VOLATILE_BYTES;
}

static bool decode_state(checkpoint_reader_t *reader,
                         checkpoint_state_projection_t *state)
{
    *state = (checkpoint_state_projection_t){0};
    state->run.version = read_u32(reader);
    state->run.current_generation = read_u64(reader);
    state->run.best_generation = read_u64(reader);
    state->run.best_population_index = read_size(reader);
    state->run.termination_reason =
        (evo_termination_reason_t)read_u32(reader);
    state->run.adaptive_mutation_applicable = read_bool(reader);
    state->run.initialized = read_bool(reader);
    state->run.adaptive_mutation.effective_rate = read_double(reader);
    state->run.adaptive_mutation.stagnant_generations = read_size(reader);
    state->run.adaptive_mutation.initialized = read_bool(reader);
    state->run.stopping.significant_best_total = read_double(reader);
    state->run.stopping.stagnant_generations = read_size(reader);
    state->run.stopping.initialized = read_bool(reader);
    state->checkpoint_rng_algorithm_version = read_u32(reader);
    state->checkpoint_operator_seed_schedule_version = read_u32(reader);
    state->checkpoint_selection_policy_version = read_u32(reader);
    state->checkpoint_byte_operator_policy_version = read_u32(reader);
    state->checkpoint_bounded_run_policy_version = read_u32(reader);
    state->population_size = read_size(reader);
    state->genome_size = read_size(reader);
    state->storage_bytes = read_size(reader);
    state->evaluation_bytes = read_size(reader);
    state->valid_count = read_size(reader);
    state->best_index = read_size(reader);
    state->produced_count = read_size(reader);
    state->elite_count = read_size(reader);
    state->elite_source_valid_count = read_size(reader);
    state->initialization_seed = read_u64(reader);
    state->source_generation = read_u64(reader);
    state->secure_erasure_policy_version = read_u32(reader);
    state->secure_erasure_backend =
        (evo_secure_erasure_backend_t)read_u32(reader);
    state->rng_algorithm_version = read_u32(reader);
    state->operator_seed_schedule_version = read_u32(reader);
    state->selection_policy_version = read_u32(reader);
    state->selection_policy = (evo_selection_policy_t)read_u32(reader);
    state->byte_operator_policy_version = read_u32(reader);
    state->crossover_operator =
        (evo_crossover_operator_t)read_u32(reader);
    state->mutation_operator =
        (evo_mutation_operator_t)read_u32(reader);
    state->mutation_rate_used = read_double(reader);
    state->odd_child_policy_version = read_u32(reader);
    state->elite_policy_version = read_u32(reader);
    state->singleton_child_policy_version = read_u32(reader);
    state->fitness_comparison_policy_version = read_u32(reader);
    state->diversity_policy_version = read_u32(reader);
    state->diversity_metric_version = read_u32(reader);
    state->diversity_pair_count = read_size(reader);
    state->diversity_work_units = read_size(reader);
    state->diversity = read_double(reader);
    state->population_initialized = read_bool(reader);
    state->has_best = read_bool(reader);
    state->evaluated = read_bool(reader);
    state->elite_count_explicit = read_bool(reader);
    state->diversity_uses_domain_distance = read_bool(reader);
    state->secure_erasure_enabled = read_bool(reader);
    state->global_best_fitness = read_fitness(reader);
    state->result_best_genome_size = read_size(reader);
    state->result_secure_erasure_policy_version = read_u32(reader);
    state->result_secure_erasure_backend =
        (evo_secure_erasure_backend_t)read_u32(reader);
    state->result_secure_erasure_enabled = read_bool(reader);
    state->result_random_seed = read_u64(reader);

    return reader->valid && reader->offset == reader->size &&
           state->run.version == EVO_RUN_STATE_VERSION &&
           state->run.initialized && state->run.stopping.initialized &&
           state->checkpoint_rng_algorithm_version ==
               EVO_RNG_ALGORITHM_VERSION &&
           state->checkpoint_operator_seed_schedule_version ==
               EVO_OPERATOR_SEED_SCHEDULE_VERSION &&
           state->checkpoint_selection_policy_version ==
               EVO_SELECTION_POLICY_VERSION &&
           state->checkpoint_byte_operator_policy_version ==
               EVO_BYTE_OPERATOR_POLICY_VERSION &&
           state->checkpoint_bounded_run_policy_version ==
               EVO_BOUNDED_RUN_POLICY_VERSION &&
           termination_reason_is_valid(state->run.termination_reason) &&
           state->run.best_generation <= state->run.current_generation &&
           state->population_size != 0 && state->genome_size != 0 &&
           state->valid_count <= state->population_size &&
           state->best_index < state->population_size &&
           state->run.best_population_index < state->population_size &&
           state->has_best == (state->valid_count != 0) &&
           state->evaluated &&
           state->fitness_comparison_policy_version ==
               EVO_FITNESS_COMPARISON_POLICY_VERSION &&
           state->diversity_policy_version == EVO_DIVERSITY_POLICY_VERSION &&
           state->diversity_metric_version != 0 && isfinite(state->diversity) &&
           state->diversity >= 0.0 && state->diversity <= 1.0 &&
           isfinite(state->run.stopping.significant_best_total) &&
           evo_fitness_evidence_is_valid(&state->global_best_fitness) &&
           state->result_best_genome_size == state->genome_size &&
           state->secure_erasure_enabled ==
               state->result_secure_erasure_enabled &&
           state->secure_erasure_policy_version ==
               state->result_secure_erasure_policy_version &&
           secure_backend_is_valid(state->secure_erasure_backend) &&
           state->secure_erasure_backend ==
               state->result_secure_erasure_backend;
}

static bool byte_ranges_overlap(const void *left,
                                size_t left_size,
                                const void *right,
                                size_t right_size)
{
    const uintmax_t left_start = (uintmax_t)(uintptr_t)left;
    const uintmax_t right_start = (uintmax_t)(uintptr_t)right;
    uintmax_t left_end = 0;
    uintmax_t right_end = 0;

    if (left == NULL || right == NULL || left_size == 0 || right_size == 0 ||
        (uintmax_t)left_size > UINTMAX_MAX - left_start ||
        (uintmax_t)right_size > UINTMAX_MAX - right_start) {
        return true;
    }
    left_end = left_start + (uintmax_t)left_size;
    right_end = right_start + (uintmax_t)right_size;
    return left_start < right_end && right_start < left_end;
}

static bool byte_range_is_within(const void *owner,
                                 size_t owner_size,
                                 const void *view,
                                 size_t view_size)
{
    const uintmax_t owner_start = (uintmax_t)(uintptr_t)owner;
    const uintmax_t view_start = (uintmax_t)(uintptr_t)view;
    uintmax_t owner_end = 0;
    uintmax_t view_end = 0;

    if (owner == NULL || view == NULL || owner_size == 0 || view_size == 0 ||
        (uintmax_t)owner_size > UINTMAX_MAX - owner_start ||
        (uintmax_t)view_size > UINTMAX_MAX - view_start) {
        return false;
    }
    owner_end = owner_start + (uintmax_t)owner_size;
    view_end = view_start + (uintmax_t)view_size;
    return view_start >= owner_start && view_end <= owner_end;
}

static evo_status_t parse_layout(const void *checkpoint,
                                 size_t checkpoint_size,
                                 size_t max_checkpoint_bytes,
                                 checkpoint_layout_t *layout)
{
    const unsigned char *bytes = checkpoint;
    size_t section_end = 0;

    if (checkpoint == NULL || layout == NULL || max_checkpoint_bytes == 0 ||
        checkpoint_size > max_checkpoint_bytes ||
        checkpoint_size < EVO_CHECKPOINT_HEADER_SIZE) {
        return EVO_ERROR_CHECKPOINT_INVALID;
    }
    for (size_t index = 0; index < sizeof(checkpoint_magic); ++index) {
        if (bytes[index] != checkpoint_magic[index]) {
            return EVO_ERROR_CHECKPOINT_INVALID;
        }
    }
    if (read_u32_at(bytes, 8) != EVO_CHECKPOINT_FORMAT_VERSION) {
        return EVO_ERROR_CHECKPOINT_VERSION;
    }
    if (read_u32_at(bytes, 12) != EVO_CHECKPOINT_HEADER_SIZE ||
        read_u32_at(bytes, 136) != EVO_CHECKPOINT_SECTION_COUNT ||
        read_u32_at(bytes, 140) != 0) {
        return EVO_ERROR_CHECKPOINT_INVALID;
    }

    *layout = (checkpoint_layout_t){0};
    if (!u64_to_size(read_u64_at(bytes, 16), &layout->total_size) ||
        !u64_to_size(read_u64_at(bytes, 24),
                     &layout->configuration_offset) ||
        !u64_to_size(read_u64_at(bytes, 32),
                     &layout->configuration_size) ||
        !u64_to_size(read_u64_at(bytes, 40), &layout->state_offset) ||
        !u64_to_size(read_u64_at(bytes, 48), &layout->state_size) ||
        !u64_to_size(read_u64_at(bytes, 56),
                     &layout->statistics_offset) ||
        !u64_to_size(read_u64_at(bytes, 64),
                     &layout->statistics_size) ||
        !u64_to_size(read_u64_at(bytes, 72),
                     &layout->evaluations_offset) ||
        !u64_to_size(read_u64_at(bytes, 80),
                     &layout->evaluations_size) ||
        !u64_to_size(read_u64_at(bytes, 88), &layout->genomes_offset) ||
        !u64_to_size(read_u64_at(bytes, 96), &layout->genomes_size) ||
        !u64_to_size(read_u64_at(bytes, 104),
                     &layout->best_genome_offset) ||
        !u64_to_size(read_u64_at(bytes, 112),
                     &layout->best_genome_size)) {
        return EVO_ERROR_CHECKPOINT_INVALID;
    }
    layout->configuration_fingerprint = read_u64_at(bytes, 120);
    layout->integrity_algorithm = read_u32_at(bytes, 128);
    layout->integrity_value = read_u32_at(bytes, 132);

    if (layout->total_size != checkpoint_size ||
        layout->configuration_offset != EVO_CHECKPOINT_HEADER_SIZE ||
        layout->configuration_size == 0 || layout->state_size == 0 ||
        layout->statistics_size == 0 || layout->evaluations_size == 0 ||
        layout->genomes_size == 0 || layout->best_genome_size == 0 ||
        !checked_size_add(layout->configuration_offset,
                          layout->configuration_size,
                          &section_end) ||
        section_end != layout->state_offset ||
        !checked_size_add(layout->state_offset,
                          layout->state_size,
                          &section_end) ||
        section_end != layout->statistics_offset ||
        !checked_size_add(layout->statistics_offset,
                          layout->statistics_size,
                          &section_end) ||
        section_end != layout->evaluations_offset ||
        !checked_size_add(layout->evaluations_offset,
                          layout->evaluations_size,
                          &section_end) ||
        section_end != layout->genomes_offset ||
        !checked_size_add(layout->genomes_offset,
                          layout->genomes_size,
                          &section_end) ||
        section_end != layout->best_genome_offset ||
        !checked_size_add(layout->best_genome_offset,
                          layout->best_genome_size,
                          &section_end) ||
        section_end != layout->total_size) {
        return EVO_ERROR_CHECKPOINT_INVALID;
    }
    if (layout->integrity_algorithm != EVO_CHECKPOINT_INTEGRITY_CRC32) {
        return EVO_ERROR_CHECKPOINT_VERSION;
    }
    if (checkpoint_crc32(bytes, checkpoint_size) !=
        layout->integrity_value) {
        return EVO_ERROR_CHECKPOINT_INTEGRITY;
    }
    if (fingerprint_bytes(bytes + layout->configuration_offset,
                          layout->configuration_size) !=
        layout->configuration_fingerprint) {
        return EVO_ERROR_CHECKPOINT_INVALID;
    }
    return EVO_SUCCESS;
}

static bool fitness_equal(const evo_fitness_t *left,
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

static bool add_fitness(evo_fitness_t *sum, const evo_fitness_t *value)
{
    if (!evo_fitness_evidence_is_valid(value)) {
        return false;
    }
    sum->correctness += value->correctness;
    sum->performance += value->performance;
    sum->memory_use += value->memory_use;
    sum->reliability += value->reliability;
    sum->maintainability += value->maintainability;
    sum->constraint_penalty += value->constraint_penalty;
    sum->total += value->total;
    return evo_fitness_evidence_is_valid(sum);
}

static bool evaluation_section_is_valid(
    const unsigned char *bytes,
    const checkpoint_layout_t *layout,
    const checkpoint_state_projection_t *state,
    const evo_generation_statistics_t *statistics)
{
    checkpoint_reader_t reader = {
        .bytes = bytes + layout->evaluations_offset,
        .size = layout->evaluations_size,
        .valid = true,
    };
    evo_fitness_t sums = {0};
    evo_fitness_t best_fitness = {0};
    size_t valid_count = 0;
    size_t best_index = 0;
    bool has_best = false;

    for (size_t index = 0; index < state->population_size; ++index) {
        const evo_fitness_t fitness = read_fitness(&reader);
        const bool valid = read_bool(&reader);
        const bool evaluated = read_bool(&reader);

        if (!reader.valid || (!valid && (evaluated || !fitness_is_zero(&fitness))) ||
            (valid && (!evaluated ||
                       !evo_fitness_evidence_is_valid(&fitness)))) {
            return false;
        }
        if (!valid) {
            continue;
        }
        if (!add_fitness(&sums, &fitness)) {
            return false;
        }
        ++valid_count;
        if (!has_best || fitness.total > best_fitness.total) {
            best_index = index;
            best_fitness = fitness;
            has_best = true;
        }
    }

    return reader.valid && reader.offset == reader.size &&
           valid_count == state->valid_count &&
           has_best == state->has_best &&
           (!has_best || (best_index == state->best_index &&
                          best_index == statistics->best_index &&
                          fitness_equal(&best_fitness,
                                        &statistics->best_fitness))) &&
           (has_best || (state->best_index == 0 &&
                         statistics->best_index == 0)) &&
           fitness_equal(&sums, &statistics->fitness_sums);
}

static bool source_erasure_metadata_is_valid(
    bool enabled,
    uint32_t policy_version,
    evo_secure_erasure_backend_t backend)
{
    return policy_version == EVO_SECURE_ERASURE_POLICY_VERSION &&
           secure_backend_is_valid(backend) &&
           (enabled ? backend != EVO_SECURE_ERASURE_BACKEND_NONE
                    : backend == EVO_SECURE_ERASURE_BACKEND_NONE);
}

static bool decoded_population_provenance_is_valid(
    const evo_checkpoint_configuration_view_t *configuration,
    const evo_config_t *config,
    const checkpoint_state_projection_t *state)
{
    evo_population_t population = {
        .population_size = state->population_size,
        .genome_size = state->genome_size,
        .valid_count = state->valid_count,
        .diversity_policy_version = state->diversity_policy_version,
        .diversity_metric_version = state->diversity_metric_version,
        .diversity_pair_count = state->diversity_pair_count,
        .diversity_work_units = state->diversity_work_units,
        .diversity = state->diversity,
        .evaluated = state->evaluated,
        .diversity_uses_domain_distance =
            state->diversity_uses_domain_distance,
    };
    size_t requested_elites = 0;
    size_t effective_elites = 0;
    size_t offspring_count = 0;
    uint32_t expected_odd_policy = 0;
    uint32_t expected_singleton_policy = 0;
    bool production_is_valid = false;

    if (configuration == NULL || config == NULL ||
        !source_erasure_metadata_is_valid(
            state->secure_erasure_enabled,
            state->secure_erasure_policy_version,
            state->secure_erasure_backend) ||
        state->evaluation_bytes > config->max_evaluation_bytes ||
        state->fitness_comparison_policy_version !=
            EVO_FITNESS_COMPARISON_POLICY_VERSION ||
        state->diversity_uses_domain_distance !=
            configuration->distance_callback_present ||
        state->diversity_metric_version !=
            (configuration->distance_callback_present
                 ? configuration->genome_distance_version
                 : EVO_BYTE_DIVERSITY_METRIC_VERSION) ||
        !evo_population_diversity_evidence_is_valid(config, &population)) {
        return false;
    }

    if (state->population_initialized) {
        production_is_valid =
            state->initialization_seed == config->random_seed &&
            state->rng_algorithm_version == EVO_RNG_ALGORITHM_VERSION &&
            state->produced_count == 0 && state->elite_count == 0 &&
            state->elite_source_valid_count == 0 &&
            state->source_generation == 0 &&
            state->operator_seed_schedule_version == 0 &&
            state->selection_policy_version == 0 &&
            state->selection_policy == EVO_SELECTION_TOURNAMENT &&
            state->byte_operator_policy_version == 0 &&
            state->crossover_operator == EVO_CROSSOVER_CONSUMER &&
            state->mutation_operator == EVO_MUTATION_CONSUMER &&
            state->mutation_rate_used == 0.0 &&
            state->odd_child_policy_version == 0 &&
            state->elite_policy_version == 0 &&
            state->singleton_child_policy_version == 0 &&
            !state->elite_count_explicit &&
            state->storage_bytes <= config->max_population_bytes;
        return production_is_valid;
    }

    if (evo_elite_policy_counts(config,
                                state->elite_source_valid_count,
                                &requested_elites,
                                &effective_elites,
                                &offspring_count) != EVO_SUCCESS) {
        return false;
    }
    (void)requested_elites;
    if (!config->elite_count_enabled && state->population_size % 2 != 0) {
        expected_odd_policy = EVO_ODD_CHILD_POLICY_VERSION;
    }
    if (offspring_count % 2 != 0) {
        expected_singleton_policy = EVO_SINGLETON_CHILD_POLICY_VERSION;
    }

    return state->initialization_seed == 0 &&
           state->rng_algorithm_version == 0 &&
           state->produced_count == state->population_size &&
           state->elite_count == effective_elites &&
           state->operator_seed_schedule_version ==
               EVO_OPERATOR_SEED_SCHEDULE_VERSION &&
           state->selection_policy_version == EVO_SELECTION_POLICY_VERSION &&
           state->selection_policy == config->selection_policy &&
           state->byte_operator_policy_version ==
               EVO_BYTE_OPERATOR_POLICY_VERSION &&
           state->crossover_operator == config->crossover_operator &&
           state->mutation_operator == config->mutation_operator &&
           isfinite(state->mutation_rate_used) &&
           state->mutation_rate_used >= 0.0 &&
           state->mutation_rate_used <= 1.0 &&
           state->odd_child_policy_version == expected_odd_policy &&
           state->elite_policy_version == EVO_ELITE_POLICY_VERSION &&
           state->singleton_child_policy_version ==
               expected_singleton_policy &&
           state->elite_count_explicit == config->elite_count_enabled &&
           state->storage_bytes <= config->max_child_population_bytes;
}

static bool decoded_termination_is_valid(
    const evo_checkpoint_configuration_view_t *configuration,
    const checkpoint_state_projection_t *state,
    const evo_generation_statistics_t *statistics)
{
    evo_termination_reason_t natural_reason = EVO_TERMINATION_NONE;

    if (!state->has_best) {
        natural_reason = EVO_TERMINATION_ALL_INVALID;
    } else if (configuration->fitness_target_enabled &&
               state->global_best_fitness.total >=
                   configuration->fitness_target) {
        natural_reason = EVO_TERMINATION_CONVERGED;
    } else if ((configuration->stagnation_enabled &&
                state->run.stopping.stagnant_generations >=
                    configuration->stagnation_patience) ||
               (configuration->diversity_floor_enabled &&
                statistics->diversity <=
                    configuration->diversity_floor)) {
        natural_reason = EVO_TERMINATION_STAGNATED;
    } else if (state->run.current_generation ==
               configuration->generation_limit) {
        natural_reason = EVO_TERMINATION_GENERATION_LIMIT;
    }

    if (natural_reason != EVO_TERMINATION_NONE) {
        return state->run.termination_reason == natural_reason;
    }
    return state->run.termination_reason == EVO_TERMINATION_NONE ||
           (state->run.termination_reason ==
                EVO_TERMINATION_APPLICATION_REQUESTED &&
            configuration->generation_stop_present);
}

static bool decoded_global_best_is_valid(
    const unsigned char *bytes,
    const checkpoint_layout_t *layout,
    const checkpoint_state_projection_t *state,
    const evo_generation_statistics_t *statistics)
{
    size_t current_best_offset = 0;

    if (state->has_best &&
        state->global_best_fitness.total < statistics->best_fitness.total) {
        return false;
    }
    if (state->run.best_generation != state->run.current_generation) {
        return true;
    }
    if (!state->has_best ||
        state->run.best_population_index != state->best_index ||
        !fitness_equal(&state->global_best_fitness,
                       &statistics->best_fitness) ||
        !checked_size_multiply(state->best_index,
                               state->genome_size,
                               &current_best_offset) ||
        current_best_offset > layout->genomes_size ||
        state->genome_size > layout->genomes_size - current_best_offset) {
        return false;
    }
    for (size_t index = 0; index < state->genome_size; ++index) {
        if (bytes[layout->genomes_offset + current_best_offset + index] !=
            bytes[layout->best_genome_offset + index]) {
            return false;
        }
    }
    return true;
}

static bool decoded_sections_are_consistent(
    const unsigned char *bytes,
    const checkpoint_layout_t *layout,
    const evo_checkpoint_configuration_view_t *configuration,
    const evo_config_t *config,
    const checkpoint_state_projection_t *state,
    const evo_generation_statistics_t *statistics)
{
    size_t expected_storage_bytes = 0;
    size_t expected_evaluation_bytes = 0;

    if (!checked_size_multiply(state->population_size,
                               state->genome_size,
                               &expected_storage_bytes) ||
        !checked_size_multiply(state->population_size,
                               EVO_CHECKPOINT_EVALUATION_RECORD_SIZE,
                               &expected_evaluation_bytes) ||
        state->population_size != configuration->population_size ||
        state->genome_size != configuration->genome_size ||
        state->run.current_generation > configuration->generation_limit ||
        state->storage_bytes != expected_storage_bytes ||
        state->storage_bytes != layout->genomes_size ||
        layout->best_genome_size != state->genome_size ||
        expected_evaluation_bytes != layout->evaluations_size ||
        state->evaluation_bytes < expected_evaluation_bytes ||
        state->result_random_seed != configuration->random_seed ||
        state->secure_erasure_enabled !=
            configuration->secure_erasure_enabled ||
        statistics->generation_index != state->run.current_generation ||
        statistics->population_size != state->population_size ||
        statistics->valid_count != state->valid_count ||
        statistics->has_best != state->has_best ||
        statistics->diversity_policy_version !=
            state->diversity_policy_version ||
        statistics->diversity_metric_version !=
            state->diversity_metric_version ||
        statistics->diversity_pair_count != state->diversity_pair_count ||
        statistics->diversity_work_units != state->diversity_work_units ||
        statistics->diversity != state->diversity ||
        statistics->diversity_uses_domain_distance !=
            state->diversity_uses_domain_distance ||
        state->run.stopping.significant_best_total >
            state->global_best_fitness.total ||
        state->run.stopping.stagnant_generations >
            state->run.current_generation ||
        (!configuration->stagnation_enabled &&
         state->run.stopping.stagnant_generations != 0) ||
        (state->run.termination_reason == EVO_TERMINATION_NONE &&
         state->run.current_generation >= configuration->generation_limit) ||
        (state->run.termination_reason ==
             EVO_TERMINATION_GENERATION_LIMIT &&
         state->run.current_generation != configuration->generation_limit) ||
        (state->run.termination_reason == EVO_TERMINATION_ALL_INVALID &&
         (state->has_best || state->run.current_generation == 0 ||
          state->run.best_generation >= state->run.current_generation)) ||
        (state->run.termination_reason != EVO_TERMINATION_ALL_INVALID &&
         !state->has_best) ||
        (state->run.current_generation == 0 &&
         (!state->population_initialized || state->source_generation != 0 ||
          state->initialization_seed != configuration->random_seed ||
          state->run.best_generation != 0 ||
          state->run.best_population_index != state->best_index ||
          state->run.stopping.significant_best_total !=
              state->global_best_fitness.total)) ||
        (state->run.current_generation != 0 &&
         (state->population_initialized ||
          state->source_generation != state->run.current_generation - 1)) ||
        state->run.adaptive_mutation_applicable !=
            evo_adaptive_mutation_is_applicable(config) ||
        (state->run.adaptive_mutation_applicable &&
         (!state->run.adaptive_mutation.initialized ||
          !isfinite(state->run.adaptive_mutation.effective_rate) ||
          state->run.adaptive_mutation.effective_rate !=
              statistics->mutation_rate_effective ||
          state->run.adaptive_mutation.stagnant_generations !=
              statistics->adaptive_mutation_stagnant_generations)) ||
        (!state->run.adaptive_mutation_applicable &&
         (state->run.adaptive_mutation.initialized ||
          state->run.adaptive_mutation.effective_rate != 0.0 ||
          state->run.adaptive_mutation.stagnant_generations != 0)) ||
        (state->run.current_generation != 0 &&
         state->mutation_rate_used !=
             (state->run.adaptive_mutation_applicable
                  ? statistics->mutation_rate_prior
                  : 0.0)) ||
        !source_erasure_metadata_is_valid(
            state->result_secure_erasure_enabled,
            state->result_secure_erasure_policy_version,
            state->result_secure_erasure_backend) ||
        !decoded_population_provenance_is_valid(configuration,
                                                config,
                                                state) ||
        !evo_adaptive_mutation_statistics_are_valid(config, statistics) ||
        !decoded_termination_is_valid(configuration, state, statistics) ||
        !decoded_global_best_is_valid(bytes, layout, state, statistics) ||
        !evaluation_section_is_valid(bytes,
                                     layout,
                                     state,
                                     statistics)) {
        return false;
    }
    return true;
}

evo_status_t evo_checkpoint_inspect(const void *checkpoint,
                                    size_t checkpoint_size,
                                    size_t max_checkpoint_bytes,
                                    evo_checkpoint_view_t *view)
{
    checkpoint_layout_t layout = {0};
    checkpoint_state_projection_t state = {0};
    evo_checkpoint_configuration_view_t configuration = {0};
    evo_problem_t decoded_problem = {0};
    evo_config_t decoded_config = {0};
    evo_generation_statistics_t statistics = {0};
    checkpoint_reader_t configuration_reader = {0};
    checkpoint_reader_t state_reader = {0};
    checkpoint_reader_t statistics_reader = {0};
    const unsigned char *bytes = checkpoint;
    evo_status_t status = EVO_SUCCESS;

    if (view == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (checkpoint != NULL && checkpoint_size != 0 &&
        byte_ranges_overlap(checkpoint,
                            checkpoint_size,
                            view,
                            sizeof(*view))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    *view = (evo_checkpoint_view_t){0};
    status = parse_layout(checkpoint,
                          checkpoint_size,
                          max_checkpoint_bytes,
                          &layout);
    if (status != EVO_SUCCESS) {
        return status;
    }

    configuration_reader = (checkpoint_reader_t){
        .bytes = bytes + layout.configuration_offset,
        .size = layout.configuration_size,
        .valid = true,
    };
    state_reader = (checkpoint_reader_t){
        .bytes = bytes + layout.state_offset,
        .size = layout.state_size,
        .valid = true,
    };
    statistics_reader = (checkpoint_reader_t){
        .bytes = bytes + layout.statistics_offset,
        .size = layout.statistics_size,
        .valid = true,
    };
    if (!decode_configuration(&configuration_reader, &configuration) ||
        !decoded_configuration_is_valid(&configuration,
                                        &decoded_problem,
                                        &decoded_config) ||
        !decode_state(&state_reader, &state) ||
        !decode_statistics(&statistics_reader, &statistics) ||
        !decoded_sections_are_consistent(bytes,
                                         &layout,
                                         &configuration,
                                         &decoded_config,
                                         &state,
                                         &statistics)) {
        return EVO_ERROR_CHECKPOINT_INVALID;
    }

    *view = (evo_checkpoint_view_t){
        .version = EVO_CHECKPOINT_VIEW_VERSION,
        .format_version = EVO_CHECKPOINT_FORMAT_VERSION,
        .integrity_algorithm = layout.integrity_algorithm,
        .integrity_value = layout.integrity_value,
        .configuration_fingerprint = layout.configuration_fingerprint,
        .serialized_checkpoint = checkpoint,
        .serialized_checkpoint_size = checkpoint_size,
        .configuration = configuration,
        .current_generation = state.run.current_generation,
        .termination_reason = state.run.termination_reason,
        .population_size = state.population_size,
        .valid_count = state.valid_count,
        .current_best_index = state.best_index,
        .current_has_best = state.has_best,
        .global_best_generation = state.run.best_generation,
        .global_best_population_index = state.run.best_population_index,
        .global_best_fitness = state.global_best_fitness,
        .global_best_genome = bytes + layout.best_genome_offset,
        .global_best_genome_size = layout.best_genome_size,
        .generation_statistics = statistics,
        .rng_algorithm_version =
            state.checkpoint_rng_algorithm_version,
        .operator_seed_schedule_version =
            state.checkpoint_operator_seed_schedule_version,
        .bounded_run_policy_version =
            state.checkpoint_bounded_run_policy_version,
        .selection_policy_version =
            state.checkpoint_selection_policy_version,
        .byte_operator_policy_version =
            state.checkpoint_byte_operator_policy_version,
        .fitness_comparison_policy_version =
            state.fitness_comparison_policy_version,
        .diversity_policy_version = state.diversity_policy_version,
        .diversity_metric_version = state.diversity_metric_version,
        .adaptive_mutation_policy_version =
            statistics.adaptive_mutation_policy_version,
        .effective_mutation_rate =
            state.run.adaptive_mutation.effective_rate,
        .adaptive_mutation_stagnant_generations =
            state.run.adaptive_mutation.stagnant_generations,
        .significant_best_total =
            state.run.stopping.significant_best_total,
        .stopping_stagnant_generations =
            state.run.stopping.stagnant_generations,
        .secure_erasure_policy_version =
            state.secure_erasure_policy_version,
        .secure_erasure_backend = state.secure_erasure_backend,
        .secure_erasure_enabled = state.secure_erasure_enabled,
        .population_genome_bytes = layout.genomes_size,
        .population_genomes = bytes + layout.genomes_offset,
        .population_genome_stride = state.genome_size,
        .population_evaluation_records = state.population_size,
        .population_evaluation_bytes = state.evaluation_bytes,
        .serialized_evaluations = bytes + layout.evaluations_offset,
        .serialized_evaluation_record_size =
            EVO_CHECKPOINT_EVALUATION_RECORD_SIZE,
    };
    return EVO_SUCCESS;
}

evo_status_t evo_checkpoint_candidate_inspect(
    const evo_checkpoint_view_t *checkpoint,
    size_t population_index,
    evo_checkpoint_candidate_view_t *candidate)
{
    checkpoint_reader_t reader = {0};
    evo_checkpoint_candidate_view_t projected = {0};
    const unsigned char *evaluation_bytes = NULL;
    const unsigned char *genome_bytes = NULL;
    size_t genome_size = 0;
    size_t genome_offset = 0;
    size_t evaluation_offset = 0;
    size_t evaluation_size = 0;

    if (checkpoint == NULL || candidate == NULL ||
        checkpoint->version != EVO_CHECKPOINT_VIEW_VERSION ||
        checkpoint->format_version != EVO_CHECKPOINT_FORMAT_VERSION ||
        checkpoint->integrity_algorithm != EVO_CHECKPOINT_INTEGRITY_CRC32 ||
        checkpoint->configuration.version !=
            EVO_CHECKPOINT_CONFIGURATION_VIEW_VERSION ||
        checkpoint->serialized_checkpoint == NULL ||
        checkpoint->population_genomes == NULL ||
        checkpoint->serialized_evaluations == NULL ||
        checkpoint->population_size !=
            checkpoint->configuration.population_size ||
        checkpoint->population_evaluation_records !=
            checkpoint->population_size ||
        checkpoint->population_genome_stride !=
            checkpoint->configuration.genome_size ||
        checkpoint->serialized_evaluation_record_size !=
            EVO_CHECKPOINT_EVALUATION_RECORD_SIZE ||
        checkpoint->population_evaluation_bytes == 0 ||
        checkpoint->population_evaluation_bytes >
            checkpoint->configuration.max_evaluation_bytes ||
        population_index >= checkpoint->population_size ||
        byte_ranges_overlap(checkpoint,
                            sizeof(*checkpoint),
                            candidate,
                            sizeof(*candidate))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    *candidate = (evo_checkpoint_candidate_view_t){0};
    if (!checked_size_multiply(checkpoint->population_size,
                               checkpoint->population_genome_stride,
                               &genome_size) ||
        genome_size != checkpoint->population_genome_bytes ||
        !checked_size_multiply(checkpoint->population_evaluation_records,
                               EVO_CHECKPOINT_EVALUATION_RECORD_SIZE,
                               &evaluation_size) ||
        !byte_range_is_within(checkpoint->serialized_checkpoint,
                              checkpoint->serialized_checkpoint_size,
                              checkpoint->population_genomes,
                              checkpoint->population_genome_bytes) ||
        !byte_range_is_within(checkpoint->serialized_checkpoint,
                              checkpoint->serialized_checkpoint_size,
                              checkpoint->serialized_evaluations,
                              evaluation_size) ||
        !checked_size_multiply(population_index,
                               checkpoint->configuration.genome_size,
                               &genome_offset) ||
        !checked_size_multiply(population_index,
                               EVO_CHECKPOINT_EVALUATION_RECORD_SIZE,
                               &evaluation_offset) ||
        genome_offset > checkpoint->population_genome_bytes ||
        checkpoint->configuration.genome_size >
            checkpoint->population_genome_bytes - genome_offset ||
        evaluation_offset > evaluation_size ||
        EVO_CHECKPOINT_EVALUATION_RECORD_SIZE >
            evaluation_size - evaluation_offset) {
        return EVO_ERROR_CHECKPOINT_INVALID;
    }
    genome_bytes = checkpoint->population_genomes;
    evaluation_bytes = checkpoint->serialized_evaluations;
    reader = (checkpoint_reader_t){
        .bytes = evaluation_bytes + evaluation_offset,
        .size = EVO_CHECKPOINT_EVALUATION_RECORD_SIZE,
        .valid = true,
    };
    projected.version = EVO_CHECKPOINT_CANDIDATE_VIEW_VERSION;
    projected.population_index = population_index;
    projected.genome = genome_bytes + genome_offset;
    projected.genome_size = checkpoint->configuration.genome_size;
    projected.fitness = read_fitness(&reader);
    projected.valid = read_bool(&reader);
    projected.evaluated = read_bool(&reader);
    if (!reader.valid || reader.offset != reader.size ||
        (!projected.valid &&
         (projected.evaluated || !fitness_is_zero(&projected.fitness))) ||
        (projected.valid &&
         (!projected.evaluated ||
          !evo_fitness_evidence_is_valid(&projected.fitness)))) {
        return EVO_ERROR_CHECKPOINT_INVALID;
    }
    *candidate = projected;
    return EVO_SUCCESS;
}

static bool checkpoint_section_sizes(const evo_problem_t *problem,
                                     const evo_config_t *config,
                                     size_t *configuration_size,
                                     size_t *state_size,
                                     size_t *statistics_size)
{
    const evo_population_t empty_population = {0};
    const evo_result_t empty_result = {0};
    const evo_run_state_t empty_state = {0};
    const evo_generation_statistics_t empty_statistics = {0};
    checkpoint_writer_t configuration_writer = {.valid = true};
    checkpoint_writer_t state_writer = {.valid = true};
    checkpoint_writer_t statistics_writer = {.valid = true};

    if (problem == NULL || config == NULL || configuration_size == NULL ||
        state_size == NULL || statistics_size == NULL) {
        return false;
    }
    encode_configuration(&configuration_writer, problem, config);
    encode_state(&state_writer,
                 &empty_population,
                 &empty_result,
                 &empty_state);
    encode_statistics(&statistics_writer, &empty_statistics);
    if (!configuration_writer.valid || !state_writer.valid ||
        !statistics_writer.valid) {
        return false;
    }
    *configuration_size = configuration_writer.offset;
    *state_size = state_writer.offset;
    *statistics_size = statistics_writer.offset;
    return true;
}

static bool make_layout(const evo_problem_t *problem,
                        const evo_config_t *config,
                        checkpoint_layout_t *layout)
{
    size_t offset = EVO_CHECKPOINT_HEADER_SIZE;
    uint64_t converted = 0;

    if (layout == NULL || problem == NULL || config == NULL ||
        problem->genome_size == 0 || config->population_size == 0) {
        return false;
    }
    *layout = (checkpoint_layout_t){0};
    layout->configuration_offset = offset;
    if (!checkpoint_section_sizes(problem,
                                  config,
                                  &layout->configuration_size,
                                  &layout->state_size,
                                  &layout->statistics_size) ||
        !checked_size_add(offset, layout->configuration_size, &offset)) {
        return false;
    }
    layout->state_offset = offset;
    if (!checked_size_add(offset, layout->state_size, &offset)) {
        return false;
    }
    layout->statistics_offset = offset;
    if (!checked_size_add(offset, layout->statistics_size, &offset)) {
        return false;
    }
    layout->evaluations_offset = offset;
    if (!checked_size_multiply(config->population_size,
                               EVO_CHECKPOINT_EVALUATION_RECORD_SIZE,
                               &layout->evaluations_size) ||
        !checked_size_add(offset, layout->evaluations_size, &offset)) {
        return false;
    }
    layout->genomes_offset = offset;
    if (!checked_size_multiply(config->population_size,
                               problem->genome_size,
                               &layout->genomes_size) ||
        !checked_size_add(offset, layout->genomes_size, &offset)) {
        return false;
    }
    layout->best_genome_offset = offset;
    layout->best_genome_size = problem->genome_size;
    if (!checked_size_add(offset, layout->best_genome_size, &offset)) {
        return false;
    }
    layout->total_size = offset;
    layout->integrity_algorithm = EVO_CHECKPOINT_INTEGRITY_CRC32;

    return size_to_u64(layout->total_size, &converted) &&
           size_to_u64(layout->configuration_offset, &converted) &&
           size_to_u64(layout->configuration_size, &converted) &&
           size_to_u64(layout->state_offset, &converted) &&
           size_to_u64(layout->state_size, &converted) &&
           size_to_u64(layout->statistics_offset, &converted) &&
           size_to_u64(layout->statistics_size, &converted) &&
           size_to_u64(layout->evaluations_offset, &converted) &&
           size_to_u64(layout->evaluations_size, &converted) &&
           size_to_u64(layout->genomes_offset, &converted) &&
           size_to_u64(layout->genomes_size, &converted) &&
           size_to_u64(layout->best_genome_offset, &converted) &&
           size_to_u64(layout->best_genome_size, &converted);
}

evo_status_t evo_checkpoint_size(const evo_problem_t *problem,
                                 const evo_config_t *config,
                                 size_t *checkpoint_size)
{
    checkpoint_layout_t layout = {0};

    if (checkpoint_size == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if ((problem != NULL &&
         byte_ranges_overlap(problem,
                             sizeof(*problem),
                             checkpoint_size,
                             sizeof(*checkpoint_size))) ||
        (config != NULL &&
         byte_ranges_overlap(config,
                             sizeof(*config),
                             checkpoint_size,
                             sizeof(*checkpoint_size)))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    *checkpoint_size = 0;
    if (problem == NULL || config == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (!make_layout(problem, config, &layout)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    *checkpoint_size = layout.total_size;
    return EVO_SUCCESS;
}

evo_status_t evo_checkpoint_validate_config(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_result_t *result)
{
    size_t required_size = 0;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || result == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (config->checkpoint_observer == NULL) {
        return config->checkpoint_buffer == NULL &&
                       config->checkpoint_buffer_size == 0
                   ? EVO_SUCCESS
                   : EVO_ERROR_INVALID_ARGUMENT;
    }
    if (config->checkpoint_buffer == NULL ||
        problem->checkpoint_problem_identity == 0 ||
        config->checkpoint_context_identity == 0 ||
        config->max_checkpoint_bytes == 0) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (evo_selection_validate_config(config) != EVO_SUCCESS ||
        !evo_crossover_operator_is_valid(config->crossover_operator) ||
        !evo_mutation_operator_is_valid(config->mutation_operator)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    status = evo_checkpoint_size(problem, config, &required_size);
    if (status != EVO_SUCCESS) {
        return status;
    }
    if (required_size > config->max_checkpoint_bytes ||
        required_size > config->checkpoint_buffer_size) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    if (byte_ranges_overlap(config->checkpoint_buffer,
                            required_size,
                            problem,
                            sizeof(*problem)) ||
        byte_ranges_overlap(config->checkpoint_buffer,
                            required_size,
                            config,
                            sizeof(*config)) ||
        byte_ranges_overlap(config->checkpoint_buffer,
                            required_size,
                            result,
                            sizeof(*result))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    return EVO_SUCCESS;
}

static bool live_checkpoint_objects_are_independent(
    const evo_config_t *config,
    size_t checkpoint_size,
    const evo_population_t *population,
    const evo_result_t *result,
    const evo_run_state_t *state)
{
    const void *buffer = config->checkpoint_buffer;

    return !byte_ranges_overlap(buffer,
                                checkpoint_size,
                                population,
                                sizeof(*population)) &&
           !byte_ranges_overlap(buffer,
                                checkpoint_size,
                                result,
                                sizeof(*result)) &&
           !byte_ranges_overlap(buffer,
                                checkpoint_size,
                                state,
                                sizeof(*state)) &&
           !byte_ranges_overlap(buffer,
                                checkpoint_size,
                                population->genomes,
                                population->storage_bytes) &&
           !byte_ranges_overlap(buffer,
                                checkpoint_size,
                                population->evaluations,
                                population->evaluation_bytes) &&
           !byte_ranges_overlap(buffer,
                                checkpoint_size,
                                result->best_genome,
                                result->best_genome_size);
}

static bool write_header(unsigned char *bytes,
                         const checkpoint_layout_t *layout)
{
    uint64_t converted = 0;

    for (size_t index = 0; index < sizeof(checkpoint_magic); ++index) {
        bytes[index] = checkpoint_magic[index];
    }
    write_u32_at(bytes, 8, EVO_CHECKPOINT_FORMAT_VERSION);
    write_u32_at(bytes, 12, EVO_CHECKPOINT_HEADER_SIZE);
    if (!size_to_u64(layout->total_size, &converted)) {
        return false;
    }
    write_u64_at(bytes, 16, converted);
    if (!size_to_u64(layout->configuration_offset, &converted)) {
        return false;
    }
    write_u64_at(bytes, 24, converted);
    if (!size_to_u64(layout->configuration_size, &converted)) {
        return false;
    }
    write_u64_at(bytes, 32, converted);
    if (!size_to_u64(layout->state_offset, &converted)) {
        return false;
    }
    write_u64_at(bytes, 40, converted);
    if (!size_to_u64(layout->state_size, &converted)) {
        return false;
    }
    write_u64_at(bytes, 48, converted);
    if (!size_to_u64(layout->statistics_offset, &converted)) {
        return false;
    }
    write_u64_at(bytes, 56, converted);
    if (!size_to_u64(layout->statistics_size, &converted)) {
        return false;
    }
    write_u64_at(bytes, 64, converted);
    if (!size_to_u64(layout->evaluations_offset, &converted)) {
        return false;
    }
    write_u64_at(bytes, 72, converted);
    if (!size_to_u64(layout->evaluations_size, &converted)) {
        return false;
    }
    write_u64_at(bytes, 80, converted);
    if (!size_to_u64(layout->genomes_offset, &converted)) {
        return false;
    }
    write_u64_at(bytes, 88, converted);
    if (!size_to_u64(layout->genomes_size, &converted)) {
        return false;
    }
    write_u64_at(bytes, 96, converted);
    if (!size_to_u64(layout->best_genome_offset, &converted)) {
        return false;
    }
    write_u64_at(bytes, 104, converted);
    if (!size_to_u64(layout->best_genome_size, &converted)) {
        return false;
    }
    write_u64_at(bytes, 112, converted);
    write_u64_at(bytes, 120, 0);
    write_u32_at(bytes, 128, EVO_CHECKPOINT_INTEGRITY_CRC32);
    write_u32_at(bytes, 132, 0);
    write_u32_at(bytes, 136, EVO_CHECKPOINT_SECTION_COUNT);
    write_u32_at(bytes, 140, 0);
    return true;
}

static bool encode_evaluations(checkpoint_writer_t *writer,
                               const evo_population_t *population)
{
    for (size_t index = 0; index < population->population_size; ++index) {
        const evo_candidate_evaluation_t *evaluation =
            &population->evaluations[index];

        write_fitness(writer, &evaluation->fitness);
        write_bool(writer, evaluation->valid);
        write_bool(writer, evaluation->evaluated);
    }
    return writer->valid;
}

static void copy_bytes(const void *source, void *destination, size_t size)
{
    const unsigned char *source_bytes = source;
    unsigned char *destination_bytes = destination;

    for (size_t index = 0; index < size; ++index) {
        destination_bytes[index] = source_bytes[index];
    }
}

evo_status_t evo_checkpoint_emit(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *population,
    const evo_result_t *result,
    const evo_run_state_t *state)
{
    checkpoint_layout_t layout = {0};
    checkpoint_writer_t configuration_writer = {0};
    checkpoint_writer_t state_writer = {0};
    checkpoint_writer_t statistics_writer = {0};
    checkpoint_writer_t evaluations_writer = {0};
    evo_checkpoint_view_t view = {0};
    unsigned char *bytes = NULL;
    size_t valid_count = 0;
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || population == NULL ||
        result == NULL || state == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (config->checkpoint_observer == NULL) {
        return EVO_SUCCESS;
    }
    status = evo_checkpoint_validate_config(problem, config, result);
    if (status != EVO_SUCCESS) {
        return status;
    }
    if (!make_layout(problem, config, &layout) ||
        !live_checkpoint_objects_are_independent(config,
                                                 layout.total_size,
                                                 population,
                                                 result,
                                                 state)) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (!state->initialized || state->version != EVO_RUN_STATE_VERSION ||
        state->current_generation != result->generations_completed ||
        state->best_generation > state->current_generation ||
        result->generation_statistics.generation_index !=
            state->current_generation ||
        result->best_genome == NULL ||
        result->best_genome_size != problem->genome_size ||
        !evo_population_validate_completed(config,
                                           population,
                                           &valid_count) ||
        valid_count != population->valid_count) {
        return EVO_ERROR_STATE;
    }

    bytes = config->checkpoint_buffer;
    for (size_t index = 0; index < layout.total_size; ++index) {
        bytes[index] = 0;
    }
    if (!write_header(bytes, &layout)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    configuration_writer = (checkpoint_writer_t){
        .bytes = bytes,
        .capacity = layout.total_size,
        .offset = layout.configuration_offset,
        .valid = true,
    };
    state_writer = (checkpoint_writer_t){
        .bytes = bytes,
        .capacity = layout.total_size,
        .offset = layout.state_offset,
        .valid = true,
    };
    statistics_writer = (checkpoint_writer_t){
        .bytes = bytes,
        .capacity = layout.total_size,
        .offset = layout.statistics_offset,
        .valid = true,
    };
    evaluations_writer = (checkpoint_writer_t){
        .bytes = bytes,
        .capacity = layout.total_size,
        .offset = layout.evaluations_offset,
        .valid = true,
    };
    encode_configuration(&configuration_writer, problem, config);
    encode_state(&state_writer, population, result, state);
    encode_statistics(&statistics_writer, &result->generation_statistics);
    if (!encode_evaluations(&evaluations_writer, population) ||
        !configuration_writer.valid || !state_writer.valid ||
        !statistics_writer.valid ||
        configuration_writer.offset != layout.state_offset ||
        state_writer.offset != layout.statistics_offset ||
        statistics_writer.offset != layout.evaluations_offset ||
        evaluations_writer.offset != layout.genomes_offset) {
        return EVO_ERROR_STATE;
    }
    copy_bytes(population->genomes,
               bytes + layout.genomes_offset,
               layout.genomes_size);
    copy_bytes(result->best_genome,
               bytes + layout.best_genome_offset,
               layout.best_genome_size);
    layout.configuration_fingerprint =
        fingerprint_bytes(bytes + layout.configuration_offset,
                          layout.configuration_size);
    write_u64_at(bytes, 120, layout.configuration_fingerprint);
    layout.integrity_value = checkpoint_crc32(bytes, layout.total_size);
    write_u32_at(bytes, 132, layout.integrity_value);

    status = evo_checkpoint_inspect(bytes,
                                    layout.total_size,
                                    config->max_checkpoint_bytes,
                                    &view);
    if (status != EVO_SUCCESS) {
        return EVO_ERROR_STATE;
    }
    config->checkpoint_observer(bytes,
                                layout.total_size,
                                &view,
                                config->checkpoint_observer_context);
    return EVO_SUCCESS;
}

static bool configuration_matches(const evo_problem_t *problem,
                                  const evo_config_t *config,
                                  const unsigned char *checkpoint,
                                  const checkpoint_layout_t *layout)
{
    unsigned char expected[EVO_CHECKPOINT_CONFIGURATION_BUFFER_SIZE] = {0};
    checkpoint_writer_t writer = {
        .bytes = expected,
        .capacity = sizeof(expected),
        .valid = true,
    };

    encode_configuration(&writer, problem, config);
    if (!writer.valid || writer.offset != layout->configuration_size) {
        return false;
    }
    for (size_t index = 0; index < writer.offset; ++index) {
        if (expected[index] !=
            checkpoint[layout->configuration_offset + index]) {
            return false;
        }
    }
    return true;
}

static bool statistics_base_equal(
    const evo_generation_statistics_t *left,
    const evo_generation_statistics_t *right)
{
    return left->version == right->version &&
           left->generation_index == right->generation_index &&
           left->population_size == right->population_size &&
           left->valid_count == right->valid_count &&
           left->invalid_count == right->invalid_count &&
           left->best_index == right->best_index &&
           fitness_equal(&left->best_fitness, &right->best_fitness) &&
           fitness_equal(&left->fitness_sums, &right->fitness_sums) &&
           left->has_best == right->has_best &&
           left->fitness_comparison_policy_version ==
               right->fitness_comparison_policy_version &&
           left->diversity_policy_version ==
               right->diversity_policy_version &&
           left->diversity_metric_version ==
               right->diversity_metric_version &&
           left->diversity_pair_count == right->diversity_pair_count &&
           left->diversity_work_units == right->diversity_work_units &&
           left->diversity == right->diversity &&
           left->diversity_uses_domain_distance ==
               right->diversity_uses_domain_distance;
}

static void apply_population_projection(
    const checkpoint_state_projection_t *source,
    evo_population_t *population)
{
    population->valid_count = source->valid_count;
    population->best_index = source->best_index;
    population->produced_count = source->produced_count;
    population->elite_count = source->elite_count;
    population->elite_source_valid_count =
        source->elite_source_valid_count;
    population->initialization_seed = source->initialization_seed;
    population->source_generation = source->source_generation;
    population->rng_algorithm_version = source->rng_algorithm_version;
    population->operator_seed_schedule_version =
        source->operator_seed_schedule_version;
    population->selection_policy_version =
        source->selection_policy_version;
    population->selection_policy = source->selection_policy;
    population->byte_operator_policy_version =
        source->byte_operator_policy_version;
    population->crossover_operator = source->crossover_operator;
    population->mutation_operator = source->mutation_operator;
    population->mutation_rate_used = source->mutation_rate_used;
    population->odd_child_policy_version =
        source->odd_child_policy_version;
    population->elite_policy_version = source->elite_policy_version;
    population->singleton_child_policy_version =
        source->singleton_child_policy_version;
    population->fitness_comparison_policy_version =
        source->fitness_comparison_policy_version;
    population->diversity_policy_version =
        source->diversity_policy_version;
    population->diversity_metric_version =
        source->diversity_metric_version;
    population->diversity_pair_count = source->diversity_pair_count;
    population->diversity_work_units = source->diversity_work_units;
    population->diversity = source->diversity;
    population->initialized = source->population_initialized;
    population->has_best = source->has_best;
    population->evaluated = source->evaluated;
    population->elite_count_explicit = source->elite_count_explicit;
    population->diversity_uses_domain_distance =
        source->diversity_uses_domain_distance;
}

static bool decode_evaluations_to_population(
    const unsigned char *checkpoint,
    const checkpoint_layout_t *layout,
    evo_population_t *population)
{
    checkpoint_reader_t reader = {
        .bytes = checkpoint + layout->evaluations_offset,
        .size = layout->evaluations_size,
        .valid = true,
    };

    for (size_t index = 0; index < population->population_size; ++index) {
        population->evaluations[index].fitness = read_fitness(&reader);
        population->evaluations[index].valid = read_bool(&reader);
        population->evaluations[index].evaluated = read_bool(&reader);
    }
    return reader.valid && reader.offset == reader.size;
}

static bool restored_continuation_is_valid(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_population_t *population,
    const evo_result_t *result,
    const evo_run_state_t *state)
{
    const evo_candidate_evaluation_t *best_evaluation = NULL;
    const void *best_genome = NULL;
    const bool adaptive_applicable =
        evo_adaptive_mutation_is_applicable(config);
    size_t valid_count = 0;

    if (!evo_population_validate_completed(config,
                                           population,
                                           &valid_count) ||
        valid_count != population->valid_count ||
        state->version != EVO_RUN_STATE_VERSION || !state->initialized ||
        state->current_generation != result->generations_completed ||
        state->current_generation !=
            result->generation_statistics.generation_index ||
        state->current_generation > config->generation_limit ||
        state->best_generation > state->current_generation ||
        state->best_population_index >= config->population_size ||
        state->adaptive_mutation_applicable != adaptive_applicable ||
        !state->stopping.initialized ||
        !isfinite(state->stopping.significant_best_total) ||
        state->stopping.significant_best_total >
            result->best_fitness.total ||
        state->stopping.stagnant_generations >
            result->generations_completed ||
        (!config->stagnation_enabled &&
         state->stopping.stagnant_generations != 0) ||
        (state->termination_reason == EVO_TERMINATION_NONE &&
         state->current_generation >= config->generation_limit) ||
        result->termination_reason != state->termination_reason ||
        !evo_adaptive_mutation_statistics_are_valid(
            config,
            &result->generation_statistics)) {
        return false;
    }
    if (adaptive_applicable) {
        if (!state->adaptive_mutation.initialized ||
            state->adaptive_mutation.effective_rate !=
                result->generation_statistics.mutation_rate_effective ||
            state->adaptive_mutation.stagnant_generations !=
                result->generation_statistics
                    .adaptive_mutation_stagnant_generations) {
            return false;
        }
    } else if (state->adaptive_mutation.initialized ||
               state->adaptive_mutation.effective_rate != 0.0 ||
               state->adaptive_mutation.stagnant_generations != 0) {
        return false;
    }
    if (state->current_generation == 0 &&
        (state->best_generation != 0 ||
         state->best_population_index != population->best_index ||
         state->stopping.stagnant_generations != 0 ||
         state->stopping.significant_best_total !=
             result->best_fitness.total)) {
        return false;
    }
    if (state->best_generation == state->current_generation) {
        best_genome = evo_population_genome_const(
            population,
            state->best_population_index);
        best_evaluation = evo_population_evaluation_const(
            population,
            state->best_population_index);
        if (state->best_population_index != population->best_index ||
            best_genome == NULL || best_evaluation == NULL ||
            !fitness_equal(&best_evaluation->fitness,
                           &result->best_fitness)) {
            return false;
        }
        for (size_t index = 0; index < problem->genome_size; ++index) {
            if (((const unsigned char *)best_genome)[index] !=
                ((const unsigned char *)result->best_genome)[index]) {
                return false;
            }
        }
    }
    return true;
}

static evo_status_t destroy_restored_with_status(
    evo_population_t *population,
    evo_result_t *result,
    evo_status_t status)
{
    evo_population_destroy(population);
    evo_result_destroy(result);
    return status;
}

evo_status_t evo_checkpoint_restore(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const void *checkpoint,
    size_t checkpoint_size,
    evo_population_t *population,
    evo_result_t *result,
    evo_run_state_t *state)
{
    checkpoint_layout_t layout = {0};
    checkpoint_state_projection_t projection = {0};
    checkpoint_reader_t state_reader = {0};
    checkpoint_reader_t statistics_reader = {0};
    evo_checkpoint_view_t view = {0};
    evo_population_t restored_population = {0};
    evo_result_t restored_result = {0};
    evo_generation_statistics_t restored_statistics = {0};
    evo_generation_statistics_t expected_statistics = {0};
    evo_status_t status = EVO_SUCCESS;

    if (problem == NULL || config == NULL || population == NULL ||
        result == NULL || state == NULL || checkpoint == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    if (population->genomes != NULL || population->evaluations != NULL ||
        result->best_genome != NULL || state->initialized ||
        byte_ranges_overlap(population,
                            sizeof(*population),
                            result,
                            sizeof(*result)) ||
        byte_ranges_overlap(population,
                            sizeof(*population),
                            state,
                            sizeof(*state)) ||
        byte_ranges_overlap(result,
                            sizeof(*result),
                            state,
                            sizeof(*state)) ||
        byte_ranges_overlap(checkpoint,
                            checkpoint_size,
                            population,
                            sizeof(*population)) ||
        byte_ranges_overlap(checkpoint,
                            checkpoint_size,
                            result,
                            sizeof(*result)) ||
        byte_ranges_overlap(checkpoint,
                            checkpoint_size,
                            state,
                            sizeof(*state))) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    *population = (evo_population_t){0};
    *result = (evo_result_t){0};
    *state = (evo_run_state_t){0};
    if (config->max_checkpoint_bytes == 0 ||
        checkpoint_size > config->max_checkpoint_bytes) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    if (problem->checkpoint_problem_identity == 0 ||
        config->checkpoint_context_identity == 0 ||
        problem->evaluate == NULL) {
        return EVO_ERROR_INVALID_ARGUMENT;
    }
    status = evo_bounded_run_validate_config(problem, config);
    if (status != EVO_SUCCESS) {
        return status;
    }
    if (evo_selection_validate_config(config) != EVO_SUCCESS ||
        !evo_crossover_operator_is_valid(config->crossover_operator) ||
        !evo_mutation_operator_is_valid(config->mutation_operator)) {
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    status = evo_checkpoint_validate_config(problem, config, result);
    if (status != EVO_SUCCESS) {
        return status;
    }
    status = evo_checkpoint_inspect(checkpoint,
                                    checkpoint_size,
                                    config->max_checkpoint_bytes,
                                    &view);
    if (status != EVO_SUCCESS) {
        return status;
    }
    status = parse_layout(checkpoint,
                          checkpoint_size,
                          config->max_checkpoint_bytes,
                          &layout);
    if (status != EVO_SUCCESS) {
        return status;
    }
    if (!configuration_matches(problem,
                               config,
                               checkpoint,
                               &layout)) {
        return EVO_ERROR_CHECKPOINT_MISMATCH;
    }
    state_reader = (checkpoint_reader_t){
        .bytes = (const unsigned char *)checkpoint + layout.state_offset,
        .size = layout.state_size,
        .valid = true,
    };
    statistics_reader = (checkpoint_reader_t){
        .bytes = (const unsigned char *)checkpoint +
                 layout.statistics_offset,
        .size = layout.statistics_size,
        .valid = true,
    };
    if (!decode_state(&state_reader, &projection) ||
        !decode_statistics(&statistics_reader, &restored_statistics)) {
        return EVO_ERROR_CHECKPOINT_INVALID;
    }

    status = evo_population_create(problem,
                                   config,
                                   &restored_population);
    if (status != EVO_SUCCESS) {
        return status;
    }
    status = evo_population_restore_evaluations_allocate(
        config,
        &restored_population);
    if (status != EVO_SUCCESS) {
        return destroy_restored_with_status(&restored_population,
                                            &restored_result,
                                            status);
    }
    copy_bytes((const unsigned char *)checkpoint + layout.genomes_offset,
               restored_population.genomes,
               layout.genomes_size);
    if (!decode_evaluations_to_population(checkpoint,
                                          &layout,
                                          &restored_population)) {
        return destroy_restored_with_status(&restored_population,
                                            &restored_result,
                                            EVO_ERROR_CHECKPOINT_INVALID);
    }
    apply_population_projection(&projection, &restored_population);

    status = evo_result_storage_allocate(problem,
                                         config,
                                         &restored_result);
    if (status != EVO_SUCCESS) {
        return destroy_restored_with_status(&restored_population,
                                            &restored_result,
                                            status);
    }
    copy_bytes((const unsigned char *)checkpoint +
                   layout.best_genome_offset,
               restored_result.best_genome,
               problem->genome_size);
    restored_result.best_fitness = projection.global_best_fitness;
    restored_result.generations_completed =
        (size_t)projection.run.current_generation;
    restored_result.random_seed = projection.result_random_seed;
    restored_result.termination_reason =
        projection.run.termination_reason;
    restored_result.generation_statistics = restored_statistics;

    status = evo_generation_statistics_record(
        config,
        &restored_population,
        projection.run.current_generation,
        &expected_statistics);
    if (status != EVO_SUCCESS ||
        !statistics_base_equal(&expected_statistics,
                               &restored_statistics) ||
        !restored_continuation_is_valid(problem,
                                        config,
                                        &restored_population,
                                        &restored_result,
                                        &projection.run)) {
        return destroy_restored_with_status(&restored_population,
                                            &restored_result,
                                            EVO_ERROR_CHECKPOINT_INVALID);
    }

    *population = restored_population;
    *result = restored_result;
    *state = projection.run;
    return EVO_SUCCESS;
}
