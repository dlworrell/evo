# EVO Version Surfaces

EVO deliberately carries two semantic version surfaces while the source optimizer remains private and the reusable `catalyst_evo` C17 core is the installed package.

## Product implementation version

The EVO product implementation version tracks the repository-level source-optimizer delivery boundary. It is the `project(... VERSION ...)` value in CMake and the `AC_INIT` package version in Autotools. At the 0.42.0 boundary it describes the private source-optimizer foundation through bounded external-process orchestration.

This version is not an installed-core compatibility claim. Advancing the private source optimizer does not, by itself, advance the public reusable C core API or ABI.

Build frontends expose this value internally as `EVO_PRODUCT_VERSION`.

## Installed reusable-core version

The installed `catalyst_evo` library version is authoritative in the public header:

- `EVO_VERSION_MAJOR`
- `EVO_VERSION_MINOR`
- `EVO_VERSION_PATCH`

At the current boundary those macros identify core version `0.37.0`.

Both build frontends derive `EVO_CORE_VERSION` from those public macros. The generated `catalyst-evo.pc` file publishes `EVO_CORE_VERSION`, so `pkg-config --modversion catalyst-evo` must report the same semantic version as a consumer compiled against `<catalyst/evo/evo.h>`.

A private product-version increment must therefore leave installed pkg-config metadata unchanged unless the reusable core version macros are also intentionally advanced under the core compatibility contract.

## Compatibility rule

For an installed `catalyst_evo` package, these values must agree exactly:

1. `pkg-config --modversion catalyst-evo`;
2. the version compiled from `EVO_VERSION_MAJOR`, `EVO_VERSION_MINOR`, and `EVO_VERSION_PATCH` in the staged public header; and
3. the version metadata emitted by both CMake and Autotools installs.

No public symbol or ABI change is implied by separating these version surfaces.

## Enforcement

`scripts/verify-installed-core-version.sh` is the staged-install assertion. It deliberately resolves only pkg-config metadata beneath the supplied installation prefix, compiles and links a small external consumer against the staged static package, executes that consumer to read the public header macros, and fails if the two versions differ.

`.github/workflows/version-parity.yml` runs this assertion independently for CMake and GNU Autotools staged installs. This makes future product/core version drift a CI failure rather than a downstream ambiguity.

The broader AES-BLD-001 parity workflow remains authoritative for full compiler/toolchain, install-manifest, symbol, package-metadata, and downstream-consumer parity.
