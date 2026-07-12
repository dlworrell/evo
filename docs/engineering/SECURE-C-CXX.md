# Secure C/C++ Profile

Status: Adopted
Repository: `dlworrell/evo`
Inherited Standard: AES-SEC-001
Waiver Log: `docs/engineering/AES-SEC-001-waivers.md`

## Policy

No new C or C++ code may be accepted unless it follows AES-SEC-001.

## Required Local Behavior

EVO native code must:

- avoid banned unsafe interfaces;
- carry explicit lengths for external buffers;
- validate serialized and user-controlled lengths before use;
- check allocation and copy-size arithmetic for overflow;
- avoid signed-to-unsigned length conversion until bounds are proven;
- isolate unsafe operations behind reviewed interfaces;
- compile cleanly under the repository warning profile;
- run sanitizer tests when supported;
- include fuzz coverage for parsers and external-input handlers when applicable;
- avoid custom cryptography;
- record every approved exception in the waiver log.

## Unsafe Code

Unsafe operations are permitted only when required by an ABI, platform, allocator, parser, or low-level boundary. The code and its review must state the invariant that makes the operation safe enough.

## Ratchet Rule

New violations block acceptance unless an approved waiver exists. Existing violations, if discovered, are baselined and removed over time.