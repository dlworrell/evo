# REA recovery companion (EVO-004)

## EVO/REA fused build runner and encrypted source vault (ADR-0048, proposed)

`evo_rea_build.py` builds the fused product into one destination. EVO is built from this checkout with its documented CMake build. REA is built in a separate checkout pinned to `9e22da8`, and its files are never changed.

```sh
python3 tools/rea_recovery/evo_rea_build.py discover --rea ../rea --dest /opt/evo-rea
python3 tools/rea_recovery/evo_rea_build.py build    --rea ../rea --dest /opt/evo-rea            # prints the plan
python3 tools/rea_recovery/evo_rea_build.py build    --rea ../rea --dest /opt/evo-rea --execute  # EVO cmake build/ctest/install, then REA npm ci/build, doctor
python3 tools/rea_recovery/evo_rea_build.py ui evo   --dest /opt/evo-rea [--execute] [evo args]
python3 tools/rea_recovery/evo_rea_build.py ui rea   --dest /opt/evo-rea [--execute] [mcp | <rea CLI args>]
python3 tools/rea_recovery/evo_rea_build.py pdf      report.md --out report.pdf [--print]
python3 tools/rea_recovery/evo_rea_build.py vault list
python3 tools/rea_recovery/evo_rea_build.py vault person add SSHID_USER sshid [--name NAME] [--yes]
python3 tools/rea_recovery/evo_rea_build.py vault person add SSHID_USER age age1... [--device iphone] [--yes]
python3 tools/rea_recovery/evo_rea_build.py vault person remove-key SSHID_USER --fingerprint FP
python3 tools/rea_recovery/evo_rea_build.py vault grant  VAULT SSHID_USER [--role view|edit]
python3 tools/rea_recovery/evo_rea_build.py vault revoke VAULT SSHID_USER
python3 tools/rea_recovery/evo_rea_build.py vault access SSHID_USER
python3 tools/rea_recovery/evo_rea_build.py vault seal   VAULT SOURCE_DIR --out VAULT_DIR
python3 tools/rea_recovery/evo_rea_build.py vault verify VAULT_DIR
python3 tools/rea_recovery/evo_rea_build.py vault open   VAULT_DIR --identity ~/.age/me.key --dest OPENED_DIR
```

- **`discover`** checks:
  - the platform;
  - the EVO checkout and version, a C compiler, CMake 3.20 or later, and Bubblewrap (the sandbox `evo analyze` uses);
  - Node.js against REA's `engines` range, and npm;
  - that the REA checkout is at the pin with a clean tree;
  - that the destination is writable;
  - the PDF backends (pandoc, headless Chromium) and printers (`lp`, `lpr`);
  - whether a display is available for UI;
  - the vault tool, and which vaults someone can actually open.

  Missing EVO source, C compiler, CMake, Node.js, npm, REA source or destination blocks `build`.
- **`ui`** runs the installed `evo` CLI, REA's CLI, or REA's stdio MCP server, using the layout `build` records in `evo-rea.json`. The runner refuses `rea setup`, `rea update` and `rea uninstall`, because they write agent configuration or install.
- **Vaults, modelled on Termius.** `vaults.json` (schema `evo.rea-vaults.v2`) holds the named vaults: `personal-donovan`, `team` and the custom `evo-rea-sources`.
- **People, by sshid.** Each person is listed once under `people`, keyed by their sshid.io username, with their public keys. `vault person add USER sshid` fetches the key from sshid.io and shows its fingerprint. Confirm the fingerprint with the person, then rerun with `--yes`.
- **Grants, per vault.** `vault grant VAULT USER --role view|edit` gives one person access to one vault. **view** means they can open it; **edit** means they may also change members and re-seal, enforced by PR review. `vault access USER` shows exactly which vaults a person has.
- **Fine-grained by vault, not by key.** A key is not scoped to a vault. Each vault's files are encrypted only to the keys of the people granted that vault, so one person with one key opens exactly the vaults they are granted and no others. A key may belong to only one person.
- **Revoking.** `vault revoke VAULT USER` removes the grant (never the last editor). Re-seal the vault afterwards. Copies the person already had stay readable to them, so rotate anything secret in them.
- **SSH ID keys are encrypt-only.** Termius SSH ID passkeys can't be exported, so age can't open files with them. They are stored with `can_decrypt: false`, and `vault seal` refuses any vault that no member can open. Each person who must open a vault adds an `age1` key with `age-keygen -o ~/.age/me.key`, followed by `vault person add USER age age1...`; it applies to every vault they are granted. The owner keeps an offline recovery key in every vault.
- **Names are encrypted too.** `vault seal` stores files as numbered objects; the paths live only in the encrypted `manifest.age`. The public `vault-index.json` lists object hashes and member fingerprints, so `vault verify` needs no key. Seal into an empty directory.
- **One key per device.** Add each device's key with `--device` (for example `iphone`, `mac`, `recovery`); any of a person's keys opens their vaults. If a device is lost, `vault person remove-key USER --fingerprint FP` removes just that key and lists the vaults to re-seal. A person's last key can't be removed; revoke their grants instead.
- **Older files.** A v1 file (keys listed inside each vault) is upgraded to v2 when loaded and written back as v2 on the next change.


`companion_manifest.py` checks a **companion manifest v0.1** (EVO-004 §3) against the retained artifacts it references. This is the first implementation slice of [#132](https://github.com/dlworrell/evo/issues/132), stage E1. Its tests use synthetic fixtures only.

```sh
python3 tools/rea_recovery/companion_manifest.py MANIFEST.json --store STORE_DIR [--pin COMMIT]
```

The command exits with 0 when the manifest is accepted, 3 when it is refused, and 2 for unusable input.

## What it refuses

- **Evidence IDs.** Unknown or duplicate `evidence_id`s, or a native bundle that lists none.
- **References.** A missing, symlinked or altered artifact, or a locator that is absolute or contains `..` or a backslash.
- **Versions and fields.** An unsupported contract version, or unknown or missing top-level fields.
- **REA pin.** A REA commit other than the supported pin. The default is the EVO-004 pin `9e22da8`; `--pin` overrides it.
- **Native bundles.** A bundle that no longer has exactly REA's seven native arrays. Enrichment is refused.
- **Calibration.** Calibration marked unknown or not-applicable without a rationale, or with values attached.
- **Claims.** A passing claim with no evidence, or with evidence that isn't in the manifest. A synthetic manifest passing stage C1, A1, O1, P1 or H1.
- **Access.** A restricted artifact inside a public manifest, or a derivative that doesn't name its original.

## What it does not do

It never runs REA and never changes an artifact. An accepted manifest only matches the bytes it names; it is not compatibility, measurement, timing or safety evidence.

## Design choices (31 October 2026)

1. **Stay on the EVO-004 pin `9e22da8`.** The candidate `eb3b90b` changes only dependencies (vite, zod, fast-uri), and REA's evidence and bundle validator sources are byte-identical. But no retained test run shows REA behaves the same after those changes (TASK-38 finding F-001). The manifest records the pin it was built against, and `--pin` lets a later decision move to the candidate without changing code.
2. **Synthetic first record.** No instrument or capture has been chosen, and private captures may not enter public CI. Synthetic fixtures can prove the E1 refusal behaviour now. They are labelled synthetic and are barred from passing stage C1 or later.
3. **The evidence store is just a directory.** Locators are paths relative to a store root given at run time. That keeps the validator independent of where originals live (local disk, an access-controlled share), so choosing the real store stays a separate decision.
4. **Strict, closed format.** Unknown top-level fields are refused so that equipment detail cannot leak into the native bundle or drift unreviewed. Groups whose fields EVO-004 leaves to later stages (`subject`, `interface`) are accepted as objects for now and tightened when C1 is implemented.
