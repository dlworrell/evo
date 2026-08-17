# EVO 1.0 Roadmap

The operational roadmap is GitHub issue
[#38](https://github.com/dlworrell/evo/issues/38). This document preserves the
version-controlled product boundary and dependency topology.

## Product Goal

EVO 1.0 is a source-to-source evolutionary optimization system for C
codebases. It emits the highest-ranked fully verified source candidate found
within a recorded bounded search as a reviewable patch and reproducibility
package.

EVO 0.43.0 contains the completed deterministic C17 evolutionary-search core,
the private source-optimizer foundation through bounded external-process
orchestration, and the first executable-facing product command contract. It is
not yet the installed standalone application.

The source-optimizer foundation provides strict project-manifest ingestion,
immutable baseline snapshots, normalized compilation-unit and Clang/LLVM
evidence, canonical transformation recipes, three versioned AST-aware C
operations, isolated source-candidate materialization, candidate correctness
assurance, reproducible baseline-versus-candidate measurement, deterministic
structured recipe evolution, and bounded external-process orchestration with
stable commit/cleanup traces and product checkpoint/resume authority.

Issue #64 implements reproducible candidate performance measurement; its
0.40.0 evidence and fitness semantics remain part of the 0.43.0 package
boundary unchanged by the command-contract layer.

EVO 0.43.0 adds the fixed `analyze`, `evolve`, `replay`, and `report` command
registry defined by EVO-003 and ADR-0045. The command layer validates an
already parsed manifest, explicit path roles, checkpoint/replay policy, stable
exit-status semantics, output/stream authority, and the exact production
provider identities, implementation versions, availability, and capability
policy required before external project execution may be authorized. The
planner itself launches no target process, mutates no repository, and exposes
no callback-based substitute for the standalone product path.

The v1 provider policy is `catalyst.evo.provider-policy.v1`. The command
contract selects the production identities owned by #114:

- `catalyst.evo.provider.clang-analysis.v1`;
- `catalyst.evo.provider.clang-ast.v1`;
- `catalyst.evo.provider.linux-bwrap.v1`; and
- `catalyst.evo.provider.local-evaluation.v1`.

`analyze` requires production analysis plus sandboxed execution; `evolve` and
`replay` require all four provider roles; `report` consumes retained evidence
without authorizing target execution. Missing, unavailable, version-mismatched,
or capability-incompatible required providers fail before execution is
permitted. EVO-002A reconciles the historical caller-supplied provider language
of the private 0.34.0-0.42.0 foundation with this product boundary.

Core issues #39 through #55 remain the installed reusable C17 evolutionary-core
track. The core compatibility version is independent of the source-optimizer
product version. Issues #58 through #66 implement the private source-optimizer
foundation without claiming an installed CLI. Issue #67 fixes the command and
configuration contract consumed by the next production-provider and installed
application milestones.

**After #67 lands, #114 is the next dependency-ready implementation boundary.**
Its concrete provider implementation may already be prepared on a separate
branch, but final #114 closure must reconcile against the landed EVO-003
command/provider contract before #93 installs the executable.

## Cross-Cutting Human-Readable Abstraction Gate

Issue #83 and ADR-0026 govern every remaining roadmap issue. EVO may introduce
a compressed, cached, indexed, probabilistic, or otherwise accelerated
structure only when the same change defines:

1. exact reference semantics and canonical authority;
2. a deterministic human-readable audit projection with stable domain order;
3. source identity, construction version, provenance, and resource bounds;
4. complete or explicitly windowed/paginated scope and reconstruction rules;
5. stale, corrupt, incompatible, and fallback behavior; and
6. differential evidence proving equivalence to the explicit reference path.

Caches remain derived state with an exact fallback or recomputation path.
Membership filters and other probabilistic structures are prechecks only and
cannot independently commit acceptance, rejection, ranking, selection,
publication, suppression, or termination. Exact authority must confirm every
committed result.

Every child issue and pull request must identify affected accelerated
structures and projections or explicitly state that the rule is not
applicable. This is a conformance gate, not a new serial dependency in the
spine below.

For issue #67, the command registry and provider-requirement registry are fixed
ordered arrays with direct scans. They are their own exact reference
representations; no accelerated authority is introduced. EVO-HRA-016 records
that issue-specific assessment.

## Phase Topology

| Phase | Issues | Boundary |
|---|---|---|
| Product reconciliation | #57 | Mission, architecture, contracts, governance, and roadmap |
| Core semantics | #39–#42 | Termination, statistics, observation, and stopping |
| Core search quality | #43–#49 | Constraints, diversity, convergence, selection, operators, and adaptation |
| Core durability/performance | #50–#53 | Erasure, checkpoint, storage recycling, and in-process parallel evaluation |
| Core evidence/integrations | #54–#55 | Core benchmarks and bounded consumer adapters |
| Source foundation | #58–#59 | Project ingestion, immutable baseline, and Clang/LLVM analysis |
| Structured source evolution | #60–#62 | Recipes, AST-aware catalogue, and isolated source materialization |
| Candidate assurance | #63–#64 | Build/correctness gates and reproducible performance fitness |
| Whole-codebase search | #65–#66 | Recipe evolution and bounded external-process orchestration |
| Product interface/artifacts | #67, #114, #93, #68 | Product commands, concrete production providers, installed standalone executable, and optimized patch evidence |
| Product proof/release | #69, #56 | End-to-end source proof and 1.0 stabilization |

## Dependency Spine

Core:

`#57 → #39 → #40 → #41 → #42`

`#40 + #43 → #44`

`#39 + #42 + #43 + #44 → #45 → #46 → #47 → #48 → #49`

`#49 → #50 → #51 → #52`

`#41 + #42 + #52 → #53`

`#45 + #49 + #53 → #54 → #55`

Source optimizer:

`#55 + #57 → #58 → #59 → #60 → #61 → #62`

`#62 → #63`

`#54 + #63 → #64`

`#49 + #60 + #61 + #63 + #64 → #65`

`#51 + #53 + #65 → #66 → #67 → #114 → #93 → #68 → #69 → #56`

## Product Interface Handoff

### #67 — command contract

EVO-003 fixes operation names, request schemas, help synopses, manifest/path
roles, checkpoint/replay preconditions, exact production-provider selection,
security/output invariants, and stable exit-status classes. It deliberately
stops before installing or executing a final product binary.

### #114 — concrete production providers

#114 must implement the exact provider identities, implementation versions, and
capability vocabulary admitted by EVO-003. Real providers replace private fake
or caller-supplied seams as the standalone operational path. Unsupported
provider capabilities fail closed; there is no silent weaker execution mode.

### #93 — installed executable

#93 parses the documented CLI into EVO-003 requests, resolves real #114
providers, performs filesystem/path/symlink and installed-asset resolution,
executes valid plans, implements stdout/stderr and signal behavior, and proves
staged CMake/Autotools installation from an unrelated working directory.

### #68 and #69 — artifacts and proof

#68 emits the complete optimized patch/evidence bundle. #69 proves the complete
installed path on reference C projects using the same production providers; a
private test harness or fake provider cannot satisfy the 1.0 product proof.

## 1.0 Gate

The release must prove actual source evolution through the installed standalone
executable. Compiler-option changes or a private test harness alone are
insufficient. The retained proof must include a real C baseline, at least one
source-level change, supported builds, all declared correctness and security
gates, statistically defensible improvement, a reviewable patch, successful
replay, and staged CMake/Autotools invocation of the documented
`analyze`/`evolve`/`replay`/`report` command surface using the production
provider identities.

Any accelerated structure exercised by that proof must also produce its
declared human-readable audit projection and pass reference-equivalence
verification.

"Best" is bounded by the recorded input, targets, workloads, transformation
catalogue, objective, constraints, and search resources. No global-optimality
or universal-equivalence claim is permitted.