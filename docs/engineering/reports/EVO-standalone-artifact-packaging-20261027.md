# Standalone CLI artifact packaging: before and after

Date: 2026-10-27

Related work: [EVO #93](https://github.com/dlworrell/evo/issues/93), which remains open.

## Observation and cause

The supplied `standalone-cmake-clang-40592787634.zip` is 833 bytes, with SHA-256
`59009077450bdcfdaf3db6783fffb253951ab1bd9db6964d98322dfff9d61c56`.
It contains only `evo-dynamic.txt` (1,464 bytes) and `evo-dependencies.txt`
(222 bytes). It contains no executable or compiler toolchain.

[Workflow run 40592787634](https://github.com/dlworrell/evo/actions/runs/40592787634)
used bootstrap commit `700051739b05452066b9f2f7c5a2a3147546c8ca` and checked out
source `606c290c4a266b4646ac625bb14081f1f4c6a7ff`. The job successfully built,
tested and installed `bin/evo`; a later real-provider contract failed at source
capture with resource exit 70. Its unconditional artifact step explicitly
selected only the two text reports and ignored missing files. The uploaded
ZIP's digest in the job log matches the supplied file exactly.

The packaging defect is therefore the upload selection and ambiguous artifact
name, not evidence that the binary failed to compile. The provider/storage
failure was separately repaired and qualified by merged
[PR #138](https://github.com/dlworrell/evo/pull/138). This change starts at main
`ccd922aee364f3d328c294594338c495cd0384b5` and does not reopen that runtime fix.

## Change and expected observable result

Activate the existing reviewed standalone workflow with the packaging step.
After both native frontend jobs pass the complete real-provider installed
contract, package the actual `bin/evo` in a Linux x86-64 tarball. Include the
license, instructions, source commit/compiler/frontend identity, binary hash,
ELF reports, observed required GLIBC symbol versions, complete file checksums,
and an external tarball checksum.

GitHub documents that ZIP artifact uploads do not preserve file permissions:
[upload-artifact permission loss](https://github.com/actions/upload-artifact#permission-loss).
The inner tarball preserves executable mode. The packager extracts its actual
archive, checks every file digest, and runs extracted `--version` and `--help`
with an installation-only PATH before allowing the package to be uploaded.

Successful binaries use `standalone-linux-x86_64-<frontend>-<run-id>-<attempt>`.
The binary upload fails on missing files and cannot run after a failed
validation or packaging step. Diagnostic reports use the separate
`standalone-diagnostics-<frontend>-<run-id>-<attempt>` artifact, retained even
after a failed job. Missing binaries, unexpected RPATH/RUNPATH, private or
unresolved shared libraries, inspection errors and failed extracted execution
cannot produce a completed download. Existing output is never overwritten.

## Validation performed locally

- Ten package regression tests pass using real compiled ELF fixtures. They
  cover executable bytes/mode, complete checksums and provenance, relocated
  execution after removing the original install, absent/non-executable/symlink/
  text inputs, invalid source identity, existing-output protection, inspection
  failure cleanup, extracted-command failure cleanup and build-tree RPATH.
- Fresh GCC 13.3/CMake Debug warnings-as-errors build succeeds. CTest reports
  48 passed, zero failed and three unavailable-provider skips (51 total).
- The installed provider-free CLI contract passes all 34 checks. This host
  cannot qualify real Clang/Bubblewrap execution; the active workflow requires
  that mode without skips before uploading a binary.
- The actual newly installed EVO executable packages successfully, preserving
  its executable bytes and mode through archive extraction, complete checksum
  validation, and extracted help/version execution with an installation-only
  PATH. This local CMake/GCC check uses the accurately labeled `cmake-gcc`
  profile; hosted jobs remain CMake/Clang and Autotools/GCC.
- AES-BLD structural/profile verification passes. Eight release-readiness
  unit tests and four command-schema tests pass. Workflow YAML and embedded
  shell syntax validate; active/reviewed workflow copies match byte for byte.

Hosted evidence for this change belongs to the resulting PR's exact commit.
The earlier run and these local checks do not claim a new hosted CI pass.

## Boundaries and recovery

No C API, ABI, product version, command/provider behavior, kernel capability,
runtime sandbox, target execution policy or release-readiness gate is changed.
Workflow permissions remain `contents: read` and checkout credentials are not
persisted. The packager operates only on the trusted just-built executable,
does not bundle compilers/system libraries and never overwrites an existing
download directory. Failed packaging removes its newly created output.

The manifest/checksums are exact audit and corruption-detection projections,
not signatures, source admission or release qualification. ADR-0026 does not
apply: no accelerated representation is introduced. Permanent release-gate
registration and all other outstanding #93 capability work remain separate.

An older or jailed target still needs the declared loader/libraries/CPU; its
maximum required GLIBC version is only one compatibility check. In particular,
`user.max_user_namespaces=0` still prevents production analysis. No sandbox
fallback or privilege change is introduced. Reverting this single change
removes the packaging helper and active workflow without changing the product.
