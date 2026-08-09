# ADR-0034: Installed Reference Consumer Adapters

Status: Accepted
Date: 2026-08-09
Decision owner: EVO

## Context

EVO 0.32.0 proves the deterministic core with unit, lifecycle, replay,
sanitizer, build-parity, and benchmark evidence. Those checks do not yet prove
that representative consumers can use only the installed public package to
model constraints, stop a run, resume a checkpoint, request bounded parallel
evaluation, and export reviewable evidence.

Examples can give false confidence if they include private headers, link an
in-tree target, replace an integration with a precomputed answer, or quietly
claim that tuning compiler flags is source-to-source optimization. They can
also violate ADR-0026 by retaining only a digest, summary, work-queue handle,
or other opaque shortcut instead of the records on which a decision depends.

## Decision

EVO 0.33.0 adds four bounded installed-consumer adapters:

1. repository scoring over four explicit reviewed records;
2. compiler-option configuration search over a small cost model;
3. scheduler-parameter tuning over six explicit jobs; and
4. FPGA placement exploration over an 8×8 integer resource model.

The release changes no public structure, function, status, algorithm policy,
RNG schedule, checkpoint format, or successful-run semantics. It updates the
package version and adds external programs, evidence tooling, documentation,
and staged-install verification.

### Installed boundary

Every adapter is one external C17 translation unit plus the shared
header-only consumer harness. It includes `<catalyst/evo/evo.h>` and standard
headers only. Neither standalone build adds the repository's `src/` directory,
uses a private symbol, nor links the in-tree CMake target.

The CMake adapter project discovers `catalyst-evo` from its installed
pkg-config metadata and links the platform thread target. The GNU Make adapter
build consumes `pkg-config --cflags` and `pkg-config --libs --static`. The
workflow first installs EVO through CMake or Autotools into independent fresh
prefixes, then configures, builds, runs, and validates the corresponding
external consumers.

### Common bounded search policy

Every fixture uses a four-byte genome, population 12, six possible child
transitions, one fixed seed, stable rank selection with integer base/step
weights 1/1, uniform-byte crossover at 0.75, nonzero byte-XOR mutation at
0.35, one explicit elite, built-in byte diversity, and population recycling.
The declared resource bounds are:

- 4 bytes for one genome;
- 48 bytes for each population genome slab;
- 4,096 bytes for private evaluation records;
- 264 byte comparisons for the all-valid diversity worst case;
- 16 KiB of caller-owned checkpoint storage when enabled; and
- the exact public worker-scratch query result when parallel evaluation is
  enabled.

Each domain defines at least one hard invalid combination and one explicit
soft penalty. The evaluator includes the penalty in `fitness.total`; EVO does
not infer or reapply it.

### Replay, resume, and scheduling

Each process executes its run twice with distinct zero-initialized result and
capture objects. It directly compares the owned best genome, complete fitness,
termination, final statistics, every generation record, and any checkpoint or
schedule projection. A mismatch fails before output.

Repository scoring copies the complete format-3 generation-two checkpoint
with a bounded byte loop, retains its CRC-32 only as non-authoritative
corruption evidence, and inspects all 12 candidate views in population order.
It resumes from those bytes under the same stable problem/context identities
and requires exact final-result equality plus equality with the uninterrupted
notification suffix. The serialized bytes and decoded candidates, not the
CRC, remain authority.

Scheduler tuning declares its evaluator thread-safe because it reads only the
immutable fixture and distinct read-only genomes. It requests three logical
workers and the exact reported scratch bound. Its observer copies every
candidate assignment after workers join. The output retains logical worker,
wave, disposition, commit presence, and commit order; native thread identity
and completion timing cannot affect or explain a result.

Compiler options uses patience stopping. FPGA placement uses an application
stop callback at generation three. These callbacks receive their own
caller-owned contexts and retain no borrowed view.

### Evidence authority and failure policy

Each executable writes one JSON object to standard output and owns no evidence
path. A bounded Python driver invokes exact argument vectors without a shell,
allows 15 seconds and 128 KiB per process, rejects stderr or a nonzero exit,
parses UTF-8 JSON, validates fields and stable ordering, and constructs
`catalyst.evo-reference-adapters.v1` in fixed adapter order.

The complete constructed object must equal
`examples/reference-adapters-v1.golden.json`. The 256-KiB golden/document
bound prevents unbounded input. Only after exact equality may the driver write
the canonical JSON artifact and derive the Markdown table. The Markdown is
convenience projection, never a second authority.

Any invalid argument, resource failure, callback projection error, EVO error,
replay difference, resume difference, output error, timeout, output overflow,
malformed evidence, schema drift, or golden mismatch exits unsuccessfully and
publishes no accepted artifact.

### Ownership and callback lifetime

Fixture models are immutable static records and outlive synchronous calls.
Evaluation retains no genome or context pointer. Trace, checkpoint, stop, and
schedule contexts are disjoint caller-owned records. Their borrowed public
views are copied only into fixed bounded arrays during the callback and are
not used afterward.

Every `evo_result_t` starts fully zero-initialized and is destroyed exactly
once on every exit path. Checkpoint delivery uses one fixed 16-KiB scratch
array and the retained generation-two snapshot uses a distinct fixed array.
The adapter never frees, reallocates, or casts away const from EVO-owned or
borrowed storage.

### Product boundary

The adapters prove the reusable `catalyst_evo` interface. They do not ingest a
C repository, analyze AST or LLVM IR, construct a structured transformation,
materialize or compile a source candidate, benchmark a patch, or publish a
downstream change. Compiler-option search is explicitly build-configuration
model search and neither reads nor emits C source. It cannot satisfy EVO-002,
issue #57, or the source-optimizer proof tracked by issues #58 through #69.

### Human-Readable Abstraction assessment

No adapter adds a cache, compact index, bitmap, compression layer, membership
filter, probabilistic decision, or accelerated authority. The immutable
fixtures and bounded arrays are their direct reference forms.

The complete stable adapter registry is the fixed ordered JSON array. Each
entry identifies its domain, fixture, exact configuration, capabilities,
authority/limitation, final result, complete trace, and replay outcome.
Repository scoring adds every checkpoint candidate; scheduler tuning adds every
candidate assignment. This satisfies the issue's expected audit view without
inventing an accelerator or a parallel opaque API. EVO-HRA-006 retains the
detailed reconciliation.

## Consequences

- Supported installations now have executable external examples for four
  representative domains.
- Checkpoint/resume and bounded parallel evaluation are exercised outside the
  private test boundary.
- Exact golden output makes deterministic drift intentional and reviewable.
- Fixtures remain small and do not require heavyweight compilers, simulators,
  repositories, FPGA tools, or network services in CI.
- Domain examples remain pedagogical models, not production fitness claims or
  substitutes for source-optimizer work.

## Alternatives considered

### Build examples as in-tree CMake targets

Rejected because a private include or target dependency could pass without
being present in the installed package.

### Retain only best genome and a digest

Rejected because replay, resume, constraint, schedule, and Human-Readable
Abstraction review would become opaque.

### Use real compiler, repository, scheduler, and FPGA toolchains in PR CI

Rejected for this core proof because uncontrolled versions, workloads,
machines, and external state would weaken determinism and resource bounds.
Product integrations may add optional heavyweight evidence under their own
contracts.

### Present compiler flags as source optimization

Rejected because configuration selection does not analyze or transform C and
would contradict EVO-002's product boundary.

## Verification

- `.github/workflows/reference-adapters.yml` independently stages CMake and
  Autotools installations and builds only against their pkg-config metadata.
- `examples/validate_reference_adapters.py` enforces bounded execution,
  structure, ordering, capability-specific evidence, and exact golden equality.
- The repository-scoring record proves generation-two checkpoint inspection
  and exact resume.
- The scheduler record proves complete three-worker logical schedules for all
  committed generations.
- All four records prove second-run replay and explicit constraints, penalties,
  stopping, limitations, resource bounds, and source-optimizer non-claims.
- EVO-HRA-006 audits the stable registry, direct reference forms, complete
  projections, and absence of accelerated or probabilistic authority.
