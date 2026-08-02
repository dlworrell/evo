# ADR-0018: Bounded Deterministic Generation Statistics

Status: Accepted
Date: 2026-08-02
Decision owner: EVO

## Context

EVO 0.17.0 exposes the global best candidate, completed-transition count, and
termination reason. It does not expose the composition of the committed
population that produced the current run state. Consumers therefore cannot
distinguish a uniformly strong population from one strong candidate among many
weak or invalid candidates without reimplementing private evaluation policy.

The observer later delivered by issue #41 requires one deterministic read-only record
after generation zero and after each promoted child. Retaining an array sized
from `generation_limit` would make result memory grow with the run bound and
would introduce allocation, ownership, and failure behavior that observation
does not require.

## Decision

EVO 0.18.0 defines public generation-statistics schema version 1 and appends
one `evo_generation_statistics_t generation_statistics` value to
`evo_result_t`.

The record contains:

- schema and aggregation-policy version;
- committed generation index;
- population, valid, and invalid counts;
- the generation-local stable-best index and complete fitness evidence;
- component-wise fitness sums over valid evaluated candidates; and
- an explicit `has_best` flag.

Generation zero has index zero. A child produced from source generation `g`
has committed index `g + 1`. The record is computed for generation zero before
winner transfer and for a child before promotion, but it becomes public only
after the corresponding generation is committed. Each successful promotion
replaces the prior record. The terminal all-invalid child is therefore
retained with zero valid count, population-sized invalid count, no best, and
zero sums while the separate result genome continues to hold the earlier
global winner.

The implementation owns no statistics history and performs no statistics
allocation. Result storage is constant with respect to `generation_limit`.
EVO 0.19.0 now delivers each committed record synchronously through the
bounded read-only view governed by ADR-0019 without retaining a history array.

## Aggregation Policy Version 1

Candidates are visited once in ascending population index. Invalid records are
identified only by their validity and evaluated flags; their fitness payloads
are never read. Each valid record must be evaluated and contain finite fitness
components.

For each of the seven `evo_fitness_t` components, the accumulator begins at
positive zero and performs one C `double` addition per valid candidate in that
fixed order. The implementation does not reorder, parallelize, compensate,
weight, average, normalize, or otherwise reinterpret consumer evidence. If an
intermediate component sum becomes non-finite, statistics construction returns
`EVO_ERROR_EVALUATION`; the public run follows its existing empty-failure
contract.

The statistics boundary copies the stable generation-local best already
committed by population evaluation. It neither ranks candidates nor changes
the population best index, result winner, callback order, RNG state, or
stopping policy. Exact ties therefore retain the same lower-index generation
winner established by evaluation.

## Commit and Failure Semantics

- Generation-zero statistics are retained only when a valid winner can be
  transferred to the public result.
- Child statistics become visible only after atomic generation promotion.
- A failed provisional child never replaces the last committed record.
- Any public run failure destroys the result and returns an all-zero statistics
  value with version zero.
- Active-result rejection preserves the caller's statistics unchanged.
- `evo_result_destroy` resets the complete record to zero.
- The private bounded-run entry rejects generation-zero result evidence whose
  statistics do not exactly match its completed parent population.

## ABI Consequences

`evo_generation_statistics_t` is a new public value type. Its instance is
appended after `termination_reason`, preserving every pre-0.18.0
`evo_result_t` member offset. `sizeof(evo_result_t)` and array stride change,
so consumers must rebuild against the 0.18.0 header. No public function
signature or installed symbol changes.

## Alternatives Considered

### Retain every generation in a result-owned array

Rejected because storage and allocation failure would scale with
`generation_limit`, and the future observer can deliver each record without
retaining history.

### Publish only valid and invalid counts

Rejected because later convergence, diversity, evidence, and reporting policy
needs a versioned generation-local best and aggregate fitness boundary.

### Recompute statistics after the run

Rejected because prior populations have been released and an all-invalid
terminal population differs materially from the retained global winner.

### Include diversity now

Rejected because issue #44 owns its metric, work budget, invalid-candidate
policy, and statistics-schema extension.

### Invoke a consumer callback now

Rejected because callback lifetime, ordering, alias, and replay behavior are
separately governed by issue #41.

## Verification

- `tests/statistics_test.c` provides golden vectors for even, odd, one-member,
  tied, mixed-validity, and all-invalid populations.
- Invalid records carry non-finite poison values in the mixed and all-invalid
  vectors, proving their fitness payloads are excluded.
- Non-finite valid fitness and component-sum overflow reject without changing
  caller output.
- Bounded-run tests prove generation-zero, promoted, replay, tie, odd-tail,
  one-member, terminal all-invalid, and private preflight behavior.
- Wrapped allocation tests prove statistics add no allocation or release.
- GitHub issue: `https://github.com/dlworrell/evo/issues/40`
