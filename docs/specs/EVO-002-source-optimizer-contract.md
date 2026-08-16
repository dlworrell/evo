# EVO-002: Source-to-Source C Optimizer Contract

Status: Implemented through the 0.39.0 candidate-assurance boundary; draft 1.0 target
Version: 0.39.0
Owner: EVO
Governing ADRs: ADR-0016, ADR-0026, ADR-0035, ADR-0036, ADR-0037, ADR-0038, ADR-0039, and ADR-0040

## Purpose

EVO ingests a buildable C codebase and a declared optimization contract,
analyzes its source and measured behavior, evolves structured source-level
alternatives, compiles and validates isolated candidates, and emits the
highest-ranked fully verified C source candidate found within the bounded
search as a reviewable patch and reproducibility package.

This specification defines the product layer above the reusable C17
evolutionary-search core governed by EVO-001. Nothing in this draft claims that
the complete source optimizer is implemented in version 0.39.0. This release
implements strict project ingestion and immutable baselines, normalized
analysis/hotspot evidence, canonical transformation recipes, the initial AST-
aware C transformation catalogue, deterministic isolated candidate
materialization, and candidate build/correctness assurance. Assurance consumes
exact results from a caller-supplied isolated execution provider; portable OS
sandbox implementation remains provider responsibility. Performance fitness
and every later roadmap boundary remain a 1.0 target until their issues land.

## Claim Boundary

EVO does not promise the globally fastest or globally optimal equivalent
program. General program equivalence and exhaustive search over all possible C
implementations are not available product claims.

An EVO result is:

> the highest-ranked candidate that passed every required gate among the
> candidates generated and evaluated under the recorded baseline, targets,
> workloads, transformation catalogue, objective, constraints, and search
> budget.

Every report must retain those qualifiers.

## Terms

- **Input project**: the user-authorized C source and declared build,
  correctness, and workload interfaces.
- **Baseline**: an immutable, fingerprinted snapshot and its recorded build,
  correctness, and measurement evidence.
- **Analysis model**: versioned normalized Clang/LLVM and configured runtime
  evidence over the baseline.
- **Transformation**: a versioned AST-aware source operation with explicit
  targets, parameters, preconditions, dependencies, conflicts, and validation
  obligations.
- **Recipe**: a canonical ordered set of compatible transformation records.
- **Candidate**: a complete isolated source tree produced by applying one
  recipe to the baseline.
- **Admissible candidate**: a candidate that passed every required pre-
  performance build and correctness gate.
- **Finalist**: a ranked candidate subjected to the complete publication gate.
- **Champion**: the highest-ranked finalist that passed the complete gate.
- **Artifact bundle**: the champion patch or source tree and all evidence
  required to review, verify, and replay it.

## Human-Readable Abstraction Contract

EVO preserves human-readable architecture even when it uses machine-optimized
data structures. Every compressed, cached, indexed, probabilistic, or otherwise
accelerated structure must have both explicit reference semantics and a
deterministic human-readable audit projection. No such representation may
become opaque authority.

An audit projection expresses logical domain facts rather than implementation
layout. It must:

- identify the canonical source state, construction policy, representation
  version, and relevant provenance;
- use stable domain identifiers and deterministic ordering;
- expose the logical members, mappings, ranges, relationships, or decisions in
  its declared scope;
- state whether it is complete; and
- for bounded pages or windows, declare bounds, continuation, total when known,
  and a stable reconstruction order.

Representative obligations are:

- a runtime hash table projects to a stable registry view;
- a bitmap projects to an explicit ordered result set;
- an Elias-Fano or other monotone index projects to an ordered event or
  generation window;
- a cache projects its logical result together with source identity,
  freshness, invalidation, and exact fallback evidence; and
- a membership filter reports its precheck result and the exact authority that
  confirmed the committed outcome.

Exact accelerators require differential verification against an explicit
reference representation. Stale, corrupt, incompatible, or unreconcilable
state must fail closed or rebuild from exact authority. Caches are derived
state and may not become the sole canonical source.

Probabilistic structures are prechecks only. They may prioritize or batch exact
work, but may not independently accept, reject, rank, select, publish,
suppress, or terminate a candidate or result. False positives and other
approximations must be tested and must not change committed evidence.

Canonical decoded logical records and their exact reference semantics remain
authority for schema validation, replay, and automated comparison. A checksum,
fingerprint, compressed representation, index, cache, filter, or projection
cannot independently authorize a result. Human-readable reports and audit
projections derive from the canonical records, must be regenerable, and must
reconcile with them. Inability to produce or reconcile the required projection
is a conformance failure; readability does not turn a drifting summary into a
second authority.

Issues and pull requests must identify every accelerated structure, reference
authority, audit projection, resource budget, failure behavior, and
equivalence evidence, or explicitly state that this rule is not applicable.
ADR-0026 governs the complete contract.

The implemented 0.35.0 analysis model introduces no accelerated structure.
Complete bounded arrays and direct scans remain authority, and canonical JSON
plus Markdown enumerate the same ordered records. ADR-0036 and EVO-HRA-008
retain this issue-specific assessment; it does not pre-approve later analysis
indexes, transformation catalogues, candidate caches, or schedulers.

The implemented 0.36.0 recipe model likewise introduces no accelerator.
Explicit owned transformation records and dependency edges plus bounded direct
scans are authority. Canonical JSON embeds the complete portable recipe in the
fixed genome, Markdown enumerates the same logical records, and decode rebuilds
all derived facts from live authority before exact-byte comparison. ADR-0037
and EVO-HRA-009 retain this issue-specific assessment.

The implemented 0.37.0 transformation catalogue also introduces no
accelerator. Three stable static capability records and direct dispatch are
exact authority. Each application retains one complete half-open source range
and exact replacement or an explicit no-change result. Complete registry and
application JSON plus derived Markdown expose every transformation, provider,
AST fact, semantic assumption, validation obligation, and rejection policy.
ADR-0038 and EVO-HRA-010 retain this issue-specific assessment.

The implemented 0.38.0 candidate materializer also introduces no accelerator.
Immutable baseline file order, exact recipe-record identity, and accepted half-
open edit ranges remain authority. Overlap is a hard conflict. A normalized
patch plus complete candidate JSON/Markdown expose every committed change, and
candidate identity is stable across output locations and retain/discard policy.
ADR-0039 and EVO-HRA-011 retain this issue-specific assessment.

The implemented 0.39.0 candidate assurance boundary also introduces no
accelerator. Direct bounded gate-policy arrays and exact execution-provider
outcomes remain authority. Every required gate has an explicit ordered result,
including skipped/rejected states; policy and assurance identities are stable
diagnostics; and canonical JSON plus Markdown expose the same gate trace.
Required fast-gate success controls performance admission and complete finalist
success across both declared release profiles controls champion admission.
ADR-0040 and EVO-HRA-012 retain this issue-specific assessment.

## Optimization Manifest

Every run begins from a versioned manifest. It records at least:

- source repository or tree identity and permitted source roots;
- dependency identities and generated-source policy;
- supported language dialect and target platforms;
- CMake and/or Autotools build profiles and exact command policy;
- test, differential-test, fuzz, sanitizer, analyzer, ABI, security, and
  governance gates;
- benchmark workloads, datasets, target hardware, measurement procedure, and
  improvement threshold;
- fitness components, direction, weights, penalties, and hard constraints;
- transformation catalogue and provider versions;
- seed, population, generation, stopping, checkpoint, worker, and search
  policy;
- filesystem, network, environment, time, memory, storage, and process
  budgets; and
- artifact retention and cleanup policy.

EVO must reject incomplete or inconsistent required policy before executing
project-controlled commands. EVO may not silently infer missing correctness or
performance requirements and present them as user policy.

Version 0.34.0 fixes `catalyst.evo-project-manifest.v1`. It requires one
declared source identity, non-overlapping permitted relative roots, one
retained compilation database, generated-source rejection policy, a CMake or
Autotools frontend, exact argv vectors for configure/compile/correctness and
benchmark stages, benchmark-required policy, C17 target identity, ordered
dependency/toolchain/environment/workload/constraint registries, deterministic
search settings, nested resource budgets with network disabled, and explicit
retention/cleanup policy. Unknown or duplicate fields and unsupported policy
values reject; caller outer limits must be at least as strict as every
manifest-requested resource.

## Baseline Contract

EVO captures the complete authorized input identity before candidate work. The
baseline is immutable for the run and includes normalized source, dependency,
build, toolchain, target, workload, and environment evidence.

Baseline preparation must:

1. validate every authorized root and reject path escape;
2. capture or validate the applicable compilation database;
3. execute declared baseline build and correctness gates under process policy;
4. determine benchmark eligibility; and
5. retain machine-readable success or failure evidence.

No analysis, candidate, or artifact operation may modify the input project or
baseline snapshot.

The implemented 0.34.0 capture uses one atomic output transaction. It copies
each authorized regular file into a read-only snapshot and separate writable
derived workspace, parses the compilation database into a complete stable
translation-unit registry, runs the four baseline gates only through a
caller-supplied bounded execution provider, byte-verifies the source after the
gates, and removes the workspace. While the incomplete marker remains,
publication temporarily restores owner-write permission only on the snapshot
root so its parent entry can move out of the staging directory; no callback
remains reachable, all descendants remain read-only, and the root is
immediately re-hardened and synchronized. EVO then commits canonical
JSON/Markdown evidence and makes the complete output read-only. The capture API
and target remain private and uninstalled until the product interface is fixed.

`catalyst.evo-project-baseline.v1` distinguishes ingestion errors through the
capture status and commits valid captures as `eligible`, `build-failed`,
`correctness-failed`, or `benchmark-ineligible`. A failed ingestion publishes
no completed baseline. Exact snapshot bytes and complete ordered registries are
authority. Versioned FNV-1a labels are replay diagnostics only and provide no
authentication or sole identity authority. ADR-0035 defines the complete
ownership, normalization, failure, and evidence contract.

## Analysis Contract

The declared Clang/LLVM provider produces versioned normalized evidence for
translation units, declarations, stable source identities, call and control-
flow relationships, applicable data-flow evidence, compiler optimization
records, and configured runtime profiles.

Static and dynamic evidence remain distinguishable. Missing runtime evidence
must not be represented as zero cost. Provider, compiler, target, flags, and
schema versions are part of the analysis identity.

Analysis ranks supported opportunities but does not modify source or guarantee
that a proposed change will improve performance.

The implemented 0.35.0 boundary accepts only an eligible committed baseline.
One declared bounded provider receives the read-only snapshot and complete
normalized compilation-unit registry together with exact provider, Clang,
LLVM, target, flags, runtime-profile, and resource-policy identities. Provider
results are borrowed only at callback return. EVO deep-copies, bounds,
validates, cross-references, and canonically orders every retained record.

Stable source locations distinguish spelling and macro-expansion locations;
each expansion references one explicit spelling record. Generated-source
locations return `unsupported-evidence` under the v1 ingestion policy.
Declarations name normalized translation units and locations. Calls,
control-flow, data-flow, compiler records, and runtime records must resolve to
accepted declarations and locations. Unknown enums, duplicate identities,
missing references, undeclared workloads, zero sample counts, malformed text
or ranges, and over-limit output fail closed before evidence publication.

Runtime profile state is exactly `not-configured`, `unavailable`, or
`available`. The first has no identity, the other two have one, and only an
available profile may carry positive `sample-count` records. Absence therefore
never becomes a measured zero. EVO derives opportunities from missed compiler
records and positive runtime samples and ranks them by runtime-evidence
presence, descending summed samples, descending missed-record count, then
stable location identity.

After every provider return, including an error return, EVO re-enumerates the
committed baseline snapshot and verifies every path, size, hardened mode, and
byte fingerprint. Drift returns `baseline-changed`. A successful atomic output
contains bounded read-only `analysis.json` and `analysis.md`; both expose
complete translation-unit, source-location, structural, compiler, runtime, and
opportunity records from one owner. The output uses the lower of the caller's
outer evidence limit and the immutable manifest's declared evidence budget.
Analysis invokes no evolutionary operator and performs no source write.
ADR-0036 defines the complete ownership, ranking, failure, identity, and
evidence contract.

## Transformation-Recipe Contract

One EVO source genome represents one complete canonical recipe. Raw C source
text is not a byte genome and must not be directly crossed over or mutated.

Every transformation record includes:

- baseline and analysis identity;
- stable source target;
- transformation identifier and implementation version;
- bounded parameters;
- structural and semantic preconditions;
- dependency and conflict information; and
- replay and provenance evidence.

Recipes have a versioned canonical encoding, checked length and resource
bounds, deterministic hash, and exact compatibility rules. Unknown, stale,
cyclic, conflicting, malformed, or over-budget recipes reject before source
materialization.

The implemented 0.36.0 boundary accepts only an eligible committed baseline,
its completed normalized analysis, one canonical catalogue, bounded proposal
records, and one exact fixed genome size. Catalogue entries are ordered by
transformation identity/version and declare supported source-location kinds,
typed parameter schemas, preconditions, dependencies, and conflicts. A
proposal supplies only record identity, target identity, transformation
identity/version, and parameter values. EVO resolves and deep-copies every
source range, spelling relationship, dependency record, conflict declaration,
opportunity rank, and compiler/runtime provenance from live authority.

Every dependency must select exactly one matching transformation record.
Missing and ambiguous closure, selected conflicts, and cycles have distinct
statuses. Canonical composition uses a stable topological traversal with
record identity breaking ties among ready records. Parameters and all
catalogue/analysis-owned arrays retain their validated stable order.

`catalyst.evo-project-recipe.v1` is embedded in a fixed genome after the
`EVORCPG1` magic and a little-endian 64-bit payload length; mandatory zero
padding fills the remaining caller-selected size. Decode extracts only
proposal-bearing fields, rebuilds the complete model from the current
baseline, analysis, and catalogue, regenerates the full genome, and accepts
only an exact byte match. Serialized derived fields, alternate JSON forms,
padding, and the diagnostic FNV label cannot become authority.

The complete in-memory recipe and Markdown audit view derive from one owner.
Every model collection and string has an explicit caller limit; combined
genome and Markdown evidence capacity is bounded by the lower of caller policy
and the immutable manifest evidence budget. Build and decode verify the
baseline snapshot before and after work, invoke no callback or evolutionary
operator, write no source, and materialize no candidate. ADR-0037 defines the
full ownership, ordering, encoding, replay, failure, evidence, and
Human-Readable Abstraction contract.

### Implemented AST-aware catalogue boundary

The implemented 0.37.0 catalogue identity is
`catalyst.evo.c.ast-transformations`, version 1. It contains exactly three
versioned operations:

- plain nonvolatile same-declaration assignment to compound assignment for a
  selected arithmetic or bitwise operator;
- removal of double negation from a scalar controlling expression in a
  selected C statement context; and
- unsigned multiplication by a verified decimal power-of-two constant to a
  type-preserving bounded left shift.

Each catalogue capability declares required typed parameters, supported
normalized AST forms, deterministic formatting, idempotence, semantic
assumptions, validation obligations, and unsupported categories. Version 1
rejects macro targets, target comments, preprocessor directives, language
extensions, unproved alias assumptions, ambiguous targets, volatile access
where relevant, inconsistent declaration/type evidence, malformed ranges, and
source/provider token disagreement.

One application accepts the live immutable baseline/analysis/recipe chain and
a bounded normalized AST provider. EVO resolves the recipe line/column target
against the exact source bytes, requires an identical provider half-open range,
checks all component ranges, basic identifier/operator tokens, and independently
parsed shift-constant bytes, then re-verifies the snapshot after provider
execution and evidence construction. The provider has explicit
`network_access:false` and supplies no retained compiler pointer or source
write.

Success owns either one exact edit or an `already-satisfied` no-change record.
`catalyst.evo-c-transformation-application.v1` retains all authority,
provenance, AST, byte-range, before/replacement, formatting, assumption, and
obligation evidence. Application invokes no compiler or correctness gate,
writes no source, and materializes no candidate. ADR-0038 defines the complete
provider, semantics, token, ownership, resource, failure, identity, and
Human-Readable Abstraction contract.

Mutation and crossover operate on complete transformation records. Any repair
policy is explicit, versioned, deterministic, and recorded. Hidden heuristic
repair is prohibited.

## Candidate Materialization Contract

Each accepted recipe receives a fresh isolated workspace derived from the
baseline. EVO applies records in canonical order and emits:

- candidate source-tree identity;
- normalized reviewable patch;
- changed-file and changed-range inventory;
- complete transformation provenance; and
- completed or failed materialization evidence.

Path traversal, symlink escape, unauthorized generated-source mutation,
ambiguous targets, partial application, or budget exhaustion fail atomically.
A failed materialization publishes no completed candidate identity.

The same baseline, provider, catalogue, and recipe identities must reproduce
byte-identical candidate source and patch.

The implemented 0.38.0 transaction requires one current application per recipe
record, revalidates immutable before/replacement bytes and fingerprints, sorts
edits deterministically within immutable baseline file order, and rejects any
overlap. It builds beneath a private staging directory, uses an incomplete
publication marker, and supports explicit `retain` or `discard` source-tree
policy. Success publishes `candidate.patch`, `candidate.json`, and
`candidate.md`, plus `candidate/` when retained. Baseline freshness is checked
before output reservation and again before commit; failure cleans the reserved
output and publishes no candidate view. This boundary executes no compiler,
correctness gate, benchmark, ranking step, commit, push, or deployment. ADR-0039
defines the complete materialization contract.

## Build and Correctness Contract

Candidate commands execute only in isolated resource-bounded processes under
the manifest's command, environment, filesystem, network, timeout, memory,
storage, and process-count policy.

Validation is staged:

1. **preflight** validates recipe and workspace authority;
2. **fast candidate gates** establish build and minimum correctness before any
   performance fitness;
3. **measurement gates** establish benchmark eligibility; and
4. **complete finalist gates** establish publication eligibility.

A candidate failing a required fast gate receives no performance fitness. A
candidate failing any complete finalist gate cannot become the champion. The
next eligible finalist is processed through a deterministic recorded policy.

The complete gate includes every manifest-required unit/integration,
differential, fuzz, sanitizer, analyzer, ABI, security, toolchain, and AEMS/AES
obligation. EVO claims correctness only with respect to those recorded
obligations.

## Measurement and Fitness Contract

Baseline and candidate measurements use comparable recorded conditions and a
versioned procedure defining warmup, repetition, ordering, timeout,
aggregation, outlier, variance, and tolerance policy.

Runtime measurement is platform-tolerant evidence, not bit-for-bit
deterministic state. Logical candidate generation, ordering, recipe identity,
gate decisions for fixed evidence, and fitness derivation remain deterministic.

Every fitness component and total must be finite and derivable from retained
measurements, declared direction, weights, and penalties. Correctness remains
a hard gate rather than a tradeable fitness component.

## Parallelism and Checkpoint Contract

Source candidate compilation and execution use isolated external workers,
distinct from EVO Core in-process callback parallelism. Completion order may
vary, but evidence commits in stable candidate order.

A product checkpoint binds:

- EVO Core checkpoint and algorithm versions;
- baseline and analysis identities;
- provider and transformation-catalogue versions;
- recipe population and candidate evidence;
- toolchain, target, workload, manifest, and measurement identities; and
- product checkpoint and artifact schemas.

EVO Core checkpoint format 3 supplies only the committed evolutionary state,
algorithm/configuration identity, logical population-storage registry,
bounded parallel-evaluation provenance, corruption check, and ordered audit
projections defined by ADR-0030 through ADR-0032 and EVO-HRA-002 through
EVO-HRA-004. Its CRC-32 and FNV-1a values are not
authentication, provenance, confidentiality, or product-level resume authority.
A product checkpoint must wrap the Core bytes with the remaining identities
above, an explainable projection of those bindings, and approved authentication
whenever bytes cross a trust boundary. It must reject a Core checkpoint whose
decoded projections cannot be reconciled with the product record.

Any incompatible identity rejects resume before executing a candidate. Every
worker is joined, terminated, and cleaned before failure or checkpoint state is
published.

## Standalone Application Contract

EVO 1.0 requires a real installed executable, not only private C functions,
library examples, workflow scripts, or an in-tree test harness. CMake and GNU
Autotools installations must place the same executable in the documented
binary directory and staged-package tests must invoke that installed path
without source-tree or private-header access.

The executable provides stable `--help` and `--version` behavior and explicit
`analyze`, `evolve`, `replay`, and `report` operations. Each operation accepts
documented manifest, input, output, checkpoint, and evidence paths as
applicable; it may not infer a hidden repository, working directory, network
service, credential, cache, or asset as authority. Relative-path resolution,
existing-output behavior, atomic publication, standard input/output/error use,
terminal versus non-terminal behavior, and platform support are part of the
versioned interface.

Exit statuses distinguish at least usage/configuration rejection, failed
ingestion, failed baseline build, failed correctness, benchmark ineligibility,
analysis/materialization/candidate failure, no eligible champion, replay
mismatch, interrupted execution, and internal failure. Human diagnostics go to
standard error; canonical evidence and explicitly requested machine output do
not become interleaved with prose. Signals and cancellation stop new work,
join or terminate owned workers, preserve only contractually complete
checkpoints/artifacts, clean incomplete workspaces, and return the documented
status.

Issue #67 defines product orchestration and command semantics. Issue #93 is the
delivery gate for the installed executable, package parity, help/version,
subcommands, paths, exit statuses, signals, and clean-environment behavior.
Artifact publication and the end-to-end proof cannot bypass that gate.

## Champion and Artifact Contract

The emitted champion must have passed the complete finalist gate. The artifact
bundle contains at least:

- baseline identity and original comparison evidence;
- champion source patch or source tree and candidate identity;
- canonical transformation recipe and changed-range provenance;
- toolchain, target, workload, environment, and policy identities;
- all required correctness, security, ABI, and governance results;
- raw and derived performance measurements;
- complete fitness derivation, generation lineage, and termination reason;
- known limitations and search/claim boundary;
- checksums and schema versions; and
- replay instructions and required external dependencies.

Canonical machine-readable evidence is authoritative for verification and
replay. Human-readable reports derive from it and must satisfy the audit-
projection contract above. EVO does not automatically apply or publish the
patch.

## Replay Contract

Replay first verifies every recorded checksum, schema, provider, toolchain,
baseline, catalogue, workload, and policy identity. It then rematerializes the
same candidate source and patch and reruns the declared validation and
comparison procedure.

Replay requires identical logical candidate identity and decisions. Runtime
measurements need only satisfy the recorded platform and statistical
tolerances; exact timing equality is not required or claimed.

## Security Boundary

Source projects, manifests, compilation databases, checkpoints, tool output,
patches, and evidence are untrusted inputs unless separately authenticated.
Implementations must defend against malformed syntax, integer overflow,
resource exhaustion, command injection, path traversal, symlink escape,
environment leakage, process escape, unexpected network access, malicious
build scripts, corrupt checkpoints, and artifact substitution.

EVO must not weaken AES-DEV-001, AES-SEC-001, AES-BLD-001, the retained
Project Zero onboarding certification, or downstream repository controls in
order to improve fitness. Ordinary product changes use the current AES/AEMS
gates and do not rerun Project Zero.

## EVO 1.0 Conformance

EVO may claim this contract at 1.0 only after retained end-to-end evidence
proves that it can:

1. ingest and baseline a real C project;
2. generate an actual source-level change rather than only compiler options;
3. build the candidate through the supported profiles;
4. pass every declared correctness, security, sanitizer, analyzer, and ABI
   gate;
5. demonstrate a statistically defensible improvement against the baseline;
6. emit a readable patch and complete evidence bundle;
7. recreate the same source candidate from recorded inputs, versions,
   configuration, and seed; and
8. perform analyze, evolve, replay, and report through the installed standalone
   executable from both staged build frontends.

Issue #69 owns this reference proof. Issue #56 may stabilize 1.0 only after
that proof passes and all non-deferred roadmap requirements are reconciled.

## Related Records

- `docs/specs/EVO-001-library-contract.md`
- `docs/adr/ADR-0001-library-boundary-and-build-system.md`
- `docs/adr/ADR-0016-layered-source-to-source-c-optimizer.md`
- `docs/adr/ADR-0026-human-readable-abstraction-and-audit-projection.md`
- `docs/adr/ADR-0030-versioned-checkpoint-and-deterministic-resume.md`
- `docs/adr/ADR-0031-deterministic-population-storage-recycling.md`
- `docs/adr/ADR-0035-immutable-project-ingestion-and-baselines.md`
- `docs/adr/ADR-0036-clang-llvm-analysis-and-hotspot-model.md`
- `docs/adr/ADR-0037-versioned-source-transformation-recipes.md`
- `docs/adr/ADR-0038-ast-aware-c-transformation-catalogue.md`
- `docs/architecture.md`
- `docs/algorithms.md`
- `docs/benchmarks.md`
- `docs/engineering/reports/EVO-HRA-001-human-readable-abstraction-audit.md`
- `docs/engineering/reports/EVO-HRA-002-checkpoint-audit.md`
- `docs/engineering/reports/EVO-HRA-003-population-storage-recycling-audit.md`
- `docs/engineering/reports/EVO-HRA-008-project-analysis-audit.md`
- `docs/engineering/reports/EVO-HRA-009-project-recipe-audit.md`
- `docs/engineering/reports/EVO-HRA-010-c-transformation-catalogue-audit.md`
- `docs/roadmap.md`
- GitHub issues #38, #56 through #69, #83, and #93
