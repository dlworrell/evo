# EVO-002: Source-to-Source C Optimizer Contract

Status: Draft 1.0 target
Version: 0.1
Owner: EVO
Governing ADRs: ADR-0016 and ADR-0026

## Purpose

EVO ingests a buildable C codebase and a declared optimization contract,
analyzes its source and measured behavior, evolves structured source-level
alternatives, compiles and validates isolated candidates, and emits the
highest-ranked fully verified C source candidate found within the bounded
search as a reviewable patch and reproducibility package.

This specification defines the product layer above the reusable C17
evolutionary-search core governed by EVO-001. Nothing in this draft claims that
the source optimizer is implemented in version 0.33.0.

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

EVO must not weaken AES-DEV-001, AES-SEC-001, AES-BLD-001, Project Zero, or
downstream repository controls in order to improve fitness.

## EVO 1.0 Conformance

EVO may claim this contract at 1.0 only after retained end-to-end evidence
proves that it can:

1. ingest and baseline a real C project;
2. generate an actual source-level change rather than only compiler options;
3. build the candidate through the supported profiles;
4. pass every declared correctness, security, sanitizer, analyzer, and ABI
   gate;
5. demonstrate a statistically defensible improvement against the baseline;
6. emit a readable patch and complete evidence bundle; and
7. recreate the same source candidate from recorded inputs, versions,
   configuration, and seed.

Issue #69 owns this reference proof. Issue #56 may stabilize 1.0 only after
that proof passes and all non-deferred roadmap requirements are reconciled.

## Related Records

- `docs/specs/EVO-001-library-contract.md`
- `docs/adr/ADR-0001-library-boundary-and-build-system.md`
- `docs/adr/ADR-0016-layered-source-to-source-c-optimizer.md`
- `docs/adr/ADR-0026-human-readable-abstraction-and-audit-projection.md`
- `docs/adr/ADR-0030-versioned-checkpoint-and-deterministic-resume.md`
- `docs/adr/ADR-0031-deterministic-population-storage-recycling.md`
- `docs/architecture.md`
- `docs/algorithms.md`
- `docs/benchmarks.md`
- `docs/engineering/reports/EVO-HRA-001-human-readable-abstraction-audit.md`
- `docs/engineering/reports/EVO-HRA-002-checkpoint-audit.md`
- `docs/engineering/reports/EVO-HRA-003-population-storage-recycling-audit.md`
- `docs/roadmap.md`
- GitHub issues #38, #56 through #69, and #83
