#!/usr/bin/env python3
"""External archive-audit research consumer for EVO (standard library only).

HMAC-SHA-256 provides a domain-separated, reproducible stream seeded from OS
entropy; Lemire rejection gives unbiased indices; Floyd selection samples
without replacement. Prime-stride sentinels are separate, NON-statistical
coverage checks. No file contents are modified, repaired, or deleted.
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import math
from pathlib import Path
import secrets

DOMAIN = b"evo.archive-audit.sample.v1\x00"
MASK64 = (1 << 64) - 1


def canonical(obj):
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


class HMACStream:
    """Deterministic PRF stream; not an independently validated NIST DRBG."""

    def __init__(self, seed: bytes, population_digest: str, purpose: str):
        if len(seed) != 32:
            raise ValueError("seed must have 32 bytes")
        context = DOMAIN + bytes.fromhex(population_digest) + b"\x00" + purpose.encode("ascii")
        self.key = hmac.digest(seed, context, "sha256")
        self.counter = 0
        self.buffer = b""

    def u64(self):
        if len(self.buffer) < 8:
            self.buffer += hmac.digest(self.key, self.counter.to_bytes(16, "big"), "sha256")
            self.counter += 1
        value = int.from_bytes(self.buffer[:8], "big")
        self.buffer = self.buffer[8:]
        return value

    def randbelow(self, bound: int):
        if not 1 <= bound <= MASK64:
            raise ValueError("bound outside 1..2^64-1")
        threshold = ((1 << 64) - bound) % bound
        while True:
            product = self.u64() * bound
            if (product & MASK64) >= threshold:
                return product >> 64


def sample_without_replacement(population: int, n: int, rng: HMACStream):
    """Floyd sampling: uniform n-subset of [0, population), O(n) memory."""
    if not 0 <= n <= population:
        raise ValueError("invalid sample size")
    selected = set()
    for j in range(population - n, population):
        v = rng.randbelow(j + 1)
        selected.add(j if v in selected else v)
    assert len(selected) == n
    return selected


def miss_log_probability(population: int, defective: int, n: int):
    if not 0 <= defective <= population or not 0 <= n <= population:
        raise ValueError("invalid population parameters")
    if n > population - defective:
        return -math.inf
    return sum(math.log1p(-defective / (population - i)) for i in range(n))


def required_sample(population: int, confidence: float, prevalence: float):
    """Minimum sample with hypergeometric P(detect >= 1) >= confidence."""
    if not 1 <= population <= MASK64:
        raise ValueError("population out of range")
    if not 0 < confidence < 1 or not 0 < prevalence <= 1:
        raise ValueError("confidence or prevalence out of range")
    defective = max(1, math.ceil(prevalence * population))
    target = math.log1p(-confidence)
    low, high = 0, population
    while low < high:
        middle = (low + high) // 2
        if miss_log_probability(population, defective, middle) <= target:
            high = middle
        else:
            low = middle + 1
    return low, defective


def is_prime(number):
    if number < 2:
        return False
    if number % 2 == 0:
        return number == 2
    p = 3
    while p * p <= number:
        if number % p == 0:
            return False
        p += 2
    return True


def coprime_prime_stride(population):
    """Coverage walk, not a source of entropy or statistical confidence."""
    if population < 3:
        return 1
    candidate = max(2, int(population * 0.6180339887498949))
    while candidate < population:
        if math.gcd(candidate, population) == 1 and is_prime(candidate):
            return candidate
        candidate += 1
    return 1


def prime_sentinels(population, count, rng, excluded):
    if count < 0:
        raise ValueError("negative sentinel count")
    if population == 0 or count == 0:
        return set(), {"stride": None, "start": None}
    stride = coprime_prime_stride(population)
    start = rng.randbelow(population)
    selected = set()
    for i in range(population):
        j = (start + i * stride) % population
        if j not in excluded:
            selected.add(j)
            if len(selected) >= min(count, population - len(excluded)):
                break
    return selected, {"stride": stride, "start": start}


def freeze_index(paths):
    """Hash and count *index bytes*. This is NOT a full source-message census."""
    if not paths:
        raise ValueError("at least one JSONL index is required")
    paths = sorted((Path(p) for p in paths), key=lambda p: p.name)
    if len({p.name for p in paths}) != len(paths):
        raise ValueError("duplicate index filenames")
    combined = hashlib.sha256()
    info = []
    total = 0
    for path in paths:
        digest = hashlib.sha256()
        count = 0
        with path.open("rb") as stream:
            for line in stream:
                if not line.strip():
                    raise ValueError("blank index line")
                record = json.loads(line)
                if not isinstance(record, dict):
                    raise ValueError("index record must be an object")
                digest.update(line)
                count += 1
        combined.update(DOMAIN + b"index\x00" + path.name.encode("utf-8")
                        + count.to_bytes(8, "big") + digest.digest())
        info.append({"name": path.name, "count": count,
                     "sha256": digest.hexdigest()})
        total += count
    if not total:
        raise ValueError("no records")
    if total > MASK64:
        raise ValueError("too many records")
    return paths, total, combined.hexdigest(), info


def generate_plan(paths, seed, confidence=0.95, prevalence=0.001,
                  sentinels=32):
    paths, population, digest, index_info = freeze_index(paths)
    n, defective = required_sample(population, confidence, prevalence)
    random_indices = sample_without_replacement(
        population, n, HMACStream(seed, digest, "uniform"))
    sent_indices, stride = prime_sentinels(
        population, sentinels, HMACStream(seed, digest, "prime-coverage"),
        random_indices)
    indexes = random_indices | sent_indices
    sample = []
    ordinal = 0
    for path in paths:
        with path.open(encoding="utf-8") as stream:
            for line in stream:
                if ordinal in indexes:
                    record = json.loads(line)
                    filename = record.get("file")
                    offset, length = record.get("offset"), record.get("length")
                    if (not isinstance(filename, str) or
                            not filename or Path(filename).name != filename or
                            not isinstance(offset, int) or offset < 0 or
                            not isinstance(length, int) or length <= 0):
                        raise ValueError("selected index has invalid byte range")
                    sample.append({
                        "ordinal": ordinal,
                        "kind": "simple_random" if ordinal in random_indices else "prime_sentinel",
                        "index": path.name,
                        "file": filename,
                        "offset": offset,
                        "length": length,
                        "message_id": record.get("message_id"),
                    })
                ordinal += 1
    actual_confidence = -math.expm1(miss_log_probability(population, defective, n))
    plan = {
        "schema": "evo.archive-audit.sample.v1",
        "population": population,
        "index_digest_sha256": digest,
        "index_files": index_info,
        "seed_hex": seed.hex(),  # Retain privately for deterministic replay.
        "seed_commitment_sha256": hashlib.sha256(DOMAIN + b"commit\x00" + seed).hexdigest(),
        "confidence_target": confidence,
        "prevalence_threshold": prevalence,
        "minimum_defective_items": defective,
        "random_sample_size": n,
        "actual_detection_probability": actual_confidence,
        "prime_sentinels": len(sent_indices),
        "prime_walk": stride,
        "sample": sample,
        "limitations": [
            "Sampling does not establish that source export was complete.",
            "Prime sentinels do not contribute to stated detection probability.",
            "The index and audit plan contain potentially sensitive metadata; never commit live plans.",
            "Archive repair and parity protection are NOT implemented by this module.",
        ],
    }
    return plan


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--index", nargs="+", required=True, help="canonical MBOX JSONL index files")
    parser.add_argument("--seed-hex", help="optional 32-byte seed in hex, for replay")
    parser.add_argument("--confidence", type=float, default=0.95)
    parser.add_argument("--prevalence", type=float, default=0.001)
    parser.add_argument("--prime-sentinels", type=int, default=32)
    parser.add_argument("--out", required=True, help="private output plan JSON")
    args = parser.parse_args()
    seed = bytes.fromhex(args.seed_hex) if args.seed_hex else secrets.token_bytes(32)
    plan = generate_plan(args.index, seed, args.confidence,
                         args.prevalence, args.prime_sentinels)
    Path(args.out).write_bytes(canonical(plan) + b"\n")
    print("population=%d simple_random=%d prime_sentinels=%d" % (
        plan["population"], plan["random_sample_size"], plan["prime_sentinels"]))


if __name__ == "__main__":
    main()
