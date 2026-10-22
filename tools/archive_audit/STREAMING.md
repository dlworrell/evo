# One-pass chunking, hashing and index acceleration

Status: read-only research prototype; no archive data or secrets in this public repository.

## Decision

Stream input **once** through bounded buffers. Compute file-level and chunk-level
cryptographic digests on the same bytes, while later implementation stages can
consume the same stream to create Reed-Solomon parity and a separately tested
message index. Avoid rereading entire large archives for each operation.

Reference pipeline:

    immutable source stream
         |
         +--> bounded reader [4 MiB default; configurable]
                |
                +--> update whole-file SHA-256
                +--> finalize each chunk SHA-256
                +--> record {ordinal, offset, length, digest}
                +--> future parity-stripe encoder (4 data + 2 recovery)
                +--> future independently audited MIME/message index
         |
         +--> final manifest and source census reconciliation

Chunk boundaries are byte boundaries, **not necessarily message or MIME
boundaries**. The message parser must tolerate one message crossing any number
of chunks and keep an exact byte offset/length index.

A fast non-cryptographic key hash may accelerate finding a candidate record,
but **never** determines that a message, file, or chunk is intact. Every
candidate hit must be confirmed against its full immutable key and, where
needed, cryptographic SHA-256 digest.

## Existing Atarix work

Useful for a **separate index/recovery accelerator**, not the file digest:

- [ATX-SPEC-020](https://github.com/dlworrell/atarix/blob/main/docs/architecture/ATX-SPEC-020-Hash-Table-and-Lookup-Acceleration-Model.md)
  defines open-addressed hash tables, compressed ordered indexes, bitmap and
  membership filter prechecks, and the principle that acceleration is not
  authority.
- [Krapivin open-addressing research note](https://github.com/dlworrell/atarix/blob/main/docs/research/hashing/krapivin-2025-open-addressing-summary.md)
  examines fast lookup without rearranging entries; it does not propose a
  collision-resistant content hash.
- [16-lane control-byte probe](https://github.com/dlworrell/atarix/blob/main/rtl/atarix/atx_simd_probe_core.v)
  compares 7-bit candidate fingerprints. A fingerprint hit is **not** proof
  of key equality; an independent exact-key check is mandatory.
- [Krapivin-style probe stepper](https://github.com/dlworrell/atarix/blob/main/rtl/atarix/atx_krapivin_stepper.v)
  is an RTL research scaffold for address probing, not a cryptographic
  digest or established production software hash-map implementation.

Reuse the **architecture and test principles** first. Do not transplant
the FPGA/RTL logic or a 7-bit fingerprint into an archival integrity check
without a separately defined portable, collision-safe index and benchmarks.

The authoritative archive index must be reconstructable from manifest and
archive bytes, with a sorted/paginated human-readable projection. Source
census, SHA digests and verified bytes—not lookup tables—are authoritative.

## Checked-in native proof of concept

    cc -std=c17 -O2 -Wall -Wextra -Wpedantic -Werror \
       tools/archive_audit/stream_hash.c \
       -o /tmp/stream_hash $(pkg-config --cflags --libs openssl)

    /tmp/stream_hash /private/input.mbox 4194304 > /private/manifest.jsonl
    python3 -m unittest discover -s tools/archive_audit -p 'test_stream_hash.py' -v

- Reads a file or standard input (\`-\`) with bounded allocations.
- Uses OpenSSL EVP SHA-256 for a file digest and individual chunk digests.
- Emits JSONL chunk {offset, length, sha256} entries followed by an explicit
  final complete marker and file digest; a reader **must reject** a missing
  final marker, interrupted writes, duplicate/overlapping offsets, length
  mismatches, or malformed JSON.
- A failed run can leave partial stdout: the caller must write a temporary
  artifact and atomically publish it only after success and validation.
- No compression, deduplication, parity generation, messaging parser,
  human-readable index projection, or actual recovery is implemented by
  this proof of concept.
- The file's SHA-256 is *not* its Merkle root. A Merkle tree can optionally be
  built from domain-separated chunk digests in a later independently tested
  format.
- Do not commit private manifest output, the input MBOX, message identifiers,
  secret random seeds, or filesystem paths.

## Next implementation gates

1. Benchmark sequential cold/warm reads, hashing, JSONL emission, and data
   transfer separately, including an optimized BLAKE3 **comparison**, not an
   unjustified replacement of archive SHA-256.
2. Use an independent source inventory to catch omissions, not just hashes
   of the files Google actually exported.
3. Build a streaming MBOX parser with hostile-input corpus and exact offsets.
4. Add an authoritative sorted index and an optional Atarix-inspired fast
   lookup path. For each candidate, verify the entire key and required SHA.
5. Implement a tested erasure-coding stripe builder/reader (4+2 RS), preserving
   padding and original lengths; test missing/corrupt chunks, interruption
   and atomic recovery.
6. Version every artifact; tie content hashes, source revision identity,
   parity layout, sampling seed commitment, and audit exceptions together
   in a separately held signed manifest.

Read-only verification remains the default. Neither this module nor an
accelerated index authorizes deleting original emails or source archives.
