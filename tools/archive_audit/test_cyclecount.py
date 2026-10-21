#!/usr/bin/env python3
"""Deterministic, synthetic-only tests for the archive cycle-count sampler."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

import cyclecount as c


class CycleCountTests(unittest.TestCase):
    def setUp(self):
        self.seed = bytes(32)
        self.digest = hashlib.sha256(b"synthetic index").hexdigest()

    def test_lemire_unbiased_range_and_replay(self):
        a = c.HMACStream(self.seed, self.digest, "uniform")
        b = c.HMACStream(self.seed, self.digest, "uniform")
        values = [a.randbelow(7) for _ in range(500)]
        self.assertEqual(values, [b.randbelow(7) for _ in range(500)])
        self.assertTrue(all(0 <= v < 7 for v in values))
        self.assertEqual(set(values), set(range(7)))
        with self.assertRaises(ValueError):
            a.randbelow(0)

    def test_uniform_sample_without_replacement(self):
        for population in (1, 2, 7, 99, 510):
            for n in (0, population // 2, population):
                selected = c.sample_without_replacement(
                    population, n, c.HMACStream(self.seed, self.digest, "sample"))
                self.assertEqual(len(selected), n)
                self.assertTrue(all(0 <= i < population for i in selected))

    def test_exact_hypergeometric_threshold(self):
        n, bad = c.required_sample(510, 0.95, 0.001)
        self.assertEqual((n, bad), (485, 1))
        a = -__import__("math").expm1(c.miss_log_probability(510, bad, n))
        b = -__import__("math").expm1(c.miss_log_probability(510, bad, n - 1))
        self.assertGreaterEqual(a, 0.95)
        self.assertLess(b, 0.95)
        n, bad = c.required_sample(1_000_000, 0.95, 0.001)
        self.assertGreaterEqual(-__import__("math").expm1(
            c.miss_log_probability(1_000_000, bad, n)), 0.95)

    def test_prime_walk_does_not_overlap_random_sample(self):
        for population in (1, 2, 5, 31, 510):
            rng = c.HMACStream(self.seed, self.digest, "sentinel")
            excluded = set(range(population // 3))
            picks, meta = c.prime_sentinels(population, 15, rng, excluded)
            self.assertFalse(picks.intersection(excluded))
            self.assertEqual(len(picks), min(15, population - len(excluded)))
            self.assertEqual(__import__("math").gcd(meta["stride"], population), 1)

    def test_inventory_commit_and_deterministic_plan(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "synthetic.jsonl"
            records = [
                {"file": "synthetic.mbox", "offset": i * 256, "length": 256,
                 "message_id": "<synthetic-%d@example.invalid>" % i}
                for i in range(100)
            ]
            path.write_text("".join(json.dumps(r) + "\n" for r in records))
            p1 = c.generate_plan([path], self.seed, sentinels=10)
            p2 = c.generate_plan([path], self.seed, sentinels=10)
            self.assertEqual(c.canonical(p1), c.canonical(p2))
            self.assertEqual(p1["population"], 100)
            self.assertEqual(len(p1["sample"]), 100)  # small corpus, 95% detect 1 error
            self.assertEqual(p1["prime_sentinels"], 5)
            path.write_text("".join(json.dumps(r) + "\n" for r in records[:-1]))
            p3 = c.generate_plan([path], self.seed, sentinels=10)
            self.assertNotEqual(p1["index_digest_sha256"], p3["index_digest_sha256"])

    def test_rejects_invalid_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "synthetic.jsonl"
            path.write_text(json.dumps({"file": "../unsafe.mbox",
                                        "offset": 0, "length": 10}) + "\n")
            with self.assertRaises(ValueError):
                c.generate_plan([path], self.seed)


if __name__ == "__main__":
    unittest.main()
