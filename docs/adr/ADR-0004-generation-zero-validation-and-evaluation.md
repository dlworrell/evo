# ADR-0004: Generation-Zero Validation and Evaluation

Status: Accepted
Date: 2026-07-30
Decision owner: EVO

## Context

EVO 0.4.0 can construct and deterministically initialize private population
storage, but it cannot distinguish valid candidates, obtain fitness evidence,
or identify a generation-zero winner. Connecting that incomplete subsystem to
`evo_run` would still let the public scaffold imply that useful optimization
work occurred without defined correctness, comparison, or failure semantics.

The public problem definition already provides an optional `is_valid` callback
and a required-for-evaluation `evaluate` callback. Its seven-field
`evo_fitness_t` structure does not independently state whether each component
is maximized or minimized. The consumer-computed `total` field is therefore the
only existing scalar objective that can be compared without inventing
domain-specific policy.

Evaluation metadata introduces another allocation class. The existing
`max_population_bytes` policy applies only to the genome slab and cannot
silently authorize fitness and validity storage.

## Decision

EVO 0.5.0 adds a private generation-zero validation and evaluation phase.

The public configuration appends `max_evaluation_bytes`. Before allocating one
private candidate record per population member, EVO proves that
`population_size * sizeof(evo_candidate_evaluation_t)` is representable as
`size_t` and no greater than that caller-controlled budget. No compiled-in
population or metadata limit is introduced.

The phase requires an active, initialized, unevaluated population consistent
with the supplied problem and configuration. A null validator means every
candidate is valid. Otherwise EVO invokes `is_valid` exactly once for each
genome in ascending index order. It then invokes `evaluate` exactly once for
each valid genome, again in ascending index order. Invalid genomes are never
evaluated.

Validity is a hard gate. Among valid candidates, the greater
`evo_fitness_t::total` wins. An exact total tie retains the lower population
index. The other six fitness components remain recorded evidence supplied by
the consumer; EVO does not infer their direction or recompute `total`.

Every field in a returned fitness structure must be finite. NaN or infinity
returns `EVO_ERROR_EVALUATION`, releases provisional records, and preserves
the initialized genome slab as unevaluated. Allocation and resource failures
have the same internal-state preservation boundary. Consumer context side
effects are outside EVO ownership and cannot be rolled back.

ADR-0021 extends this boundary in EVO 0.21.0: `constraint_penalty` is a non-
negative magnitude, negative penalty evidence also returns
`EVO_ERROR_EVALUATION`, and comparison is centralized under public policy
version 1 without changing caller ownership of `total`.

Completing the phase with no valid candidates returns `EVO_SUCCESS`, records a
completed evaluation with `valid_count == 0`, and records no winner. This
distinguishes successful classification of an all-invalid population from an
internal failure. A later `evo_run` integration must decide how that completed
state maps to its public run result.

Evaluation records and winner metadata remain private. Population destruction
releases both allocation classes and resets the complete population object.
The phase remains disconnected from `evo_run`.

## Consequences

- Validation, evaluation, and tie-breaking replay in a fixed order.
- Invalid candidates cannot win through placeholder or extreme fitness values.
- Non-finite consumer output is rejected instead of entering comparisons.
- Callers explicitly budget evaluation metadata separately from genome bytes.
- A completed all-invalid phase is inspectable without being confused with
  allocation or callback-output failure.
- Consumers must rebuild for the appended configuration field.
- EVO can next integrate population creation, initialization, evaluation, and
  winner transfer without simultaneously inventing comparison semantics.
- Selection, crossover, mutation, elitism, diversity, checkpointing, and the
  generation loop remain unimplemented.

## Alternatives considered

### Penalize invalid candidates and still evaluate them

Rejected because candidate correctness is a hard gate. Calling the evaluator
for invalid memory or domain representations may itself be unsafe.

### Rank every fitness component inside EVO

Rejected because the public API does not define a universal direction,
priority, or scale for the component fields. The consumer already owns the
domain policy and computes `total`.

### Treat NaN as the worst score

Rejected because NaN breaks ordinary ordering and usually indicates a
consumer calculation defect. Silent demotion would hide invalid evidence.

### Return an error for an all-invalid population

Deferred to public-run integration. The private phase successfully completed
its classification work, so it records a completed state without a winner.

### Reuse `max_population_bytes`

Rejected because that field explicitly bounds only the contiguous genome slab.
Extending it implicitly would make working-set policy ambiguous.

## Verification

- `tests/population_evaluation_test.c` proves callback order, optional
  validation, invalid-candidate suppression, budget enforcement, all-invalid
  completion, finite fitness, stable winner selection, retry, and repeat-call
  behavior.
- `tests/allocation_failure_test.c` proves evaluation allocation failure
  preserves initialized population storage.
- `tests/population_storage_test.c` and
  `tests/population_initialization_test.c` prove full reset and compatibility
  with the earlier lifecycle boundaries.
- GitHub issue: `https://github.com/dlworrell/evo/issues/12`
