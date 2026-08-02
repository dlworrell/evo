# Compiler-Option Search Core Example

This planned example exercises the generic `catalyst_evo` core by exploring
compiler pass selection and ordering, inlining thresholds, instruction
selection, register allocation, code-size versus speed tradeoffs, and memory-
bank placement.

Fitness may include execution cycles, binary size, bank crossings, build time,
and test correctness. Candidate configurations that fail tests are rejected.

## Boundary

This is a build-configuration adapter. It does not parse a C project, analyze
its AST or LLVM IR, evolve structured source transformations, or emit optimized
C source. Successful completion therefore proves only that the reusable core
can search compiler options; it does not satisfy the EVO 1.0 source-to-source
product contract in `docs/specs/EVO-002-source-optimizer-contract.md`.

The actual source-optimization reference proof is tracked by issues #58
through #69 and must emit at least one reviewable source-level change.
