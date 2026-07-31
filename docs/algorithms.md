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

Version 0.7.0 adds unbiased bounded-index sampling. One sample combines two
successive 32-bit outputs into an explicit low-word/high-word 64-bit value,
rejects the modulo-bias prefix, and reduces an accepted value by the bound.
Fixed vectors cover normal and rejection paths, exact stream consumption, and
replay.

Version 0.8.0 adds deterministic probability events. A finite probability in
`[0, 1]` is quantized to `floor(probability * 2^32)` successful 32-bit values.
Every successful decision consumes exactly one word, including probabilities
zero and one. Invalid input preserves the stream and output.

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

## Generation-Zero Validation and Evaluation

Version 0.5.0 implements validation and evaluation as two deterministic passes
over the initialized population. The optional `is_valid` callback runs once
per genome in ascending index order. A missing validator accepts every
candidate. The `evaluate` callback then runs once for each valid candidate in
ascending order; invalid candidates are never evaluated.

All seven returned fitness components must be finite. `fitness.total` is the
consumer-computed scalar objective, and EVO selects the greatest total. Exact
ties preserve the lower population index. The remaining components are
recorded for evidence but receive no library-defined ordering.

An all-invalid population completes evaluation without a winner. Evaluation
records are bounded by `max_evaluation_bytes`, and failures discard provisional
records while preserving the initialized genome slab within the private phase.

## Public Generation-Zero Execution

Version 0.6.0 composes population construction, deterministic initialization,
validation, evaluation, and stable winner transfer inside `evo_run`.

The public result receives an independent copy of the lower-index
highest-total valid candidate and all seven fields returned by its evaluator.
The private population slab and evaluation records are always released before
the call returns. If every candidate is invalid, the private phase still
completes successfully, but the public run returns
`EVO_ERROR_NO_VALID_CANDIDATE` with an empty result because there is no asset
whose ownership can be transferred.

This boundary performs no parent selection, crossover, mutation, elitism,
diversity processing, or generation transition. A successful call therefore
records `generations_completed == 0`; it is generation-zero evidence rather
than a completed optimization search.

## Deterministic Tournament Selection

Version 0.7.0 implements tournament selection as a private operator over a
completed evaluation population.

- `tournament_size` is in `1..population_size`.
- Draws are with replacement.
- Only valid, evaluated candidates participate.
- A valid-candidate ordinal is sampled without modulo bias and mapped to the
  corresponding ascending population index.
- Higher `fitness.total` wins; the lower population index wins an exact tie.
- Completed all-invalid input returns `EVO_ERROR_NO_VALID_CANDIDATE`.
- Invalid lifecycle or evidence returns `EVO_ERROR_STATE`.
- The population and output are unchanged on failure, and validation consumes
  no RNG state.

The caller supplies the seeded private stream. This isolates selection
semantics from the still-undecided generation-level stream schedule. The
operator is not called by `evo_run` and performs no crossover, mutation,
elitism, or generation advancement.

## Deterministic Crossover Dispatch

Version 0.8.0 implements a private representation-neutral crossover pair
operator. Two read-only parent genome views and two distinct, non-overlapping
child views are supplied by the private caller.

- `genome_size` must be nonzero and within `max_genome_bytes`.
- `crossover_rate` must be finite and in `[0, 1]`.
- One probability decision consumes exactly one RNG word per successful pair.
- A selected event invokes the consumer callback exactly once when present.
- A non-selected event or missing callback clones each parent to its
  corresponding child for exactly `genome_size` bytes.
- Precondition failures preserve the RNG and child outputs.
- The callback must fully initialize both children, preserve parents and
  ownership, retain no view, and remain deterministic for fixed inputs and
  context.

The operator does not choose parents, allocate a child population, mutate
children, or execute a generation transition. Representation-specific
one-point, two-point, and uniform helpers remain later algorithm-library work.

## Deterministic Mutation Dispatch

Version 0.9.0 implements a private representation-neutral mutation operator
over one bounded writable genome.

- `genome_size` must be nonzero and within `max_genome_bytes`.
- `mutation_rate` must be finite and in `[0, 1]`.
- One probability decision consumes exactly one RNG word per valid attempt,
  including rates zero and one and an absent callback.
- A selected event invokes the consumer mutation callback exactly once when
  present.
- A non-selected event or absent callback leaves the genome unchanged.
- Precondition failures preserve the RNG and genome bytes.
- The callback receives `mutation_rate` unchanged as its representation-
  specific intensity and must remain deterministic for fixed genome bytes,
  rate, and context.
- The callback owns no storage, retains no genome view, uses no unrecorded
  entropy, and has no failure or rollback channel.

The operator does not allocate a child population, define built-in bit,
integer, floating-point, or permutation mutations, adapt the rate, or execute
a generation transition. Those remain later algorithm and orchestration work.
