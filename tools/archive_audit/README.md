# Archive Integrity Cycle Counting (external research consumer)

Status: experimental; no production recovery or source deletion authorized.

This module is deliberately outside the \`catalyst_evo\` C17 optimizer.
An email-archive retention/recovery system is a distinct consumer, not a
source optimizer. Reuse EVO's deterministic replay, evidence, and
human-readable architecture patterns without changing its current ABI, RNG,
or source-optimizer search semantics.

## C-first implementation decision

Production implementation: **C17**; small Python reference for mathematical
oracle tests, report generation and operational orchestration.

Hot path:
1. Stream original MBOX bytes; use a bounded parser and produce a canonical
   message/attachment inventory. No message bodies or secrets enter Git.
2. SHA-256 every archive, chunk, and selected message with a maintained crypto
   library (e.g., OpenSSL EVP). Keep chunk identities, offsets, and digests
   in a human-readable canonical manifest.
3. Generate 4+2 Reed-Solomon parity across fixed-size chunks using a reviewed,
   established erasure-coding implementation; require parity-byte tests,
   independent decoder validation and fault-injection evidence before use.
4. Generate reproducible unbiased audit samples from 256-bit OS entropy using
   a reviewed cryptographic primitive (HMAC-SHA-256; separate from EVO PCG).
   Prime-stride sentinels are supplemental deterministic coverage only.
5. Verify a frozen source census against archive inventory (including duplicate
   and identifier-collision handling). Sampling never substitutes for census.
6. Repair a corrupted chunk only after verifying the archive and remaining
   parity material; verify the recovered chunk's cryptographic digest.
   Never silently replace or delete original archives.
7. Produce domain-ordered JSON evidence and a derived human-readable report.

Benchmark parser throughput, SHA-256 throughput, parity encode/decode, disk
transfer and full-file recovery separately. At multi-gigabyte size,
throughput is dominated by I/O, hashing and coding; sample-index selection is
not a performance bottleneck. Avoid replacing a tested cryptographic library
with custom ECC or custom cryptography.

## Prototype currently checked into this branch

- \`cyclecount.py\`: standard-library Python reference for cryptographic
  reproducible selection and hypergeometric sample sizing.
- \`test_cyclecount.py\`: synthetic tests for exact finite-population
  threshold, sampling without replacement, replay, and prime sentinels.

It accepts JSONL index lines of:

    {"file":"part-001.mbox","offset":0,"length":512,"message_id":"<sample@example.invalid>"}

A unique canonical, immutable file index must be frozen before the audit;
the Python prototype hashes index bytes and samples **that index**. It does
not validate the source inventory or archive bytes, and must not be
represented as having verified a complete export.

Run from this directory:

    python3 -m unittest -v test_cyclecount
    python3 cyclecount.py --index /private/messages.jsonl --out /private/sample.json

The seed and message metadata in a plan are sensitive. Keep all real user
inventories, message identifiers, source archive paths, seeds, and sample plans
**out of the public EVO repository**. Commit synthetic fixtures only.
Reproducibility requires preserving a seed securely outside the repository.

## Statistical authority

For a population with N unique records, D defective records, and an
unbiased size-n sample without replacement:

    P(detect >= 1) = 1 - C(N-D, n) / C(N, n)

The default confidence and defect prevalence are 0.95 and 0.001.
Every planned sample records N, D, n, frozen index identity and replay seed.
A prime-index/stride pass is *not* counted toward the detection probability.

Stratified audits require a separate allocation and confidence calculation.
The formula for unstratified simple random sampling must not be presented
as the detection confidence for a weighted/stratified policy.

## Security and limitations

- EVO's PCG RNG and its prime/elliptic research variants are deterministic,
  non-cryptographic, and **not** approved for the unpredictable audit sampler.
  This follows EVO ADR-0003 and report EVO-RNG-001.
- The repository is public; no exported email, personal metadata, or passwords
  may be committed.
- A SHA-256 manifest detects changed bytes if held in a trusted location.
  A signature needs independently managed signing keys and public-key trust;
  signatures are a future feature, not part of this prototype.
- Erasure coding handles bounded damage *after* correct parity was generated.
  It cannot reconstruct a message missing from the original Google export.
- Recovery parity is not an independent backup, and parity/rebuild are **not
  implemented yet**.
- Archive deletion, Gmail removal, and mutation of exported archives are out
  of scope; all early workflows are read-only.

## Relevant existing EVO facilities

- \`docs/adr/ADR-0003-prime-and-elliptic-seed-schedules.md\`: empirical
  rejection of unhelpful prime/elliptic entropy derivation.
- \`docs/adr/ADR-0026-human-readable-abstraction-and-audit-projection.md\`:
  exact records remain authoritative; accelerators must remain explainable.
- \`docs/adr/ADR-0030-versioned-checkpoint-and-deterministic-resume.md\`:
  state schema, provenance, versioned resume, full logical projection.
- \`docs/adr/ADR-0033-reproducible-core-benchmark-evidence.md\`:
  evidence, conditions, benchmarks, replay.
- \`docs/engineering/SECURE-C-CXX.md\`: mandatory secure implementation gate.

## Next acceptance gates for a native implementation

1. Define a strict C17 \`archive_audit\` library API and versioned manifest
   format, with bounds, file identity and source-census policy.
2. Implement C sampling with OpenSSL-backed OS-entropy and HMAC (or a
   documented approved primitive); differential-test against Python.
3. Implement a streaming, offset-preserving MBOX parser with hostile-input
   fixtures and sanitizers/fuzzing.
4. Add chunk hashes and exact reconciliation. Verify on synthetic archives.
5. Add a reviewed Reed-Solomon backend with 0/1/2 erasure tests per stripe,
   detected-corruption tests, interrupted-rebuild and read-only recovery.
6. Document cross-platform replay, secure seed handling, attack/retention
   boundaries, performance results, CLI parity, and rollback.

Do not combine source-optimizer mutation with archive-recovery authority.
An EVO optimizer may later **benchmark alternative implementation strategies**
for this module, but its search output must still pass all archive correctness,
security and interoperability gates independently.
