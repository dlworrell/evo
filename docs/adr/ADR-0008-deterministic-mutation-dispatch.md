# ADR-0008: Deterministic Mutation Dispatch

Status: Accepted (extended by ADR-0027)
Date: 2026-07-31
Decision owner: EVO

## Context

EVO 0.8.0 provides deterministic private tournament selection and crossover
dispatch, but a produced child still needs a representation-aware mutation
boundary. The public problem definition already contains a consumer mutation
callback and one floating-point mutation rate. Hard-coding byte-level bit
flips, numeric perturbations, or permutation operations into the first
boundary would impose genetic meaning on opaque genome bytes.

Mutation gating must remain reproducible and composable with crossover. The
engine must define endpoint behavior, callback absence, and exact random-stream
consumption before generation orchestration can sequence the operators.

## Decision

EVO 0.9.0 adds a private fixed-rate mutation dispatcher over RNG algorithm
version 1.

The mutation operator:

- accepts one complete bounded writable genome view;
- requires nonzero `genome_size` within `max_genome_bytes`;
- requires a finite `mutation_rate` in `[0, 1]` and a seeded RNG;
- rejects null pointers and invalid policy before consuming RNG or modifying
  the genome;
- consumes exactly one RNG word for every valid attempt, including rates zero
  and one and a missing callback;
- invokes the consumer mutation callback exactly once when the event is
  selected and the callback exists; and
- otherwise leaves the genome unchanged.

The existing public callback shape gives `mutation_rate` two explicit roles.
EVO owns its interpretation as the per-genome dispatch probability. When the
event is selected, EVO forwards the same value unchanged as the consumer's
representation-specific mutation intensity. A consumer must not add an
unrecorded stochastic gate. Its callback must be deterministic for fixed
genome bytes, rate, and context, mutate only the supplied span, preserve
ownership, use no unrecorded entropy, and retain no view.

The callback mutates in place and returns no status. EVO therefore cannot roll
back a callback contract violation. The operator is private, allocation-free,
and not called by `evo_run`.

ADR-0027 extends this boundary in EVO 0.26.0. The zero-valued consumer mode
preserves the callback/no-op behavior and exact RNG schedule above. Explicit
byte-XOR mode adds two bounded draws after a selected one-word gate, changes
exactly one byte with a nonzero mask, and bypasses the callback.

ADR-0028 implements adaptive scheduling in EVO 0.27.0 without changing this
dispatcher. Bounded orchestration supplies one committed effective rate, and
this operation uses and forwards that scalar unchanged.

## Consequences

- Mutation decisions are fixed-vector testable and replay stable.
- Every valid consumer-mode genome attempt advances the supplied stream by one
  word, independent of endpoint rate or callback presence. ADR-0027 defines
  additional selected built-in consumption.
- Consumer-mode representation semantics remain consumer-owned; byte semantics
  require an explicit ADR-0027 selector.
- A missing callback is a deterministic no-op.
- Precondition failures preserve genome bytes and RNG state.
- Public layouts, installed symbols, RNG algorithm version 1, and `evo_run`
  behavior remain unchanged.
- Other typed mutation helpers, child-population ownership, operator
  sequencing, and the first generation transition remain separate milestones.

## Alternatives considered

### Add generic byte mutation

Rejected for the first boundary because byte positions and bit flips need not
preserve structured, numeric, or permutation genome validity. ADR-0027 later
adds an explicit opt-in byte policy without inferring representation.

### Let each consumer decide whether mutation occurs

Rejected because the configured event rate and stream-consumption evidence
would no longer be owned or reproducible by EVO.

### Invoke the callback on every genome

Rejected because the engine would cease to own the configured per-genome
mutation probability. The callback still receives the rate as intensity after
the engine selects the event.

### Skip RNG consumption at rates zero and one

Rejected because genome attempts would consume a configuration-dependent
number of stream words. Fixed one-word consumption is easier to reproduce and
compose.

### Copy the genome for rollback

Rejected because the callback has no failure result and scratch storage would
introduce another allocation and policy budget before child-population
ownership is specified.

### Integrate mutation into `evo_run`

Rejected because child-population ownership, operator stream sequencing,
evaluation of the new population, termination, and generation counting are
not yet specified.

## Verification

- `tests/mutation_test.c` covers pointer and policy rejection, unseeded state,
  endpoint and missing-callback consumption, fixed gate decisions, callback
  evidence, genome preservation, and deterministic replay.
- Existing RNG tests lock probability thresholds and fixed vectors.
- The mutation test is normative in CMake, GNU Autotools, and the AES-BLD-001
  repository profile.
- GitHub issue: `https://github.com/dlworrell/evo/issues/22`
