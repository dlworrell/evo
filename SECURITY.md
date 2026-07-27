# Security Policy

## Supported Versions

Until the first stable release, security fixes apply to the `main` branch.

## Reporting a Vulnerability

Report suspected vulnerabilities privately to the repository owner, preferably
through a private GitHub security advisory when that option is available. Do
not disclose a suspected vulnerability in a public issue before the owner has
had a reasonable opportunity to assess and remediate it.

Include:

- a clear description of the issue and its impact;
- affected versions, files, and symbols;
- reproduction steps or a minimal test case;
- the relevant input, memory, ABI, or trust boundary; and
- a suggested mitigation, if known.

## Security Expectations

EVO is a native C17 library and follows the local AES-SEC-001 profile in
`docs/engineering/SECURE-C-CXX.md`.

Changes must:

- validate externally controlled lengths, indices, and serialized values;
- check allocation and copy-size arithmetic for overflow;
- avoid banned unsafe interfaces and custom cryptography;
- preserve correctness constraints as hard optimization boundaries;
- keep GitHub Actions permissions at least privilege;
- run warning-clean, static-analysis, and sanitizer checks; and
- record approved exceptions in
  `docs/engineering/AES-SEC-001-waivers.md`.
