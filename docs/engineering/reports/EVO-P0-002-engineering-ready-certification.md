# EVO-P0-002 Engineering Ready Certification

Status: Approved upon merge of the focused issue #95 certification change
Owner: EVO
Reviewer: dlworrell
Decision date: 2026-08-09
Governing standards: AES-002, AES-003

## Decision

EVO is approved to transition from Project Zero `CERTIFICATION` to
`ENGINEERING_READY`. Project Zero has completed its one-time purpose: bringing
an existing repository into initial Catylist, AES, and AEMS oversight with an
explicit manifest, ownership, contracts, workflows, evidence, and review
boundary.

This decision is effective only when the separate pull request closing issue
#95 is reviewed and merged by `dlworrell`. That merge is the traceable reviewer
approval; the proposal branch cannot approve itself.

## Reviewed Baseline

- Repository: `dlworrell/evo`
- Squash commit: `49b56148d81671f605ceaf1a544370fc0c6283da`
- Git tree: `849b0867e0418c4d9b3736a89e8802f5b62cfdb7`
- Product boundary: EVO 0.34.0 deterministic C17 core plus private immutable
  C-project ingestion and baseline-capture foundation
- Certification issue: <https://github.com/dlworrell/evo/issues/95>
- Reviewed implementation PR: <https://github.com/dlworrell/evo/pull/94>

The reviewed PR contained one issue-specific commit, was based directly on
the preceding `main`, and passed all eleven hosted workflows before squash
merge: AES security governance, native build parity, documentation,
repository compliance, repository verification, reference adapters, CI, core
benchmarks, project ingestion, code quality, and sanitizers.

## Certification Evidence

- Local Project Zero `verify` and `certify` reached readiness level P0-9 with
  zero critical, high, medium, or low findings.
- Project Zero inventoried 251 tracked files in the reviewed tree and proposed
  no remediation.
- Project Zero accepted deferrals: none.
- Project Zero certification status: approved through the separate issue #95
  review and merge boundary.
- Canonical AEMS assessment of the transition manifest must report
  `engineering_ready: true` and no findings before the certification pull
  request is handed off.

## Ongoing Oversight

Project Zero is not a recurring development, compliance, or release gate after
this transition. Ongoing work remains governed by:

- Catylist repository and ecosystem authority;
- AES lifecycle, development, security, and build standards;
- AEMS enforcement and retained evidence;
- EVO's normative contracts and ADRs;
- CMake/Clang/LLVM and Autotools/GNU build parity;
- deterministic tests, security governance, analyzers, and sanitizers; and
- the Human-Readable Abstraction Rule for every accelerated structure.

The manual Project Zero workflow and local engine remain available only for an
explicit Catylist/AEMS reassessment request or invalidated certification. A
feature, version, release, or expanded product scope does not automatically
restart Project Zero.

## Human-Readable Abstraction

This transition introduces no compressed, cached, indexed, probabilistic, or
accelerated structure. It changes lifecycle authority and workflow routing
only, so ADR-0026 requires no new projection.

## Accepted Deferrals

None.
