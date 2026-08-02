# EVO-001: Evolutionary Optimization Library Contract

Status: Baseline
Version: 0.16.0
Owner: EVO

## Scope Boundary

This specification governs the reusable deterministic C17 evolutionary-search
core implemented through version 0.16.0. It does not define C-project
ingestion, Clang/LLVM analysis, structured source transformations, isolated
candidate builds, baseline-versus-candidate measurement, optimized patches, or
product-level replay artifacts.

Those source-to-source product responsibilities are defined separately by the
draft 1.0 target in `EVO-002-source-optimizer-contract.md`. An EVO-001
conforming run or compiler-option adapter alone must not be described as an
optimized C codebase.

## Purpose

EVO provides a reusable C17 interface for deterministic, bounded evolutionary
optimization. Consumers define genomes and problem-specific operations through
callbacks; EVO owns orchestration, reproducibility, and evidence without
embedding consumer policy in the library.

## Public Interface

The public API is declared in `include/catalyst/evo/evo.h`.

### Problem definition

`evo_problem_t` declares:

- the byte size of one genome;
- initialization, mutation, crossover, and evaluation callbacks;
- an optional validity callback; and
- an opaque consumer context passed to callbacks.

The consumer owns callback code and context lifetime. Callback behavior must be
deterministic for a fixed input, context, and random stream unless the consumer
records an additional source of variation.

### Run configuration

`evo_config_t` records population size, generation limit, tournament size,
crossover rate, mutation rate, random seed, `max_genome_bytes`,
`max_population_bytes`, `max_evaluation_bytes`, and
`max_child_population_bytes`.

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

`max_child_population_bytes` independently bounds one private child-population
genome slab. EVO checks the same `population_size * genome_size` arithmetic
before allocating this second slab. It is required when `generation_limit` is
positive and is unused when the limit is zero. The field does not authorize
child evaluation records, operator scratch space, checkpoints, or an aggregate
run working set.

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
6. Destruction releases the slab and resets every population field to zero.
   It is null-safe and repeatable for initialized objects.

Population destruction does not securely erase the genome slab. The same
secret-material restriction defined for `evo_result_destroy` applies to
population storage and every non-owning genome view.

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
6. Every field in the returned `evo_fitness_t` must be finite. NaN or infinity
   returns `EVO_ERROR_EVALUATION`, releases provisional records, and preserves
   the initialized population as unevaluated.
7. Validity is a hard gate. Higher consumer-computed `fitness.total` wins, and
   the lower population index wins an exact tie. EVO records but does not
   independently rank the other fitness components.
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

### Private deterministic tournament selection

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
7. Sampling is with replacement. Higher `fitness.total` wins, and an exact tie
   selects the lower population index.
8. The operator performs no allocation and never changes population storage or
   evaluation evidence. It assigns the output index only after every draw
   succeeds.

The caller supplies an already seeded private RNG stream. Version 0.7.0 does
not derive a selection seed, split initialization streams, or alter RNG
algorithm version 1. Version 0.11.0 separately defines pair-local selection-
stream ownership, and version 0.16.0 invokes that composition from `evo_run`.

### Private deterministic crossover dispatch

Version 0.8.0 adds a private representation-neutral crossover boundary. The
normative decision is recorded in
`docs/adr/ADR-0007-deterministic-crossover-dispatch.md`.

The crossover lifecycle is:

1. The problem, configuration, seeded RNG, two parent views, and two child
   views must be non-null.
2. `problem->genome_size` must be nonzero and no greater than
   `config->max_genome_bytes`.
3. `config->crossover_rate` must be finite and in the inclusive range
   `[0, 1]`.
4. The two child pointers must be distinct and may not exactly equal either
   parent pointer. The private caller must additionally provide complete,
   non-overlapping child spans that do not partially overlap a parent.
5. Every preceding check occurs before RNG consumption or child output. Null
   and exact-alias errors return `EVO_ERROR_INVALID_ARGUMENT`; invalid genome
   policy or rate returns `EVO_ERROR_RESOURCE_LIMIT`; unseeded state returns
   `EVO_ERROR_STATE`.
6. Every successful pair consumes exactly one probability-event word,
   including rate endpoints and missing-callback operation.
7. When the event is selected and `problem->crossover` is non-null, EVO invokes
   that callback exactly once with two read-only parents, two writable
   children, and the consumer context.
8. When the event is not selected or the callback is null, EVO clones parent A
   to child A and parent B to child B for exactly `genome_size` bytes.
9. The callback must fully initialize both children, preserve parent bytes and
   ownership, retain no view, and remain deterministic for fixed parents and
   context. The callback returns no status, so EVO cannot roll back a consumer
   contract violation.

The operator performs no allocation, does not select parents, and does not own
child storage. Version 0.8.0 does not implement a next-generation population or
a generation transition. Version 0.11.0 defines pair-local crossover stream
derivation, and version 0.16.0 invokes the complete composition from `evo_run`.

### Private deterministic mutation dispatch

Version 0.9.0 adds a private representation-neutral mutation boundary. The
normative decision is recorded in
`docs/adr/ADR-0008-deterministic-mutation-dispatch.md`.

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
   rate endpoints and missing-callback operation.
6. When the event is selected and `problem->mutate` is non-null, EVO invokes
   that callback exactly once with the writable genome, the configured rate,
   and consumer context.
7. When the event is not selected or the callback is null, EVO leaves the
   genome unchanged.
8. EVO owns `mutation_rate` as the per-genome event probability and forwards
   the same value unchanged as the callback's representation-specific
   mutation intensity.
9. The callback must mutate only the complete supplied span, preserve
   ownership, retain no view, use no unrecorded entropy, and remain
   deterministic for fixed genome bytes, rate, and context. The callback
   returns no status, so EVO cannot roll back a consumer contract violation.

The operator performs no allocation and does not own genome storage. Version
0.9.0 does not implement built-in representation-specific mutation helpers,
adaptive mutation, a next-generation population, or a generation transition.
Version 0.11.0 defines child-indexed mutation stream derivation, and version
0.16.0 invokes the complete composition from `evo_run`.

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
   private validator used by tournament selection.
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
2. `population_size` and `tournament_size` must be nonzero, and tournament size
   must not exceed population size; otherwise planning returns
   `EVO_ERROR_RESOURCE_LIMIT`.
3. Exactly `floor(population_size / 2)` complete pair ordinals are valid. An
   out-of-range ordinal returns `EVO_ERROR_RESOURCE_LIMIT` and preserves the
   output.
4. EVO proves the completed parent storage and evaluation evidence through the
   shared population validator. Inconsistent evidence returns
   `EVO_ERROR_STATE`; a consistent all-invalid parent returns
   `EVO_ERROR_NO_VALID_CANDIDATE`.
5. Pair ordinal `i` maps to child indexes `2i` and `2i + 1`. These arithmetic
   results are in range by construction.
6. EVO derives one selection-domain schedule-version-1 stream from the
   configured master seed, source generation, and pair ordinal.
7. Two deterministic tournaments execute sequentially on the pair-local
   stream. Sampling remains with replacement, so both outputs may identify the
   same parent.
8. The plan records both parent indexes, both child indexes, pair ordinal,
   source generation, and seed-schedule version only after both tournaments
   succeed.
9. Every rejection preserves the output object and the complete parent
   population.
10. For an odd population, the final child index is not part of a complete
    pair. A later singleton or elitism policy owns it.

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
2. Genome dimensions, child allocation size, child budget, tournament policy,
   crossover rate, and mutation rate must be internally consistent.
3. Child evaluation, generation-zero initialization, validity, fitness, and
   best-candidate evidence must remain empty.
4. The child records a contiguous `produced_count`. Pair `i` is accepted only
   when `produced_count == 2i`; repeated or skipped pairs return
   `EVO_ERROR_STATE` unchanged.
5. The first successful pair records the supplied source generation and
   operator seed-schedule version 1. Every later pair must match both values.
6. EVO invokes the complete parent-pair planner and preserves its valid-only,
   tournament-with-replacement semantics.
7. EVO derives the crossover-domain stream from the pair ordinal and derives
   one mutation-domain stream from each child index.
8. EVO resolves the two parent and two child views and completes every
   expected fallible library check before callback dispatch or child output.
9. The valid suffix invokes the crossover dispatcher once, then the mutation
   dispatcher once for each child. Given the completed preflight and seeded
   streams, this suffix contains no expected library rejection.
10. Success commits both child genomes, advances `produced_count` by exactly
    two, records source generation and schedule version, and commits output
    evidence with RNG algorithm version 1.
11. Rejection before callback dispatch preserves the child bytes, child
    metadata, output evidence, and complete parent population.
12. Consumer callbacks retain their existing bounded deterministic contract
    and no failure channel. EVO cannot roll back consumer-context side effects
    or recover from a callback contract violation.
13. For an odd population, production stops after the last complete pair and
    leaves the trailing child untouched.

Production metadata is not completed-population evidence. The child remains
uninitialized, unevaluated, and ineligible for selection. Odd-slot policy,
child evaluation, population swapping, generation advancement, and public
`evo_run` integration are not implemented by this boundary.

### Private deterministic odd-tail elite cloning

Version 0.13.0 adds a private odd-population completion boundary. The
normative decision is recorded in
`docs/adr/ADR-0012-deterministic-odd-tail-elite-cloning.md`.

The odd-tail lifecycle is:

1. Problem, configuration, completed parents, active child storage, and output
   evidence must be non-null and separately owned.
2. The configured population must be odd and nonzero, dimensions must match,
   and the genome and child-slab budgets must authorize the existing storage.
3. Parent evaluation evidence must be structurally complete and contain a
   stable best valid candidate.
4. Child initialization, evaluation, validity, fitness, and best-candidate
   evidence must remain empty.
5. For populations larger than one, `produced_count` must equal
   `population_size - 1` and the pair prefix must match the supplied source
   generation and operator seed-schedule version 1. A one-member population
   accepts the corresponding zero-pair empty metadata.
6. EVO resolves bounded distinct views of the recorded best parent and final
   child before writing any byte.
7. Policy version 1 copies exactly `genome_size` bytes from the stable best
   parent into child index `population_size - 1`.
8. The operation consumes no RNG word and invokes no consumer callback.
9. Success records `produced_count == population_size`, source generation,
   operator schedule version 1, odd-tail policy version 1, and output evidence.
10. Every rejection preserves parent state, child bytes and metadata, and
    output evidence. Repeated completion and later pair production reject.

The one elite clone is the complete odd-tail policy, not a generalized elitism
contract. Full production metadata alone does not make the child initialized,
evaluated, or selectable. Version 0.14.0 adds evaluation as the next distinct
boundary; swapping, generation advancement, and public `evo_run` integration
remain unimplemented.

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
   source generation, and operator seed-schedule version 1.
4. Odd populations must record odd-tail policy version 1. Even populations
   must record no odd-tail policy. This includes the defined one-member case.
5. Generation-zero initialization evidence, RNG initialization evidence,
   evaluation records, valid count, and best-candidate evidence must remain
   empty before evaluation.
6. EVO allocates zero-initialized provisional candidate records within
   `max_evaluation_bytes`. Allocation failure leaves the child unchanged.
7. The optional validator visits every candidate in ascending index order. A
   missing validator means every candidate is valid.
8. Only valid candidates are passed to the evaluator, also in ascending index
   order. Invalid candidates retain zero fitness and an unevaluated record.
9. All seven returned fitness fields must be finite. A non-finite value
   releases every provisional record and preserves the child object and output
   evidence, although callback-context side effects cannot be rolled back.
10. Higher `fitness.total` wins. Exact ties retain the lower candidate index.
11. Success commits the complete record allocation, valid count, stable best
    evidence, and evaluated state while preserving every genome byte and all
    production provenance. An all-invalid child completes with no best.
12. Output evidence records population and evaluation sizes, valid count,
    stable best, source generation, production-policy versions, child-
    evaluation policy version 1, and completion.
13. Repeated evaluation and every malformed, incomplete, mismatched, resource,
    allocation, or detectable callback-output failure preserve caller-owned
    evidence and library-owned child state.

The operation consumes no RNG word and invokes no initialization, selection,
crossover, or mutation callback. The shared completed-population validator
accepts either generation-zero or evaluated-child provenance. An evaluated
child can therefore authorize tournament selection and the next independently
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
    state, previous generation, completed generation, production-policy
    versions, and generation-advancement policy version 1.
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
recycling, generalized elitism, checkpoint persistence, and public `evo_run`
iteration are not part of this operation.

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
   one require valid tournament size and finite crossover and mutation rates in
   `[0, 1]`. A one-member population uses odd-tail cloning directly and does not
   require unused pair policy.
3. EVO constructs, initializes, and evaluates generation zero and transfers its
   stable valid winner into one independent result allocation.
4. For each source generation in ascending order, EVO creates one child slab,
   produces all complete pairs, completes an odd tail when required, evaluates
   the complete child, and resolves strict global-best improvement.
5. EVO atomically promotes the evaluated child before changing the result.
   Promotion increments the completed-transition count and releases the former
   parent.
6. A later candidate replaces the existing result bytes and complete fitness
   only when its `fitness.total` is strictly greater. Exact cross-generation
   ties retain the earlier winner.
7. A promoted all-invalid child terminates the loop successfully and retains
   the earlier valid winner. Its promotion is included in
   `generations_completed`.
8. Any other failure destroys every current internal owner and the result
   allocation, returning the inactive result to its complete zero state. No
   partial public progress is retained.

The result allocation is created once and is not reallocated during a run.
Bounded-run policy evidence is private. Version 0.16.0 does not define
convergence, stagnation, application stop or observer callbacks, generalized
elitism, adaptive mutation, old-slab recycling, checkpointing, parallelism,
secure erasure, or a public termination-reason field.

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
4. Callers may use bounded, non-owning aliases to read or write genome bytes
   while the result remains alive. An alias may not free or reallocate the
   storage and must not survive result destruction.
5. `evo_result_destroy` releases the owned allocation and resets every result
   field to zero. Destruction is null-safe and repeatable for initialized
   result objects.
6. A destroyed result may be passed to `evo_run` again immediately.

`evo_result_destroy` does not securely erase genome bytes. Consumers must not
place secret or cryptographic material in genomes without a separately
reviewed erasure boundary.

### Status values

`evo_status_t` defines:

| Status | Meaning |
|---|---|
| `EVO_SUCCESS` | Generation zero produced a valid winner and every requested transition completed, or a promoted later all-invalid child ended the run early. |
| `EVO_ERROR_INVALID_ARGUMENT` | A required pointer argument is null. |
| `EVO_ERROR_OUT_OF_MEMORY` | The system allocator returned null. |
| `EVO_ERROR_RESULT_ACTIVE` | The result already owns a genome and is preserved unchanged. |
| `EVO_ERROR_RESOURCE_LIMIT` | A required size is zero, arithmetic overflows, or a caller budget is exceeded. |
| `EVO_ERROR_STATE` | A private lifecycle operation received inactive, initialized, or inconsistent state. |
| `EVO_ERROR_EVALUATION` | A fitness callback returned a non-finite component. |
| `EVO_ERROR_NO_VALID_CANDIDATE` | Generation-zero evaluation completed, but every candidate was invalid, so no public winner exists. |

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

## Current 0.16.0 Conformance Boundary

The current implementation exposes generation-zero compatibility plus bounded
multi-generation execution:

- validation enforces required pointers, an evaluator, an inactive result, and
  the three generation-zero memory budgets;
- successful execution constructs, initializes, validates, and evaluates a
  private population in deterministic order;
- invalid candidates are never evaluated;
- finite consumer totals select a stable winner with lower-index tie-breaking;
- success transfers one independent global-best genome copy and complete
  fitness evidence;
- all-invalid completion has a distinct public status;
- allocation, resource, state, and evaluation failures return an empty result
  after complete private cleanup;
- result destruction is null-safe, repeatable, and restores the empty state;
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
- private validation and evaluation run in deterministic ascending passes;
- private tournament selection samples valid evaluated candidates with
  replacement and deterministic tie handling;
- private crossover dispatch consumes a fixed one-word probability decision,
  invokes the representation-aware callback or clones parents, and preserves
  child output on precondition failure;
- private mutation dispatch consumes a fixed one-word probability decision,
  invokes the representation-aware callback or preserves the genome, and
  preserves genome output on precondition failure;
- private child-population creation validates completed parent evidence,
  enforces a separate checked budget, and creates independently owned empty
  output storage;
- private complete-pair planning derives a pair-local selection stream, runs
  two tournaments with replacement, maps consecutive child slots, and
  preserves output and parent evidence on rejection;
- private complete-pair production preflights the combined boundary, derives
  pair- and child-indexed operator streams, dispatches crossover and both
  mutations, records a contiguous child prefix, and preserves parent evidence;
- private odd-tail completion requires the complete pair prefix, clones the
  stable best valid parent without RNG or callbacks, records policy version 1,
  and preserves every object on rejection;
- private produced-child evaluation accepts complete even and odd production
  provenance, commits deterministic valid-only finite-fitness evidence, and
  promotes the child to shared completed-population authority;
- private generation advancement validates current/child lineage and all
  ownership ranges, moves the evaluated child into the parent handle, empties
  the child handle, releases the former parent, and records the next generation
  without allocation, copying, RNG, or callbacks;
- public bounded execution validates transition-only policy before callbacks,
  runs ascending transitions, allocates the result once, retains earlier exact
  ties, counts completed promotions, and stops successfully after promoting a
  later all-invalid child; and
- generalized elitism, adaptive mutation, diversity, convergence, stagnation,
  application stopping and observation, checkpointing, buffer recycling,
  parallelism, and public termination-reason evidence are not implemented.

Consumers may treat `EVO_SUCCESS` as evidence of a valid global winner and
exactly `generations_completed` promoted child generations. A value below a
positive configured limit identifies later all-invalid termination in version
0.16.0; no other public early-stop reason exists.

## Verification

The baseline verification set is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The portable result-lifecycle test proves public callback order, invalid
suppression, winner transfer, complete fitness evidence, stable ties,
all-invalid mapping, non-finite rejection, active-result preservation, and
reuse. Population-storage, RNG-vector, population-initialization, and
population-evaluation tests continue to verify each private phase
independently. The selection test proves completed-state validation, tournament
bounds, all-invalid handling, valid-only sampling, fixed replay, exact ties,
failure preservation, and single-draw behavior. A separate Linux-only
static-link test uses the GNU-compatible `--wrap=calloc` linker facility to
prove failure and cleanup at the population, evaluation-record, and
result-transfer allocations.

The crossover test proves pointer and policy validation, exact alias rejection,
rate endpoints, exact RNG consumption, callback and identity-clone paths,
parent preservation, output preservation on rejection, and deterministic
replay. RNG tests separately lock probability thresholds and vectors.

The mutation test proves pointer and policy validation, rate endpoints, exact
RNG consumption, callback and no-op paths, rate and context forwarding, genome
preservation on rejection, and deterministic replay.

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
source-generation separation, callback and identity-clone paths, odd-tail
preservation, parent immutability, and rejection of repeated or skipped pairs.

The child-tail test proves complete-prefix enforcement, stable-best and tie
behavior, one-member completion, byte-and-evidence replay, absence of extra RNG
or callbacks, parent and prefix preservation, all-invalid and alias rejection,
and terminal policy metadata.

The child-evaluation test proves even, odd, and one-member production
provenance; deterministic validation-before-evaluation ordering; invalid-
candidate suppression; stable ties; all-invalid completion; byte, record, and
evidence replay; budget and non-finite-fitness rollback; repeated-evaluation
rejection; completed-population validation; and next-child authorization. The
allocation-failure test separately proves that a failed child-evaluation record
allocation preserves the fully produced child unchanged.

The generation-advancement test proves generation-zero and later-generation
lineage, allocation-identity and byte preservation, all-invalid promotion,
empty-child reuse, overflow, object and owned-range alias rejection, malformed
state preservation, and repeated-call rejection. The wrapped-allocation and
release test also proves that advancement succeeds while the next allocator
call is forced to fail and releases exactly the two former-parent allocations,
confirming the transition's allocation-free single-owner contract.

The bounded-run test proves zero-limit compatibility; positive-limit policy
validation before callbacks; even, odd, and one-member execution; deterministic
multi-transition replay; strict global improvement; earlier-winner exact ties;
later all-invalid promotion and successful stop; generation-zero all-invalid
mapping; active-result preservation; and versioned private run evidence. The
wrapped-allocation test additionally proves the five-allocation bounded path,
exact successful cleanup, and empty public failure after child-slab or child-
evaluation allocation failure.

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
- `docs/architecture.md`
- `docs/algorithms.md`
- `docs/benchmarks.md`
- `docs/engineering/AES-DEV-001-development-principles.md`
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
- `https://github.com/dlworrell/AEMS/issues/18`
