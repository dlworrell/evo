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
 * Fill bytes from successive 32-bit values, least-significant byte first.
 * This defines output independently of native byte order.
 */
bool evo_rng_fill_bytes(evo_rng_t *rng,
                        unsigned char *destination,
                        size_t byte_count);

#endif
