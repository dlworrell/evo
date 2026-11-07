# CodeQL triage: `rea/` scope

Status: first triage, 2026-11-07
Scope: `rea/` (EVO ADR-0046 Amendment 1), under EVO ADR-0049 and Catylist ADR-0009
Source: CodeQL 2.27.2 `security-extended`. The scans were the PR analysis of
EVO #188 and a whole-tree run (EVO Actions run 42345658923, SARIF retained as
artifacts).
Reviewer: claude-code (agent triage; owner confirmation pending)

This file is EVO-owned scope evidence. It records a disposition for every
CodeQL result that blocked or was flagged on the first promotion that carried
`rea/`, and the categories of the remaining whole-tree results. Code fixes are
`REA-Local` changes listed in `rea/LOCAL-CHANGES.md`.

## Results flagged on EVO #188

The 20 `rea/` results, plus one in EVO's own workflow, that GitHub annotated
as new in the promotion:

| Location                                                                  | Rule                          | Disposition                                                                                                                                                                                                             |
| ------------------------------------------------------------------------- | ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `src/application/desktop/DesktopBuildBundle.ts:62`                        | js/file-system-race           | **Fixed** (27f6294): the apt key is read through one bounded descriptor.                                                                                                                                                |
| `src/application/decompileDirectory/exportTree.ts:653`                    | js/file-system-race           | **Fixed** (27f6294): the journal is opened non-blocking and its descriptor is checked.                                                                                                                                  |
| `scripts/electron-active-hook-boundaries.cjs:255`                         | js/file-system-race           | **Fixed** (27f6294): the shipped hook hashes the add-on through one bounded descriptor.                                                                                                                                 |
| `scripts/build-windows-native.mjs:71`                                     | js/http-to-file-access        | **Fixed** (27f6294): downloads are verified against SHASUMS256 before they are written.                                                                                                                                 |
| `src/artifacts/SafeOutputTree.ts:326`                                     | js/file-system-race           | Not a defect. REA reopens its own just-written output with `O_NOFOLLOW` after checking its identity, then hashes it and compares the result with the inventory.                                                         |
| `src/cli.ts:522`                                                          | js/file-system-race           | Low risk, unchanged. This is a read-modify-write of the user's own Amp `settings.json` after `amp mcp add`, so the only other writer is Amp itself.                                                                     |
| `src/filesystem/RegularFile.ts:121`                                       | js/insecure-temporary-file    | False positive. Every flagged source is a temp file created by a test (`tests/boundary/...`). Product code only opens an existing file read-only.                                                                       |
| `bridge/ghidra/ReaGhidraBridge.java:4959`                                 | java/regex-injection          | Intended. Regex search is the feature, and the pattern comes from the local agent session. Syntax and stack-exhaustion failures are already caught. Catastrophic backtracking (ReDoS) is possible and is the open item. |
| `scripts/fixtures/page-cdp-proxy.mjs:151`                                 | js/request-forgery (critical) | Test fixture. A local proxy forwards to the Chrome DevTools endpoint the verifier started; it never ships.                                                                                                              |
| `scripts/verify-windows-native.mjs:252`, `:347`                           | js/incomplete-sanitization    | Intended. Replacing only the first backslash deliberately tests mixed-separator paths.                                                                                                                                  |
| `scripts/verify/apple/macho-occurrences.mjs:15`, `:77`                    | js/file-system-race           | Verifier script on its own fixture bundle; not shipped.                                                                                                                                                                 |
| `scripts/verify-apktool.mjs:68`, `scripts/verify-adb.mjs:303`             | js/file-system-race           | Verifier scripts hashing files they just pulled; not shipped.                                                                                                                                                           |
| `scripts/lib/verifier-file-snapshot-owner.mts:43`                         | js/file-system-race           | Verifier support; the file is opened and owned right after the existence check.                                                                                                                                         |
| `scripts/verify-qwen-client.mjs:333`                                      | js/bad-code-sanitization      | Verifier builds its own script from `JSON.stringify` values and base64-encodes it; there is no outside input.                                                                                                           |
| `scripts/verify/clients/{anthropic-model,gemini-model,model}-fixture.mjs` | js/http-to-file-access        | Local mock model servers log their own requests to a temp folder; test only.                                                                                                                                            |
| `.github/workflows/aes-sec-001-governance.yml:26` (EVO)                   | actions/unpinned-tag          | **Fixed** in EVO #193: the AEMS reusable workflow is pinned by commit.                                                                                                                                                  |

## Remaining whole-tree results

The whole-tree run reported 204 results: 2 critical, 145 high and 57 medium.
Beyond the table above:

- **`third_party/wakaru/testcases/webpack5/dist/modules.js`** (property
  injection, XSS, dynamic method calls, one critical): bundled webpack
  _output_ used as a decompiler test input. It is not executed by REA.
- **`tests/**`, `src/**/*.test.ts` and `tests/fixtures/**`** (about 70
  file-system races, fake servers): test code on temp files the tests own.
- **`third_party/binwalk/src/structures/common.rs:167`** (rust/cleartext-logging):
  logs a parsed SquashFS uid table in debug output. That is filesystem
  metadata, not a secret. Upstream binwalk.
- **`src/native/SignatureTargetBinding.ts:329`** and
  **`src/flutter/FlutterBuildProvider.ts:1172`** (js/file-system-race): not a
  defect. The first checks with `lstat` and then opens with
  `O_NOFOLLOW | O_NONBLOCK`. The second reopens a file REA created
  exclusively (`wx`) in its own snapshot root.
- **`src/hopper/HopperFileOffset.ts:155`** (js/insecure-temporary-file): the
  same pattern as `RegularFile.ts`. The temp files are created by the tests.
- **`src/javascript/analysis/JavaScriptAnalysisWorker.ts:790`**: **fixed**
  (27f6294). The checkpoint's size checks and read now use one descriptor.

EVO's own C code had two results, outside `rea/`. Both stay with EVO's
AES-SEC-001 process:

- `src/project_provider_sandbox.c:308` (cpp/path-injection): not a defect. The
  "user input" is a numeric PID read with `%ld` from `/proc/<pid>/task/<tid>/children`
  and formatted back into a `/proc` path.
- `tests/product_cli_test.c:95` (cpp/world-writable-file-creation): the test
  sets a fixture folder to 0777 on purpose, to check that
  `evo_product_stage_prepare` refuses a shared writable output parent. It then
  restores 0700.

## Open items

- **ReDoS in the Ghidra bridge's regex search.** Bounding regex evaluation (a
  time or step budget) would remove it. It is not changed here, because it
  alters search semantics. It needs an owner decision, or an upstream fix
  offered to `morluto/rea`.
- **Dismissals in code scanning.** This session cannot dismiss alerts (the
  API is not available to it). The dispositions above are the record. In the
  Security tab the owner can dismiss them as "false positive", "used in
  tests" or "won't fix".
