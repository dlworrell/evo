#!/usr/bin/env python3
"""Verify the AES-BLD machine inventory and its human-readable profile projection."""
from __future__ import annotations

import json
import re
import runpy
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AEMS = ROOT / ".aems/aes-bld-001.json"
PROFILE = ROOT / "docs/engineering/AES-BLD-001-toolchain-profile.md"
CMAKE = ROOT / "CMakeLists.txt"
MAKEFILE = ROOT / "Makefile.am"
TEST_CMAKE = ROOT / "tests/CMakeLists.txt"
BENCH_CMAKE = ROOT / "benchmarks/CMakeLists.txt"
READINESS = ROOT / "scripts/release_readiness.py"


def target_sources(text: str, target: str) -> list[str]:
    match = re.search(rf"add_library\(\s*{re.escape(target)}\s+STATIC(?P<body>.*?)\n\)", text, re.DOTALL)
    if match is None:
        raise RuntimeError(f"missing CMake target: {target}")
    return re.findall(r"\bsrc/[A-Za-z0-9_./-]+\.c\b", match.group("body"))


def fail(message: str) -> None:
    print(f"AES-BLD profile verification failed: {message}", file=sys.stderr)
    raise SystemExit(1)


data = json.loads(AEMS.read_text(encoding="utf-8"))
profile = PROFILE.read_text(encoding="utf-8")
cmake = CMAKE.read_text(encoding="utf-8")
makefile = MAKEFILE.read_text(encoding="utf-8")
test_cmake = TEST_CMAKE.read_text(encoding="utf-8")
bench_cmake = BENCH_CMAKE.read_text(encoding="utf-8")
core = target_sources(cmake, "catalyst_evo")
private = target_sources(cmake, "catalyst_evo_project_foundation")
production = data["build"]["production_sources"]
normative = data["build"]["normative_tests"]

if production != core + private:
    fail(".aems production_sources does not exactly match ordered CMake core + private target membership")
if len(production) != len(set(production)):
    fail("duplicate production source in .aems inventory")

combined_build = cmake + "\n" + test_cmake + "\n" + bench_cmake + "\n" + makefile
for test in normative:
    source = test.get("source", "")
    cmake_target = test.get("cmake", "")
    autotools_target = test.get("autotools", "")
    if not source or not (ROOT / source).is_file():
        fail(f"normative test source missing: {source!r}")
    if cmake_target not in combined_build:
        fail(f"normative CMake target not declared: {cmake_target!r}")
    if autotools_target not in makefile:
        fail(f"normative Autotools target not declared: {autotools_target!r}")

marker = re.search(
    r"<!-- AES-BLD-INVENTORY installed_core=(\d+) private_product=(\d+) production=(\d+) normative_tests=(\d+) -->",
    profile,
)
if marker is None:
    fail("profile inventory marker is missing")
actual = (len(core), len(private), len(production), len(normative))
recorded = tuple(int(value) for value in marker.groups())
if recorded != actual:
    fail(f"profile inventory marker {recorded} does not match machine/build authority {actual}")

namespace = runpy.run_path(str(READINESS))
for gate in namespace["GATES"]:
    if gate.category != "source-optimizer":
        continue
    if gate.display_name not in profile or f"`{gate.workflow_path}`" not in profile:
        fail(f"source-optimizer release gate missing from profile: {gate.display_name}")

print(
    "AES-BLD profile verified: "
    f"core={len(core)} private={len(private)} production={len(production)} normative_tests={len(normative)}"
)
