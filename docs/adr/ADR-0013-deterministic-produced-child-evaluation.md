# ADR-0013: Deterministic Produced-Child Evaluation

Status: Accepted
Date: 2026-08-01
Decision owner: EVO

## Context

EVO 0.13.0 can fill every slot in an independently owned child slab. Complete
pairs are generated through versioned selection, crossover, and mutation
streams, and an odd trailing slot uses the version-1 stable-best elite clone.
Production evidence alone intentionally does not assert validity, fitness, or
a selectable best candidate.

The generation-zero evaluator already provides the required deterministic
semantics: validate all candidates in ascending order, evaluate only valid
candidates in ascending order, require finite fitness, retain the lower index
on exact ties, and keep records provisional until success. Duplicating those
semantics for children would create two authorities that could drift.

## Decision

EVO 0.14.0 adds a private produced-child evaluation operation and reuses one
private evaluation engine after lifecycle-specific preflight.

Child-evaluation policy version 1 accepts only a fully produced slab with:

1. exact problem, configuration, storage, and caller-budget dimensions;
2. `produced_count == population_size`;
3. source generation matching the caller's supplied value;
4. operator seed-schedule version 1;
5. odd-tail policy version 1 for odd populations and zero for even
   populations; and
6. empty initialization, RNG-initialization, validity, fitness, and best-
   candidate state.

The operation allocates provisional candidate records under
`max_evaluation_bytes`. It validates every genome in ascending order and then
evaluates only valid genomes in ascending order. All seven returned fitness
fields must be finite. Higher `fitness.total` wins; exact ties preserve the
lower candidate index. Invalid records remain zero and unevaluated.

Only the complete record set is committed to the child. Success preserves all
genome bytes and production provenance while recording valid count, stable
best evidence, and evaluated state. An all-invalid population completes
without a best candidate. Caller-owned output evidence records the input
provenance, result summary, and policy version.

The shared completed-population validator now recognizes two distinct
provenance forms:

- generation-zero: deterministic initialization evidence and no child-
  production metadata; or
- produced child: complete production metadata and no generation-zero
  initialization evidence.

Both forms require structurally consistent evaluation records. An evaluated
child can therefore become the read-only completed authority for selection and
for allocating another independent child slab.

The operation consumes no RNG state and does not invoke initialization,
selection, crossover, or mutation callbacks. It remains private and is not
called by `evo_run`.

ADR-0021 advances child-evaluation policy to version 2 in EVO 0.21.0. The
operation now records fitness-comparison policy version 1 and rejects negative
soft-penalty evidence in addition to non-finite evidence. Callback ordering,
allocation, and atomic commit semantics remain unchanged.

ADR-0022 advances child-evaluation policy to version 3 in EVO 0.22.0. The
operation now commits bounded diversity evidence with evaluation and records
its policy and metric versions. Distance measurement and rollback are governed
by ADR-0022.

## Consequences

- Generation-zero and child evaluation cannot drift in ordering, finite-
  fitness, stable-tie, allocation, or commit semantics.
- Even, odd, and one-member child populations share one evaluation policy.
- Child genomes and production evidence remain immutable during evaluation.
- Preflight, budget, allocation, and non-finite-fitness failures leave the
  child object and output evidence unchanged.
- Callback-context side effects after validation or evaluation dispatch begins
  cannot be rolled back; this remains part of the consumer callback contract.
- All-invalid children are completed evaluated populations but cannot supply a
  tournament winner.
- A subsequent child slab can be allocated from an evaluated child without a
  population swap.
- Parent/child swapping, generation-number advancement and overflow,
  termination, recycling, and public multi-generation execution remain later
  decisions.
- No public layout, installed function, memory policy, or generation-zero
  `evo_run` behavior changes in 0.14.0.

## Alternatives considered

### Treat production as completed-population evidence

Rejected because produced bytes have not passed consumer validity or fitness
policy and cannot safely authorize selection.

### Duplicate the evaluator for child populations

Rejected because two implementations could diverge on callback ordering,
finite-field checks, tie handling, or provisional-state cleanup.

### Evaluate each child immediately after production

Rejected because it would interleave production and evaluation callbacks,
weaken full-slab preflight, and prevent one deterministic validation pass
before evaluation.

### Reject an all-invalid child immediately

Rejected because completed evaluation evidence and the policy decision to stop
are separate concerns, matching generation-zero behavior.

### Swap and increment the generation in the same operation

Deferred because ownership transfer, generation overflow, prior-parent
recycling, and failure recovery require a separate atomic transition contract.

## Verification

- `tests/child_evaluation_test.c` covers callback order, invalid suppression,
  stable ties, all-invalid completion, one-member populations, even/odd
  provenance, replay, byte preservation, non-finite fitness, budget and state
  rejection, repeated evaluation, completed validation, and next-child
  allocation.
- `tests/allocation_failure_test.c` covers deterministic failure of the child-
  evaluation record allocation after complete child production.
- Existing generation-zero evaluation, selection, child-storage, pair, and
  odd-tail tests protect the shared invariants and earlier lifecycle forms.
- The child-evaluation test is normative in CMake, GNU Autotools, and
  AES-BLD-001.
- GitHub issue: `https://github.com/dlworrell/evo/issues/32`
