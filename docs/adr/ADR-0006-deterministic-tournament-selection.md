# ADR-0006: Deterministic Tournament Selection

Status: Accepted
Date: 2026-07-31
Decision owner: EVO

## Context

EVO 0.6.0 publicly executes generation-zero construction, initialization,
validation, evaluation, and stable winner transfer. The next execution step is
parent selection, but composing selection directly into `evo_run` would also
require generation-buffer ownership, crossover and mutation sequencing,
termination semantics, generation counting, and a defined operator-stream
schedule.

Tournament selection also needs a bounded random-index operation. Direct
modulo reduction favors some indexes unless the generator range is an exact
multiple of the bound. Retrying invalid candidates after drawing from the full
population would make RNG consumption depend on validity density and could
admit an unbounded loop for all-invalid input.

The accepted seed-schedule research in ADR-0003 found no basis for adopting the
prime-indexed or finite-field elliptic schedules. Selection therefore needs a
clear stream input without prematurely defining a new schedule.

## Decision

EVO 0.7.0 adds a private deterministic tournament-selection operator and an
unbiased bounded-index helper over RNG algorithm version 1.

The bounded helper consumes successive 32-bit PCG values in fixed low-word then
high-word order to form a 64-bit sample. For a nonzero bound it computes the
unsigned rejection threshold `(-bound) % bound`, discards samples below that
threshold, and reduces the accepted sample modulo the bound. Invalid input
preserves the stream and output.

The tournament operator:

- requires `tournament_size` in `1..population_size`;
- validates the complete evaluated population before consuming RNG state;
- proves storage and evaluation byte counts, initialization evidence, validity
  flags, finite fitness, valid count, and stable-best metadata;
- returns `EVO_ERROR_NO_VALID_CANDIDATE` for consistent all-invalid input;
- samples with replacement from valid candidates only;
- maps sampled valid ordinals to ascending population indexes without
  allocation;
- selects the highest `fitness.total`, with lower population index resolving an
  exact tie; and
- commits the output index only after all tournament draws succeed.

The caller supplies an explicitly seeded private RNG stream. Selection neither
derives that seed nor records stream ownership. RNG algorithm version 1 and the
generation-zero initialization stream remain unchanged.

The operator is private and is not invoked by `evo_run`. Version 0.7.0 therefore
does not perform a generation transition and does not change public success or
`generations_completed`.

ADR-0021 preserves these draw and tie semantics in EVO 0.21.0 but routes
rankability and ordering through fitness-comparison policy version 1. A
completed population must record that policy before selection consumes RNG.

## Consequences

- Tournament behavior is deterministic for fixed population evidence,
  configuration, and RNG state.
- Bounded sampling has no modulo bias and has an exact cross-platform word
  order.
- Invalid candidates consume no candidate slots and can never win.
- All structural rejection occurs before the RNG advances.
- Selection adds no allocation class or memory budget.
- Public API layout, installed symbols, and generation-zero behavior remain
  unchanged.
- Generation orchestration must later define whether streams continue, split,
  or derive per generation before selection can be composed with other
  operators.

## Alternatives considered

### Reduce a 32-bit output directly modulo the population size

Rejected because it introduces modulo bias for most population sizes and caps
the representable bound at 32 bits.

### Draw from every population index and retry invalid candidates

Rejected because validity density changes stream consumption, all-invalid
input needs a special termination mechanism, and invalid candidates remain in
the sampling domain.

### Sample without replacement

Rejected because it requires additional scratch state or population mutation
and is not the conventional tournament-with-replacement behavior selected for
the initial operator.

### Derive a prime or elliptic selection seed

Rejected because ADR-0003 found no measured benefit sufficient to justify
those schedules. Stream derivation remains a generation-orchestration concern.

### Integrate selection into `evo_run`

Rejected because selection alone cannot produce a next generation. Combining
it now with unimplemented buffer, crossover, mutation, and termination
semantics would weaken the independently testable boundary.

## Verification

- `tests/rng_test.c` locks bounded-sampling vectors, normal consumption,
  rejection consumption, invalid-input preservation, and replay.
- `tests/selection_test.c` covers null arguments, tournament bounds, unseeded
  RNG, inconsistent evaluation state, all-invalid completion, valid-only
  sampling, fixed replay, single-draw behavior, and lower-index exact ties.
- The same selection test is normative in CMake, GNU Autotools, and the
  AES-BLD-001 repository profile.
- GitHub issue: `https://github.com/dlworrell/evo/issues/18`
