# ADR-0002: Deterministic RNG and Population Initialization

Status: Accepted
Date: 2026-07-30
Decision owner: EVO

## Context

EVO requires reproducible population initialization before it can add
validation, evaluation, or evolutionary operators. The C library cannot rely
on `rand()`, hidden global state, host byte order, clocks, process identity, or
platform entropy without making a recorded seed insufficient to reproduce a
run.

The public `evo_problem_t::initialize` callback does not accept a random-stream
handle. Replacing that callback would break the current public structure
layout. Generation-zero initialization therefore needs deterministic input
bytes that an existing callback can transform without obtaining its own
entropy.

## Decision

EVO version 0.4.0 adopts private RNG algorithm version 1:

- PCG-XSH-RR with 64-bit state and 32-bit output;
- multiplier `6364136223846793005`;
- fixed odd stream increment `1442695040888963407`;
- seed initialization that clears state, advances once, adds the complete
  `uint64_t` seed modulo 2^64, and advances again; and
- byte emission from each 32-bit output in least-significant-byte-first order.

All integer wraparound occurs in unsigned fixed-width types and is intentional.
The explicit byte order makes the initialized population independent of native
endianness. Every `uint64_t` seed, including zero, is valid. RNG state is local
to an operation; EVO introduces no mutable global random state.

Population initialization performs one byte-fill operation across the complete
contiguous population slab. It then calls the optional consumer initializer
exactly once for each genome in ascending index order. Each callback receives
its already-prefilled genome as a bounded, non-owning view and may transform
only those bytes.

A conforming callback:

- is deterministic for fixed prefilled bytes and context;
- does not obtain time, platform entropy, or another unrecorded random source;
- writes only within its genome;
- does not free or reallocate population storage; and
- does not retain the genome view after returning.

The private population records the seed, algorithm version, and initialized
state only after the byte fill and every callback return. Inactive,
already-initialized, policy-inconsistent, or metadata-inconsistent populations
are rejected without modification.

The RNG is non-cryptographic. It is not approved for secrets, keys, nonces,
authentication, or adversarial unpredictability.

## Consequences

- Identical version, seed, dimensions, callback, and context produce identical
  generation-zero bytes across supported C17 platforms.
- Changing the RNG algorithm, stream constant, seed procedure, output
  transform, or byte order requires a new algorithm version and new fixed
  vectors.
- A consumer callback that uses unrecorded entropy is nonconforming and breaks
  reproducibility outside EVO's control.
- Version 0.4.0 can initialize private population storage but still cannot
  validate, evaluate, rank, select, mutate, cross over, checkpoint, or iterate
  candidates.
- `evo_run` remains disconnected from private population storage until the
  validation and evaluation lifecycle is specified and implemented.

## Alternatives considered

### C library `rand()`

Rejected because algorithm, width, sequence, and global-state behavior vary by
implementation and are unsuitable as a cross-platform reproduction contract.

### Platform entropy or time-derived seeds

Rejected because a configured seed would no longer be sufficient evidence to
reproduce generation zero.

### Replace the public initializer callback

Deferred because changing the callback signature would break the existing
public problem-definition layout. Deterministic prefilled bytes give the
current callback a reproducible input boundary.

### Cryptographic random generation

Rejected for this milestone because deterministic engineering search does not
require secret unpredictability, and cryptographic entropy would conflict with
seed-only replay unless separately designed.

## Verification

- `tests/rng_test.c` locks the version-1 integer and byte vectors.
- `tests/population_initialization_test.c` proves seed replay, zero-seed
  behavior, callback order, callback reproducibility, lifecycle rejection, and
  destruction reset.
- GitHub issue: `https://github.com/dlworrell/evo/issues/8`
