# ADR-0025: Stable Rank-Based Parent Selection

Status: Accepted
Date: 2026-08-03
Decision owner: EVO

## Context

EVO has used deterministic tournament selection since 0.7.0. Tournament mode
is allocation-free, replayable, and already integrated with pair-local
selection streams, but tournament size is an indirect control over selection
pressure. The 1.0 core roadmap also requires a stable rank-based policy whose
probabilities depend on ordering rather than the scale of caller fitness.

Adding rank selection must not change existing zero-initialized configurations,
operator-stream identities, hard-validity rules, or comparison semantics. The
distribution must be exact on every supported `size_t` width, reject arithmetic
that cannot be represented, and remain independently reviewable through child
production and promotion.

## Decision

EVO 0.25.0 appends `selection_policy`, `rank_base_weight`, and
`rank_step_weight` to `evo_config_t` and publishes
`EVO_SELECTION_POLICY_VERSION == 1`.

`EVO_SELECTION_TOURNAMENT == 0` is the compatibility default. In tournament
mode both rank weights must be zero. An active selection requires
`tournament_size` in `1..population_size`; structurally valid transitions that
produce no ordinary child may leave it zero. The existing valid-only draws,
with-replacement semantics, tie handling, bounded-sampling algorithm, and RNG
consumption are unchanged.

`EVO_SELECTION_RANK` requires `tournament_size == 0`,
`rank_base_weight > 0`, and any non-negative `rank_step_weight`. For `n`
hard-valid candidates, stable rank zero is best and the weight at rank `r` is:

```text
weight(r) = rank_base_weight
          + (n - 1 - r) * rank_step_weight
```

The exact total is:

```text
total(n) = n * rank_base_weight
         + n * (n - 1) / 2 * rank_step_weight
```

EVO evaluates that expression with checked `size_t` arithmetic, dividing one
factor of the triangular term by two before multiplication. Configuration is
rejected with `EVO_ERROR_RESOURCE_LIMIT` unless the all-valid total for the
configured `population_size` is representable. The check occurs in positive-
limit run preflight before generation-zero allocation or any consumer callback.

### Stable ranking and sampling

Only hard-valid, evaluated candidates participate. Rank is derived exclusively
through fitness-comparison policy version 1: greater caller-computed `total`,
then lower population index because candidates are from the same generation.
Consequently every participant has one unique stable rank, including exact
fitness ties. Invalid candidates have neither rank nor weight.

Before consuming RNG state, rank selection validates the completed population,
resolves every participant rank and weight, and proves the observed weight sum
equals `total(valid_count)`. It then performs one existing unbiased bounded-
index sample over that total. Ticket intervals are visited in ascending
population-index order; the selected candidate owns the interval whose length
is its rank-derived weight. This traversal order is part of replay policy even
though it does not alter the probability assigned to any rank.

The operation allocates no memory, commits the output index only on success,
and preserves the RNG and output on every rejection that precedes sampling.
One-member populations are defined, including any representable base and step
payload because the step term is zero.

### Composition and evidence

Parent-pair planning and the ordinary singleton call one selection dispatch
instead of a tournament-specific entry point. Both policies reuse the existing
selection domain and pair or singleton stream index. Crossover and mutation
domain values, tuple identities, and consumption are unchanged, so selecting a
different policy cannot shift another operator's stream.

Produced populations and private pair, singleton, elite, child-evaluation,
generation-advancement, and bounded-run evidence record policy version 1 and
the selected enum. Child-evaluation policy advances to version 5,
generation-advancement policy to version 5, and bounded-run policy to version
7. A completed produced population is rejected if its selection provenance
does not match the active configuration.

## Consequences

- Zero-initialized callers retain tournament selection and byte-for-byte replay.
- Rank pressure is explicit and independent of absolute fitness spacing.
- `rank_step_weight == 0` gives every valid candidate equal probability while
  preserving stable rank evidence.
- Invalid candidates receive zero probability without retry loops.
- Selection remains allocation-free with constant auxiliary storage.
- One rank draw uses `O(N^2)` comparisons and `O(N)` index scans for population
  size `N`; this favors a small, auditable memory boundary over a rank cache.
- The public configuration layout grows, so consumers must rebuild; no public
  function signature, installed symbol, or resource-budget field changes.
- A zero generation limit continues to ignore unused transition selection
  policy.

## Alternatives considered

### Floating-point probabilities

Rejected because rounding, summation order, and implementation differences can
change interval boundaries and fixed-seed replay. Exact integer weights use the
existing unbiased bounded sampler without floating-point normalization.

### Fitness-proportionate selection

Rejected because negative values, offsets, and fitness scale would require a
separate public normalization policy. Rank selection deliberately depends only
on the accepted comparison order.

### Allocate and sort a rank table for every draw

Rejected for this boundary because it introduces allocation failure and a new
memory budget inside selection. A later performance issue may add a
generation-scoped cache only with explicit lifecycle and replay evidence.

### Replace tournament selection

Rejected because existing callers and golden vectors depend on its precise
with-replacement consumption. Versioned dispatch preserves it as the canonical
zero-valued compatibility mode.

### Use a new RNG domain for rank mode

Rejected because selection policy is an interpretation of the existing
selection stream, not a new operator. Domain separation from crossover and
mutation already provides the required isolation.

## Verification

- `tests/selection_test.c` covers canonical policy validation, invalid enums,
  conflicting fields, representable and overflowing extreme weights, exact
  ties, invalid candidates, one-member populations, golden rank vectors,
  replay, and fixed-seed distribution sanity.
- The same test compares tournament dispatch with the legacy tournament entry
  point for identical indexes and exact final RNG state.
- Parent-pair and bounded-run tests cover rank-policy composition and replay.
- Bounded-run preflight tests prove invalid policy and overflowing arithmetic
  reject before initialization or other callbacks; the wrapped-allocation test
  proves overflowing rank policy returns with zero allocation attempts.
- Lifecycle tests prove selection provenance survives child production,
  elite completion, evaluation, promotion, and bounded-run evidence.
- CMake, GNU Autotools, and AES-BLD-001 continue to enumerate the same
  twenty-two production sources and twenty-six normative tests.
- GitHub issue: `https://github.com/dlworrell/evo/issues/47`
