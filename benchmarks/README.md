# EVO Benchmarks

EVO 0.32.0 provides `EVO-CORE-001`, a reproducible byte-genome core benchmark
with versioned canonical JSON and a Markdown projection generated from that
validated JSON.

The benchmark compares serial/parallel evaluation and explicit/recycled
population storage under identical seeds and search policy. Fixed small
oracles and exact cross-mode result equality are correctness gates. Wall time,
CPU time, requested-memory models, and process RSS are measurements only; no
timing or cross-machine threshold can make a run pass or fail.

## CMake

Use an immutable 40-hex commit instead of the placeholder:

```sh
cmake -S . -B build/benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DEVO_BENCHMARK_COMMIT=<commit>
cmake --build build/benchmark --target benchmark-smoke --parallel
```

The intentionally larger local tier is:

```sh
cmake --build build/benchmark --target benchmark-extended --parallel
```

## GNU Autotools

```sh
autoreconf -fvi
mkdir -p build/autotools-benchmark
cd build/autotools-benchmark
CC=gcc ../../configure
make
EVO_BENCHMARK_COMMIT=<commit> make benchmark-smoke
```

Replace the final target with `benchmark-extended` for the larger tier.

Both frontends write these files under their build tree's
`benchmark-results/` directory:

- `EVO-CORE-001-smoke.json` or `EVO-CORE-001-extended.json` — canonical
  ordered evidence;
- the matching `.md` — human-readable summary parsed from the JSON.

The schema is `benchmarks/evo-core-benchmark-v1.schema.json`. The validator and
projection generator is `benchmarks/validate_core_benchmark.py`. Pull requests
and `main` run only the bounded smoke tier; the GitHub workflow exposes the
extended tier through manual dispatch. The driver uses an argument vector with
no shell, applies tier-specific execution timeouts, and rejects canonical input
or output larger than 2 MiB.

See `docs/benchmarks.md`, ADR-0033, and EVO-HRA-005 for the evidence,
tolerance, memory-model, and Human-Readable Abstraction contracts.
