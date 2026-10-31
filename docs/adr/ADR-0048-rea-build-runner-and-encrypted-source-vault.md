# ADR-0048: REA Build Runner and Encrypted Source Vault

Status: Proposed
Date: 2026-10-31 (America/Los_Angeles)
Owner: EVO
Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132)
Builds on: [ADR-0046](ADR-0046-rea-assisted-recovery-and-source-admission.md), [EVO-004](../specs/EVO-004-rea-recovery-integration-contract.md)
Acceptance: pending an attributable owner decision through the signed PR process

## Context

On 2026-10-31 the owner directed two things:

- REA should be executable, with a build file that discovers its environment, source files, destination, PDF printing and UI integration.
- The source files should be stored "in an encrypted vault for authorised users".

ADR-0046 keeps REA an external-reference dependency at `9e22da8` and forbids vendoring it. EVO-004 §3 requires original evidence to be kept byte-for-byte in an access-controlled store, and keeps private captures out of public CI. TASK-38 Lane B (PRM-0047) recorded that `rea setup` writes to agent client configuration and installs skills. That is a sensitive write outside REA's own directory.

## Decision

1. **The build runner lives in EVO, not in REA.** `tools/rea_recovery/rea_build.py` drives a separate REA checkout and never changes REA's files. It has four commands:
   - **`discover`** writes a JSON report of:
     - the operating system and architecture;
     - Node.js against REA's `engines` range, plus npm;
     - whether the REA checkout is at the pinned commit with a clean tree;
     - whether the destination is writable;
     - which PDF backends and printers exist;
     - whether a display or browser is available for UI use;
     - the vault tool and recipients.

     Every check reports `ok`, `missing` or `mismatch` with the evidence behind it. Nothing is guessed.
   - **`build`** prints the plan by default and runs only with `--execute`. The plan is REA's own documented source build, `npm ci` then `npm run build:cached`, run in the REA checkout, followed by the read-only `rea doctor --json`. It refuses when `discover` reports a blocking problem.
   - **`pdf`** renders a Markdown or HTML report to PDF with the first available backend: pandoc, then headless Chromium. It sends the PDF to a printer only when `--print` is given and a print command exists.
   - **`ui`** reports or starts REA's own interfaces: the CLI and the stdio MCP server (`rea mcp`). Starting needs `--execute`.

   The runner never runs `rea setup`, `rea update` or `rea uninstall`, and never edits agent or client configuration.
2. **The encrypted vault uses [age](https://age-encryption.org/), with one key pair per authorised person.**
   - `vault seal` encrypts each source file separately to every listed recipient.
   - It writes a vault index holding each file's path, plaintext sha256 and byte count, ciphertext sha256, and the fingerprints of the recipients it was sealed to.
   - `vault verify` checks the ciphertext without decrypting. `vault open` decrypts with the caller's own identity file into a destination that the caller chooses.
3. **Key custody.**
   - Each authorised person generates their own key with `age-keygen` and keeps the private key themselves.
   - Only public keys go into the recipients file, which is reviewed like any other change.
   - Private keys are never committed, uploaded, printed or sent to an agent.
   - The owner holds a recovery key kept offline.
   - Removing a person means re-sealing to the remaining recipients. Files that person has already opened cannot be recalled; the index records this limit.
4. **Vault location.** The ciphertext and index go to an access-controlled location the owner chooses, such as a private Dropbox folder or a private repository. Never the public EVO repository. CI uses only synthetic files sealed to a throwaway test key.

## Alternatives considered

- **gpg.** Available, but harder to operate safely: a web of trust, many options, and easy mistakes.
- **git-crypt or SOPS.** These tie the vault to one repository and need extra tooling.
- **A provider-managed vault.** That needs an account and credentials the project doesn't have yet.

age is small, audited, packaged on Ubuntu and macOS, and does multi-recipient encryption without configuration.

## Consequences

- REA becomes buildable and inspectable from EVO without vendoring it and without agent-configuration side effects.
- The vault protects files at rest and in transit. It does not protect a file after an authorised person decrypts it.
- `npm ci` and the REA build run third-party code, and REA's dependency tree is large. Run them only on an isolated machine or container, never on CI runners that hold secrets.

## Verification before acceptance

- Tests cover `discover` against fake environments, the default build plan, and refusal on a pin mismatch.
- Tests seal and open with throwaway age keys, and confirm tamper and wrong-key refusal.
- Owner review of the key-custody rules above.
