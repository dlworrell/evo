# ADR-0012: Deterministic Odd-Tail Elite Cloning

Status: Accepted
Date: 2026-08-01
Decision owner: EVO

## Context

EVO 0.12.0 deterministically produces every complete child pair, but an odd
population retains one uncommitted trailing slot. Leaving that slot implicit
would prevent the child slab from recording complete production. Filling it
through another tournament, crossover, or mutation would introduce extra RNG
consumption and representation-specific policy at a boundary that only needs
one deterministic completion rule.

The completed parent already records a structurally validated, stable best
valid candidate. Exact fitness ties resolve to the lower population index, so
that candidate is a deterministic elite source without another selection
operation.

## Decision

EVO 0.13.0 adds a private odd-tail production operation. Policy version 1
copies the completed parent's recorded best valid genome byte-for-byte into
the final child slot.

The operation accepts only an odd, nonzero population. For populations larger
than one, all `population_size / 2` complete pairs must already be committed,
the child prefix must record the supplied source generation, and operator
seed-schedule version 1 must be present. A one-member population has no pair
prefix, so its empty production metadata is accepted and completed directly.

Before copying any byte, EVO validates:

1. the problem, configuration, completed parent, child storage, and output
   evidence pointers;
2. the odd-population policy and caller-provided genome and child-slab bounds;
3. structurally consistent completed parent evidence with at least one valid
   candidate and the stable best index;
4. separate parent and child ownership, exact dimensions, an unevaluated child
   lifecycle, and the complete-pair prefix; and
5. bounded, distinct views for the elite parent and final child.

The copy consumes no RNG state and invokes no initializer, validator,
evaluator, crossover, or mutation callback. Success records the full produced
count, source generation, operator seed-schedule version 1, odd-tail policy
version 1, and explicit output evidence. Repeated completion or an attempt to
resume pair production is rejected.

The operation remains private and is not called by `evo_run`.

## Consequences

- Odd child slabs are byte-replayable without an additional random decision.
- The stable best valid parent survives once as the trailing elite; exact ties
  retain the existing lower-index rule.
- A one-member population is supported without inventing a degenerate pair.
- Parent genomes and evaluation evidence remain read-only.
- Rejection before the copy preserves parent state, child state, and output
  evidence.
- `produced_count == population_size` is production evidence only. The child
  still has no validity, fitness, best-candidate, initialization, or evaluation
  evidence.
- Generalized elite counts, configurable singleton policies, child evaluation,
  population swapping, and generation advancement remain later decisions.
- No public layout, installed function, memory budget, or generation-zero
  `evo_run` behavior changes in 0.13.0.

## Alternatives considered

### Run another tournament

Rejected because it would consume a new selection stream and could choose a
non-elite source when the completed parent already has deterministic winner
evidence.

### Mutate the cloned singleton

Rejected because it would consume an extra mutation stream and would no longer
preserve one elite exactly.

### Cross the final child with a scratch sibling

Rejected because crossover owns two child outputs and would require an
additional storage and discard policy.

### Leave the trailing slot zero or discard it

Rejected because zero bytes are representation-dependent and a short child
population would violate the configured population size.

### Add configurable elitism now

Deferred. A public or private elite-count policy should be designed with the
complete generation transition rather than embedded in the minimal odd-tail
completion boundary.

## Verification

- `tests/child_tail_test.c` covers complete odd populations, stable-best tie
  handling, one-member populations, byte-and-evidence replay, complete-pair
  prefix preservation, no additional callbacks, all-invalid parents, even and
  malformed state, alias rejection, repeated completion, pair resumption, and
  rejection-state preservation.
- Existing child-pair tests verify that policy metadata blocks later pair
  dispatch and that an uncompleted odd tail remains outside pair production.
- The child-tail test is normative in CMake, GNU Autotools, and AES-BLD-001.
- GitHub issue: `https://github.com/dlworrell/evo/issues/30`
