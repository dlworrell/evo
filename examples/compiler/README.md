# Compiler-Option Search Reference Adapter

`compiler_options.c` searches a small deterministic model containing
optimization level, unroll level, inline level, and vectorization enablement.
Two unsupported combinations are hard invalid. Modeled size growth above 180
units is a visible soft penalty already included in `fitness.total`. A bounded
stagnation policy terminates the run when improvement stops.

## Boundary

This is build-configuration search only. It does not invoke a compiler, parse
a C project, analyze AST or LLVM IR, evolve a pass pipeline, transform source,
measure a binary, or emit optimized C source. Successful completion proves
only that the reusable core can search the explicit fixture model; it does not
satisfy the EVO 1.0 source-to-source product contract in
`docs/specs/EVO-002-source-optimizer-contract.md`.

The actual source-optimization reference proof is tracked by issues #58
through #69 and must emit at least one reviewable source-level change.
