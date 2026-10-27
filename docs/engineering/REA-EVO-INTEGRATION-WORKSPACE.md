# REA and EVO integration workspace

Status: integration investigation and review; implementation qualification open.
Owner direction: 2026-10-26, America/Los_Angeles.
Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132).

## Working area and merge boundary

Use the existing EVO issue branch:

```text
132-integrate-rea-assisted-recovery-as-an-evo-capability-with-verified-c-source-intake
```

Keep a dedicated EVO Git worktree on that branch and a sibling, detached REA
checkout at the recorded source commit. The EVO branch is the review and merge
unit. The REA checkout supplies source/reference material for investigation.
This workspace does not itself provide process or device isolation.

The [integration plan](REA-EVO-INTEGRATION-PLAN.md) carries the recovered
requirements, ownership, evidence model, offline pilot and qualification limits.
Use a draft pull request into EVO `main` to review this work. Subsequent changes
should be focused commits or separately reviewed dependent PRs. Keep the draft
open while investigating; merging this documentation alone cannot close #132.

## Recorded source baseline

| Role | Repository | Commit |
|---|---|---|
| Owning implementation and PR base | [EVO](https://github.com/dlworrell/evo) | `9a2bce1ca7d9da31f84e93f2f7f6c7f30664bdb2` |
| Analysis/reference dependency | [REA](https://github.com/dlworrell/rea) | `9e22da845bf8b447f81539e0ba57d5deb59e9103` |

These commits identify the comparison baseline. The EVO working branch may
advance through reviewed commits. A later REA update needs a new recorded pin
and contract/replay comparison. Preserve REA's upstream history and component
licenses; this change copies no REA implementation into EVO and introduces no
new runtime dependency.

## Reproduce the separate checkouts

From an existing EVO clone, with the issue branch not already created locally
or checked out in another worktree:

```sh
git fetch origin
rea_integration_branch='132-integrate-rea-assisted-recovery-as-an-evo-capability-with-verified-c-source-intake'
git worktree add --track -b "$rea_integration_branch" \
  ../evo-rea "origin/$rea_integration_branch"

git clone --no-checkout https://github.com/dlworrell/rea.git ../rea-reference
git -C ../rea-reference switch --detach 9e22da845bf8b447f81539e0ba57d5deb59e9103

git -C ../evo-rea status --short --branch
git -C ../rea-reference rev-parse HEAD
```

Both destination directories must be new. If the local EVO issue branch already
exists and is not checked out elsewhere, use this worktree command instead:

```sh
git worktree add ../evo-rea "$rea_integration_branch"
```

The originally proposed `git fetch origin` followed by `git checkout BRANCH`
also works when the local branch is available or Git can unambiguously infer
the tracking branch. It changes the current checkout. The separate worktree
keeps its files, index and working changes independent of the main checkout.

Do not merge the unrelated REA repository history into EVO. References to REA
should use its repository URL and full commit identity. Keep downloaded REA
source, provider projects, captures, tool builds and experiments outside the
tracked EVO source tree unless a later focused change explicitly owns them.

## Proposed implementation boundary

Prefer a companion integration in EVO that consumes REA's pinned public
CLI/MCP and evidence-bundle contracts. Keep REA execution and its TypeScript
runtime outside the reusable C17 library ABI. Final runtime/package choices
belong in the implementing EVO ADR and threat-model review.

| Candidate arrangement | Assessment for this investigation |
|---|---|
| Sibling pinned REA checkout; public CLI/MCP/evidence boundary | Selected for source comparison and the first contract prototype; preserves independent ownership and upgrades. |
| Git submodule inside EVO | Reconsider if the implementation requires a distributed source checkout; adds submodule checkout and release obligations. |
| Vendored subtree or combined histories | Adds third-party source, license, update and build ownership to EVO; no current evidence requires this arrangement. |

REA analysis informs an evidence-linked behavior specification. A compatible
implementation must first become a tested, buildable C baseline before it is
admitted to EVO's existing source optimizer. REA pseudocode is not equivalent
to an established C baseline. Preserve REA's native evidence and residual
unknowns unchanged; put equipment/run metadata in linked companion records.

## Source landmarks to inspect

| Question | Owning sources |
|---|---|
| What can REA expose? | REA `docs/cli.md`, `docs/mcp-contracts.md`, `src/cliEvidenceCommands.ts` |
| How is analysis evidence represented? | REA `src/domain/evidence.ts`, `src/domain/evidenceBundle.ts` |
| What effects and provider limits need a policy? | REA `src/contracts/toolEffects.ts`, `docs/firmware-analysis.md`, `AGENTS.md` |
| What can EVO admit and optimize? | EVO `docs/specs/EVO-002-source-optimizer-contract.md`, `docs/specs/EVO-002A-product-command-provider-addendum.md`, `docs/specs/EVO-003-product-command-contract.md` |
| Who governs the relationship and its evidence? | Catylist ADR-0004/ADR-0007, the authoritative repository inventory, and AEMS's artifact/traceability model |

## Reviewable delivery sequence

1. Retain this source baseline and complete the declared REA intake/history
   gaps through TASK-38. That task remains a read-only source-review assignment.
2. Specify the lossless REA-bundle handoff, companion metadata, identity and
   rejection rules. Record the relationship in the owning EVO/Catylist ADR
   changes. Ordinary modules need no new governed scope; a formal top-level
   scope must satisfy ADR-0004's declaration and coverage requirements.
3. Select one offline record and implement value/unit/status/timestamp decoding
   against independent expected results. Retain malformed, unsupported,
   mismatched, stale and missing-calibration cases as applicable.
4. Establish the licensed, reproducible C baseline and its correctness/ABI,
   resource and workload contracts before optimizer admission.
5. Prove installed analysis/evolution/replay/report and retained artifacts using
   the production providers as EVO #93/#68 become available. Preserve #69's
   separate product-proof obligation and device-specific qualification limits.

Use linked source, test, CI and artifact evidence to assess each exit criterion.
An issue checkbox, branch, plan or draft PR alone cannot establish implementation
or independent verification. No instrument/capture has been selected and no
hardware/provider execution is established by preparing these checkouts.

## Verification and recovery

For this documentation increment, verify the exact branch/base and REA pin,
check the changed paths and whitespace, and run the existing documentation
profile check. Runtime, ABI, compiler and performance tests become applicable
when those surfaces change. ADR-0026's accelerator rule is not applicable to
this documentation increment; it introduces no data structure or runtime path.

Continue publishing verified commits under Catylist ADR-0006 and the existing
signed-PR runbook. A Git worktree does not configure a signing identity. Retain
AI-assistance attribution and leave human review with its actual reviewer.

An unmerged branch/PR can be abandoned without changing `main`. Preserve local
changes before removing a worktree. If an increment is merged and needs
reversal, use a reviewed revert through the ordinary protected-branch process.
