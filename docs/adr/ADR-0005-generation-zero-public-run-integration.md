# ADR-0005: Generation-Zero Public Run Integration

Status: Accepted
Date: 2026-07-31
Decision owner: EVO

## Context

EVO 0.5.0 independently verifies private population construction,
deterministic initialization, validity classification, finite fitness
evaluation, and stable generation-zero winner selection. The public
`evo_run`, however, still allocates one zero-filled genome without invoking
those phases. Public success therefore cannot yet prove that any candidate was
initialized, validated, or evaluated.

The private evaluation phase intentionally treats an all-invalid population as
a successfully completed classification with no winner. Public execution must
map that state explicitly because it cannot transfer a valid result asset.

Connecting the phases also creates a third allocation point: an independently
owned public copy of the winning genome. Returning a view into the private
population would violate the existing rule that population destruction
invalidates every view.

## Decision

EVO 0.6.0 composes the existing private generation-zero phases inside
`evo_run`.

An active public result is rejected first and preserved unchanged. For an
inactive result, a null problem, configuration, or evaluator returns
`EVO_ERROR_INVALID_ARGUMENT` before population construction. The evaluator is
required because public success now means evaluated evidence exists.

EVO constructs the private genome slab under `max_genome_bytes` and
`max_population_bytes`, initializes it from `random_seed`, and evaluates it
under `max_evaluation_bytes`. The established callback order, invalid-candidate
suppression, finite-fitness requirement, higher-total comparison, and
lower-index tie rule remain unchanged.

After successful private evaluation:

- zero valid candidates map to the new public status
  `EVO_ERROR_NO_VALID_CANDIDATE`;
- an inconsistent completed population maps to `EVO_ERROR_STATE`; and
- a valid winner is copied into a separate caller-owned allocation of exactly
  `problem->genome_size` bytes.

The result receives all seven fitness fields, the configured random seed, and
`generations_completed == 0`. Zero is deliberate: version 0.6.0 performs no
parent selection or generation transition.

The winner copy uses an explicit byte-bounded loop. Both source and destination
are proven to cover `problem->genome_size`, and the destination allocation is
already constrained by caller policy.

Every private allocation is released before return. Every failure other than
active-result rejection leaves the public result in its empty zero state.
Allocation failure is verified independently at the population slab,
evaluation record, and public winner-copy stages.

## Consequences

- `EVO_SUCCESS` now proves that a valid generation-zero candidate was
  initialized, evaluated, selected, and transferred.
- Public success no longer represents a placeholder allocation.
- The result remains valid after private population destruction because it
  owns an independent copy.
- All-invalid completion is distinguishable from invalid arguments, resource
  limits, allocation failure, lifecycle inconsistency, and non-finite
  evaluator output.
- Callers must provide all three memory budgets and a non-null evaluator.
- Consumers must rebuild against the 0.6.0 header and handle the added status.
- Selection, crossover, mutation, diversity handling, checkpointing, and the
  generation loop remain outside this decision.

## Alternatives considered

### Transfer ownership of one genome inside the population slab

Rejected because genomes share one contiguous allocation. Extracting one
element would either retain unrelated candidates or require a new fragmented
ownership model, and every existing population view assumes destruction
invalidates the full slab.

### Return success with an empty result for an all-invalid population

Rejected because public success promises a transferred valid candidate. An
empty success would force callers to infer a second outcome from a pointer and
would make omission handling inconsistent.

### Treat all-invalid completion as an evaluation error

Rejected because evaluation executed correctly and produced no non-finite
evidence. The absence of an eligible candidate is a domain outcome, not a
callback-output defect.

### Add selection and a generation loop in the same change

Rejected because it would combine orchestration integration with new operator,
termination, and generation-count semantics. Generation-zero is independently
specified and testable.

## Verification

- `tests/evo_lifecycle_test.c` proves callback order, invalid suppression,
  complete winner and fitness transfer, exact-tie behavior, resource policy,
  missing evaluators, all-invalid mapping, non-finite rejection,
  active-result preservation, destruction, and reuse.
- `tests/allocation_failure_test.c` proves deterministic cleanup at all three
  `evo_run` allocation stages.
- The private population, initialization, evaluation, and RNG tests remain
  unchanged and continue to verify each composed phase independently.
- CMake/Clang, CMake/GCC, Autotools/Clang, and Autotools/GCC builds must expose
  the same installed API and test behavior.
- GitHub issue: `https://github.com/dlworrell/evo/issues/16`
