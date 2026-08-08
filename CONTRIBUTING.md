# Contributing

Thank you for contributing to Catalyst EVO.

## Before Changing Code

Use a focused issue or change request to identify the objective, governing
specification or ADR, interface impact, test plan, failure and recovery
behavior, observability, and security implications.

Every proposal must also identify whether it adds or changes a compressed,
cached, indexed, probabilistic, or otherwise accelerated structure. If it does,
the proposal must define exact reference semantics, canonical authority, a
deterministic human-readable audit projection, resource and completeness
bounds, failure behavior, and differential equivalence tests. If it does not,
state that ADR-0026 is not applicable.

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

Changes affecting core search behavior, execution mode, or performance
evidence must also run the bounded canonical benchmark:

```sh
cmake -S . -B build/benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DEVO_BENCHMARK_COMMIT=<commit>
cmake --build build/benchmark --target benchmark-smoke --parallel
```

Do not add timing thresholds to the smoke gate. Retain raw samples and derive
human-readable summaries from validated canonical evidence.

## Change Discipline

- Keep each commit to one logical, reviewable change.
- Add tests for behavior changes or document why a test is not applicable.
- Preserve deterministic behavior under a recorded random seed.
- Preserve human-readable architecture: no accelerated representation may
  become opaque authority, and probabilistic structures remain prechecks only.
- Do not weaken correctness, safety, or validation gates to improve fitness.
- Update benchmarks when performance claims change.
- Record every AES exception in the appropriate waiver or ADR.
- Open a pull request; do not write directly to `main`.

The pull-request template lists the evidence expected for review.
