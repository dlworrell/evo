# ADR-0030: Versioned Checkpoint and Deterministic Resume

Status: Accepted
Date: 2026-08-08
Decision owner: EVO

## Context

EVO 0.28.0 could replay a complete bounded run from its seed and configuration,
but it could not persist a committed generation and continue only the remaining
suffix. A correct checkpoint cannot contain merely the current genome bytes.
Continuation also depends on the evaluated population, stable global-winner
identity, operator and RNG versions, latest schema-4 statistics, effective
adaptive-mutation rate and stagnant count, and convergence-patience state.
Reconstructing any of those values from a partial or hidden history would make
resume a different algorithm.

Checkpoint input is untrusted serialized data. Every length, offset, count,
enum, floating-point field, owner relationship, and arithmetic operation must
be validated under an explicit byte budget before allocation, RNG use, or a
consumer callback. Decode failure must release every provisional owner and
leave the public result empty.

Function pointers, context addresses, allocator addresses, and backend-specific
object padding are neither portable nor meaningful replay identities. Callback
code and external state must be reattached by the consumer under explicit
stable semantic identities. A checksum can detect accidental corruption, but
it cannot authenticate an attacker-controlled checkpoint or encrypt genome
contents.

Finally, the canonical byte format is compact machine-readable persistence.
Even though it is not an accelerated decision structure, treating raw bytes as
opaque authority would violate ADR-0026. Every accepted checkpoint therefore
needs an ordered allocation-free projection in EVO domain terms.

## Decision

EVO 0.29.0 defines checkpoint format version 1, checkpoint/run-state view
version 1, and bounded-run policy version 10.

### Public capture and resume surface

`evo_problem_t` appends `checkpoint_problem_identity`. `evo_config_t` appends:

- `max_checkpoint_bytes`;
- caller-owned `checkpoint_buffer` and `checkpoint_buffer_size`;
- synchronous `checkpoint_observer` and its caller-owned context; and
- `checkpoint_context_identity`.

Checkpoint capture is disabled when the observer is null and the buffer fields
are zero. Enabled capture requires nonzero problem and context identities and a
buffer large enough for the exact value returned by `evo_checkpoint_size`.
The buffer is a runtime delivery resource: its pointer, capacity, observer
pointer, observer context, and maximum checkpoint budget are not serialized or
included in deterministic configuration equality.

The public operations are:

- `evo_checkpoint_size`, which performs checked layout arithmetic and reports
  the exact required byte count;
- `evo_checkpoint_inspect`, which validates untrusted bytes without allocation
  and returns the complete ordered `evo_checkpoint_view_t` projection;
- `evo_checkpoint_candidate_inspect`, which projects one explicit population
  member in constant time from a validated unchanged view; and
- `evo_resume`, which validates, reconstructs private owners through the local
  allocation/secure-erasure backend, and continues only an unfinished suffix.

An active result is rejected unchanged. Every other resume failure leaves an
empty result. A restored generation is already committed and is never sent
again to the stop, generation-observer, or checkpoint-observer callbacks.
Terminal resume reconstructs and returns the terminal result without invoking
any callback. Intermediate resume begins with the next transition.

### Commit and delivery order

For every successful generation, including generation zero, the order is:

1. commit population ownership, global winner, statistics, adaptive state, and
   stopping state;
2. classify the natural termination reason;
3. invoke the optional application stop decision when continuation remains;
4. invoke the generation observer with the final reason for that generation;
5. encode and validate the checkpoint in the caller-owned buffer; and
6. invoke the checkpoint observer with the same committed reason and audit
   projection.

Provisional or failed generations emit no checkpoint. A checkpoint observer
may copy the bytes during its call; it must not retain borrowed views. The
caller owns persistent storage and is responsible for its confidentiality,
erasure, authentication, and lifetime.

### Canonical wire format

Format 1 is canonical little-endian and contains one fixed header followed by
six explicitly offset and sized sections in this exact order:

1. deterministic configuration;
2. continuation and ownership state;
3. latest generation statistics;
4. explicit candidate-evaluation records;
5. the contiguous current-population genome slab; and
6. the independently owned global-best genome.

Native structure images and padding are never serialized. Every integer uses a
fixed-width unsigned encoding. Every `size_t` is encoded as an unsigned 64-bit
value and must fit the restoring platform before use. Doubles are encoded as
IEEE-754 binary64 bits; the implementation rejects compilation on an
incompatible `double` representation. Booleans accept only zero or one.
Candidate evaluation records contain seven explicit binary64 values followed
by validity and evaluated flags.

The configuration section contains every deterministic problem/configuration
scalar, resource budget that affects acceptance, callback-presence flags, and
the stable problem/context identities. It excludes all raw pointers and the
checkpoint-delivery resources listed above. Its FNV-1a-64 value is a quick
format fingerprint only. Resume canonically re-encodes the supplied problem
and configuration and compares every configuration byte; the hash never
authorizes a mismatch.

The header records CRC-32 over the complete checkpoint with the checksum field
treated as zero. CRC failure reports an integrity error. CRC-32 is accidental-
corruption detection only. EVO does not claim message authentication,
encryption, collision resistance, provenance, rollback prevention, or safe
transport. Consumers that cross a trust boundary must wrap checkpoint bytes in
an approved authenticated format outside EVO.

### Complete continuation authority

Private run-state schema 1 persists:

- current committed generation and final reason, if any;
- stable global-best generation and population index;
- RNG algorithm, operator seed-schedule, bounded-run, selection, byte-operator,
  comparison, diversity, adaptive, and stopping versions;
- the exact effective next mutation rate and adaptive stagnant count;
- the significant-best patience reference and stopping stagnant count;
- current population dimensions, provenance, validity/best evidence, elite and
  singleton evidence, diversity, and exact source owner counts;
- complete schema-4 generation statistics;
- every candidate's validity and fitness evidence; and
- current-population and global-best genome bytes.

No warm-up generation, observer log, wall clock, process identity, address, or
unrecorded entropy is consulted on restore. The same continuation loop accepts
fresh generation-zero state and fully validated restored state, so transition
generation numbers and domain-separated RNG streams remain identical.

### Untrusted-input and ownership validation

Inspection first enforces the caller byte budget, fixed header, supported
format/integrity versions, total size, ordered non-overlapping sections,
checked offset arithmetic, CRC, and configuration fingerprint. It then decodes
and validates configuration values, continuation bounds, finite fitness and
state values, termination consistency, evaluation-record flags, stable-best
ordering, valid/invalid counts, and fitness sums without allocating.

Resume next verifies the exact canonical configuration and reattached
identities before allocation. Only then does it allocate the current genome
slab, evaluation array, and global-best result through the same reviewed owner
allocators used by a fresh run. It decodes into provisional private objects,
re-runs completed-population and generation-statistics validation, reconciles
adaptive public/private state, checks current/global winner relationships, and
commits the three restored objects together. Any failure destroys every
provisional owner through the configured secure-erasure lifecycle.

Checkpoint secure-erasure backend metadata is audit evidence about the source
process. Restored allocations register the restoring build's local backend.
This permits replay between supported builds while preserving exact cleanup
claims. EVO never erases caller-owned input, output, or retained checkpoint
buffers; even secure-mode checkpoints are cleartext copies under caller policy.

## Human-Readable Abstraction Assessment

The format is a canonical persistence representation, not a cache, compressed
index, membership filter, probabilistic summary, or accelerated decision path.
Decoded population/state invariants—not the fingerprint or CRC—remain exact
authority.

Because raw binary persistence is not human-readable, format 1 nevertheless
provides a mandatory explainable projection. `evo_checkpoint_view_t` presents,
in stable logical order:

1. format, integrity, and exact configuration identity;
2. current generation and termination state;
3. population counts and stable current/global winners;
4. RNG, substream, operator, comparison, and diversity versions;
5. complete schema-4 statistics and adaptive/stopping state;
6. secure-erasure and source ownership counts; and
7. explicit population genome/evaluation ranges.

`evo_checkpoint_candidate_inspect` enumerates candidates in ascending
population order and exposes the exact genome, validity, evaluation flag, and
fitness record. It uses the transparent projected ranges and fixed record
stride; there is no hidden lookup table or secondary authority. A full audit
is linear in population size. Tests reconcile this projection with
uninterrupted results and the exact serialized records.

This satisfies ADR-0026 for the current direct format. A future compressed
checkpoint, compact index, deduplication table, delta chain, or cached decoder
must remain derived, project the same logical sequence, prove differential
equivalence, and fall back to or reject against format-1 reference semantics.

## Consequences

- Generation-zero, intermediate, natural-terminal, and application-terminal
  checkpoints have one uniform format and callback order.
- Resume reproduces the uninterrupted suffix without replaying committed
  callbacks or reconstructing hidden adaptive/stopping history.
- Checkpoint capture performs no EVO allocation; the caller supplies the exact
  bounded scratch range.
- Restore adds no allocation class beyond the existing population genome,
  evaluation, and result owners.
- Public structures and installed symbols change; consumers must rebuild
  against 0.29.0.
- Stable semantic identities are declarations by the consumer, not security
  credentials. The consumer must restore external callback state that
  corresponds to the checkpoint.
- Persistent checkpoint confidentiality, authenticated transport, rollback
  protection, atomic file replacement, and media erasure remain application
  responsibilities.

ADR-0031 advances the current EVO 0.30.0 format, checkpoint view, and canonical
configuration view to version 2 with magic `EVOCKPT2`. It persists and projects
the complete logical population-storage registry plus recycling configuration
and owner identity. Format 1 remains the historical decision defined here but
is rejected by the format-2 parser because it contains no registry or private
run-state schema-2 continuation. EVO-HRA-003 audits the amendment.

ADR-0032 advances the EVO 0.31.0 format and both top-level views to version 3
with magic `EVOCKPT3`. It binds evaluator thread-safety, worker count, library
scratch budget, schedule-observer presence, and committed parallel-policy
provenance while excluding live threads and provisional scheduler state.
Formats 1 and 2 are rejected by the format-3 parser rather than inventing those
fields. EVO-HRA-004 audits the bounded scheduler projection independently.

## Alternatives considered

### Serialize native C structures

Rejected because padding, enum width, `size_t`, endianness, pointers, and
backend-specific layout are not a portable or reviewable format.

### Store only seed and generation

Rejected because population, global winner, adaptive rate, patience reference,
and callback-visible evidence cannot be reconstructed without replaying hidden
history and side effects.

### Allocate an internal checkpoint buffer

Rejected because a caller-owned exact buffer gives an explicit resource bound,
adds no allocation/failure point to each commit, and makes transfer ownership
unambiguous.

### Treat CRC or the configuration hash as authority

Rejected because neither provides authentication and a collision must never
authorize malformed state or a different deterministic configuration.

### Serialize function or context pointers

Rejected because addresses are process-specific, nondeterministic semantic
identities and would expose implementation layout without proving equivalence.

## Verification

- `tests/checkpoint_test.c` proves generation-zero, intermediate, and terminal
  resume equivalence; callback suffix order; checkpoint projection and
  candidate enumeration; adaptive/patience continuation; corruption,
  truncation, version, budget, configuration, identity, and alias rejection.
  It independently checks the CRC and rejects impossible configuration,
  provenance, and termination fields even after the test recomputes both CRC
  and configuration fingerprint.
- `tests/checkpoint_fuzz_test.c` tests every truncation, a one-bit mutation at
  every byte, and 2,048 deterministic random inputs.
- `tests/fuzz/checkpoint_fuzz.c` provides a libFuzzer entry point for continuous
  untrusted-parser exploration.
- `tests/allocation_failure_test.c` fails every restore allocation and proves
  atomic cleanup with no callback.
- `tests/secure_erasure_test.c` proves restored owners use the local backend and
  erase exact ranges once, while caller checkpoint buffers are not EVO owners.
- CMake, GNU Autotools, and AES-BLD-001 enumerate the same twenty-four
  production sources and thirty normative tests.
- GitHub issue: `https://github.com/dlworrell/evo/issues/51`
