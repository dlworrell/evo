# ADR-0011: Deterministic Complete-Pair Child Production

Status: Accepted (operator provenance extended by ADR-0025 and ADR-0027)
Date: 2026-07-31
Decision owner: EVO

## Context

EVO 0.11.0 can allocate an independent child slab, derive domain-separated
operator streams, and plan complete parent pairs. Crossover and mutation are
also independently verified, but no boundary composes them. Writing children
without recorded progress would permit skipped or repeated pairs and would
leave callback order implicit.

The consumer crossover and mutation callbacks return no status. Once a
callback starts, EVO cannot undo changes to consumer context or recover from a
callback that violates its bounded deterministic contract. The composition
boundary therefore needs a complete preflight and a suffix with no expected
library failure.

## Decision

EVO 0.12.0 adds a private complete-pair production operation. It accepts a
problem, configuration, callback context, completed parent population, source
generation, pair ordinal, active child population, and output evidence.

The private population object records:

- the contiguous number of child genomes produced;
- the source generation shared by those children; and
- operator seed-schedule version 1.

Pairs are produced in ascending order. Pair `i` is accepted only when exactly
`2i` children have already been committed. This makes callback order and the
produced prefix deterministic without allocating a per-slot bitmap. The first
successful pair records source generation and schedule version; every later
pair must match them.

Before child output begins, the operation:

1. validates problem rates, tournament policy, and child budget;
2. proves that the child slab is active, dimensionally consistent, separately
   owned, unevaluated, and at the expected progress point;
3. invokes the accepted parent-pair planner;
4. derives one crossover-domain stream from the pair ordinal;
5. derives one mutation-domain stream from each child index; and
6. resolves every bounded parent and child view.

It then invokes the existing crossover dispatcher once and the mutation
dispatcher once for each child. Those dispatchers have already validated
preconditions and receive seeded streams, so the valid suffix contains no
expected library rejection. After all three dispatches return successfully,
EVO commits the produced count, source generation, schedule version, and pair
evidence.

Consumer callbacks remain responsible for bounded deterministic behavior.
They have no failure channel, and side effects outside the supplied child
views cannot be rolled back. A callback contract violation is not converted
into recoverable EVO state.

The operation leaves `initialized`, `evaluated`, validity, fitness, and best-
candidate evidence empty. For an odd population, the trailing child remains
outside the committed complete-pair prefix.

The operation is private and is not called by `evo_run`.

ADR-0025 adds selection-policy version and enum provenance to pair plans and
produced child state in EVO 0.25.0. It also generalizes preflight from
tournament-only validation to the configured selection dispatch while leaving
crossover, mutation, callback order, and child-slot ownership unchanged.

ADR-0027 adds byte-operator policy version and both operator enums to pair
evidence and produced child state in EVO 0.26.0. Explicit byte modes use the
same pair- and child-indexed streams, bypass consumer callbacks, and retain the
crossover-then-child-A-mutation-then-child-B-mutation order.

ADR-0028 adds the exact effective mutation rate to pair evidence and produced
child state in EVO 0.27.0. Both children use the same committed transition
rate, which remains unchanged through evaluation and promotion.

## Consequences

- Complete-pair child bytes replay from parent evidence, configuration,
  context, source generation, pair order, RNG version, and schedule version.
- Selection and crossover use pair-indexed streams; mutation uses independent
  child-indexed streams.
- A repeated, skipped, mismatched-generation, or out-of-range pair rejects
  before callback dispatch and preserves child bytes, metadata, and output.
- Parent genomes and completed evaluation evidence remain read-only.
- Progress is a contiguous prefix rather than an arbitrary set of completed
  slots. Parallel or out-of-order production requires a later decision.
- No public layout, installed function, memory budget, or generation-zero
  behavior changes in 0.12.0.
- Odd-slot policy, child evaluation, population swapping, and generation
  advancement remain separate milestones.

## Alternatives considered

### Permit arbitrary pair order without metadata

Rejected because repeated or skipped callback dispatch would be
indistinguishable from completed child output.

### Allocate a per-child completion bitmap

Rejected for this boundary because it would add an allocation class and caller
budget when a deterministic contiguous prefix is sufficient.

### Allocate temporary pair scratch for rollback

Rejected because callbacks expose no failure status. All expected library
failures can be resolved before callbacks, while a consumer-context side
effect or out-of-bounds callback violation cannot be made transactional by
copying genome bytes.

### Produce the odd trailing child

Rejected because clone, singleton selection, and elitism are algorithm policy
rather than complete-pair mechanics.

### Evaluate or swap children immediately

Rejected to keep production evidence, odd-slot completion, evaluation
allocation, and generation advancement independently reviewable.

## Verification

- `tests/child_pair_test.c` covers null and policy rejection, all-invalid
  parents, fixed pair and byte vectors, replay, sequential progress, source-
  generation separation, callback and clone paths, odd-tail preservation,
  parent preservation, and rejection-state preservation.
- Existing parent-pair, crossover, mutation, child-population, and RNG tests
  continue to verify the composed boundaries independently.
- The child-pair test is normative in CMake, GNU Autotools, and AES-BLD-001.
- GitHub issue: `https://github.com/dlworrell/evo/issues/28`
