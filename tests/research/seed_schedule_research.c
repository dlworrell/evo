#include "research/seed_schedule_research.h"

#include "internal/rng.h"

#include <limits.h>

#define EVO_PCG32_MULTIPLIER UINT64_C(6364136223846793005)

#define EVO_GENERATION_TAG UINT64_C(0x9e3779b97f4a7c15)
#define EVO_POPULATION_TAG UINT64_C(0xd1b54a32d192ed03)
#define EVO_DOMAIN_TAG UINT64_C(0x94d049bb133111eb)
#define EVO_STATE_TAG UINT64_C(0xa0761d6478bd642f)
#define EVO_STREAM_TAG UINT64_C(0xe7037ed1a0b428db)
#define EVO_ELLIPTIC_TAG UINT64_C(0x8ebc6af09c88c6e3)
#define EVO_INFINITY_TAG UINT64_C(0x589965cc75374cc3)

static uint64_t rotate_left_64(uint64_t value, unsigned int distance)
{
    const unsigned int normalized_distance = distance & 63u;

    return (value << normalized_distance) |
           (value >> ((64u - normalized_distance) & 63u));
}

static uint64_t mix_64(uint64_t value)
{
    value ^= value >> 30u;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27u;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31u;
    return value;
}

static bool domain_is_valid(evo_research_seed_domain_t domain)
{
    return domain >= EVO_RESEARCH_DOMAIN_INITIALIZATION &&
           domain <= EVO_RESEARCH_DOMAIN_MUTATION;
}

static uint64_t tuple_material(const evo_research_seed_tuple_t *tuple)
{
    return mix_64(
        tuple->master_seed ^
        mix_64(tuple->generation ^ EVO_GENERATION_TAG) ^
        mix_64(tuple->population_index ^ EVO_POPULATION_TAG) ^
        mix_64((uint64_t)tuple->domain ^ EVO_DOMAIN_TAG));
}

static bool candidate_is_prime(uint32_t candidate,
                               const uint32_t *known_primes,
                               size_t known_count)
{
    for (size_t index = 0; index < known_count; ++index) {
        const uint32_t divisor = known_primes[index];

        if (divisor > candidate / divisor) {
            break;
        }
        if (candidate % divisor == 0u) {
            return false;
        }
    }

    return true;
}

bool evo_research_generate_prime_vector(uint32_t *primes, size_t prime_count)
{
    uint32_t candidate = 5;
    uint32_t step = 2;
    size_t generated = 0;

    if (prime_count == 0) {
        return true;
    }
    if (primes == NULL) {
        return false;
    }

    primes[generated] = 2;
    ++generated;
    if (generated == prime_count) {
        return true;
    }

    primes[generated] = 3;
    ++generated;
    while (generated < prime_count) {
        if (candidate_is_prime(candidate, primes, generated)) {
            primes[generated] = candidate;
            ++generated;
        }

        if (candidate > UINT32_MAX - step) {
            return false;
        }
        candidate += step;
        step = 6u - step;
    }

    return true;
}

static uint32_t field_add(uint32_t left, uint32_t right)
{
    return (uint32_t)(((uint64_t)left + right) %
                      EVO_RESEARCH_FIELD_PRIME);
}

static uint32_t field_subtract(uint32_t left, uint32_t right)
{
    if (left >= right) {
        return left - right;
    }

    return EVO_RESEARCH_FIELD_PRIME - (right - left);
}

static uint32_t field_multiply(uint32_t left, uint32_t right)
{
    return (uint32_t)(((uint64_t)left * right) %
                      EVO_RESEARCH_FIELD_PRIME);
}

static uint32_t field_power(uint32_t base, uint32_t exponent)
{
    uint32_t result = 1;

    while (exponent != 0u) {
        if ((exponent & 1u) != 0u) {
            result = field_multiply(result, base);
        }
        base = field_multiply(base, base);
        exponent >>= 1u;
    }

    return result;
}

static uint32_t field_inverse(uint32_t value)
{
    return field_power(value, EVO_RESEARCH_FIELD_PRIME - 2u);
}

static evo_research_curve_point_t curve_infinity(void)
{
    const evo_research_curve_point_t infinity = {
        .x = 0,
        .y = 0,
        .infinity = true,
    };

    return infinity;
}

static evo_research_curve_point_t curve_add(
    evo_research_curve_point_t left,
    evo_research_curve_point_t right)
{
    evo_research_curve_point_t result = {0};
    uint32_t numerator = 0;
    uint32_t denominator = 0;
    uint32_t slope = 0;

    if (left.infinity) {
        return right;
    }
    if (right.infinity) {
        return left;
    }

    if (left.x == right.x) {
        if (field_add(left.y, right.y) == 0u) {
            return curve_infinity();
        }

        numerator = field_add(
            field_multiply(3u, field_multiply(left.x, left.x)),
            EVO_RESEARCH_CURVE_A);
        denominator = field_multiply(2u, left.y);
    } else {
        numerator = field_subtract(right.y, left.y);
        denominator = field_subtract(right.x, left.x);
    }

    if (denominator == 0u) {
        return curve_infinity();
    }

    slope = field_multiply(numerator, field_inverse(denominator));
    result.x = field_subtract(
        field_subtract(field_multiply(slope, slope), left.x),
        right.x);
    result.y = field_subtract(
        field_multiply(slope, field_subtract(left.x, result.x)),
        left.y);
    result.infinity = false;
    return result;
}

bool evo_research_curve_multiply(uint32_t scalar,
                                 evo_research_curve_point_t *point)
{
    evo_research_curve_point_t accumulator = curve_infinity();
    evo_research_curve_point_t addend = {
        .x = 0,
        .y = 1,
        .infinity = false,
    };

    if (point == NULL) {
        return false;
    }

    while (scalar != 0u) {
        if ((scalar & 1u) != 0u) {
            accumulator = curve_add(accumulator, addend);
        }
        addend = curve_add(addend, addend);
        scalar >>= 1u;
    }

    *point = accumulator;
    return true;
}

static bool derive_v1_baseline(const evo_research_seed_tuple_t *tuple,
                               evo_research_pcg_schedule_t *schedule)
{
    evo_rng_t rng = {0};

    if (!evo_rng_seed(&rng, tuple->master_seed)) {
        return false;
    }

    schedule->state = rng.state;
    schedule->increment = rng.increment;
    return true;
}

static void derive_mixed_control(const evo_research_seed_tuple_t *tuple,
                                 evo_research_pcg_schedule_t *schedule)
{
    const uint64_t material = tuple_material(tuple);

    schedule->state = mix_64(material ^ EVO_STATE_TAG);
    schedule->increment =
        (mix_64(rotate_left_64(material, 23u) ^ EVO_STREAM_TAG) << 1u) |
        UINT64_C(1);
}

static bool prime_index_is_available(
    const evo_research_seed_tuple_t *tuple,
    const uint32_t *primes,
    size_t prime_count)
{
    if (primes == NULL || prime_count < 3) {
        return false;
    }

    return tuple->population_index <= (uint64_t)(prime_count - 3);
}

static bool derive_prime_indexed(
    const evo_research_seed_tuple_t *tuple,
    const uint32_t *primes,
    size_t prime_count,
    evo_research_pcg_schedule_t *schedule)
{
    size_t prime_index = 0;
    uint64_t material = 0;

    if (!prime_index_is_available(tuple, primes, prime_count)) {
        return false;
    }

    prime_index = (size_t)tuple->population_index;
    material = mix_64(
        tuple->master_seed ^
        mix_64((uint64_t)primes[prime_index]) ^
        mix_64(tuple->generation ^ EVO_GENERATION_TAG) ^
        mix_64((uint64_t)tuple->domain ^ EVO_DOMAIN_TAG));

    schedule->state =
        mix_64(material ^ (uint64_t)primes[prime_index + 1] ^
               EVO_STATE_TAG);
    schedule->increment =
        (mix_64(rotate_left_64(material, 23u) ^
                (uint64_t)primes[prime_index + 2] ^ EVO_STREAM_TAG)
         << 1u) |
        UINT64_C(1);
    return true;
}

static bool derive_elliptic(
    const evo_research_seed_tuple_t *tuple,
    const uint32_t *primes,
    size_t prime_count,
    evo_research_pcg_schedule_t *schedule)
{
    evo_research_curve_point_t point = {0};
    size_t prime_index = 0;
    uint64_t material = 0;
    uint64_t encoded_point = 0;
    uint64_t scalar_material = 0;
    uint32_t scalar = 0;

    if (!prime_index_is_available(tuple, primes, prime_count)) {
        return false;
    }

    prime_index = (size_t)tuple->population_index;
    material = tuple_material(tuple);
    scalar_material =
        mix_64(material ^ (uint64_t)primes[prime_index] ^
               EVO_ELLIPTIC_TAG);
    scalar =
        (uint32_t)(scalar_material %
                   (uint64_t)(EVO_RESEARCH_FIELD_PRIME - 1u)) +
        1u;

    if (!evo_research_curve_multiply(scalar, &point)) {
        return false;
    }

    if (point.infinity) {
        encoded_point = EVO_INFINITY_TAG ^ scalar_material;
    } else {
        encoded_point = ((uint64_t)point.x << 32u) | point.y;
    }

    schedule->state =
        mix_64(material ^ encoded_point ^ EVO_STATE_TAG);
    schedule->increment =
        (mix_64(rotate_left_64(material, 23u) ^
                rotate_left_64(encoded_point, 17u) ^ EVO_STREAM_TAG)
         << 1u) |
        UINT64_C(1);
    return true;
}

bool evo_research_derive_schedule(
    evo_research_seed_candidate_t candidate,
    const evo_research_seed_tuple_t *tuple,
    const uint32_t *primes,
    size_t prime_count,
    evo_research_pcg_schedule_t *schedule)
{
    if (schedule == NULL) {
        return false;
    }

    *schedule = (evo_research_pcg_schedule_t){0};
    if (tuple == NULL || !domain_is_valid(tuple->domain)) {
        return false;
    }

    switch (candidate) {
    case EVO_RESEARCH_CANDIDATE_V1_BASELINE:
        return derive_v1_baseline(tuple, schedule);
    case EVO_RESEARCH_CANDIDATE_MIXED_CONTROL:
        derive_mixed_control(tuple, schedule);
        return true;
    case EVO_RESEARCH_CANDIDATE_PRIME_INDEXED:
        return derive_prime_indexed(tuple, primes, prime_count, schedule);
    case EVO_RESEARCH_CANDIDATE_ELLIPTIC:
        return derive_elliptic(tuple, primes, prime_count, schedule);
    default:
        return false;
    }
}

uint32_t evo_research_schedule_next_u32(
    evo_research_pcg_schedule_t *schedule)
{
    uint64_t previous_state = 0;
    uint32_t xor_shifted = 0;
    uint32_t rotation = 0;

    if (schedule == NULL) {
        return 0;
    }

    previous_state = schedule->state;
    xor_shifted =
        (uint32_t)(((previous_state >> 18u) ^ previous_state) >> 27u);
    rotation = (uint32_t)(previous_state >> 59u);
    schedule->state =
        previous_state * EVO_PCG32_MULTIPLIER + schedule->increment;
    return (xor_shifted >> rotation) |
           (xor_shifted << ((32u - rotation) & 31u));
}
