# AES-BLD-001 Toolchain Profile

## Status

- Repository: `dlworrell/evo`
- Applicability: `active-native`
- Tracking issue: `dlworrell/evo#14`
- Standard authority: `dlworrell/AES`
- Enforcement authority: `dlworrell/AEMS`
- Waivers: none

## Authoritative toolchains

EVO is a C17 static-library project. CMake and GNU Autotools are independent,
supported build frontends over one declared source and test inventory.

| Path | Compiler | Archive and inspection tools | Linker |
|---|---|---|---|
| CMake/Clang | Clang 18 | LLVM 18 `ar`, `ranlib`, `nm`, and `objdump` | LLD 18 |
| CMake/GCC | GCC 13 | GNU `ar`, `ranlib`, `nm`, and `objdump` | GNU BFD |
| Autotools/Clang | Clang 18 | LLVM 18 tools | LLD 18 |
| Autotools/GCC | GCC 13 | GNU tools | GNU BFD |

The Clang analysis path additionally records Clang-Tidy 18, `llvm-cov`, and
`llvm-profdata`. Checked-in CMake presets select the exact binary-tool family;
the reusable AEMS workflow supplies versioned Ubuntu 24.04 packages and
records their versions as evidence.

## Frontend parity

`CMakeLists.txt` and `Makefile.am` enumerate all twenty-four production
sources and the same thirty normative tests. Both frontends:

- compile the project as C17 with the same warning policy;
- default to warnings-as-errors;
- expose sanitizer instrumentation as an explicit option;
- independently detect `explicit_bzero`, define the same private capability
  macro when available, and otherwise compile the same volatile-byte secure-
  erasure fallback;
- build the private seed-schedule research support only for tests;
- install `libcatalyst_evo.a`, the public header, and `catalyst-evo.pc`;
- support an out-of-tree build without network access after bootstrap.

Generated CMake and Autotools paths are isolated from source inputs.

## Observable evidence

The AEMS gate performs four distinct checks:

1. structure, declared tool bindings, and exact tool versions;
2. CMake and Autotools build/test matrices with both compilers;
3. Clang-Tidy and Clang ASan/UBSan runs;
4. staged install, package metadata, public-symbol, consumer, uninstall, and
   source-distribution parity.

The CMake staged library is inspected with `llvm-nm`; the GNU Autotools staged
library is independently inspected with GNU `nm`. A matching result therefore
cannot be produced by silently routing both frontends through one tool family.
