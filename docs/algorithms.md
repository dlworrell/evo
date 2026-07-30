# Algorithm Roadmap

## Initial Release

- Tournament selection
- Rank-based selection
- Elitism
- One-point crossover
- Two-point crossover
- Uniform crossover
- Bit, integer, floating-point, and permutation mutation helpers
- Adaptive mutation
- Diversity measurement
- Stagnation detection
- Constraint penalties

## Later Releases

- Evolution strategies
- Differential evolution
- CMA-ES
- Simulated annealing
- Particle swarm optimization
- NSGA-II and multi-objective optimization
- Niching, crowding, and fitness sharing
- Parallel and distributed evaluation

Algorithms must expose deterministic behavior under a recorded random seed and preserve sufficient evidence to reproduce a run.

## Deterministic Random Stream

EVO 0.4.0 defines private RNG algorithm version 1 as PCG-XSH-RR with 64-bit
state and 32-bit output. Its multiplier, fixed stream increment, seed
procedure, output transform, and least-significant-byte-first emission order
are normative in
`docs/adr/ADR-0002-deterministic-rng-and-population-initialization.md`.

The stream uses only unsigned fixed-width arithmetic. It has no mutable global
state and consumes no clock, process, operating-system, or hardware entropy.
Every `uint64_t` seed is valid. Fixed vectors in `tests/rng_test.c` prevent an
implementation change from silently altering reproducibility.

This stream is designed for repeatable engineering search. It is not
cryptographically secure and must not generate secrets, keys, nonces, or
authentication material.

## Seed-Schedule Research

EVO-RNG-001 compared the version-1 baseline with a plain tuple-mixed control,
a Code Noodling-derived prime-indexed schedule, and a portable finite-field
elliptic schedule. The prime and elliptic candidates did not improve the
measured separation over the simpler control, and the elliptic candidate added
approximately 600-fold derivation cost.

EVO therefore preserves RNG algorithm version 1 and does not link the research
schedules into the production library. If future operators require
independently addressable streams, their consumption model will be specified
first and a new versioned design will begin from the plain tuple-mixed control.
See `docs/adr/ADR-0003-prime-and-elliptic-seed-schedules.md`.

## Generation-Zero Initialization

The version 0.4.0 private population initializer consumes one continuous
version-1 stream to fill the entire genome slab. It then invokes the optional
consumer initializer once per genome in ascending index order.

Callbacks receive deterministic prefilled bytes and may perform bounded,
deterministic domain transformations. A callback that consults unrecorded
entropy, writes outside its genome, changes ownership, or retains the genome
view violates the EVO contract.

Initialization does not validate or evaluate candidates. Those phases remain
separate algorithm boundaries.
