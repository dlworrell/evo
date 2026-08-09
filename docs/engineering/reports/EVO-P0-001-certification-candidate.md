# EVO-P0-001 Project Zero Certification Candidate

Status: Historical candidate; superseded by EVO-P0-002
Owner: EVO
Governing standards: AES-002, AES-003

## Claim

The EVO repository contains a reviewable Project Zero candidate baseline:

- an AES-003 repository manifest with ownership, documentation authority,
  lifecycle, standards, and evidence declarations;
- the canonical Project Zero profile, local engine, wrapper, and stable
  lifecycle workflow entry points from `repo_templates`;
- repository-specific security, contribution, governance, architecture,
  specification, ADR, and engineering-profile records;
- a CMake-based C17 library scaffold with CTest coverage;
- GCC, Clang, static-analysis, formatting, and sanitizer workflows; and
- machine-readable JSON and human-readable Markdown lifecycle evidence.

## Certification Boundary

EVO-P0-002 records the later reviewed transition to `ENGINEERING_READY` and
clarifies that Project Zero is one-time onboarding rather than a recurring
development or release gate. The text below preserves the original candidate
boundary.

This record does not approve its own claim. The manifest therefore declares
`project_zero.state: CERTIFICATION` with certification status `candidate`.

The repository may transition to `ENGINEERING_READY` only after:

1. the candidate implementation and governing documents are reviewed;
2. Project Zero verification, build, test, quality, and sanitizer checks pass
   for the reviewed commit;
3. retained AEMS evidence identifies the reviewed commit;
4. any accepted deferral is explicit, owned, and approved; and
5. a reviewer approves certification in a separate, traceable change.

## Reproducible Commands

```sh
bash scripts/project-zero verify
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 ../AEMS/scripts/aems_project_zero.py \
  . \
  --output build/aems/project-zero \
  --format all
```

## Accepted Deferrals

None are approved by this candidate record.
