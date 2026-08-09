# EVO Reference Consumer Adapters

These four bounded programs are external consumers of the installed
`catalyst_evo` C17 API. They include only `<catalyst/evo/evo.h>` and standard
headers; they do not use `src/`, private types, or an in-tree link shortcut.

| Adapter | Explicit fixture | Contract exercised |
|---|---|---|
| Repository scoring | Four reviewed repository records | Hard validity, soft penalty, checkpoint inspection, exact resume |
| Compiler options | Small build-option cost model | Hard option compatibility, size penalty, stagnation stopping |
| Scheduler tuning | Six immutable queued jobs | Declared thread-safe evaluation, three bounded workers, complete schedules |
| FPGA placement | 8×8 integer resource model | Placement validity, timing/resource penalties, application stopping |

Every program executes its fixed seed and configuration twice and rejects any
difference in the owned result, full generation trace, checkpoint bytes, or
parallel schedule. Repository scoring resumes the format-3 generation-two
checkpoint and compares the final result and uninterrupted trace suffix.
Scheduler tuning emits every candidate's stable logical worker, dispatch wave,
final disposition, and candidate-ordered commit evidence.

The programs write one bounded JSON object to standard output. The validator
runs them without a shell, enforces a 15-second timeout and 128-KiB output
limit per process, validates the stable fields and ordering, and compares the
complete combined object to `reference-adapters-v1.golden.json`. Only after
that exact comparison may it write `EVO-ADAPTERS-001.json` and its derived
Markdown projection.

## Build from an installed package

After installing EVO into a staging prefix, point `PKG_CONFIG_PATH` at that
installation. The CMake and GNU Make entry points are independent external
consumer builds:

```sh
export PKG_CONFIG_PATH=/staged/evo/lib/pkgconfig
cmake -S examples -B build/reference-adapters
cmake --build build/reference-adapters
python3 examples/validate_reference_adapters.py \
  --repository build/reference-adapters/evo_repository_scoring_adapter \
  --compiler build/reference-adapters/evo_compiler_options_adapter \
  --scheduler build/reference-adapters/evo_scheduler_tuning_adapter \
  --fpga build/reference-adapters/evo_fpga_placement_adapter

make -C examples validate BUILD_DIR="$PWD/build/reference-adapters-make"
```

The `Reference Adapters` workflow performs those builds against independent
CMake and Autotools staged installations and retains both evidence artifacts.

## Ownership, callbacks, and failure

- Fixture and evaluator contexts are immutable static records and outlive each
  synchronous run. Evaluators retain no genome pointer.
- Only scheduler evaluation opts into concurrency; its evaluator reads the
  immutable fixture and writes no shared state. All trace, checkpoint, and
  schedule observers remain synchronous and use their distinct caller-owned
  contexts.
- `evo_result_t` is zero-initialized and destroyed exactly once on every path.
  Checkpoint delivery and snapshots use fixed 16-KiB caller-owned arrays.
- Any EVO error, callback projection error, replay mismatch, resume mismatch,
  output error, malformed JSON, timeout, output overflow, schema drift, or
  golden mismatch makes the adapter or validator fail without publishing
  evidence.

## Product and abstraction boundary

These are reference adapters for the reusable core, not EVO's source optimizer.
They do not ingest or mutate a repository, analyze AST or LLVM IR, build an
evolved source candidate, measure a patch, or publish anything downstream.
Compiler-option search is explicitly configuration search and emits no C
source. The source-optimizer contract begins at issue #58.

The fixtures, configurations, candidate projections, traces, and schedules are
direct explicit records. No cache, compressed index, membership filter,
probabilistic decision, or other accelerator is introduced. The JSON is
complete authority; the Markdown table is a derived human-readable view.
