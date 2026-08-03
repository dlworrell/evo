# EVO Algorithms and Source-Evolution Roadmap

This document distinguishes algorithms implemented by the reusable
`catalyst_evo` core from the structured program transformations and evaluation
algorithm required by the EVO 1.0 source optimizer. Version 0.20.0 implements
only the core boundary described below.

## EVO Core Initial Release

- Tournament selection
- Rank-based selection
- Elitism
- One-point crossover
- Two-point crossover
- Uniform crossover
- Bit, integer, floating-point, and permutation mutation helpers
- Adaptive mutation
- Diversity measurement
- Stagnation detection
- Constraint penalties

## Later EVO Core Releases

- Evolution strategies
- Differential evolution
- CMA-ES
- Simulated annealing
- Particle swarm optimization
- NSGA-II and multi-objective optimization
- Niching, crowding, and fitness sharing
- Parallel and distributed evaluation

Algorithms must expose deterministic behavior under a recorded random seed and preserve sufficient evidence to reproduce a run.

## Deterministic Random Stream

EVO 0.4.0 defines private RNG algorithm version 1 as PCG-XSH-RR with 64-bit
state and 32-bit output. Its multiplier, fixed stream increment, seed
procedure, output transform, and least-significant-byte-first emission order
are normative in
`docs/adr/ADR-0002-deterministic-rng-and-population-initialization.md`.

The stream uses only unsigned fixed-width arithmetic. It has no mutable global
state and consumes no clock, process, operating-system, or hardware entropy.
Every `uint64_t` seed is valid. Fixed vectors in `tests/rng_test.c` prevent an
implementation change from silently altering reproducibility.

Version 0.7.0 adds unbiased bounded-index sampling. One sample combines two
successive 32-bit outputs into an explicit low-word/high-word 64-bit value,
rejects the modulo-bias prefix, and reduces an accepted value by the bound.
Fixed vectors cover normal and rejection paths, exact stream consumption, and
replay.

Version 0.8.0 adds deterministic probability events. A finite probability in
`[0, 1]` is quantized to `floor(probability * 2^32)` successful 32-bit values.
Every successful decision consumes exactly one word, including probabilities
zero and one. Invalid input preserves the stream and output.

This stream is designed for repeatable engineering search. It is not
cryptographically secure and must not generate secrets, keys, nonces, or
authentication material.

## Seed-Schedule Research

EVO-RNG-001 compared the version-1 baseline with a plain tuple-mixed control,
a Code Noodling-derived prime-indexed schedule, and a portable finite-field
elliptic schedule. The prime and elliptic candidates did not improve the
measured separation over the simpler control, and the elliptic candidate added
approximately 600-fold derivation cost.

EVO preserves RNG algorithm version 1 and does not link the prime or elliptic
research schedules into the production library. Version 0.11.0 supplies the
required operator-consumption model and promotes only the plain tuple-mixed
control as a separately versioned production schedule. See
`docs/adr/ADR-0003-prime-and-elliptic-seed-schedules.md` and ADR-0010.

## Operator Seed Schedule

Operator seed-schedule version 1 derives an independently addressable PCG
stream from:

```text
(master_seed, source_generation, population_index, operation_domain)
```

Selection and crossover own pair ordinals as tuple indexes; mutation owns child
indexes. Stable domains separate selection, crossover, and mutation. The
unsigned fixed-width tuple mix and fixed schedule vectors are normative. Pair-
local derivation prevents rejection sampling or changed operator consumption
in one pair from shifting a later pair's stream.

This schedule does not alter generation-zero initialization, add entropy, or
make PCG cryptographically secure.

## Generation-Zero Initialization

The version 0.4.0 private population initializer consumes one continuous
version-1 stream to fill the entire genome slab. It then invokes the optional
consumer initializer once per genome in ascending index order.

Callbacks receive deterministic prefilled bytes and may perform bounded,
deterministic domain transformations. A callback that consults unrecorded
entropy, writes outside its genome, changes ownership, or retains the genome
view violates the EVO contract.

Initialization does not validate or evaluate candidates. Those phases remain
separate algorithm boundaries.

## Generation-Zero Validation and Evaluation

Version 0.5.0 implements validation and evaluation as two deterministic passes
over the initialized population. The optional `is_valid` callback runs once
per genome in ascending index order. A missing validator accepts every
candidate. The `evaluate` callback then runs once for each valid candidate in
ascending order; invalid candidates are never evaluated.

All seven returned fitness components must be finite. `fitness.total` is the
consumer-computed scalar objective, and EVO selects the greatest total. Exact
ties preserve the lower population index. The remaining components are
recorded for evidence but receive no library-defined ordering.

An all-invalid population completes evaluation without a winner. Evaluation
records are bounded by `max_evaluation_bytes`, and failures discard provisional
records while preserving the initialized genome slab within the private phase.

## Public Generation-Zero Execution

Version 0.6.0 composes population construction, deterministic initialization,
validation, evaluation, and stable winner transfer inside `evo_run`.

The public result receives an independent copy of the lower-index
highest-total valid candidate and all seven fields returned by its evaluator.
The private population slab and evaluation records are always released before
the call returns. If every candidate is invalid, the private phase still
completes successfully, but the public run returns
`EVO_ERROR_NO_VALID_CANDIDATE` with an empty result because there is no asset
whose ownership can be transferred.

In version 0.6.0 this boundary performed no parent selection, crossover,
mutation, elitism, diversity processing, or generation transition. A
successful zero-limit call still records `generations_completed == 0`;
version 0.16.0 composes later private boundaries when the limit is positive.

## Deterministic Tournament Selection

Version 0.7.0 implements tournament selection as a private operator over a
completed evaluation population.

- `tournament_size` is in `1..population_size`.
- Draws are with replacement.
- Only valid, evaluated candidates participate.
- A valid-candidate ordinal is sampled without modulo bias and mapped to the
  corresponding ascending population index.
- Higher `fitness.total` wins; the lower population index wins an exact tie.
- Completed all-invalid input returns `EVO_ERROR_NO_VALID_CANDIDATE`.
- Invalid lifecycle or evidence returns `EVO_ERROR_STATE`.
- The population and output are unchanged on failure, and validation consumes
  no RNG state.

The caller supplies the seeded private stream. Version 0.11.0 derives that
stream for complete parent pairs while keeping selection semantics independent.
The operator performs no crossover, mutation, elitism, or generation
advancement itself. Version 0.16.0 invokes it through complete-pair production
from `evo_run`.

## Deterministic Crossover Dispatch

Version 0.8.0 implements a private representation-neutral crossover pair
operator. Two read-only parent genome views and two distinct, non-overlapping
child views are supplied by the private caller.

- `genome_size` must be nonzero and within `max_genome_bytes`.
- `crossover_rate` must be finite and in `[0, 1]`.
- One probability decision consumes exactly one RNG word per successful pair.
- A selected event invokes the consumer callback exactly once when present.
- A non-selected event or missing callback clones each parent to its
  corresponding child for exactly `genome_size` bytes.
- Precondition failures preserve the RNG and child outputs.
- The callback must fully initialize both children, preserve parents and
  ownership, retain no view, and remain deterministic for fixed inputs and
  context.

The operator does not choose parents, allocate a child population, mutate
children, or execute a generation transition. Version 0.12.0 composes it with
those separate private boundaries for complete pairs. Representation-specific
one-point, two-point, and uniform helpers remain later algorithm-library work.

## Deterministic Mutation Dispatch

Version 0.9.0 implements a private representation-neutral mutation operator
over one bounded writable genome.

- `genome_size` must be nonzero and within `max_genome_bytes`.
- `mutation_rate` must be finite and in `[0, 1]`.
- One probability decision consumes exactly one RNG word per valid attempt,
  including rates zero and one and an absent callback.
- A selected event invokes the consumer mutation callback exactly once when
  present.
- A non-selected event or absent callback leaves the genome unchanged.
- Precondition failures preserve the RNG and genome bytes.
- The callback receives `mutation_rate` unchanged as its representation-
  specific intensity and must remain deterministic for fixed genome bytes,
  rate, and context.
- The callback owns no storage, retains no genome view, uses no unrecorded
  entropy, and has no failure or rollback channel.

The operator does not allocate a child population, define built-in bit,
integer, floating-point, or permutation mutations, adapt the rate, or execute
a generation transition. Version 0.12.0 composes the dispatcher for complete
child pairs; the other capabilities remain later work.

## Child-Population Storage

Version 0.10.0 adds the private storage boundary required before the existing
operators can be composed.

- A source population must have structurally consistent completed evaluation
  evidence.
- The child population has the same population and genome dimensions.
- Checked multiplication proves the child slab size before allocation.
- `max_child_population_bytes` independently authorizes the child slab.
- Child bytes are zero-initialized and child evaluation records are absent.
- Parent genomes, evaluations, and lifecycle evidence remain unchanged.
- Parent and child storage have independent ownership and destruction.
- An all-invalid but structurally complete parent may still allocate storage;
  selection policy remains a later orchestration decision.

This boundary does not select or pair parents, invoke crossover or mutation,
mark children complete, evaluate children, swap populations, derive operator
streams, or advance a generation.

## Complete Parent-Pair Planning

Version 0.11.0 adds a read-only private planner over completed parent evidence.

- Exactly `population_size / 2` complete pairs are addressable.
- Pair `i` owns child indexes `2i` and `2i + 1`.
- The selection stream is derived from the master seed, source generation,
  pair ordinal, and selection domain.
- Two tournaments run sequentially on that pair-local stream, with replacement.
- Both parents must be selected before the output plan is committed.
- The plan records pair ordinal, source generation, and seed-schedule version.
- Null, policy, lifecycle, all-invalid, and pair-bound failures preserve parent
  evidence and the output object.
- An odd trailing child index is outside complete-pair planning and remains
  reserved for later singleton or elitism policy.

The planner does not accept child pointers, write genome bytes, invoke
crossover or mutation, mark child storage complete, or advance a generation.

## Deterministic Complete-Pair Child Production

Version 0.12.0 produces one complete child pair from the accepted private
boundaries.

- Pairs are accepted in ascending order; pair `i` requires a committed child
  prefix of exactly `2i` genomes.
- The parent plan supplies two valid parent indexes and child indexes `2i` and
  `2i + 1`.
- Crossover derives its stream from the pair ordinal and crossover domain.
- Mutation derives one independent stream from each child index and the
  mutation domain.
- Every lifecycle, rate, budget, stream, and bounded-view check completes
  before callback dispatch or child output.
- Crossover executes once, followed by one mutation decision for child A and
  one for child B.
- Success records produced count, source generation, schedule version, and
  pair evidence.
- Repeated, skipped, mismatched-generation, and invalid requests preserve
  child bytes and output evidence before callback dispatch.
- Parent genomes and completed fitness evidence remain unchanged.

Consumer callbacks return no status. After complete preflight, the valid
dispatch suffix has no expected library rejection, but callback side effects
and callback contract violations cannot be rolled back.

For an odd population, complete-pair production stops before the final child.
The child remains unevaluated and is not a completed population. Odd-slot
policy, evaluation, swapping, and generation advancement remain separate.

## Deterministic Odd-Tail Elite Cloning

Version 0.13.0 completes the single trailing slot of an odd child population
after every complete pair has been committed.

- Policy version 1 clones the completed parent's stable best valid genome into
  child index `population_size - 1`.
- The accepted pair prefix is exactly `population_size - 1` children and must
  match the supplied source generation and operator schedule version 1.
- A one-member population has no pair prefix and is completed directly from
  parent index zero when that candidate is valid.
- Exact parent-fitness ties retain the existing lower-index stable-best rule.
- Every lifecycle, ownership, budget, and bounded-view check completes before
  the copy.
- No selection, crossover, mutation, or other consumer callback runs, and no
  RNG word is consumed.
- Success records the full produced count and odd-tail policy version 1.
- Repeated, even-population, all-invalid, aliased, incomplete-prefix, and
  mismatched-generation requests preserve all inputs and output evidence.

Full child production does not imply initialization or evaluation. The child
still has no validity, fitness, or best-candidate evidence and cannot be used
for selection until evaluation succeeds.
General elite counts, swapping, and generation advancement remain separate.

## Deterministic Produced-Child Evaluation

Version 0.14.0 evaluates a fully produced child population without consuming
RNG state or changing any genome byte.

- The child must contain exactly `population_size` produced genomes with
  source-generation and operator-schedule provenance from production.
- Odd populations require odd-tail policy version 1; even populations require
  no odd-tail policy marker.
- Evaluation records are allocated provisionally under
  `max_evaluation_bytes`.
- Validation visits every candidate in ascending index order.
- Only valid candidates are evaluated, also in ascending index order.
- All seven fitness fields must be finite before the record set is committed.
- Higher `fitness.total` wins, with the lower index retained on exact ties.
- Invalid candidates retain zero fitness and are never evaluated.
- An all-invalid child completes without a best candidate.
- Success preserves production metadata and commits evaluation records,
  valid count, stable best, and policy evidence exactly once.
- Preflight, resource, allocation, and non-finite-fitness failures preserve
  child-owned bytes and metadata plus caller-owned evidence. Consumer callback
  side effects after dispatch begins cannot be rolled back.

The shared completed-population validator accepts both generation-zero and
evaluated-child provenance. This makes the evaluated child eligible for
selection and for authorizing the next independent child allocation. It does
not swap population ownership, increment a generation, recycle the prior
parent, or alter public `evo_run`.

## Atomic Generation Advancement

Version 0.15.0 promotes one evaluated child to the next completed generation
through generation-advancement policy version 1:

1. Require non-null problem, configuration, parent, child, and evidence
   objects with non-overlapping object storage.
2. Reject `current_generation == UINT64_MAX` before any state change.
3. Validate problem dimensions and both populations through the common
   completed-population authority.
4. Require all genome and evaluation ownership ranges to be pairwise
   disjoint, and require evidence storage to be outside every owned range.
5. For generation zero, require generation-zero parent provenance. For every
   later generation `g`, require parent source provenance `g - 1`.
6. Require evaluated-child source provenance equal to `g`; the child therefore
   represents completed generation `g + 1`.
7. Construct versioned output evidence before changing ownership.
8. Move the child structure into the parent handle, reset the child handle to
   zero, release the former parent, and commit evidence.

Steps 1 through 7 are fallible and read-only. Step 8 is a no-fail suffix with
no allocation, genome copying, evaluation copying, RNG consumption, or
callback dispatch. Thus a rejected transition preserves both population
objects and evidence exactly. Success preserves the incoming child's owner
identities and every byte while releasing the former owners exactly once.

All-invalid completed children use the same move. Termination, generation-
limit enforcement, old-slab recycling, and public loop integration remain
separate policies.

## Bounded Multi-Generation Execution

Version 0.16.0 adds bounded-run policy version 1 and composes the complete
generation pipeline through public `evo_run`:

1. Interpret `generation_limit` as the requested number of child transitions
   after generation zero. A zero limit executes no transition and ignores
   transition-only configuration.
2. For a positive limit, validate the child slab budget and all operator
   policy before constructing generation zero or invoking a consumer callback.
3. Construct, initialize, validate, and evaluate generation zero. Transfer its
   stable valid winner into one independently owned result allocation.
4. For source generations `g = 0..generation_limit - 1`, allocate one child
   slab and produce complete pairs in ascending pair order.
5. If the population size is odd, complete the trailing child using odd-tail
   policy version 1. A one-member population takes this path directly.
6. Evaluate the complete child and resolve whether its stable best has a
   strictly greater total fitness than the retained global winner.
7. Atomically promote the evaluated child to completed generation `g + 1`.
   Only after promotion may a strict improvement overwrite the existing result
   buffer and fitness evidence.
8. Record the completed transition. If the promoted population is all-invalid,
   stop successfully; otherwise continue until the requested limit is met.

The result buffer is never reallocated during a run. Exact total-fitness ties
across generations retain the earlier winner, so deterministic replay is not
affected by allocation addresses or later equal candidates. Failure at any
transition destroys all internal owners and the result allocation; no partial
winner or completion count escapes the public call.

This algorithm has a bounded sequential working set of one current population,
one child population, one result genome, and the current population's
evaluation records plus provisional child evaluation records during child
evaluation. It does not recycle slabs, run callbacks concurrently, infer
convergence, or infer any stop beyond the two existing conditions.

Version 0.17.0 maps successful completion to explicit result evidence after
all fallible run work succeeds. Exhausting the configured transition bound
records `EVO_TERMINATION_GENERATION_LIMIT`; promoting a later all-invalid
child records `EVO_TERMINATION_ALL_INVALID`. Failure and destruction retain
the zero-valued `EVO_TERMINATION_NONE`. This mapping consumes no RNG, invokes
no callback, and changes no generation transition or winner.

## Deterministic Generation Statistics

Version 0.18.0 computes schema-version-1 statistics for generation zero and
every successfully promoted child. The engine visits candidate records once in
ascending index and completely skips invalid fitness payloads. For each valid
candidate it adds the seven finite `evo_fitness_t` components to seven `double`
accumulators in that exact order. No reassociation, compensation, weighting,
averaging, normalization, or parallel reduction is permitted by policy version
1. A non-finite valid component or intermediate sum rejects with
`EVO_ERROR_EVALUATION`.

The record copies the stable generation-local best already established by
evaluation; it never reranks candidates or changes the global result winner.
It also records the generation index and exact population, valid, and invalid
counts. An all-invalid generation has `has_best == false`, best index zero,
zero best fitness, and zero component sums.

Only the most recently committed record is retained in `evo_result_t`. A child
record is computed before promotion but replaces the prior record only after
promotion succeeds. This constant-space rule records terminal population state
without allocating history proportional to `generation_limit`.

## Synchronous Generation Observation

Version 0.19.0 delivers the committed statistics stream without changing that
working-set bound. If configured, the observer receives generation zero and
then each promoted child in strictly ascending generation order. EVO constructs
fresh stack snapshots for the result view and statistics, invokes the callback
synchronously, and discards those snapshots when it returns.

For generation `g`, delivery occurs only after `g` is committed, its statistics
replace the previous record, and any strict global winner improvement is
copied. The observer therefore sees the global best-so-far together with the
generation-local record. Delivery also follows stop classification: continuing
events use `EVO_TERMINATION_NONE`, while the final event identifies generation-
limit, all-invalid, or application-requested termination.

The observer has no return value. Its invocation cannot add a stop decision,
retry a failed generation, or change winner selection. A provisional child
that fails evaluation, statistics, or promotion emits no event. No observer
delivery allocates, consumes RNG state, runs concurrently, or retains history.

## Deterministic Application-Requested Stopping

Version 0.20.0 adds one synchronous decision point to the committed-generation
suffix:

1. Commit the generation statistics, completed-transition count, and any strict
   global-winner improvement.
2. Classify structural termination. A zero limit, the final requested child,
   or a promoted all-invalid child ends without an application callback.
3. If another transition is permitted, construct independent read-only result
   and statistics snapshots with `EVO_TERMINATION_NONE` and invoke
   `generation_stop`.
4. Map a true return to `EVO_TERMINATION_APPLICATION_REQUESTED`.
5. Invoke the observer with the final classification, then continue only when
   the reason remains `NONE`.

An immediate decision after generation zero retains its valid winner and zero
completed transitions. An intermediate decision retains every promotion
through that generation. A false or null decision changes no candidate work,
fitness comparison, allocation count, RNG schedule, or final structural result.
The decision sees no provisional child, owns no view, retains no history, and
cannot reject, retry, or roll back a commit.

## Structured C Source Evolution

The source optimizer does not treat source text as a byte genome. Raw textual
mutation and crossover cannot preserve token, declaration, macro, type,
control-flow, ownership, or semantic boundaries reliably and are prohibited.

One source-optimization genome is a canonical transformation recipe. Each
record contains:

- baseline and analysis identity;
- stable source target;
- transformation identifier and implementation version;
- bounded parameters;
- structural and semantic preconditions;
- dependencies and conflicts; and
- provenance sufficient to explain and replay the emitted source range.

The recipe has a versioned canonical serialization and hash. Unknown, stale,
cyclic, conflicting, or over-budget recipes reject before a candidate
workspace is written.

## Source-Recipe Initialization

Generation-zero recipes are derived only from opportunities supported by the
recorded Clang/LLVM analysis and the active transformation catalogue. The
initializer may select no-op, single-transformation, and compatible multi-
transformation recipes according to a versioned deterministic policy. It may
not invent an unregistered transformation or consult unrecorded analysis.

## Source-Recipe Mutation

Mutation operates on transformation records, not C characters or arbitrary
tokens. Versioned mutation operations may add, remove, replace, parameterize,
or reorder a transformation when its dependencies and conflicts permit.

After mutation, canonical validation either produces one complete admissible
recipe, applies a specifically versioned deterministic repair, or rejects the
candidate. Hidden heuristic repair is prohibited because it prevents replay
and obscures lineage.

## Source-Recipe Crossover

Crossover combines complete transformation records and their dependency
closure from two parent recipes. It performs canonical deduplication and
conflict handling before materialization. It never copies partial source
spans, fragments tokens, or combines raw source bytes.

Fixed vectors must lock parent-record selection, conflict resolution, repair
or rejection, canonical child order, RNG consumption, and replay.

## Source-Candidate Evaluation

Evaluation is a staged product operation:

1. validate the recipe against the exact baseline, analysis, and catalogue;
2. materialize a fresh isolated candidate source tree and patch;
3. build through the declared profile;
4. execute fast correctness and admissibility gates;
5. measure eligible candidates against the immutable baseline;
6. convert recorded finite measurements into the declared fitness; and
7. require complete finalist gates before a candidate can become the emitted
   champion.

Candidate compilation and execution occur in resource-bounded external
processes. Completion order may vary under parallel execution, but results are
committed to the evolutionary core in stable candidate order.

## Source-Optimization Result

The winning core genome is not itself the product artifact. The product maps
it to the exact transformation recipe, source candidate, reviewable patch,
validation evidence, measurements, fitness derivation, lineage, and replay
instructions.

The report calls this result the highest-ranked verified candidate discovered
within the recorded bounded search contract. It does not claim a globally
optimal equivalent program. Issues #58 through #69 implement this algorithmic
boundary in dependency order; issue #56 stabilizes it for 1.0 only after the
end-to-end source proof succeeds.
