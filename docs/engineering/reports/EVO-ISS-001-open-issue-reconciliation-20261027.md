# EVO-ISS-001: Open-issue and audit reconciliation

Observed: 2026-10-27 (America/Los_Angeles). Owner: EVO. AI-assisted preparation
by ChatGPT for @dlworrell under the owner's request to resolve open issues such
as [#112](https://github.com/dlworrell/evo/issues/112).

This reconciles existing evidence and issue state. It does not perform a new
360-degree audit, qualify an installed product, accept a new ADR, change REA
classification or approve a release.

## Inspected subjects

| Authority | Inspected default-branch commit |
|---|---|
| EVO | `27fa571ee9bcd98e9acba70d4bd31444810abf76` |
| Catylist | `ae617db4074b676795ea5a8f9e1a4d50ab047930` |

The complete open-issue response contained nine issues and one PR. PR #136 is
tracked separately; pull requests are not counted as product/audit issues.
Live child issue states, merged PR records and actual default-branch files were
compared rather than treating old checkboxes as acceptance evidence.

## Stale findings resolved

### #112: Catylist classification

EVO [PR #124](https://github.com/dlworrell/evo/pull/124) merged at
`70ea976bdb7c53ef295d54654aadc608145a8137`. The required upstream
Catylist [PR #11](https://github.com/dlworrell/Catylist/pull/11) merged afterward
at `35a1c7b094022c2e6021c8434a879f0b5078a447`. The issue remained reopened even
after its missing upstream dependency landed.

Current Catylist `main` retains the approved
[CAT-DISP-001 disposition](https://github.com/dlworrell/Catylist/blob/ae617db4074b676795ea5a8f9e1a4d50ab047930/docs/reviews/CAT-DISP-001-EVO-repository-classification.md)
and matching repository inventory. Current EVO `main` retains matching
`repo.yaml` and `GOVERNANCE.md`: top-level `governed-product`, role
`source-optimization-toolchain`, `approved-catylist`, reusable-library core and
engineering-application optimizer component. The EVO merge is an ancestor of
the inspected EVO default tip.

All five acceptance items were verified and #112 was closed as completed on
2026-10-27. No additional classification decision, waiver or AES/AEMS
applicability change is needed. #93 and #56 retain their own implementation and
release requirements.

### #117: Original audit index

All eight indexed findings have completed child issues and retained landing
evidence. The old index still displayed six of them unchecked.

| Finding | Completed scope | Retained landing evidence |
|---|---|---|
| #109 | Installed core version and pkg-config parity | [PR #118](https://github.com/dlworrell/evo/pull/118), `8d78dab3be6ef6e1dfb11cc480246bcb773b5c48` |
| #110 | AES-BLD machine/profile inventory parity | [PR #123](https://github.com/dlworrell/evo/pull/123), `1d47a6f6f33035ef54727583492d24e6893b180b` |
| #111 | Reconciliation of the original 0.42 parent roadmap | [Completed issue](https://github.com/dlworrell/evo/issues/111); current later 0.43 drift is reconciled by this change |
| #112 | Catylist/EVO repository classification | Catylist #11 and EVO #124, verified above |
| #113 | Source-optimizer security threat model | [PR #120](https://github.com/dlworrell/evo/pull/120), `5d69ea92465426395a3f1160afd35a4e45df0327` |
| #114 | Built-in production providers | [PR #119](https://github.com/dlworrell/evo/pull/119), `1714235510d568fb3eed51d2c6c5b425f0a1e2d5` |
| #115 | Exact-commit release gate aggregation | [PR #121](https://github.com/dlworrell/evo/pull/121), `b79d5647ec1f555b42a73ca127438860a31e27e4` |
| #116 | Actions runtime compatibility guard | [PR #123](https://github.com/dlworrell/evo/pull/123), `1d47a6f6f33035ef54727583492d24e6893b180b` |

The index is eligible for closure under its existing close condition. The PR
for this reconciliation closes #117 after the current roadmap projection is
corrected on `main`. This does not re-run or enlarge the original audit, and
does not change its historical scope or limitations.

## Roadmap and acceptance corrections

#67 completed through [PR #122](https://github.com/dlworrell/evo/pull/122), merge
`44f8b789fb6cede3a205d36c422f27d4bca19c33`; #114 completed through PR #119.
The live parent #38 and version-controlled roadmap still identified one or both
as pending. Both are reconciled to **#93 as the next dependency-ready product
boundary**. The classification dependency in #112 is already satisfied.

#132's seven checked parent items conflicted with its own acceptance language
and EVO-004's explicitly unimplemented runtime boundary. The issue is reconciled
to preserve delivered planning, specifications and source-intake evidence while
leaving unmet parent acceptance items open. Checked boxes do not constitute
decoder execution, source admission, an installed handoff or independent
verification. Existing Notion ownership and TASK-38's unfinished scope remain
unchanged.

## Work that must remain open

These seven issues are genuine continuing work, rather than unresolved copies
of the classification/audit defect. Their original owners, scope and criteria
are preserved.

| Issue | Current disposition and next concrete requirement |
|---|---|
| [#93](https://github.com/dlworrell/evo/issues/93) | Dependency-ready. Deliver the installed standalone executable, filesystem/output/signal behavior, real providers and CMake/Autotools staged-install acceptance. Private command/provider layers are already present. |
| [#68](https://github.com/dlworrell/evo/issues/68) | Depends on #93. Deliver the complete reviewable optimized patch/evidence bundle through the installed path. |
| [#69](https://github.com/dlworrell/evo/issues/69) | Depends on #68. Retain real reference-project source-change, correctness, measurement and replay proof through the installed production path. |
| [#56](https://github.com/dlworrell/evo/issues/56) | Depends on #69 and all non-deferred roadmap obligations. Final 1.0 compatibility/release stabilization is not implied by audit closure. |
| [#38](https://github.com/dlworrell/evo/issues/38) | Parent roadmap remains open until its product milestones satisfy their acceptance criteria. Its completed children and next boundary are corrected. |
| [#125](https://github.com/dlworrell/evo/issues/125) | New versioned thermal/energy/economic measurement feature. Requires the openCUDA #41 telemetry boundary, explicit evidence/policy inputs and real measured candidate proof; no tariff, meter data or hardware result is invented. |
| [#132](https://github.com/dlworrell/evo/issues/132) | REA integration and independent admission remain open. Planning and intake evidence exist; decoder/C-baseline/admission/installed-product acceptance does not. REA stays external-reference/excluded. |

[REA evidence PR #136](https://github.com/dlworrell/evo/pull/136) is a separate
open, verified-commit evidence delivery with 45/45 successful checks on
`5e89fb6aa17e46985b72561651ce8afe96827bf2` when inspected. Its source candidate,
dependency evidence and unresolved admission checks retain their own record.
This audit reconciliation does not silently merge it or adopt its source pin.

## Validation and limits

The inspected EVO default tip has 52 returned check runs, all completed
successfully. Local checks of that same source also passed:

- `scripts/verify_aes_bld_profile.py`: 26 core + 44 private = 70 production
  sources; 50 normative tests.
- `scripts/verify_github_actions_runtime.py`: 43 checkout references and ten
  upload-artifact references meet the enforced runtime floors.
- `test_release_readiness.py`: eight tests passed.
- `test_product_command_schemas.py`: four tests passed.

These checks support the bounded reconciliation of already-landed remediation.
They do not reproduce every historical audit result, establish release
readiness for a different commit, or qualify an installed optimizer. New PR
signature and exact-head CI observations belong to the PR publication record.

This change adds no runtime behavior, public ABI, installed schema, workflow
permission, standard applicability or accelerated runtime structure; ADR-0026
is not applicable to the documentation correction. The issue-state procedure
in `docs/roadmap.md` requires reconciliation at milestone/upstream completion and
before the next milestone, without creating an automatic evidence authority.
Recover through a reviewed correction/revert and reopen any issue whose actual
acceptance is later invalidated; preserve the original evidence and history.
