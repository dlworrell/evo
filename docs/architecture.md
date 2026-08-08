# EVO Architecture

## Design Goals

EVO is a source-to-source evolutionary optimization system for C codebases,
built on a reusable C17 library for bounded engineering search. The core
engine remains independent of repository scoring, operating-system policy,
compiler tuning, FPGA placement, and C source semantics; consumers provide
problem-specific genome, fitness, and validation callbacks plus either
consumer-defined operators or explicitly selected reference byte operators.

The source-optimizer product is a separate layer over that generic core. It
owns C-project ingestion, Clang/LLVM analysis, structured transformation
recipes, isolated candidate source trees, build and correctness gates,
baseline-versus-candidate measurement, whole-run orchestration, and optimized
patch/evidence artifacts.

## Product Component Model

### Evolutionary-search core

`catalyst_evo` owns deterministic population storage, random streams,
selection, crossover, mutation dispatch, validation/evaluation ordering,
fitness comparison, generation advancement, stopping, and core result
ownership. EVO-001 is its
normative contract. Version 0.19.0 implements a bounded multi-generation
subset of this layer, version 0.20.0 adds application-requested stopping over
committed state, and version 0.21.0 formalizes hard constraints and soft-
penalty comparison evidence. Version 0.22.0 adds bounded deterministic
population-diversity evidence, and version 0.23.0 adds deterministic
convergence and stagnation classification over committed evidence. Version
0.24.0 adds caller-bounded deterministic elite preservation with an explicit
ordinary-singleton path and compatibility-stable operator scheduling.
Version 0.25.0 adds versioned tournament/rank parent-selection dispatch with
exact integer weights and stable comparison-derived ranks.
Version 0.26.0 adds explicit compatibility-preserving operator dispatch plus
bounded reference one-point, two-point, uniform, and byte-XOR policies.
Version 0.27.0 adds deterministic committed-evidence mutation-rate adaptation
with a schema-4 human-readable decision projection.
Version 0.28.0 adds a disabled-by-default exact secure-erasure lifecycle for
every EVO-owned genome and evaluation range.
Version 0.29.0 adds canonical endian-stable committed-generation checkpoints,
allocation-free inspection, deterministic callback reattachment, and suffix-
only resume through the same continuation loop.
Version 0.30.0 adds disabled-by-default deterministic population-storage
recycling through two fixed run-local slots and a complete address-free owner
registry.

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

## Human-Readable Abstraction Boundary

Efficiency does not replace domain architecture. When EVO introduces a
compressed, cached, indexed, probabilistic, or otherwise accelerated structure,
the implementation must retain explicit reference semantics and provide a
deterministic human-readable audit projection:

```text
Machine-optimized structure
    +
Human-readable audit projection
```

The accelerated representation remains behind a domain interface. Its
projection uses stable identifiers and order and exposes logical registries,
result sets, relationships, ranges, event or generation windows, work
assignments, or decision traces rather than buckets, compressed words, pointer
graphs, or allocator metadata.

Projections may be caller-bounded, paginated, or windowed. Each view states its
scope, completeness, continuation, source identity, representation version,
and reconstruction order so a complete traversal cannot hide gaps,
duplication, or reordering.

Exact accelerators must be differentially equivalent to an explicit reference
path. Caches are derived state with source identity, freshness, invalidation,
and exact fallback or recomputation. Probabilistic structures are prechecks
only and never commit acceptance, rejection, ranking, selection, publication,
suppression, or termination; exact authority confirms every such result.

Canonical machine-readable evidence remains authoritative for replay and
automated verification, but its logical contents must be projectable and
reconcilable. Missing, stale, corrupt, incompatible, or unreconcilable
accelerated state fails closed or falls back to the bounded reference path.
ADR-0026 defines the complete rule.

## Current Conformance Boundary

Only the evolutionary-search core exists in version 0.30.0. Source ingestion,
analysis, transformation, candidate materialization, external-process
isolation, target-code measurement, product commands, and optimized-patch
artifacts are planned by issues #58 through #69. Documentation of those
planned boundaries is not an implementation claim.

The 0.30.0 core uses explicit bounded arrays, direct deterministic scans, and
one direct constant-space adaptive-rate record rather than compressed,
probabilistic, cached, or indexed run authority. Its
reference byte operators act directly on those exact arrays and introduce no
accelerated authority or retained compact decision structure. Secure erasure
uses the same direct owner fields and exact byte counts as its stable registry,
not an address-keyed cache or pool. Checkpoint format 1 is canonical binary
persistence rather than an accelerated decision path; its mandatory ordered
view exposes every configuration, generation, population, RNG/substream,
statistics, adaptation, ownership, and resume-identity field, while its
candidate accessor enumerates the exact population in stable order. The
population recycler is an exact allocation accelerator over two fixed local
owners. Exact pointer/count fields remain authority, while the complete stable
owner registry projects role, capacity, provenance, handoff, reset, and
erasure history without exposing addresses. EVO-HRA-003 differentially
reconciles that projection against the explicit allocation path. The current
core therefore has no opaque accelerated authority requiring remediation.
This audit does not pre-approve later variable pools, compressed checkpoints,
parallelism, analysis, recipe, orchestration, or artifact implementations.

## Core Modules

- Population management
- Versioned tournament and stable rank-based selection
- Consumer and reference byte-genome crossover
- Consumer and reference byte-genome mutation
- Evidence-driven bounded mutation-rate adaptation
- Opt-in secure erasure with exact owner-and-byte-count evidence
- Opt-in two-slot population recycling with an address-free audit registry
- Deterministic elite preservation and ordinary singleton production
- Diversity evidence and deterministic stagnation handling
- Fitness and constraint handling
- Statistics and evidence
- Versioned checkpoint inspection, capture, and deterministic resume
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

Version 0.28.0 retains policy version, backend, and enabled disposition beside
the existing genome and evaluation owner/count pairs. Disabled destruction
performs ordinary release and makes no erasure claim. Enabled destruction
erases each exact range once immediately before its sole release. Child
promotion moves this metadata with both owners before destroying the former
parent.

Version 0.30.0 optionally retains exactly two run-local logical population
slots. Generation zero owns stable slot 1; the first transition materializes
slot 2, and later committed generations alternate active and reusable roles.
Before reuse, EVO resets the former active genome and evaluation ranges in
full, clears their population evidence, and retains only exact capacities,
owner identity, reset/erasure metadata, and the detached evaluation reserve.
Disabled execution continues to allocate and release each child through the
0.29.0 path.

`evo_population_storage_registry_t` is the complete human-readable projection
of this accelerated lifecycle. It orders at most two entries by stable logical
identity and exposes role, represented/source generations, capacities,
handoffs, resets, erasures, and owner presence. It contains no address or
allocator metadata and cannot authorize ownership: every use reconciles the
registry against the exact pointer/count owners and committed generation.

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
fitness fields are proven finite and soft-penalty evidence satisfies the
versioned comparison policy.

Validity is a hard correctness gate. Hard-invalid candidates are never
evaluated, included in statistics fitness sums, selected, or retained as an
elite. Under fitness-comparison policy version 1, `constraint_penalty` is a
finite non-negative magnitude. The caller accounts for any desired penalty
when computing the authoritative `fitness.total`; EVO never applies it again.
Among valid evaluated candidates, higher total wins and exact ties prefer the
earlier generation, then the lower population index. The other component
fields remain evidence rather than library-defined objectives. An all-invalid
population is a completed evaluation state with no winner.

Resource, allocation, and malformed-fitness failures leave the initialized
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
`fitness.total` wins, and the lower index wins an exact tie through the same
versioned authority used by evaluation and global-best replacement. Invalid
candidates are excluded before sampling rather than sampled and retried.

Selection is read-only over population state, performs no allocation, and
commits its output only after all draws succeed. It does not define stream
derivation or persistence. Version 0.11.0 supplies that ownership separately
for complete parent pairs without changing the selection operator.

Version 0.25.0 places the existing operator behind selection-policy version 1
as the zero-valued compatibility dispatch and adds stable rank mode. Rank mode
uses the common fitness comparator to assign unique ranks, including lower-
index ordering for exact ties. It maps exact caller-configured linear integer
weights to one unbiased bounded ticket; invalid candidates own no interval.
All-valid worst-case arithmetic is checked before a positive-limit run can
allocate or invoke callbacks, and each actual rank distribution is dry-resolved
before its selection stream advances. Rank selection remains allocation-free,
with constant auxiliary storage.

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

Version 0.26.0 places that callback/clone behavior behind the zero-valued
`EVO_CROSSOVER_CONSUMER` compatibility selector and adds three explicit byte
modes. One-point mode selects an internal boundary for genomes larger than one
byte. Two-point mode selects two distinct boundaries from the inclusive
boundary set `[0, genome_size]` and swaps the half-open range between them.
Uniform mode consumes 32-bit masks least-significant bit first across ascending
byte offsets. Both children are complementary and fully initialized. Built-in
modes never invoke the callback and add no allocation or scratch owner.

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

Version 0.26.0 places that callback/no-op behavior behind the zero-valued
`EVO_MUTATION_CONSUMER` compatibility selector and adds byte-XOR mode. After the
existing per-genome probability gate, it selects one bounded byte index and one
nonzero value in `[1, 255]`, then XORs exactly that byte. A selected built-in
event therefore always changes one in-bounds byte and never invokes the
consumer callback.

This boundary performs no allocation. Version 0.12.0 composes it for complete
child pairs, and version 0.16.0 invokes that composition from the bounded
public loop. Other typed representation-specific mutation helpers remain
future work beyond the reference byte-XOR helper. Version 0.27.0 selects the
effective scalar before this dispatcher is called; dispatch itself does not
adapt or retain rate history.

## Adaptive-Mutation Boundary

Version 0.27.0 appends mutation-adaptation policy version 1. Its private
authority is one finite effective rate for the next transition and one
saturating committed-stagnation count. It is initialized only after generation
zero commits and updated only after a later evaluated child is atomically
promoted. Provisional or rolled-back work cannot change it.

The state machine uses the stable strict-global-improvement decision and the
committed normalized diversity value. It clamps the base rate into caller
bounds, raises by one bounded step for low initial diversity or later
stagnation, and optionally resets to the minimum on improvement. An inclusive
diversity threshold and explicit reset precedence remove hidden tie behavior.
No RNG word, callback, allocation, clock, process identity, or address enters
the decision.

Public generation-statistics schema 4 is the human-readable audit projection.
It exposes prior and next rates, bounds, step, threshold, stagnant count,
improvement/diversity/clamp/reset facts, and one reason enum in committed
generation order. Pair, singleton, elite, child-evaluation, generation-
advancement, and bounded-run evidence carry the rate used so lifecycle
validation can reject mismatched provenance. Version 0.29.0 persists this state
and projection together; no hidden history is reconstructed.

## Secure-Erasure Boundary

Version 0.28.0 appends secure-erasure policy version 1. The stable ownership
registry has four domain entries: public result genome, population genome
slab, attached population evaluations, and provisional evaluations. Each entry
uses its direct owner plus the exact retained allocation count; no global
pointer table, cache, compressed record, or address identity participates.

The zero-initialized policy records ordinary release. Enabled construction
records the selected backend beside each owner. CMake and GNU Autotools detect
`explicit_bzero` under the same internal capability macro; when unavailable,
the sole wrapper uses an ascending volatile-byte zero loop. It does not use
generic `memset`, allocate memory, call a consumer, or consume RNG.

Terminal cleanup erases each active genome or evaluation range exactly once
and then immediately releases that sole owner. Recycling extends the same
wrapper to every complete former-active range before reuse, so one long-lived
owner may have several reset erasures followed by one final-release erasure.
This covers successful population and result destruction, former-parent reset
or disposal after promotion, provisional evaluation rejection or recycled-
reserve reset, attached-evaluation rollback, child failure, public-run failure,
and partial allocation. Final destruction zeroes the complete owner record, so
repeated destruction is inert.

The guarantee is deliberately bounded to live EVO-owned process memory. It
does not claim to scrub consumer copies, stale aliases, allocator metadata,
swap, crash artifacts, device caches, or persistent media. ADR-0029 defines
the exact registry, lifecycle states, backend boundary, and test evidence.

## Checkpoint and Resume Boundary

Version 0.29.0 defines checkpoint format version 1 as a fixed little-endian
header plus six explicit sections: deterministic configuration, continuation
state, latest statistics, candidate evaluations, current population genomes,
and the global-best genome. Native padding, function pointers, context
addresses, and allocator addresses never cross the boundary. Unsigned sizes
use checked 64-bit encoding and floating-point values use explicit IEEE-754
binary64 bits.

Capture is opt-in and writes into one exact caller-owned buffer after stop and
generation-observer delivery for a committed generation. It performs no EVO
allocation. The observer receives borrowed bytes and the stable
`evo_checkpoint_view_t`; persistent copies remain caller-owned cleartext and
are outside EVO's secure-erasure guarantee.

Inspection is an allocation-free untrusted-input parser. It enforces the byte
budget, section arithmetic, CRC-32, configuration fingerprint, enum/boolean
domains, finite evidence, counts, stable-best order, statistics sums,
termination state, and owner lengths. CRC and FNV are corruption/navigation
evidence only. They provide no authentication, encryption, or independent
authority.

Resume then re-encodes and exactly compares every deterministic configuration
byte, callback-presence flag, and caller-declared nonzero problem/context
identity before allocating. Restored population, evaluation, and result
owners use the same reviewed allocation sites and the restoring build's local
secure-erasure backend. Full native population/statistics/adaptive invariants
must pass before those provisional objects commit. Failure destroys all
restored owners atomically and invokes no callback or RNG.

The continuation record retains current/global winner identities, global RNG
and substream versions, bounded-run/operator provenance, schema-4 statistics,
effective adaptive rate and stagnant count, and significant-improvement
patience state. Fresh and restored state enter the same bounded loop. Resume
never replays callbacks for the restored generation; terminal resume returns
immediately and intermediate resume begins with the next generation.

The binary bytes are not permitted to become opaque authority. The ordered
checkpoint view exposes exact logical sections and direct owner ranges, and
`evo_checkpoint_candidate_inspect` projects each genome/evaluation record in
ascending population order in constant time. A complete audit is linear and
uses no compact index, cache, filter, or hidden decoder state. ADR-0030 defines
the wire contract, trust boundary, projection, and verification evidence.

Version 0.30.0 advances the checkpoint format, checkpoint view, and canonical
configuration view to version 2 with magic `EVOCKPT2`. Format 2 persists the
recycling flag, storage-observer presence, population recycling disposition,
stable active owner identity, and complete logical storage registry. Inspection
reconciles that registry with generation parity, capacities, provenance,
secure-erasure metadata, and configuration before allocation. Resume creates
new local owners, reattaches the restoring build's local erasure backend, and
materializes the opposite slot only when a later transition requires it.
Format 1 is rejected explicitly rather than assigning lifecycle state absent
from its bytes. ADR-0031 defines this amendment and EVO-HRA-003 retains the
projection and reference-equivalence audit.

## Private Child-Population Ownership Boundary

Version 0.10.0 adds one independently owned child-population genome slab. The
operation accepts a completed parent population, validates its structure using
the same internal authority as parent selection, and allocates matching
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
runs two draws through the configured selection dispatch, and maps the output
to child indexes `2i` and `2i + 1`. Tournament mode retains with-replacement
semantics. The plan records its source generation, seed-schedule version, and
selection-policy provenance and is committed only after both selections
succeed.

The completed parent remains read-only, and no child pointer is accepted or
written. Through 0.23.0 exactly `population_size / 2` complete pairs were
planned. Version 0.24.0 first resolves the effective elite suffix, then plans
`floor(ordinary_offspring_count / 2)` pairs. A remaining ordinary slot belongs
to singleton policy version 1. Crossover streams keep pair ordinals and
mutation streams keep child indexes.

## Private Complete-Pair Production Boundary

Version 0.12.0 composes parent planning, operator-stream derivation, crossover,
mutation, and child ownership for one complete pair at a time. The private
child object records a contiguous produced count, source generation, and
operator seed-schedule version. From 0.25.0 it also records selection-policy
version and enum. From 0.26.0 it additionally records byte-operator policy
version and the selected crossover and mutation enums.

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
or evaluated, handle an unpaired ordinary slot or elite suffix, swap
populations, or increment a generation.

## Deterministic Elite-Preservation Boundary

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

Version 0.24.0 retains that rule as disabled-config compatibility and adds
elite policy version 1. `elite_count_enabled` selects an explicit request from
zero through population size; disabled mode requires a zero payload and
requests the old odd tail or no even elite. Effective count is capped at the
source valid count, so every retained elite has one distinct valid parent.

Ordinary offspring occupy the prefix and stable elites the suffix. Complete
pairs fill the largest even part of the prefix. If one ordinary slot remains,
singleton policy version 1 selects a valid parent using the next unused
pair-selection stream, clones it, and dispatches its child-indexed mutation
stream without crossover or scratch storage. Elite completion then ranks valid
parents through the common comparator and clones them best-to-worst after a
complete dry pass. Elite copying consumes no RNG and invokes no callback.

The resulting slab is fully produced but not initialized or evaluated. It
records effective elite count, source valid count, elite and singleton policy
versions, and explicit-versus-compatibility mode so later lifecycle validation
can reconstruct the slot layout.

From 0.25.0, the ordinary singleton uses the configured selection dispatch on
the same next-unused selection stream and every completed slab records matching
selection-policy provenance. Elite ordering itself remains comparison-based,
RNG-free, and independent of parent-selection mode.

## Private Produced-Child Evaluation Boundary

Version 0.14.0 adds a private evaluation operation for a fully produced child
slab. From 0.24.0, the operation accepts the policy-derived pair prefix,
optional singleton, and stable elite suffix, including the old version-1
stable-best tail compatibility form. It requires matching source-generation,
operator-schedule, elite, and singleton provenance before allocating
provisional evaluation records.

From 0.25.0 it also requires selection-policy version 1 and the enum matching
the active configuration. From 0.26.0 it requires byte-operator policy version
1 plus both configured operator enums. Child-evaluation policy version 6
preserves those identities without changing validation or evaluator callback
order.

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

Version 0.30.0 supplies that separate decision without weakening atomicity.
Enabled advancement dry-validates current and next registries, both owner
ranges, reset eligibility, and all aliases. Its no-fail commit moves the child
owners into the active handle, moves the former active owners into the reusable
handle, resets both complete reusable ranges, and then commits the registry and
version-8 evidence. Disabled advancement retains the exact release behavior
above.

## Public Bounded Multi-Generation Run

Version 0.16.0 composes the accepted private generation boundaries inside
`evo_run`. `generation_limit` is the number of completed child transitions
after generation zero, so a zero limit retains the complete version 0.6.0
generation-zero behavior.

Transition-only configuration is validated before generation-zero allocation
or any consumer callback. For every source generation in ascending order, a
private bounded-run owner constructs one child slab, produces the policy-
derived complete pairs, an optional ordinary singleton, and the stable elite
suffix, evaluates the full child, and atomically promotes it. Compatibility or
explicit one-elite mode completes a one-member population directly and does
not validate operator rates that cannot be exercised, although positive-limit
selector enums remain structurally valid provenance. One-member explicit-zero
mode exercises only configured parent selection and mutation, so its crossover
rate is likewise unused.

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

Version 0.19.0 appends optional observer configuration and delivers one
synchronous event after every committed generation. The observer receives
stack-backed result and statistics snapshots. Its only genome alias is a
byte-bounded `const` view of the independently owned global winner; no writable
population or result ownership crosses the boundary. Each invocation returns
before the next generation begins.

Generation-zero delivery follows winner transfer. Child delivery follows
statistics construction, atomic promotion, completion-count update, global-
winner update, and stop classification. Intermediate events carry
`EVO_TERMINATION_NONE`; the final event carries the applicable limit or all-
invalid reason. Failed and provisional generations do not emit events, and a
later failure does not invalidate observations already delivered for earlier
committed generations.

Version 0.20.0 appends `generation_stop` and its independent caller-owned
context after the observer fields. The synchronous decision receives fresh
read-only result and statistics snapshots only after a generation is fully
committed and only when another child could otherwise be attempted. Returning
true preserves that committed state and ends successfully with
`EVO_TERMINATION_APPLICATION_REQUESTED`; returning false continues unchanged.

Natural terminal conditions take precedence. A zero-limit generation and a
promoted all-invalid child do not invoke the stop callback. The configured
hard generation limit remains authoritative. When both callback types exist,
the stop decision runs first with reason `EVO_TERMINATION_NONE`; the observer
then receives an independent snapshot with the final post-decision reason. No
callback runs for provisional or failed child state.

Version 0.21.0 centralizes rankability and comparison in `src/fitness.c`.
Evaluation, completed-population validation, parent selection, odd-tail
elite validation, and global-best replacement therefore share one policy:
hard-valid and evaluated evidence only, greatest caller total, then earlier
generation and lower index. Evaluated populations and private transition
evidence record comparison policy version 1. Public generation-statistics
schema version 2 appends that policy identity without adding history or a new
allocation.

Version 0.22.0 measures diversity once after a population's validity and
fitness records are complete and before that evaluation phase commits. The
default version-1 metric is normalized byte mismatch over every unordered pair
of hard-valid candidates. An optional consumer callback supplies a versioned
normalized domain distance. Both use the same fixed lexicographic `(i, j)`
schedule with `i < j`; no sampling occurs and no RNG state exists at this
boundary.

The caller declares `max_diversity_work`. EVO checks the all-valid worst case
before any run callback and again before measurement. Default work units are
byte comparisons (`pairs * genome_size`); domain work units are callback
invocations (`pairs`). Invalid candidates do not form pairs. Zero or one valid
candidate records zero pairs, work, and diversity without invoking the domain
callback. The resulting value and provenance live with the private population
and are copied into public generation-statistics schema version 3, so state
validation and observation never repeat a domain callback.

Version 0.23.0 appends target, tolerance/patience, and diversity-floor controls
after the complete 0.22.0 configuration prefix. Their zero values are a
canonical disabled state. Enabled policies inspect the stable global winner
and the latest committed statistics only. A finite target is reached
with global-best total `>= target`. Patience counts committed children that do
not improve the last significant-best reference by strictly more than the
finite non-negative tolerance; a significant improvement resets the count.
An enabled diversity floor matches committed diversity `<= floor`.

Classification is allocation-free and RNG-free. Coincident terminal evidence
uses the fixed order all-invalid, converged, stagnated, generation limit, then
application requested. Natural classification suppresses the application stop
callback, and the observer sees the selected final reason. Bounded-run policy
version 5 records constant-space stopping evidence privately. Population
recycling and parallelism remain separate decisions at this historical 0.23.0
boundary.

Version 0.24.0 advances bounded-run policy to version 6. It records the final
elite count, source valid count, elite policy, singleton policy, and explicit-
mode flag. Child-evaluation and generation-advancement policies advance to
version 4 so the same provenance survives evaluation and ownership transfer.

Version 0.25.0 advances bounded-run policy to version 7 and child-evaluation
and generation-advancement policies to version 5. Each records selection-policy
version 1 and the active enum, while operator domain identities and public
result layout remain unchanged.

Version 0.26.0 advances bounded-run policy to version 8 and child-evaluation
and generation-advancement policies to version 6. Each records byte-operator
policy version 1 plus the configured crossover and mutation selectors. The
direct bounded byte arrays remain exact reference state, so this adds no cache,
index, compressed authority, or projection lifecycle.

Version 0.27.0 advances bounded-run policy to version 9 and child-evaluation
and generation-advancement policies to version 7. Each carries the effective
mutation rate through production, evaluation, promotion, and final evidence.
Generation-statistics schema 4 publishes the ordered adaptive decision; its
explicit fields are the ADR-0026 audit projection, not a compressed or cached
authority.

Version 0.28.0 leaves those decision-evidence schemas unchanged. Secure-
erasure policy is retained directly with each owner and moves atomically with
population ownership; it does not alter the bounded-run decision trace.

Version 0.29.0 advances bounded-run policy to version 10 and introduces
private run-state schema 1. The schema makes current/global winner identity,
adaptive and stopping state, terminal reason, and global RNG/operator versions
explicit continuation authority. Checkpoint format 1 persists this state plus
the exact population and public evidence; restore supplies it unchanged to the
same loop.

Version 0.30.0 advances bounded-run policy to version 11, child-evaluation and
generation-advancement policies to version 8, and private run-state schema to
version 2. These records carry the recycling policy and stable active/reusable
identities through evaluation, atomic promotion, checkpoint capture, and
resume without making the projection an ownership authority.

## EVO Core Execution Flow

1. Initialize a population.
2. Validate and evaluate each genome.
3. Select parents.
4. Apply crossover and mutation.
5. Preserve elites and diversity.
6. Record statistics and evidence.
7. Stop on convergence, stagnation, generation limit, or an application-defined condition.

Version 0.30.0 publicly implements steps 1 through 5 for at most
`generation_limit` bounded transitions, with caller-bounded elite policy
version 1, explicit consumer/reference byte-operator policy, and bounded
diversity measurement in step 5. It implements the constant-space statistics
and adaptive-decision portion of step 6 for every committed generation, records the global
winner and completed transition count, and explicitly identifies limit
completion, later all-invalid extinction, convergence, stagnation, or an
application request after a committed generation. Committed-generation
observation, the population-storage registry, and versioned checkpoint
projection complete the current bounded portion of step 6. Deterministic
resume continues the suffix from any retained committed generation. Enabled
storage recycling changes allocation/reset work only; parallel evaluation
remains absent.

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
