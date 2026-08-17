# AES-BLD-001 Toolchain Profile

## Status

- Repository: `dlworrell/evo`
- Applicability: `active-native`
- Tracking issue: `dlworrell/evo#14`
- Audit reconciliation: `dlworrell/evo#110`
- Standard authority: `dlworrell/AES`
- Enforcement authority: `dlworrell/AEMS`
- Waivers: none
- Product version: 0.43.0
- Installed reusable-core compatibility version: 0.37.0

<!-- AES-BLD-INVENTORY installed_core=26 private_product=44 production=70 normative_tests=50 -->

## Build-output boundary

EVO is deliberately a two-layer C17 repository rather than only a static-library
project. The installed output at the current 0.43.0 boundary remains the stable
`catalyst_evo` reusable library, public header, and `catalyst-evo.pc`. The
source-optimizer product foundation, command planner, and built-in production
providers are compiled as a private, uninstalled product layer. Issue #93 owns
installation of the standalone executable; until that boundary lands, the
AES-BLD machine profile's `build.kind: c-library` describes the installed
distribution surface, not the complete repository mission.

CMake and GNU Autotools are independent supported build frontends over the same
declared source and normative-test inventory.

## Authoritative toolchains

| Path | Compiler | Archive and inspection tools | Linker |
|---|---|---|---|
| CMake/Clang | Clang 18 | LLVM 18 `ar`, `ranlib`, `nm`, and `objdump` | LLD 18 |
| CMake/GCC | GCC 13 | GNU `ar`, `ranlib`, `nm`, and `objdump` | GNU BFD |
| Autotools/Clang | Clang 18 | LLVM 18 tools | LLD 18 |
| Autotools/GCC | GCC 13 | GNU tools | GNU BFD |

The Clang analysis path additionally records Clang-Tidy 18, `llvm-cov`, and
`llvm-profdata`. Checked-in CMake presets select the exact binary-tool family;
the reusable AEMS workflow supplies versioned Ubuntu 24.04 packages and records
their versions as evidence.

## Machine-authority projection

`.aems/aes-bld-001.json` is the authoritative AES-BLD inventory. At this
revision it projects to:

- **26 installed-core production sources** in `catalyst_evo`;
- **44 private source-optimizer/product production sources** in
  `catalyst_evo_project_foundation`;
- **70 total production sources**; and
- **50 normative build/test targets**.

`scripts/verify_aes_bld_profile.py` derives the CMake target membership, compares
it byte-for-byte by ordered path against the machine inventory, validates every
normative test mapping against both build frontends, and compares these counts
with the marker embedded above. The Documentation Report and Verify Repository
gates execute that verifier, so a source/test inventory change cannot leave this
active human-readable profile stale.

## Frontend parity

Both frontends:

- compile the reusable core and private product foundation as C17 with the same
  warnings-as-errors policy;
- keep the public installed core separate from private source-optimizer headers
  and implementation;
- expose sanitizer instrumentation explicitly and retain independent Clang/GCC
  build authority;
- link the same POSIX thread requirements and preserve deterministic worker
  semantics;
- independently detect `explicit_bzero` for the installed core;
- build the private seed-schedule research support only for tests;
- build ingestion, analysis, recipe, transformation, candidate, assurance,
  measurement, structured search, bounded orchestration/checkpoint-resume,
  the 0.43 command planner, and built-in production-provider plumbing without
  widening the installed public C API;
- build the private core-benchmark executable and equivalent bounded smoke
  evidence targets;
- install `libcatalyst_evo.a`, the public header, and `catalyst-evo.pc`; and
- support out-of-tree operation without network access after bootstrap.

The AEMS consumer inventory remains public-only: installed smoke and reference
adapter translation units compile only through staged package metadata, never a
private include path.

## Repository-owned source-optimizer validation

The release-readiness catalog is the machine authority for required hosted
workflow coverage. Its current `source-optimizer` gates are:

| Gate | Workflow | Release role |
|---|---|---|
| Project Ingestion | `.github/workflows/project-ingestion.yml` | Candidate + production release evidence |
| Project Analysis | `.github/workflows/project-analysis.yml` | Candidate + production release evidence |
| Project Recipe | `.github/workflows/project-recipe.yml` | Candidate + production release evidence |
| Project Transformation | `.github/workflows/project-transformation.yml` | Candidate + production release evidence |
| Project Candidate | `.github/workflows/project-candidate.yml` | Candidate + production release evidence |
| Project Assurance | `.github/workflows/project-assurance.yml` | Candidate + production release evidence |
| Project Measurement | `.github/workflows/project-measurement.yml` | Candidate + production release evidence |
| Project Search | `.github/workflows/project-search.yml` | Candidate + production release evidence |
| Project Orchestration | `.github/workflows/project-orchestration.yml` | Candidate + production release evidence |
| Production Providers | `.github/workflows/production-providers.yml` | Candidate + production release evidence |
| Production Provider Async Lifecycle | `.github/workflows/production-provider-async.yml` | Candidate + production release evidence |

Together these gates cover immutable ingestion, normalized analysis, canonical
recipes, AST-aware transformations, isolated candidate materialization,
candidate correctness/assurance, reproducible measurement, structured recipe
search, bounded orchestration/checkpoint-resume, and concrete production
provider lifecycle behavior. They supplement rather than replace the complete
AES-BLD CMake/Autotools compiler matrix.

The 0.43 `Project Command` contract is represented in the normative inventory by
`tests/project_command_test.c`. It is exercised by the complete build/test
matrices; the release-readiness catalog does not currently define a separate
Project Command workflow, so this profile does not invent one.

## Observable evidence

AES-BLD-001 provides:

1. structure, declared tool bindings, and exact tool-version evidence;
2. independent CMake and Autotools build/test matrices with Clang and GCC;
3. Clang-Tidy plus sanitizer/static-analysis evidence;
4. staged install, package metadata, public-symbol, consumer, uninstall, and
   source-distribution parity; and
5. repository-owned source-optimizer gates listed above, including production
   provider and asynchronous lifecycle proof.

The staged CMake library is inspected with LLVM tooling while the GNU Autotools
staged library is independently inspected with GNU tooling; matching evidence
cannot be produced by silently routing both frontends through one tool family.

## GitHub Actions runtime and supply-chain policy

Issue #116 updates official GitHub actions away from deprecated Node 20 runtime
releases. EVO requires `actions/checkout` major version 5 or newer and
`actions/upload-artifact` major version 6 or newer; those baselines execute on
Node 24. `scripts/verify_github_actions_runtime.py` enforces the repository
floor in `Verify Repository`.

This ticket does **not** change the repository's supply-chain reference policy:
official `actions/*` dependencies remain major-version tags where the existing
workflow fleet already uses that model. Moving to reviewed commit-SHA pins is a
separate governance decision and must not be inferred from this runtime upgrade.
Least-privilege workflow permissions remain unchanged.
