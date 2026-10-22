#!/usr/bin/env python3
"""Validate application-level transfer attempts against immutable chunk hashes.

Accounting describes OBSERVED application-level reads/retries only. TCP/TLS
retransmits and NIC FEC corrections are not observable through this event feed;
record those separately from trustworthy transport telemetry if available.
No network operations, archive writes, repair or deletion.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

HEX64 = re.compile(r"^[0-9a-f]{64}$")
STATUSES = frozenset(("verified", "short_read", "digest_mismatch", "transport_error"))


def parse_json_line(line):
    try:
        result = json.loads(line)
    except (ValueError, UnicodeDecodeError) as exc:
        raise ValueError("malformed JSONL") from exc
    if not isinstance(result, dict):
        raise ValueError("JSONL record must be an object")
    return result


def read_manifest(path):
    """Strictly validate the complete contiguous stream_hash.c manifest."""
    raw = Path(path).read_bytes()
    try:
        lines = raw.decode("utf-8").splitlines()
    except UnicodeDecodeError as exc:
        raise ValueError("manifest encoding") from exc
    if len(lines) < 2:
        raise ValueError("manifest truncated")
    rows = [parse_json_line(line) for line in lines]
    header, footer = rows[0], rows[-1]
    if (header.get("schema") != "evo.archive-audit.stream-sha256.v1" or
            type(header.get("chunk_bytes")) is not int or
            not 4096 <= header["chunk_bytes"] <= 64 * 1024 * 1024):
        raise ValueError("unknown or invalid manifest header")
    if (footer.get("kind") != "file" or
            footer.get("complete") is not True or
            type(footer.get("length")) is not int or
            type(footer.get("chunks")) is not int or
            footer["length"] < 0 or footer["chunks"] < 0 or
            not HEX64.fullmatch(str(footer.get("sha256", "")))):
        raise ValueError("missing complete-file marker")
    records = rows[1:-1]
    if len(records) != footer["chunks"]:
        raise ValueError("manifest chunk count mismatch")
    offset = 0
    for i, chunk in enumerate(records):
        if (chunk.get("kind") != "chunk" or
                type(chunk.get("offset")) is not int or
                type(chunk.get("length")) is not int or
                chunk["offset"] != offset or
                not 1 <= chunk["length"] <= header["chunk_bytes"] or
                not HEX64.fullmatch(str(chunk.get("sha256", "")))):
            raise ValueError("invalid/noncontiguous chunk at %d" % i)
        if i < len(records) - 1 and chunk["length"] != header["chunk_bytes"]:
            raise ValueError("short nonterminal chunk")
        offset += chunk["length"]
        if offset > 2**64 - 1:
            raise ValueError("manifest length overflow")
    if offset != footer["length"]:
        raise ValueError("manifest file length mismatch")
    return rows, hashlib.sha256(raw).hexdigest()


def summarize(manifest_path, event_path):
    manifest, manifest_hash = read_manifest(manifest_path)
    chunks = manifest[1:-1]
    seen = {i: 0 for i in range(len(chunks))}
    complete = set()
    count = 0
    bytes_received = 0
    retries = 0
    mismatches = 0
    short_reads = 0
    transport_errors = 0

    with Path(event_path).open("r", encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                raise ValueError("blank event line")
            event = parse_json_line(line)
            if event.get("type") != "attempt":
                raise ValueError("unknown event type")
            ordinal = event.get("chunk")
            sequence = event.get("sequence")
            size = event.get("bytes_received")
            status = event.get("status")
            digest = event.get("sha256")
            if (type(ordinal) is not int or ordinal not in seen or
                    type(sequence) is not int or sequence != seen[ordinal] + 1 or
                    type(size) is not int or not 0 <= size <= chunks[ordinal]["length"] or
                    status not in STATUSES or ordinal in complete):
                raise ValueError("invalid, duplicate, or late transfer attempt")
            expected = chunks[ordinal]
            if status == "verified":
                if size != expected["length"] or digest != expected["sha256"]:
                    raise ValueError("unsubstantiated verified claim")
                complete.add(ordinal)
            elif status == "digest_mismatch":
                if size != expected["length"] or not isinstance(digest, str) or \
                        not HEX64.fullmatch(digest) or digest == expected["sha256"]:
                    raise ValueError("invalid digest mismatch evidence")
                mismatches += 1
            elif status == "short_read":
                if size >= expected["length"] or digest is not None:
                    raise ValueError("invalid short read")
                short_reads += 1
            elif status == "transport_error":
                if digest is not None:
                    raise ValueError("transport error must not claim digest")
                transport_errors += 1
            if seen[ordinal] > 0:
                retries += 1
            seen[ordinal] = sequence
            bytes_received += size
            count += 1
    expected_bytes = manifest[-1]["length"]
    verified_bytes = sum(chunks[i]["length"] for i in complete)
    return {
        "schema": "evo.archive-audit.transport-accounting.v1",
        "manifest_sha256": manifest_hash,
        "source_sha256_claimed_by_manifest": manifest[-1]["sha256"],
        "expected_chunks": len(chunks),
        "verified_chunks": len(complete),
        "unverified_chunks": len(chunks) - len(complete),
        "expected_bytes": expected_bytes,
        "verified_bytes": verified_bytes,
        "application_bytes_received": bytes_received,
        "application_attempts": count,
        "application_retries": retries,
        "digest_mismatches": mismatches,
        "short_reads": short_reads,
        "transport_errors": transport_errors,
        "network_tcp_retransmits": None,
        "network_fec_corrected_symbols": None,
        "complete": len(chunks) == len(complete),
        "limitations": [
            "Only application attempts reported by the caller are counted.",
            "An event claiming a digest is not proof that the bytes were hashed independently.",
            "No network-layer TCP retry or NIC FEC counters are inferred.",
            "This does not verify the source message census or enable deletion.",
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--events", required=True)
    parser.add_argument("--report", required=True)
    args = parser.parse_args()
    report = summarize(args.manifest, args.events)
    destination = Path(args.report)
    # No automatic writes to the source archive; caller should store reports
    # privately and retain failed/incomplete session evidence.
    destination.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n")
    print("complete=%s verified=%d/%d application_retries=%d" % (
        report["complete"], report["verified_chunks"],
        report["expected_chunks"], report["application_retries"]))


if __name__ == "__main__":
    main()
