# EVO Architecture

## Design Goals

EVO is a source-to-source evolutionary optimization system for C codebases,
built on a reusable C17 library for bounded engineering search. The core
engine remains independent of repository scoring, operating-system policy,
compiler tuning, FPGA placement, and C source semantics; consumers provide
problem-specific genome, fitness, validation, mutation, and crossover
callbacks.

The source-optimizer product is a separate layer over that generic core. It
owns C-project ingestion, Clang/LLVM analysis, structured transformation
recipes, isolated candidate source trees, build and correctness gates,
baseline-versus-candidate measurement, whole-run orchestration, and optimized
patch/evidence artifacts.

## Product Component Model

### Evolutionary-search core

`catalyst_evo` owns deterministic population storage, random streams,
selection, crossover, mutation dispatch, validation/evaluation ordering,
generation advancement, stopping, and core result ownership. EVO-001 is its
normative contract. Version 0.18.0 implements a bounded multi-generation
subset of this layer.

### Source analysis and transformation

The planned source layer captures an immutable C-project baseline and declared
build graph, then uses a versioned Clang/LLVM provider to produce stable source
identities, structural evidence, compiler optimization records, and configured
runtime hotspot evidence. It maps analysis opportunities into versioned
structured transformation recipes.

Source genomes never contain arbitrary C text for byte-wise mutation or
crossover. One genome represents a complete transformation recipe containing
stable targets, transformation identifiers and versions, parameters,
preconditions, dependencies, conflicts, and provenance.

### Candidate evaluation

Every recipe is applied to a fresh resource-bounded workspace derived from the
immutable baseline. Materialization produces a candidate source identity and
reviewable patch before compilation. Build, test, sanitizer, analyzer, ABI,
security, benchmark, and governance commands execute only under explicit
process, filesystem, network, environment, time, memory, and storage policy.

Correctness and admissibility are hard gates. Performance evidence cannot make
an invalid candidate valid, and a candidate cannot become the published
champion until it passes every configured finalist gate.

### Product orchestration and artifacts

The product layer coordinates analyze, evolve, replay, and report operations;
maps candidate evidence into finite EVO fitness; binds product checkpoints to
baseline, analysis, catalogue, toolchain, workload, and schema identities; and
emits the selected patch, recipe, lineage, validation, measurements, and
replay evidence.

EVO never applies, commits, pushes, merges, deploys, or publishes a target-
project patch automatically.

## Current Conformance Boundary

Only the evolutionary-search core exists in version 0.18.0. Source ingestion,
analysis, transformation, candidate materialization, external-process
isolation, target-code measurement, product commands, and optimized-patch
artifacts are planned by issues #58 through #69. Documentation of those
planned boundaries is not an implementation claim.

## Core Modules

- Population management
- Selection
- Crossover
- Mutation
- Diversity and stagnation handling
- Fitness and constraint handling
- Statistics and evidence
- Checkpointing
- Reproducible random-number generation

## Population Storage Boundary

Version 0.3.0 establishes the private population-storage foundation without
claiming that the execution loop exists. The subsystem owns one contiguous
zero-initialized genome slab. It checks `population_size * genome_size` for
`size_t` overflow and enforces both the per-genome and total slab budgets
provided by the caller.

Indexed genome pointers are bounded non-owning views. Population destruction
invalidates every view, releases the slab, and resets the complete private
object. The subsystem remains independently tested; version 0.6.0 invokes it
as the private storage boundary for `evo_run`.

## Deterministic Initialization Boundary

Version 0.4.0 adds private RNG algorithm version 1 and generation-zero
population initialization. One operation-local PCG-XSH-RR stream fills the
complete contiguous population slab using an explicit low-byte-first order.
The configured `uint64_t` seed, including zero, completely determines the raw
population bytes.

After prefill, EVO calls the optional consumer initializer once per genome in
ascending index order. The callback is a bounded deterministic transformation:
it may change only the provided genome, may not change ownership or retain the
view, and may not consult unrecorded entropy.

Successful initialization records the seed, RNG algorithm version, and
lifecycle state. Inactive, already initialized, or inconsistent populations
are rejected unchanged. Population destruction clears this metadata together
with the owned slab.

The random stream is reproducible rather than cryptographically secure.
Private population initialization does not itself call validity or fitness
callbacks and does not represent a completed search. Version 0.6.0 composes it
with the separate evaluation phase inside `evo_run`.

## Validation and Evaluation Boundary

Version 0.5.0 adds private generation-zero validation and evaluation. EVO first
classifies every candidate in ascending index order. A missing validator means
all candidates are valid. It then evaluates only valid candidates, again in
ascending order.

One caller-budgeted private record stores each candidate's validity and fitness
evidence. EVO checks the record-array multiplication for `size_t` overflow and
enforces `max_evaluation_bytes` independently from the genome-slab budget.
Provisional records are attached to the population only after all returned
fitness fields are proven finite.

Validity is a hard correctness gate. Among valid candidates, higher
consumer-computed `fitness.total` wins, and the lower index wins an exact tie.
The other component fields remain evidence rather than library-defined
objectives. An all-invalid population is a completed evaluation state with no
winner.

Resource, allocation, and non-finite-fitness failures leave the initialized
genome slab owned and unevaluated inside the private phase. Population
destruction releases both the genome slab and evaluation records and resets
the complete private object.

## Public Generation-Zero Boundary

Version 0.6.0 makes `evo_run` the owner of one complete private
generation-zero lifecycle:

1. reject an active public result unchanged;
2. construct and deterministically initialize private population storage;
3. validate every candidate and evaluate only valid candidates;
4. identify the stable highest-total winner;
5. transfer an independent copy and complete fitness evidence to the result;
6. release every private allocation before returning.

A missing evaluator is an invalid argument. Completion with no valid candidate
maps to `EVO_ERROR_NO_VALID_CANDIDATE`; all other private failures preserve
their existing status. Every non-active-result failure leaves the public
result empty.

In the version 0.6.0 boundary, the result copy is a distinct allocation because
private population destruction invalidates every population view. The copy
covers exactly the caller-bounded `genome_size`. `generations_completed`
remains zero because no selection or generation transition occurs.

## Private Selection Boundary

Version 0.7.0 adds a private tournament operator without changing public
generation-zero execution. The operator accepts a completed evaluated
population and an explicitly seeded private RNG stream. It validates storage,
evaluation evidence, validity flags, finite fitness, counts, and stable-best
metadata before advancing the stream.

Each draw uses rejection-sampled bounded indexing and maps a valid-candidate
ordinal to an ascending population index. Sampling is with replacement, higher
`fitness.total` wins, and the lower index wins an exact tie. Invalid candidates
are excluded before sampling rather than sampled and retried.

Selection is read-only over population state, performs no allocation, and
commits its output only after all draws succeed. It does not define stream
derivation or persistence. Version 0.11.0 supplies that ownership separately
for complete parent pairs without changing the selection operator.

## Private Crossover Boundary

Version 0.8.0 adds a private representation-neutral crossover dispatcher. It
accepts two bounded read-only parent views, two distinct non-overlapping child
views, the configured rate, and an explicitly seeded private RNG.

Each successful pair consumes exactly one 32-bit probability decision. A
selected event invokes the consumer callback exactly once; a non-selected
event or absent callback clones each parent into its corresponding child. All
pointer, genome-policy, rate, and RNG-state validation occurs before stream
consumption or child writes.

The callback owns genome representation semantics and must fully initialize
both children without changing parent bytes, ownership, or retaining any view.
The dispatcher performs no allocation and has no callback rollback path.

This boundary does not itself select parents or own next-generation storage.
Child-population ownership and operator stream derivation remain separate
private boundaries; version 0.12.0 composes them for complete-pair production,
and version 0.16.0 invokes that composition from the bounded public loop.

## Private Mutation Boundary

Version 0.9.0 adds a private representation-neutral mutation dispatcher. It
accepts one bounded writable genome, the configured mutation rate, and an
explicitly seeded private RNG.

Each valid attempt consumes exactly one 32-bit probability decision. A
selected event invokes the consumer mutation callback exactly once when
present; a non-selected event or absent callback leaves the genome unchanged.
All pointer, genome-policy, rate, and RNG-state validation occurs before stream
consumption or genome writes.

The engine owns the per-genome probability decision. The callback receives the
same configured scalar as its representation-specific mutation intensity and
must be deterministic for fixed bytes, rate, and context. It may not consult
unrecorded entropy, change storage ownership, or retain the view. Because the
callback mutates in place and returns no status, the dispatcher has no rollback
path.

This boundary performs no allocation. Version 0.12.0 composes it for complete
child pairs, and version 0.16.0 invokes that composition from the bounded
public loop. Built-in representation-specific mutation helpers and adaptive
schedules remain future work.

## Private Child-Population Ownership Boundary

Version 0.10.0 adds one independently owned child-population genome slab. The
operation accepts a completed parent population, validates its structure using
the same internal authority as tournament selection, and allocates matching
child dimensions under `max_child_population_bytes`.

The child slab is contiguous and zero-initialized. It has no evaluation
records, initialization seed, RNG version, validity count, or best-candidate
evidence. These empty lifecycle fields distinguish allocated output storage
from a completed population. Parent storage and evidence remain read-only and
the two populations may be destroyed independently.

The additional public configuration field is appended, preserving every
existing member offset while expanding `sizeof(evo_config_t)`. It authorizes
only one child genome slab and does not silently authorize operator scratch,
new evaluation records, checkpoints, or a total run working set.

This boundary performs no selection, pairing, crossover, mutation, elitism,
child completion, evaluation, swapping, RNG stream derivation, or generation
advancement by itself. Version 0.16.0 invokes it from the bounded public loop.

## Private Operator-Stream and Pair-Planning Boundary

Version 0.11.0 promotes the plain tuple-mixed schedule measured by EVO-RNG-001
into operator seed-schedule version 1. Each stream is independently derived
from the configured master seed, source generation, pair or child index, and a
stable selection, crossover, or mutation domain. This schedule is separate
from RNG algorithm version 1 and leaves generation-zero initialization
unchanged.

The private parent-pair planner owns selection-stream derivation. For complete
pair ordinal `i`, it derives the selection-domain stream at tuple index `i`,
runs two tournaments with replacement, and maps the output to child indexes
`2i` and `2i + 1`. The plan records its source generation and seed-schedule
version and is committed only after both selections succeed.

The completed parent remains read-only, and no child pointer is accepted or
written. Exactly `population_size / 2` complete pairs are planned. An odd
trailing slot remains explicitly unassigned until a singleton or elitism
policy is selected. Future crossover streams use pair ordinals and future
mutation streams use child indexes, but this milestone invokes neither
operator.

## Private Complete-Pair Production Boundary

Version 0.12.0 composes parent planning, operator-stream derivation, crossover,
mutation, and child ownership for one complete pair at a time. The private
child object records a contiguous produced count, source generation, and
operator seed-schedule version.

Pair `i` is accepted only when `2i` children have already been committed. EVO
validates child ownership and lifecycle state, plans the parents, derives one
pair-indexed crossover stream and two child-indexed mutation streams, and
resolves every bounded view before any callback or child write. It then
dispatches crossover followed by mutation for child A and child B.

After the no-expected-failure dispatch suffix returns, EVO commits production
metadata and pair evidence. Repeated, skipped, mismatched-generation, or
inconsistent requests reject before callbacks and preserve the child and
output. Parent genomes and completed evaluation evidence remain read-only.

Callbacks return no status, so their effects cannot be rolled back; violating
the bounded deterministic callback contract remains a consumer error. The
operation does not allocate child evaluations, mark the child as initialized
or evaluated, handle an odd trailing slot, swap populations, increment a
generation, or participate in `evo_run`.

## Private Odd-Tail Completion Boundary

Version 0.13.0 adds one private completion rule for odd child populations.
After the complete-pair prefix reaches `population_size - 1`, EVO validates the
completed parent and clones its stable best valid genome into the final child.
The operation is representation-neutral, consumes no RNG state, and invokes no
consumer callback.

The child records odd-tail policy version 1 alongside the source generation,
operator schedule version, and full produced count. A one-member population is
the defined zero-pair case. Every other request requires the complete pair
prefix and matching metadata. Rejection preserves parent, child, and output
evidence.

The resulting slab is fully produced but not initialized or evaluated. It has
no child validity, fitness, or best-candidate evidence and is not yet eligible
for selection or population swap until evaluation succeeds. Generalized
elitism, generation accounting, and `evo_run` integration remain later
boundaries.

## Private Produced-Child Evaluation Boundary

Version 0.14.0 adds a private evaluation operation for a fully produced child
slab. The operation accepts even populations completed entirely by pairs and
odd populations completed by the version-1 stable-best tail policy. It
requires matching source-generation and operator-schedule provenance before
allocating provisional evaluation records.

The existing evaluation engine is now shared by generation-zero and produced-
child lifecycle preflights. It validates all candidates first, evaluates only
valid candidates second, requires finite values in all fitness fields, and
commits the record set only after the complete pass succeeds. Stable best-
candidate selection retains the lower index on exact total-fitness ties.

Success leaves child genomes and all production metadata unchanged while
recording validity, fitness, valid count, best index, and completed evaluation
state. An all-invalid child is completed without a best. The common completed-
population validator recognizes both generation-zero and evaluated-child
provenance, allowing the evaluated child to become the read-only authority for
later selection and next-child allocation.

The operation consumes no RNG state and invokes no initialization, selection,
crossover, or mutation callback. It does not itself swap ownership, advance a
generation, or recycle the prior parent. Version 0.16.0 invokes it from the
bounded public loop.

## Private Atomic Generation Advancement

Version 0.15.0 adds one private ownership transition after produced-child
evaluation. The operation receives the current completed parent, a distinct
completed evaluated child, and the caller's current generation number.

The transition first validates both populations through the common completed-
population authority. A generation-zero parent is valid only for current
generation zero. A parent originating from an earlier child is valid only when
its recorded source generation is exactly one less than the current
generation. The incoming child's recorded source generation must equal the
current generation. Increment at `UINT64_MAX` is rejected.

All genome and evaluation allocations must be internally distinct and must
not overlap either population's owned ranges. Caller-owned evidence must also
be independent of both population objects and all owned allocations. These
checks make the old-parent release safe and preserve the single-owner model.

After every fallible check succeeds, the child structure is moved into the
parent handle, the child handle is reset to zero, the former parent is
destroyed, and versioned completion evidence is committed. This suffix
allocates no memory, copies no genome or evaluation byte, consumes no RNG word,
and invokes no callback. Here, atomic means rejection-before-mutation followed
by a no-fail library-state commit; it does not imply concurrent or lock-free
access to the population handles.

An all-invalid evaluated child may be promoted. Whether that state terminates
an optimization run is deliberately left to a later stopping-policy boundary.
The old parent is released rather than recycled into the child handle; buffer
recycling remains a separate ownership decision.

## Public Bounded Multi-Generation Run

Version 0.16.0 composes the accepted private generation boundaries inside
`evo_run`. `generation_limit` is the number of completed child transitions
after generation zero, so a zero limit retains the complete version 0.6.0
generation-zero behavior.

Transition-only configuration is validated before generation-zero allocation
or any consumer callback. For every source generation in ascending order, a
private bounded-run owner constructs one child slab, produces all complete
pairs, completes an odd tail through stable-best cloning when necessary,
evaluates the full child, and atomically promotes it. A one-member population
uses the odd-tail rule directly and does not require tournament, crossover, or
mutation policy that cannot be exercised.

The independent result genome is allocated once after generation zero. It is
a global best-so-far snapshot, not a view into either working population. A
later candidate replaces its bytes and fitness only when its total fitness is
strictly greater; exact cross-generation ties retain the earlier winner. The
copy occurs only after successful child promotion, so a failed transition
cannot publish uncommitted child evidence.

An evaluated all-invalid child is promoted, increments
`generations_completed`, and terminates the bounded loop successfully while
the earlier valid global winner remains. Any other failure destroys the child,
current parent, and public result allocation before returning an empty public
result. Partial progress is never exposed through `evo_run`.

Version 0.17.0 appends explicit public termination evidence after the existing
result fields. Successful limit completion records
`EVO_TERMINATION_GENERATION_LIMIT`; successful termination after a promoted
all-invalid child records `EVO_TERMINATION_ALL_INVALID`. The zero-valued
`EVO_TERMINATION_NONE` identifies an unstarted, failed, or destroyed result.
The reason is assigned only after the bounded operation and cleanup succeed.

Version 0.18.0 appends a versioned constant-space generation-statistics record
after the termination reason. Generation zero and each evaluated child are
summarized in ascending candidate order. A child record replaces the prior
record only after atomic promotion, so failed provisional generations remain
unobservable. The terminal all-invalid record has no generation-local best,
while the independent result genome continues to retain the earlier global
winner.

Statistics include population, valid, and invalid counts; the stable
generation-local best index and fitness; and component-wise sums over valid
fitness records. Invalid fitness payloads are never read. No history array,
statistics allocation, RNG consumption, callback, or ranking decision is
introduced.

Bounded-run policy evidence remains private. Convergence, stagnation,
application stop and observer callbacks, generalized elitism, adaptive
mutation, recycling, checkpointing, and parallelism remain separate
decisions.

## EVO Core Execution Flow

1. Initialize a population.
2. Validate and evaluate each genome.
3. Select parents.
4. Apply crossover and mutation.
5. Preserve elites and diversity.
6. Record statistics and evidence.
7. Stop on convergence, stagnation, generation limit, or an application-defined condition.

Version 0.18.0 publicly implements steps 1 through 4 for exactly
`generation_limit` bounded transitions, with the version-1 odd-tail policy as
the current elite-preservation rule in step 5. It implements the constant-space
statistics portion of step 6 for every committed generation, records the global
winner and completed transition count, and explicitly identifies limit
completion or later all-invalid extinction. Those remain the only public stop
conditions. Diversity, convergence, stagnation, application-defined stopping,
statistics observers, checkpointing, and parallel evaluation remain absent.

## Source-Optimizer Execution Flow

The 1.0 product flow is:

1. Parse and validate an explicit optimization manifest.
2. Capture the source, dependency, toolchain, target, workload, correctness,
   and resource-policy identities in an immutable baseline.
3. Build declared baseline profiles and establish benchmark eligibility.
4. Analyze the project with the declared Clang/LLVM provider and optional
   recorded runtime profiles.
5. Map supported opportunities into versioned structured transformation
   recipes.
6. Let the core evolve compatible recipes under deterministic seed and bounded
   population, generation, memory, storage, process, and time policy.
7. Materialize each admissible recipe into an isolated source candidate and
   reviewable patch.
8. Compile and run the declared fast correctness gates. Invalid candidates do
   not receive performance fitness.
9. Measure eligible candidates against the baseline under the recorded
   workload and measurement policy.
10. Select provisional finalists through the core, then require complete
    correctness, sanitizer, analyzer, ABI, security, toolchain, and governance
    gates before publication.
11. Emit the highest-ranked fully verified candidate found within the bounded
    search as a checksummed patch and evidence bundle.
12. Replay by verifying every recorded identity, rematerializing the same
    source candidate, and rerunning the declared validation and comparison.

Parallel source optimization schedules isolated external processes and commits
logical evidence in stable candidate order. It is distinct from the core's
planned in-process callback parallelism.

## Correctness Boundary

Candidate correctness is a hard gate. Invalid candidates are not evaluated and
cannot win. Fitness callbacks must return finite evidence, and consumer policy
is responsible for producing the scalar `total` used for comparison.

For source optimization, "correct" means that the candidate satisfied the
explicit obligations recorded in the optimization manifest and required by
EVO/AES governance. EVO does not infer missing specifications or claim
universal semantic equivalence. The final artifact reports the exact tests,
analysis, toolchains, workloads, limitations, and tolerances supporting the
result.

"Best" means the highest-ranked verified candidate discovered within the
recorded baseline, target platforms, workloads, transformation catalogue,
fitness definition, constraints, and search budget. It is not a global-
optimality claim.
