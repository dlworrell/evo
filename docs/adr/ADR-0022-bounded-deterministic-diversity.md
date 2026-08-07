# ADR-0022: Bounded Deterministic Population Diversity

Status: Accepted
Date: 2026-08-03
Decision owner: EVO

## Context

EVO 0.21.0 exposes fitness and validity evidence but cannot distinguish a
homogeneous committed population from one containing meaningfully separated
candidates. Later convergence, stagnation, selection, operator-adaptation, and
reporting work requires an explicit diversity signal. The generic C17 core
cannot assume a source-recipe representation, and unbounded all-pairs work or
random sampling would violate its resource and replay contracts.

## Decision

EVO 0.22.0 defines diversity policy version 1. Every successfully evaluated
population records a normalized diversity value plus metric provenance, pair
count, and work units. Public generation-statistics schema version 3 appends
that evidence.

Only hard-valid evaluated candidates participate. EVO traverses every
unordered pair in fixed lexicographic population-index order: increasing left
index, then increasing right index with `left < right`. There is no sampling.
The schedule therefore needs no seed and consumes no initialization,
selection, crossover, mutation, or other operator RNG state.

The built-in byte-distance metric has version 1. It counts unequal byte
positions across all visited pairs and divides once by
`pair_count * genome_size`. The result is in `[0, 1]`: zero means bytewise
homogeneous, and one means every byte differs for every pair.

A consumer may instead append `genome_distance` and a nonzero
`genome_distance_version` to `evo_problem_t`. The callback receives two
read-only genome views, their common size, and the run context. It must be
deterministic for fixed inputs and context, retain no view, use no unrecorded
entropy, and return a finite normalized value in `[0, 1]`. A null callback
requires version zero and selects the built-in metric. A non-null callback
requires a nonzero consumer-owned version. Callback results are added in the
fixed pair order and divided by pair count. Invalid results return
`EVO_ERROR_EVALUATION`.

For `n` candidates, EVO computes `n * (n - 1) / 2` with divide-first checked
`size_t` arithmetic. `max_diversity_work`, appended to `evo_config_t`, must
cover the all-valid worst case for one generation:

- built-in metric: `pair_count * genome_size` byte comparisons;
- domain metric: `pair_count` callback invocations.

Overflow or an insufficient budget returns `EVO_ERROR_RESOURCE_LIMIT` before
any run callback. The same validation runs at the private evaluation boundary
before validity, fitness, or distance dispatch. A callback's internal work is
consumer-owned and must be bounded separately by that callback's contract.

Zero or one valid candidate has diversity zero, pair count zero, and work zero.
The domain callback is not invoked. Invalid candidates neither form pairs nor
consume diversity work.

Measurement runs once after validity and fitness evidence is complete. The
population temporarily owns those records while distances are measured. A
distance failure releases the records and restores the initialized or produced
genome population to its prior unevaluated state; consumer callback side
effects cannot be rolled back. On success, diversity evidence commits with the
evaluation. Statistics and completed-population validation inspect or copy the
stored evidence and never invoke the distance callback again.

Child-evaluation and generation-advancement policies advance to version 3, and
bounded-run policy advances to version 4, so their private evidence carries
diversity policy and metric provenance. Diversity is observational in 0.22.0:
it does not alter fitness comparison, tournament selection, operator rates,
termination, or winner retention.

## Public Evidence

Statistics schema version 3 appends:

- `diversity_policy_version`;
- `diversity_metric_version`;
- `diversity_pair_count`;
- `diversity_work_units`;
- `diversity`; and
- `diversity_uses_domain_distance`.

The policy version is 1 for every successful record. The built-in metric
publishes metric version 1 and a false kind flag. A domain callback publishes
its configured version and a true flag.

ADR-0028 makes this already-committed normalized value an input to mutation-
adaptation policy version 1 in EVO 0.27.0. Measurement order, budget, metric
authority, and RNG neutrality do not change. Schema 4 preserves the complete
schema-3 diversity prefix and records the inclusive low-diversity decision.

## ABI Consequences

The new problem fields, configuration budget, and schema-3 statistics fields
are appended after their respective pre-0.22.0 prefixes. Existing member
offsets are preserved, but structure sizes and array strides may change.
Binary compatibility is not assumed; consumers must rebuild against the
0.22.0 header. No public function signature or installed library symbol is
added. The implementation adds only private internal functions and no
allocation class.

## Alternatives Considered

### Randomly sample pairs

Rejected because sampling introduces a seed schedule, consumes or duplicates
RNG policy, and makes exact small-population evidence weaker. Caller-bounded
all-pairs work is simpler and fully reviewable.

### Include invalid candidates

Rejected because hard-invalid genomes are outside the ranked population and
may not have meaningful representation-level distance.

### Use only byte distance

Rejected because opaque byte genomes remain supported, but structured domains
such as source-transformation recipes need semantic distance with explicit
version provenance.

### Recompute diversity while constructing statistics

Rejected because statistics validation and bounded-run state checks could
repeat a consumer callback and its side effects. Stored evidence makes
observation callback-free.

### Let diversity immediately influence selection or mutation

Rejected because issue #44 owns measurement evidence. Selection, operators,
adaptation, convergence, and stagnation have separate dependency-ordered
policies.

## Verification

- `tests/diversity_test.c` locks zero-valid, one-valid, homogeneous, mixed,
  maximally separated, odd, and invalid-heavy golden vectors.
- The domain callback test locks lexicographic pair order, metric versioning,
  statistics propagation, and replay-identical values and traces.
- Budget and overflow tests prove rejection before initializer, validity,
  evaluator, or distance callbacks.
- Malformed domain values prove evaluation rollback and the public empty-
  failure contract.
- Equivalent built-in and domain metrics followed by tournament selection
  prove identical selected indices and final RNG state.
- CMake, GNU Autotools, and AES-BLD-001 run the same normative test across the
  supported compiler matrix.
- GitHub issue: `https://github.com/dlworrell/evo/issues/44`
