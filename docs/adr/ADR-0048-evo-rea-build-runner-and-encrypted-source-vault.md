# ADR-0048: EVO/REA Fused Build Runner and Encrypted Source Vault

Status: Proposed
Date: 2026-10-31 (America/Los_Angeles)
Owner: EVO
Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132)
Builds on: [ADR-0046](ADR-0046-rea-assisted-recovery-and-source-admission.md), [EVO-004](../specs/EVO-004-rea-recovery-integration-contract.md)
Acceptance: pending an attributable owner decision through the signed PR process

## Context

On 2026-10-31 the owner directed two things:

- The fused EVO/REA product should be executable, with a build file that discovers its environment, source files, destination, PDF printing and UI integration. The owner first said "REA" and then clarified on the same day that this means the EVO/REA fusion.
- The source files should be stored "in an encrypted vault for authorised users".

ADR-0046 keeps REA an external-reference dependency at `9e22da8` and forbids vendoring it. EVO-004 §3 requires original evidence to be kept byte-for-byte in an access-controlled store, and keeps private captures out of public CI. TASK-38 Lane B (PRM-0047) recorded that `rea setup` writes to agent client configuration and installs skills. That is a sensitive write outside REA's own directory.

## Decision

1. **The build runner lives in EVO, not in REA.** `tools/rea_recovery/evo_rea_build.py` builds the fused product into one destination. EVO is built from its own checkout; REA is driven from a separate checkout pinned by ADR-0046, and its files are never changed. It has four commands:
   - **`discover`** writes a JSON report of:
     - the operating system and architecture;
     - the EVO checkout and version, a C compiler, CMake 3.20 or later, and Bubblewrap (the sandbox `evo analyze` uses);
     - Node.js against REA's `engines` range, plus npm;
     - whether the REA checkout is at the pinned commit with a clean tree;
     - whether the destination is writable;
     - which PDF backends and printers exist;
     - whether a display or browser is available for UI use;
     - the vault tool, and which vaults someone can actually open.

     Every check reports `ok`, `missing` or `mismatch` with the evidence behind it. Nothing is guessed.
   - **`build`** prints the plan by default and runs only with `--execute`. The plan is each project's own documented build:
     - **EVO:** CMake configure, build, `ctest`, then `cmake --install` into the destination.
     - **REA:** `npm ci`, then `npm run build:cached` in the REA checkout, followed by the read-only `rea doctor --json`.

     It then records the fused layout in `evo-rea.json`. It refuses when `discover` reports a blocking problem.
   - **`pdf`** renders a Markdown or HTML report, such as `evo report` output, to PDF with the first available backend: pandoc, then headless Chromium. It sends the PDF to a printer only when `--print` is given and a print command exists.
   - **`ui`** reports or starts the fused product's interfaces: the installed `evo` CLI, REA's CLI, or REA's stdio MCP server (`rea mcp`). Starting needs `--execute`.

   The runner never runs `rea setup`, `rea update` or `rea uninstall`, and never edits agent or client configuration.
2. **Vaults follow the Termius model, encrypted with [age](https://age-encryption.org/).** The owner pointed to [Termius vaults](https://docs.termius.com/getting-started/learn-about-vaults) as the model.
   - **Named vaults.** `tools/rea_recovery/vaults.json` defines them:
     - a **Personal** vault (`personal-donovan`);
     - a shared **Team** vault (`team`);
     - **custom** vaults per project or environment, starting with `evo-rea-sources` for EVO/REA source files and original REA evidence.
   - **Members and roles.** Each member is a label, a public key and a role:
     - **view** means they can open the vault. This is enforced by encryption: only listed keys can decrypt.
     - **edit** means they may also change membership and re-seal. This is enforced by review: `vaults.json` changes go through signed, reviewed PRs.

     Every vault needs at least one editor.
   - **Sealing and opening.**
     - `vault seal NAME` encrypts each file to every member of that vault.
     - The vault index records each file's hashes, the vault name, and each member's label, role and fingerprint. It never records a key.
     - `vault verify` checks ciphertext without decrypting. `vault open` decrypts with the caller's own identity file.
   - **Adding people.** `vault add-member NAME sshid|github|age WHO --label --role` fetches a published public key from sshid.io or GitHub, or takes an `age1` key. It shows the fingerprint and writes only with `--yes`, after the fingerprint has been confirmed with the person.
3. **Device-bound keys are encrypt-only.**
   - Termius SSH ID passkeys "never leave your device and cannot be copied or exported". age needs the private key as a file to decrypt, so an SSH ID key can lock files for its owner but can never open them.
   - Such members are recorded with `can_decrypt: false`. `vault seal` refuses any vault in which no member can decrypt, so nothing can be locked away from everyone.
   - SSH ID keys stay the right tool for logging in to machines, such as the isolated build machine, through Termius.
4. **Key custody.**
   - Each person keeps their own private key, and only public keys are committed.
   - The owner holds an offline `age1` recovery key in every vault.
   - Removing a person means re-sealing to the remaining members. Copies they already decrypted cannot be recalled; the index records this limit.
5. **Vault location.** The ciphertext and index go to an access-controlled location the owner chooses, such as a private Dropbox folder or a private repository. Never the public EVO repository. CI uses only synthetic files sealed to throwaway test keys.

## Alternatives considered

- **gpg.** Available, but harder to operate safely: a web of trust, many options, and easy mistakes.
- **git-crypt or SOPS.** These tie the vault to one repository and need extra tooling.
- **A provider-managed vault.** That needs an account and credentials the project doesn't have yet.
- **Storing the files in Termius itself.** Termius vaults hold hosts, keys, snippets and port-forwarding rules, not arbitrary files, so they can't hold source trees. Termius is used for access to machines, and the age vaults copy its structure for files.

age is small, audited, packaged on Ubuntu and macOS, and does multi-recipient encryption without configuration.

## Consequences

- The fused EVO/REA product becomes buildable from one command into one destination, without vendoring REA and without agent-configuration side effects.
- The vault protects files at rest and in transit. It does not protect a file after an authorised person decrypts it.
- The EVO tests and the REA `npm ci` and build run real code, and REA's dependency tree is large. Run them only on an isolated machine or container, never on CI runners that hold secrets.

## Verification before acceptance

- Tests cover `discover` against fake environments, the default build plan, and refusal on a pin mismatch.
- Tests seal and open with throwaway age keys, and confirm tamper and wrong-key refusal.
- Owner review of the key-custody rules above.
