#!/usr/bin/env python3
"""Reject official GitHub action majors that still declare the deprecated Node 20 runtime."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POLICY = {
    "actions/checkout": 5,
    "actions/upload-artifact": 6,
}
failures: list[str] = []
seen: dict[str, int] = {name: 0 for name in POLICY}

for path in sorted((ROOT / ".github/workflows").glob("*.yml")):
    text = path.read_text(encoding="utf-8")
    for action, minimum in POLICY.items():
        pattern = re.compile(rf"uses:\s*{re.escape(action)}@v(\d+)(?:\.\d+)*")
        for match in pattern.finditer(text):
            seen[action] += 1
            major = int(match.group(1))
            if major < minimum:
                failures.append(f"{path.relative_to(ROOT)}: {action}@v{major} is below required v{minimum}")

if failures:
    for failure in failures:
        print(f"GitHub Actions runtime verification failed: {failure}", file=sys.stderr)
    raise SystemExit(1)
for action, minimum in POLICY.items():
    if seen[action] == 0:
        print(f"GitHub Actions runtime verification failed: no {action} references found", file=sys.stderr)
        raise SystemExit(1)
    print(f"{action}: {seen[action]} references satisfy >= v{minimum}")
