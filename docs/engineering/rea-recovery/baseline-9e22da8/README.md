# REA full-file source-intake baseline

Date: 2026-10-26 (America/Los_Angeles)
Producer/owner: EVO; AI-assisted preparation by ChatGPT for @dlworrell
Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132)
Source: `dlworrell/rea` at `9e22da845bf8b447f81539e0ba57d5deb59e9103`
Source tree: `652cb43560a2034fa1fffdff2c07e91062b9d070`
Disposition: full tracked-snapshot inventory verified; source admission remains blocked

## Result

The baseline accounts for **every tracked entry** in the pinned REA pull:
2,399 file blobs and five submodule references. Every file was read, matched
to its Git blob and assigned a SHA-256, type/role, scanner coverage and
scan/disposition entry in [file-registry.csv](file-registry.csv). The connector's
complete recursive tree and the Git checkout agree on all 2,404 entries.
The checkout remained unchanged before, during and after scanning.

This is the REA **source-intake baseline**. A reconstructed, independently
verified buildable C implementation must establish its own correctness/build
baseline before EVO optimization. This report supplies neither that C baseline
nor an installed integration or hardware qualification.

| Check | Observed result | Interpretation |
|---|---|---|
| Tracked snapshot | 2,399/2,399 file blobs verified; 17,425,756 bytes | Complete byte identity for this exact repository snapshot. |
| Text/binary accounting | 2,397 UTF-8 files; two PNG files; five Gitlinks | Both images are hashed and explicitly accounted for. Image semantics/binary vulnerability analysis was not run. |
| Syntax checks | 2,131 pass; nine require parser review, out of 2,140 invocations | Mechanical parsing under the recorded tools; no application code was executed. |
| TypeScript parsing | 1,826 pass; nine require review, out of 1,835 `.ts`/`.mts` inputs | Node's experimental TypeScript parsing is not a semantic/type check or the project's compiler. |
| JavaScript / Python / JSON / shell | 251 / 27 / 26 / 1 pass | Node syntax-only, Python AST parsing, JSON parsing and `bash -n`, respectively. |
| Native API scan | 28 files, including fixtures excluded by the default scan; zero banned matches and four review-required candidates | The AEMS matcher reports candidate call sites, not safety proofs. |
| AES-SEC-002 diagnostic run | Seven present signals, seven absent-evidence signals, four not-applicable, zero violation signals | Diagnostic inclusion only; it does not enroll the external reference or establish compliance. |
| Dependency inventory | 560 lock entries, all with integrity metadata; two declare install scripts | Metadata inspected without installing dependencies or executing lifecycle hooks. No vulnerability-database or license approval claim. |
| CI action references | 92 external `uses:` occurrences; all use refs other than a full commit SHA | Record for dependency/trust review; this is not a claim that a workflow was exploited. |
| Token-shaped marker pass | Zero matches for the three recorded patterns | Not a full secret, entropy, credential-validity or history scan. |
| Submodule closure | Five pinned references; their contents unscanned | Required provider/dependency contents must be separately fetched, baselined and reviewed before their use is admitted. |

## Retained artifacts and authority

- [summary.json](summary.json): exact subject, tool identities, counts and open checks.
- [file-registry.csv](file-registry.csv): all 2,404 tracked entries, including non-source files and default-scanner exclusions.
- [findings.json](findings.json): native candidate identities, per-file syntax results, bounded source-effect signals and value-free token-shape results.
- [aems-sec001.json](aems-sec001.json) and [aems-sec002.json](aems-sec002.json): retained diagnostic scanner reports, with external-reference applicability context and source snippets omitted.
- [dependencies.json](dependencies.json): root/package identities, all locked dependency metadata, lifecycle hooks, submodule pins and workflow `uses:` references.
- [scan-method.py](scan-method.py): reproducible read-only collection/scan method; it imports the pinned AEMS scanners, never target application modules.
- [checksums.sha256](checksums.sha256): byte integrity for every other file in this package. It is not an attestation, approval or authenticity mechanism.

REA remains `external-reference` with default-branch policy `excluded` in
Catylist. AEMS owns the reused scanner implementations; EVO owns these newly
produced observations. REA's existing native evidence, confidence, authority,
limitations and unknown histories are unchanged. No second observation database
or repository enrollment was created. TASK-38 retains its existing owner,
read-only mining scope and unfinished history/CI acceptance criteria.

## Coverage and limitations

AEMS scanners are pinned to commit
`e620f4bd3cc45f05f2f18b61e3a087303eb2b4bd`. The method verifies these source
digests before importing them:

| Scanner | SHA-256 |
|---|---|
| `scripts/aes_sec_001_scan.py` | `2af99be2c49dbdc384cbe6642ce9c37d02503280915e3408aff70563983358a8` |
| `scripts/aes_sec_002_scan.py` | `89f6b9529c2bd292c24c101ed5e54160739e9f622331082d5d2171973e9bae0c` |

Tools: Python 3.12.14, Node v24.22.1 and Git 2.51.1. The source and scanner
revisions identify this run; a later tool or pin change needs a new record.
The method's SHA-256 is retained in `summary.json`.

The default SEC-001 scan selects 22 native files and 2,228 signal-only paths,
while excluding 142 fixture paths and seven directory-filtered files. Its native
matcher was additionally applied to all 28 tracked native files. These overlay
matches remain diagnostic and are not source-owned waiver dispositions.

The default SEC-002 scan selects 257 text files, excludes seven directory-filtered
files and does not select 2,135 files by extension. Its extension list omits
TypeScript/JavaScript. The all-file registry exposes that limitation; syntax
checks and effect-marker observations do not substitute for missing language
security analysis. The explicit ad-hoc `in-scope` scanner argument only enables
diagnostic signals. Catylist's external-reference classification is unchanged,
and missing AES profiles in REA are not imposed governance violations.

All UTF-8 files received the recorded effect/marker pass. Its expressions find
process, network, dynamic-code and filesystem-write markers and three token
shapes. They do not follow data flow, prove reachability, validate credentials,
or establish absence of vulnerabilities/secrets. Fixtures and documentation can
legitimately contain those markers. No matched credential values are retained.

Nine Node TypeScript checks returned a parse-review outcome. Direct inspection
showed leading TypeScript `type`/`interface` declarations rejected by this
syntax-check path in two cases; this is evidence of a parser/mode limitation,
not confirmation of source defects. All nine remain open for the declared
project parser/typechecker:

| Path | Recorded disposition |
|---|---|
| `scripts/lib/mcp-startup-probe.d.mts` | Parser-compatible review required. |
| `scripts/lib/real-hopper-exhaustive-search.d.mts` | Parser-compatible review required. |
| `src/cliOutput.ts` | Parser-compatible review required. |
| `src/domain/analysisErrorBase.ts` | Parser-compatible review required. |
| `src/domain/native/recordedCrashNoteCoverage.ts` | Parser-compatible review required. |
| `src/domain/sourceMapEnvelope.ts` | Parser-compatible review required. |
| `src/domain/swiftSymbolClassification.ts` | Parser-compatible review required. |
| `src/native/parsers/dyldInfo.ts` | Parser-compatible review required. |
| `src/native/parsers/otool.ts` | Parser-compatible review required. |

No TypeScript semantic/type analysis, CodeQL/Semgrep run, full secret/history
scan, dependency vulnerability database, transitive license approval, complete
upstream/fork history comparison, build/test suite, provider execution, installed
EVO handoff or hardware validation is claimed. No REA setup/install command,
package lifecycle hook, fixture program or provider was executed.

## Native candidate review observations

The four candidates retain their original AEMS finding IDs and fingerprints
in `findings.json`. Selected source lines were inspected by the same preparing
agent; that does not supply independent review or approve an invariant.

| Source location | Observation | Next disposition |
|---|---|---|
| `scripts/fixtures/binary-layout.c:11` | `snprintf` has an explicit 32-byte destination bound; its return is ignored. This is a binary-layout fixture. | Retain the truncation/return-value review question; no source patch or approval inferred. |
| `tests/conformance/native/calls.m:22` | `memset` initializes the complete local 8,192-byte array with `sizeof(chunk)`. It is ordinary fixture initialization, not a demonstrated secret-erasure boundary. | Record the scoped observation; independent disposition remains open. |
| `tests/fixtures/windows/processBoundary.cc:46` | First `memcpy` copies the local header into a vector sized from header plus payload. | Review the complete size/narrowing invariant and actual Windows fixture behavior before reuse. |
| `tests/fixtures/windows/processBoundary.cc:47` | Second `memcpy` copies `substituteBytes` from the original string into the sized vector. | Review the `WORD` narrowing and supported input bounds together; no Windows execution was performed. |

## Outstanding admission work

Use EVO #132 to retain these scan gaps; do not create a competing source-mining
task or mark TASK-38 complete. Before admitting affected integration/source:

1. Resolve the five submodule/provider closures needed by the selected path,
   keeping repository/commit/license and per-file evidence for their contents.
2. Run the declared TypeScript/compiler/language security checks and reconcile
   all nine parser-review outcomes with the correct tool/mode.
3. Obtain a full secret/history and dependency-vulnerability/license assessment
   appropriate to the actual source/toolchain/provider selection.
4. Review native call candidates, tagged CI dependencies, lifecycle hooks,
   actual process/network/filesystem effects and the required isolation policy.
5. Retain reviewer attribution, rule applicability and each unresolved,
   not-run, rejected or accepted disposition. Admission remains blocked where
   required evidence is unavailable or indeterminate.

The full-file baseline must be repeated for a new pin, with explicit added,
removed, renamed, mode-changed and content-changed entries and comparison to the
prior baseline. Unchanged file hashes alone do not preserve a result when its
scanner, rule, dependency or toolchain identity changes.

## Reproduce

Prepare separate REA and AEMS checkouts at the exact commits above, preserving
their histories. Fetch the explicit commit if a shallow clone does not contain
it. Do not install REA or initialize providers/submodules merely to run this
read-only snapshot baseline.

From an EVO checkout, using the displayed package path and separate output:

```sh
python3 docs/engineering/rea-recovery/baseline-9e22da8/scan-method.py \
  --root ../rea-reference \
  --aems-scripts ../AEMS/scripts \
  --output ../rea-baseline-replay
```

Run `sha256sum -c checksums.sha256` from this retained package to verify its
listed bytes. Replay output depends on the declared parser versions; a changed
parser is a new observation and needs an attributed comparison. The method
asserts the exact source pin, clean input, per-file Git identities and unchanged
SHA-256 values after scanning. It does not write into the source checkout.

Reversal of this evidence increment uses the ordinary reviewed PR process;
retain this run and its source identities as historical evidence.
