# ADR-0010: Versioned Operator Substreams and Parent-Pair Planning

Status: Accepted (selection dispatch extended by ADR-0025)
Date: 2026-07-31
Decision owner: EVO

## Context

EVO 0.10.0 has independently verified tournament selection, crossover and
mutation dispatch, and separate parent/child population ownership. The
operators still accept caller-seeded RNG objects, so composing them without a
stream contract would make replay depend on an undocumented call sequence.

ADR-0003 compared prime-indexed and finite-field elliptic seed schedules with
a plain tuple-mixed control. The candidates had comparable separation results,
while the prime and elliptic variants added provenance, capacity, arithmetic,
and performance costs. ADR-0003 therefore selected the plain control as the
starting point if real operator-consumption semantics later justified
independently addressable streams.

Parent-pair planning supplies those semantics. It needs two tournament results
per complete pair while preserving the evaluated parent population and without
yet writing child storage.

## Decision

EVO 0.11.0 adopts operator seed-schedule version 1. It promotes the exact plain
tuple-mixed control measured by EVO-RNG-001 and derives one PCG stream from:

```text
(master_seed, source_generation, population_index, operation_domain)
```

All transformations use modulo-2^64 unsigned arithmetic. Stable domain values
are `2` for selection, `3` for crossover, and `4` for mutation, preserving the
research tuple encoding. A derived stream has its own mixed state and odd
increment. The schedule is deterministic and independently addressable; it is
not a cryptographic derivation.

The existing generation-zero initialization procedure remains RNG algorithm
version 1 with its fixed increment and seed procedure. Operator seed-schedule
version 1 is a separate version boundary and does not alter any existing
initialization vector.

Tuple index ownership is:

- complete-pair ordinal for selection and future crossover streams; and
- child population index for future mutation streams.

The new private pair planner accepts a completed parent population, source
generation, and complete-pair ordinal. It:

- validates configuration bounds and completed parent evidence before deriving
  a stream;
- recognizes exactly `floor(population_size / 2)` complete pairs;
- maps pair `i` to child slots `2i` and `2i + 1`;
- derives the selection-domain stream at tuple index `i`;
- runs two tournaments sequentially on that local stream, with replacement;
- records the source generation and schedule version in the private plan; and
- commits the output only after both selections succeed.

The parent is read-only. The planner neither receives nor writes child storage.
For an odd population, the final child slot is deliberately not assigned; a
later singleton or elitism decision owns that policy.

The planner is private and is not called by `evo_run`.

ADR-0025 changes the two tournament-specific calls in EVO 0.25.0 to two calls
through selection-policy version 1. Tournament mode retains the exact sequence
above. Rank mode interprets the same selection-domain stream without changing
tuple ownership, crossover domains, or mutation domains, and the plan records
the selected policy.

## Consequences

- A pair can be reproduced directly from recorded configuration, generation,
  pair ordinal, parent evidence, and schedule/RNG versions.
- Selection consumption for one pair cannot shift another pair's stream.
- Future crossover and mutation operations have defined, domain-separated
  tuple ownership without yet being invoked.
- Selection continues to sample valid candidates with replacement; the two
  planned parents may be identical.
- Invalid input, inconsistent parent evidence, all-invalid completion, invalid
  tournament policy, and out-of-range pair ordinals preserve output and parent
  state.
- No public type, function signature, installed symbol, or memory budget
  changes in 0.11.0.
- Child writes, odd-slot policy, operator dispatch, completion/evaluation,
  swapping, generation counting, and public-run integration remain deferred.

## Alternatives considered

### Continue one shared operation stream

Rejected because rejection sampling and future changes in one operator's
consumption would shift every later decision and weaken random-access replay.

### Derive one stream per generation only

Rejected because pair-local replay would still depend on all preceding pairs.

### Promote the prime-indexed schedule

Rejected because ADR-0003 found no measured separation advantage and it would
add a prime artifact and population-index capacity rule.

### Promote the elliptic schedule

Rejected because ADR-0003 found no measured advantage and recorded roughly a
600-fold derivation cost plus substantially greater review surface.

### Assign the odd trailing child now

Rejected because clone, extra-parent, and elitism behaviors are algorithm
policy rather than pair-planning mechanics.

### Produce child genomes in this operation

Rejected to keep stream derivation and parent pairing independently testable
before callbacks create irreversible child output.

## Verification

- `tests/rng_test.c` locks operator schedule state, increment, prefix vectors,
  invalid-input preservation, replay, tuple-index separation, and domain
  separation while retaining all initialization vectors.
- `tests/seed_schedule_research_test.c` proves production schedule version 1
  exactly matches the accepted research mixed control.
- `tests/parent_pair_test.c` covers nulls, policy and pair bounds, incomplete
  and all-invalid parents, fixed pair vectors, selection with replacement,
  valid-only selection, replay, output preservation, odd-population behavior,
  and parent preservation.
- The parent-pair test is normative in CMake, GNU Autotools, and AES-BLD-001.
- GitHub issue: `https://github.com/dlworrell/evo/issues/26`
