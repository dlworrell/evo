# Contributing

Thank you for contributing to Catalyst EVO.

## Before Changing Code

Use a focused issue or change request to identify the objective, governing
specification or ADR, interface impact, test plan, failure and recovery
behavior, observability, and security implications.

Public API or behavior changes must update the corresponding specification in
`docs/specs/` and documentation in the same change series. A major change to
the language, ABI, allocation model, or build-system boundary requires an ADR.

## Build and Test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the Project Zero baseline locally with:

```sh
bash scripts/project-zero verify
```

## Change Discipline

- Keep each commit to one logical, reviewable change.
- Add tests for behavior changes or document why a test is not applicable.
- Preserve deterministic behavior under a recorded random seed.
- Do not weaken correctness, safety, or validation gates to improve fitness.
- Update benchmarks when performance claims change.
- Record every AES exception in the appropriate waiver or ADR.
- Open a pull request; do not write directly to `main`.

The pull-request template lists the evidence expected for review.
