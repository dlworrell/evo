# ADR-0021: Versioned Hard-Constraint and Fitness-Comparison Policy

Status: Accepted
Date: 2026-08-02
Decision owner: EVO

## Context

EVO already exposes an optional `is_valid` callback, seven-field
`evo_fitness_t` evidence, deterministic population winners, tournament
selection, odd-tail elite cloning, and a global best-so-far result. The
implementation consistently maximizes caller-computed `fitness.total`, but the
relationship among hard validity, `constraint_penalty`, and `total` was not a
complete contract. In particular, every finite penalty value was accepted,
and evaluation, completed-population validation, tournament selection, and
global-best replacement contained separate comparison expressions.

That ambiguity creates two risks. A caller cannot know whether EVO will apply
`constraint_penalty` again after the caller computes `total`, and a later
change to one comparison site could alter replay while the other sites retain
the old ordering. Source optimization also requires a clear distinction
between candidates that fail correctness gates and candidates that remain
admissible but carry a measured soft-constraint cost.

## Decision

EVO 0.21.0 defines public fitness-comparison policy version 1:

```c
#define EVO_FITNESS_COMPARISON_POLICY_VERSION UINT32_C(1)
```

The policy has three layers.

### Hard admissibility

`problem->is_valid` remains the hard gate. A null validator admits every
candidate. Otherwise EVO calls the validator exactly once per candidate in
ascending population order and never calls `evaluate` for a false result.
Hard-invalid records remain unevaluated with zero fitness, are absent from
statistics sums, are never sampled by selection, and cannot be an elite or a
global winner.

Correctness, build, security, or other conditions that must not be traded for
performance belong in this hard gate or in the source optimizer's later
candidate-assurance gates. They must not be represented only as a large soft
penalty.

### Soft-penalty evidence

`evo_fitness_t.constraint_penalty` is a finite, non-negative penalty magnitude.
Both signed zero representations are accepted as zero. A negative, NaN, or
infinite penalty is malformed evaluation evidence and returns
`EVO_ERROR_EVALUATION` before provisional records commit. The public
`evo_run` failure path destroys all private owners and restores the complete
zero result.

The five component fields other than the penalty, plus `total`, may have
either finite sign. Their domain, normalization, and weighting remain caller
policy.

### Caller-owned total and stable order

`fitness.total` remains the authoritative caller-computed scalar objective and
must be finite. A caller that wants a soft penalty to affect selection accounts
for it while computing `total`, commonly by subtracting an appropriately
weighted penalty magnitude from its unpenalized objective. EVO does not
subtract, normalize, cap, or reweight `constraint_penalty` independently. It
also does not infer or verify the caller's aggregation formula because the
component directions and weights are domain-owned.

Among hard-valid evaluated candidates, policy version 1 orders by:

1. greater `fitness.total`;
2. earlier committed generation for an exact total tie; and
3. lower population index within the same generation.

Other fitness components, including `constraint_penalty`, never provide an
independent tie-break. A total-only evaluator that zero-initializes the other
components remains source-compatible and produces the same logical ordering
and replay as version 0.20.0.

## One Comparison Authority

`src/fitness.c` owns evidence validation, hard-valid/evaluated rankability,
and stable comparison. Population evaluation, completed-population validation,
tournament selection, and global-best replacement all call that authority.
The current odd-tail elite clones the stable parent best only after completed-
population validation has reconstructed that same result through the shared
comparator.

Private evaluated populations record comparison policy version 1. Child-
evaluation, generation-advancement, and bounded-run evidence propagate the
same version. Public generation-statistics schema version 2 appends
`fitness_comparison_policy_version`, so every successful result and callback
records the policy that established its generation-local best. Future policy
changes must use a new version and cannot silently reinterpret retained replay
evidence.

ADR-0022 later advances public statistics to schema version 3 and the three
private policies to child-evaluation version 3, generation-advancement version
3, and bounded-run version 4. Comparison policy remains version 1; the newer
versions add diversity provenance without changing ranking.

Child-evaluation policy advances to version 2, generation-advancement policy
to version 2, and bounded-run policy to version 3 because their evidence now
records or depends on comparison policy version 1.

## Failure and Resource Semantics

Penalty validation and comparison allocate no memory, consume no RNG word,
invoke no additional callback, and add no caller-configured resource budget.
All fitness records remain provisional until every hard-valid candidate
returns policy-valid evidence. Any malformed valid record releases the
provisional record array and preserves an initialized generation as
unevaluated. At the public boundary, the existing failure cleanup returns a
fully empty result even if an earlier generation had committed privately.

An invalid candidate's zero fitness is structural state, not a penalty. EVO
does not inspect or validate a fitness payload for a hard-invalid candidate
because the evaluator is never invoked for it.

## ABI Consequences

The comparison-policy macro adds no symbol. The three comparison helpers are
internal implementation symbols declared only by the non-installed
`src/internal/fitness.h`; no public function declaration or signature changes.
`fitness_comparison_policy_version` is appended to
`evo_generation_statistics_t`, preserving every existing statistics member
offset. The statistics schema advances from version 1 to version 2. Depending
on ABI padding, the containing structure sizes and array strides may remain
unchanged or increase; binary layout compatibility is not assumed. Consumers
must rebuild against the 0.21.0 header.

`evo_fitness_t`, `evo_problem_t`, `evo_config_t`, public function signatures,
allocation classes, and resource-budget fields do not change.

## Alternatives Considered

### Subtract the penalty inside EVO

Rejected because `total` is already caller-computed. Applying the penalty a
second time would silently change consumer weights and break total-only and
custom-aggregation workloads.

### Use a negative penalty contribution

Rejected in favor of a non-negative magnitude, which has one unambiguous
evidence sign while leaving maximize/minimize direction and weighting in the
caller-owned total.

### Rank by penalty before total

Rejected because that would introduce a library-selected lexicographic
objective and change existing total-based replay. Hard constraints already
have a separate non-tradeable gate.

### Verify an exact sum of fitness components

Rejected because EVO does not own component direction, normalization, or
weights, and valid total-only workloads intentionally leave component evidence
at zero.

### Keep comparison expressions at each call site

Rejected because independent expressions can drift and change selection,
elitism, or global-winner replay without a policy-version change.

## Verification

- `tests/fitness_test.c` locks policy version 1, penalty sign and finiteness,
  hard-valid/evaluated rankability, total authority, generation/index ties,
  atomic malformed-penalty failure, and total-only replay compatibility.
- `tests/population_evaluation_test.c` proves a negative penalty rolls back the
  complete provisional record set and permits deterministic retry.
- `tests/selection_test.c` proves missing or stale comparison-policy evidence
  rejects before RNG consumption.
- Existing evaluation, selection, odd-tail, bounded-run, statistics, observer,
  application-stop, allocation-failure, and installed-consumer tests exercise
  the shared policy through every current ranking and evidence path.
- GitHub issue: `https://github.com/dlworrell/evo/issues/43`
