# REA source-intake refresh: eb3b90b

Observed: 2026-10-27 (America/Los_Angeles). Owner: EVO; AI-assisted preparation
by ChatGPT for @dlworrell. Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132).
This is an evidence increment under EVO-004 section 2.1/B0 and ADR-0046.

**The complete source snapshot is reconciled; admission remains blocked.**
REA remains `external-reference` with default-branch policy `excluded`. This
assessment neither changes the original interface-analysis/contract pin nor
authorizes an installed integration, provider execution or C-source admission.
Independent admission requirements and TASK-38's unfinished read-only scope
remain in force. No independent reviewer or human approval is asserted.

| Subject | Immutable identity |
|---|---|
| Original REA baseline | `9e22da845bf8b447f81539e0ba57d5deb59e9103` |
| Original source tree | `652cb43560a2034fa1fffdff2c07e91062b9d070` |
| Current verified REA candidate | `eb3b90beb4ac9245bac415696aa0de5a1a66c6d2` |
| Current source tree | `282cd9cfb83c87c5b0fb0092fabbae1f264c68a5` |
| Original EVO evidence delivery | [PR #135](https://github.com/dlworrell/evo/pull/135), commit `b6726564eb714e711bfcdbeaab079f3c814ed2d0` |
| Pinned AEMS scanners | `e620f4bd3cc45f05f2f18b61e3a087303eb2b4bd` |

The current candidate was REA `main` when inspected; GitHub reports its commit
signature `verified: true`, reason `valid`. Signature evidence identifies the
commit, not software safety. The original package and all its manifested bytes
were verified without modification. Both original and current registries were
then independently reconciled to the corresponding Git trees and **every blob
was reread from Git and checked against its recorded SHA-256**. The new complete
recursive connector tree is untruncated and agrees with the checkout on all
2,404 non-directory entries. [Source observations](github-source-evidence.json)
retain that evidence and the merged PR identities.

## Comparison and retained findings

| Observation | Original | Current | Disposition |
|---|---:|---:|---|
| Tracked entries | 2,404 | 2,404 | 2,402 unchanged; two content changes. |
| File blobs verified | 2,399 | 2,399 | Exact Git identities and SHA-256 verified for both snapshots. |
| Total tracked blob bytes | 17,425,756 | 17,426,654 | +898 bytes. |
| Added / removed / renamed / mode-changed / type-changed entries | — | 0 / 0 / 0 / 0 / 0 | Explicit complete-tree comparison, not an inference from selected files. |
| Syntax checks | 2,131 pass; nine parser reviews | Identical | The exact per-file outcomes are retained, not silently closed. |
| Native overlay | 28 files; four review-required candidates | Identical | Finding IDs, fingerprints and records match exactly; no independent disposition. |
| Submodule references | Five, contents unscanned | Identical | Provider/dependency closure remains open. |
| Token-shape candidates | Zero | Zero | Same three bounded patterns; no full secret/history claim. |
| External CI action occurrences without full commit refs | 92 | 92 | Existing trust-review finding, unchanged. |
| Locked dependency entries | 560 | 561 | 20 changed versions and one added nested entry. |
| Locked entries with integrity metadata | 560 | 561 | Registry/archive verification is separately scoped below. |
| Declared install-script packages | Two | Two | Existing `fsevents` and nested VitePress `esbuild`; no new collector flag. |

Only `package.json` and `package-lock.json` differ. The manifest has one added
and one removed line; the lockfile has 110 added and 80 removed lines. The
six-commit delta includes **REA PR #1 (Vite)** as well as PR #2 (Zod) and PR #4
(fast-uri). It must not be attributed solely to the two latter PRs.

The source scan is the original read-only method with **only its subject PIN
changed**. Python 3.12.14, Node v24.22.1, Git 2.51.1 and both verified AEMS
scanner source digests match the original run. SEC-001's result is identical;
SEC-002 differs only in its recorded source revision. The sole lexical-effect
delta is the lockfile's URL-marker count/line positions after dependency edits;
it is not evidence of new reachable network behavior.

The nine open parser outcomes remain:

- `scripts/lib/mcp-startup-probe.d.mts`
- `scripts/lib/real-hopper-exhaustive-search.d.mts`
- `src/cliOutput.ts`
- `src/domain/analysisErrorBase.ts`
- `src/domain/native/recordedCrashNoteCoverage.ts`
- `src/domain/sourceMapEnvelope.ts`
- `src/domain/swiftSymbolClassification.ts`
- `src/native/parsers/dyldInfo.ts`
- `src/native/parsers/otool.ts`

The four unchanged native candidates remain at `scripts/fixtures/binary-layout.c:11`,
`tests/conformance/native/calls.m:22`, and
`tests/fixtures/windows/processBoundary.cc:46` and `:47`. Their original review
questions remain in the [original report](../baseline-9e22da8/README.md).
The five unchanged Gitlinks and their exact URLs/pins remain individually
listed in [dependencies.json](dependencies.json). Unchanged findings were
re-observed with the same method; that is not independent acceptance.

## Dependency compatibility, security and licenses

| Affected component | Lock change | Bounded assessment |
|---|---|---|
| `zod` | 4.4.3 → 4.6.7 | Manifest/root lock agree; recorded dependent ranges accept it. Schema behavior and type compatibility need declared-project checks. |
| `fast-uri` | 3.1.8 → 3.1.9 | Ajv 8.20.0's `^3.0.1` range accepts it; URI rejection/normalization changes need application regression review. |
| Root `vite` | 8.3.2 → 8.3.5 | Vitest's range accepts it. The separate VitePress override and nested Vite 6.4.4 remain unchanged. |
| `rolldown` and 15 platform bindings | 1.2.12 → 1.2.15 | Exact binding pins agree; native behavior and selected-platform execution remain unverified. |
| `@oxc-project/types` | 0.152.0 → 0.155.0 | Matches Rolldown's new exact dependency. |
| Nested Vite `postcss` | Added 8.5.31 | Satisfies Vite's new `^8.5.30` constraint; existing root PostCSS remains distinct. |

The complete raw lock-entry comparison retains dependency, optional/peer,
platform, license, URL and integrity fields, including root metadata. Metadata
checking found 39 satisfied affected dependency/peer/optional ranges, nine
absent optional peers, and no required range mismatch. All 19 applicable Node
engine checks accept both `.nvmrc` 24.18.0 and analysis Node v24.22.1. This is
metadata compatibility, not a successful compiler, build or test run. The audit
used npm 11.9.0, whereas REA declares `npm@11.16.0`; project-toolchain
qualification remains unestablished.

### Dated vulnerability-database comparison

Separate copies of the exact original/current package files were queried with
`npm audit --package-lock-only --ignore-scripts --json`, without omission flags,
installation or `audit fix`. Authentication/configuration was not inherited;
the runtime network proxy and system CA were used with TLS verification
enabled. [Execution records](npm-audit-execution.json) bind both input files
by SHA-256 and retain the command/tool identity and outcomes.

| Registry query | Vulnerable package entries | Distinct advisories | Highest recorded severity |
|---|---:|---:|---|
| [Original lockfile](npm-audit-before.json) | 3 | 11 | Two high package entries; one moderate. |
| [Current lockfile](npm-audit-after.json) | 0 | 0 | No known findings returned. |

The original findings comprise four fast-uri advisories, six Vite advisories
and Zod's oversized constrained-collection memory-exhaustion advisory
`GHSA-mrhx-c7xc-m3w5`. All original advisory IDs, ranges and URLs are retained
in the audit and [comparison](comparison.json). The original audit's exit 1 is
a completed query with findings, not an unavailable scan. The current query
exits 0. These are newly obtained observations about **both** pins; the original
baseline did not run a vulnerability database. They do not retroactively claim
an earlier scan or prove exploit reachability, malware absence, future database
coverage, or comprehensive source security.

### Package-byte and license evidence

[Registry evidence](registry-evidence.json) covers 20 old-version metadata
records and 21 current records. Every current archive was fetched as inert
bytes and matched to the lockfile's SHA-512 SRI, registry URL/integrity, registry
SHA-1 and embedded package identity. Each archive also has a SHA-256. No archive
was extracted to a filesystem, installed, imported or executed.

All 21 current package manifests, registry records and lock entries agree on
their license labels: 20 MIT and one BSD-3-Clause (`fast-uri`). No changed
existing lock entry changes its license label. Notice files were located and
hashed in six archives. **The 15 Rolldown binding archives have no notice file
matching the recorded LICENSE/LICENCE/NOTICE/COPYING selection.** They still
declare MIT. This is a newly recorded notice-review gap; old archive notices
were not compared, so the gap is not claimed to have been introduced by this
update. Bundled native components and the actual intended redistribution need
complete notice/license review. A declared label or an MIT-text marker is not
license approval.

All current metadata records expose registry signature and provenance
references. Those cryptographic signatures and SLSA/Sigstore statements were
**not verified**; SRI agreement proves bytes match the selected lock, not their
publisher/build provenance. [Upstream records](upstream-records.json) retain
the maintainer-reported Zod Git-credential incident and fast-uri publishing
transition. Zod's maintainer says the malicious Git commits did not reach npm;
this report does not allege that the selected npm archive is malicious.

No newly introduced security defect was confirmed. The changed behavior,
native notice closure and unverified release provenance remain explicit review
items in [dependency-assessment.json](dependency-assessment.json). The full
561-entry license metadata inventory includes other license families; approval
of the complete selected dependency/provider closure remains open. Existing
root lifecycle hooks and two install-script declarations are unchanged; none
of the 21 inspected current package manifests declares an install/prepare hook.

## Evidence package and reproduction

- [file-registry.csv](file-registry.csv): all 2,404 current tracked entries,
  hashes, roles, coverage, exclusions and dispositions.
- [file-delta.csv](file-delta.csv): every original/current path, including all
  unchanged entries and both sets of exact identities.
- [comparison.json](comparison.json) and [summary.json](summary.json): verified
  snapshot comparison, preserved outcomes, counts, method/tool identities and
  admission state.
- [findings.json](findings.json), [aems-sec001.json](aems-sec001.json) and
  [aems-sec002.json](aems-sec002.json): complete repeated diagnostic results.
- [dependencies.json](dependencies.json), [dependency-delta.json](dependency-delta.json),
  [compatibility.json](compatibility.json), [registry-evidence.json](registry-evidence.json),
  both audit reports and [dependency-assessment.json](dependency-assessment.json):
  inventory, changed metadata, compatibility/security/license observations and gaps.
- [github-source-evidence.json](github-source-evidence.json) and
  [upstream-records.json](upstream-records.json): complete fetched source tree,
  commit/merge observations and bounded upstream attribution.
- [scan-method.py](scan-method.py), [reconcile-method.py](reconcile-method.py),
  [dependency-method.py](dependency-method.py) and
  [compatibility-method.cjs](compatibility-method.cjs): collection and verification
  methods; no target application imports or execution.
- [validation.json](validation.json) and [checksums.sha256](checksums.sha256):
  verification observations and integrity for every other package file.

Prepare clean, separate REA/AEMS checkouts at the full commits above without
installing REA or initializing submodules. From an EVO checkout, repeat the
read-only source scan into a separate output directory:

```sh
python3 docs/engineering/rea-recovery/baseline-eb3b90b/scan-method.py \
  --root ../rea-reference --aems-scripts ../AEMS/scripts \
  --output ../rea-refresh-replay
```

For an offline verification of the retained dated observations, copy this
package's `github-source-evidence.json`, `npm-audit-before.json` and
`npm-audit-after.json` to that output, then run:

```sh
python3 docs/engineering/rea-recovery/baseline-eb3b90b/reconcile-method.py \
  --root ../rea-reference \
  --original docs/engineering/rea-recovery/baseline-9e22da8 \
  --output ../rea-refresh-replay
```

Reconciliation updates the new dependency inventory/summary with the
separately retained audit, so the collector's original `not-run` field cannot
mask the supplemental database query. Original evidence is never rewritten.
To recollect external archive metadata/bytes, use `dependency-method.py` with
the same `--root`/`--output` and, where needed, `--ca-file` naming a trusted CA
bundle. The archive/member bounds and notice-selection rule are in that method.
Run `compatibility-method.cjs ROOT OUTPUT TRUSTED_SEMVER_MODULE` with a trusted
analysis copy of `semver`; its entry-module digest is recorded. Do not resolve
or load a target-owned module to perform this check.

Fresh registry/audit queries are dated new observations and may change. A
query error or a new finding must remain a failed/indeterminate comparison;
do not substitute an empty report. The retained reconciler asserts this exact
published observation set. A changed source/tool/rule identity requires a new
assessment. `sha256sum -c checksums.sha256` verifies this package's listed
bytes; checksums are not an attestation, authenticity or approval mechanism.

## Admission and recovery

The original unsupported security-language coverage, nine parser reviews,
four native candidate dispositions, five submodule-content gaps, secret/history
review and source-effect/isolation assessment remain open. Selected dependency
provenance/notices, declared-project semantic/build/regression checks and an
independently verified buildable C baseline remain necessary. Installed EVO
production-provider replay and any hardware claims need their own evidence.
No REA/provider/fixture/build/lifecycle program was executed by this refresh.

Merge approval accepts this bounded evidence increment only. It cannot close
EVO #132, TASK-38 or admission requirements by implication. Native REA evidence,
authority, confidence and unknown histories remain unchanged; EVO owns these
new observations. No second authoritative evidence database or repository
enrollment was created. ADR-0026 introduces no runtime accelerated structure
here; exact source bytes and complete records remain authoritative. Reversal
uses the ordinary reviewed revert/superseding-record process while preserving
both source-intake baselines as historical evidence.
