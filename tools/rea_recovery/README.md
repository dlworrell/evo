# REA recovery companion (EVO-004)

## Build runner and encrypted source vault (ADR-0048, proposed)

`rea_build.py` drives a separate REA checkout pinned to `9e22da8` and never changes REA's files.

```sh
python3 tools/rea_recovery/rea_build.py discover --rea ../rea --dest out/ [--recipients vault-recipients.txt]
python3 tools/rea_recovery/rea_build.py build    --rea ../rea --dest out/            # prints the plan
python3 tools/rea_recovery/rea_build.py build    --rea ../rea --dest out/ --execute  # npm ci; npm run build:cached; rea doctor --json
python3 tools/rea_recovery/rea_build.py ui       --rea ../rea [--execute] [mcp | <rea CLI args>]
python3 tools/rea_recovery/rea_build.py pdf      report.md --out report.pdf [--print]
python3 tools/rea_recovery/rea_build.py vault seal   SOURCE_DIR --vault VAULT_DIR --recipients vault-recipients.txt
python3 tools/rea_recovery/rea_build.py vault verify VAULT_DIR
python3 tools/rea_recovery/rea_build.py vault open   VAULT_DIR --identity ~/.age/me.key --dest OPENED_DIR
```

- **`discover`** checks:
  - the platform;
  - Node.js against REA's `engines` range, and npm;
  - that the REA checkout is at the pin with a clean tree;
  - that the destination is writable;
  - the PDF backends (pandoc, headless Chromium) and printers (`lp`, `lpr`);
  - whether a display is available for UI;
  - the vault tool and recipients.

  Missing Node.js, npm, REA source or destination blocks `build`.
- **`ui`** runs REA's CLI or its stdio MCP server. The runner refuses `rea setup`, `rea update` and `rea uninstall`, because they write agent configuration or install.
- **Vault keys.** Each authorised person runs `age-keygen -o ~/.age/me.key` and adds only the public `age1…` line to the recipients file. Private keys never leave their owner. The vault index records file hashes and recipient fingerprints, never keys.


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
