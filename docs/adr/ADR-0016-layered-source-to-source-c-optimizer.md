# ADR-0016: Layered Source-to-Source C Optimizer

Status: Accepted
Date: 2026-08-02
Decision owner: EVO
Scope: Supersedes ADR-0001 as the repository-wide product boundary while
preserving ADR-0001 for the reusable C17 core

## Context

ADR-0001 correctly established `catalyst_evo` as a reusable deterministic C17
evolutionary-optimization library. Work through version 0.16.0 implemented a
bounded multi-generation core with explicit ownership, resource policy,
replay-stable random streams, and correctness-gated fitness callbacks.

The repository mission is broader than that core. EVO is intended to analyze
an existing C codebase, evolve better source implementations, compile and
validate the candidates, and return the best verified source candidate found
within a declared search contract. The prior roadmap could have stabilized the
generic library and a compiler-option adapter as 1.0 without ever ingesting,
analyzing, transforming, or emitting C source.

Clang source analysis and rewriting use C++ interfaces, while the completed
core deliberately exposes a narrow C ABI. Candidate compilation and execution
also introduce a materially larger trust, process, filesystem, and resource
boundary than an in-process callback library.

## Decision

EVO is one product with layered components.

### Evolutionary-search core

`catalyst_evo` remains a reusable C17 library. EVO-001 continues to govern its
public ABI, ownership, deterministic execution, and core evidence. Consumer-
specific source semantics do not enter this ABI.

### Source-analysis and transformation provider

The source optimizer owns a versioned Clang/LLVM-backed provider for C project
analysis and AST-aware source transformation. The provider may use C++ where
required by supported Clang/LLVM interfaces, but C++ types, exceptions, and
ownership do not cross the public C core ABI.

Provider interactions are explicit and versioned. A reviewed subprocess-
backed tool may supplement linked tooling where it improves isolation or
toolchain compatibility, but its command, environment, inputs, outputs, and
version must be recorded.

### Structured source genomes

Raw C source text is never mutated or crossed over as arbitrary bytes. One
source-optimization genome encodes a canonical transformation recipe with
stable targets, transformation identifiers and versions, parameters,
preconditions, dependencies, conflicts, and provenance.

Applying a recipe to an immutable baseline produces a complete candidate
source tree and reviewable patch. The recipe, baseline, provider, and catalogue
identities are sufficient to reproduce the same source candidate.

### Candidate evaluation

Each candidate is materialized in a fresh isolated workspace. Compiler,
linker, test, sanitizer, analyzer, benchmark, and governance commands run under
explicit filesystem, environment, network, time, memory, storage, and process
policy.

Fast correctness gates precede performance fitness. Complete finalist gates
precede publication. A candidate failing any required gate cannot become the
emitted champion regardless of measured speed.

CMake/Clang/LLVM is the authoritative analysis and primary candidate-build
path. The selected candidate must also satisfy the declared Autotools/GNU
validation profile when that profile is applicable to the target project.

### Product orchestration and artifacts

The source optimizer exposes versioned analyze, evolve, replay, and report
operations. Product checkpoints bind the immutable baseline, analysis,
transformation catalogue, recipes, toolchains, workloads, policies, and schema
versions in addition to the EVO Core run state.

The final deliverable is a reviewable source patch or source tree plus a
checksummed evidence bundle. EVO never applies, commits, pushes, merges,
deploys, or publishes that output into a downstream repository automatically.

### Claim boundary

"Best" means the highest-ranked fully verified candidate discovered within the
recorded baseline, targets, workloads, transformation catalogue, fitness
definition, constraints, and search budget. EVO does not claim global program
optimality or universal semantic equivalence.

## Consequences

- The completed 0.16.0 core remains valid and is not rewritten.
- The repository is no longer accurately classified only as a reusable
  library; its metadata and Project Zero/AEMS scope must be reconciled.
- Product schemas evolve independently from the stable C core ABI and require
  explicit compatibility rules before 1.0.
- Clang/LLVM and candidate process execution become reviewed dependencies and
  security boundaries.
- Deterministic source identity, transformation provenance, immutable
  baselines, and isolated candidate workspaces become architectural
  invariants.
- Benchmark evidence must separate deterministic logical replay from
  platform-tolerant runtime measurement.
- A compiler-option search example remains useful core evidence but cannot
  satisfy the source-to-source product mission.

## Alternatives Considered

### Treat the generic library as the complete EVO product

Rejected because it delegates the defining source-analysis, transformation,
and artifact responsibilities to unspecified consumers.

### Evolve raw C text directly

Rejected because arbitrary byte or token crossover cannot preserve program
structure, types, macros, ownership, or reviewable provenance.

### Perform only LLVM IR optimization

Rejected as the product boundary because an optimized binary or transient IR
does not produce the required reviewable C source patch. LLVM IR remains valid
analysis and measurement evidence.

### Replace the C core with a C++ application

Rejected because the tested C17 core and narrow ABI remain valuable reusable
infrastructure. C++ is confined to the source-tooling boundary where Clang
interfaces require it.

### Apply the winning patch automatically

Rejected because downstream maintainers remain authoritative for accepting
and publishing source changes.

## Verification

- Issue #57 records the mission correction.
- Issue #38 defines the dependency-ordered core and source-optimizer roadmap.
- EVO-001 remains the core contract; EVO-002 defines the source-optimizer
  contract.
- Issues #58 through #69 implement the product layers and end-to-end proof.
- Issue #56 may stabilize 1.0 only after #69 demonstrates an actual verified C
  source improvement and reproducible patch artifact.

