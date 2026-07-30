#include "research/seed_schedule_research.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define EVO_CORPUS_SEED_COUNT ((size_t)4)
#define EVO_CORPUS_GENERATION_COUNT ((size_t)8)
#define EVO_CORPUS_POPULATION_COUNT ((size_t)32)
#define EVO_CORPUS_DOMAIN_COUNT ((size_t)4)
#define EVO_CORPUS_SIZE                                    \
    (EVO_CORPUS_SEED_COUNT * EVO_CORPUS_GENERATION_COUNT * \
     EVO_CORPUS_POPULATION_COUNT * EVO_CORPUS_DOMAIN_COUNT)
#define EVO_PREFIX_LENGTH ((size_t)4)
#define EVO_FAST_PERFORMANCE_ROUNDS ((size_t)4096)
#define EVO_ELLIPTIC_PERFORMANCE_ROUNDS ((size_t)32)
#define EVO_AVALANCHE_BUCKET_COUNT ((size_t)17)
#define EVO_DOMAIN_PAIR_COUNT ((size_t)6)

typedef struct schedule_pair {
    uint64_t state;
    uint64_t increment;
} schedule_pair_t;

typedef struct stream_prefix {
    uint32_t values[EVO_PREFIX_LENGTH];
} stream_prefix_t;

typedef struct domain_pair_metrics {
    uint64_t changed_bits;
    uint64_t pairs;
    long double prefix_correlation;
} domain_pair_metrics_t;

typedef struct experiment_metrics {
    size_t schedule_collisions;
    size_t prefix_collisions;
    uint64_t state_one_bits;
    uint64_t increment_one_bits;
    uint64_t avalanche_changed_bits;
    uint64_t avalanche_pairs;
    uint64_t avalanche_histogram[EVO_AVALANCHE_BUCKET_COUNT];
    domain_pair_metrics_t domain_pairs[EVO_DOMAIN_PAIR_COUNT];
    clock_t derivation_ticks;
    uint64_t derivations_timed;
} experiment_metrics_t;

typedef struct domain_pair_definition {
    evo_research_seed_domain_t left;
    evo_research_seed_domain_t right;
    const char *name;
} domain_pair_definition_t;

static const uint64_t corpus_seeds[EVO_CORPUS_SEED_COUNT] = {
    UINT64_C(0),
    UINT64_C(1),
    UINT64_C(42),
    UINT64_MAX,
};

static const domain_pair_definition_t
    domain_pair_definitions[EVO_DOMAIN_PAIR_COUNT] = {
        {
            EVO_RESEARCH_DOMAIN_INITIALIZATION,
            EVO_RESEARCH_DOMAIN_SELECTION,
            "initialization-selection",
        },
        {
            EVO_RESEARCH_DOMAIN_INITIALIZATION,
            EVO_RESEARCH_DOMAIN_CROSSOVER,
            "initialization-crossover",
        },
        {
            EVO_RESEARCH_DOMAIN_INITIALIZATION,
            EVO_RESEARCH_DOMAIN_MUTATION,
            "initialization-mutation",
        },
        {
            EVO_RESEARCH_DOMAIN_SELECTION,
            EVO_RESEARCH_DOMAIN_CROSSOVER,
            "selection-crossover",
        },
        {
            EVO_RESEARCH_DOMAIN_SELECTION,
            EVO_RESEARCH_DOMAIN_MUTATION,
            "selection-mutation",
        },
        {
            EVO_RESEARCH_DOMAIN_CROSSOVER,
            EVO_RESEARCH_DOMAIN_MUTATION,
            "crossover-mutation",
        },
};

static const char *candidate_name(evo_research_seed_candidate_t candidate)
{
    switch (candidate) {
    case EVO_RESEARCH_CANDIDATE_V1_BASELINE:
        return "v1-baseline";
    case EVO_RESEARCH_CANDIDATE_MIXED_CONTROL:
        return "mixed-control";
    case EVO_RESEARCH_CANDIDATE_PRIME_INDEXED:
        return "prime-indexed";
    case EVO_RESEARCH_CANDIDATE_ELLIPTIC:
        return "elliptic";
    default:
        return "invalid";
    }
}

static unsigned int count_one_bits_64(uint64_t value)
{
    unsigned int count = 0;

    while (value != 0) {
        count += (unsigned int)(value & UINT64_C(1));
        value >>= 1u;
    }

    return count;
}

static int compare_schedules(const void *left_pointer,
                             const void *right_pointer)
{
    const schedule_pair_t *left = left_pointer;
    const schedule_pair_t *right = right_pointer;

    if (left->state < right->state) {
        return -1;
    }
    if (left->state > right->state) {
        return 1;
    }
    if (left->increment < right->increment) {
        return -1;
    }
    if (left->increment > right->increment) {
        return 1;
    }
    return 0;
}

static int compare_prefixes(const void *left_pointer,
                            const void *right_pointer)
{
    const stream_prefix_t *left = left_pointer;
    const stream_prefix_t *right = right_pointer;

    for (size_t index = 0; index < EVO_PREFIX_LENGTH; ++index) {
        if (left->values[index] < right->values[index]) {
            return -1;
        }
        if (left->values[index] > right->values[index]) {
            return 1;
        }
    }

    return 0;
}

static size_t count_schedule_collisions(schedule_pair_t *schedules)
{
    size_t collisions = 0;

    qsort(
        schedules,
        EVO_CORPUS_SIZE,
        sizeof(schedules[0]),
        compare_schedules);
    for (size_t index = 1; index < EVO_CORPUS_SIZE; ++index) {
        if (compare_schedules(&schedules[index - 1], &schedules[index]) ==
            0) {
            ++collisions;
        }
    }

    return collisions;
}

static size_t count_prefix_collisions(stream_prefix_t *prefixes)
{
    size_t collisions = 0;

    qsort(
        prefixes,
        EVO_CORPUS_SIZE,
        sizeof(prefixes[0]),
        compare_prefixes);
    for (size_t index = 1; index < EVO_CORPUS_SIZE; ++index) {
        if (compare_prefixes(&prefixes[index - 1], &prefixes[index]) == 0) {
            ++collisions;
        }
    }

    return collisions;
}

static bool derive(
    evo_research_seed_candidate_t candidate,
    const evo_research_seed_tuple_t *tuple,
    const uint32_t *primes,
    evo_research_pcg_schedule_t *schedule)
{
    return evo_research_derive_schedule(
        candidate,
        tuple,
        primes,
        EVO_RESEARCH_PRIME_VECTOR_COUNT,
        schedule);
}

static bool measure_corpus(
    evo_research_seed_candidate_t candidate,
    const uint32_t *primes,
    experiment_metrics_t *metrics)
{
    schedule_pair_t schedules[EVO_CORPUS_SIZE] = {0};
    stream_prefix_t prefixes[EVO_CORPUS_SIZE] = {0};
    size_t corpus_index = 0;

    for (size_t seed_index = 0; seed_index < EVO_CORPUS_SEED_COUNT;
         ++seed_index) {
        for (size_t generation = 0;
             generation < EVO_CORPUS_GENERATION_COUNT;
             ++generation) {
            for (size_t population_index = 0;
                 population_index < EVO_CORPUS_POPULATION_COUNT;
                 ++population_index) {
                for (size_t domain = 1;
                     domain <= EVO_CORPUS_DOMAIN_COUNT;
                     ++domain) {
                    evo_research_seed_tuple_t tuple = {
                        .master_seed = corpus_seeds[seed_index],
                        .generation = generation,
                        .population_index = population_index,
                        .domain =
                            (evo_research_seed_domain_t)domain,
                    };
                    evo_research_pcg_schedule_t schedule = {0};
                    evo_research_pcg_schedule_t stream = {0};

                    if (!derive(candidate, &tuple, primes, &schedule) ||
                        (schedule.increment & UINT64_C(1)) == 0) {
                        return false;
                    }

                    schedules[corpus_index].state = schedule.state;
                    schedules[corpus_index].increment =
                        schedule.increment;
                    metrics->state_one_bits +=
                        count_one_bits_64(schedule.state);
                    metrics->increment_one_bits +=
                        count_one_bits_64(schedule.increment);

                    stream = schedule;
                    for (size_t prefix_index = 0;
                         prefix_index < EVO_PREFIX_LENGTH;
                         ++prefix_index) {
                        prefixes[corpus_index].values[prefix_index] =
                            evo_research_schedule_next_u32(&stream);
                    }
                    ++corpus_index;
                }
            }
        }
    }

    metrics->schedule_collisions =
        count_schedule_collisions(schedules);
    metrics->prefix_collisions = count_prefix_collisions(prefixes);
    return true;
}

static bool measure_avalanche(
    evo_research_seed_candidate_t candidate,
    const uint32_t *primes,
    experiment_metrics_t *metrics)
{
    for (size_t seed_index = 0; seed_index < EVO_CORPUS_SEED_COUNT;
         ++seed_index) {
        for (uint64_t generation = 0; generation < 2; ++generation) {
            for (uint64_t population_index = 0;
                 population_index < 4;
                 ++population_index) {
                for (size_t domain = 1;
                     domain <= EVO_CORPUS_DOMAIN_COUNT;
                     ++domain) {
                    evo_research_seed_tuple_t tuple = {
                        .master_seed = corpus_seeds[seed_index],
                        .generation = generation,
                        .population_index = population_index,
                        .domain =
                            (evo_research_seed_domain_t)domain,
                    };
                    evo_research_pcg_schedule_t baseline = {0};

                    if (!derive(candidate, &tuple, primes, &baseline)) {
                        return false;
                    }

                    for (unsigned int bit = 0; bit < 64u; ++bit) {
                        evo_research_pcg_schedule_t changed = {0};
                        unsigned int changed_bits = 0;
                        size_t histogram_bucket = 0;

                        tuple.master_seed ^=
                            UINT64_C(1) << bit;
                        if (!derive(candidate, &tuple, primes, &changed)) {
                            return false;
                        }
                        tuple.master_seed ^=
                            UINT64_C(1) << bit;

                        changed_bits =
                            count_one_bits_64(
                                baseline.state ^ changed.state) +
                            count_one_bits_64(
                                baseline.increment ^ changed.increment);
                        metrics->avalanche_changed_bits += changed_bits;
                        ++metrics->avalanche_pairs;

                        histogram_bucket = changed_bits / 8u;
                        if (histogram_bucket >=
                            EVO_AVALANCHE_BUCKET_COUNT) {
                            histogram_bucket =
                                EVO_AVALANCHE_BUCKET_COUNT - 1;
                        }
                        ++metrics
                              ->avalanche_histogram[histogram_bucket];
                    }
                }
            }
        }
    }

    return true;
}

static bool measure_domain_relationship(
    evo_research_seed_candidate_t candidate,
    const uint32_t *primes,
    experiment_metrics_t *metrics)
{
    for (size_t pair_index = 0; pair_index < EVO_DOMAIN_PAIR_COUNT;
         ++pair_index) {
        long double sample_count = 0;
        long double sum_left = 0;
        long double sum_right = 0;
        long double sum_left_squared = 0;
        long double sum_right_squared = 0;
        long double sum_product = 0;

        for (size_t seed_index = 0; seed_index < EVO_CORPUS_SEED_COUNT;
             ++seed_index) {
            for (uint64_t generation = 0;
                 generation < EVO_CORPUS_GENERATION_COUNT;
                 ++generation) {
                for (uint64_t population_index = 0;
                     population_index < EVO_CORPUS_POPULATION_COUNT;
                     ++population_index) {
                    evo_research_seed_tuple_t tuple = {
                        .master_seed = corpus_seeds[seed_index],
                        .generation = generation,
                        .population_index = population_index,
                        .domain =
                            domain_pair_definitions[pair_index].left,
                    };
                    evo_research_pcg_schedule_t left_schedule = {0};
                    evo_research_pcg_schedule_t right_schedule = {0};
                    long double left = 0;
                    long double right = 0;

                    if (!derive(
                            candidate,
                            &tuple,
                            primes,
                            &left_schedule)) {
                        return false;
                    }
                    tuple.domain =
                        domain_pair_definitions[pair_index].right;
                    if (!derive(
                            candidate,
                            &tuple,
                            primes,
                            &right_schedule)) {
                        return false;
                    }

                    metrics->domain_pairs[pair_index].changed_bits +=
                        count_one_bits_64(
                            left_schedule.state ^
                            right_schedule.state);
                    metrics->domain_pairs[pair_index].changed_bits +=
                        count_one_bits_64(
                            left_schedule.increment ^
                            right_schedule.increment);
                    ++metrics->domain_pairs[pair_index].pairs;

                    left = evo_research_schedule_next_u32(
                        &left_schedule);
                    right = evo_research_schedule_next_u32(
                        &right_schedule);
                    sample_count += 1;
                    sum_left += left;
                    sum_right += right;
                    sum_left_squared += left * left;
                    sum_right_squared += right * right;
                    sum_product += left * right;
                }
            }
        }

        {
            const long double numerator =
                sample_count * sum_product - sum_left * sum_right;
            const long double left_variance =
                sample_count * sum_left_squared -
                sum_left * sum_left;
            const long double right_variance =
                sample_count * sum_right_squared -
                sum_right * sum_right;
            const long double denominator =
                sqrtl(left_variance * right_variance);

            if (denominator == 0) {
                metrics->domain_pairs[pair_index]
                    .prefix_correlation = numerator == 0 ? 1 : 0;
            } else {
                metrics->domain_pairs[pair_index]
                    .prefix_correlation = numerator / denominator;
            }
        }
    }

    return true;
}

static bool measure_performance(
    evo_research_seed_candidate_t candidate,
    const uint32_t *primes,
    experiment_metrics_t *metrics)
{
    volatile uint64_t sink = 0;
    const size_t performance_rounds =
        candidate == EVO_RESEARCH_CANDIDATE_ELLIPTIC
            ? EVO_ELLIPTIC_PERFORMANCE_ROUNDS
            : EVO_FAST_PERFORMANCE_ROUNDS;
    const clock_t start = clock();

    for (size_t round = 0; round < performance_rounds; ++round) {
        for (size_t seed_index = 0; seed_index < EVO_CORPUS_SEED_COUNT;
             ++seed_index) {
            for (uint64_t generation = 0;
                 generation < EVO_CORPUS_GENERATION_COUNT;
                 ++generation) {
                for (uint64_t population_index = 0;
                     population_index < EVO_CORPUS_POPULATION_COUNT;
                     ++population_index) {
                    for (size_t domain = 1;
                         domain <= EVO_CORPUS_DOMAIN_COUNT;
                         ++domain) {
                        evo_research_seed_tuple_t tuple = {
                            .master_seed = corpus_seeds[seed_index],
                            .generation = generation,
                            .population_index = population_index,
                            .domain =
                                (evo_research_seed_domain_t)domain,
                        };
                        evo_research_pcg_schedule_t schedule = {0};

                        if (!derive(
                                candidate,
                                &tuple,
                                primes,
                                &schedule)) {
                            return false;
                        }
                        sink ^= schedule.state;
                        sink ^= schedule.increment;
                        ++metrics->derivations_timed;
                    }
                }
            }
        }
    }

    metrics->derivation_ticks = clock() - start;
    if (sink == UINT64_C(0x6a09e667f3bcc909)) {
        return false;
    }
    return true;
}

static bool measure_candidate(
    evo_research_seed_candidate_t candidate,
    const uint32_t *primes,
    experiment_metrics_t *metrics)
{
    *metrics = (experiment_metrics_t){0};
    return measure_corpus(candidate, primes, metrics) &&
           measure_avalanche(candidate, primes, metrics) &&
           measure_domain_relationship(candidate, primes, metrics) &&
           measure_performance(candidate, primes, metrics);
}

static uint64_t prime_vector_fnv1a64(const uint32_t *primes)
{
    uint64_t digest = UINT64_C(14695981039346656037);

    for (size_t index = 0; index < EVO_RESEARCH_PRIME_VECTOR_COUNT;
         ++index) {
        uint32_t value = primes[index];

        for (unsigned int byte = 0; byte < 4u; ++byte) {
            digest ^= value & UINT32_C(0xff);
            digest *= UINT64_C(1099511628211);
            value >>= 8u;
        }
    }

    return digest;
}

static const char *compiler_family(void)
{
#if defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#else
    return "unknown";
#endif
}

static const char *platform_family(void)
{
#if defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

static bool print_fixed_vectors(const uint32_t *primes)
{
    const evo_research_seed_tuple_t tuple = {
        .master_seed = 42,
        .generation = 7,
        .population_index = 11,
        .domain = EVO_RESEARCH_DOMAIN_MUTATION,
    };

    printf(
        "  \"fixed_tuple\": {\"master_seed\": 42, \"generation\": 7, "
        "\"population_index\": 11, \"domain\": \"mutation\"},\n");
    printf("  \"fixed_vectors\": [\n");
    for (evo_research_seed_candidate_t candidate =
             EVO_RESEARCH_CANDIDATE_V1_BASELINE;
         candidate <= EVO_RESEARCH_CANDIDATE_ELLIPTIC;
         candidate = (evo_research_seed_candidate_t)(candidate + 1)) {
        evo_research_pcg_schedule_t schedule = {0};
        evo_research_pcg_schedule_t stream = {0};
        uint32_t prefix[EVO_PREFIX_LENGTH] = {0};

        if (!derive(candidate, &tuple, primes, &schedule)) {
            return false;
        }
        stream = schedule;
        for (size_t index = 0; index < EVO_PREFIX_LENGTH; ++index) {
            prefix[index] =
                evo_research_schedule_next_u32(&stream);
        }

        printf(
            "    {\"candidate\": \"%s\", \"state\": "
            "\"%016" PRIx64 "\", \"increment\": \"%016" PRIx64
            "\", \"prefix_u32\": [\"%08" PRIx32 "\", \"%08" PRIx32
            "\", \"%08" PRIx32 "\", \"%08" PRIx32 "\"]}%s\n",
            candidate_name(candidate),
            schedule.state,
            schedule.increment,
            prefix[0],
            prefix[1],
            prefix[2],
            prefix[3],
            candidate == EVO_RESEARCH_CANDIDATE_ELLIPTIC ? "" : ",");
    }
    printf("  ],\n");

    printf("  \"curve_fixed_points\": [\n");
    {
        static const uint32_t scalars[] = {
            1u,
            2u,
            3u,
            17u,
            65537u,
        };

        for (size_t index = 0;
             index < sizeof(scalars) / sizeof(scalars[0]);
             ++index) {
            evo_research_curve_point_t point = {0};

            if (!evo_research_curve_multiply(
                    scalars[index],
                    &point) ||
                point.infinity) {
                return false;
            }
            printf(
                "    {\"scalar\": %u, \"x\": %u, \"y\": %u}%s\n",
                scalars[index],
                point.x,
                point.y,
                index + 1 == sizeof(scalars) / sizeof(scalars[0])
                    ? ""
                    : ",");
        }
    }
    printf("  ],\n");
    return true;
}

static void print_metrics(
    evo_research_seed_candidate_t candidate,
    const experiment_metrics_t *metrics,
    bool final_candidate)
{
    const long double total_component_bits =
        (long double)EVO_CORPUS_SIZE * 64.0L;
    const long double state_balance =
        metrics->state_one_bits / total_component_bits;
    const long double increment_balance =
        metrics->increment_one_bits / total_component_bits;
    const long double avalanche_mean =
        metrics->avalanche_pairs == 0
            ? 0
            : (long double)metrics->avalanche_changed_bits /
                  metrics->avalanche_pairs;
    const long double derivations_per_second =
        metrics->derivation_ticks == 0
            ? 0
            : (long double)metrics->derivations_timed *
                  CLOCKS_PER_SEC / metrics->derivation_ticks;

    printf("    {\n");
    printf("      \"candidate\": \"%s\",\n", candidate_name(candidate));
    printf(
        "      \"schedule_collisions\": %zu,\n",
        metrics->schedule_collisions);
    printf(
        "      \"four_word_prefix_collisions\": %zu,\n",
        metrics->prefix_collisions);
    printf(
        "      \"state_one_bit_fraction\": %.12Lf,\n",
        state_balance);
    printf(
        "      \"increment_one_bit_fraction\": %.12Lf,\n",
        increment_balance);
    printf(
        "      \"master_seed_flip_mean_hamming_128\": %.12Lf,\n",
        avalanche_mean);
    printf("      \"master_seed_flip_histogram_8_bit_buckets\": [");
    for (size_t bucket = 0; bucket < EVO_AVALANCHE_BUCKET_COUNT;
         ++bucket) {
        printf(
            "%" PRIu64 "%s",
            metrics->avalanche_histogram[bucket],
            bucket + 1 == EVO_AVALANCHE_BUCKET_COUNT ? "" : ", ");
    }
    printf("],\n");
    printf("      \"domain_pair_metrics\": [\n");
    for (size_t pair_index = 0; pair_index < EVO_DOMAIN_PAIR_COUNT;
         ++pair_index) {
        const domain_pair_metrics_t *pair =
            &metrics->domain_pairs[pair_index];
        const long double hamming_mean =
            pair->pairs == 0
                ? 0
                : (long double)pair->changed_bits / pair->pairs;

        printf(
            "        {\"pair\": \"%s\", \"mean_hamming_128\": "
            "%.12Lf, \"prefix_correlation\": %.12Lf}%s\n",
            domain_pair_definitions[pair_index].name,
            hamming_mean,
            pair->prefix_correlation,
            pair_index + 1 == EVO_DOMAIN_PAIR_COUNT ? "" : ",");
    }
    printf("      ],\n");
    printf(
        "      \"timed_derivations\": %" PRIu64 ",\n",
        metrics->derivations_timed);
    printf(
        "      \"clock_ticks\": %" PRIuMAX ",\n",
        (uintmax_t)metrics->derivation_ticks);
    printf(
        "      \"derivations_per_cpu_second\": %.3Lf\n",
        derivations_per_second);
    printf("    }%s\n", final_candidate ? "" : ",");
}

int main(void)
{
    uint32_t primes[EVO_RESEARCH_PRIME_VECTOR_COUNT] = {0};

    if (!evo_research_generate_prime_vector(
            primes,
            EVO_RESEARCH_PRIME_VECTOR_COUNT)) {
        return 1;
    }

    printf("{\n");
    printf("  \"schema_version\": \"1.0.0\",\n");
    printf("  \"experiment\": \"EVO-RNG-001\",\n");
    printf("  \"rng_algorithm_version\": 1,\n");
    printf("  \"research_schedule_version\": 1,\n");
    printf(
        "  \"code_noodling_commit\": "
        "\"43c4b386acfcc634f1d62e96a5b7809e96d8a1ec\",\n");
    printf(
        "  \"prime_vector\": {\"count\": %zu, \"first\": %u, "
        "\"last\": %u, \"encoding\": \"little-endian-uint32\", "
        "\"sha256\": \"%s\", \"fnv1a64\": \"%016" PRIx64 "\"},\n",
        EVO_RESEARCH_PRIME_VECTOR_COUNT,
        primes[0],
        primes[EVO_RESEARCH_PRIME_VECTOR_COUNT - 1],
        EVO_RESEARCH_PRIME_VECTOR_SHA256,
        prime_vector_fnv1a64(primes));
    printf(
        "  \"curve\": {\"field_prime\": %u, \"a\": %u, \"b\": %u, "
        "\"base_x\": 0, \"base_y\": 1},\n",
        EVO_RESEARCH_FIELD_PRIME,
        EVO_RESEARCH_CURVE_A,
        EVO_RESEARCH_CURVE_B);
    printf(
        "  \"corpus\": {\"master_seeds\": 4, \"generations\": 8, "
        "\"population_indices\": 32, \"domains\": 4, "
        "\"tuples\": %zu},\n",
        EVO_CORPUS_SIZE);
    if (!print_fixed_vectors(primes)) {
        return 1;
    }
    printf(
        "  \"environment\": {\"platform\": \"%s\", "
        "\"compiler_family\": \"%s\", \"compiler_version\": \"%s\", "
        "\"c_standard\": %ld, \"clock_ticks_per_second\": %ld},\n",
        platform_family(),
        compiler_family(),
        __VERSION__,
        (long)__STDC_VERSION__,
        (long)CLOCKS_PER_SEC);
    printf("  \"candidates\": [\n");

    for (evo_research_seed_candidate_t candidate =
             EVO_RESEARCH_CANDIDATE_V1_BASELINE;
         candidate <= EVO_RESEARCH_CANDIDATE_ELLIPTIC;
         candidate = (evo_research_seed_candidate_t)(candidate + 1)) {
        experiment_metrics_t metrics = {0};

        if (!measure_candidate(candidate, primes, &metrics)) {
            return 1;
        }
        print_metrics(
            candidate,
            &metrics,
            candidate == EVO_RESEARCH_CANDIDATE_ELLIPTIC);
    }

    printf("  ]\n");
    printf("}\n");
    return 0;
}
