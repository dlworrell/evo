# EVO-001: Evolutionary Optimization Library Contract

Status: Baseline
Version: 0.43.0
Owner: EVO

## Scope Boundary

This specification governs the reusable deterministic C17 evolutionary-search
core packaged through version 0.43.0. Version 0.43.0 changes no installed
core semantics or public 0.37.0 compatibility surface; its new implementation
is confined to the private source-optimizer executable-facing command and
execution-plan foundation. This contract does not define C-project
ingestion, Clang/LLVM analysis, structured source transformations, isolated
candidate builds, baseline-versus-candidate measurement, optimized patches, or
product-level replay artifacts.

Those source-to-source product responsibilities are defined separately by the
draft 1.0 target in `EVO-002-source-optimizer-contract.md`, its EVO-002A
provider addendum, and `EVO-003-product-command-contract.md`. An EVO-001
conforming run or compiler-option adapter alone must not be described as an
optimized C codebase.

## Purpose

EVO provides a reusable C17 interface for deterministic, bounded evolutionary
optimization. Consumers define genomes and problem-specific operations through
callbacks and may explicitly select reference byte-genome operators; EVO owns
orchestration, reproducibility, and evidence without inferring representation.

## Human-Readable Abstraction Contract

EVO Core follows the Human-Readable Abstraction Rule defined by ADR-0026:

```text
Machine-optimized structure
    +
Human-readable audit projection
```

An implementation may use a compressed, cached, indexed, probabilistic, or
otherwise accelerated internal representation only when it also defines the
exact reference semantics and exposes a deterministic projection in EVO domain
terms. The accelerated layout may not become opaque authority.

The projection must use stable identifiers and ordering; identify the source
state, construction policy, and representation version; state its complete or
bounded scope; and expose the logical members, ranges, mappings, or decisions
within that scope. A large structure may use caller-bounded pages or windows
only when their bounds, continuation, completeness, and reconstruction order
are explicit.

Exact accelerators require differential tests against an explicit reference
path. A cache is derived state and requires source identity, freshness,
invalidation, and an exact fallback or recomputation path. A probabilistic
structure is a precheck only: it may not independently accept, reject, rank,
select, suppress, terminate, or publish a committed result. Exact authority
must confirm the decision.

If an accelerator is stale, corrupt, incompatible, over budget, or cannot
produce a projection reconcilable with exact authority, EVO must reject it or
fall back to the bounded reference path. It may not silently publish a partial
or plausible answer. A version containing no accelerated structure satisfies
this section without adding an artificial projection API, but its design
record must state that determination.

The audited 0.25.0 core uses explicit bounded genome and evaluation arrays plus
direct deterministic scans. It contains no compressed, probabilistic, cached,
or indexed accelerator that controls a run; stable rank selection specifically
does not introduce a rank cache. This finding does not pre-approve later
checkpoint, storage-recycling, parallel-evaluation, or integration work.

Version 0.26.0 retains that direct exact representation. Reference byte
operators read and write the bounded genome arrays without a cache, index,
compressed decision record, probabilistic precheck, or other accelerator.
ADR-0027 therefore records this change as not requiring a separate projection
API while defining the selectors, parameters, boundaries or masks, affected
byte ranges, RNG schedule, and provenance needed for human review.

Version 0.27.0 adds no accelerator. Its canonical adaptive-mutation state is
one explicit next rate plus one saturating committed-stagnation count. Public
schema-4 statistics expose the corresponding ordered decision projection for
human audit. ADR-0028 defines the projection and the later checkpoint fields
required to resume it without hidden history.

Version 0.28.0 also adds no accelerator. Its canonical secure-erasure registry
is the fixed set of direct result, population-genome, population-evaluation,
and provisional-evaluation owners with exact byte counts and explicit
dispositions. ADR-0029 defines that stable human-readable registry and its
exact-once lifecycle tests. There is no compressed, cached, indexed,
probabilistic, pooled, or address-keyed authority.

Version 0.29.0 adds canonical binary checkpoint format 1 but does not make its
CRC-32, FNV-1a configuration fingerprint, or offset table authoritative. The
allocation-free `evo_checkpoint_view_t` and indexed candidate accessor expose
the complete logical configuration, generation, population, RNG, statistics,
adaptation, stopping, ownership, and identity state in stable order. Decoded
invariants and exact canonical configuration bytes remain authority. The
retained EVO-HRA-002 audit records this assessment; a future compressed,
indexed, cached, deduplicated, or probabilistic checkpoint representation must
independently satisfy ADR-0026.

Version 0.30.0 adds an exact allocation accelerator: the optional two-slot
population recycler. Exact private pointer/count owners remain canonical. The
complete allocation-free `evo_population_storage_registry_t` projection uses
stable identities rather than addresses and exposes both slots' lifecycle,
capacity, population provenance, handoff, reset, erasure, and presence state in
fixed order. Every use reconciles that projection with the physical owners and
committed generation; a stale, aliased, malformed, or unreconcilable registry
fails closed. The explicit 0.29.0 allocation path remains executable and is the
differential oracle. ADR-0031 and the retained EVO-HRA-003 audit define and
verify this conformance boundary.

Version 0.31.0 adds an exact execution accelerator: bounded worker evaluation.
The complete serial evaluator remains executable and is the differential
oracle. `evo_evaluation_schedule_t` projects every candidate in stable index
order with logical worker, wave, completion/failure/cancellation disposition,
and commit presence/order. Private evaluation records and completed-population
validation remain authority; operating-system thread identity, completion
timing, atomic scheduler state, and the projection cannot commit fitness.
ADR-0032 and the retained EVO-HRA-004 audit define and verify this boundary.

Version 0.32.0 adds no accelerated core structure. Its maintained benchmark
retains explicit cases, seeds, semantic results, complete generation traces,
and raw measurement repetitions in canonical JSON. Aggregates and the scoped
FNV locator are derived, non-authoritative values. The human-readable report is
regenerated only by parsing and validating the JSON. ADR-0033 and the retained
EVO-HRA-005 audit define and verify this evidence boundary.

Version 0.33.0 also adds no accelerated core structure. Four external
reference consumers use immutable fixtures and direct bounded arrays, retain a
stable ordered JSON registry with complete generation traces, and explicitly
project all checkpoint candidates or logical worker assignments when those
capabilities are used. Exact golden-object comparison is authority; the
Markdown summary is derived only afterward. ADR-0034 and the retained
EVO-HRA-006 audit define and verify this installed-consumer evidence boundary.

Version 0.34.0 changes no accelerated core structure. The new private
source-optimizer foundation is outside the installed EVO-001 API and uses exact
snapshot bytes plus explicit bounded file, compilation-unit, policy, and gate
registries. Its versioned FNV labels are non-authoritative diagnostics and its
complete JSON/Markdown projections derive from the same owner. ADR-0035,
EVO-002, and EVO-HRA-007 govern that separate boundary.

Version 0.35.0 also changes no accelerated core structure. Its private
project-analysis foundation remains outside the installed EVO-001 API and uses
complete bounded translation-unit, source-location, structural, compiler,
runtime, and opportunity arrays with direct scans. Canonical JSON and Markdown
derive from the same retained owner; no cache, index, compressed form, or
probabilistic authority is introduced. ADR-0036, EVO-002, and EVO-HRA-008
govern that separate boundary.

Version 0.36.0 also changes no accelerated core structure. Its private recipe
foundation remains outside the installed EVO-001 API and uses explicit bounded
transformation records, resolved edges, direct scans, and stable topological
ordering. Canonical JSON is embedded in the fixed genome, Markdown projects
the complete logical model, and decode reconstructs derived facts from live
authority before exact-byte acceptance. ADR-0037, EVO-002, and EVO-HRA-009
govern that separate boundary.

Version 0.37.0 also changes no accelerated core structure. Its private
AST-aware transformation foundation remains outside the installed EVO-001 API
and uses three stable static capability records, direct bounded dispatch, and
one exact half-open edit or explicit no-change record as authority. Complete
catalogue/application JSON and derived Markdown expose the logical model; no
AST cache, transformation index, compressed form, or probabilistic authority
is introduced. ADR-0038, EVO-002, and EVO-HRA-010 govern that separate
boundary.

## Public Interface

The public API is declared in `include/catalyst/evo/evo.h`.

### Problem definition

`evo_problem_t` declares:

- the byte size of one genome;
- initialization, mutation, crossover, and evaluation callbacks;
- an optional validity callback;
- an optional versioned normalized genome-distance callback;
- a stable nonzero semantic problem identity for checkpoint operations;
- an explicit evaluator callback thread-safety declaration; and
- an opaque consumer context passed to callbacks.

The consumer owns callback code and context lifetime. Callback behavior must be
deterministic for a fixed input, context, and random stream unless the consumer
records an additional source of variation.

### Fitness, constraints, and comparison

`EVO_FITNESS_COMPARISON_POLICY_VERSION` is `1`.

`problem->is_valid` is the hard admissibility gate. A false candidate is never
passed to `evaluate`, included in statistics fitness sums, sampled by
selection, retained as an elite, or considered for the global winner. A null
validator admits every candidate. Conditions that cannot be traded against
fitness must use this hard gate or a higher-level candidate-assurance gate.

For a hard-valid candidate, all `evo_fitness_t` fields must be finite and
`constraint_penalty` must be a non-negative soft-constraint penalty magnitude.
Both signed zeros are accepted as zero. `fitness.total` is the authoritative
caller-computed scalar objective. The caller accounts for any desired penalty
effect when computing `total`; EVO never subtracts, normalizes, or reweights
`constraint_penalty` independently and does not infer a component aggregation
formula.

Comparison policy version 1 accepts only hard-valid evaluated evidence, then
orders it by greater `total`, earlier committed generation for an exact tie,
and lower population index within that generation. No other fitness field
provides an independent tie-break. Total-only evaluators that zero-initialize
the other fields remain valid and replay-compatible.

### Run configuration

`evo_config_t` records population size, generation limit, tournament size,
crossover rate, mutation rate, random seed, `max_genome_bytes`,
`max_population_bytes`, `max_evaluation_bytes`, and
`max_child_population_bytes`, followed by the optional
`generation_observer` and its caller-owned
`generation_observer_context`, then the optional `generation_stop` and its
independent caller-owned `generation_stop_context`, followed by
`max_diversity_work`, then the disabled-by-default fitness-target,
tolerance/patience, and diversity-floor stopping controls, then
`elite_count_enabled` and `elite_count`, then selection-policy version 1's
`selection_policy`, `rank_base_weight`, and `rank_step_weight`, then byte-
operator policy version 1's `crossover_operator` and `mutation_operator`, then
mutation-adaptation policy version 1's enable flag, finite minimum, maximum,
step, inclusive diversity threshold, and reset-on-improvement flag, followed
by secure-erasure policy version 1's disabled-by-default enable flag, then the
checkpoint input/output budget, caller-owned delivery buffer, synchronous
checkpoint observer and context, and stable semantic context identity.
The complete pre-0.31.0 prefix is followed by population-recycling control,
its storage observer and context, then parallel evaluation's worker count,
library scratch budget, schedule observer, and observer context.

`max_genome_bytes` is trusted caller policy for the largest individual genome
allocation accepted by `evo_run`. It avoids a platform-specific hard-coded
limit.

`max_population_bytes` is trusted caller policy for the contiguous genome slab
owned by the internal population subsystem. Before allocation, EVO proves that
`population_size * genome_size` is representable as `size_t` and no greater
than this budget. The v0.3.0 field bounds the complete storage allocation made
by that subsystem. It does not silently authorize future fitness arrays,
second-generation buffers, checkpoint state, or a total run working set; each
additional allocation class requires an updated specification and explicit
policy.

`max_evaluation_bytes` independently bounds the private candidate-evaluation
record array. EVO checks
`population_size * sizeof(evo_candidate_evaluation_t)` for `size_t` overflow
before allocation. The field does not authorize future generation buffers,
operator scratch space, checkpoint state, or parallel-worker storage.

`max_evaluation_worker_scratch_bytes` independently bounds the sole temporary
library scheduler allocation. For positive worker count it must be at least the
exact checked value reported by `evo_evaluation_worker_scratch_size`; zero
workers require a zero budget. This budget does not authorize the candidate-
evaluation record array, population storage, checkpoints, consumer state, or
implementation-defined POSIX thread stacks. The number of such runtime thread
resources is bounded exactly by the worker count.

`max_child_population_bytes` independently bounds one private child-population
genome slab. EVO checks the same `population_size * genome_size` arithmetic
before allocating this second slab. It is required when `generation_limit` is
positive and is unused when the limit is zero. The field does not authorize
child evaluation records, operator scratch space, checkpoints, or an aggregate
run working set.

`max_checkpoint_bytes` bounds both checkpoint capture and untrusted checkpoint
input. Capture is disabled when `checkpoint_observer` is null and the buffer
pointer and size are zero. Enabled capture requires an exact caller-owned
buffer of at least `evo_checkpoint_size` bytes, a nonzero problem identity,
and a nonzero context identity. The buffer, its capacity, the checkpoint
observer and context pointers, and this input/output budget are delivery
resources: they are not serialized and do not affect deterministic
configuration equality. EVO never retains, releases, or securely erases a
caller-owned checkpoint buffer.

Ordinary zero-limit execution continues to ignore unused transition policy.
Checkpoint-enabled zero-limit execution validates selection and byte-operator
selectors because their canonical provenance is emitted for later exact
configuration matching. Malformed checkpoint configuration rejects before
population allocation, RNG use, initialization, evaluation, or observation.

`generation_observer` is a synchronous, non-stopping callback for committed-
generation evidence. A null callback disables observation. Its context is
independent caller-owned state; EVO never inspects, allocates, releases, or
retains that pointer. Observer delivery adds no memory-budget requirement.

`generation_stop` is a synchronous decision callback for a committed
generation from which another transition could otherwise be attempted. A null
callback disables application stopping. Its context is independent caller-
owned state; EVO never inspects, allocates, releases, or retains that pointer.
Stop delivery adds no memory-budget requirement.

`max_diversity_work` bounds one generation's diversity measurement. EVO
checks the all-valid worst case before any run callback. Built-in byte-metric
units are byte comparisons; domain-distance units are callback invocations.
The budget is required even when `generation_limit` is zero because generation
zero is measured. A population that cannot form a pair requires zero work.

`fitness_target_enabled` activates `fitness_target`. The target may be any
finite `double`, including a negative value. When disabled, the target payload
must be zero.

`stagnation_enabled` activates `improvement_tolerance` and
`stagnation_patience` together. The tolerance must be finite and non-negative,
and patience must be a positive number of committed child generations. When
disabled, both payloads must be zero.

`diversity_floor_enabled` activates `diversity_floor`, which must be finite and
in `[0, 1]`. When disabled, the floor payload must be zero. These canonical
disabled representations make a zero-initialized configuration preserve the
0.22.0 stopping surface exactly. Malformed controls return
`EVO_ERROR_INVALID_ARGUMENT` before any run callback.

### Elite-preservation policy

`EVO_ELITE_POLICY_VERSION` is `1`.

For a positive `generation_limit`, disabled mode requires
`elite_count == 0` and preserves pre-0.24.0 behavior: one stable-best elite for
an odd population and no elite for an even population. Enabled mode accepts an
explicit count in `[0, population_size]`. Zero is a valid explicit request. A
count above population size or a nonzero disabled payload returns
`EVO_ERROR_RESOURCE_LIMIT` before a run callback. A zero generation limit does
not validate unused transition policy.

For each transition, EVO derives:

```text
requested = elite_count_enabled ? elite_count : population_size % 2
effective = min(requested, source_valid_count)
ordinary_offspring = population_size - effective
```

The cap prevents duplicate elite ownership when fewer hard-valid parents exist
than requested. Every distinct valid source is retained once when the request
exceeds that set, and ordinary offspring fill the remaining slots. A completed
all-invalid population stops before another transition.

Ordinary offspring occupy the child prefix. Stable elites occupy the suffix in
best-to-worst order under fitness-comparison policy version 1. Complete pairs
fill the largest even portion of the prefix. If the prefix is odd, singleton-
child policy version 1 uses the next unused pair-selection stream to select one
valid parent, clones it, and runs the standard child-indexed mutation stream.
The singleton invokes no crossover and allocates no scratch sibling. Elite
ranking and copies allocate nothing, consume no RNG, and invoke no callback.

### Parent-selection policy

`EVO_SELECTION_POLICY_VERSION` is `1`.

`EVO_SELECTION_TOURNAMENT == 0` is the canonical compatibility default. Both
rank weights must be zero. Whenever an ordinary parent draw is required,
`tournament_size` must be in `[1, population_size]`; a transition with no
ordinary child may leave it zero because selection is unused.

`EVO_SELECTION_RANK` requires `tournament_size == 0`,
`rank_base_weight > 0`, and any non-negative `rank_step_weight`. For `n` valid
candidates, stable rank zero is best and rank `r` receives exact weight:

```text
rank_base_weight + (n - 1 - r) * rank_step_weight
```

Configuration is accepted only when the all-valid total
`n * rank_base_weight + n * (n - 1) / 2 * rank_step_weight` for configured
population size `n` is representable as `size_t`. Positive-limit preflight
performs this check before allocation or callback dispatch. Invalid enum
values, conflicting fields, zero rank base, and overflowing totals return
`EVO_ERROR_RESOURCE_LIMIT`. A zero generation limit does not validate unused
transition policy.

Ranks use fitness-comparison policy version 1 and are therefore stable on exact
ties by lower population index. Invalid candidates have no rank or selection
weight. Each rank draw dry-resolves every actual weight before RNG consumption,
then samples one unbiased bounded ticket. Ticket intervals are traversed in
ascending population-index order. Tournament and rank modes share the existing
selection-domain streams; crossover and mutation domains are unchanged.

### Reference byte-operator policy

`EVO_BYTE_OPERATOR_POLICY_VERSION` is `1`.

`EVO_CROSSOVER_CONSUMER == 0` and `EVO_MUTATION_CONSUMER == 0` are the
canonical compatibility defaults. They preserve the complete pre-0.26.0
callback, clone, no-op, and RNG behavior. Explicit nonzero selectors opt into
bounded byte semantics; EVO never infers them from a missing callback. A
built-in selector takes precedence over a non-null consumer callback and never
invokes it.

Defined crossover selectors are one-point, two-point, and uniform. Defined
mutation policy is byte-XOR. Invalid enum values return
`EVO_ERROR_RESOURCE_LIMIT` before an active transition allocates or invokes a
callback. A zero generation limit does not validate unused transition policy.
Every positive-limit run requires defined selector values so provenance is
canonical. A one-member compatibility or explicit-one-elite path may still
ignore unused crossover/mutation rates and callbacks; a one-member explicit-
zero path validates selection and mutation but not the unused crossover rate.

Every produced population and pair, singleton, elite, child-evaluation,
generation-advancement, and bounded-run evidence record policy version 1 and
both selected enums. A completed produced population is valid only when that
provenance matches the active configuration. The helpers add no allocation or
resource budget and operate on the complete bounded byte genome.

These selectors are EVO Core utilities, not C-source transformation operators.
Raw source text may not be passed through byte crossover or mutation as a
substitute for EVO-002's structured transformation recipes and AST-aware
catalogue.

### Adaptive-mutation policy

`EVO_MUTATION_ADAPTATION_POLICY_VERSION` is `1`. A disabled policy requires
the complete appended adaptation payload to be zero and preserves the
configured static `mutation_rate` exactly. An enabled policy requires finite
minimum, maximum, step, and diversity threshold values; bounds and threshold
lie in `[0, 1]`, minimum is no greater than maximum, and step lies in `(0, 1]`.

After generation zero commits, EVO clamps the requested base rate into the
configured interval. Diversity is low at the inclusive boundary
`diversity <= threshold`; low generation-zero diversity raises the clamped
rate by one step up to the maximum. After each later committed child, a strict
global-best improvement resets the consecutive stagnant count. With reset
enabled it selects the minimum rate. Without reset it holds the rate unless
diversity is low, in which case it raises one step. A non-improving child
increments the count, saturating at `SIZE_MAX`, and raises one step whether or
not diversity is low.

Adaptation occurs only after evaluation and atomic promotion and before stop
classification or application callbacks. It consumes no RNG, allocation,
clock, process, address, or new callback input. The effective rate is copied
unchanged into the next transition configuration, where consumer and reference
mutation share it as their probability/intensity policy. Pair, singleton,
elite, child-evaluation, generation-advancement, and bounded-run evidence all
carry the rate actually used.

Generation-zero-only execution and one-member transitions containing only a
compatibility or explicit elite do not validate unused mutation/adaptation
fields and publish a zero not-applicable projection. Every other positive-
limit run publishes policy version 1, including a disabled static run. ADR-0028
defines the reason enum, exact clamp semantics, public schema-4 projection,
replay contract, and checkpoint requirements.

### Diversity policy

`EVO_DIVERSITY_POLICY_VERSION` and
`EVO_BYTE_DIVERSITY_METRIC_VERSION` are both `1`.

A null `problem->genome_distance` requires
`problem->genome_distance_version == 0` and selects the built-in byte metric.
A non-null callback requires a nonzero caller-owned metric version. It receives
two bounded read-only genome views, `genome_size`, and the run context. For
fixed inputs and context it must return a finite normalized distance in
`[0, 1]`, consume no unrecorded entropy, retain no view, and preserve
ownership. A malformed value returns `EVO_ERROR_EVALUATION`.

Only hard-valid evaluated candidates participate. EVO visits all unordered
pairs in lexicographic index order with `left < right`; it does not sample.
The pair count is checked using divide-first `n * (n - 1) / 2` arithmetic.
The built-in metric counts unequal byte positions and divides total
differences by `pair_count * genome_size`. A domain metric adds callback
results in pair order and divides by pair count. Zero or one valid candidate
records zero pairs, work, and diversity and invokes no distance callback.

The all-valid budget check occurs before initializer, validity, fitness,
distance, observer, or stopping callbacks. Diversity consumes no RNG state and
does not alter comparison, selection, operator dispatch, or stopping in
version 0.22.0. Successful evidence is stored with the evaluated population
and copied into statistics without repeating a distance callback.

### Convergence and stagnation policy

`EVO_STOPPING_POLICY_VERSION` is `1`.

The enabled fitness target is reached when the committed stable global-best
`fitness.total >= fitness_target`. The enabled diversity floor is reached when
the latest committed statistics record has
`diversity <= diversity_floor`. Both equality boundaries are inclusive and
both policies may classify generation zero.

Patience establishes generation zero's global-best total as the significant-
best reference without consuming patience. For each committed child, EVO
computes `significant_best + improvement_tolerance` once using `double`
arithmetic. A current global-best total strictly greater than that threshold
replaces the reference and resets the consecutive stagnant-generation count.
Otherwise the count increases once. Equality, an exact global tie, and a
strict improvement no greater than the tolerance do not reset the count.
Because the reference remains the last significant best, multiple small
strict improvements may reset patience after their cumulative global best
crosses the threshold. A count greater than or equal to
`stagnation_patience` selects stagnation.

Stopping reads only the independently owned global winner and the latest
committed schema-4 statistics. It allocates no storage, consumes no RNG,
invokes no new callback, and uses no wall clock, process identity, allocation
address, or unrecorded entropy. Patience counts committed child generations,
while `generation_limit` remains the hard upper bound on those transitions.

Coincident conditions use this exact precedence: promoted all-invalid child,
fitness-target convergence, diversity-floor or patience stagnation,
generation limit, then application-requested stopping. A natural reason
suppresses the application callback. The observer sees the selected reason.
Generation-zero all-invalid remains `EVO_ERROR_NO_VALID_CANDIDATE` before this
classification.

### Internal population storage

Version 0.3.0 added a private, independently verified population-storage
subsystem. It is not part of the installed public API. Version 0.6.0 invokes
that subsystem as the storage boundary for public generation-zero execution.

The internal lifecycle contract is:

1. A population object is zero-initialized before its first construction.
2. Construction rejects an active population without modifying it.
3. Null input, invalid size policy, arithmetic overflow, budget excess, and
   allocation failure leave an inactive population in the empty zero state.
4. Successful construction owns one contiguous, zero-initialized genome slab
   of exactly `population_size * genome_size` bytes.
5. Indexed genome access returns a bounded, non-owning view. Out-of-range
   access returns null, and no view may outlive the population.
6. Destruction applies the population's retained ordinary or secure-erasure
   disposition independently to evaluation records and the genome slab,
   releases each sole owner, and resets every population field to zero. It is
   null-safe and repeatable for initialized objects.

Version 0.28.0 retains exact genome and evaluation byte counts plus policy
version, backend, and enabled state beside every active population. Disabled
destruction makes no scrubbing claim. Enabled destruction follows the secure-
erasure lifecycle below. Every non-owning genome or evaluation view becomes
invalid at release and must not be retained by a consumer callback.

### Deterministic population initialization

Version 0.4.0 defines private RNG algorithm version 1 and a generation-zero
population initializer. The normative algorithm decision is recorded in
`docs/adr/ADR-0002-deterministic-rng-and-population-initialization.md`.

RNG version 1 uses PCG-XSH-RR with 64-bit state, 32-bit output, a fixed odd
stream increment, and explicit least-significant-byte-first output. Unsigned
fixed-width wraparound is intentional. Every `uint64_t` seed, including zero,
is valid. No global RNG state, clock, process identity, platform entropy, or
native byte-order conversion participates in the stream.

The initialization lifecycle is:

1. The population must be active, uninitialized, and structurally consistent
   with the supplied problem and configuration.
2. EVO seeds one operation-local RNG from `config->random_seed`.
3. EVO fills the complete contiguous slab from one continuous stream.
4. If `problem->initialize` is non-null, EVO calls it once per genome in
   ascending index order.
5. Each callback receives deterministic prefilled bytes as a bounded,
   non-owning view. It must be deterministic for fixed bytes and context,
   remain within the genome, preserve ownership, and not retain the view.
6. After all callbacks return, the population records the seed, RNG algorithm
   version, and initialized state.
7. Null arguments return `EVO_ERROR_INVALID_ARGUMENT`. Inactive, previously
   initialized, policy-inconsistent, or metadata-inconsistent populations
   return `EVO_ERROR_STATE` without modification.
8. Population destruction clears the initialization metadata together with
   the owned storage.

The RNG is not cryptographically secure and is not approved for secrets, keys,
nonces, authentication, or adversarial unpredictability.

Version 0.7.0 adds a private unbiased bounded-index operation over the same
version-1 stream. Each candidate sample consumes two 32-bit outputs, treating
the first as the low word and the second as the high word of one explicit
64-bit sample. Rejection sampling discards values below
`(-bound) % bound`; accepted values are reduced modulo the nonzero bound. This
avoids modulo bias and is independent of native byte order. Invalid input
preserves both the RNG state and the output index.

Version 0.8.0 adds a private probability-event operation over the same stream.
A finite probability in `[0, 1]` is converted to the 64-bit threshold
`floor(probability * 2^32)`. One 32-bit output is successful when its unsigned
value is less than that threshold. Every successful call consumes exactly one
word, including probability zero and probability one. Invalid input preserves
the RNG state and output flag. Reproducible evidence must record the exact
configured `double` rate rather than relying on an imprecise display string.

Population initialization does not itself call `is_valid` or `evaluate`.
Version 0.6.0 composes it with the distinct evaluation phase; initialization
failure prevents every later callback and result transfer.

### Private operator seed schedule

Version 0.11.0 adds operator seed-schedule version 1 without changing RNG
algorithm version 1 or generation-zero initialization. The normative decision
is recorded in
`docs/adr/ADR-0010-versioned-operator-substreams-and-parent-pair-planning.md`.

The schedule promotes the exact plain tuple-mixed control measured by
EVO-RNG-001. It derives a PCG state and odd increment from:

```text
(master_seed, source_generation, population_index, operation_domain)
```

The contract is:

1. Every transformation uses modulo-2^64 unsigned fixed-width arithmetic.
2. Stable operation-domain values are selection `2`, crossover `3`, and
   mutation `4`.
3. Selection and crossover use complete-pair ordinals as tuple indexes;
   mutation uses child population indexes.
4. Identical tuples reproduce exact state, increment, and output prefixes.
5. Changing source generation, tuple index, or domain derives an independently
   addressable stream rather than advancing another operation's stream.
6. Null output or an invalid domain returns false and preserves the destination
   RNG object.
7. Schedule version 1 does not add entropy, make a cryptographic claim, or
   modify the generation-zero stream.

Prime-indexed and finite-field elliptic schedules remain research-only and are
not linked into the production library.

### Generation-zero validation and evaluation

Version 0.5.0 adds a private evaluation phase after successful population
initialization. The normative decision is recorded in
`docs/adr/ADR-0004-generation-zero-validation-and-evaluation.md`.

The evaluation lifecycle is:

1. The population must be active, initialized, unevaluated, and structurally
   consistent with the supplied problem and configuration.
2. The evaluator callback is required. A missing evaluator returns
   `EVO_ERROR_INVALID_ARGUMENT`.
3. EVO proves the evaluation-record byte count is representable and no greater
   than `config->max_evaluation_bytes`.
4. If `problem->is_valid` is null, every candidate is valid. Otherwise EVO
   calls it exactly once for every genome in ascending index order.
5. EVO calls `problem->evaluate` exactly once for each valid genome in
   ascending index order. Invalid candidates are never evaluated.
6. Every field in the returned `evo_fitness_t` must be finite, and
   `constraint_penalty` must be non-negative. Non-finite or negative-penalty
   evidence returns `EVO_ERROR_EVALUATION`, releases provisional records, and
   preserves the initialized population as unevaluated.
7. Validity is a hard gate. Shared fitness-comparison policy version 1 selects
   the higher consumer-computed `fitness.total`, with the lower population
   index winning an exact generation-local tie. EVO records but does not
   independently rank or reapply the other fitness components.
8. A completed all-invalid population returns `EVO_SUCCESS`, records zero
   valid candidates, and has no best-candidate index.
9. Repeated evaluation is rejected with `EVO_ERROR_STATE` without modifying
   the completed records.
10. Population destruction releases the evaluation records together with the
    genome slab and resets all lifecycle, count, and winner metadata.

Callbacks receive bounded, non-owning, read-only genome views. They must not
change storage ownership or retain a view. EVO can roll back only its private
provisional records; side effects in consumer context remain consumer-owned.

### Public generation-zero execution

Version 0.6.0 composes the private storage, initialization, and evaluation
phases inside `evo_run`. The normative decision is recorded in
`docs/adr/ADR-0005-generation-zero-public-run-integration.md`.

The public execution lifecycle is:

1. A null result returns `EVO_ERROR_INVALID_ARGUMENT`.
2. A result with a non-null `best_genome` returns
   `EVO_ERROR_RESULT_ACTIVE` before any other input validation or callback and
   preserves the active result unchanged.
3. An inactive result is reset to its empty zero state. A null problem,
   configuration, or evaluator returns `EVO_ERROR_INVALID_ARGUMENT`.
4. EVO constructs one private population under `max_genome_bytes` and
   `max_population_bytes`, initializes it from the recorded seed, and evaluates
   it under `max_evaluation_bytes`.
5. Initialization, validation, and evaluation callbacks retain their
   deterministic ascending-order contracts. Invalid candidates are not
   evaluated.
6. If evaluation completes without a valid candidate, EVO releases all
   private allocations and returns `EVO_ERROR_NO_VALID_CANDIDATE` with an empty
   public result.
7. Otherwise EVO allocates one independently owned result genome, copies
   exactly `problem->genome_size` bytes from the stable best candidate, copies
   all seven fitness fields, records the configured seed, and sets
   `generations_completed` to zero.
8. EVO releases the private evaluation records and population slab before
   returning. The result allocation is the only allocation transferred to the
   caller.
9. Every failure other than active-result rejection releases all private
   allocations and leaves the public result empty.

This version 0.6.0 boundary proves that a valid initialized candidate was
evaluated and transferred. A zero-limit version 0.16.0 call retains that exact
meaning; positive limits invoke the separately specified bounded loop.

### Private versioned deterministic parent selection

Version 0.7.0 adds a private tournament-selection boundary after completed
population evaluation. The normative decision is recorded in
`docs/adr/ADR-0006-deterministic-tournament-selection.md`.

The selection lifecycle is:

1. The configuration, completed population, seeded RNG, and output pointer must
   be non-null.
2. `tournament_size` must be in the inclusive range
   `1..config->population_size`; otherwise selection returns
   `EVO_ERROR_RESOURCE_LIMIT`.
3. Before consuming RNG state, EVO proves that population storage and
   evaluation-record byte counts are exact and representable, configuration
   budgets and initialization evidence match, validity and evaluation flags are
   consistent, every valid fitness field is finite, the valid count is exact,
   and the recorded stable best index is correct.
4. A structurally inconsistent or unevaluated population, or an unseeded RNG,
   returns `EVO_ERROR_STATE`.
5. A consistent all-invalid population returns
   `EVO_ERROR_NO_VALID_CANDIDATE` without consuming RNG state.
6. Each tournament draw uses unbiased bounded sampling over the number of valid
   candidates, then maps the sampled valid ordinal to its ascending population
   index. Invalid candidates are never eligible.
7. Sampling is with replacement. Shared fitness-comparison policy version 1
   selects higher `fitness.total`, and an exact tie selects the lower
   population index.
8. The operator performs no allocation and never changes population storage or
   evaluation evidence. It assigns the output index only after every draw
   succeeds.

The caller supplies an already seeded private RNG stream. Version 0.7.0 does
not derive a selection seed, split initialization streams, or alter RNG
algorithm version 1. Version 0.11.0 separately defines pair-local selection-
stream ownership, and version 0.16.0 invokes that composition from `evo_run`.

Version 0.25.0 retains that tournament lifecycle unchanged as selection-policy
version 1's zero-valued compatibility dispatch and adds rank mode through
ADR-0025. Rank selection follows this lifecycle:

1. Common pointer, active-policy, completed-population, all-invalid, and seeded-
   RNG rules match tournament selection.
2. EVO resolves every hard-valid candidate's unique rank through fitness-
   comparison policy version 1. Exact total ties place the lower population
   index first; invalid candidates are excluded.
3. Rank `r` among `n` valid candidates receives exact weight
   `base + (n - 1 - r) * step`.
4. Before RNG consumption, checked arithmetic proves every weight, the
   observed participant count, and the exact total
   `n * base + n * (n - 1) / 2 * step`.
5. One existing unbiased bounded-index sample draws a ticket in
   `[0, total)`. EVO visits valid candidates in ascending population-index
   order and assigns the ticket to the corresponding rank-weight interval.
6. The operation allocates no memory and assigns the output only after a
   candidate interval is resolved.

The dispatch consumes the caller-supplied selection stream only. It neither
derives a new domain nor changes crossover or mutation stream identity.

### Private deterministic crossover dispatch

Version 0.8.0 adds a private representation-neutral crossover boundary.
Version 0.26.0 adds explicit reference byte modes without replacing that
consumer boundary. The normative decisions are recorded in ADR-0007 and
ADR-0027.

The crossover lifecycle is:

1. The problem, configuration, seeded RNG, two parent views, and two child
   views must be non-null.
2. `problem->genome_size` must be nonzero and no greater than
   `config->max_genome_bytes`.
3. `config->crossover_rate` must be finite and in the inclusive range
   `[0, 1]`.
4. The complete child spans must not overlap one another or either complete
   parent span. Parent spans are read-only and may coincide with one another.
5. Every preceding check occurs before RNG consumption or child output. Null
   and child-span overlap errors return `EVO_ERROR_INVALID_ARGUMENT`; invalid
   genome policy or rate returns `EVO_ERROR_RESOURCE_LIMIT`; unseeded state returns
   `EVO_ERROR_STATE`.
6. Every successful pair consumes exactly one probability-event word,
   including rate endpoints and every selector.
7. When the event is not selected, EVO clones parent A to child A and parent B
   to child B for exactly `genome_size` bytes and consumes no mode-specific
   draw.
8. In consumer mode, a selected event invokes `problem->crossover` exactly once
   when non-null; a null callback selects the same corresponding-parent clone.
9. In an explicit byte mode, a selected event bypasses the callback and writes
   both complete children through the selected reference algorithm.
10. The callback must fully initialize both children, preserve parent bytes and
   ownership, retain no view, and remain deterministic for fixed parents and
   context. The callback returns no status, so EVO cannot roll back a consumer
   contract violation.

Selected byte modes consume and write in this exact form:

- one-point with `n > 1` draws one unbiased value with bound `n - 1`, adds one
  to form `cut`, and swaps `[cut, n)`; `n == 1` clones with no cut draw;
- two-point draws one boundary with bound `n + 1`, then one rank with bound
  `n`, maps around the first boundary, sorts both, and swaps the nonempty
  half-open range `[lower, upper)`; `n == SIZE_MAX` is rejected because
  `n + 1` is unrepresentable; and
- uniform consumes one 32-bit word per group of up to 32 ascending byte
  offsets, using the least-significant available bit to retain or swap
  corresponding parents and discarding unused final high bits.

Every bounded sample uses RNG algorithm version 1's two-word unbiased rejection
schedule. Both children are complementary and fully initialized, including
equal-parent and endpoint cases.

The operator performs no allocation, does not select parents, and does not own
child storage. Version 0.11.0 defines pair-local crossover stream derivation,
and version 0.16.0 invokes the complete composition from `evo_run`.

### Private deterministic mutation dispatch

Version 0.9.0 adds a private representation-neutral mutation boundary. Version
0.26.0 adds an explicit reference byte mode without replacing that consumer
boundary. The normative decisions are recorded in ADR-0008 and ADR-0027.

The mutation lifecycle is:

1. The problem, configuration, seeded RNG, and writable genome view must be
   non-null.
2. `problem->genome_size` must be nonzero and no greater than
   `config->max_genome_bytes`.
3. `config->mutation_rate` must be finite and in the inclusive range
   `[0, 1]`.
4. Every preceding check occurs before RNG consumption or genome output. Null
   errors return `EVO_ERROR_INVALID_ARGUMENT`; invalid genome policy or rate
   returns `EVO_ERROR_RESOURCE_LIMIT`; unseeded state returns
   `EVO_ERROR_STATE`.
5. Every valid attempt consumes exactly one probability-event word, including
   rate endpoints and every selector.
6. When the event is not selected, EVO leaves the genome unchanged and consumes
   no mode-specific draw.
7. In consumer mode, a selected event invokes `problem->mutate` exactly once
   when non-null; a null callback leaves the genome unchanged.
8. In byte-XOR mode, a selected event bypasses the callback, draws one unbiased
   byte index with bound `genome_size`, draws one unbiased mask rank with bound
   `255`, adds one, and XORs exactly the selected byte with that nonzero mask.
   The two bounded draws retain the existing two-word rejection schedule.
9. EVO owns `mutation_rate` as the per-genome event probability and forwards
   the same value unchanged as the callback's representation-specific
   mutation intensity.
10. The callback must mutate only the complete supplied span, preserve
   ownership, retain no view, use no unrecorded entropy, and remain
   deterministic for fixed genome bytes, rate, and context. The callback
   returns no status, so EVO cannot roll back a consumer contract violation.

The selected byte-XOR event changes exactly one in-bounds byte, including for a
one-byte genome. The operator performs no allocation and does not own genome
storage. It does not adapt the rate locally or define typed bit, integer,
floating-point, or permutation mutation. Version 0.11.0 defines child-indexed
mutation stream derivation, version 0.16.0 invokes the complete composition
from `evo_run`, and version 0.27.0 supplies the committed effective rate before
this helper is called.

### Private child-population ownership

Version 0.10.0 adds a private child-population storage boundary. The normative
decision is recorded in
`docs/adr/ADR-0009-bounded-child-population-ownership.md`.

The child-storage lifecycle is:

1. The problem, configuration, completed parent population, and empty child
   population must be distinct, non-null objects.
2. An active child is rejected with `EVO_ERROR_INVALID_ARGUMENT` and preserved
   unchanged. Parent/child object aliasing is rejected the same way.
3. EVO validates the parent's dimensions, allocation sizes, budgets,
   initialization seed, RNG version, evaluations, finite fitness evidence,
   valid count, best-candidate state, and stable tie handling through the same
   private validator used by parent selection.
4. An incomplete or inconsistent parent, or a parent genome size that differs
   from the supplied problem, returns `EVO_ERROR_STATE` before allocation.
5. EVO proves `config->population_size * problem->genome_size` is
   representable and enforces both `max_genome_bytes` and
   `max_child_population_bytes`.
6. Zero or insufficient child policy returns `EVO_ERROR_RESOURCE_LIMIT`; an
   allocator failure returns `EVO_ERROR_OUT_OF_MEMORY`. Either failure leaves
   the child empty and parent unchanged.
7. Success creates one distinct, zero-initialized contiguous child genome slab
   with dimensions matching the parent.
8. The child has no evaluation allocation, initialization seed, RNG algorithm
   version, validity count, best-candidate evidence, committed production
   count, source generation, operator schedule version, or completed lifecycle
   flag.
9. Parent and child objects own separate allocations and may be destroyed in
   either order. Destruction remains null-safe, repeatable, and fully
   resetting.
10. A structurally complete all-invalid parent may create child storage. The
    later selection/orchestration layer owns the no-valid-candidate decision.

The child operation does not itself select or pair parents, invoke crossover
or mutation, mark genomes complete, evaluate children, swap populations,
derive operator streams, or increment a generation. Version 0.16.0 invokes it
from the bounded `evo_run` composition.

### Private complete-parent-pair planning

Version 0.11.0 adds a private read-only pair-planning boundary over a completed
parent population. The normative decision is recorded in ADR-0010.

The planning lifecycle is:

1. Configuration, completed parent population, and output plan must be
   non-null.
2. Population size and the active selection-policy fields must be valid.
   Tournament mode requires size in `[1, population_size]`; rank mode requires
   canonical rank fields and representable configured weight arithmetic.
   Rejection returns `EVO_ERROR_RESOURCE_LIMIT`.
3. Through 0.23.0 exactly `floor(population_size / 2)` complete pair ordinals
   were valid. Version 0.24.0 resolves elite policy first; exactly
   `floor(ordinary_offspring / 2)` pair ordinals are valid. An out-of-range
   ordinal returns `EVO_ERROR_RESOURCE_LIMIT` and preserves the output.
4. EVO proves the completed parent storage and evaluation evidence through the
   shared population validator. Inconsistent evidence returns
   `EVO_ERROR_STATE`; a consistent all-invalid parent returns
   `EVO_ERROR_NO_VALID_CANDIDATE`.
5. Pair ordinal `i` maps to child indexes `2i` and `2i + 1`. These arithmetic
   results are in range by construction.
6. EVO derives one selection-domain schedule-version-1 stream from the
   configured master seed, source generation, and pair ordinal.
7. Two deterministic configured-policy draws execute sequentially on the pair-
   local stream. Tournament sampling remains with replacement, and either mode
   may identify the same parent twice.
8. The plan records both parent indexes, both child indexes, pair ordinal,
   source generation, seed-schedule version, selection-policy version, and
   selection enum only after both draws succeed.
9. Every rejection preserves the output object and the complete parent
   population.
10. If the ordinary-offspring prefix is odd, its final index is not part of a
    complete pair and singleton policy version 1 owns it.

The planner receives no child pointer, writes no genome, invokes no crossover
or mutation callback, and marks no lifecycle state. Version 0.16.0 invokes it
through complete-pair production from `evo_run`.

### Private deterministic complete-pair child production

Version 0.12.0 adds a private composition boundary. The normative decision is
recorded in
`docs/adr/ADR-0011-deterministic-complete-pair-child-production.md`.

The production lifecycle is:

1. Problem, configuration, completed parents, active child storage, and output
   evidence must be non-null. Parent and child objects and genome slabs must be
   distinct.
2. Genome dimensions, child allocation size, child budget, selection policy,
   crossover and mutation selectors, crossover rate, and mutation rate must be
   internally consistent.
3. Child evaluation, generation-zero initialization, validity, fitness, and
   best-candidate evidence must remain empty.
4. The child records a contiguous `produced_count`. Pair `i` is accepted only
   when `produced_count == 2i`; repeated or skipped pairs return
   `EVO_ERROR_STATE` unchanged.
5. The first successful pair records the supplied source generation, operator
   seed-schedule version 1, selection-policy version 1 and enum, byte-operator
   policy version 1, and both active operator enums. Every later pair must match
   that provenance.
6. EVO invokes the complete parent-pair planner and preserves the configured
   policy's valid-only semantics.
7. EVO derives the crossover-domain stream from the pair ordinal and derives
   one mutation-domain stream from each child index.
8. EVO resolves the two parent and two child views and completes every
   expected fallible library check before callback dispatch or child output.
9. The valid suffix invokes the crossover dispatcher once, then the mutation
   dispatcher once for each child. Given the completed preflight and seeded
   streams, this suffix contains no expected library rejection.
10. Success commits both child genomes, advances `produced_count` by exactly
    two, records source generation plus schedule, selection, and byte-operator
    provenance, and commits output evidence with RNG algorithm version 1.
11. Rejection before callback dispatch preserves the child bytes, child
    metadata, output evidence, and complete parent population.
12. Consumer callbacks retain their existing bounded deterministic contract
    and no failure channel. EVO cannot roll back consumer-context side effects
    or recover from a callback contract violation.
13. Production stops after the last complete pair and leaves any ordinary
    singleton plus the elite suffix untouched.

Production metadata is not completed-population evidence. The child remains
uninitialized, unevaluated, and ineligible for selection. Singleton and elite
completion, child evaluation, population swapping, and generation advancement
are separate boundaries.

### Private deterministic elite preservation

Version 0.13.0 adds the odd-population stable-best tail recorded by ADR-0012.
Version 0.24.0 generalizes it through ADR-0024 while retaining that exact rule
as the disabled-config compatibility subset of elite policy version 1.

The generalized lifecycle is:

1. Problem, configuration, completed parents, active child storage, and output
   evidence must be non-null and every typed object and owned byte range must be
   independent.
2. Population and genome dimensions must match; checked storage size and the
   genome and child-slab budgets must authorize the existing owners.
3. Disabled mode requires a zero payload. Enabled mode accepts every requested
   count through population size. Invalid configuration returns
   `EVO_ERROR_RESOURCE_LIMIT` before child output.
4. Parent evaluation evidence must be structurally complete and contain at
   least one hard-valid rankable candidate.
5. EVO computes requested, effective, and ordinary-offspring counts. Effective
   count is capped at source valid count and therefore names distinct sources.
6. Child initialization, evaluation, validity, fitness, best-candidate, and
   completed elite evidence must remain empty.
7. Exactly `floor(ordinary_offspring / 2)` complete pairs must form the
   contiguous prefix with matching source generation and operator schedule.
8. If `ordinary_offspring` is odd, singleton policy version 1 derives the
   selection stream at index `floor(ordinary_offspring / 2)`, selects one valid
   parent through the configured selection dispatch, clones it into
   `ordinary_offspring - 1`, and invokes the standard mutation dispatcher with
   that child index's mutation stream.
   It invokes no crossover and allocates no scratch storage.
9. Before elite output, EVO performs a complete stable ranking dry pass through
   fitness-comparison policy version 1 and resolves bounded source and
   destination views.
10. EVO copies exactly `genome_size` bytes for each effective elite into suffix
    indexes `ordinary_offspring + rank`, best-to-worst. Ranking and copies
    consume no RNG and invoke no callback.
11. Success records full produced count, source generation, operator schedule,
    selection-policy version 1 and enum, byte-operator policy version 1 and both
    operator enums, effective elite count, source valid count, elite policy
    version 1, singleton policy version when used, explicit-versus-compatibility
    mode, and complete output evidence.
12. Only disabled odd compatibility records odd-tail policy version 1. Disabled
    even and every explicit configuration record no odd-tail marker.
13. Every detectable rejection before consumer singleton dispatch preserves
    parent, child, and output evidence. Elite completion has a fully preflighted
    no-expected-failure copy suffix. Consumer mutation side effects retain the
    established no-rollback contract after dispatch.

Full production metadata alone does not make the child initialized, evaluated,
or selectable. Version 0.14.0 evaluation remains the next distinct lifecycle
boundary.

### Private deterministic produced-child evaluation

Version 0.14.0 adds a private child-evaluation boundary. The normative
decision is recorded in
`docs/adr/ADR-0013-deterministic-produced-child-evaluation.md`.

The child-evaluation lifecycle is:

1. Problem, configuration, fully produced child storage, and caller-owned
   output evidence must be non-null, and an evaluator must be present.
2. Genome and population dimensions, per-genome policy, child-slab budget,
   exact checked storage size, and evaluation budget must be consistent.
3. The child must record `produced_count == population_size`, the supplied
   source generation, operator seed-schedule version 1, selection-policy
   version 1 and its matching enum, plus byte-operator policy version 1 and both
   operator enums matching the active configuration.
4. The child must record elite policy version 1, effective elite count, source
   valid count, explicit-versus-compatibility mode, and singleton policy
   version exactly when the ordinary prefix is odd. Only disabled odd
   compatibility records odd-tail policy version 1.
5. Generation-zero initialization evidence, RNG initialization evidence,
   evaluation records, valid count, and best-candidate evidence must remain
   empty before evaluation.
6. EVO allocates zero-initialized provisional candidate records within
   `max_evaluation_bytes`. Allocation failure leaves the child unchanged.
7. The optional validator visits every candidate in ascending index order. A
   missing validator means every candidate is valid.
8. Only valid candidates are passed to the evaluator, also in ascending index
   order. Invalid candidates retain zero fitness and an unevaluated record.
9. All seven returned fitness fields must satisfy fitness-comparison policy
   version 1. A non-finite value or negative penalty releases every provisional
   record and preserves the child object and output evidence, although
   callback-context side effects cannot be rolled back.
10. Shared comparison selects higher `fitness.total`. Exact ties retain the
    lower candidate index.
11. Success commits the complete record allocation, valid count, stable best
    evidence, and evaluated state while preserving every genome byte and all
    production provenance. An all-invalid child completes with no best.
12. Output evidence records population and evaluation sizes, valid count,
    stable best, source generation, production-policy versions, fitness-
    comparison policy version 1, child-evaluation policy version 6, and
    completion.
13. Repeated evaluation and every malformed, incomplete, mismatched, resource,
    allocation, or detectable callback-output failure preserve caller-owned
    evidence and library-owned child state.

The operation consumes no RNG word and invokes no initialization, selection,
crossover, or mutation callback. The shared completed-population validator
accepts either generation-zero or evaluated-child provenance. An evaluated
child can therefore authorize parent selection and the next independently
owned child allocation without first changing object ownership.

This boundary does not itself swap parent and child objects, increment a
generation, destroy or recycle the prior parent, or implement termination
policy. Version 0.16.0 invokes it from public `evo_run`.

### Private atomic generation advancement

Version 0.15.0 adds a private generation-advancement boundary. The normative
decision is recorded in
`docs/adr/ADR-0014-atomic-generation-advancement.md`.

The generation-advancement lifecycle is:

1. Problem, configuration, current parent, evaluated child, and caller-owned
   output evidence must be non-null, distinct typed objects.
2. Evidence storage must not overlap either population object or any genome or
   evaluation allocation owned by either population.
3. `current_generation == UINT64_MAX` returns
   `EVO_ERROR_RESOURCE_LIMIT`; no wrapped or saturated generation is emitted.
4. Problem and configured dimensions must be nonzero and within the caller's
   per-genome policy.
5. Both population objects must pass the shared completed-population validator
   and match the supplied problem genome size.
6. Genome and evaluation allocations within and across the two populations
   must be pairwise disjoint. An overlap is malformed ownership and returns
   `EVO_ERROR_STATE` before destruction can occur.
7. An initialized generation-zero parent is accepted only when
   `current_generation == 0`.
8. A parent that originated as a produced child is accepted only when the
   current generation is nonzero and its source generation equals
   `current_generation - 1`.
9. The incoming population must be a produced, evaluated child whose source
   generation equals `current_generation`.
10. Output evidence is prepared with population size, valid count, stable-best
    state, previous generation, completed generation, elite count and source-
    valid count, production-policy versions including selection and byte-
    operator policy, fitness-comparison policy version 1, and generation-
    advancement policy version 6.
11. After all fallible checks, EVO moves the child structure into the parent
    handle, resets the child handle to the complete zero state, releases the
    former parent allocations, and commits evidence.

The commit suffix performs no allocation and copies no genome or evaluation
bytes. It consumes no RNG state and invokes no callback. Rejection preserves
both populations and output evidence. Success preserves the incoming child's
allocation identities, complete bytes, validity, fitness, stable best, and
production provenance while transferring exclusive ownership to the parent
handle.

An all-invalid evaluated child is a structurally completed population and is
therefore promotable. A later termination boundary decides whether the run
stops. Generation-limit enforcement, convergence and stagnation, old-parent
recycling, checkpoint persistence, and public `evo_run` iteration are not part
of this operation. Generalized elite provenance is preserved by the move but
does not alter ownership semantics.

Atomicity here is a library-state contract: every rejection precedes mutation,
and the remaining commit suffix has no expected failure. It does not make the
population handles safe for concurrent unsynchronized access.

### Public bounded multi-generation execution

Version 0.16.0 composes the private generation pipeline in `evo_run`. The
normative decision is recorded in
`docs/adr/ADR-0015-bounded-public-multigeneration-run.md`.

The bounded-run lifecycle is:

1. `generation_limit` is the number of completed child transitions after
   generation zero. Zero retains the version 0.6.0 generation-zero behavior.
2. For a positive limit, EVO validates the child slab budget and every
   transition policy before allocation or callback dispatch. Populations above
   one require valid configured selection fields and finite crossover and
   mutation rates in `[0, 1]` plus canonical applicable adaptation policy.
   Every positive-limit run requires defined
   crossover and mutation selectors. A one-member compatibility population or
   explicit one-elite population uses elite cloning directly and does not
   validate unused operator rates. A one-member explicit-zero population uses
   one singleton, requires valid selection and mutation policy, and does not
   validate the unused crossover rate.
3. EVO constructs, initializes, and evaluates generation zero and transfers its
   stable valid winner into one independent result allocation.
4. For each source generation in ascending order, EVO creates one child slab,
   resolves elite counts, produces complete pairs, an optional ordinary
   singleton, and the stable elite suffix, evaluates the complete child, and
   resolves strict global-best improvement.
5. EVO atomically promotes the evaluated child before changing the result.
   Promotion increments the completed-transition count and releases the former
   parent.
6. EVO classifies strict global-best improvement and commits the next adaptive
   mutation rate plus its schema-4 audit projection. The rate that produced the
   child remains explicit as `mutation_rate_prior`.
7. Shared comparison policy version 1 lets a later candidate replace the
   existing result bytes and complete fitness only when its `fitness.total` is
   strictly greater. Exact cross-generation ties retain the earlier winner.
8. A promoted all-invalid child terminates the loop successfully and retains
   the earlier valid winner. Its promotion is included in
   `generations_completed`.
9. After each commit, enabled target, patience, and diversity-floor policy
   classifies only the committed winner and statistics. Natural reasons use
   the fixed all-invalid, converged, stagnated, then generation-limit order.
10. After any still-nonterminal committed generation, an optional application
   stop decision may terminate successfully while preserving that exact
   committed winner, statistics, and completed-transition count.
11. Any other failure destroys every current internal owner and the result
   allocation, returning the inactive result to its complete zero state. No
   partial public progress is retained.

The result allocation is created once and is not reallocated during a run.
Bounded-run policy evidence is private. Version 0.17.0 publishes its successful
stop classification, version 0.18.0 retains one constant-space statistics
record for the most recently committed generation, and version 0.19.0 can
deliver each committed record synchronously without retaining history.
Version 0.20.0 can stop on an application decision after a committed
generation. Version 0.21.0 advances bounded-run policy to version 3 and records
fitness-comparison policy version 1 plus the winning generation and population
index. Version 0.22.0 advances it to version 4 and records diversity policy and
metric provenance without changing operator RNG or selection. Version 0.23.0
advances it to version 5 and records stopping policy version 1, the
significant-best reference, stagnant generation count, and distinct final
classification flags. Version 0.24.0 advances it to version 6 and records final
elite count, source valid count, elite and singleton policy versions, and
explicit-versus-compatibility mode. Version 0.25.0 advances it to version 7 and
records selection-policy version 1 and the configured enum. Child-evaluation
and generation-advancement policies advance to version 5 for the same
provenance. Version 0.26.0 advances it to version 8 and records byte-operator
policy version 1 plus the configured crossover and mutation enums. Child-
evaluation and generation-advancement policies advance to version 6 for the
same provenance. Version 0.27.0 advances bounded-run policy to version 9 and
child-evaluation and generation-advancement policies to version 7. They record
the effective mutation rate through production, evaluation, promotion, and
final run evidence. Version 0.28.0 composes its existing owner
cleanup with the separate secure-erasure policy without changing run decisions
or RNG. Version 0.29.0 advances bounded-run policy to version 10, factors the
complete committed continuation state into run-state schema 1, and passes both
fresh and restored state through the same remaining-generation loop. The
bounded run still does not define old-slab recycling or parallelism.

Version 0.30.0 advances bounded-run policy to version 11, child-evaluation and
generation-advancement policies to version 8, and private run-state schema to
version 2. Enabled recycling carries policy version 1 and stable active and
reusable owner identities through child evaluation, atomic promotion, registry
observation, checkpoint capture, and resume. Disabled execution preserves the
complete version-10 allocation and RNG path.

Version 0.31.0 advances bounded-run policy to version 12 and child-evaluation
and generation-advancement policies to version 9. They carry parallel-
evaluation policy version 1 and the exact configured worker count through
completed populations, evaluation, promotion, checkpoint/resume, and final run
evidence. The zero-worker serial path records zero parallel provenance.

### Result lifecycle

The caller must zero-initialize `evo_result_t` before its first use:

```c
evo_result_t result = {0};
```

The lifecycle contract is:

1. `evo_run` rejects a result whose `best_genome` is non-null and preserves the
   active result unchanged.
2. Null input, invalid resource policy, allocation failure, evaluation
   failure, and completion without a valid candidate leave a non-null,
   inactive result in the empty zero state.
3. On success, the result exclusively owns an independent copy of the highest-
   total valid genome observed through all completed transitions.
4. On success, `termination_reason` is
   `EVO_TERMINATION_GENERATION_LIMIT` when the configured transition bound
   completed or `EVO_TERMINATION_ALL_INVALID` when a promoted later child had
   no valid candidate, `EVO_TERMINATION_CONVERGED` when a fitness target was
   reached, `EVO_TERMINATION_STAGNATED` when patience or a diversity floor was
   reached, or `EVO_TERMINATION_APPLICATION_REQUESTED` when the application
   stop callback selected a committed generation.
   `EVO_TERMINATION_NONE` is never a successful reason.
5. On success, `generation_statistics` describes the most recently committed
   population. Generation zero and every promoted child replace the record in
   constant space; no history allocation scales with `generation_limit`.
6. An optional observer receives a read-only snapshot after generation zero
   and after every successfully promoted child. Failed and provisional
   generations do not produce events.
7. An optional application stop callback receives a read-only snapshot only
   after a committed generation from which execution could continue. A true
   decision stops successfully without changing that committed state.
8. Callers may use bounded, non-owning aliases to read or write genome bytes
   while the result remains alive. An alias may not free or reallocate the
   storage and must not survive result destruction.
9. A live result retains `best_genome_size`, secure-erasure policy version 1,
   the enabled flag, and either backend `NONE` or the selected enabled backend
   beside its sole owner.
10. `evo_result_destroy` applies that retained disposition, releases the owned
   allocation, and resets every result field to zero. Destruction is null-safe
   and repeatable for initialized result objects.
11. A destroyed result may be passed to `evo_run` again immediately.

With the default disabled policy, `evo_result_destroy` does not securely erase
genome bytes and makes no such claim. Consumers that enable policy version 1
receive the bounded process-memory erasure defined below, but remain
responsible for their own copies, aliases, callback state, swap, crash
artifacts, allocator behavior, and platform-level data-remanence policy.

### Opt-in secure-erasure lifecycle

`EVO_SECURE_ERASURE_POLICY_VERSION` is `1`.
`evo_secure_erasure_backend_t` defines the zero-valued
`EVO_SECURE_ERASURE_BACKEND_NONE`, detected
`EVO_SECURE_ERASURE_BACKEND_EXPLICIT_BZERO`, and reviewed fallback
`EVO_SECURE_ERASURE_BACKEND_VOLATILE_BYTES` values.

The stable logical owner registry is:

| Owner | Exact count | Terminal disposition |
|---|---:|---|
| Public result genome | `evo_result_t.best_genome_size` | `evo_result_destroy` |
| Population genome slab | `evo_population_t.storage_bytes` | population destruction |
| Population evaluations | `evo_population_t.evaluation_bytes` | rollback or population destruction |
| Provisional evaluations | checked local `evaluation_bytes` | provisional discard |

Successful construction records policy version 1 with each active owner.
Disabled owners record backend `NONE`; enabled owners record the build-selected
backend. A child owner receives the active configuration policy, and atomic
generation advancement moves the pointers, exact counts, and policy metadata
together before disposing of the former parent.

For disabled owners, terminal cleanup invokes no secure-erasure primitive,
uses ordinary release, and makes no claim that the prior bytes were scrubbed.
For enabled canonical owners, cleanup invokes the sole erasure wrapper exactly
once over the complete retained range, completes that erase before the sole
release, and then zeros the owner record. Provisional evaluation failure and
post-attachment diversity rollback follow the same rule. No erasure path
allocates, invokes a consumer callback, or consumes RNG state.

When population recycling is enabled, the same wrapper also erases each
complete former-active genome and evaluation range before that owner becomes
reusable. Those reset-time erasures are recorded separately from terminal
release. A long-lived slot may therefore have multiple complete reset erasures
and one final-release erasure; exact-once continues to apply to each individual
reset or release event, not to the allocation's entire multi-generation life.

CMake and GNU Autotools independently detect `explicit_bzero` and define the
same internal capability macro. When detected, the wrapper invokes that
supported primitive. Otherwise it writes zero through a volatile-byte loop in
ascending range order. Generic `memset` is not an erasure backend. Backend
selection may vary by supported platform but cannot affect evolutionary
decisions or replay streams.

The guarantee covers only the exact live process allocation owned by EVO. It
does not cover consumer copies, aliases after destruction, allocator metadata,
paging or crash artifacts, device caches, or persistent media. Callers must
not modify the result's owner pointer, byte count, policy version, backend, or
enabled flag. ADR-0029 defines the complete boundary and audit registry.

### Opt-in deterministic population-storage recycling

`EVO_POPULATION_RECYCLING_POLICY_VERSION` and
`EVO_POPULATION_STORAGE_REGISTRY_VERSION` are `1`.
`EVO_POPULATION_STORAGE_OWNER_SLOTS` is `2`.

`evo_config_t.population_recycling_enabled` is the zero-valued compatibility
control. When false, every transition executes the complete pre-0.30.0 child
allocation, evaluation, promotion, and former-parent release path. A configured
storage observer does not implicitly enable recycling.

When true, one run has exactly two logical slot identities. Slot 1 contains
generation zero. The first attempted transition materializes slot 2's genome
owner and one detached zeroed evaluation reserve. Later transitions alternate
the slots' active and reusable roles by committed-generation parity. Slots are
local to one run, are never shared, and are never selected by an address,
allocator behavior, hash, clock, process identity, or random input.

Both slots have the same checked genome capacity
`population_size * genome_size` and evaluation capacity
`population_size * sizeof(evo_candidate_evaluation_t)`. Existing population,
child-population, and evaluation budgets authorize those exact ranges; no new
aggregate budget exists. A successful positive-length enabled run performs
five allocation classes regardless of transition count: initial genomes,
initial evaluations, result genome, second-slot genomes, and second-slot
evaluations. A zero-length run retains the three generation-zero allocations.

Before an enabled promotion changes state, EVO validates both completed
populations, current and next registries, every pointer/count range, generation
and production provenance, reset eligibility, and independence among both
population objects, both owned ranges, the registry, and caller-owned evidence.
Rejection preserves every object and byte. The remaining no-fail phase:

1. moves evaluated child owners into the active population;
2. moves former active owners into the reusable population;
3. resets the complete former evaluation and genome ranges;
4. clears all reusable candidate, production, statistics, and generation
   evidence while retaining only capacities, stable owner identity,
   erasure/reset metadata, and the detached evaluation reserve;
5. commits the next registry; and
6. commits generation-advancement evidence.

Disabled reset uses an ascending full-range zero-byte operation and records
`EVO_POPULATION_STORAGE_RESET_ZERO_BYTES`; this makes no secure-erasure claim.
Enabled secure erasure records
`EVO_POPULATION_STORAGE_RESET_SECURE_ERASE`. If evaluation or diversity fails
after taking a recycled reserve, EVO resets the complete provisional range and
returns it to the same reusable owner. It does not allocate or release that
reserve during rollback. Enclosing failure still destroys every acquired owner
under its retained cleanup policy and leaves the public result empty.

`evo_population_storage_registry_t` is the complete address-free audit
projection. The disabled canonical registry has zero entries and zero active
and reusable identities. Enabled generation zero has one ordered active entry;
every later committed generation has one active and one reusable entry ordered
by owner identity. `evo_population_storage_entry_t` exposes identity,
`EMPTY`/`ACTIVE`/`REUSABLE` lifecycle, population and source generations,
genome and evaluation capacities, handoff and reset counts, reset-time erasure
counts, last reset disposition, and explicit owner-presence flags. The registry
also exposes policy versions and secure-erasure disposition.

The registry is evidence, never a second ownership table. Exact private
pointer/count fields remain authority. Every registry is reconciled against
those fields, configuration, secure-erasure metadata, and committed generation
before it can be used, delivered, or serialized. Malformed, stale, aliased, or
unreconcilable evidence fails closed; no fallback guesses ownership.

When non-null, `population_storage_observer` receives a fresh synchronous
borrowed registry for every committed generation, including generation zero.
Delivery follows the generation observer and any application-stop decision and
precedes checkpoint delivery. The observer cannot stop or modify EVO, receives
no address-bearing owner state, owns no projection storage, and must not retain
the borrowed registry address.

Recycling consumes no RNG word and invokes no additional problem callback.
For identical problem, seed, and algorithmic configuration, enabled and
disabled runs have identical initialization, validity, evaluation, selection,
crossover, mutation, stopping, and generation-observer traces; committed
genomes and fitness; statistics and adaptive/stopping evidence; result winner;
generation count; termination; and algorithm-visible RNG state. Only allocator
events, release timing, reset work, recycler evidence, and recycling-bound
checkpoint fields may differ. ADR-0031 defines the complete policy and
EVO-HRA-003 retains its Human-Readable Abstraction audit.

### Deterministic bounded parallel evaluation

`EVO_PARALLEL_EVALUATION_POLICY_VERSION` and
`EVO_EVALUATION_SCHEDULE_VERSION` are `1`.

`evo_problem_t.evaluation_callback_thread_safety` accepts only
`EVO_EVALUATION_CALLBACK_SERIAL` or
`EVO_EVALUATION_CALLBACK_THREAD_SAFE`. The zero value is serial-only. A
positive `evaluation_worker_count` requires the thread-safe declaration and is
at most `population_size`. The consumer guarantees that concurrent `evaluate`
calls over independent read-only genome ranges may safely share its context.
No other consumer callback is made concurrent by this policy.

Zero workers require zero `max_evaluation_worker_scratch_bytes` and a null
schedule observer. They execute the complete pre-0.31.0 serial path, including
allocation count, callback order, first malformed-fitness failure, and policy
provenance. A positive worker count requires a scratch budget no smaller than
`evo_evaluation_worker_scratch_size(population_size, worker_count)`. That query
performs checked size/alignment arithmetic but no allocation, callback, or
thread operation.

For positive worker count `W`, EVO creates exactly `W` run-local POSIX workers.
The caller thread coordinates and never acts as a hidden evaluator. Validity is
resolved serially for every candidate in ascending population order before the
first evaluation wave. Candidate `i` is assigned stable one-based logical
worker `i % W + 1` and zero-based wave `i / W`. Hard-invalid candidates retain
that projected assignment but never invoke `evaluate`.

An atomic release/acquire epoch dispatches one wave and joins its observable
results before a later wave begins. At most `W` evaluator callbacks are active.
Worker completion order inside a wave is neither recorded as authority nor used
by EVO. The coordinator validates a complete wave in ascending candidate order.
After all waves and all worker joins succeed, valid records commit in ascending
candidate order, receive ascending commit ordinals, and undergo the ordinary
stable-best reduction. The scheduler consumes no RNG and changes no genome,
selection, crossover, mutation, adaptive, or stopping stream.

A worker-start failure occurs while all successfully started workers still
wait at epoch zero. EVO stops and joins them before validity or evaluation
callbacks. A non-finite fitness lets the already dispatched wave finish, marks
every failing row, selects the lowest failure index, cancels every later pending
row, joins every worker, and commits no record. A join failure likewise commits
nothing, retains scratch until termination, and makes one recovery join attempt
before detaching a terminated handle whose recovery also fails. Cancellation
suppresses future waves; EVO does not asynchronously unwind arbitrary C
callbacks already running. Evaluation allocation, worker scratch allocation,
start/join, fitness, or later diversity failure publishes no population
generation and the public run retains no partial result.

One `calloc` contains exactly the private worker records, checked alignment
padding, and one public assignment record per population candidate. The range
is zeroed and released only after all workers terminate and the synchronous
schedule observer returns. EVO creates no dynamic queue node, global executor,
spare or nested worker, cross-run pool, or retained thread.

The schedule observer receives a complete borrowed
`evo_evaluation_schedule_t` after every positive-worker attempt, including
failed attempts. It exposes policy/view versions, population generation and
size, worker count, exact library scratch bytes, aggregate validation and
disposition counts, stable first-failure evidence, final outcome, and the
candidate-ordered assignment array. Rows expose population index, logical
worker, wave, disposition, commit ordinal, and explicit commit presence. The
observer cannot stop or modify execution and must not retain the borrowed
array. Failure schedules are diagnostic and always commit zero records.
ADR-0032 and EVO-HRA-004 define the complete authority and projection contract.

### Versioned checkpoint and deterministic resume

`EVO_CHECKPOINT_FORMAT_VERSION`, `EVO_CHECKPOINT_VIEW_VERSION`,
and `EVO_CHECKPOINT_CONFIGURATION_VIEW_VERSION` are `3`.
`EVO_CHECKPOINT_CANDIDATE_VIEW_VERSION` remains `1`.
`EVO_CHECKPOINT_INTEGRITY_CRC32` identifies the format-3 corruption check.

`evo_checkpoint_size` computes the exact bounded size required by the supplied
problem and configuration. It performs no allocation or callback work.
Checkpoint-enabled execution writes only to the caller's declared buffer and
emits one synchronous checkpoint view after each successfully committed
generation, including generation zero. Delivery follows natural stop
classification, the optional application stop decision, and the generation
observer. A provisional or failed generation emits nothing.

Format 3 is canonical little-endian data with magic `EVOCKPT3`, one fixed
header, and then configuration, continuation state, latest statistics,
explicit evaluation records, the current population genome slab, and the
independently owned
global-best genome in that order. It contains no native structure image,
padding, pointer, function address, context address, allocator address, clock,
process identity, or unrecorded entropy. Fixed-width integers, checked 64-bit
encoded sizes, strict booleans, and IEEE-754 binary64 fitness values define the
wire representation.

The format records CRC-32 over the complete byte range with the checksum field
treated as zero. CRC-32 detects accidental corruption only; it is not message
authentication, encryption, provenance, rollback protection, or authority.
The FNV-1a configuration fingerprint is likewise only a quick format
diagnostic. Inspection validates every decoded range, count, enum, finite
value, owner relationship, winner relationship, statistics sum, and
continuation invariant. Format 3 also records the recycling control, storage-
observer presence, population recycling disposition, stable active owner
identity, and complete logical storage registry. Inspection reconciles that
registry against configuration, generation parity, capacities, population
provenance, and secure-erasure metadata. It additionally binds evaluator thread-
safety declaration, worker count, library scratch budget, schedule-observer
presence, and committed population parallel provenance. No thread handle,
atomic epoch, runtime queue, provisional assignment, completion timing, or
worker scratch is serialized. Resume canonically re-encodes the supplied
configuration and compares its exact bytes. Neither checksum nor fingerprint
can authorize a malformed or different configuration.

`evo_checkpoint_inspect` treats its byte range as untrusted and validates the
budget, header, supported versions, total size, ordered non-overlapping
sections, checked arithmetic, integrity, configuration, state, records, and
statistics without allocation, RNG, or consumer callbacks. It returns the
ordered allocation-free `evo_checkpoint_view_t`. The view identifies the
exact input range and remains valid only while that caller-owned range is
unchanged.

`evo_checkpoint_candidate_inspect` projects one candidate in ascending
population order from a previously validated unchanged view. It exposes that
candidate's index, bounded genome span, validity/evaluation flags, and complete
fitness. The accessor uses explicit projected ranges and a fixed record stride
in constant time; enumerating the population is a complete linear audit. No
offset table, CRC, fingerprint, or projected view supersedes decoded exact
state. This is the mandatory Human-Readable Abstraction projection for binary
format 3 and is audited by EVO-HRA-002. Its appended
`population_storage_registry` is the mandatory complete recycler projection
and is audited by EVO-HRA-003.

`evo_resume` first performs inspection and exact configuration and stable-
identity matching. It allocates only after those checks, reconstructs the
population genome, evaluation, and result owners through the same reviewed
allocation and secure-erasure lifecycle as a fresh run, and revalidates the
completed population and statistics before committing any public result.
Every failure destroys all provisional owners and leaves an inactive result;
an already active result is rejected unchanged.

Continuation state includes the current generation and final reason, stable
global winner, all relevant RNG/operator/policy/schema versions, exact next
adaptive rate and stagnant count, patience reference and stagnant count,
complete current-population provenance and candidate evidence, schema-4
statistics, population-recycling policy and registry, committed parallel-
evaluation policy and worker count, and current/global-best genomes. Resume
invokes no callback for
the restored generation and begins at the next transition. A terminal
checkpoint reconstructs its terminal result without callbacks. An
uninterrupted run and a resume from any emitted checkpoint therefore produce
the same suffix generations, RNG streams, final winner, statistics, adaptive
state, stopping state, generation count, and termination reason.

Function and context pointers are newly attached runtime resources. Nonzero
problem and context identities declare their semantics but are not security
credentials; the consumer is responsible for attaching equivalent callback
code and external state. Persistent storage, confidentiality, authenticated
transport, rollback prevention, atomic file replacement, and erasure of
caller-owned checkpoint copies remain consumer responsibilities. Restored
EVO-owned allocations use the restoring build's local erasure backend; source
backend metadata is retained only as audit evidence.
The active slot is reconstructed through local allocations. A continued
enabled run materializes the opposite stable slot before its next visible
commit; a terminal resume needs no reusable physical owner. Formats 1 and 2
are rejected explicitly because neither contains complete format-3 parallel
configuration and committed provenance. ADR-0030 remains the base persistence
decision, ADR-0031 defines the format-2 recycling amendment, and ADR-0032
defines the format-3 parallel-evaluation amendment.

### Status values

`evo_status_t` defines:

| Status | Meaning |
|---|---|
| `EVO_SUCCESS` | Generation zero produced a valid winner and the run ended at the hard limit, after a promoted later all-invalid child, after convergence or stagnation, or after an application stop decision over committed state. |
| `EVO_ERROR_INVALID_ARGUMENT` | A required pointer is null, distance callback/version coupling is inconsistent, or stopping controls are malformed or noncanonical. |
| `EVO_ERROR_OUT_OF_MEMORY` | The system allocator returned null. |
| `EVO_ERROR_RESULT_ACTIVE` | The result already owns a genome and is preserved unchanged. |
| `EVO_ERROR_RESOURCE_LIMIT` | A required size is zero, arithmetic overflows, a caller budget is exceeded, or positive-limit elite count configuration is outside its canonical population bound. |
| `EVO_ERROR_STATE` | A private lifecycle operation received inactive, initialized, or inconsistent state. |
| `EVO_ERROR_EVALUATION` | A fitness callback returned a non-finite component or negative penalty, a domain-distance callback returned outside finite `[0, 1]`, or a fixed-order statistics component sum became non-finite. |
| `EVO_ERROR_NO_VALID_CANDIDATE` | Generation-zero evaluation completed, but every candidate was invalid, so no public winner exists. |
| `EVO_ERROR_CHECKPOINT_INVALID` | Untrusted bytes have malformed layout, fields, decoded state, or relationships. |
| `EVO_ERROR_CHECKPOINT_INTEGRITY` | The format-3 CRC-32 corruption check does not match. |
| `EVO_ERROR_CHECKPOINT_VERSION` | A checkpoint format, integrity algorithm, view-bound schema, or continuation-policy version is unsupported. |
| `EVO_ERROR_CHECKPOINT_MISMATCH` | A valid checkpoint does not exactly match the supplied deterministic configuration, callback-presence declarations, or stable semantic identities. |

### Termination reasons

`evo_termination_reason_t` defines successful outcome evidence separately
from `evo_status_t`:

| Reason | Meaning |
|---|---|
| `EVO_TERMINATION_NONE` | No successful run outcome exists. This is the zero-initialized, failed, and destroyed state. |
| `EVO_TERMINATION_GENERATION_LIMIT` | Generation zero completed with a zero limit, or every requested child transition completed. |
| `EVO_TERMINATION_ALL_INVALID` | A later all-invalid child was evaluated, promoted, counted, and ended the run while the earlier global winner was retained. |
| `EVO_TERMINATION_APPLICATION_REQUESTED` | The application stop callback returned true after a committed generation from which execution could otherwise continue. |
| `EVO_TERMINATION_CONVERGED` | The stable committed global-best total reached the enabled finite fitness target. |
| `EVO_TERMINATION_STAGNATED` | Enabled patience was exhausted or committed diversity reached the enabled floor. |

The reason is assigned only after all fallible public run work succeeds. It
does not replace `generations_completed`, which remains the exact quantitative
transition count. For one committed generation the precedence is
`ALL_INVALID`, `CONVERGED`, `STAGNATED`, `GENERATION_LIMIT`, then
`APPLICATION_REQUESTED`. The application callback is not invoked when an
earlier reason applies.

### Generation statistics

`EVO_GENERATION_STATISTICS_VERSION` is `4`. A successful active result retains
one `evo_generation_statistics_t` for the most recently committed generation:

| Field | Meaning |
|---|---|
| `version` | Statistics schema version. Version 4 identifies the 0.27.0 layout; zero exists only in an empty result. |
| `generation_index` | Zero for the initialized baseline; otherwise the promoted child generation. |
| `population_size` | Exact number of candidates in the committed population. |
| `valid_count` | Candidates admitted by `is_valid` and evaluated. |
| `invalid_count` | `population_size - valid_count`. |
| `best_index` | Stable generation-local best index, or zero when no valid candidate exists. |
| `best_fitness` | Complete generation-local stable-best fitness, or all zeros when no best exists. |
| `fitness_sums` | Component-wise sums over valid evaluated candidates only. |
| `has_best` | Whether the generation contains a valid evaluated candidate. |
| `fitness_comparison_policy_version` | Policy that admitted and ordered the generation's fitness evidence; version 1 for every success since 0.21.0 and zero only in an empty result. |
| `diversity_policy_version` | Diversity traversal and evidence policy; version 1 for every 0.22.0 success. |
| `diversity_metric_version` | Built-in byte metric version 1, or the nonzero caller-supplied domain metric version. |
| `diversity_pair_count` | Checked number of unordered pairs of hard-valid candidates. |
| `diversity_work_units` | Byte comparisons for the built-in metric, or domain callback invocations. |
| `diversity` | Fixed-order arithmetic mean normalized to `[0, 1]`; zero for fewer than two valid candidates. |
| `diversity_uses_domain_distance` | False for built-in byte distance and true for a caller domain callback. |
| `adaptive_mutation_policy_version` | Policy version 1 for an applicable positive-limit run; zero when mutation policy is not applicable. |
| `mutation_rate_prior` | Requested base rate at generation zero; otherwise the exact effective rate used to produce this generation. |
| `mutation_rate_effective` | Rate selected after this commit for a possible next transition. |
| `adaptive_mutation_min_rate`, `adaptive_mutation_max_rate`, `adaptive_mutation_step`, `adaptive_mutation_diversity_threshold` | Complete configured numeric policy projection. |
| `adaptive_mutation_stagnant_generations` | Saturating count of consecutive committed child generations without strict global-best improvement. |
| `mutation_adaptation_reason` | Explainable not-applicable, disabled, initial, diversity, stagnation, reset, or hold decision. |
| adaptation booleans | Enabled/reset policy plus low-diversity, strict-improvement, and min/max clamp facts for this commit. |

Aggregation policy version 1 traverses candidates in ascending index. Invalid
records contribute only to `invalid_count`; their fitness payloads are never
read. Each valid finite component is added to a `double` accumulator beginning
at positive zero. No reassociation, compensation, weighting, averaging,
normalization, or parallel reduction is permitted. A non-finite valid component
or intermediate sum returns `EVO_ERROR_EVALUATION` and the public result follows
the complete empty-failure contract.

Statistics copy the stable generation-local best already established by
population evaluation and never rank or mutate candidates. A child record is
computed before promotion but replaces the result record only after promotion
succeeds. A terminal all-invalid child therefore has zero sums and no local
best while the separate result allocation retains the earlier global winner.
Only the latest record is retained. Version 0.19.0 may deliver every committed
record to the synchronous observer, and version 0.20.0 may present it to a
synchronous stop decision. Neither callback defines history ownership.

Schema version 2 appends only the comparison-policy identity. The fixed-order
component aggregation remains policy version 1 and byte-for-logical-value
compatible with schema version 1 for every pre-existing field.

Schema version 3 preserves the complete schema-2 prefix and appends diversity
evidence governed by ADR-0022. Statistics copy the metric result already
committed by population evaluation and invoke no consumer callback.

Schema version 4 preserves the complete schema-3 prefix and appends the
adaptive-mutation projection governed by ADR-0028. Every record is constant-
space, RNG-neutral, and sufficient to explain the next-rate decision. The
observer sequence provides the ordered human-readable trace; the owning result
retains only the latest record.

### Generation observer

`EVO_GENERATION_RESULT_VIEW_VERSION` is `1`.
`evo_generation_observer_fn` receives a
`const evo_generation_result_view_t *`, a
`const evo_generation_statistics_t *`, and the configured observer context.
The callback returns `void` and cannot stop, reject, retry, or otherwise alter
the run.

The result view contains:

| Field | Meaning |
|---|---|
| `version` | Result-view schema version. |
| `best_genome` | Non-owning `const` view of the committed global winner. |
| `best_genome_size` | Exact readable byte bound for `best_genome`. |
| `best_fitness` | Complete global-best fitness after this commit. |
| `generations_completed` | Exact promoted-child count after this commit. |
| `random_seed` | Configured run seed. |
| `termination_reason` | `NONE` while execution continues, otherwise the stop decision for this final event. |

EVO constructs the result and statistics view objects as independent stack
snapshots. The view pointers and bounded genome alias remain valid only until
the callback returns. An observer must not retain their addresses, cast away
`const`, write through the genome view, free or reallocate it, or use it after
return. It may copy values or genome bytes into caller-owned bounded storage.

Generation-zero observation follows successful evaluation, statistics
construction, and global-winner transfer. A zero-limit event carries
`EVO_TERMINATION_GENERATION_LIMIT`; otherwise generation zero carries
`EVO_TERMINATION_NONE`.

Child observation follows successful statistics construction, atomic
promotion, completion-count update, strict global-winner update, natural-stop
classification, and any application stop decision. Each invocation completes
before the next child begins. The last requested child carries
`EVO_TERMINATION_GENERATION_LIMIT`; a promoted all-invalid child carries
`EVO_TERMINATION_ALL_INVALID` while retaining the earlier global winner in the
result view; and an application-selected generation carries
`EVO_TERMINATION_APPLICATION_REQUESTED`.

No event is emitted for invalid configuration, failed generation zero, failed
winner transfer, provisional child, failed child evaluation or statistics, or
failed promotion. If a later operation fails, already delivered committed-
generation observations remain valid while the final owning public result
still follows its complete empty-failure contract.

Observation is synchronous, serial, allocation-free, RNG-neutral, and non-
stopping. It does not define cancellation, asynchronous delivery, concurrent
callbacks, or retained event history.

### Application stop decision

`evo_generation_stop_fn` receives the same result-view and statistics types as
the observer plus its separately configured context. It returns `bool`: false
preserves execution, and true selects successful
`EVO_TERMINATION_APPLICATION_REQUESTED` termination at the current committed
generation.

EVO invokes the callback only when all of these conditions hold:

1. generation zero or a child generation has committed successfully;
2. its latest statistics and any strict global-winner improvement are visible;
3. no generation-limit or all-invalid terminal reason is already known; and
4. another child transition remains within `generation_limit`.

The callback therefore does not run for a zero-limit generation, the final
hard-limit generation, a promoted all-invalid child, provisional work, or any
failed generation. A never-stopping callback is invoked for generations
`0..generation_limit - 1`; the observer still receives the final hard-limit
event at `generation_limit`.

The stop result view always carries `EVO_TERMINATION_NONE`, because the
callback is deciding whether execution should continue. If it returns true,
the owning result later publishes `EVO_TERMINATION_APPLICATION_REQUESTED`
after private cleanup succeeds. If an observer is configured, EVO constructs
new independent stack snapshots and invokes it after the stop decision, so the
observer sees the final reason. The two callback snapshot objects never alias
one another or the owning public result.

The stop callback is synchronous, serial, allocation-free, and RNG-neutral.
Its views are non-owning and valid only for the call. It must not retain a view,
cast away `const`, mutate or release the global genome, invoke lifecycle
operations on the active public result, or depend on unrecorded state if replay
is required. It does not provide signal handling, cross-thread cancellation,
time limits, reentrancy, or asynchronous event-loop integration.

### Result fitness

On `EVO_SUCCESS`, `best_fitness` is the complete seven-field value returned by
the consumer evaluator for the selected candidate. A zero-valued component or
total is therefore valid evaluated evidence. On failure, every fitness field
is reset to zero as part of the empty result state.

## API Compatibility

Version 0.2.0 appended `max_genome_bytes` to `evo_config_t` and changed
`evo_run` from a raw `int` result to `evo_status_t`. Version 0.3.0 appended
`max_population_bytes`, and version 0.4.0 appended one status enumerator.
Version 0.5.0 appends `max_evaluation_bytes` and one additional status value.
Existing member offsets remain preserved, but `sizeof(evo_config_t)` and its
array stride change again. Version 0.6.0 appends
`EVO_ERROR_NO_VALID_CANDIDATE` without changing a public structure layout, and
changes successful `evo_run` semantics from an allocation scaffold to a
generation-zero evaluated result. Consumers must rebuild against the 0.6.0
header and provide all three nonzero memory budgets.

Version 0.7.0 changes no public type, function signature, installed symbol, or
`evo_run` behavior. It increments the version and adds independently verified
private RNG and tournament-selection behavior.

Version 0.8.0 likewise changes no public layout, signature, installed symbol,
or `evo_run` behavior. It documents the existing callback contract more
precisely and adds private probability and crossover-dispatch behavior.

Version 0.9.0 likewise changes no public layout, signature, installed symbol,
or `evo_run` behavior. It documents the existing mutation callback contract
more precisely and adds private mutation-dispatch behavior.

Version 0.10.0 appends `max_child_population_bytes` to `evo_config_t` without
changing existing member offsets. `sizeof(evo_config_t)` and its array stride
increase, so consumers must rebuild. Generation-zero `evo_run` behavior and
the installed public function signatures remain unchanged; the appended field
is used only by the new private child-storage boundary.

Version 0.11.0 changes no public layout, function signature, installed symbol,
memory policy, or `evo_run` behavior. It adds private operator-stream derivation
and complete parent-pair planning. Consumers rebuilding from 0.10.0 require no
source change.

Version 0.12.0 likewise changes no public layout, function signature,
installed symbol, memory policy, or `evo_run` behavior. It adds private child-
production progress metadata and deterministic complete-pair composition.
Consumers rebuilding from 0.11.0 require no source change.

Version 0.13.0 likewise changes no public layout, function signature,
installed symbol, memory policy, or `evo_run` behavior. It adds private
odd-tail policy metadata and deterministic stable-elite completion. Consumers
rebuilding from 0.12.0 require no source change.

Version 0.14.0 likewise changes no public layout, function signature,
installed symbol, memory policy, or `evo_run` behavior. It adds private child-
evaluation policy evidence, shares the existing provisional evaluation engine,
and recognizes evaluated-child provenance as completed-population authority.
Consumers rebuilding from 0.13.0 require no source change.

Version 0.15.0 likewise changes no public layout, function signature,
installed symbol, memory policy, or `evo_run` behavior. It adds a private,
allocation-free generation-advancement operation that transfers evaluated-
child ownership, releases the former parent, and records versioned transition
evidence. Consumers rebuilding from 0.14.0 require no source change.

Version 0.16.0 changes no public layout, function signature, installed symbol,
or memory-policy field. It changes `evo_run` behavior for positive
`generation_limit` values by composing bounded child transitions. A zero limit
retains the established generation-zero behavior. Consumers using positive
limits must provide valid transition policy and `max_child_population_bytes`.

Version 0.17.0 adds `evo_termination_reason_t` and appends
`termination_reason` to `evo_result_t`. Every existing result member retains
its offset, but `sizeof(evo_result_t)` and array stride change, so consumers
must rebuild. No public function signature or installed symbol changes. The
new field classifies the two existing successful stop conditions without
changing callback order, RNG replay, ownership, selection, generation count,
or failure behavior.

Version 0.18.0 adds `evo_generation_statistics_t` and appends
`generation_statistics` after `termination_reason` in `evo_result_t`. Every
pre-0.18.0 result member retains its offset, but `sizeof(evo_result_t)` and
array stride change again, so consumers must rebuild. No public function
signature, installed symbol, configuration field, or allocation budget changes.
The appended value retains only the latest committed generation and therefore
does not add history ownership proportional to `generation_limit`.

Version 0.19.0 adds `evo_generation_result_view_t` and
`evo_generation_observer_fn`, then appends `generation_observer` and
`generation_observer_context` to `evo_config_t`. Every pre-0.19.0 config member
retains its offset, but `sizeof(evo_config_t)` and array stride change, so
consumers must rebuild. No installed function signature, result layout,
symbol, allocation class, or resource budget changes. A null callback preserves
the prior execution surface.

Version 0.20.0 adds `evo_generation_stop_fn`, appends `generation_stop` and
`generation_stop_context` to `evo_config_t`, and appends
`EVO_TERMINATION_APPLICATION_REQUESTED` to `evo_termination_reason_t`. Every
pre-0.20.0 config member retains its offset, but `sizeof(evo_config_t)` and
array stride change, so consumers must rebuild. No installed function
signature, result layout, symbol, allocation class, or resource budget changes.
A null stop callback preserves the 0.19.0 execution and observation surface.

Version 0.21.0 adds the public
`EVO_FITNESS_COMPARISON_POLICY_VERSION` macro and appends
`fitness_comparison_policy_version` to
`evo_generation_statistics_t`. Every pre-0.21.0 statistics member retains its
offset, but the statistics schema advances to version 2. Depending on ABI
padding, `sizeof(evo_generation_statistics_t)`, `sizeof(evo_result_t)`, and
their array strides may remain unchanged or increase; binary layout
compatibility is not assumed, so consumers must rebuild. `evo_fitness_t`,
problem and config layouts, and public function signatures do not change. The
implementation adds internal comparison-helper symbols declared only by a
non-installed header; allocation classes and resource budgets do not change.

Version 0.22.0 adds `EVO_DIVERSITY_POLICY_VERSION`,
`EVO_BYTE_DIVERSITY_METRIC_VERSION`, and `evo_genome_distance_fn`; appends the
distance callback and version to `evo_problem_t`; appends
`max_diversity_work` to `evo_config_t`; and appends the schema-3 diversity
fields to `evo_generation_statistics_t`. Every pre-0.22.0 member retains its
offset, but structure sizes and array strides may change. Binary compatibility
is not assumed, so consumers must rebuild. Public function signatures and
installed symbols do not change, and diversity introduces no allocation
class.

Version 0.23.0 adds `EVO_STOPPING_POLICY_VERSION`, appends
`EVO_TERMINATION_CONVERGED` and `EVO_TERMINATION_STAGNATED` after every prior
termination value, and appends the seven fitness-target, tolerance/patience,
and diversity-floor controls after the complete pre-0.23.0
`evo_config_t` prefix. Existing member offsets and enum values are preserved,
but the configuration size and array stride may change. Binary compatibility
is not assumed, so consumers must rebuild. Public function signatures,
installed symbols, statistics schema, allocation classes, and resource budgets
do not change.

Version 0.24.0 adds `EVO_ELITE_POLICY_VERSION` and appends
`elite_count_enabled` plus `elite_count` after the complete pre-0.24.0
`evo_config_t` prefix. Existing member offsets are preserved, but configuration
size and array stride may change, so consumers must rebuild. Public function
signatures, installed symbols, statistics schema, allocation classes, and
resource budgets do not change. Disabled zero payload preserves pre-0.24.0
positive-limit behavior; explicit mode changes child composition as requested.

Version 0.25.0 adds `evo_selection_policy_t`,
`EVO_SELECTION_POLICY_VERSION`, `EVO_SELECTION_TOURNAMENT`, and
`EVO_SELECTION_RANK`; then appends `selection_policy`,
`rank_base_weight`, and `rank_step_weight` after the complete pre-0.25.0
`evo_config_t` prefix. Existing member offsets and the zero-valued tournament
enum are preserved, but configuration size and array stride may change, so
consumers must rebuild. Public function signatures, installed symbols,
statistics schema, allocation classes, and resource budgets do not change.

Version 0.26.0 adds `evo_crossover_operator_t`,
`evo_mutation_operator_t`, `EVO_BYTE_OPERATOR_POLICY_VERSION`, the zero-valued
consumer compatibility selectors, and the explicit reference byte selectors;
then appends `crossover_operator` and `mutation_operator` after the complete
pre-0.26.0 `evo_config_t` prefix. Existing member offsets and zero-initialized
consumer behavior are preserved, but configuration size and array stride may
change, so consumers must rebuild. Supported public function signatures and
symbols, statistics schema, allocation classes, and resource budgets do not
change. The implementation adds validator symbols declared only by
non-installed headers.

Version 0.27.0 adds `evo_mutation_adaptation_reason_t` and
`EVO_MUTATION_ADAPTATION_POLICY_VERSION`; appends six adaptive-policy fields
after the complete pre-0.27.0 `evo_config_t` prefix; and appends the schema-4
adaptation projection after the complete pre-0.27.0 statistics prefix.
Existing member offsets and zero-initialized static-rate behavior are
preserved, but configuration, statistics, and result sizes or array strides
may change, so consumers must rebuild. Supported public function signatures
and installed function symbols, allocation classes, resource budgets, and RNG
domains do not change.

Version 0.28.0 adds `evo_secure_erasure_backend_t` and
`EVO_SECURE_ERASURE_POLICY_VERSION`; appends `secure_erasure_enabled` after
the complete pre-0.28.0 `evo_config_t` prefix; and appends
`best_genome_size`, policy version, backend, and enabled evidence after the
complete pre-0.28.0 `evo_result_t` prefix. Existing member offsets and
zero-initialized ordinary-release behavior are preserved, but configuration
and result sizes or array strides may change, so consumers must rebuild.
Supported public function signatures and RNG domains do not change. One new
private production module supplies the build-selected erasure wrapper.

Version 0.29.0 appends `checkpoint_problem_identity` after the complete
pre-0.29.0 `evo_problem_t` prefix and appends the checkpoint budget, buffer,
observer, context, and stable context identity after the complete pre-0.29.0
`evo_config_t` prefix. It adds four checkpoint status values, the versioned
configuration/candidate/checkpoint projection types, the checkpoint observer,
and the `evo_checkpoint_size`, `evo_checkpoint_inspect`,
`evo_checkpoint_candidate_inspect`, and `evo_resume` installed symbols.
Existing member offsets and prior enum values are preserved, but problem and
configuration sizes or array strides change, so consumers must rebuild.
Zero-initialized checkpoint fields preserve ordinary 0.28.0 execution.
Checkpoint capture and resume require explicit nonzero semantic identities and
a caller byte budget. The private bounded-run policy advances to version 10 so
fresh and restored committed state use the same continuation loop.

Version 0.30.0 adds `evo_population_storage_lifecycle_t`,
`evo_population_storage_reset_disposition_t`,
`evo_population_storage_entry_t`, `evo_population_storage_registry_t`, and the
population-storage observer type and version constants. It appends the
recycling control, observer, and observer context after the complete pre-0.30.0
`evo_config_t` prefix; appends recycling and observer-presence fields after the
complete version-1 checkpoint-configuration prefix; and appends the registry
after the complete version-1 checkpoint-view prefix. Existing member offsets
and enum values are preserved, but configuration and checkpoint-view sizes or
array strides change, so consumers must rebuild. Supported public function
signatures and installed function symbols do not change. Zero initialization
preserves the 0.29.0 allocator lifecycle. Checkpoint format, checkpoint view,
and checkpoint-configuration view advance to version 2; the candidate view
remains version 1.

Version 0.31.0 adds the evaluator thread-safety enum, parallel policy and
schedule version constants, assignment disposition and schedule outcome enums,
versioned assignment/schedule projections, schedule observer, and
`evo_evaluation_worker_scratch_size`. It appends the thread-safety declaration
after the complete pre-0.31.0 `evo_problem_t` prefix and worker count, scratch
budget, observer, and context after the complete pre-0.31.0 `evo_config_t`
prefix. It appends parallel configuration and committed provenance to the
complete version-2 checkpoint views. Existing member offsets and prior enum
values are preserved, but public structure sizes or array strides change, so
consumers must rebuild. Zero initialization preserves serial evaluation.
Checkpoint format and both top-level views advance to version 3; the candidate
view remains version 1.

Version 0.32.0 changes no public type, member offset, enum value, function
signature, installed symbol, core policy version, checkpoint format, callback
order, allocation behavior, or RNG schedule. It advances only EVO's package
and public version macros and adds repository benchmark/schema/projection
entry points outside the installed library ABI.

Version 0.33.0 likewise changes no public type, member offset, enum value,
function signature, installed symbol, core policy version, checkpoint format,
callback order, allocation behavior, or RNG schedule. It advances only EVO's
package and public version macros and adds external reference consumers,
evidence schema/golden/validator entry points, documentation, and independent
staged-install verification outside the installed library ABI.

Version 0.34.0 likewise changes no public type, member offset, enum value,
function signature, installed symbol, core policy version, checkpoint format,
callback order, allocation behavior, or RNG schedule. It advances the package
and public version macros and adds an uninstalled private project-ingestion
foundation, schemas, fixtures, evidence, documentation, and independent
verification outside the stable library ABI.

Version 0.35.0 likewise changes no public type, member offset, enum value,
function signature, installed symbol, core policy version, checkpoint format,
callback order, allocation behavior, or RNG schedule. It advances the package
and public version macros and adds an uninstalled private project-analysis
model, schema, evidence, documentation, and independent verification outside
the stable library ABI.

Version 0.36.0 likewise changes no public type, member offset, enum value,
function signature, installed symbol, core policy version, checkpoint format,
callback order, allocation behavior, or RNG schedule. It advances the package
and public version macros and adds an uninstalled private transformation-recipe
model, canonical fixed-genome encoding, schema, golden, audit projection,
documentation, and independent verification outside the stable library ABI.

Version 0.37.0 likewise changes no public type, member offset, enum value,
function signature, installed symbol, core policy version, checkpoint format,
callback order, allocation behavior, or RNG schedule. It advances the package
and public version macros and adds an uninstalled private AST-aware C
transformation catalogue, provider/application model, evidence schemas,
goldens, audit projection, documentation, and independent verification outside
the stable library ABI.

## Current 0.37.0 Conformance Boundary

The current implementation exposes generation-zero compatibility plus bounded
multi-generation execution:

- validation enforces required pointers, an evaluator, an inactive result, the
  three generation-zero memory budgets, distance callback/version coupling,
  and the all-valid diversity work bound;
- successful execution constructs, initializes, validates, and evaluates a
  private population in deterministic order;
- zero workers retain exact serial evaluator order; positive workers require an
  explicit thread-safety declaration, exact scratch budget, fixed assignment,
  wave-bounded dispatch, complete join, and stable candidate-order commit;
- the complete schedule projection exposes assignment, completion, failure,
  cancellation, and commit order while runtime thread identity and timing have
  no decision authority;
- hard-invalid candidates are never evaluated, aggregated, selected, or
  ranked;
- comparison policy version 1 requires finite fitness, a non-negative soft-
  penalty magnitude, and selects the stable greatest caller total using
  earlier-generation/lower-index ties without reapplying the penalty;
- success transfers one independent global-best genome copy and complete
  fitness evidence;
- all-invalid completion has a distinct public status;
- allocation, resource, state, and evaluation failures return an empty result
  after complete private cleanup;
- result destruction is null-safe, repeatable, restores the empty state, and
  applies the retained ordinary or secure-erasure disposition;
- private population construction checks size arithmetic and both caller
  budgets before allocating a contiguous zero-initialized slab;
- private population views are bounds-checked and non-owning;
- private population destruction is null-safe, repeatable, and fully
  resetting;
- private RNG output and byte order are locked by fixed vectors;
- private bounded-index sampling is unbiased, fixed-vector locked, and replay
  stable;
- private operator streams are versioned, tuple-addressable, domain-separated,
  fixed-vector locked, and exactly matched to the accepted mixed-control
  research schedule;
- private population initialization is seed-reproducible and records its RNG
  algorithm version;
- optional initializers run exactly once in ascending genome order;
- private validity always runs in an ascending pass; serial evaluation remains
  ascending, while worker evaluation commits the same ascending record order;
- private tournament selection samples valid evaluated candidates with
  replacement and deterministic tie handling, with byte-for-byte pre-0.25.0
  replay through the zero-valued dispatch;
- private stable rank selection uses the common comparator, exact linear
  integer weights, unbiased tickets, ascending-index interval traversal, and
  no allocation;
- selection-policy configuration and worst-case rank arithmetic reject before
  positive-limit allocation or callbacks;
- private crossover dispatch consumes a fixed one-word probability decision,
  invokes the representation-aware callback or clones parents in zero-valued
  compatibility mode, and preserves child output on precondition failure;
- explicit byte crossover bypasses callbacks and provides bounded one-point,
  distinct-boundary two-point, and least-significant-bit-first uniform modes
  that completely initialize complementary children;
- private mutation dispatch consumes a fixed one-word probability decision,
  invokes the representation-aware callback or preserves the genome in zero-
  valued compatibility mode, and preserves genome output on precondition
  failure;
- explicit byte-XOR mutation bypasses callbacks, selects one bounded byte and
  one nonzero mask through unbiased draws, and changes exactly that byte;
- private child-population creation validates completed parent evidence,
  enforces a separate checked budget, and creates independently owned empty
  output storage;
- private complete-pair planning derives a pair-local selection stream, runs
  two configured selection draws, maps consecutive ordinary-prefix slots,
  records selection provenance, and preserves output and parent evidence on
  rejection;
- private complete-pair production preflights the combined boundary, derives
  pair- and child-indexed operator streams, dispatches crossover and both
  mutations, records a contiguous child prefix with selection and byte-operator
  provenance, and preserves parent evidence;
- private elite policy resolves explicit or compatibility requests, caps them
  at distinct hard-valid parents, lays stable best-to-worst clones in a suffix,
  consumes no RNG or callbacks for elites, and preserves every object on
  detectable rejection;
- a private ordinary-singleton path owns an odd prefix slot through the next
  selection stream and the standard child-indexed mutation stream without
  crossover or scratch allocation;
- private produced-child evaluation accepts complete even and odd production
  provenance, commits deterministic valid-only policy-valid fitness evidence,
  preserves selection- and byte-operator-policy identity, and promotes the
  child to shared completed-population authority;
- private generation advancement validates current/child lineage and all
  ownership ranges, then either releases the former parent in compatibility
  mode or resets it into the reusable handle under enabled recycling; both
  paths record the next generation plus complete policy provenance without
  allocation, RNG, or callbacks after preflight;
- public bounded execution validates transition-only policy before callbacks,
  runs ascending transitions, allocates the result once, retains earlier exact
  ties, counts completed promotions, and stops successfully after promoting a
  later all-invalid child;
- public success records generation-limit, later-all-invalid, converged,
  stagnated, or application-requested termination, while every failure and
  destruction restores the zero reason;
- generation zero and each promoted child receive versioned fixed-order
  statistics over valid records, with the terminal record retained in constant
  result space;
- invalid fitness payloads are excluded from statistics, finite component sums
  are checked, schema version 4 records comparison, bounded diversity, and
  adaptive-mutation audit evidence, and
  statistics never change stable-best or global-winner selection;
- every unordered hard-valid pair is measured in fixed lexicographic order by
  built-in byte metric version 1 or a versioned normalized domain callback;
- checked pair/work arithmetic rejects overflow or insufficient diversity
  budget before callbacks, zero/one valid candidates require no pair work, and
  diversity consumes no operator RNG or selection draw;
- an optional synchronous observer receives independent read-only result and
  statistics snapshots after generation zero and every promoted child;
- an optional synchronous application stop decision receives independent
  read-only snapshots only after a nonterminal committed generation;
- natural terminal reasons suppress the application decision, a true decision
  preserves committed state, and a null callback is 0.19.0 replay-equivalent;
- zero-initialized stopping controls preserve the 0.22.0 limit behavior, while
  enabled target, tolerance/patience, and diversity-floor controls inspect
  only committed global-winner and statistics evidence;
- stopping equality boundaries, significant-improvement reset, generation
  counts, and the all-invalid/converged/stagnated/limit/application precedence
  are fixed by policy version 1 without time, addresses, RNG, or allocation;
- zero-initialized adaptation preserves static-rate replay, while enabled
  policy clamps and steps within finite bounds from committed strict-
  improvement and inclusive diversity evidence only;
- every applicable generation exposes prior/effective rates, policy inputs,
  saturating stagnant count, clamp/reset facts, and an explainable reason;
- the effective rate is propagated through consumer and reference mutation,
  child production, evaluation, promotion, bounded-run evidence, and replay;
- observer delivery follows winner update and final stop classification, precedes
  the next generation, allocates no history, and emits nothing for provisional
  or failed generations;
- direct bounded arrays remain exact byte-operator authority, with no
  compressed, cached, indexed, or probabilistic structure requiring an ADR-0026
  audit projection;
- the direct owner-and-byte-count registry records every governed erasure
  range, enabled lifecycle disposition, and build-selected backend without an
  accelerated or address-keyed authority;
- the optional recycler alternates exactly two stable run-local slot identities
  and performs no allocation after the first enabled transition;
- its complete address-free registry exposes roles, capacities, provenance,
  handoffs, resets, erasures, and owner presence while exact pointer/count
  owners remain authority;
- enabled and disabled differential replay has identical consumer callbacks,
  committed algorithm evidence, winners, stopping, termination, and RNG state;
- format-3 checkpoint capture uses an exact caller-owned bounded buffer and
  serializes every continuation dependency without native padding, pointers,
  addresses, clocks, or process state;
- untrusted inspection validates all byte ranges, arithmetic, versions,
  integrity, decoded state, population evidence, statistics, and ownership
  relationships without allocation, RNG, or callbacks;
- exact canonical configuration bytes and stable semantic identities gate
  resume before allocation; CRC-32 and FNV-1a remain diagnostics only;
- checkpoint and per-candidate views expose the stable ordered audit
  projection required by ADR-0026, including direct genome/evaluation ranges,
  and the checkpoint view exposes the complete recycler registry, with no
  compressed or opaque authority;
- restored state uses the same reviewed population, evaluation, result, and
  secure-erasure ownership paths as fresh execution;
- generation-zero, intermediate, and terminal resume continue only the
  remaining suffix without duplicate callback delivery and preserve RNG,
  winners, statistics, adaptation, patience, recycler identities, counts, and
  termination; and
- the maintained core benchmark gates explicit fixed oracles and complete
  serial/parallel plus allocate/recycle semantic equivalence before retaining
  ordered raw timing and memory evidence, with no performance threshold; and
- variable-size or cross-run pooling, persistent worker executors, asynchronous
  callback cancellation, and distributed evaluation are not implemented.

Consumers may treat `EVO_SUCCESS` as evidence of a valid global winner and
exactly `generations_completed` promoted child generations. They must inspect
`termination_reason` for the successful outcome rather than infer it from the
count. They may inspect `generation_statistics` for the final committed
population, which is distinct from the global winner on all-invalid
termination. When configured, they may copy each callback-lifetime observation
into their own bounded storage. They may also configure deterministic stopping
over committed snapshots. Version 0.32.0 defines no statistics history,
asynchronous callback cancellation, or retained callback delivery. It adds
caller-owned checkpoint copies, deterministic suffix resume, and optional
run-local storage recycling and bounded evaluator concurrency, but it still
retains no internal checkpoint history and provides no persistent-storage,
authentication, confidentiality, rollback-prevention, or asynchronous-I/O
service.

## Verification

The baseline verification set is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The bounded benchmark verification set is:

```sh
cmake -S . -B build/benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DEVO_BENCHMARK_COMMIT=<commit>
cmake --build build/benchmark --target benchmark-smoke --parallel
```

`EVO-CORE-001` proves fixed oracle vectors, repeated semantic replay, exact
serial/parallel and allocate/recycle equivalence, complete ordered generation
traces and raw samples, validated schema/authority fields, and Markdown
regeneration from canonical JSON. Runtime and RSS remain reporting-only.

The portable result-lifecycle test proves public callback order, invalid
suppression, winner transfer, complete fitness evidence, stable ties,
all-invalid mapping, non-finite rejection, active-result preservation, and
reuse. Population-storage, RNG-vector, population-initialization, and
population-evaluation tests continue to verify each private phase
independently. The selection test proves completed-state validation, tournament
bounds and exact compatibility replay, rank-policy arithmetic, golden rank
vectors, stable ties, invalid exclusion, one-member and extreme weights,
fixed-seed statistical sanity, all-invalid handling, and failure preservation.
A secure-erasure link-wrapper test proves every registry range is erased over
its exact byte count immediately before each governed reset or release on
success, promotion/reuse, allocation failure, all-invalid transfer failure,
generation-zero and child provisional failure, rollback, and restored
checkpoint owners. It separately proves exact per-slot repeated reset counts,
one terminal erasure per live owner, that caller checkpoint buffers are outside
EVO's erasure authority, disabled policy invokes no erasure, and both the
detected `explicit_bzero` backend and volatile-byte fallback produce complete
zero ranges.
A separate Linux-only static-link test uses the GNU-compatible
`--wrap=calloc` linker facility to
prove failure and cleanup at the population, evaluation-record, and
result-transfer allocations, including every restore allocation point. It also
proves enabled runs have exactly five allocations for one or seven transitions,
failure at each of those sites cleans every earlier owner, no sixth allocation
can occur, and overflowing rank configuration or malformed checkpoint
preflight rejects before the first allocation attempt.

The fitness-policy test locks comparison policy version 1, hard-valid and
evaluated rankability, non-negative finite penalty evidence, caller-total
authority without double application, earlier-generation/lower-index ties,
atomic malformed-penalty failure, public statistics policy evidence, and
total-only replay compatibility. Population-evaluation tests independently
prove negative-penalty rollback and deterministic retry, while selection tests
reject missing comparison-policy evidence before consuming RNG.

The crossover test proves pointer and selector validation, complete-span alias
rejection, rate endpoints, exact callback compatibility, and deterministic
reference one-point, two-point, and uniform behavior. It covers one-byte and
equal-parent inputs, every internal one-point cut, every distinct two-point
boundary pair, a uniform mask crossing 32 bytes, full child writes, guard bytes,
callback bypass, golden outputs, exact post-operation RNG state, rejection
preservation, and replay. RNG tests separately lock probability thresholds and
bounded-sampling vectors.

The mutation test proves pointer and selector validation, rate endpoints, exact
consumer callback/no-op compatibility, rate and context forwarding, and
deterministic byte-XOR behavior. It covers one-byte genomes, every byte
position, a guaranteed nonzero mask, exactly one changed byte, guard bytes,
callback bypass, golden output, exact post-operation RNG state, rejection
preservation, and replay.

The adaptive-mutation test locks dyadic exact traces for initial min/max clamp,
inclusive diversity equality, stagnation, low diversity, improvement reset,
improvement hold, bounded increases, and disabled static behavior. It also
proves malformed and alias rejection are atomic; observer projections are
ordered and complete; selected consumer callbacks receive the committed
effective rate unchanged; reference byte mutation bypasses the callback;
replay is exact; and zero-limit or all-elite one-member runs ignore genuinely
unused transition policy.

The child-population test proves completed-parent validation, separate budget
enforcement, zero-initialized distinct storage, parent preservation, active-
child rejection, all-invalid storage behavior, and independent destruction.
The allocation-failure test separately proves empty-child cleanup while the
completed parent remains intact.

The parent-pair test proves policy and pair bounds, completed-parent and all-
invalid handling, fixed pair vectors, valid-only selection with replacement,
replay, odd trailing-slot exclusion, output preservation, and parent
preservation. RNG tests lock the production operator schedule, and the seed-
schedule research test proves byte-for-byte stream agreement with the accepted
plain mixed control.

The child-pair test proves combined preflight preservation, all-invalid parent
handling, fixed parent and child-byte vectors, replay, sequential progress,
source-generation separation, callback and identity-clone paths, built-in
callback bypass and replay composition, byte-operator provenance, odd-tail
preservation, parent immutability, and rejection of repeated or skipped pairs.

The child-tail test proves complete-prefix enforcement, stable-best and tie
behavior, one-member completion, byte-and-evidence replay, absence of extra RNG
or callbacks, parent and prefix preservation, all-invalid and alias rejection,
and terminal policy metadata.

The elite test proves explicit counts `0`, `1`, `N - 1`, and `N` across even
and odd populations; stable tie order; invalid-heavy effective-count capping;
best-to-worst suffix layout; singleton selection and mutation stream identity;
compatibility bytes; replay; absence of hidden elite callbacks; parent
immutability; and atomic object and owned-range alias rejection.

The child-evaluation test proves even, odd, and one-member production
provenance; deterministic validation-before-evaluation ordering; invalid-
candidate suppression; stable ties; all-invalid completion; byte, record, and
evidence replay; budget and malformed-fitness rollback; repeated-evaluation
rejection; completed-population validation; and next-child authorization. The
allocation-failure test separately proves that a failed child-evaluation record
allocation preserves the fully produced child unchanged.

The generation-advancement test proves generation-zero and later-generation
lineage, allocation-identity and byte preservation, all-invalid promotion,
empty-child reuse, overflow, object and owned-range alias rejection, malformed
state preservation, and repeated-call rejection. It separately proves enabled
two-slot role exchange, full former-parent reset, registry/evidence propagation,
and stale or aliased registry rejection. The wrapped-allocation and release
test also proves that compatibility advancement succeeds while the next
allocator call is forced to fail and releases exactly the two former-parent
allocations, confirming that transition's allocation-free single-owner
contract.

The generation-statistics test locks fixed aggregation policy and schema
version 4 golden vectors for
even, odd, one-member, tied, mixed-validity, and all-invalid populations. It
poisons invalid fitness payloads with non-finite values to prove they are not
read, verifies fixed-order component sums and stable generation-local best
evidence, and proves malformed state, non-finite valid fitness, and aggregate
overflow reject without modifying caller output.

The diversity test locks zero-valid, one-valid, homogeneous, mixed, maximally
separated, odd, and invalid-heavy byte-metric vectors; domain callback order
and version propagation; replay-identical statistics; checked budget and
arithmetic rejection before callbacks; malformed-distance rollback; invalid
exclusion; and selection/RNG neutrality.

The generation-observer test proves one event for a zero-limit run, N+1 events
for N completed transitions, synchronous ordering before the next generation,
updated global-winner and generation-statistics evidence, terminal reason
visibility, all-invalid global/local separation, independent snapshot
addresses, fixed-seed replay, and absence of events for failed or provisional
generations. The installed consumer exercises converged reason delivery through
the public observer view. Wrapped allocation tests prove observation adds no
allocation or release and reports only earlier committed generations before a
later injected failure.

The application-stop test proves immediate generation-zero stopping,
intermediate stopping after a promoted child, stop-before-observer ordering,
const independent callback snapshots that exactly mirror the committed public
winner and statistics, callback-time deferral of the public termination reason,
application termination evidence, natural-reason precedence, complete failure
reset, absence of decisions for provisional or failed children, and null/never-
stop replay equivalence with 0.19.0. Wrapped allocation tests prove
immediate stopping adds no allocation, consumes no child transition, and
releases each owner exactly once; they also prove a continuing stop decision is
delivered only for generations committed before an injected later failure.

The stopping-policy test locks exact target, tolerance, and diversity-floor
boundaries; generation-zero target and floor decisions; significant-
improvement patience reset; exact fitness ties; cumulative small improvement;
replay; extinction, convergence, stagnation, limit, and application
precedence; canonical disabled behavior; and malformed-control rejection
before callbacks. The installed consumer proves target convergence suppresses
the application callback and reaches the observer and final public result.

The bounded-run test proves zero-limit compatibility; positive-limit policy
validation before callbacks, including invalid operator selectors; even, odd,
and one-member execution; deterministic consumer and reference-operator multi-
transition replay with built-in callback bypass; strict global improvement;
earlier-winner exact ties;
later all-invalid promotion and successful stop; generation-zero all-invalid
mapping; explicit generation-limit and all-invalid termination reasons;
active-result preservation; appended result layout; destruction reset; and
versioned private run evidence including application-requested completion. It
also proves generation-zero, promoted,
replay, odd-tail, one-member, tied, and terminal all-invalid statistics, plus
private rejection of mismatched generation-zero statistics. The
wrapped-allocation test additionally proves the five-allocation bounded path,
exact successful cleanup, explicit-elite transition composition, and empty
public failure after child-slab or child-evaluation allocation failure.

The population-recycling test differentially compares the disabled reference
path, enabled accelerator, and replay. It locks the complete callback trace,
result, schema-4 statistics, generation events, RNG-neutral operator behavior,
and every generation's ordered registry. It also proves observer delivery order
after generation observation and stopping but before checkpoint capture.

The parallel-evaluation test compares worker counts 1 through 4 with serial
evaluation across invalid candidates and multiple recycled generations. It
locks fixed logical assignment and wave identity, stable valid-candidate commit
order, exact schedule counts, thread-safe configuration and scratch boundaries,
non-finite wave cancellation, worker-start failure before callbacks, complete
join, replay, and public result/statistics parity. The hosted ThreadSanitizer
job runs this focused scheduler surface, and wrapped allocation tests prove the
single scratch allocation and atomic exhaustion path.

The checkpoint test proves exact size calculation; capture ordering for
generation zero and every committed child; allocation-free ordered inspection;
constant-time candidate projection; generation-zero, intermediate, natural-
terminal, and application-terminal suffix resume; final genome, fitness,
statistics, adaptation, patience, generation-count, and termination parity;
enabled registry persistence, parallel policy/configuration projection,
parallel suffix replay, byte-identical recycled resume suffix; and
suppression of duplicate callbacks for the restored generation. It also
proves active-result preservation and callback-free rejection for malformed
identity, configuration, budget, selector, alias, truncation, version,
integrity, registry, and decoded-state inputs. Independent CRC calculation plus
re-signed configuration, provenance, and termination tampering prove that
diagnostic integrity values cannot authorize semantically impossible state.

The deterministic checkpoint-fuzz test rejects every truncation, a one-bit
mutation at every serialized byte, and 2,048 seeded arbitrary byte ranges.
The separate `tests/fuzz/checkpoint_fuzz.c` entry point exposes the same
allocation-free untrusted parser to libFuzzer. Build-manifest parity requires
exactly 26 installed-core sources plus eighteen private source-foundation
sources and 37 normative targets in CMake, GNU Autotools, and AES-BLD-001
inventories.

## Related Records

- `docs/adr/ADR-0001-library-boundary-and-build-system.md`
- `docs/adr/ADR-0002-deterministic-rng-and-population-initialization.md`
- `docs/adr/ADR-0004-generation-zero-validation-and-evaluation.md`
- `docs/adr/ADR-0005-generation-zero-public-run-integration.md`
- `docs/adr/ADR-0006-deterministic-tournament-selection.md`
- `docs/adr/ADR-0007-deterministic-crossover-dispatch.md`
- `docs/adr/ADR-0008-deterministic-mutation-dispatch.md`
- `docs/adr/ADR-0009-bounded-child-population-ownership.md`
- `docs/adr/ADR-0010-versioned-operator-substreams-and-parent-pair-planning.md`
- `docs/adr/ADR-0011-deterministic-complete-pair-child-production.md`
- `docs/adr/ADR-0012-deterministic-odd-tail-elite-cloning.md`
- `docs/adr/ADR-0013-deterministic-produced-child-evaluation.md`
- `docs/adr/ADR-0014-atomic-generation-advancement.md`
- `docs/adr/ADR-0015-bounded-public-multigeneration-run.md`
- `docs/adr/ADR-0016-layered-source-to-source-c-optimizer.md`
- `docs/adr/ADR-0017-explicit-public-termination-reason.md`
- `docs/adr/ADR-0018-bounded-generation-statistics.md`
- `docs/adr/ADR-0019-read-only-generation-observer.md`
- `docs/adr/ADR-0020-deterministic-application-requested-stopping.md`
- `docs/adr/ADR-0021-versioned-fitness-comparison-policy.md`
- `docs/adr/ADR-0022-bounded-deterministic-diversity.md`
- `docs/adr/ADR-0023-deterministic-convergence-and-stagnation.md`
- `docs/adr/ADR-0024-generalized-deterministic-elite-preservation.md`
- `docs/adr/ADR-0025-stable-rank-based-parent-selection.md`
- `docs/adr/ADR-0026-human-readable-abstraction-and-audit-projection.md`
- `docs/adr/ADR-0027-reference-byte-genome-operators.md`
- `docs/adr/ADR-0028-evidence-driven-adaptive-mutation.md`
- `docs/adr/ADR-0029-opt-in-secure-erasure-lifecycle.md`
- `docs/adr/ADR-0030-versioned-checkpoint-and-deterministic-resume.md`
- `docs/adr/ADR-0031-deterministic-population-storage-recycling.md`
- `docs/adr/ADR-0032-deterministic-bounded-parallel-evaluation.md`
- `docs/adr/ADR-0033-reproducible-core-benchmark-evidence.md`
- `docs/adr/ADR-0034-reference-consumer-adapters.md`
- `docs/adr/ADR-0035-immutable-project-ingestion-and-baselines.md`
- `docs/adr/ADR-0036-clang-llvm-analysis-and-hotspot-model.md`
- `docs/adr/ADR-0037-versioned-source-transformation-recipes.md`
- `docs/adr/ADR-0038-ast-aware-c-transformation-catalogue.md`
- `docs/architecture.md`
- `docs/algorithms.md`
- `docs/benchmarks.md`
- `docs/engineering/AES-DEV-001-development-principles.md`
- `docs/engineering/reports/EVO-HRA-001-human-readable-abstraction-audit.md`
- `docs/engineering/reports/EVO-HRA-002-checkpoint-audit.md`
- `docs/engineering/reports/EVO-HRA-003-population-storage-recycling-audit.md`
- `docs/engineering/reports/EVO-HRA-004-parallel-evaluation-audit.md`
- `docs/engineering/reports/EVO-HRA-005-core-benchmark-evidence-audit.md`
- `docs/engineering/reports/EVO-HRA-006-reference-adapter-audit.md`
- `docs/engineering/reports/EVO-HRA-007-project-ingestion-audit.md`
- `docs/engineering/reports/EVO-HRA-008-project-analysis-audit.md`
- `docs/engineering/reports/EVO-HRA-009-project-recipe-audit.md`
- `docs/engineering/reports/EVO-HRA-010-c-transformation-catalogue-audit.md`
- `docs/engineering/SECURE-C-CXX.md`
- `docs/engineering/AES-SEC-001-review-dispositions.json`
- `https://github.com/dlworrell/evo/issues/4`
- `https://github.com/dlworrell/evo/issues/6`
- `https://github.com/dlworrell/evo/issues/8`
- `https://github.com/dlworrell/evo/issues/12`
- `https://github.com/dlworrell/evo/issues/16`
- `https://github.com/dlworrell/evo/issues/18`
- `https://github.com/dlworrell/evo/issues/20`
- `https://github.com/dlworrell/evo/issues/22`
- `https://github.com/dlworrell/evo/issues/24`
- `https://github.com/dlworrell/evo/issues/26`
- `https://github.com/dlworrell/evo/issues/28`
- `https://github.com/dlworrell/evo/issues/30`
- `https://github.com/dlworrell/evo/issues/32`
- `https://github.com/dlworrell/evo/issues/34`
- `https://github.com/dlworrell/evo/issues/36`
- `https://github.com/dlworrell/evo/issues/39`
- `https://github.com/dlworrell/evo/issues/40`
- `https://github.com/dlworrell/evo/issues/41`
- `https://github.com/dlworrell/evo/issues/42`
- `https://github.com/dlworrell/evo/issues/43`
- `https://github.com/dlworrell/evo/issues/44`
- `https://github.com/dlworrell/evo/issues/45`
- `https://github.com/dlworrell/evo/issues/46`
- `https://github.com/dlworrell/evo/issues/47`
- `https://github.com/dlworrell/evo/issues/48`
- `https://github.com/dlworrell/evo/issues/49`
- `https://github.com/dlworrell/evo/issues/50`
- `https://github.com/dlworrell/evo/issues/51`
- `https://github.com/dlworrell/evo/issues/52`
- `https://github.com/dlworrell/evo/issues/53`
- `https://github.com/dlworrell/evo/issues/54`
- `https://github.com/dlworrell/evo/issues/55`
- `https://github.com/dlworrell/evo/issues/60`
- `https://github.com/dlworrell/evo/issues/61`
- `https://github.com/dlworrell/AEMS/issues/18`
