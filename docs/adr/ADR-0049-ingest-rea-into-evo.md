# ADR-0049: Ingest REA into EVO

Status: Proposed
Date: 2026-11-06 (America/Los_Angeles)
Owner: EVO
Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132)
Supersedes: [ADR-0046](ADR-0046-rea-assisted-recovery-and-source-admission.md) decision 2 (REA as an external-reference dependency, no vendoring), and the parts of [ADR-0048](ADR-0048-evo-rea-build-runner-and-encrypted-source-vault.md) decision 1 that build REA from a separate pinned checkout
Requires: an amendment to Catylist [ADR-0015](https://github.com/dlworrell/Catylist/blob/main/docs/adr/ADR-0015-rea-recovery-through-evo.md) and REA's entry in Catylist's `inventory/repositories.json` (Catylist Code, Law 4)
Acceptance: pending owner approval in this pull request

## Context

On 2026-11-06 the owner stated the intent: "We are supposed to be ingesting the totality of REA into evo." The accepted design does the opposite. ADR-0046 keeps REA an external-reference dependency pinned to `9e22da8` and forbids vendoring, submodules and upstream-history merges. ADR-0048 builds REA from a separate checkout whose files are never changed.

The owner then answered three questions:

1. **Which version.** "Ideally, I'd like to merge the newest most capable version. And then apply working updates to fix the bugs."
2. **History.** "We need the history, but can it be stored in Dropbox://repo/Rea and keep being updated over time as needed?"
3. **Updates.** "Yes, but with the help of Claude or ChatGPT maybe? Diverge if needed."

Facts as of this proposal:

- Upstream [morluto/rea](https://github.com/morluto/rea) is MIT-licensed (`rea-agents` 6.8.0). Its tip is `48f5f30fd5536cb783c74c1a2df7c2003689dbd0` (PR #7870, 2026-11-06).
- Upstream is active: roughly 50–130 merged pull requests a week since October, from morluto and at least five other contributors.
- The tree has 7,509 tracked files (3,808 in `src/`, 2,499 in `tests/`), about 106 MB. It uses no Git LFS, and its largest file is 2.2 MB. EVO itself has 499 tracked files.
- The upstream history packs to more than 150 MiB.
- [TASK-38](https://app.notion.com/p/402ae4dd715381bfa629f893f99e0543) (Lane A) has spent about ten daily passes tracing upstream changes one pull request at a time. At upstream's rate that cannot converge, and it never produced a version recommendation.

## Decision

1. **Ingest the newest upstream.**
   - REA's full working tree at a recorded upstream commit is added to EVO under a new top-level scope, `rea/`, in one commit through an EVO pull request.
   - The first ingestion takes the upstream tip current when that PR is opened.
   - `rea/UPSTREAM.json` records the upstream repository, commit, commit date, retrieval time and SHA-256 of the tree, and lists the license files.
   - The fork `dlworrell/rea` and the `9e22da8` pin are no longer the source. What happens to the fork is an owner decision recorded separately.
2. **Licensing.** REA's `LICENSE` (MIT), `THIRD_PARTY_NOTICES.md` and `licenses/` stay with the code under `rea/` unchanged. EVO's `NOTICE` names REA and its license. EVO's own files stay Apache-2.0; files under `rea/` keep MIT unless EVO later rewrites them.
3. **History lives in Dropbox, not in EVO.**
   - EVO carries one squashed ingestion commit. REA's full upstream history is kept in `Dropbox:/repo/Rea/` as git bundles:
     - `rea-full-<date>-<sha>.bundle`, made once with every upstream ref;
     - `rea-incr-<from>-<to>.bundle` for each later sync;
     - `MANIFEST.json`, listing each bundle's refs, SHA-256 and size, and the EVO commit that ingested it.
   - A working git repository is never placed in Dropbox, because file sync corrupts `.git`. A bundle is a single file that `git clone` and `git fetch` read directly, and `git bundle verify` checks it.
   - REA is public, so these bundles are not sealed in an ADR-0048 vault.
4. **Bugs are fixed in EVO.**
   - Fixes to REA are ordinary EVO pull requests that change files under `rea/`, held to the `rea/` scope's checks.
   - Each such commit carries a `REA-Local: <reason>` trailer, and `rea/LOCAL-CHANGES.md` lists them. The next sync can then re-apply or retire each one.
   - Offering a fix upstream to morluto/rea is optional and decided case by case.
5. **Upstream sync is assisted and owner-approved.**
   - When the owner asks, or on a schedule the owner sets, an agent (Claude or ChatGPT):
     - fetches upstream;
     - extends the Dropbox history with an incremental bundle and updates the manifest;
     - opens an EVO pull request that moves `rea/` to the new upstream commit with the `REA-Local` changes re-applied.
   - The pull request lists upstream changes since the last sync, re-applied local changes, conflicts, and test results.
   - Nothing reaches `rea/` without the owner merging it.
6. **Divergence is allowed and recorded.**
   - The owner may stop taking upstream changes for a path or for all of `rea/`.
   - `rea/LOCAL-CHANGES.md` records each divergence: the path, the date, the reason and the last upstream commit taken.
   - A fully diverged `rea/` is EVO-owned code.
7. **Scope and checks.**
   - `rea/` is declared as its own ADR-0004 scope in `aes-manifest.yaml`, with owner, class, lifecycle and complete file coverage.
   - Its CI runs Node (`npm ci`, REA's own build and tests) only when files under `rea/` change. This follows the owner's 2026-11-06 rule that one subgroup's gates must not run on another's changes.
   - REA's dependency install and tests run real code. They run on hosted runners that hold no secrets, as ADR-0048 already requires.
8. **The ADR-0048 runner builds the in-tree copy.** `evo_rea_build.py` builds REA from `rea/` instead of a separate pinned checkout. It keeps its other rules: it never runs `rea setup`, `rea update` or `rea uninstall`, and never edits agent configuration.
9. **TASK-38 changes question.** Per-PR upstream history mining ends. Lane A's open findings, F-001 (archive bytes) and F-002 (the JavaScript semantic-graph schema break), become checks run against the ingested tree, so the code answers them.

## Consequences

- EVO grows from about 500 tracked files to about 8,000, and its checkout by about 106 MB. Clones and scans that cover the whole repository take longer. Scope-aware scans must treat `rea/` as its own scope.
- REA becomes EVO's to maintain. Upstream's pace means a sync PR can be large; the owner chooses how often to sync.
- Git blame and log inside EVO stop at the ingestion commit. Upstream history is reachable from the Dropbox bundles (`git clone rea-full-….bundle`) and from morluto/rea while it exists.
- The Dropbox connector available to agents cannot upload binary files. Writing bundles needs a workflow holding Dropbox app credentials, like the election-archive uploader, or the owner's machine. That choice is made at implementation, before the first sync.

## Alternatives considered

- **Git subtree with full history in EVO.** This keeps blame inside EVO, but adds more than 6,600 upstream commits to EVO's history and makes every EVO clone carry them. The owner asked for history to live in Dropbox.
- **Submodule.** EVO would not contain the code, which contradicts "ingest the totality". It also adds checkout friction.
- **Keep ADR-0046 (external reference).** This contradicts the owner's direction.

## Verification before acceptance

- Owner review of decisions 1–9.
- The Catylist ADR-0015 amendment and REA's inventory reclassification are drafted, so they can be accepted together.

## Verification of implementation (after acceptance)

- `rea/UPSTREAM.json` matches the ingested tree's SHA-256, and `git bundle verify` passes for each Dropbox bundle listed in `MANIFEST.json`.
- REA's own build and test suite runs in the `rea/` CI. The first run's failures are recorded as the starting bug list, not hidden.
- An EVO-only change does not trigger `rea/` CI, and a `rea/`-only change does not trigger EVO's C checks.
