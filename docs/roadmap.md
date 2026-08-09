# EVO 1.0 Roadmap

The operational roadmap is GitHub issue
[#38](https://github.com/dlworrell/evo/issues/38). This document preserves the
version-controlled product boundary and dependency topology.

## Product Goal

EVO 1.0 is a source-to-source evolutionary optimization system for C
codebases. It emits the highest-ranked fully verified source candidate found
within a recorded bounded search as a reviewable patch and reproducibility
package.

The completed EVO 0.33.0 code is the deterministic C17 evolutionary-search
core. It is necessary infrastructure, not the completed source optimizer.

Core issues #39 through #55 are represented in that boundary, including
versioned constraints, diversity, convergence/stagnation, and caller-bounded
deterministic elite preservation, stable rank-based parent selection, and
explicit reference byte-genome crossover/mutation operators and evidence-driven
adaptive mutation, opt-in exact secure erasure, and versioned deterministic
checkpoint/resume with an ordered audit projection. Opt-in deterministic
population-storage recycling adds a fixed two-slot lifecycle and complete
address-free registry while preserving the explicit allocation path as its
reference oracle. Bounded parallel evaluation retains serial evaluation as its
reference oracle and projects complete stable candidate assignment and commit
order independent of runtime scheduling. Reproducible core benchmarks retain
explicit correctness oracles, generation traces, and raw measurements while
deriving a validated human-readable projection from canonical JSON. Issue #55
adds four installed reference consumers with exact replay, checkpoint/resume,
bounded parallel evaluation, constraints, stopping, complete JSON evidence,
and explicit source-optimizer non-claims. The core integration track is
complete at this boundary; issue #58 is the next dependency-ready
source-optimizer implementation work. The dependency spine remains unchanged.

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
| Product interface/artifacts | #67–#68 | Analyze/evolve/replay/report and optimized patch evidence |
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

`#51 + #53 + #65 → #66 → #67 → #68 → #69 → #56`

## 1.0 Gate

The release must prove actual source evolution. Compiler-option changes alone
are insufficient. The retained proof must include a real C baseline, at least
one source-level change, supported builds, all declared correctness and
security gates, statistically defensible improvement, a reviewable patch, and
successful replay.
Any accelerated structure exercised by that proof must also produce its
declared human-readable audit projection and pass reference-equivalence
verification.

"Best" is bounded by the recorded input, targets, workloads, transformation
catalogue, objective, constraints, and search resources. No global-optimality
or universal-equivalence claim is permitted.
