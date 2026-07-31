#ifndef CATALYST_EVO_INTERNAL_RNG_H
#define CATALYST_EVO_INTERNAL_RNG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_RNG_ALGORITHM_VERSION UINT32_C(1)

typedef struct evo_rng {
    uint64_t state;
    uint64_t increment;
    bool seeded;
} evo_rng_t;

/*
 * Seed the version-1 deterministic PCG-XSH-RR 64/32 stream. Every uint64_t
 * seed, including zero, is valid. This generator is not cryptographically
 * secure and must not be used to generate secrets.
 */
bool evo_rng_seed(evo_rng_t *rng, uint64_t seed);

/* Advance the seeded stream and return its next 32-bit value. */
bool evo_rng_next_u32(evo_rng_t *rng, uint32_t *value);

/*
 * Draw an unbiased index in [0, upper_bound) from two-word 64-bit samples.
 * Rejection sampling avoids modulo bias. Invalid input preserves both the
 * stream and the output object.
 */
bool evo_rng_uniform_index(evo_rng_t *rng,
                           size_t upper_bound,
                           size_t *index);

/*
 * Resolve one event with probability in [0, 1]. The probability is quantized
 * to floor(probability * 2^32) successful 32-bit values. Every successful
 * call consumes exactly one word, including endpoint probabilities. Invalid
 * input preserves both the stream and the output object.
 */
bool evo_rng_probability_event(evo_rng_t *rng,
                               double probability,
                               bool *occurred);

/*
 * Fill bytes from successive 32-bit values, least-significant byte first.
 * This defines output independently of native byte order.
 */
bool evo_rng_fill_bytes(evo_rng_t *rng,
                        unsigned char *destination,
                        size_t byte_count);

#endif
