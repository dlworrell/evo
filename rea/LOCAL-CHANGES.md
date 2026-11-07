# REA local changes

EVO maintains the REA tree under `rea/` (ADR-0046 Amendment 1). This file lists
every change EVO makes to it, so the next upstream sync can re-apply or retire
each one. `tools/rea_recovery/rea_upstream.py check` fails when `rea/` differs
from `rea/UPSTREAM.json` and nothing is recorded here.

## Local fixes

Each fix is an EVO pull request that changes files under `rea/` and carries a
`REA-Local: <reason>` commit trailer.

| Commit | Paths | Reason | Upstream status |
| --- | --- | --- | --- |

## Divergences

Paths where EVO has stopped taking upstream changes (owner decision).

| Path | Date | Reason | Last upstream commit taken |
| --- | --- | --- | --- |
