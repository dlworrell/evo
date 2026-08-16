#!/usr/bin/env python3
"""Independent structural checks for the isolated candidate boundary."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "docs/schemas/evo-project-candidate-v1.schema.json"
ADR = ROOT / "docs/adr/ADR-0039-isolated-candidate-materialization.md"
HRA = ROOT / "docs/engineering/reports/EVO-HRA-011-candidate-materialization-audit.md"
PUBLIC_HEADER = ROOT / "src/internal/project_candidate.h"
MODEL = ROOT / "src/project_candidate_model.c"
MATERIALIZER = ROOT / "src/project_candidate.c"
RUNTIME = ROOT / "src/project_candidate_runtime.c"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"candidate validator failure: {message}")


def main() -> None:
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    require(schema.get("type") == "object", "schema root must be an object")
    require(schema.get("additionalProperties") is False, "schema must be closed")

    required = schema.get("required", [])
    for field in (
        "schema_version",
        "baseline_fingerprint",
        "recipe_fingerprint",
        "candidate_fingerprint",
        "workspace_policy",
        "file_count",
        "changed_files",
        "patch_bytes",
        "projection_complete",
        "probabilistic_authority",
        "source_modified",
        "snapshot_modified",
    ):
        require(field in required, f"schema omits required field {field}")

    properties = schema["properties"]
    require(properties["schema_version"].get("const") == 1, "schema version must be 1")
    require(
        properties["workspace_policy"].get("enum") == ["discard", "retain"],
        "workspace policies must remain explicit and ordered",
    )
    require(properties["projection_complete"].get("const") is True, "projection must be complete")
    require(
        properties["probabilistic_authority"].get("const") is False,
        "probabilistic authority must remain forbidden",
    )
    require(properties["source_modified"].get("const") is False, "source mutation claim changed")
    require(properties["snapshot_modified"].get("const") is False, "snapshot mutation claim changed")

    header = PUBLIC_HEADER.read_text(encoding="utf-8")
    model = MODEL.read_text(encoding="utf-8")
    materializer = MATERIALIZER.read_text(encoding="utf-8")
    runtime = RUNTIME.read_text(encoding="utf-8")
    adr = ADR.read_text(encoding="utf-8")
    hra = HRA.read_text(encoding="utf-8")

    for token in (
        "EVO_PROJECT_CANDIDATE_ERROR_CONFLICT",
        "EVO_PROJECT_CANDIDATE_ERROR_BASELINE_CHANGED",
        "EVO_PROJECT_CANDIDATE_ERROR_APPLICATION_DUPLICATE",
        "EVO_PROJECT_CANDIDATE_WORKSPACE_DISCARD",
        "EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN",
    ):
        require(token in header, f"public candidate contract missing {token}")

    require("evo_project_snapshot_verify_baseline" in model, "preflight must verify immutable baseline")
    require("evo_project_snapshot_verify_baseline" in materializer, "publication must reverify immutable baseline")
    require("EVO_PROJECT_CANDIDATE_ERROR_CONFLICT" in model, "overlap conflict path missing")
    require("AT_SYMLINK_NOFOLLOW" in runtime, "filesystem traversal must reject symlink aliases")
    require("O_NOFOLLOW" in runtime, "filesystem opens must be no-follow")
    require("candidate.patch" in materializer, "normalized patch publication missing")
    require(".evo-incomplete-v1" in materializer, "incomplete transaction marker missing")
    require(".evo-stage-v1" in materializer, "private staging directory missing")
    require("probabilistic_authority = false" in materializer, "result must reject probabilistic authority")
    require("source_modified = false" in materializer, "result must state source is unchanged")
    require("snapshot_modified = false" in materializer, "result must state snapshot is unchanged")

    combined = "\n".join((model, materializer, runtime))
    for forbidden in ("rand(", "random(", "bloom", "last-writer-wins"):
        require(forbidden not in combined.lower(), f"forbidden candidate authority pattern: {forbidden}")

    for phrase in (
        "Overlap is a hard conflict",
        "source_modified:false",
        "snapshot_modified:false",
        "Issue #63 owns candidate correctness gates",
    ):
        require(phrase in adr, f"ADR-0039 missing boundary statement: {phrase}")

    require("normalized patch" in hra.lower(), "HRA must audit the human-readable patch projection")
    require("probabilistic" in hra.lower(), "HRA must audit probabilistic authority")

    print("project candidate validator: all checks passed")


if __name__ == "__main__":
    main()
