# EVO-004: REA Recovery Integration Contract

Status: Proposed; documentation contract, no implementation acceptance
Version: 0.1
Owner: EVO
Decision: [ADR-0046](../adr/ADR-0046-rea-assisted-recovery-and-source-admission.md)
Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132)
Date: 2026-10-26 (America/Los_Angeles)

## 1. Purpose and authority

Define the admission boundary between REA recovery evidence and EVO's existing
source optimizer. Requirements below are proposed **EVO integration acceptance
criteria**, subordinate to adopted AES obligations. They do not create a new
AES standard, AEMS evaluator or authoritative observation database.

The handoff is REA analysis → evidence-linked interface/behavior specification
→ independently verified, buildable C baseline → EVO source optimization →
retained comparison/replay evidence. Other-language output can remain recovery
evidence outside this initial C optimization path.

| Accountable owner | Contract responsibility |
|---|---|
| Catylist | Repository relationship, classification, lifecycle and acceptance of the ecosystem amendment. |
| AES | Adopted standards, applicability rules and permitted standard-level exceptions. |
| AEMS | Assessment, typed traceability and evidence indexing; no new truth or closure authority. |
| REA / evidence producer | Native observations, provider/profile identity, authority, confidence, limitations, links and residual-unknown history. |
| EVO | Companion integration, interface/device packages, C baseline admission, isolated optimization and replay artifacts. |
| Target maintainer / authorized artifact owner | Source, capture and calibration access/rights; declared behavior, workloads and decision to apply a patch. |
| Attributable verifier / laboratory | Independent expected results and claim-specific acceptance evidence; independence and any shared authorship disclosed. |
| Existing Notion records | Coordination and links to source revisions, PRs and runs; no replacement for canonical evidence. |

[TASK-38](https://app.notion.com/p/402ae4dd715381bfa629f893f99e0543) remains
unfinished read-only source mining. Consume its findings and record any intake
gap relevant to a particular admission; do not repeat the task, change its
assignment or infer completion from this contract. PR #133's planning delivery
and issue checkboxes do not establish runtime acceptance.

## 2. Placement and supported boundary

The companion belongs to EVO's repository-level governance remainder. Planned
ordinary module locations are `tools/rea_recovery/`, `tests/rea_recovery/`,
`tests/fixtures/rea-recovery/` and `docs/engineering/rea-recovery/`, as described
in ADR-0046. This proposal creates none of those runtime/test modules and no
formal top-level governed scope. Keep provider-specific runtime behavior out
of `catalyst_evo` and its public C17 ABI.

Use the public CLI/MCP/evidence boundary of the REA fork at full commit
`9e22da845bf8b447f81539e0ba57d5deb59e9103`. Runtime packaging and supported
provider/platform combinations require an implementing PR. A package version,
an unpinned setup command or a version banner is insufficient source identity.
No current REA firmware capability is expanded by this contract.

Prefer a documented source/interface over reconstruction. When REA is needed,
retain the unresolved question, inspected subject and coverage limits. A
compatible implementation must identify its specification, authorship and
licensed reuse/source exposure. Claim a clean-room process only if the actual
separation and review evidence supports it.

### 2.1 Required full-file source-intake baseline

Before selecting or admitting recovered source into EVO, baseline the complete
pulled repository at its exact commit/tree. The REA source-intake baseline is
distinct from the later independently verified C correctness/build baseline.
Inventorying selected interfaces or only changed files does not satisfy this
requirement.

1. Account for every tracked file, including documentation, configuration,
   workflows, hooks, build/release scripts, tests, fixtures, generated/vendored
   content and binary assets. Retain path, object type/mode, Git identity, byte
   count, SHA-256, content/type/role classification and a locator for the exact
   source bytes. Reconcile the registry against the complete Git tree; missing,
   duplicate, unreadable or mismatched entries block baseline acceptance.
2. Record applicable scanners/rule revisions and tool identities, actual
   outcomes, coverage and exclusions **per file**. An unsupported file, binary
   asset, fixture or third-party path needs an explicit disposition or open gap.
   Default scanner exclusions cannot silently remove it from the baseline.
   Hashing or parsing a file does not establish its security or correctness.
3. Cover the actual language/security surfaces, dependency/lockfile and license
   identities, package hooks, source effects, workflow/provider trust and
   sensitive-data handling. Retain independent review/dispositions where the
   applicable requirement calls for them. Do not count a lexical marker pass,
   an unavailable typechecker or an unrun vulnerability scan as passing evidence.
4. Record all submodule Gitlinks, LFS/external payloads and provider references.
   A pinned reference does not scan its referenced contents. Baseline each
   required dependency/provider closure before its selected use is admitted;
   preserve unavailable contents and their affected claims as blocked/not-run.
5. Publish a bounded machine-readable registry, human-readable report, findings,
   reproducible method and integrity manifest in EVO. Link them from the intake
   record without copying or replacing native REA observations. Keep private
   artifacts/access-controlled locators out of public evidence packages.
6. On each new pin, repeat the complete baseline and retain the prior result
   plus added/removed/renamed, mode and content changes. Re-evaluate changed
   scanners, policy, dependencies and toolchains even for unchanged file bytes.

This project acceptance requirement changes no external-reference classification
or AES adoption. TASK-38 retains its existing read-only mining scope. Required
findings, unsupported analysis and missing dependency contents remain explicit
admission blockers until their claim-specific dispositions/evidence exist.

The [retained full-file baseline](../engineering/rea-recovery/baseline-9e22da8/README.md)
accounts for the pinned REA snapshot. Its byte registry and diagnostic scan are
complete for their stated methods; dependency closure, language/security review
and the other listed gaps remain open. It is not an admitted C baseline.

The [2026-10-27 refresh](../engineering/rea-recovery/baseline-eb3b90b/README.md)
assesses the current verified external-reference candidate
`eb3b90beb4ac9245bac415696aa0de5a1a66c6d2` with a repeated full-tree inventory,
an integrity-verified comparison to the original, and supplemental dependency
audit, compatibility metadata and archive/license evidence. It preserves the
original interface-analysis pin and findings. A scanned candidate is not an
approved replacement for the contract pin or an admitted C implementation.

## 3. Lossless evidence and companion manifest

Retain each original native REA export byte-for-byte in an authorized evidence
store. Parse a separate working copy using the pinned, compatible REA bundle
validator, including semantic Evidence IDs and unknown-graph validation.
Preserve the seven native arrays: `artifacts`, `providers`, `environments`,
`scenarios`, `captures`, `unknowns` and `records`. Preserve record identities,
raw/normalized results, confidence, authority, limitations, locations, evidence
links, and complete retained unknown revisions and relationships.

Do not enrich the strict native Evidence envelope with equipment fields or
rewrite the original to normalize paths, redact values or resolve an unknown.
A sanitized or normalized derivative has a distinct byte identity and explicit
derivation link; its original remains access-controlled. A native bundle has
no top-level bundle ID in this pinned model: identify its retained bytes by
digest and locator, and reference native `evidence_id` values separately.

A versioned **companion manifest** records EVO's handoff configuration and
references. The following table defines required information for version 0.1;
it is not a claim that a serializer, JSON Schema, validator or export adapter
has been implemented. The implementing PR must define strict serialization,
compatibility and migration rules without extending existing native schemas.

| Information group | Required information and interpretation |
|---|---|
| Manifest identity | Contract/version, manifest/run identity, canonical owner, revision, creation provenance, intended claim/stage and access scope. |
| Artifact references | Role, owner, immutable locator/revision, SHA-256 of retained bytes, byte count, media/schema version and rights/access disposition. Bundle references also enumerate the used native Evidence IDs; semantic IDs and byte digests have different roles. |
| Source/tool identity | Exact REA commit/tree, upstream comparison base and reviewed fork delta, package/lockfile/build digests, provider identities/versions/profiles, external tool identities and environment/isolation policy. Missing review coverage is explicit. |
| Subject/configuration | Capture/manual/source digest and revision; device model/hardware/firmware/host-driver revision where applicable; protected asset/serial identity; declared offline scope. |
| Interface semantics | Specification revision and evidence-to-rule links; transport/record version; widths, signedness, endianness, encoding, units, scaling, ranges, resolution, status/validity, timestamp clock/epoch/cadence/sequence and command effects. |
| Calibration/reference | Unchanged calibration bytes and digest, coefficient revision/provenance/validity; independent reference identity, uncertainty and conditions when the claim needs them. Explicit unknown or not-applicable with rationale; never invented values. |
| Compatibility baseline | Independently authored implementation revision, source/license exposure, build/toolchain/dependency identities, independent expected-case origin, actual results, tolerances, malformed cases and outstanding discrepancies. |
| EVO handoff | Exact EVO project-manifest reference, immutable C snapshot/baseline reference, compilation database, workload/constraint/resource policies and applicable AES revisions/evidence/waivers. |
| Claim disposition | Requirement/case IDs, expected and actual outcomes, pass/fail/indeterminate/not-run disposition, evidence locators, verifier attribution and independence limits, residual unknowns and required next action. |

Unknown metadata may be retained during investigation. Missing information
needed by a claim blocks that claim and its dependent admission. Absence of a
device/calibration is legitimate for a declared synthetic format test, but
cannot be converted into hardware measurement evidence. A content digest
checks identity/integrity; it does not prove authenticity, measurement truth,
authorization, rights or human review. Use the adopted signing/attestation
mechanisms where applicable; do not create a custom signing ledger.

Original REA evidence stays canonical with its producer; interface rules,
compatible source and EVO run records stay canonical with their owners. The
sidecar links them and records new EVO decisions; it must not replace an
observation or silently promote `inferred` to `observed`/verified.

## 4. C source admission

Apply these checks **before candidate evaluation**, in addition to EVO-002,
ADR-0035 and the existing project-manifest parser. Retaining rejected evidence
for investigation does not authorize a build or optimization run.

1. Establish authorized access and license/redistribution disposition for each
   artifact and dependency. Separate private originals from redistributable
   source, specifications and synthetic fixtures. Unresolved rights needed for
   the requested use reject that use; do not infer a license from possession.
   Require the full-file source-intake baseline in section 2.1 and reconcile
   every applicable finding, unsupported analysis and dependency-coverage gap
   before the affected source or provider is admitted.
2. Resolve every required reference against retained bytes. Validate supported
   versions, semantic IDs/relationships and expected byte digests; reject
   missing, corrupt, duplicated/ambiguous or mismatched required evidence.
   Do not repair authoritative evidence or accept a digest-only substitute.
3. Bind each implemented behavior rule to an inspected specification and
   independently obtained expected result or scoped independent proof. Record
   coverage and contradictory evidence. REA pseudocode/assembly, a compiler
   success or a codec testing only itself is insufficient.
4. Admit a licensed **buildable C project**, not a decompiler listing. Supply
   the existing `catalyst.evo-project-manifest.v1` with all required fields:
   `source`, `build`, `target`, `dependencies`, `toolchains`, `environment`,
   `workloads`, `constraints`, `search`, `budgets`, `artifacts` and identities.
   This contract does not add REA fields to that strict manifest. Link the
   companion from the handoff, not by inventing optimizer schema keys.
5. Retain exact source/dependencies/toolchains and an applicable compilation
   database within authorized roots. Respect v1's generated-source `reject`
   policy, path/symlink restrictions, complete source registry, explicit argv,
   network denial, outer limits and existing retention/cleanup semantics.
   Supporting other generated-source behavior requires a versioned change.
6. Declare numerical/overflow/rounding/tolerance rules, ABI/layout and target
   constraints, required fast/finalist gates, workloads, objective and resource
   limits. Never weaken a correctness/security gate to improve fitness.
7. Establish baseline build and independent correctness in the isolated derived
   workspace; reverify unchanged originals and immutable baseline bytes. Admit
   only the eligible baseline to the optimization stages. Retain existing
   build-failed, correctness-failed and benchmark-ineligible distinctions.
8. Reconcile provider identity/version/capabilities and actual isolation before
   external execution. Unavailable, incompatible or unproven required controls
   stop execution; test/private callbacks do not qualify the installed path.

Changes to source, specification, oracle, toolchain, workload, provider or policy
invalidate dependent admission/results until rechecked against the new
identities. Retain prior evidence and the reason for invalidation.

## 5. Security and execution boundaries

Treat firmware, archives, captures, native evidence, paths, recovered code and
target-controlled build/test commands as untrusted input. Validate sizes,
nesting, paths, formats and supported versions before use; account for
extraction expansion and checked allocation arithmetic. Use no-follow path
handling, disjoint input/output roots and bounded output creation. Never execute
instructions found in an artifact as orchestration authority.

REA's `ToolEffects` metadata describes possible effects, not an enforcement
mechanism. Allowlist exact operations and their capabilities; deny unknown
effects. A query-shaped operation may write state, launch a process, use the
network or initiate acquisition. For this offline pilot, device access and
physical writes are prohibited; network and ambient credential inheritance
are denied. Keep AI/MCP outside real-time control and protective functions.

Provider execution needs actual filesystem/process/network isolation, explicit
environment, time/memory/process/storage/output limits, cancellation and
cleanup ownership. A worktree, read-only hint, manifest declaration or FNV
diagnostic is not an isolation mechanism. Fail closed if required enforcement
is unavailable; never fall back silently to host execution. Preserve original
inputs and calibration. Delete only owned derived outputs; report failed
cleanup and retain ownership until resolved.

Public/untrusted PRs run on disposable infrastructure without lab devices,
lab networks, private captures, credentials or self-hosted lab runners. Trusted
attestation workflows must not execute PR-supplied code with elevated authority.
Hardware qualification later needs a separately authorized lab boundary and
device/command/limit policy; adoption here grants none of that access.

On parse, identity, policy, resource, interruption or execution failure, retain
a bounded attributable failure record and explicit suppressed/not-run stages.
Finalize no partial successful admission, champion or acceptance artifact.
Never apply, commit, push, merge, deploy or publish a candidate into a target
repository as an optimizer side effect.

## 6. Existing AEMS traceability, without a second ledger

Reuse AEMS's artifact classes, qualified IDs and relation directions. Use
immutable refs for acceptance evidence. The following are semantic mapping
rules for later projections, not fabricated run records:

| Source → relation → target | Meaning |
|---|---|
| Catylist relationship decision → `governs` → EVO integration specification | Program relationship constrains the owning contract. |
| EVO #132 → `tracks` → integration specification/implementation | One existing implementation work item. |
| REA evidence → `derives-from` → interface specification | AEMS defines this edge as the target being informed/produced from the source; authority does not transfer. |
| Compatible C implementation → `implements` → interface specification | Implementation realizes the declared rules. |
| Independent comparison evidence → `verifies` → compatible implementation | Scoped test/check supports the software compatibility claim. |
| EVO run evidence → `evidences` → exact candidate/claim artifact | Run supports only its recorded subject and conditions. |

Use existing `evidence`, `specification`, `implementation`, `test`, `decision`,
`issue` and other supported classes. Do not invent a device/calibration class
or native REA fields inside the strict AEMS node schema. Keep equipment detail
in canonical companion artifacts and index their references. Validate any
actual graph with AEMS's schema and semantic validator. Unsupported mappings
are explicit contract gaps for the owning authority. Graph validity does not
prove truth, closure, compliance or readiness. Notion links canonical identities
and next work; it does not become a competing observation ledger.

## 7. Staged acceptance and dependencies

Record each claim with exact input/spec/code/test/output revisions, expected
and actual outcomes, verifier attribution and limitations. Every required case
must pass; indeterminate/not-run fails the dependent acceptance. Predeclare
tolerances and decision rules before measuring.

| Stage / claim | Required exit evidence | Current limit / dependency |
|---|---|---|
| G1 — governance contract | Attributable acceptance of this EVO ADR/contract and the separate Catylist relationship amendment; consistent ownership and unchanged external-reference classification. | Publication is a proposal, not acceptance; TASK-38 remains open. |
| B0 — full-file source-intake scan | Complete tracked-file/Gitlink registry bound to the pulled commit/tree; exact file identities, applicable scanner/tool/rule results, exclusions and dispositions; dependency/provider closure for selected use; retained report/method/integrity evidence. | The retained 9e22da8 scan accounts for all 2,404 entries; required language/security review and five submodule-content gaps remain open. Byte/syntax coverage alone cannot authorize admission. |
| E1 — evidence handoff | Native validation, unchanged original bytes/IDs/unknown history, complete linked metadata and rights/access; tests refuse corrupt IDs, broken references, unsupported versions and subject mismatches. | Adapter/serializer/validator implementation and relevant intake dispositions still required. |
| C1 — offline compatibility | One selected record decoded deterministically to value/unit/status/timestamp against independently obtained expectations; unchanged originals and complete identity chain. | No real instrument/capture selected; no decoder executed by this proposal. |
| C2 — negative and unknown cases | Applicable truncation/corruption, wrong units/scaling, unsupported revision, device mismatch, stale sequence/time and missing/conflicting calibration cases; invalid/missing samples never become plausible valid readings. | Each omission needs claim-specific rationale. Synthetic cases are labeled synthetic; no self-round-trip-only acceptance. |
| A1 — C admission | Licensed buildable baseline, valid existing EVO manifest, independent correctness, required ABI/numerical/security gates, immutable inputs, bounded actual isolation and refusal tests from section 4. | Separate implementation/evidence; no installed-optimizer claim follows from offline success. |
| O1 — installed handoff | Installed production-provider analyze/evolve/replay/report; baseline/candidate/recipe/catalogue/provider identities, patch, complete gate outcomes, benchmark protocol/raw samples and reproducible logical replay. | [#93](https://github.com/dlworrell/evo/issues/93), [#68](https://github.com/dlworrell/evo/issues/68); separate [#69](https://github.com/dlworrell/evo/issues/69) product proof remains. Fake providers cannot qualify this stage. |
| P1 — performance | Predeclared workload/statistical policy, raw repeated measurements, environment, noise/uncertainty analysis and defensible baseline/candidate comparison. | A valid no-improvement outcome is retained but proves no improvement. Never claim global optimality. |
| H1 — measurement/timing/safety | Separate calibration/reference/uncertainty and conditions, deadline/load/fault evidence, independent protective functions and accountable device/installation acceptance for each claim. | Later authorized laboratory work; offline compatibility, historical driver matching and average latency are insufficient. |

Governance, contract design and offline work can advance before O1. Nothing
here closes #93, #68, #69, #132 or TASK-38. Evidence from crypto, archive work
or other devices stays bound to its original subject. An unknown calibration
may leave a format claim testable while blocking a measurement-fitness claim.

## 8. Versioning, update and recovery

Pin updates are explicit reviewed changes: record old/new REA commits, upstream
comparison base/fork delta, package/lockfile/build/provider identities, license
and effect changes, contract compatibility, fixture and replay differences.
No automatic npm/latest upgrade or fork reclassification follows from this
proposal. A new contract/serializer version must describe supported old inputs,
rejection of unsupported versions and any migration as a separate derivative.

Preserve canonical observations and historic admissions. Revert an unaccepted
proposal by abandoning its PR; reverse a merged increment through an ordinary
reviewed revert/superseding decision. Never rewrite history or hide failed runs.
No runtime or public ABI/schema is changed by this documentation increment.
ADR-0026 is not applicable here: no accelerated structure is introduced.

## 9. Source basis and bounded review

This contract builds on the selected intake already recorded in
[the REA outcome](https://app.notion.com/p/403ae4dd715381f2bc91d7974c2cc207),
[the integration plan](../engineering/REA-EVO-INTEGRATION-PLAN.md) and
[workspace guide](../engineering/REA-EVO-INTEGRATION-WORKSPACE.md). It does not
claim full history/CI mining, provider execution or independent reproduction.

- Pinned REA [Evidence](https://github.com/dlworrell/rea/blob/9e22da845bf8b447f81539e0ba57d5deb59e9103/src/domain/evidence.ts), [bundles](https://github.com/dlworrell/rea/blob/9e22da845bf8b447f81539e0ba57d5deb59e9103/src/domain/evidenceBundle.ts), [effects](https://github.com/dlworrell/rea/blob/9e22da845bf8b447f81539e0ba57d5deb59e9103/src/contracts/toolEffects.ts) and [public CLI](https://github.com/dlworrell/rea/blob/9e22da845bf8b447f81539e0ba57d5deb59e9103/docs/cli.md).
- EVO [ADR-0016](../adr/ADR-0016-layered-source-to-source-c-optimizer.md), [ADR-0035](../adr/ADR-0035-immutable-project-ingestion-and-baselines.md), [EVO-002](EVO-002-source-optimizer-contract.md) and [existing strict manifest schema](../schemas/evo-project-manifest-v1.schema.json).
- Catylist [inventory](https://github.com/dlworrell/Catylist/blob/863a50e8f85d462d3e9e064688a09a85fd2fcce2/inventory/repositories.json), [ADR-0004](https://github.com/dlworrell/Catylist/blob/863a50e8f85d462d3e9e064688a09a85fd2fcce2/docs/adr/ADR-0004-governed-project-scopes.md) and [Accepted ADR-0007](https://github.com/dlworrell/Catylist/blob/863a50e8f85d462d3e9e064688a09a85fd2fcce2/docs/adr/ADR-0007-governed-check-in-and-shared-automation.md); acceptance does not imply repository/scope implementation qualification.
- AEMS [artifact/traceability model](https://github.com/dlworrell/AEMS/blob/main/docs/architecture/artifact-and-traceability-model.md) and [strict graph schema](https://github.com/dlworrell/AEMS/blob/main/schemas/aems-traceability-v1.schema.json), a draft implementation contract; any projection must record the actual consumed revision.
