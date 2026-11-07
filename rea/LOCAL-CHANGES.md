# REA local changes

EVO maintains the REA tree under `rea/` (ADR-0046 Amendment 1). This file lists
every change EVO makes to it, so the next upstream sync can re-apply or retire
each one. `tools/rea_recovery/rea_upstream.py check` fails when `rea/` differs
from `rea/UPSTREAM.json` and nothing is recorded here.

## Local fixes

Each fix is an EVO pull request that changes files under `rea/` and carries a
`REA-Local: <reason>` commit trailer.

| Commit           | Paths                                                                                                               | Reason                                                                                         | Upstream status |
| ---------------- | ------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- | --------------- |
| 27f6294          | src/application/desktop/DesktopBuildBundle.ts                                                                       | Apt key read through one bounded descriptor (CodeQL js/file-system-race)                       | Not offered yet |
| 27f6294, b6f0095 | src/application/decompileDirectory/exportTree.ts                                                                    | Resume journal opened first and checked on its descriptor (js/file-system-race)                | Not offered yet |
| 27f6294          | src/javascript/analysis/JavaScriptAnalysisWorker.ts                                                                 | Static checkpoint size checks and read on one descriptor (js/file-system-race)                 | Not offered yet |
| 27f6294          | scripts/electron-active-hook-boundaries.cjs                                                                         | dlopen digest through one bounded descriptor (js/file-system-race)                             | Not offered yet |
| 27f6294          | scripts/build-windows-native.mjs                                                                                    | Downloads verified before they are written (js/http-to-file-access)                            | Not offered yet |
| bc712dd          | src/application/decompileDirectory/exportTreeJournal.test.ts                                                        | Tests for the journal's open-first refusals (EVO #212)                                         | Not offered yet |
| b989e54          | bridge/hopper_bridge.py                                                                                             | Docstring lists every global the bootstrap injects (EVO #204)                                  | Not offered yet |
| 2342e53          | bridge/agentenv/computer.py, bridge/agentenv/test_computer.py                                                       | Usage error keeps the JSON-line protocol and exits 2 (EVO #203)                                | Not offered yet |
| de29ba2          | bridge/ghidra/ReaGhidraApplySeeds.java                                                                              | Unknown seed kinds refused as malformed (EVO #205)                                             | Not offered yet |
| a2210ed          | bridge/mitmproxy/capture.py, bridge/mitmproxy/test_capture.py                                                       | mitmdump shut down on an unreadable request (EVO #206)                                         | Not offered yet |
| 03e6ee0          | bridge/native/rea_lldb_tracer.py, bridge/native/test_rea_lldb_tracer.py                                             | Identity-acknowledgement wait bounded at 30 s (EVO #208)                                       | Not offered yet |
| cca59a5          | bridge/nativeaot/NativeAotExtension.java                                                                            | inspectImage() before analyze() is an IllegalStateException (EVO #209)                         | Not offered yet |
| 3df3c31          | bridge/mitmproxy/test_capture.py                                                                                    | Tests for redaction, nesting and read limits, decode() and failure_reply (EVO #207)            | Not offered yet |
| d39f746          | src/application/desktop/DesktopBuildBundle.ts, src/application/desktop/DesktopBuildBundle.test.ts                   | readAptKey and MAX_KEY_BYTES exported for new tests (EVO #214)                                 | Not offered yet |
| 9d6517c          | src/javascript/analysis/JavaScriptAnalysisWorker.ts, src/javascript/analysis/JavaScriptStaticCheckpointFile.test.ts | Checkpoint read extracted as readStaticCheckpointFile, with tests (EVO #213)                   | Not offered yet |
| 129afdd          | src/electron/electronActiveHookDigest.test.ts                                                                       | Tests for the hook digest limit and refusals (EVO #215)                                        | Not offered yet |
| 9429d61          | bridge/process/ProcessRunTokenReaderTests.swift                                                                     | Swift tests for the procargs parser, observations and sysctl retry, run on macOS CI (EVO #210) | Not offered yet |

## Vendored layout

EVO carries REA as plain files. Several upstream files describe a git checkout
of REA and do nothing inside EVO. They are kept byte-identical to upstream and
are listed here instead of being edited.

- **Submodules are vendored (EVO #199, #200, #201).** REA's five git submodules
  (`third_party/binwalk`, `ghidra-nativeaot`, `jadx-headless-mcp`, `unblob` and
  `wakaru`) are copied in as ordinary files, each with its licence files (owner
  decision, ADR-0046 Amendment 1). Their pinned commits and tree digests are in
  `UPSTREAM.json` under `vendored_submodules`. Where `THIRD_PARTY_NOTICES.md`
  says "pinned submodule" and "Git submodules pin inspection sources", read
  "vendored under `rea/third_party` at the commit in `UPSTREAM.json`". Inside
  EVO, `rea/.gitmodules` and the `git submodule update` steps in
  `CONTRIBUTING.md` (including the Windows Unblob workaround) do not apply.
  To refresh one, copy the submodule's tree at REA's new pin into
  `rea/third_party/<name>` and re-record with
  `rea_upstream.py record <rea-checkout> --vendored third_party/<name>=<checkout>`
  (see `tools/rea_recovery/README.md`). Recording fails unless the copy
  matches the checkout file for file.
- **Git LFS objects are excluded (EVO #202).** Unblob's 2,648 LFS test files
  are not in EVO. `UPSTREAM.json` lists them by `top_paths` and says how to get
  them.
- **`.github/CODEOWNERS` is inert (EVO #197).** GitHub reads CODEOWNERS only at
  the repository root, `.github/` or `docs/`, so `rea/.github/CODEOWNERS`
  gates nothing. Its root-anchored paths would also match EVO's root, not
  `rea/`. Changes under `rea/` are review-gated by EVO's branch rules and by
  this file's REA-Local trailer, not by REA's owners.
- **`.github/dependabot.yml` is inert (EVO #198).** Dependabot reads only the
  repository-root file, and EVO has none. Dependency updates for `rea/`
  (`package-lock.json`, `scripts/verify-tools/skills`, `website/requirements.txt`
  and `.github/actions`) arrive only through an upstream re-ingest. Dependency
  review still checks every pull request that changes them.
- **Workflows under `rea/.github/workflows` do not run.** EVO runs the `rea/`
  scope's own checks (ADR-0046 Amendment 1, ADR-0049).

## Divergences

Paths where EVO has stopped taking upstream changes (owner decision).

| Path | Date | Reason | Last upstream commit taken |
| ---- | ---- | ------ | -------------------------- |
