#!/usr/bin/env python3
"""Independent structural checks for EVO candidate measurement v1."""

from __future__ import annotations

import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def normalized(text: str) -> str:
    return " ".join(text.split())


def main() -> int:
    header = read("src/internal/project_measurement.h")
    model = read("src/project_measurement_model.c")
    runtime = read("src/project_measurement_runtime.c")
    transaction = read("src/project_measurement.c")
    test = read("tests/project_measurement_test.c")
    cmake = read("CMakeLists.txt")
    tests_cmake = read("tests/CMakeLists.txt")
    automake = read("Makefile.am")
    configure = read("configure.ac")
    readme = normalized(read("README.md"))
    roadmap = normalized(read("docs/roadmap.md"))
    architecture = normalized(read("docs/architecture.md"))
    algorithms = normalized(read("docs/algorithms.md"))
    repo_metadata = normalized(read("repo.yaml"))
    evo001 = normalized(read("docs/specs/EVO-001-library-contract.md"))
    evo002 = normalized(read("docs/specs/EVO-002-source-optimizer-contract.md"))
    adr = normalized(read("docs/adr/ADR-0041-reproducible-candidate-performance-fitness.md"))
    hra = normalized(read("docs/engineering/reports/EVO-HRA-013-candidate-performance-fitness-audit.md"))
    schema = json.loads(read("docs/schemas/evo-project-measurement-v1.schema.json"))
    manifest = json.loads(read(".aems/aes-bld-001.json"))

    require("EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION 1U" in header, "measurement schema version missing")
    require("evo_project_measurement_provider_fn" in header, "measurement provider boundary missing")
    require("warmup_count" in header and "repetition_count" in header, "sampling policy missing")
    require("max_runtime_range_ppm" in header, "stability policy missing")
    require("comparison_tolerance_ppm" in header and "minimum_improvement_ppm" in header, "comparison policy missing")
    require("fitness_weights" in header, "explicit fitness weights missing")
    require("performance_eligible" in model, "assurance admission check missing")
    require("runtime-median-deviation" in runtime, "outlier evidence missing")
    require("EVO_PROJECT_MEASUREMENT_UNSTABLE" in runtime, "unstable classification missing")
    require("correctness_preserved" in runtime, "correctness separation missing")
    require("best verified candidate found within the recorded bounded search contract" in runtime, "bounded result wording missing")
    require("config->limits.max_evidence_bytes" in runtime, "evidence buffer is not resource bounded")
    require("evo_measurement_result_fingerprint_double" in transaction, "canonical floating fitness identity missing")
    require("&owner->view.fitness, sizeof(owner->view.fitness)" not in transaction, "raw struct bytes must not define measurement identity")

    for forbidden in ("system(", "popen(", "/bin/sh -c"):
        require(forbidden not in model + runtime + transaction, f"forbidden execution path present: {forbidden}")

    for case in (
        "FAKE_FASTER",
        "FAKE_EQUAL",
        "FAKE_SLOWER",
        "FAKE_UNSTABLE",
        "FAKE_INCOMPLETE",
        "FAKE_OUTLIER",
        "replay-a",
        "replay-b",
    ):
        require(case in test, f"oracle fixture missing: {case}")

    require("project(catalyst_evo VERSION 0.40.0" in cmake, "CMake version is not 0.40.0")
    require("[0.40.0]" in configure, "Autotools version is not 0.40.0")
    for source in (
        "src/project_measurement_model.c",
        "src/project_measurement_runtime.c",
        "src/project_measurement.c",
    ):
        require(source in cmake, f"CMake source missing: {source}")
        require(source in automake, f"Automake source missing: {source}")
    require("evo_project_measurement_test" in tests_cmake, "canonical CTest measurement target missing")
    require("tests/evo_project_measurement_test" in automake, "Automake measurement test missing")

    manifest_text = json.dumps(manifest, sort_keys=True)
    for source in (
        "src/project_measurement_model.c",
        "src/project_measurement_runtime.c",
        "src/project_measurement.c",
    ):
        require(source in manifest_text, f"AES-BLD production inventory missing: {source}")
    require("tests/project_measurement_test.c" in manifest_text, "AES-BLD normative test inventory missing")

    require(schema.get("$schema") == "https://json-schema.org/draft/2020-12/schema", "schema draft mismatch")
    required = set(schema.get("required", []))
    for field in (
        "candidate_fingerprint",
        "assurance_fingerprint",
        "condition_fingerprint",
        "workloads",
        "samples",
        "fitness_available",
        "correctness_preserved",
        "fitness",
        "fitness_weights",
        "measurement_fingerprint",
    ):
        require(field in required, f"schema required field missing: {field}")
    require(schema["properties"]["correctness_preserved"].get("const") is True, "schema must preserve correctness authority")
    require(schema["properties"]["probabilistic_authority"].get("const") is False, "schema must forbid probabilistic authority")
    require("total" not in schema["$defs"]["weights"]["required"], "fitness weights must not invent a total weight")

    require("Status: Accepted" in adr, "ADR-0041 is not accepted")
    require("Raw samples are authority" in adr, "ADR raw-sample authority missing")
    require("Incomplete or unstable measurements" in adr, "ADR unstable/incomplete rule missing")
    require("Audited implementation: EVO 0.40.0" in hra, "HRA implementation status missing")
    require("No accelerator participates in the initial implementation" in hra, "HRA accelerator assessment missing")
    require("Correctness authority remains the assurance result" in hra, "HRA correctness separation missing")

    require("EVO 0.40.0 packages the deterministic" in readme, "README implementation boundary is stale")
    require("ADR-0041-reproducible-candidate-performance-fitness.md" in readme, "README ADR-0041 link missing")
    require("24 private source-foundation sources, and 39 normative tests" in readme, "README build inventory counts are stale")
    require("Issue #65 is the next dependency-ready" in roadmap, "roadmap next-boundary marker is stale")
    require("Version 0.40.0 contains" in architecture, "architecture implementation boundary is stale")
    require("EVO-HRA-013 audits this 0.40.0 boundary" in architecture, "architecture HRA-013 boundary missing")
    require("Version 0.40.0 implements" in algorithms, "algorithms implementation boundary is stale")
    require("performance-measurement-implemented-0.40.0" in repo_metadata, "repository metadata is stale")
    require("Version: 0.40.0" in evo001, "EVO-001 package version is stale")
    require("0.40.0 candidate-measurement boundary" in evo002, "EVO-002 implementation boundary is stale")
    require("ADR-0041 and EVO-HRA-013" in evo002, "EVO-002 measurement governance link missing")

    workflow = read(".github/workflows/project-measurement.yml")
    require("validate_project_measurement.py" in workflow, "hosted measurement validator missing")
    require("EVO_ENABLE_SANITIZERS=ON" in workflow, "hosted sanitizer build missing")
    require("autoreconf -fi" in workflow and "make check" in workflow, "hosted Autotools/GNU path missing")

    print("candidate measurement structural validation: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, OSError, json.JSONDecodeError) as exc:
        print(f"candidate measurement structural validation: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
