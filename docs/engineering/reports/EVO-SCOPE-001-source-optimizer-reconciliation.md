# EVO-SCOPE-001: Source-Optimizer Scope Reconciliation

Status: Certification-impacting reconciliation candidate
Date: 2026-08-02
Owner: EVO
Trigger: GitHub issue #57

## Finding

The repository's implemented 0.16.0 code and EVO-001 contract form a coherent
deterministic C17 evolutionary-search core. The prior repository-wide mission,
metadata, roadmap, and 1.0 issue nevertheless treated that generic library and
a compiler-option adapter as sufficient release boundaries.

That boundary did not satisfy EVO's intended source-to-source C optimization
mission. It omitted project ingestion, source and runtime analysis, structured
source transformations, isolated candidate materialization, target-code build
and correctness gates, baseline-comparative measurement, optimized patch
artifacts, and product-level replay.

## Disposition

- Preserve all accepted 0.1.0 through 0.16.0 core implementation, ADRs, tests,
  and retained evidence.
- Reclassify the completed implementation as the EVO Core foundation.
- Add ADR-0016 as the layered product decision without rewriting ADR-0001's
  valid core decision.
- Retain EVO-001 as the core contract and add EVO-002 as the source-optimizer
  1.0 target.
- Correct issue #38, bound issues #48, #51, #53, #54, and #55 to their core
  scope, and require issues #58 through #69 before #56 may stabilize 1.0.
- Require a real evolved C source patch and reproducible proof for release.

## Governance Impact

The expanded product introduces materially new controlled surfaces:

- repository classification and output metadata;
- Clang/LLVM analysis and possible C++ adapter implementation;
- compilation-database and source-location parsing;
- source rewriting and patch provenance;
- untrusted project manifests, build scripts, checkpoints, tool output, and
  artifacts;
- isolated compiler, linker, test, sanitizer, analyzer, and benchmark
  processes;
- filesystem, path, symlink, environment, network, process, storage, time, and
  memory policy;
- runtime measurement tolerances and fitness derivation; and
- product CLI, configuration, transformation, checkpoint, evidence, and
  artifact compatibility schemas.

These surfaces require additive AES-DEV-001, AES-SEC-001, and AES-BLD-001
evidence. Historical audit and certification-candidate reports remain
unchanged.

## Project Zero and AEMS Boundary

This record does not certify the expanded product. The repository remains at
its recorded lifecycle state until the authoritative external review accepts
the new repository class, threat boundary, build/tool dependencies, and
roadmap evidence.

AEMS remains authoritative for lifecycle enforcement. EVO may propose updated
evidence but may not approve its own transition to `ENGINEERING_READY`.

## Required Follow-Up

1. Merge the focused issue #57 documentation reconciliation.
2. Confirm `source-optimization-toolchain` or its replacement as an approved
   Catylist repository class; do not silently invent ecosystem semantics.
3. Update AEMS assessment inputs for the expanded product and retain the new
   report separately from historical evidence.
4. Implement issues #58 through #69 in dependency order with one focused issue
   and PR each.
5. Require #69's end-to-end source proof before #56 can approve 1.0.

## Evidence Reviewed

- `README.md`
- `GOVERNANCE.md`
- `repo.yaml`
- `docs/adr/ADR-0001-library-boundary-and-build-system.md`
- `docs/specs/EVO-001-library-contract.md`
- `docs/architecture.md`
- `docs/algorithms.md`
- `docs/benchmarks.md`
- GitHub issues #38, #48, #51, #53 through #57

