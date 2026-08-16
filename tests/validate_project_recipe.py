#!/usr/bin/env python3
"""Independently validate the retained EVO transformation-recipe golden."""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
GOLDEN = ROOT / "tests/fixtures/project-recipe/golden-v1.json"
SCHEMA = ROOT / "docs/schemas/evo-project-recipe-v1.schema.json"
FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211
MASK64 = (1 << 64) - 1


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"project-recipe validation failed: {message}")


def unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def load_json(path: Path) -> tuple[dict[str, Any], bytes]:
    raw = path.read_bytes()
    value = json.loads(raw.decode("utf-8"), object_pairs_hook=unique_object)
    require(isinstance(value, dict), f"{path} must contain an object")
    return value, raw


def exact_keys(value: Any, keys: set[str], name: str) -> dict[str, Any]:
    require(isinstance(value, dict), f"{name} must be an object")
    require(set(value) == keys, f"{name} keys differ")
    return value


def text(value: Any, name: str) -> str:
    require(isinstance(value, str) and bool(value), f"{name} must be text")
    require(
        all(ord(character) >= 0x20 and ord(character) != 0x7F for character in value),
        f"{name} contains a control character",
    )
    return value


def uint(value: Any, name: str, minimum: int = 0, maximum: int = MASK64) -> int:
    require(
        type(value) is int and minimum <= value <= maximum,
        f"{name} must be an integer in range",
    )
    return value


def fingerprint(value: Any, name: str) -> str:
    require(
        isinstance(value, str)
        and re.fullmatch(r"fnv1a64-v1:[0-9a-f]{16}", value) is not None,
        f"{name} fingerprint syntax",
    )
    return value


def sorted_unique(values: list[str], name: str) -> None:
    require(values == sorted(set(values)), f"{name} must be sorted and unique")


def validate_catalogue(value: Any, name: str) -> tuple[str, int]:
    catalogue = exact_keys(value, {"identity", "version"}, name)
    return (
        text(catalogue["identity"], f"{name} identity"),
        uint(catalogue["version"], f"{name} version", 1, (1 << 32) - 1),
    )


def validate_parameter(value: Any, name: str) -> str:
    parameter = exact_keys(
        value,
        {"identity", "kind", "integer_value", "boolean_value", "choice_value"},
        name,
    )
    identity = text(parameter["identity"], f"{name} identity")
    kind = parameter["kind"]
    require(kind in {"integer", "boolean", "choice"}, f"{name} kind")
    if kind == "integer":
        integer_value = parameter["integer_value"]
        require(
            type(integer_value) is int
            and -(1 << 63) <= integer_value <= (1 << 63) - 1,
            f"{name} integer value",
        )
        require(parameter["boolean_value"] is None, f"{name} boolean null")
        require(parameter["choice_value"] is None, f"{name} choice null")
    elif kind == "boolean":
        require(parameter["integer_value"] is None, f"{name} integer null")
        require(type(parameter["boolean_value"]) is bool, f"{name} boolean value")
        require(parameter["choice_value"] is None, f"{name} choice null")
    else:
        require(parameter["integer_value"] is None, f"{name} integer null")
        require(parameter["boolean_value"] is None, f"{name} boolean null")
        text(parameter["choice_value"], f"{name} choice value")
    return identity


def validate_target(value: Any, name: str) -> None:
    target = exact_keys(
        value,
        {
            "location_identity",
            "file",
            "line",
            "column",
            "end_line",
            "end_column",
            "kind",
            "spelling_identity",
        },
        name,
    )
    text(target["location_identity"], f"{name} identity")
    path = text(target["file"], f"{name} file")
    require(not path.startswith("/") and "\\" not in path, f"{name} relative file")
    for field in ("line", "column", "end_line", "end_column"):
        uint(target[field], f"{name} {field}", 1, (1 << 32) - 1)
    require(
        (target["end_line"], target["end_column"])
        >= (target["line"], target["column"]),
        f"{name} range",
    )
    require(target["kind"] in {"spelling", "macro-expansion"}, f"{name} kind")
    if target["kind"] == "spelling":
        require(target["spelling_identity"] is None, f"{name} spelling identity")
    else:
        text(target["spelling_identity"], f"{name} spelling identity")


def validate_recipe(recipe: dict[str, Any], raw: bytes) -> None:
    exact_keys(
        recipe,
        {
            "schema",
            "schema_version",
            "baseline_fingerprint",
            "analysis_fingerprint",
            "catalogue",
            "records",
            "human_readable_abstraction",
            "raw_source_bytes",
        },
        "recipe",
    )
    require(recipe["schema"] == "catalyst.evo-project-recipe.v1", "schema")
    require(recipe["schema_version"] == 1, "schema version")
    baseline = fingerprint(recipe["baseline_fingerprint"], "baseline")
    analysis = fingerprint(recipe["analysis_fingerprint"], "analysis")
    catalogue = validate_catalogue(recipe["catalogue"], "catalogue")
    hra = exact_keys(
        recipe["human_readable_abstraction"],
        {"reference_form", "projection", "complete", "probabilistic_authority"},
        "human-readable abstraction",
    )
    require(
        hra
        == {
            "reference_form": "canonical-json-record-array-and-direct-scans",
            "projection": "embedded-canonical-json-and-derived-markdown",
            "complete": True,
            "probabilistic_authority": False,
        },
        "human-readable abstraction declaration",
    )
    require(recipe["raw_source_bytes"] is False, "raw source bytes must be false")
    records = recipe["records"]
    require(isinstance(records, list), "records must be an array")
    emitted: set[str] = set()
    record_identities: list[str] = []
    for index, value in enumerate(records):
        name = f"record {index}"
        record = exact_keys(
            value,
            {
                "identity",
                "baseline_fingerprint",
                "analysis_fingerprint",
                "catalogue",
                "target",
                "transformation",
                "parameters",
                "preconditions",
                "dependencies",
                "conflicts",
                "provenance",
            },
            name,
        )
        identity = text(record["identity"], f"{name} identity")
        require(identity not in emitted, f"{name} duplicate identity")
        record_identities.append(identity)
        require(record["baseline_fingerprint"] == baseline, f"{name} baseline")
        require(record["analysis_fingerprint"] == analysis, f"{name} analysis")
        require(validate_catalogue(record["catalogue"], f"{name} catalogue") == catalogue,
                f"{name} catalogue binding")
        validate_target(record["target"], f"{name} target")
        transformation = exact_keys(
            record["transformation"], {"identity", "version"}, f"{name} transformation"
        )
        text(transformation["identity"], f"{name} transformation identity")
        uint(transformation["version"], f"{name} transformation version", 1, (1 << 32) - 1)
        parameters = record["parameters"]
        require(isinstance(parameters, list), f"{name} parameters")
        parameter_ids = [
            validate_parameter(parameter, f"{name} parameter {parameter_index}")
            for parameter_index, parameter in enumerate(parameters)
        ]
        sorted_unique(parameter_ids, f"{name} parameter identities")
        preconditions = record["preconditions"]
        require(isinstance(preconditions, list), f"{name} preconditions")
        precondition_values = [
            text(item, f"{name} precondition") for item in preconditions
        ]
        sorted_unique(precondition_values, f"{name} preconditions")
        dependencies = record["dependencies"]
        require(isinstance(dependencies, list), f"{name} dependencies")
        dependency_order: list[tuple[str, int]] = []
        for dependency_index, dependency_value in enumerate(dependencies):
            dependency = exact_keys(
                dependency_value,
                {"record_identity", "transformation_identity", "version"},
                f"{name} dependency {dependency_index}",
            )
            dependency_record = text(
                dependency["record_identity"], f"{name} dependency record"
            )
            require(dependency_record in emitted, f"{name} dependency must precede consumer")
            dependency_order.append(
                (
                    text(dependency["transformation_identity"], f"{name} dependency transform"),
                    uint(dependency["version"], f"{name} dependency version", 1, (1 << 32) - 1),
                )
            )
        require(dependency_order == sorted(set(dependency_order)), f"{name} dependency order")
        conflicts = record["conflicts"]
        require(isinstance(conflicts, list), f"{name} conflicts")
        conflict_order: list[tuple[str, int]] = []
        for conflict_index, conflict_value in enumerate(conflicts):
            conflict = exact_keys(
                conflict_value,
                {"transformation_identity", "version"},
                f"{name} conflict {conflict_index}",
            )
            conflict_order.append(
                (
                    text(conflict["transformation_identity"], f"{name} conflict transform"),
                    uint(conflict["version"], f"{name} conflict version", 1, (1 << 32) - 1),
                )
            )
        require(conflict_order == sorted(set(conflict_order)), f"{name} conflict order")
        provenance = exact_keys(
            record["provenance"],
            {"opportunity_rank", "compiler_records", "runtime_records"},
            f"{name} provenance",
        )
        uint(provenance["opportunity_rank"], f"{name} opportunity rank", 1)
        for field in ("compiler_records", "runtime_records"):
            values = provenance[field]
            require(isinstance(values, list), f"{name} {field}")
            identities = [text(item, f"{name} {field} identity") for item in values]
            sorted_unique(identities, f"{name} {field}")
        emitted.add(identity)
    require(record_identities == ["record-a", "record-b"], "golden composition vector")
    require(b"unit_value" not in raw and b"raw_source_bytes\":true" not in raw,
            "golden contains source bytes or authority claim")
    value = FNV_OFFSET
    for byte in raw:
        value ^= byte
        value = (value * FNV_PRIME) & MASK64
    require(value == 0x15955EC61C0C05BC, "golden FNV-1a vector")


def main() -> int:
    schema, _ = load_json(SCHEMA)
    require(schema.get("$schema") == "https://json-schema.org/draft/2020-12/schema", "schema dialect")
    recipe, raw = load_json(GOLDEN)
    validate_recipe(recipe, raw)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
