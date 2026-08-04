# EVO Algorithms and Source-Evolution Roadmap

This document distinguishes algorithms implemented by the reusable
`catalyst_evo` core from the structured program transformations and evaluation
algorithm required by the EVO 1.0 source optimizer. Version 0.26.0 implements
only the core boundary described below.

## EVO Core Initial Release

- Tournament selection
- Rank-based selection
- Elitism
- One-point crossover
- Two-point crossover
- Uniform crossover
- Reference byte-XOR mutation
- Typed bit, integer, floating-point, and permutation mutation helpers
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

All seven returned fitness components must be finite. From version 0.21.0,
`constraint_penalty` must also be a non-negative magnitude. `fitness.total` is
the consumer-computed scalar objective, and EVO selects the greatest total.
Exact ties preserve the lower population index. The remaining components are
recorded for evidence but receive no library-defined ordering.

An all-invalid population completes evaluation without a winner. Evaluation
records are bounded by `max_evaluation_bytes`, and failures discard provisional
records while preserving the initialized genome slab within the private phase.

## Hard Constraints, Soft Penalties, and Stable Comparison

Version 0.21.0 defines fitness-comparison policy version 1. Hard feasibility is
owned exclusively by `is_valid`: false candidates never reach the evaluator or
any ranking site. A soft violation remains hard-valid and is reported as a
finite `constraint_penalty >= 0`. The evaluator computes the final scalar
`total`, including whatever weighted penalty effect the caller intends. EVO
does not subtract or reweight the penalty again.

The shared comparison algorithm accepts only hard-valid evaluated records with
policy-valid fitness evidence, then orders them by:

1. greater `total`;
2. earlier generation on an exact total tie; and
3. lower population index within the same generation.

Evaluation, completed-population validation, parent selection, odd-tail
elite validation, and global-best replacement all use this authority. Policy
version 1 is recorded in completed private populations and propagated through
child-evaluation, generation-advancement, bounded-run, and public statistics
evidence. A negative or non-finite penalty rejects with
`EVO_ERROR_EVALUATION` before provisional records commit. Total-only evaluators
remain unchanged because zero-initialized penalty evidence is valid.

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

## Versioned Deterministic Parent Selection

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

Version 0.25.0 publishes selection-policy version 1. Tournament is enum value
zero and retains the complete behavior and exact RNG sequence above. Rank mode
requires a zero tournament size, a positive integer base weight, and a
non-negative integer step weight. If `n` candidates are hard-valid, stable
rank `r` receives:

```text
weight(r) = rank_base_weight
          + (n - 1 - r) * rank_step_weight
```

Rank zero is the stable best under fitness-comparison policy version 1. Exact
fitness ties therefore rank the lower population index first. Invalid
candidates receive no rank and no interval.

The configured all-valid total
`n * base + n * (n - 1) / 2 * step` must fit in `size_t`. EVO checks that
arithmetic before a positive-limit run allocates or invokes a callback. At
selection time it validates the completed population, resolves every rank and
weight, and proves the actual total before consuming RNG state. One unbiased
bounded-index ticket is then drawn from the exact total. Candidate intervals
are traversed in ascending population-index order; this traversal is normative
for fixed-seed replay.

Rank selection uses constant auxiliary storage and performs no allocation.
The same pair-local selection streams are used in both modes, while crossover
and mutation remain in their existing independent domains. Selection policy
and version provenance accompany produced children through evaluation,
promotion, and bounded-run evidence.

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
those separate private boundaries for complete pairs.

Version 0.26.0 publishes byte-operator policy version 1. The zero-valued
consumer selector preserves the complete behavior and RNG sequence above.
Explicit byte modes bypass the callback after the same one-word gate:

- one-point crossover samples an unbiased internal cut in `[1, n - 1]` and
  swaps `[cut, n)`; a one-byte genome clones without a cut draw;
- two-point crossover samples two distinct boundaries uniformly without
  replacement from `[0, n]`, sorts them, and swaps `[lower, upper)`; and
- uniform crossover consumes one 32-bit mask per group of up to 32 ascending
  bytes, using least-significant bit first to choose corresponding or swapped
  parents.

Both children are complementary and fully initialized over `[0, n)`. Bounded
samples retain the version-1 two-word rejection schedule; uniform mode consumes
exactly `ceil(n / 32)` words after a selected gate. The direct byte arrays are
the exact reference representation and no operator allocates scratch storage.

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

The operator does not allocate a child population, adapt the rate, or execute a
generation transition. Version 0.12.0 composes the dispatcher for complete
child pairs.

Version 0.26.0 retains consumer mode as enum value zero and adds reference
byte-XOR mode. After a selected one-word probability gate, it draws one
unbiased index in `[0, n - 1]`, draws one unbiased nonzero mask in `[1, 255]`,
and XORs exactly that byte. The selected event therefore always changes one
in-bounds byte. Each bounded draw uses the existing two-word rejection schedule.
The reference mode bypasses the callback and allocates no state. Typed bit,
integer, floating-point, permutation, and adaptive policies remain later work.

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

- Through 0.23.0, exactly `population_size / 2` complete pairs are
  addressable. Version 0.24.0 resolves the effective elite count first and
  addresses `floor(ordinary_offspring_count / 2)` pairs.
- Pair `i` owns child indexes `2i` and `2i + 1`.
- The selection stream is derived from the master seed, source generation,
  pair ordinal, and selection domain.
- Two configured selection-policy draws run sequentially on that pair-local
  stream. Tournament mode retains its with-replacement semantics.
- Both parents must be selected before the output plan is committed.
- The plan records pair ordinal, source generation, seed-schedule version, and
  selection-policy provenance.
- Null, policy, lifecycle, all-invalid, and pair-bound failures preserve parent
  evidence and the output object.
- A trailing ordinary child index is outside complete-pair planning and belongs
  to singleton policy version 1.

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
- Success records produced count, source generation, schedule version,
  selection provenance, byte-operator policy version, both operator selectors,
  and pair evidence.
- Repeated, skipped, mismatched-generation, and invalid requests preserve
  child bytes and output evidence before callback dispatch.
- Parent genomes and completed fitness evidence remain unchanged.

Consumer callbacks return no status. After complete preflight, the valid
dispatch suffix has no expected library rejection, but callback side effects
and callback contract violations cannot be rolled back.

Complete-pair production stops at the largest even prefix below the resolved
ordinary-offspring count. The child remains unevaluated and incomplete until
an optional singleton and the elite suffix are committed.

## Deterministic Elite Preservation

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

Version 0.24.0 retains that rule as disabled-config compatibility and defines
elite policy version 1:

- enabled mode accepts a requested count from zero through population size;
- disabled mode requires zero and requests one elite for odd populations or
  none for even populations;
- effective count is the smaller of the request and source valid count;
- ordinary offspring occupy the prefix and distinct valid elites occupy the
  suffix in stable best-to-worst order;
- `floor(ordinary_offspring_count / 2)` complete pairs retain their existing
  streams and child slots;
- an odd ordinary prefix uses the next selection-stream index to choose one
  valid parent, clones it, and runs its child-indexed mutation stream;
- the singleton invokes no crossover and allocates no scratch sibling; and
- elite ranking and copying consume no RNG and invoke no callback.

All ranking, alias, bounds, and lifecycle checks plus a complete dry ranking
pass precede elite copies. Evidence records requested, effective, source-valid,
ordinary-offspring, singleton, and compatibility policy facts. Full child
production still does not imply initialization or evaluation.

## Deterministic Produced-Child Evaluation

Version 0.14.0 evaluates a fully produced child population without consuming
RNG state or changing any genome byte.

- The child must contain exactly `population_size` produced genomes with
  source-generation and operator-schedule provenance from production.
- Production must carry elite policy version 1, the resolved elite and source-
  valid counts, explicit-versus-compatibility mode, and the expected singleton
  marker. Only disabled odd compatibility carries odd-tail policy version 1.
- Evaluation records are allocated provisionally under
  `max_evaluation_bytes`.
- Validation visits every candidate in ascending index order.
- Only valid candidates are evaluated, also in ascending index order.
- All seven fitness fields must satisfy comparison policy version 1 before the
  record set is committed.
- Higher `fitness.total` wins, with the lower index retained on exact ties.
- Invalid candidates retain zero fitness and are never evaluated.
- An all-invalid child completes without a best candidate.
- Success preserves production metadata and commits evaluation records,
  valid count, stable best, comparison and diversity policy, and child-
  evaluation policy version 6 evidence exactly once.
- Preflight, resource, allocation, and malformed-fitness failures preserve
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

Version 0.22.0 advances generation-advancement policy to version 3 so its
evidence carries both fitness-comparison policy version 1 and the child's
diversity policy and metric versions. Ownership transfer and generation
numbering remain unchanged.

Version 0.24.0 advances generation-advancement policy to version 4 and copies
elite count, source-valid count, elite policy, singleton policy, and explicit-
mode evidence without changing ownership transfer.

Version 0.25.0 advances generation-advancement policy to version 5 and copies
selection-policy version and enum evidence without changing ownership transfer.

Version 0.26.0 advances generation-advancement policy to version 6 and copies
byte-operator policy version plus crossover and mutation selector evidence
without changing ownership transfer.

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
   slab and resolve requested, effective, and ordinary-offspring counts.
5. Produce complete pairs in ascending pair order, an ordinary singleton when
   the prefix is odd, and the stable elite suffix.
6. Evaluate the complete child and resolve whether its stable best has a
   strictly greater total fitness than the retained global winner.
7. Atomically promote the evaluated child to completed generation `g + 1`.
   Only after promotion may a strict improvement overwrite the existing result
   buffer and fitness evidence.
8. Record the completed transition. If the promoted population is all-invalid,
   stop successfully. If the hard limit is now met, stop successfully.
   Otherwise, consult the optional application stop decision and continue only
   when it returns false.

The result buffer is never reallocated during a run. Exact total-fitness ties
across generations retain the earlier winner, so deterministic replay is not
affected by allocation addresses or later equal candidates. Failure at any
transition destroys all internal owners and the result allocation; no partial
winner or completion count escapes the public call.

Through version 0.26.0, this algorithm has a bounded sequential working set of
one current population, one child population, one result genome, and the
current population's evaluation records plus provisional child evaluation
records during child evaluation. It does not recycle slabs or run callbacks
concurrently.

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

Version 0.21.0 advances the public statistics schema to version 2 by appending
fitness-comparison policy version 1. Aggregation order and arithmetic remain
schema-version-1 compatible; the new field identifies the authority that
established the copied generation-local best.

## Bounded Deterministic Diversity

Version 0.22.0 defines diversity policy version 1 and advances public
generation-statistics schema to version 3. Measurement occurs once after all
validity and fitness callbacks for a population succeed. Hard-invalid
candidates are excluded. For valid candidate indices, EVO visits every
unordered pair in the exact order `(0,1), (0,2), ...`, skipping invalid
members while retaining lexicographic index order. It performs no random
sampling and consumes no operator RNG.

The built-in byte metric has version 1. For a pair it counts byte positions
whose values differ, divides by `genome_size`, and reports generation
diversity as total differing bytes divided by `pair_count * genome_size`.
Thus the result is normalized to `[0, 1]` with one final division. A consumer
may instead provide `genome_distance` plus a nonzero metric version. Each
callback result must be finite and in `[0, 1]`; EVO adds results in traversal
order and divides by the pair count. A malformed value rejects the evaluation
with `EVO_ERROR_EVALUATION` and discards provisional evaluation and diversity
records.

For `v` valid candidates, `pair_count = v * (v - 1) / 2`, computed with
divide-first checked arithmetic. Zero or one valid candidate has zero pair
count, work, and diversity and invokes no distance callback. Before any run
callback, EVO computes the all-valid worst case from configured population
size. Built-in work is `pair_count * genome_size` byte comparisons; domain
work is `pair_count` callback invocations. Overflow or a value above
`max_diversity_work` returns `EVO_ERROR_RESOURCE_LIMIT` before dispatch.

Successful population evidence stores policy version, metric version, metric
kind, pair count, work units, and normalized value. Statistics copy this
evidence without recomputation, so validation, observation, stopping, and
promotion cannot repeat a domain callback. Diversity does not participate in
fitness comparison or selection in version 0.22.0.

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
limit, all-invalid, converged, stagnated, or application-requested
termination.

The observer has no return value. Its invocation cannot add a stop decision,
retry a failed generation, or change winner selection. A provisional child
that fails evaluation, statistics, or promotion emits no event. No observer
delivery allocates, consumes RNG state, runs concurrently, or retains history.

## Application-Requested Stopping

Version 0.20.0 adds bounded-run policy version 2 and an optional synchronous
stop decision. After generation `g` is committed, its statistics and global
winner are updated, and natural termination is classified. If no natural
reason exists and another child transition remains available, EVO constructs
fresh callback-lifetime result and statistics snapshots and invokes
`generation_stop`.

The stop snapshot always carries `EVO_TERMINATION_NONE`. Returning false leaves
the deterministic 0.19.0 transition sequence unchanged. Returning true ends
successfully at generation `g`, preserves the committed winner and statistics,
and ultimately publishes `EVO_TERMINATION_APPLICATION_REQUESTED`. When an
observer is also configured, EVO creates separate snapshots after the stop
decision; the observer therefore sees the final application reason before the
run returns.

Natural terminal reasons suppress the application decision. A zero-limit run
and the final hard-limit generation use `EVO_TERMINATION_GENERATION_LIMIT`; a
promoted all-invalid child uses `EVO_TERMINATION_ALL_INVALID`. No stop decision
is made for those states, nor for provisional or failed children. The callback
allocates no engine storage, consumes no RNG, owns no view, and may not mutate
EVO state. A null callback is replay-equivalent to 0.19.0.

Version 0.21.0 advances bounded-run policy to version 3. Its evidence records
fitness-comparison policy version 1 plus the winning generation and population
index used by the shared comparator. Candidate work, RNG schedules, callback
ordering, and the three 0.20.0 termination conditions remain unchanged.

Version 0.22.0 advances bounded-run policy to version 4 and records diversity
policy and metric provenance. It does not change operator stream derivation,
selection draws, global-best comparison, or termination classification.

## Deterministic Convergence and Stagnation

Version 0.23.0 defines stopping policy version 1. Every control is disabled in
a zero-initialized configuration. An enabled fitness target is reached when
the committed stable global-best total is `>= fitness_target`. An enabled
diversity floor is reached when the latest committed schema-3 diversity is
`<= diversity_floor`. Both comparisons intentionally include equality and may
classify generation zero.

Enabled patience establishes generation zero's global-best total as the first
significant-best reference. For each committed child, it applies exactly one
binary64 addition and comparison:

```text
threshold = significant_best + improvement_tolerance
significant = current_global_best > threshold
```

The finite tolerance must be non-negative. A significant improvement replaces
the reference and resets the consecutive stagnant count. Otherwise the count
increases once. The run stagnates at `count >= stagnation_patience`. Equality,
an exact global tie, and a strict improvement no greater than the tolerance do
not reset patience; multiple small improvements can reset it after their
cumulative global best exceeds the retained reference plus tolerance.

After each commit EVO classifies all-invalid, converged, stagnated, and
generation-limit evidence in that exact order. Only when all are absent may
the application callback select `EVO_TERMINATION_APPLICATION_REQUESTED`.
Generation-zero all-invalid remains an error. This ordering makes coincident
conditions replay-unambiguous while the configured limit still prevents any
extra transition.

The classifier reads only committed result and statistics fields. It allocates
nothing, consumes no RNG, invokes no new callback, and contains no time,
address, process, or entropy input. Bounded-run policy version 5 records
stopping policy version 1, the significant-best reference, stagnant count, and
final classification flags in constant-space private evidence.

Version 0.24.0 advances bounded-run policy to version 6 and records final
elite and singleton provenance. Elite copies do not perturb operator streams;
disabled mode remains byte- and callback-replay compatible with 0.23.0.

Version 0.25.0 advances bounded-run policy to version 7 and records final
selection-policy version and enum. Child-evaluation and generation-advancement
policies advance to version 5 so the same provenance survives evaluation and
ownership transfer.

Version 0.26.0 advances bounded-run policy to version 8 and records byte-
operator policy version 1 plus both selected operator enums. Child-evaluation
and generation-advancement policies advance to version 6 for the same
provenance. The reference operators introduce no accelerated structure; their
bounded byte arrays and direct scans remain exact authority under ADR-0026.

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
