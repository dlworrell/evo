#include "internal/rng.h"

#include <math.h>

#define EVO_PCG32_MULTIPLIER UINT64_C(6364136223846793005)
#define EVO_PCG32_INCREMENT UINT64_C(1442695040888963407)
#define EVO_PROBABILITY_SCALE 4294967296.0

#define EVO_GENERATION_TAG UINT64_C(0x9e3779b97f4a7c15)
#define EVO_POPULATION_TAG UINT64_C(0xd1b54a32d192ed03)
#define EVO_DOMAIN_TAG UINT64_C(0x94d049bb133111eb)
#define EVO_STATE_TAG UINT64_C(0xa0761d6478bd642f)
#define EVO_STREAM_TAG UINT64_C(0xe7037ed1a0b428db)

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

static bool operator_domain_is_valid(evo_operator_rng_domain_t domain)
{
    return domain >= EVO_OPERATOR_RNG_DOMAIN_SELECTION &&
           domain <= EVO_OPERATOR_RNG_DOMAIN_MUTATION;
}

static uint32_t pcg32_advance(evo_rng_t *rng)
{
    const uint64_t previous_state = rng->state;
    const uint32_t xor_shifted =
        (uint32_t)(((previous_state >> 18u) ^ previous_state) >> 27u);
    const uint32_t rotation = (uint32_t)(previous_state >> 59u);

    rng->state =
        previous_state * EVO_PCG32_MULTIPLIER + rng->increment;
    return (xor_shifted >> rotation) |
           (xor_shifted << ((32u - rotation) & 31u));
}

bool evo_rng_seed(evo_rng_t *rng, uint64_t seed)
{
    if (rng == NULL) {
        return false;
    }

    *rng = (evo_rng_t){0};
    rng->increment = EVO_PCG32_INCREMENT;
    (void)pcg32_advance(rng);
    rng->state += seed;
    (void)pcg32_advance(rng);
    rng->seeded = true;
    return true;
}

bool evo_rng_derive_operator_stream(
    evo_rng_t *rng,
    uint64_t master_seed,
    uint64_t source_generation,
    uint64_t population_index,
    evo_operator_rng_domain_t domain)
{
    evo_rng_t derived = {0};
    uint64_t material = 0;

    if (rng == NULL || !operator_domain_is_valid(domain)) {
        return false;
    }

    material = mix_64(
        master_seed ^
        mix_64(source_generation ^ EVO_GENERATION_TAG) ^
        mix_64(population_index ^ EVO_POPULATION_TAG) ^
        mix_64((uint64_t)domain ^ EVO_DOMAIN_TAG));

    derived.state = mix_64(material ^ EVO_STATE_TAG);
    derived.increment =
        (mix_64(rotate_left_64(material, 23u) ^ EVO_STREAM_TAG)
         << 1u) |
        UINT64_C(1);
    derived.seeded = true;

    *rng = derived;
    return true;
}

bool evo_rng_next_u32(evo_rng_t *rng, uint32_t *value)
{
    if (rng == NULL || value == NULL || !rng->seeded) {
        return false;
    }

    *value = pcg32_advance(rng);
    return true;
}

bool evo_rng_uniform_index(evo_rng_t *rng,
                           size_t upper_bound,
                           size_t *index)
{
    uint64_t bound = 0;
    uint64_t threshold = 0;
    uint64_t sample = 0;

    if (rng == NULL || index == NULL || !rng->seeded ||
        upper_bound == 0 ||
        upper_bound > (size_t)UINT64_MAX) {
        return false;
    }

    bound = (uint64_t)upper_bound;
    threshold = (UINT64_C(0) - bound) % bound;

    do {
        uint32_t low = 0;
        uint32_t high = 0;

        if (!evo_rng_next_u32(rng, &low) ||
            !evo_rng_next_u32(rng, &high)) {
            return false;
        }

        sample = (uint64_t)low | ((uint64_t)high << 32u);
    } while (sample < threshold);

    *index = (size_t)(sample % bound);
    return true;
}

bool evo_rng_probability_event(evo_rng_t *rng,
                               double probability,
                               bool *occurred)
{
    uint32_t sample = 0;
    uint64_t threshold = 0;

    if (rng == NULL || occurred == NULL || !rng->seeded ||
        !isfinite(probability) || probability < 0.0 ||
        probability > 1.0) {
        return false;
    }

    threshold =
        (uint64_t)(probability * EVO_PROBABILITY_SCALE);
    if (!evo_rng_next_u32(rng, &sample)) {
        return false;
    }

    *occurred = (uint64_t)sample < threshold;
    return true;
}

bool evo_rng_fill_bytes(evo_rng_t *rng,
                        unsigned char *destination,
                        size_t byte_count)
{
    size_t offset = 0;

    if (rng == NULL || !rng->seeded ||
        (destination == NULL && byte_count != 0)) {
        return false;
    }

    while (offset < byte_count) {
        uint32_t value = 0;

        if (!evo_rng_next_u32(rng, &value)) {
            return false;
        }

        for (size_t byte_index = 0;
             byte_index < sizeof(value) && offset < byte_count;
             ++byte_index) {
            destination[offset] = (unsigned char)(value & UINT32_C(0xff));
            value >>= 8u;
            ++offset;
        }
    }

    return true;
}
