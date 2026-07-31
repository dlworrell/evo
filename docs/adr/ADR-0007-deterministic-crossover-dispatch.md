# ADR-0007: Deterministic Crossover Dispatch

Status: Accepted
Date: 2026-07-31
Decision owner: EVO

## Context

EVO 0.7.0 provides deterministic private tournament selection, but a selected
parent pair still needs a representation-aware path to produce children. The
public problem definition already contains a consumer crossover callback and a
floating-point crossover rate. Hard-coding byte-level one-point, two-point, or
uniform crossover into the first boundary would impose genetic meaning on
opaque genome bytes and would be unsuitable for structured, integer,
floating-point, or permutation representations.

Crossover gating must also be reproducible. Comparing an unbounded floating
random value with the configured rate would leave the conversion and endpoint
consumption rules unclear. Skipping RNG consumption at rates zero or one would
make later operator-stream position depend on a special case rather than on
the number of attempted pairs.

## Decision

EVO 0.8.0 adds a private probability-event helper over RNG algorithm version 1
and a private crossover-pair dispatch operator.

For a finite probability in the inclusive range `[0, 1]`, the helper computes
`floor(probability * 2^32)` as a 64-bit threshold. It consumes exactly one
32-bit PCG output and reports an event when the unsigned sample is less than
that threshold. This makes probability zero always false and probability one
always true while still consuming one word at both endpoints. Invalid input
preserves the RNG and output.

The crossover operator:

- accepts two bounded, read-only parent genome views and two distinct writable
  child views;
- requires nonzero `genome_size` within `max_genome_bytes`;
- requires a finite `crossover_rate` in `[0, 1]` and a seeded RNG;
- rejects null pointers, identical child pointers, and exact child-to-parent
  aliases before consuming RNG or changing child output;
- consumes exactly one RNG word for every successful pair;
- invokes the consumer crossover callback exactly once when the event is
  selected and the callback exists; and
- otherwise clones parent A to child A and parent B to child B for exactly
  `genome_size` bytes.

The private caller must supply complete, non-overlapping child spans that do
not overlap either parent. Portable C cannot order unrelated pointers to prove
arbitrary partial overlap, so exact aliases are rejected by the operator and
the complete span rule remains an internal precondition. The consumer callback
has no failure channel. It must fully initialize both children, preserve
parent bytes and ownership, retain no view, and remain deterministic for fixed
parents and context.

The operator is not called by `evo_run`. It neither selects parents nor owns a
next-generation population. Version 0.8.0 therefore preserves generation-zero
public behavior and leaves `generations_completed` at zero.

## Consequences

- Crossover-rate decisions are fixed-vector testable and replay stable.
- Every successful pair advances the supplied stream by one word, independent
  of the rate endpoint or callback presence.
- Representation semantics remain consumer-owned rather than byte-hard-coded.
- Missing callbacks have a deterministic identity fallback.
- Precondition failures preserve parents, children, and RNG state.
- Callback contract violations cannot be rolled back by EVO because the
  callback writes caller-supplied child views and returns no status.
- Public layouts, installed symbols, RNG algorithm version 1, and `evo_run`
  behavior remain unchanged.
- Mutation, child-population ownership, operator sequencing, and the first
  generation transition remain separate milestones.

## Alternatives considered

### Add generic one-point crossover over raw bytes

Rejected for the first boundary because a byte position need not be a valid
gene boundary for structured, numeric, or permutation genomes. Built-in
representation-specific helpers can be added later under explicit contracts.

### Let each consumer decide whether crossover occurs

Rejected because the configured rate and stream-consumption evidence would no
longer be owned or reproducible by EVO.

### Skip RNG consumption at rates zero and one

Rejected because pair attempts would consume a configuration-dependent number
of stream words. A fixed one-word decision is easier to reproduce and compose.

### Allocate private scratch children for rollback

Rejected because the callback has no failure result and a scratch allocation
would introduce another policy budget before next-generation ownership is
specified.

### Integrate selection and crossover into `evo_run`

Rejected because mutation, child-population ownership, evaluation of the new
population, termination, and generation counting are not yet specified.

## Verification

- `tests/rng_test.c` locks probability endpoint behavior, threshold vectors,
  invalid-input preservation, exact consumption, and replay.
- `tests/crossover_test.c` covers null and policy rejection, exact aliases,
  unseeded state, callback and clone paths, endpoint consumption, fixed gate
  decisions, parent preservation, callback evidence, and replay.
- The crossover test is normative in CMake, GNU Autotools, and the AES-BLD-001
  repository profile.
- GitHub issue: `https://github.com/dlworrell/evo/issues/20`
