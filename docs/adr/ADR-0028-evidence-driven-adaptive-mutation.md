# ADR-0028: Evidence-Driven Adaptive Mutation

Status: Accepted
Date: 2026-08-07
Decision owner: EVO

## Context

EVO 0.26.0 applies one caller-requested mutation rate to every attempted
transition. That static policy is deterministic and replayable, but it cannot
respond when committed populations stop improving or lose diversity. The core
roadmap therefore requires a bounded adaptive policy before checkpointing,
storage recycling, and parallel evaluation are added.

Adaptation must not inspect provisional children, wall time, addresses, hidden
entropy, or callback-local state. It must preserve the exact static behavior of
zero-initialized callers and make every rate decision understandable from
committed public evidence. A later checkpoint must also be able to resume the
same next mutation rate without reconstructing an unrecorded history.

## Decision

EVO 0.27.0 publishes mutation-adaptation policy version 1. It appends a
disabled-by-default policy to `evo_config_t`:

```c
bool adaptive_mutation_enabled;
double adaptive_mutation_min_rate;
double adaptive_mutation_max_rate;
double adaptive_mutation_step;
double adaptive_mutation_diversity_threshold;
bool adaptive_mutation_reset_on_improvement;
```

When disabled, every appended payload must be its zero-initialized value. The
configured `mutation_rate` remains unchanged for every applicable transition,
and operator RNG consumption, callback dispatch, child bytes, and replay are
identical to 0.26.0.

When enabled, `mutation_rate`, both bounds, the step, and the diversity
threshold must be finite. Rates and the threshold lie in `[0, 1]`, minimum is
not greater than maximum, and step lies in `(0, 1]`. Validation occurs before
population allocation or callbacks for a positive-limit run that can dispatch
mutation. Generation-zero-only execution and a one-member transition made
entirely of a compatibility or explicit elite do not inspect unused mutation
or adaptation fields.

### Committed state and initial decision

The private state is exactly:

- the effective rate for the next attempted transition;
- a saturating count of consecutive committed child generations without a
  strict global-best improvement; and
- an initialization marker used only to reject malformed lifecycle calls.

After generation zero commits, EVO clamps the requested base `mutation_rate`
to the configured interval. Diversity is low when committed diversity is less
than or equal to the configured threshold. If generation-zero diversity is
low, EVO then raises the clamped rate by one step, capped at the maximum. This
decision consumes no RNG and invokes no callback.

### Later committed decisions

After a child is completely evaluated and atomically promoted, EVO compares
its stable best candidate with the retained global winner through fitness-
comparison policy version 1. Adaptation then applies this precedence:

1. A strict global-best improvement resets the stagnant count to zero.
2. If reset-on-improvement is enabled, that improvement selects the minimum
   rate, even when diversity is low.
3. If reset is disabled and the improved population is not low-diversity, the
   current rate is held.
4. If reset is disabled and the improved population is low-diversity, the rate
   increases by one step.
5. A non-improving committed child increments the stagnant count, saturating
   at `SIZE_MAX`, and increases the rate by one step whether or not diversity
   is low.

An increase computes the capped result without first evaluating an overflowing
or overshooting addition: if `step > max - current`, the result is `max` and
the max-clamp flag is set; otherwise the result is `current + step`. Reaching
the maximum exactly is not reported as a clamp. Trying to increase when already
at the maximum is reported as a clamp.

The decision commits before natural stopping and application callbacks inspect
the generation. A terminal generation can therefore expose a deterministic
next-rate decision that will not be consumed. It remains part of the complete
audit trace and checkpoint boundary.

### Human-readable audit projection

`evo_generation_statistics_t` advances to schema 4 and appends the complete
projection for each committed generation:

- policy version and enabled/reset controls;
- prior and effective mutation rates;
- configured minimum, maximum, step, and diversity threshold;
- stagnant-generation count;
- low-diversity and strict-global-improvement booleans;
- minimum- and maximum-clamp booleans; and
- one `evo_mutation_adaptation_reason_t` value.

The reasons distinguish not-applicable, disabled, initial, low-diversity,
stagnation, stagnation plus low diversity, improvement reset, and improvement
hold. For generation zero, `mutation_rate_prior` is the requested base rate.
For every later generation it is the exact effective rate that produced that
committed population. `mutation_rate_effective` is the rate selected after the
commit for a possible next transition.

Mutation-applicable positive-limit runs always publish policy version 1,
including disabled static runs. Generation-zero-only runs and positive-limit
runs with no possible mutation path keep the complete projection zero with the
not-applicable reason. The observer receives this projection in committed
generation order, while the result retains the final record in constant space.

### Lifecycle provenance

The effective rate is copied into produced-population state and pair,
singleton, elite, child-evaluation, generation-advancement, and bounded-run
evidence. Every lifecycle boundary rejects mismatched rate provenance before
committing ownership. Consumer mutation callbacks receive the same effective
rate unchanged when their deterministic probability event is selected.
Reference byte-XOR mutation uses the same effective rate for its probability
gate and never invokes the consumer callback.

Child-evaluation and generation-advancement policy versions advance to 7;
bounded-run policy advances to 9. These are private evidence-schema changes.

### Checkpoint requirements and fulfillment

The checkpoint format originally assigned to issue #51 was required to persist
enough committed authority to resume without a hidden warm-up history:

- mutation-adaptation policy version and every configured policy field;
- the latest schema-4 generation statistics;
- effective next rate and saturating stagnant-generation count;
- completed generation and global-winner identity/evidence; and
- all existing RNG and operator-policy state required by the transition.

Restore must validate that the checkpoint projection and private adaptive state
agree before any allocation, RNG use, or callback. Recomputing a different rate
from an incomplete retained window is not permitted.

ADR-0030 fulfills these requirements in EVO 0.29.0. Checkpoint format 1
persists every listed field, `evo_checkpoint_view_t` projects the adaptive and
stopping state in generation order, and resume reconciles the private state
with schema-4 statistics before allocating restored owners or invoking a
callback. The checkpoint/resume tests prove exact adaptive and patience
continuation against an uninterrupted run.

## Human-Readable Abstraction Assessment

The adaptive state is a direct constant-space control record, not a compressed,
probabilistic, cached, indexed, or accelerated substitute for another
authority. The ordered observer sequence of schema-4 records is its explicit
human-readable audit projection: each entry names the source generation,
source evidence, prior rate, next effective rate, bounds, stagnant count,
clamp/reset facts, and reason.

ADR-0026 therefore does not require a second projection layer for this change.
There is no hidden table, filter, heuristic score, or cache. If a future
implementation batches decisions, compresses traces, or caches diversity or
improvement state, the accelerated representation remains derived and must be
differentially equivalent to this ordered committed-generation projection.

## Consequences

- Zero-initialized callers retain exact static-rate replay.
- Enabled callers receive deterministic bounded adaptation with no new
  allocation, RNG domain, callback, clock, process, or address input.
- The public observer and final result explain every committed decision without
  access to private state.
- Appending configuration and statistics members changes structure sizes and
  array strides, so consumers must rebuild.
- The policy reacts to committed evidence only; it does not predict fitness,
  tune crossover, modify selection, or retain an unbounded history.
- A high stagnant count saturates rather than wrapping.

## Alternatives considered

### Use random rate perturbations

Rejected because an additional stochastic policy obscures causality and would
require a new RNG domain without improving the first bounded reference rule.

### Adapt from provisional child evidence

Rejected because a failed or rolled-back generation must have no externally
visible policy effect.

### Retain only the effective rate

Rejected because a bare number cannot explain whether the decision followed
initial clamping, low diversity, stagnation, or improvement reset.

### Derive the rate later from retained statistics history

Rejected because EVO intentionally retains only one public statistics record
and checkpoint replay cannot depend on an unbounded implicit history.

## Verification

- `tests/adaptive_mutation_test.c` locks exact dyadic traces for initial clamp,
  inclusive diversity equality, stagnation, low diversity, improvement reset,
  improvement hold, minimum/maximum bounds, disabled behavior, alias rejection,
  atomic failures, public observer order, consumer-rate forwarding, reference-
  operator callback bypass, replay, and unused-policy compatibility.
- Pair, singleton, elite, child-evaluation, generation-advancement, and bounded-
  run tests lock rate provenance through the complete private lifecycle.
- The installed consumer validates the appended schema and not-applicable
  projection through only the installed public header and library.
- CMake, GNU Autotools, and AES-BLD-001 enumerate the same twenty-three
  production sources and twenty-seven normative tests.
- GitHub issue: `https://github.com/dlworrell/evo/issues/49`
