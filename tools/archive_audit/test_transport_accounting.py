#!/usr/bin/env python3
"""Synthetic ledger invariants, missing chunks and retries (no network used)."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

import transport_accounting as ledger

A = b"A" * 4096
B = b"B" * 200
SHA = lambda b: hashlib.sha256(b).hexdigest()


class TransportAccountingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.manifest = root / "manifest.jsonl"
        self.events = root / "events.jsonl"
        self.chunks = [A, B]
        self.manifest.write_text("\n".join(json.dumps(v) for v in [
            {"schema": "evo.archive-audit.stream-sha256.v1", "chunk_bytes": 4096},
            {"kind": "chunk", "offset": 0, "length": len(A), "sha256": SHA(A)},
            {"kind": "chunk", "offset": len(A), "length": len(B), "sha256": SHA(B)},
            {"kind": "file", "length": len(A+B), "chunks": 2,
             "sha256": SHA(A+B), "complete": True},
        ]) + "\n")

    def attempt(self, index, sequence, status, size=None, digest=None):
        if size is None:
            size = len(self.chunks[index])
        return {"type": "attempt", "chunk": index, "sequence": sequence,
                "status": status, "bytes_received": size, "sha256": digest}

    def events_write(self, records):
        self.events.write_text("".join(json.dumps(record) + "\n" for record in records))

    def test_success_with_retransfers(self):
        self.events_write([
            self.attempt(0, 1, "short_read", size=100),
            self.attempt(0, 2, "digest_mismatch", digest=SHA(b"X" * 4096)),
            self.attempt(0, 3, "verified", digest=SHA(A)),
            self.attempt(1, 1, "verified", digest=SHA(B)),
        ])
        result = ledger.summarize(self.manifest, self.events)
        self.assertTrue(result["complete"])
        self.assertEqual(result["verified_chunks"], 2)
        self.assertEqual(result["application_retries"], 2)
        self.assertEqual(result["digest_mismatches"], 1)
        self.assertEqual(result["short_reads"], 1)
        self.assertEqual(result["network_tcp_retransmits"], None)
        self.assertEqual(result["application_bytes_received"], 100 + 4096 + 4096 + 200)

    def test_missing_chunk_does_not_qualify(self):
        self.events_write([self.attempt(0, 1, "verified", digest=SHA(A))])
        result = ledger.summarize(self.manifest, self.events)
        self.assertFalse(result["complete"])
        self.assertEqual(result["unverified_chunks"], 1)

    def test_invalid_claims_fail_closed(self):
        invalid = [
            self.attempt(0, 1, "verified", digest=SHA(B)),
            self.attempt(0, 1, "digest_mismatch", digest=SHA(A)),
            self.attempt(0, 1, "short_read", size=4096),
            self.attempt(0, 1, "transport_error", digest=SHA(A)),
            self.attempt(0, 2, "transport_error", size=0),
            {**self.attempt(0, 1, "verified", digest=SHA(A)), "chunk": -1},
            {**self.attempt(0, 1, "verified", digest=SHA(A)), "bytes_received": True},
            {**self.attempt(0, 1, "verified", digest=SHA(A)), "status": "corrected"},
        ]
        for attempt in invalid:
            with self.subTest(attempt=attempt):
                self.events_write([attempt])
                with self.assertRaises(ValueError):
                    ledger.summarize(self.manifest, self.events)

    def test_no_unaccounted_repeat_after_success(self):
        self.events_write([
            self.attempt(0, 1, "verified", digest=SHA(A)),
            self.attempt(0, 2, "verified", digest=SHA(A)),
        ])
        with self.assertRaises(ValueError):
            ledger.summarize(self.manifest, self.events)

    def test_missing_final_marker_or_offsets_fail_closed(self):
        valid = self.manifest.read_text()
        self.events_write([])
        for changed in [valid.rsplit("\n", 2)[0] + "\n",
                        valid.replace('"offset": 4096', '"offset": 4095'),
                        valid.replace('"chunks": 2', '"chunks": 3')]:
            with self.subTest(changed=changed[:100]):
                self.manifest.write_text(changed)
                with self.assertRaises(ValueError):
                    ledger.summarize(self.manifest, self.events)
        self.manifest.write_text(valid)

    def test_empty_archive(self):
        self.manifest.write_text("\n".join(json.dumps(v) for v in [
            {"schema":"evo.archive-audit.stream-sha256.v1","chunk_bytes":4096},
            {"kind":"file","length":0,"chunks":0,"sha256":SHA(b""),"complete":True}
        ]) + "\n")
        self.events_write([])
        report = ledger.summarize(self.manifest,self.events)
        self.assertTrue(report["complete"])
        self.assertEqual(report["expected_bytes"],0)


if __name__ == "__main__":
    unittest.main()
