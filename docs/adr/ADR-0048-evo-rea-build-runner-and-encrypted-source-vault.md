# ADR-0048: EVO/REA Fused Build Runner and Encrypted Source Vault

Status: Accepted
Date: 2026-10-31 (America/Los_Angeles)
Owner: EVO
Tracking: [EVO #132](https://github.com/dlworrell/evo/issues/132)
Builds on: [ADR-0046](ADR-0046-rea-assisted-recovery-and-source-admission.md), [EVO-004](../specs/EVO-004-rea-recovery-integration-contract.md)
Acceptance: accepted by Donovan Worrell on 2026-10-31 by merging EVO #153; section 6 (agent access) accepted on 2026-11-01 and implemented in `evo_rea_build.py` on 2026-11-01

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
   - **People and grants.** Each person is listed once, keyed by their sshid.io username, with their public keys. Each vault grants named people a role:
     - **view** means they can open the vault. This is enforced by encryption: each vault's files are encrypted only to the keys of the people granted it.
     - **edit** means they may also change membership and re-seal. This is enforced by review: `vaults.json` changes go through signed, reviewed PRs.

     Access is fine-grained per vault, not per key: a key is not scoped to anything, but a person opens only the vaults they are granted. A key may belong to only one person.
     Every vault needs at least one editor.
   - **Sealing and opening.**
     - `vault seal NAME` encrypts each file to every member of that vault.
     - File names and directory structure are encrypted too. Ciphertext is stored as numbered objects (`objects/000001.age`), and the map from object to path, with each file's hash, lives only in the encrypted `manifest.age`.
     - The public vault index records only object hashes, the vault name, and each member's person id, label, role and fingerprint. It never records a key or a file name. The number of files and their approximate sizes remain visible.
     - A re-seal goes into a new, empty directory, which replaces the old vault after `vault verify`.
     - `vault verify` checks ciphertext without decrypting. `vault open` decrypts with the caller's own identity file.
   - **Adding people.** `vault person add USER sshid|github|age [WHO]` fetches a published public key from sshid.io or GitHub, or takes an `age1` key. It shows the fingerprint and writes only with `--yes`, after the fingerprint has been confirmed with the person. `vault grant VAULT USER --role` and `vault revoke VAULT USER` change one vault at a time; `vault access USER` lists a person's vaults.
3. **Device-bound keys are encrypt-only.**
   - Termius SSH ID passkeys "never leave your device and cannot be copied or exported". age needs the private key as a file to decrypt, so an SSH ID key can lock files for its owner but can never open them.
   - Such members are recorded with `can_decrypt: false`. `vault seal` refuses any vault in which no member can decrypt, so nothing can be locked away from everyone.
   - SSH ID keys stay the right tool for logging in to machines, such as the isolated build machine, through Termius.
4. **Key custody.**
   - Each person keeps their own private key, and only public keys are committed.
   - The owner holds an offline `age1` recovery key in every vault.
   - Removing a person (`vault revoke`) means re-sealing to the remaining members; the last editor cannot be removed. Copies they already decrypted cannot be recalled; the index records this limit.
5. **Vault location.** The ciphertext and index go to an access-controlled location the owner chooses: a dedicated Dropbox folder that holds only sealed vaults and is not shared, or a private repository. Plaintext never goes there. Never the public EVO repository. CI uses only synthetic files sealed to throwaway test keys.

6. **Agent access (accepted 2026-11-01; implemented).**
   - Agents cannot use SSH ID: its keys are passkeys bound to a person's device and need that person present. Each agent instead gets its own `people` entry (for example `agent-claude`), sponsored by a human editor, with its own `age1` key.
   - The agent's private key lives only in that agent's secret store, never in a repository or in Dropbox. Agents are granted **view** on named vaults only, never **edit**, and the grant carries an expiry date.
   - Dropbox access is separate. It follows the Dropbox account an agent's connector signs in as, not any key. An agent that can read the Dropbox folder sees only ciphertext unless it is also granted that vault.
   - Implementation: `vault agent add` and `vault grant --expires`; `load_vaults` refuses any file that breaks these rules. The sponsor must be an editor of every vault the agent is granted. An expired grant is left out of the next seal. The 90-day ceiling on one grant is an implementation default rather than part of this decision; renewing means granting again.

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
