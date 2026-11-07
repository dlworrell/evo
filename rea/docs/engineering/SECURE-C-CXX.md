# Secure C/C++ Profile: `rea/` scope

Status: Adopted (bootstrap)
Repository: `dlworrell/evo`
Scope: `rea/` (ADR-0046 Amendment 1; Catylist ADR-0004)
Inherited Standard: AES-SEC-001
Waiver Log: `rea/docs/engineering/AES-SEC-001-waivers.md`
Review dispositions: `rea/docs/engineering/AES-SEC-001-review-dispositions.json`

This file and the two above are EVO-owned scope evidence. They are not part of
upstream REA, and `tools/rea_recovery/rea_upstream.py` leaves them out of the
upstream tree digest.

## C and C++ surface

REA is a TypeScript/Node project. Its C, C++ and Objective-C files are:

- `native/windows/src/`: the optional Windows native add-on (C++), built only
  on Windows;
- `scripts/fixtures/` and `tests/conformance/`: small programs compiled as
  inputs for REA's analysis tests. They are fixtures, not shipped code, and some
  deliberately use the patterns REA must recognise.

## Policy

- No banned AES-SEC-001 interface may be added under `rea/`.
- Each `review-required` finding is in the baseline of the disposition ledger
  until it is reviewed. New findings need a disposition before merge.
- An EVO change to upstream C/C++ follows this profile and is recorded as an
  `REA-Local` change in `rea/LOCAL-CHANGES.md`.
- Upstream syncs that add C/C++ findings list them in the sync pull request.

## Required local behaviour for EVO-authored native changes

- avoid banned unsafe interfaces;
- carry explicit lengths for external buffers;
- check allocation and copy-size arithmetic for overflow;
- compile cleanly under REA's own build for that platform;
- record every approved exception in the waiver log.

## Ratchet rule

New violations block acceptance unless an approved waiver exists. The findings
present when REA was ingested are baselined and reviewed over time.
