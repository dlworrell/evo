# ADR-0046: REA-Assisted Recovery and C Source Admission

Status: Accepted
Date: 2026-10-26 (America/Los_Angeles)
Owner: EVO
Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132)
Acceptance: accepted by Donovan Worrell on 2026-10-31 (owner decision, with Catylist ADR-0015 and EVO-004); acceptance governs the design, not implementation evidence
Amended: 2026-11-06, Amendment 1 (ingest REA into EVO), proposed in EVO #186; it takes effect when the owner merges that pull request together with the Catylist ADR-0015 amendment (Catylist Code, Law 4)
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
2. *(Amended 2026-11-06, Amendment 1.)* REA is **ingested into EVO**. Its full
   working tree, taken from the newest upstream `morluto/rea` at ingestion time,
   lives under a new top-level scope `rea/`. EVO maintains it, and upstream
   changes are synced with the owner's approval; see Amendment 1 below. Keep
   consuming REA through its CLI/MCP and evidence contracts, and preserve its
   native records, unknown histories and original export bytes. This replaces
   the original decision, which kept REA an external-reference dependency pinned
   to `9e22da845bf8b447f81539e0ba57d5deb59e9103` and authorized no vendoring,
   submodule, upstream-history merge or fork reclassification.
3. Keep REA's runtime and provider execution outside the installed C17 core
   ABI. Use ordinary repository-level modules: future adapter/orchestration in
   `tools/rea_recovery/`, tests in `tests/rea_recovery/`, redistributable fixtures
   in `tests/fixtures/rea-recovery/`, and local review records under
   `docs/engineering/rea-recovery/`. REA's own code is the exception: it lives in
   the `rea/` scope (Amendment 1). These are planned placements, not existing
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

## Amendment 1 (2026-11-06): ingest REA into EVO

Status: Proposed in EVO #186; it takes effect when the owner merges that pull
request together with the Catylist ADR-0015 amendment.

### Context

On 2026-11-06 the owner stated the intent: "We are supposed to be ingesting the
totality of REA into evo." The owner then answered three questions:

1. **Which version.** "Ideally, I'd like to merge the newest most capable
   version. And then apply working updates to fix the bugs."
2. **History.** "We need the history, but can it be stored in Dropbox://repo/Rea
   and keep being updated over time as needed?"
3. **Updates.** "Yes, but with the help of Claude or ChatGPT maybe? Diverge if
   needed."

The owner chose to amend this ADR rather than add a new one.

Facts as of this amendment:

- Upstream [morluto/rea](https://github.com/morluto/rea) is MIT-licensed
  (`rea-agents` 6.8.0). Its tip is
  `48f5f30fd5536cb783c74c1a2df7c2003689dbd0` (PR #7870, 2026-11-06).
- Upstream merges roughly 50–130 pull requests a week, from morluto and at least
  five other contributors.
- The tree has 7,509 tracked files (3,808 in `src/`, 2,499 in `tests/`), about
  106 MB. It uses no Git LFS, and its largest file is 2.2 MB. EVO has 499
  tracked files.
- The upstream history packs to more than 150 MiB.
- [TASK-38](https://app.notion.com/p/402ae4dd715381bfa629f893f99e0543) (Lane A)
  traced upstream changes one pull request at a time. At upstream's rate that
  cannot converge, and it produced no version recommendation.

### Decision

1. **Ingest the newest upstream.**
   - REA's full working tree at a recorded upstream commit is added under `rea/`
     in one commit, through an EVO pull request.
   - The first ingestion takes the upstream tip current when that PR is opened.
   - `rea/UPSTREAM.json` records the upstream repository, commit, commit date,
     retrieval time, the SHA-256 of the tree, and the license files.
   - The fork `dlworrell/rea` and the `9e22da8` pin stop being the source. What
     happens to the fork is decided by the owner separately.
2. **Licensing.** REA's `LICENSE` (MIT), `THIRD_PARTY_NOTICES.md` and
   `licenses/` stay with the code under `rea/`. EVO's `NOTICE` names REA and its
   license. EVO's own files stay Apache-2.0, and files under `rea/` keep MIT
   unless EVO rewrites them.
3. **History lives in Dropbox, not in EVO.**
   - EVO carries one squashed ingestion commit.
   - REA's full upstream history is kept in `Dropbox:/repo/Rea/` as git
     bundles: `rea-full-<date>-<sha>.bundle` once, with every upstream ref, and
     `rea-incr-<from>-<to>.bundle` for each later sync.
   - `MANIFEST.json` lists each bundle's refs, SHA-256 and size, and the EVO
     commit that ingested it.
   - A working git repository is never placed in Dropbox, because file sync
     corrupts `.git`. A bundle is one file that `git clone` and `git fetch` read
     directly, and `git bundle verify` checks it.
   - REA is public, so the bundles are not sealed in an ADR-0048 vault.
4. **Bugs are fixed in EVO.**
   - Fixes to REA are ordinary EVO pull requests that change files under `rea/`,
     held to the `rea/` scope's checks.
   - Each such commit carries a `REA-Local: <reason>` trailer, and
     `rea/LOCAL-CHANGES.md` lists them, so the next sync can re-apply or retire
     each one.
   - Offering a fix upstream is optional and decided case by case.
5. **Upstream sync is assisted and owner-approved.**
   - When the owner asks, or on a schedule the owner sets, an agent (Claude or
     ChatGPT):
     - fetches upstream;
     - adds an incremental bundle to `Dropbox:/repo/Rea/` and updates the
       manifest;
     - opens an EVO pull request that moves `rea/` to the new upstream commit
       with the `REA-Local` changes re-applied.
   - The pull request lists upstream changes since the last sync, re-applied
     local changes, conflicts, and test results.
   - Nothing reaches `rea/` without the owner merging it.
6. **Divergence is allowed and recorded.**
   - The owner may stop taking upstream changes for one path or for all of
     `rea/`.
   - `rea/LOCAL-CHANGES.md` records each divergence: the path, the date, the
     reason and the last upstream commit taken.
   - A fully diverged `rea/` is EVO-owned code.
7. **Scope and checks.**
   - `rea/` is declared as its own ADR-0004 scope in `aes-manifest.yaml`, with
     owner, class, lifecycle and complete file coverage.
   - Its CI runs Node (`npm ci`, REA's build and tests) only when files under
     `rea/` change, so one subgroup's gates never run on another's changes (owner
     rule, 2026-11-06).
   - REA's install and tests run real third-party code. They run on hosted
     runners that hold no secrets, as ADR-0048 requires.
8. **The ADR-0048 runner builds the in-tree copy.** `evo_rea_build.py` builds
   REA from `rea/` instead of a separate pinned checkout. It still never runs
   `rea setup`, `rea update` or `rea uninstall`, and never edits agent
   configuration.
9. **TASK-38 changes question.** Per-PR upstream history mining ends. Lane A's
   open findings, F-001 (archive bytes) and F-002 (the JavaScript semantic-graph
   schema break), become checks run against the ingested tree.

### Consequences

- EVO grows from about 500 tracked files to about 8,000, and its checkout by
  about 106 MB. Whole-repository clones and scans take longer, and scope-aware
  scans must treat `rea/` as its own scope.
- REA becomes EVO's to maintain. Upstream's pace makes sync pull requests
  large; the owner chooses how often to sync.
- Git blame and log inside EVO stop at the ingestion commit. Upstream history
  stays reachable from the Dropbox bundles and from morluto/rea while it exists.
- The Dropbox connector available to agents cannot upload binary files. Writing
  bundles needs a workflow with Dropbox app credentials, like the
  election-archive uploader, or the owner's machine. That choice is made before
  the first sync.
- The integration plan, EVO-004 and the Catylist inventory still describe REA
  as external-reference. They are updated when this amendment takes effect.

### Alternatives considered

- **A new superseding ADR.** This was drafted first; the owner preferred to amend
  this ADR.
- **Git subtree with full history in EVO.** It keeps blame inside EVO, but adds
  more than 6,600 upstream commits to every EVO clone. The owner asked for the
  history to live in Dropbox.
- **Submodule.** EVO would not contain the code, which contradicts ingesting the
  totality of REA.

### Verification after it takes effect

- `rea/UPSTREAM.json` matches the ingested tree's SHA-256, and
  `git bundle verify` passes for every bundle listed in `MANIFEST.json`.
- REA's build and test suite runs in the `rea/` CI. The first run's failures are
  recorded as the starting bug list, not hidden.
- An EVO-only change does not trigger `rea/` CI, and a `rea/`-only change does
  not trigger EVO's C checks.
