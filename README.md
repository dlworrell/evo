# Catalyst EVO

Catalyst EVO is a source-to-source evolutionary optimization system for C
codebases, built on a deterministic, reusable C17 evolutionary-search core.

The completed core provides bounded, reproducible candidate search. The EVO
1.0 product will ingest an existing buildable C project, analyze source and
runtime evidence, evolve structured source-transformation recipes, compile and
validate isolated candidates, and return the highest-ranked verified C source
candidate found within a declared search contract.

## Mission

Evolve existing C source toward better measured implementations while
preserving declared correctness, security, ABI, portability, build, and
maintainability constraints. The winning result must be emitted as a
human-reviewable source patch with complete baseline comparison and replay
evidence.

"Best" means the highest-ranked verified candidate EVO discovered within the
recorded baseline, target platforms, workloads, transformation catalogue,
objective, constraints, and search budget. EVO does not claim to prove a
globally optimal equivalent program.

The governing product records are:

- `docs/adr/ADR-0016-layered-source-to-source-c-optimizer.md`
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
- `docs/specs/EVO-001-library-contract.md`
- `docs/specs/EVO-002-source-optimizer-contract.md`
- `docs/roadmap.md`

## Product Boundary

EVO has two deliberate layers:

- `catalyst_evo` — the reusable C17 core that owns deterministic population,
  selection, mutation, crossover, evaluation, stopping, checkpoint, and
  evidence mechanics;
- the EVO source optimizer — the planned Clang/LLVM-backed analysis,
  structured transformation, isolated build/test/benchmark, orchestration,
  and artifact pipeline defined by `docs/specs/EVO-002-source-optimizer-contract.md`.

Opaque byte genomes remain valid for the generic core. C source is never
evolved by splicing raw text or crossing over arbitrary source bytes. One
source-optimization genome represents a versioned structured transformation
recipe whose application produces an actual reviewable C source candidate.

## Human-Readable Architecture

EVO may use machine-optimized structures, but it may not make them opaque
authority. Every compressed, cached, indexed, probabilistic, or accelerated
representation must retain explicit reference semantics and provide a stable
human-readable audit projection in domain terms.

Runtime hash tables project to registry views, bitmaps to explicit result
sets, compact monotone indexes to ordered windows, and work queues to stable
assignment and commit views. Caches identify their canonical source,
freshness, invalidation, and exact fallback. Membership filters and other
probabilistic structures are prechecks only; exact authority confirms every
committed decision.

Large projections may be bounded or paginated when scope, completeness,
continuation, and reconstruction order are explicit. Canonical machine-readable
evidence remains authoritative for replay and verification, while its logical
contents must remain deterministically projectable and reconcilable. See
ADR-0026 and EVO-001/EVO-002 for the normative contract. The retained
EVO-HRA-001 audit records why the implemented 0.25.0 core has no current
accelerated structure requiring remediation and where later work must recheck
the rule. ADR-0027 separately assesses the 0.26.0 byte operators: their direct
bounded arrays are the exact reference representation, so no accelerator or
separate audit projection is introduced. ADR-0028 assesses 0.27.0 adaptive
mutation: its direct constant-space state is canonical, and schema-4 observer
records provide the ordered human-readable decision projection. ADR-0029
assesses 0.28.0 secure erasure: direct owner fields and exact byte counts are
canonical, and its stable registry names every range, lifecycle disposition,
and backend without an accelerated or address-keyed authority. ADR-0030
assesses 0.29.0 checkpointing: canonical binary persistence is never opaque
authority because an allocation-free ordered view exposes configuration,
generation, population, RNG/substream, statistics, adaptation, ownership, and
resume identities, with explicit per-candidate projection.
The retained EVO-HRA-002 audit reconciles that projection against the binary
sections and exact resume authority. ADR-0031 assesses 0.30.0 population-
storage recycling: exact pointer/count owners remain authority while a fixed,
address-free registry projects both stable slot identities, lifecycle roles,
capacities, provenance, handoffs, and reset history. The retained EVO-HRA-003
audit proves reference-path equivalence and registry reconciliation through
checkpoint/resume.
ADR-0032 assesses 0.31.0 bounded parallel evaluation: serial evaluation remains
the exact reference while a complete candidate-ordered schedule projects stable
logical worker assignment, waves, completion, cancellation, and commit order.
The retained EVO-HRA-004 audit proves worker-count equivalence and that runtime
thread identity or timing cannot become authority.
ADR-0033 assesses 0.32.0 core benchmark evidence: complete ordered JSON cases,
seeds, generation traces, and raw samples remain authority, while aggregates
and the scoped FNV locator are derived and non-authoritative. The Markdown
summary is regenerated only from validated JSON, and EVO-HRA-005 retains the
reconciliation audit.
ADR-0034 assesses 0.33.0 installed reference adapters: their immutable fixture
records and bounded arrays are direct reference forms, the stable JSON registry
retains every configuration, trace, checkpoint candidate, and logical worker
assignment, and Markdown is derived only after exact golden validation.
EVO-HRA-006 retains this adapter-specific reconciliation.
ADR-0035 assesses 0.34.0 project ingestion: immutable snapshot bytes and
complete ordered file, compilation-unit, policy, and gate registries are exact
authority; versioned FNV labels are diagnostic only; and JSON plus Markdown
expose the whole baseline in stable domain order. The implementation uses
bounded arrays and direct scans rather than an accelerated structure.
EVO-HRA-007 retains this ingestion-specific audit.

## Roadmap Scope

The core track includes:

- Genetic algorithms
- Tournament and rank-based selection
- One-point, two-point, and uniform crossover
- Mutation and adaptive mutation
- Opt-in secure erasure for EVO-owned genomes and evaluation evidence
- Diversity measurement and stagnation handling
- Constraint and penalty handling
- Checkpointing and reproducible random-number generation
- Opt-in deterministic population-storage recycling
- Opt-in bounded deterministic parallel fitness evaluation
- Benchmarking and engineering evidence
- Installed reference consumer adapters with replay, resume, and bounded
  parallel-evaluation evidence

The source-optimizer track adds:

- Implemented C project ingestion and immutable baseline capture
- Clang AST, LLVM IR, compiler-evidence, and runtime-hotspot analysis
- Versioned structured source-transformation recipes
- AST-aware C source transformations and isolated candidate materialization
- CMake/Clang/LLVM build and correctness gates with independent
  Autotools/GNU validation
- Reproducible baseline-versus-candidate performance measurement
- Bounded parallel compilation, checkpoint/resume, and deterministic replay
- Reviewable optimized patches and machine-readable evidence bundles

## Safety Boundary

Evolved candidates must not bypass correctness tests, safety constraints, or
validation gates. Candidate work occurs only in isolated workspaces derived
from an immutable baseline. EVO may emit a proposed patch, but it must never
silently modify, commit, push, merge, deploy, or publish into the input
repository.

## Repository Layout

- `include/catalyst/evo/` — public C API
- `src/` — implementation
- `tests/` — unit and integration tests
- `examples/` — application examples
- `benchmarks/` — performance and algorithm benchmarks
- `docs/` — architecture, theory, algorithms, and evidence guidance

The first private source-optimizer foundation units are present in 0.34.0.
Clang analysis, transformations, candidate evaluation, orchestration, and the
installed executable remain dependency-ordered roadmap work; their absence is
an explicit boundary, not an implicit feature claim.

## Authoritative Native Builds

EVO treats CMake and GNU Autotools as independent, equivalent build
frontends. The checked-in AES-BLD-001 profiles bind:

- CMake + Clang 18 to LLVM archive/inspection tools and LLD
- CMake + GCC 13 to GNU Binutils and the BFD linker
- Autotools to both GCC/GNU tools and Clang/LLVM tools

The canonical local entry points are:

```sh
cmake --preset aes-clang
cmake --build --preset aes-clang
ctest --preset aes-clang

autoreconf -fvi
mkdir -p build/autotools-gcc
cd build/autotools-gcc
CC=gcc ../../configure
make
make check
```

`CMakePresets.json`, `configure.ac`, and `Makefile.am` enumerate the same 26
installed-core sources, eight private source-foundation sources, and 34
normative tests. CI also compares staged install
manifests, public symbols, package metadata, and a downstream consumer built
against each installed result. See
`docs/engineering/AES-BLD-001-toolchain-profile.md`.

These profiles currently build and validate the C17 core. The source-optimizer
roadmap separately requires isolated target-project analysis and compilation,
plus independent validation of a selected source candidate through the
declared Clang/LLVM and GNU profiles.

## Status

**Current implementation boundary:** EVO 0.34.0 implements the deterministic
evolutionary-search core plus strict C-project manifest ingestion, immutable
baseline capture, normalized compilation-unit evidence, and declared baseline
gate orchestration through a caller-supplied execution provider. It does not
yet build a Clang AST or LLVM IR model, transform source, evaluate evolved
candidates, or emit an optimized patch. It is also not yet an installed
standalone optimizer executable: issue #67 defines product commands and issue
#93 requires the actual installed binary before artifact and end-to-end work.
Issue #56 remains the final 1.0 stabilization gate.

EVO 0.16.0 composes the complete deterministic generation pipeline into a
bounded public `evo_run`. Generation zero is constructed, initialized,
validated, and evaluated first. Each configured transition then produces a
complete child population, evaluates it, atomically promotes it, and retains
the strict global best-so-far in one independently owned result buffer.

`generation_limit` counts completed child transitions after generation zero.
A zero limit preserves the established generation-zero-only behavior. Exact
cross-generation total-fitness ties retain the earlier winner. If a later
child is all-invalid, that completed child is promoted, the run stops
successfully, and the earlier valid global winner remains in the result.

EVO 0.17.0 appends `termination_reason` to `evo_result_t`. Every successful
run now records `EVO_TERMINATION_GENERATION_LIMIT` or
`EVO_TERMINATION_ALL_INVALID`; `EVO_TERMINATION_NONE` remains the exact
zero-initialized, failed, and destroyed state. This is explicit outcome
evidence only: callback order, RNG replay, ownership, winner selection,
generation counts, and the two existing stopping decisions are unchanged.

EVO 0.18.0 appends one versioned `generation_statistics` record to the result.
It reports the most recently committed generation's index, valid and invalid
counts, stable generation-local best, and component-wise valid-fitness sums.
Generation zero and every promoted child are summarized in deterministic
ascending candidate order, but only the latest record is retained. Statistics
therefore allocate no history and remain constant-space with respect to
`generation_limit`.

EVO 0.19.0 appends an optional synchronous generation observer to
`evo_config_t`. It receives independent read-only result and statistics
snapshots after generation zero and every successfully promoted child. A
zero-limit run emits one event and an N-transition run emits N+1 events. The
observer returns no control decision, owns no view, allocates no history, and
never receives provisional or failed-generation evidence.

EVO 0.20.0 appends an optional synchronous application stop decision and its
caller-owned context to `evo_config_t`. EVO consults it after a committed
generation only when another child could otherwise be attempted. Returning
true ends the run successfully at that exact generation with
`EVO_TERMINATION_APPLICATION_REQUESTED`. A zero-limit generation and a
promoted all-invalid child already have natural terminal reasons and therefore
do not invoke the stop callback. When both callbacks are configured, stopping
is decided first and the observer receives a fresh snapshot containing the
final reason.

EVO 0.21.0 formalizes hard validity and soft-penalty evidence under public
fitness-comparison policy version 1. `is_valid` remains the non-tradeable gate;
hard-invalid candidates are never evaluated or ranked. For valid candidates,
`constraint_penalty` is a finite non-negative magnitude and `fitness.total`
remains the caller-computed scalar that already reflects any desired penalty.
EVO never subtracts it again. One comparison authority maximizes `total` and
resolves exact ties by earlier generation and lower population index.
Generation-statistics schema version 2 records that comparison-policy version.

EVO 0.22.0 adds bounded deterministic population diversity. By default it
measures the normalized byte mismatch of every unordered pair of hard-valid
candidates in fixed lexicographic index order. Consumers may instead provide a
versioned deterministic domain-distance callback returning a finite value in
`[0, 1]`. `max_diversity_work` bounds the all-valid worst case before any run
callback: built-in units are byte comparisons and domain units are callback
invocations. Invalid candidates are excluded, zero or one valid candidate
reports zero without pair work, and generation-statistics schema version 3
publishes the metric identity, pair count, work, value, and metric kind. The
measurement consumes no operator RNG and does not change selection.

EVO 0.23.0 adds deterministic convergence and stagnation policy version 1.
Zero-initialized controls preserve the prior generation-limit behavior.
Consumers may enable a finite global-best fitness target, patience over
improvements greater than a finite non-negative tolerance, and an independent
normalized diversity floor. Decisions use only committed winner and statistics
evidence and count child generations, never elapsed time. Public reasons now
distinguish `EVO_TERMINATION_CONVERGED` and `EVO_TERMINATION_STAGNATED`.
Coincident reasons use the fixed order all-invalid, converged, stagnated,
generation limit, then application requested.

EVO 0.24.0 appends a caller-bounded elite-count mode and defines elite policy
version 1. Explicit counts from zero through `population_size` preserve as many
distinct hard-valid parents as are available, ranked by the common comparison
authority and cloned best-to-worst into a child suffix. Ordinary offspring fill
the prefix. When that prefix is odd, its last child uses the next pair-selection
stream, a byte clone of the selected valid parent, and the normal child-indexed
mutation stream; it allocates no scratch sibling and invokes no crossover.
Elite copies consume no RNG and invoke no callback. Leaving the new fields zero
preserves the pre-0.24.0 sequence exactly: odd populations retain one stable-
best tail and even populations retain none.

EVO 0.25.0 appends versioned parent-selection policy 1. A zero-initialized
configuration remains tournament selection and replays the pre-0.25.0 parent
sequence byte-for-byte. Rank mode assigns every hard-valid parent one stable
rank through the common fitness comparator, then samples one unbiased integer
ticket from exact caller-configured linear rank weights. Invalid parents have
no rank or interval; exact fitness ties retain lower population indexes. Rank
arithmetic is checked against the configured all-valid worst case before child
allocation or callbacks, and selection provenance is carried through child
production, evaluation, promotion, and bounded-run evidence.

EVO 0.26.0 appends byte-operator policy 1 and explicit crossover/mutation
selectors. Zero-valued consumer modes preserve callback, clone, no-op, and RNG
compatibility. Opt-in byte modes provide one-point, two-point, and uniform
crossover plus one-byte nonzero-XOR mutation over the complete bounded genome.
Built-ins use the existing pair/child operator streams, bypass callbacks,
allocate no memory, and propagate their selector/version provenance through
child production, evaluation, promotion, and bounded-run evidence. These are
generic byte-genome utilities, never raw-C-source transformation mechanisms.

EVO 0.27.0 appends disabled-by-default adaptive mutation policy 1. Enabled
runs clamp the requested base rate into finite caller bounds, then derive each
next rate from committed strict-improvement and inclusive diversity evidence.
Rates rise by a bounded step on stagnation, reset to the minimum on improvement
when configured, and never consume RNG or hidden history. Generation-statistics
schema 4 records the prior/effective rates, bounds, stagnant count, clamp/reset
facts, and explainable reason. That same effective rate is carried through
consumer and reference mutation, evaluation, promotion, and replay. Disabled
zero payload preserves the 0.26.0 static operator stream exactly.

EVO 0.28.0 appends disabled-by-default secure-erasure policy 1. Every live
result and private population owner retains its exact allocation size plus a
stable policy/backend disposition. Enabled cleanup erases each complete genome
or evaluation range exactly once immediately before its sole release on both
success and failure paths. CMake and GNU Autotools independently detect
`explicit_bzero`; unsupported platforms use the reviewed volatile-byte
fallback. Disabled cleanup invokes no erasure primitive and preserves ordinary
release without claiming that freed bytes were scrubbed.

EVO 0.29.0 adds endian-stable checkpoint format 1 and deterministic resume.
An opt-in synchronous checkpoint observer receives a committed snapshot after
stop and generation-observer delivery. The canonical little-endian format
persists exact configuration identity, current population and evaluations,
global winner, RNG/operator versions, schema-4 statistics, adaptive rate, and
patience state without serializing pointers or native padding. Untrusted
inspection is allocation-free and budgeted; CRC-32 detects corruption but is
explicitly neither authentication nor encryption. Resume requires exact
canonical configuration plus nonzero reattached problem/context identities,
allocates only after validation, never replays callbacks for the restored
generation, and reproduces the uninterrupted suffix. The ordered checkpoint
view and per-candidate accessor are the mandatory human-readable audit
projection over the binary representation.

EVO 0.30.0 adds disabled-by-default deterministic population-storage
recycling. Enabled positive-length runs retain exactly two run-local logical
slots, alternate their active and reusable roles, and reset the complete former
active ranges before reuse. Stable owner identities are never addresses. The
allocation-free population-storage registry projects the complete role,
capacity, provenance, handoff, reset, and erasure history after every committed
generation, while exact private pointer/count owners remain authority.
Disabled execution preserves the 0.29.0 allocation path and operator replay;
differential tests prove identical genomes, fitness, statistics, callback
traces, stopping, and results. Checkpoint format 2 persists and validates the
logical registry, and resumed execution reconstructs local owners without
serializing allocator state.

EVO 0.31.0 adds opt-in bounded parallel candidate evaluation. Zero workers
preserve the complete serial path. A positive caller-bounded count requires an
explicit thread-safe evaluator declaration and an exact library scratch budget.
Validity remains serial; evaluator callbacks run in fixed candidate waves; and
fitness records commit only in ascending valid-candidate order after every
worker joins. Non-finite fitness cancels later waves and publishes no partial
generation. A synchronous candidate-ordered schedule exposes stable logical
worker identity, wave, completion/failure/cancellation, and commit order without
using platform thread identity or timing as authority. Checkpoint format 3 binds
the policy and committed provenance but serializes no live scheduler state.

EVO 0.32.0 adds the versioned `EVO-CORE-001` correctness, search-quality,
runtime, and memory-evidence baseline. Fixed byte-genome oracles and complete
semantic replay gate four serial/parallel and allocate/recycle cases before
measurement. Canonical JSON retains every policy, seed, generation trace, and
raw sample in stable order. Runtime, CPU, and process-RSS values are
reporting-only; exact library-requested heap bytes state their scope and
exclusions. CMake and GNU Autotools expose bounded smoke and intentionally
dispatched extended targets, and Markdown is derived only by parsing and
validating the JSON artifact.

EVO 0.33.0 adds four bounded reference consumers built only against staged
CMake or Autotools installations: repository scoring, compiler-option model
search, scheduler tuning, and FPGA placement exploration. Every adapter uses a
fixed explicit fixture, hard constraints, soft penalties, resource budgets,
complete generation traces, and exact second-run replay. Repository scoring
proves format-3 checkpoint inspection and resume; scheduler tuning proves
three-worker evaluation with complete candidate-ordered schedules. Canonical
JSON must equal the reviewed golden before its Markdown projection is written.
These adapters introduce no accelerator and make no source-optimizer claim;
compiler options neither analyze nor emit C source.

EVO 0.34.0 adds the first private source-optimizer foundation. A strict
versioned manifest declares authorized roots, identities, exact CMake or
Autotools commands, target/workload policy, and nested resource budgets. EVO
copies authorized regular files into a read-only snapshot and a separate
derived workspace, normalizes the retained compilation database into a stable
translation-unit registry, invokes baseline gates only through an explicit
bounded provider, and then byte-verifies the input against the snapshot. A
successful atomic output contains `baseline.json`, `baseline.md`, and the
read-only snapshot; transient workspace artifacts are removed. The FNV labels
are deterministic diagnostics rather than authentication or sole identity
authority. This release performs no Clang analysis or source transformation
and installs no optimizer executable.

Version 0.3.0 added the independently tested private population-storage
foundation: checked `population_size * genome_size` arithmetic, a
caller-provided total slab budget, contiguous zero-initialized storage, bounded
non-owning genome views, and fully resetting destruction. That boundary became
the storage foundation for public generation-zero execution in version 0.6.0.

Version 0.4.0 adds private deterministic RNG algorithm version 1 and
generation-zero population initialization. A configured seed deterministically
prefills the complete population slab using an explicit cross-platform byte
order. EVO then calls the optional consumer initializer once per genome in
ascending order. Successful initialization records the seed and algorithm
version; lifecycle rejection preserves existing storage unchanged.

Consumer initializers must be bounded deterministic transformations of their
prefilled genome and context. The RNG is not cryptographically secure.
Population initialization remains private and does not itself validate,
evaluate, select, mutate, cross over, or iterate candidates.

Version 0.5.0 adds a private generation-zero validation and evaluation phase.
An optional validator is applied to every candidate in ascending order, and
only valid candidates are evaluated. Every returned fitness field must be
finite; from version 0.21.0, `constraint_penalty` must also be non-negative.
Higher consumer-computed `fitness.total` wins, with the lower population index
breaking exact ties. EVO records the penalty but never independently applies
it to `total`. An all-invalid population is a completed evaluation state
without a winner.

Evaluation records have a separate caller-provided byte budget and remain
private. Resource, allocation, or malformed-fitness failure preserves the
initialized genome slab as unevaluated within the private phase.

Version 0.6.0 connects those private lifecycle stages to `evo_run`. A missing
evaluator is rejected before allocation. Every non-active-result failure
returns an empty result after releasing private storage. A completed
all-invalid population returns `EVO_ERROR_NO_VALID_CANDIDATE`; it is distinct
from an internal state, resource, allocation, or callback-output failure.

In version 0.6.0, `EVO_SUCCESS` meant that a valid generation-zero winner was
produced, and `generations_completed` therefore remained zero. Version 0.16.0
retains that exact result when `generation_limit` is zero and extends success
to the requested bounded transition sequence when the limit is positive.

Version 0.7.0 adds unbiased bounded-index sampling over the existing private
PCG stream and tournament selection with replacement from valid evaluated
candidates. Higher `fitness.total` wins each tournament, and exact ties select
the lower population index. The operator validates the completed population
before consuming RNG state, performs no allocation, and preserves its output
on failure.

Selection accepts an explicitly seeded private RNG stream. Version 0.11.0 adds
the separate owner that derives a selection stream for each complete pair.
Version 0.25.0 retains tournament as the compatibility dispatch and adds stable
rank-based selection without changing stream derivation or the crossover and
mutation domains. Version 0.16.0 invokes the composed selection boundary from
the bounded public loop.

Version 0.8.0 adds a private probability gate and crossover pair dispatcher.
The gate quantizes `crossover_rate` to a 32-bit threshold and consumes exactly
one version-1 RNG word for every successful pair, including rates zero and one.
When selected, the existing consumer callback receives two read-only parents
and two distinct writable children. Otherwise, or when the callback is absent,
the parents are cloned into their corresponding children.

The operator remains representation-neutral and allocation-free. It does not
itself select parents, mutate children, own a child population, or advance a
generation; version 0.16.0 invokes it through complete-pair composition.

Version 0.9.0 adds a private fixed-rate mutation dispatcher over one bounded
writable genome. Every valid attempt consumes exactly one version-1 RNG word,
including rates zero and one and an absent callback. A selected event invokes
the existing consumer callback exactly once; otherwise the genome remains
unchanged.

The configured `mutation_rate` is the engine-owned per-genome event
probability and is forwarded unchanged as the callback's
representation-specific mutation intensity. Callback code must be
deterministic for fixed bytes, rate, and context and may not consult
unrecorded entropy. The in-place callback has no failure or rollback channel.
This private operator does not allocate storage, implement representation-
specific helpers or adaptive schedules, or advance a generation.

Version 0.10.0 adds an independently owned, zero-initialized child-population
slab. A completed parent population is validated through the same invariant
authority used by parent selection before a separate child allocation is
made. Parent genomes, evaluations, and lifecycle evidence remain read-only.

The appended `max_child_population_bytes` field is a caller-controlled policy
for exactly one child genome slab. Child storage matches the configured parent
dimensions, begins without initialization or evaluation evidence, and may be
destroyed independently. This boundary does not select parents, invoke genetic
operators, mark children complete, swap populations, or advance a generation.

Version 0.11.0 promotes the plain tuple-mixed seed schedule measured in the
EVO-RNG-001 research into production as operator seed-schedule version 1.
Selection, crossover, and mutation streams are independently addressable by
master seed, source generation, pair or child index, and operation domain. RNG
algorithm version 1 and every generation-zero initialization vector remain
unchanged.

The private parent-pair planner maps each complete pair ordinal to consecutive
child slots and performs two deterministic draws through the configured
selection-policy dispatch using that pair's selection-domain stream. It commits
no output until both parents are selected, leaves the completed parent
read-only, and writes no child bytes.
Through 0.23.0, `population_size / 2` complete pairs were planned. Version
0.24.0 instead plans `floor(ordinary_offspring_count / 2)` pairs after resolving
the effective elite suffix; a remaining ordinary slot belongs to the
versioned singleton path.

Version 0.12.0 composes the accepted private boundaries for the next complete
pair. It requires ascending pair order, derives a pair-indexed crossover stream
and one mutation stream per child index, preflights every expected library
rejection, then dispatches crossover and both mutations into the separately
owned child slab.

Successful production records the contiguous child count, source generation,
operator seed-schedule version, and pair evidence. Repeated, skipped, or
mismatched-generation pairs reject before callbacks and preserve child state.
Parents remain read-only, child evaluation evidence remains absent, and an odd
trailing child remains untouched. Consumer callbacks still have no failure or
rollback channel.

Version 0.13.0 completes the trailing slot of an odd child slab by cloning the
completed parent's stable best valid genome byte-for-byte. Policy version 1
requires every complete pair first, consumes no RNG state, invokes no consumer
callback, and records full production count, source generation, operator
schedule version, and odd-tail policy version. A one-member population is
completed directly from its sole valid parent.

Version 0.24.0 retains that path as the disabled-config compatibility subset of
general elite policy version 1. Explicit mode permits counts `0..N`; effective
count is capped by the completed parent's valid count, and every distinct elite
is cloned into the suffix after a full stable ranking dry pass. This general
completion boundary records requested, effective, source-valid, offspring,
singleton, and compatibility-policy evidence.

In version 0.13.0, full production remained distinct from completed-population
evidence: the child was still unevaluated and without validity, fitness, or
best-candidate records.

Version 0.14.0 evaluates a fully produced even or odd child slab through the
same provisional-record engine used for generation zero. Policy version 1
validates every candidate in ascending order, evaluates only valid candidates
in ascending order, rejects every non-finite fitness field, and retains the
lower index on exact total-fitness ties. Evaluation consumes no RNG state and
does not modify genome bytes.

Success preserves child production provenance while committing validity,
fitness, valid-count, and stable-best evidence. All-invalid children are
completed evaluated populations without a best candidate. The shared
completed-population validator accepts either generation-zero or evaluated-
child provenance, so an evaluated child can authorize the next independent
child slab.

Version 0.15.0 adds generation-advancement policy version 1. After validating
both completed populations, their generation lineage, every owned byte range,
and `uint64_t` increment safety, the operation moves the evaluated child into
the parent handle without allocation or copying. The child handle is reset to
zero, and the former parent is released only after ownership transfer.

Every fallible check precedes that no-fail commit suffix. Rejection preserves
both populations and caller evidence; success preserves child genome bytes,
evaluation records, stable-best evidence, and production provenance exactly.
The boundary consumes no RNG state and invokes no callback. Completed all-
invalid children remain promotable because extinction and stopping policy are
separate decisions. Public composition and generation-limit handling remained
separate from this private ownership operation until version 0.16.0.

Version 0.16.0 adds bounded-run policy version 1. Positive limits validate all
transition-only configuration before any consumer callback. For each source
generation in ascending order, EVO allocates one child slab, produces complete
pairs, an optional singleton, and the stable elite suffix, evaluates the
complete child, and uses generation advancement to transfer ownership. The
former parent is released at each successful promotion.

The public result genome is allocated once after generation-zero evaluation.
Only a strictly higher `fitness.total` overwrites that buffer and its complete
fitness evidence; an exact tie preserves the earlier generation. Result
updates occur only after child promotion succeeds. Every public failure
releases all private owners and the result allocation and returns an empty
result. No partial run result is exposed.

## Result Lifecycle

Callers must zero-initialize an `evo_result_t` before first use. A successful
run transfers exclusive ownership of `best_genome` to that result. Reusing an
active result is rejected. The result retains `best_genome_size`, policy
version, backend, and enabled state beside that sole owner.
`evo_result_destroy` applies the retained ordinary or erase-before-release
disposition, resets the full result to zero, and makes it immediately reusable.
The enabled guarantee covers only the exact live EVO allocation, not consumer
copies, aliases, swap, crash artifacts, allocator internals, or persistent
media.

`termination_reason` is nonzero only after `EVO_SUCCESS`. It reports whether
the configured generation limit completed, a later all-invalid generation
ended the run, a fitness target converged, patience or diversity stagnated, or
the application requested stopping after a committed generation.
Generation-zero all-invalid remains an error and leaves the reason as
`EVO_TERMINATION_NONE`.

`generation_statistics` is versioned and nonzero only in a successful active
result. It describes the last committed population, which may be an all-invalid
terminal child even while `best_genome` retains an earlier valid global winner.
The component sums include only valid evaluated candidates. Result destruction
resets the complete record to zero. Schema version 2 also identifies the
fitness-comparison policy that established the generation-local winner.

`generation_observer` is optional. When present, it receives the updated
global winner, latest generation statistics, and the stop reason applicable at
that commit. Snapshot objects and the bounded `const` genome view remain valid
only until the callback returns. Observer state is supplied separately through
`generation_observer_context`; EVO does not inspect or retain it.

`generation_stop` is optional. When present, it receives the same read-only
view schema before the observer, but only at a committed generation from which
the run could continue. The stop view always carries
`EVO_TERMINATION_NONE`; returning true selects application-requested successful
termination without changing the committed winner, statistics, RNG state, or
generation count. Stop state is supplied through `generation_stop_context`.

`max_genome_bytes` is a required caller-provided per-genome policy bound.
`max_population_bytes` separately bounds the private population genome slab,
`max_evaluation_bytes` bounds private validity and fitness records, and
`max_child_population_bytes` bounds one separately owned child slab and is
required only when `generation_limit` is positive. None of these fields is an
arbitrary compiled-in cap.

`max_checkpoint_bytes` bounds capture and untrusted resume input. Enabled
capture writes only to a caller-owned buffer sized through
`evo_checkpoint_size`; EVO never owns or erases that cleartext buffer.

The status values are:

- `EVO_SUCCESS`
- `EVO_ERROR_INVALID_ARGUMENT`
- `EVO_ERROR_OUT_OF_MEMORY`
- `EVO_ERROR_RESULT_ACTIVE`
- `EVO_ERROR_RESOURCE_LIMIT`
- `EVO_ERROR_STATE`
- `EVO_ERROR_EVALUATION`
- `EVO_ERROR_NO_VALID_CANDIDATE`
- `EVO_ERROR_CHECKPOINT_INVALID`
- `EVO_ERROR_CHECKPOINT_INTEGRITY`
- `EVO_ERROR_CHECKPOINT_VERSION`
- `EVO_ERROR_CHECKPOINT_MISMATCH`

See `docs/specs/EVO-001-library-contract.md` for the complete API, ownership,
failure-state, alias, compatibility, and secure-erasure boundaries.

The tournament, crossover-dispatch, mutation-dispatch, child-population,
operator-stream, pair-production, singleton, elite-preservation, child-
evaluation, atomic-advancement, and adaptive-mutation boundaries remain
independently verified beneath the bounded public loop. Versioned checkpoint
capture/resume is now composed at committed-generation boundaries. Population
recycling is composed at the same boundary, and bounded evaluator concurrency
now composes through fixed assignment, complete join, and stable record commit.

## Project Zero Onboarding

EVO's one-time Project Zero onboarding is complete. EVO-P0-002 records the
reviewed `ENGINEERING_READY` handoff into ordinary Catylist, AES, and AEMS
oversight. Project Zero is not rerun for routine pull requests or releases.

The retained manual interface from the canonical `repo_templates` contract is
available only for an explicit Catylist/AEMS reassessment request:

```sh
bash scripts/project-zero inspect
bash scripts/project-zero verify
python3 ../AEMS/scripts/aems_project_zero.py \
  . \
  --output build/aems/project-zero \
  --format all
```

The local workflow reports whether a requested onboarding baseline is
certifiable. It does not replace the retained certification decision or the
ongoing AES/AEMS engineering gates.

The stable GitHub Actions entry points are:

- `P0 Repository Lifecycle` (manual onboarding/reassessment only)
- `Verify Repository`
- `Documentation Report`
- `Repository Compliance`
- `EVO Optimization Experiment`
- `Release Readiness`

See `docs/standards/repository-lifecycle-contract.md` and
`docs/engineering/reports/EVO-P0-002-engineering-ready-certification.md` for
the governing contract and completed certification boundary.
