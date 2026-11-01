# REA ↔ EVO system interfaces

One page that shows every boundary between the systems involved in bringing REA-assisted recovery into EVO ([#132](https://github.com/dlworrell/evo/issues/132)):

- what crosses each boundary, and in what format;
- who owns it, and which document governs it;
- whether it exists today.

It adds no requirement of its own. The governing documents are [ADR-0046](../../adr/ADR-0046-rea-assisted-recovery-and-source-admission.md) and [EVO-004](../../specs/EVO-004-rea-recovery-integration-contract.md), both **Accepted** on 31 October 2026, and Catylist ADR-0015 (also Accepted). [ADR-0048](../../adr/ADR-0048-evo-rea-build-runner-and-encrypted-source-vault.md) adds the build runner and the encrypted source vault. Status reflects the repositories as checked on 1 November 2026.

## Data flow

```text
REA fork (external reference, pinned commit)
  │  B1 source pin (git commit/tree)        → EVO source-intake baseline (docs/engineering/rea-recovery/baseline-*)
  │  B2 REA CLI / MCP run                    → native evidence bundle (7 arrays, original bytes)
  ▼
Evidence store (access-controlled)           B3 retained original bytes + sha256 locator
  │
  ▼
EVO companion adapter (tools/rea_recovery/)  B4 companion manifest v0.1 (references, never rewrites, native records)
  │  B5 interface spec → independently authored decoder + independent expected cases
  ▼
Verified, buildable C baseline               B6 C source admission → EVO project ingestion (EVO-002, ADR-0035)
  │
  ▼
Installed EVO analyze / evolve / replay / report (production providers, #93/#68)
  │  B7 run and comparison evidence
  ▼
AEMS traceability (index only, no second ledger)   B8 integrity of retained records
  │
  ▼
Catylist relationship decision (governs)     B9 governance; B10 coordination records (Notion/Dropbox) link but never decide
```

## Boundaries

| ID | From → to | What crosses, in what format | Owner | Governing text | Status |
|---|---|---|---|---|---|
| B1 | REA repository → EVO intake | Exact commit/tree; full-file registry, scanner results, dependency closure, integrity manifest | EVO | EVO-004 §2.1 | **Exists.** Baselines [9e22da8](baseline-9e22da8/README.md) (contract pin) and [eb3b90b](baseline-eb3b90b/README.md) (candidate). Submodule contents and language/security review are still open. |
| B2 | REA runtime → companion adapter | REA public CLI/MCP output; native evidence bundle with `artifacts`, `providers`, `environments`, `scenarios`, `captures`, `unknowns`, `records` | EVO (consumer), REA (format) | EVO-004 §2, §3; ADR-0046 decision 2 | **Missing.** No adapter in `tools/rea_recovery/`; REA runtime packaging and supported providers are not chosen. |
| B3 | Adapter → evidence store | Original export bytes unchanged; sha256, byte count, locator; derivatives get their own identity and a derivation link | EVO | EVO-004 §3 | **Store chosen, locator open.** ADR-0048 designates the age vault `evo-rea-sources` for original REA evidence (encrypted names and structure; sha256 per file in the encrypted manifest). A locator scheme for citing a retained file from a manifest is not chosen yet. Private captures must stay out of public CI. |
| B4 | Adapter → EVO handoff | Companion manifest v0.1: identity, artifact references, source/tool identity, subject, interface semantics, calibration, compatibility baseline, EVO handoff, claim disposition | EVO | EVO-004 §3 (required-information table) | **v0.1 validator added** in `tools/rea_recovery/companion_manifest.py` with synthetic fixtures and refusal tests (stage E1). `subject` and `interface` are not tightened until C1. |
| B5 | Interface specification → decoder | Specification revision with evidence-to-rule links; independently authored decoder; independently obtained expected cases | EVO | EVO-004 §4, §7 (C1, C2) | **Missing.** No instrument or capture selected. |
| B6 | Verified C baseline → EVO ingestion | Licensed, buildable C snapshot; EVO project manifest; compilation database; workload, constraint and resource policies | EVO | EVO-002; ADR-0035; EVO-004 §4 (A1) | **Contract proposed**; admission checks not implemented. |
| B7 | EVO runs → AEMS | Run and comparison evidence, indexed with existing AEMS classes and relations (`governs`, `tracks`, `derives-from`, `implements`, `verifies`, `evidences`) | AEMS indexes; EVO produces | EVO-004 §6 | **Mapping proposed**; no graph produced yet. Installed handoff depends on #93/#68 (O1). |
| B8 | Retained records → integrity check | Deterministic sha256 manifest of a records folder, chained to the previous one | AEMS | AEMS `scripts/coordination_integrity.py` (pilot) | **Candidate reuse only.** The AEMS pilot could manifest the B3 store; not adopted for REA evidence. |
| B9 | Catylist → EVO | Relationship decision placing REA recovery in EVO; REA stays external-reference | Catylist | Catylist ADR-0015 (relationship amendment merged in Catylist #47); EVO ADR-0046 | **Accepted** 31 October 2026 (owner decision on all three documents), so stage G1 is met. |
| B10 | Coordination records | TASK-38 (Lane A: Em, source/evidence reconciliation; Lane B: Claude, readiness/trust boundary), REA and EVO Notion projects, PRM prompts in Dropbox `/repo/prompt` | Coordinator (Em) | EVO-004 §6 ("Notion links canonical identities… not a competing ledger") | **Exists**, as links only; decisions stay in the repositories. |

## Pin status

- **Contract pin:** `9e22da845bf8b447f81539e0ba57d5deb59e9103`.
- **Candidate:** `eb3b90beb4ac9245bac415696aa0de5a1a66c6d2`. It differs only by three Dependabot updates: vite (REA #1), zod 4.4.3 → 4.6.7 (REA #2) and fast-uri 3.1.8 → 3.1.9 (REA #4). Only `package.json` and `package-lock.json` changed.
- **Unchanged files:** `src/domain/evidence.ts` and `src/domain/evidenceBundle.ts` have identical blob IDs at both commits. Checked again on 31 October from the REA git history, matching Lane A's record.
- **Behaviour after the dependency changes:** unknown (TASK-38 finding F-001). No retained CI run validates either commit.
- **Adopting the candidate** needs its own decision.

## What unblocks progress, in order

1. ~~**G1 governance.**~~ Done: EVO ADR-0046, EVO-004 and Catylist ADR-0015 were accepted on 31 October 2026.
2. ~~**B4 companion manifest v0.1.**~~ Done: validator, synthetic fixtures and stage E1 refusal tests in `tools/rea_recovery/`.
3. **B3 locator scheme.** The store is the ADR-0048 vault `evo-rea-sources`; what remains is how a manifest cites a retained file. It can reuse the AEMS integrity pilot for B3/B8 digests.
4. **B5/C1: choose one offline record and its documented format.** This needs an owner decision on the instrument or capture.
5. **B2 adapter around the pinned REA CLI/MCP**, after REA runtime packaging is decided.
6. **B6/B7**, once #93/#68 land.
