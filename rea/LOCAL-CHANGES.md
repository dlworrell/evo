# REA local changes

EVO maintains the REA tree under `rea/` (ADR-0046 Amendment 1). This file lists
every change EVO makes to it, so the next upstream sync can re-apply or retire
each one. `tools/rea_recovery/rea_upstream.py check` fails when `rea/` differs
from `rea/UPSTREAM.json` and nothing is recorded here.

## Local fixes

Each fix is an EVO pull request that changes files under `rea/` and carries a
`REA-Local: <reason>` commit trailer.

| Commit           | Paths                                                        | Reason                                                                          | Upstream status |
| ---------------- | ------------------------------------------------------------ | ------------------------------------------------------------------------------- | --------------- |
| 27f6294          | src/application/desktop/DesktopBuildBundle.ts                | Apt key read through one bounded descriptor (CodeQL js/file-system-race)        | Not offered yet |
| 27f6294, b6f0095 | src/application/decompileDirectory/exportTree.ts             | Resume journal opened first and checked on its descriptor (js/file-system-race) | Not offered yet |
| 27f6294          | src/javascript/analysis/JavaScriptAnalysisWorker.ts          | Static checkpoint size checks and read on one descriptor (js/file-system-race)  | Not offered yet |
| 27f6294          | scripts/electron-active-hook-boundaries.cjs                  | dlopen digest through one bounded descriptor (js/file-system-race)              | Not offered yet |
| 27f6294          | scripts/build-windows-native.mjs                             | Downloads verified before they are written (js/http-to-file-access)             | Not offered yet |
| bc712dd          | src/application/decompileDirectory/exportTreeJournal.test.ts | Tests for the journal's open-first refusals (EVO #212)                          | Not offered yet |

## Divergences

Paths where EVO has stopped taking upstream changes (owner decision).

| Path | Date | Reason | Last upstream commit taken |
| ---- | ---- | ------ | -------------------------- |
