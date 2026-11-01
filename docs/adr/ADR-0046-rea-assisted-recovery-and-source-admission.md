# ADR-0046: REA-Assisted Recovery and C Source Admission

Status: Accepted
Date: 2026-10-26 (America/Los_Angeles)
Owner: EVO
Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132)
Acceptance: accepted by Donovan Worrell on 2026-10-31 (owner decision, with Catylist ADR-0015 and EVO-004); acceptance governs the design, not implementation evidence
Upstream authority: Catylist's inventory/ADR-0004/ADR-0006/ADR-0007 and applicable adopted AES obligations

## Context

The owner placed REA-assisted equipment/software recovery within EVO on
2026-10-26. [PR #133](https://github.com/dlworrell/evo/pull/133) merged the
investigation plan and workspace guide; it did not implement or qualify the
integration. The issue's checked boxes are not substitutes for exit evidence.
[TASK-38](https://app.notion.com/p/402ae4dd715381bfa629f893f99e0543) still owns
unfinished read-only REA source/history/CI mining. Its owner, scope, priority
and completion criteria remain unchanged.

REA produces reverse-engineering observations, inferences and unknowns. EVO
accepts buildable C projects under [EVO-002](../specs/EVO-002-source-optimizer-contract.md)
and [ADR-0035](ADR-0035-immutable-project-ingestion-and-baselines.md). Analysis
output alone cannot bridge that correctness and execution boundary.

## Decision

Adopt [EVO-004](../specs/EVO-004-rea-recovery-integration-contract.md) as the
companion integration contract, with this ordered handoff:

REA analysis → evidence-linked interface/behavior specification → independently
verified, buildable C baseline → EVO source optimization → retained comparison
and replay evidence.

1. EVO owns the companion adapter, interface/device packages, sidecar metadata,
   source-admission checks, compatible implementation, and replay evidence.
   Catylist owns the ecosystem relationship; AES owns adopted engineering
   requirements; AEMS assesses and indexes evidence without replacing its
   producers. The corresponding Catylist amendment must be accepted separately.
2. REA remains an **external-reference** dependency pinned to
   `9e22da845bf8b447f81539e0ba57d5deb59e9103`. Consume its public CLI/MCP and
   evidence contracts. Preserve its native records, unknown histories and
   original export bytes; link equipment/run metadata beside them. No vendoring,
   submodule, upstream-history merge, fork reclassification or new runtime
   dependency is authorized by adopting this design.
3. Keep REA's runtime and provider execution outside the installed C17 core
   ABI. Use ordinary repository-level modules: future adapter/orchestration in
   `tools/rea_recovery/`, tests in `tests/rea_recovery/`, redistributable fixtures
   in `tests/fixtures/rea-recovery/`, and local review records under
   `docs/engineering/rea-recovery/`. These are planned placements, not existing
   implementations or separately governed ADR-0004 scopes. Existing repository
   standards and coverage apply. Creating a formal scope needs its own complete
   declaration and verified scope-aware enforcement.
4. Require an independently checked specification and C baseline before
   optimizer admission. Assembly/pseudocode is analysis evidence; successful
   compilation or self-round-trip tests do not establish compatibility.
5. Start with one **offline** record decoded to value, unit, status and
   timestamp. No real instrument or private capture has been selected. Public
   synthetic fixtures cannot establish hardware measurement, timing or safety.
6. Require actual isolation, bounded execution and explicit capability checks
   for analysis and project-controlled commands. Keep devices, lab networks and
   credentials out of public CI. AI/MCP orchestration never supplies protective
   interlocks or deterministic control.

## Alternatives considered

- Put the integration in REA: rejected for now; it transfers EVO product and
  governance responsibilities into an external reference and duplicates intake.
- Vendor REA or combine histories: rejected without a demonstrated distribution
  need and explicit license/update ownership. A sibling pinned checkout remains
  suitable for investigation.
- Extend the public C17 core with REA types/runtime: rejected; the layered
  boundary in ADR-0016 remains useful and independently supported.
- Optimize decompiler output directly: rejected; missing source, build,
  semantic or independent-reference obligations cannot become fitness inputs.

## Dependencies and consequences

This documentation can be reviewed before optimizer qualification. Adapter,
offline decoder and admission implementations require their own focused PRs
and evidence. Relevant TASK-38 findings are consumed by reference; this ADR
neither completes its wider mining nor creates a replacement mining task.

Installed analyze/evolve/replay/report acceptance depends on
[EVO #93](https://github.com/dlworrell/evo/issues/93) and artifact support in
[#68](https://github.com/dlworrell/evo/issues/68). The separate
[#69](https://github.com/dlworrell/evo/issues/69) product proof remains open.
Closed provider/command-contract issues do not prove this REA integration.
Crypto and Gmail-archive results retain their original subjects and limits.

Governance acceptance, source compatibility, numerical correctness, optimization
performance, measurement fitness, timing and machinery safety are separate
claims. Neither this ADR nor a green documentation check grants an executable,
hardware, production or ecosystem-wide qualification.

## Security, migration and recovery

EVO-004 specifies the trust boundary and refusal cases. This ADR by itself
grants no execution authority and changes no workflow, runner, credential,
public ABI or existing schema. The build runner, the `rea-recovery` workflow
and the encrypted source vault that implement it are decided separately in
[ADR-0048](ADR-0048-evo-rea-build-runner-and-encrypted-source-vault.md). Existing AES
obligations remain in force; new cross-ecosystem normative requirements belong
in AES, not this ADR. Catylist ADR-0007 is Accepted while repository-specific
implementation qualification remains open.

Adopting the contract requires an explicit owner decision. Publication is only
a proposal. Under the Catylist Code's Law 1, Accepted status must be recorded
before or in the same PR as governed changes that rely on it. Implementations
must record exact source/provider/contract revisions
and preserve prior evidence. A REA upgrade requires a reviewed pin change and
contract/provider/replay comparison. Reverse a merged increment through a
reviewed revert or superseding decision; never rewrite accepted history or
discard evidence to repair a failed comparison.

ADR-0026: not applicable to this documentation increment; it adds no
accelerated structure. Each implementing PR must reassess that obligation.

## Verification required before implementation acceptance

Owner follow-up: every file in the pinned repository pull requires a recorded
source-intake baseline before affected source/provider admission. EVO-004
section 2.1/B0 defines complete inventory, per-file applicable scan/disposition,
default-exclusion visibility, dependency closure and repeat/delta requirements.
The [full-file REA baseline](../engineering/rea-recovery/baseline-9e22da8/README.md)
retains the actual byte/diagnostic scan and its unresolved checks; it supplies
neither the later verified C baseline nor optimizer/hardware qualification.

The [2026-10-27 source-intake refresh](../engineering/rea-recovery/baseline-eb3b90b/README.md)
repeats the full-file scan for verified candidate `eb3b90b` and reconciles its
dependency changes against the original baseline. The original contract and
interface analysis retain their recorded pin; independently reviewed source
admission, provider closure and license approval remain outstanding.

Use the staged acceptance matrix in EVO-004. Preserve malformed-input,
identity-mismatch, independent-oracle, immutable-input, isolation, cleanup,
installed-provider and replay evidence at their exact subjects. Missing,
failed, indeterminate and not-run cases remain visible and cannot authorize
the corresponding next stage. No REA/provider execution or pilot test is
reported by this proposal.
