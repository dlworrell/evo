#!/usr/bin/env python3
"""Independently validate the retained EVO project-analysis golden."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GOLDEN = ROOT / "tests/fixtures/project-analysis/golden-v1.json"
SCHEMA = ROOT / "docs/schemas/evo-project-analysis-v1.schema.json"
MASK64 = (1 << 64) - 1
FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"project-analysis validation failed: {message}")


def unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream, object_pairs_hook=unique_object)
    require(isinstance(value, dict), f"{path} must contain an object")
    return value


class Fingerprint:
    def __init__(self) -> None:
        self.value = FNV_OFFSET

    def bytes(self, values: bytes) -> None:
        for value in values:
            self.value ^= value
            self.value = (self.value * FNV_PRIME) & MASK64

    def u64(self, value: int) -> None:
        require(0 <= value <= MASK64, "fingerprint integer out of range")
        self.bytes(value.to_bytes(8, "little"))

    def text(self, value: str | None) -> None:
        encoded = ("" if value is None else value).encode("utf-8")
        self.u64(len(encoded))
        self.bytes(encoded)


def require_exact_keys(value: dict[str, Any], keys: set[str], name: str) -> None:
    require(set(value) == keys, f"{name} keys differ")


def require_text(value: Any, name: str) -> None:
    require(isinstance(value, str) and bool(value), f"{name} must be text")
    require(
        all(ord(character) >= 0x20 and ord(character) != 0x7F for character in value),
        f"{name} contains a control character",
    )


def require_uint(value: Any, name: str, minimum: int = 0) -> None:
    require(
        type(value) is int and minimum <= value <= MASK64,
        f"{name} must be an unsigned 64-bit integer",
    )


def require_relative_path(value: Any, name: str) -> None:
    require_text(value, name)
    require(not value.startswith("/"), f"{name} must be relative")
    require("\\" not in value and ":" not in value, f"{name} has a forbidden byte")
    require(
        all(part not in {"", ".", ".."} for part in value.split("/")),
        f"{name} has a noncanonical component",
    )


def validate_shape(analysis: dict[str, Any]) -> None:
    top_keys = {
        "schema",
        "schema_version",
        "baseline_fingerprint",
        "analysis_fingerprint",
        "provider",
        "runtime_profile",
        "translation_units",
        "source_locations",
        "declarations",
        "calls",
        "control_flows",
        "data_flows",
        "compiler_optimizations",
        "runtime_hotspots",
        "opportunities",
        "human_readable_abstraction",
        "source_modified",
        "evolutionary_operator_invoked",
    }
    require_exact_keys(analysis, top_keys, "analysis")
    require(analysis["schema"] == "catalyst.evo-project-analysis.v1", "schema")
    require(analysis["schema_version"] == 1, "schema version")
    for name in ("baseline_fingerprint", "analysis_fingerprint"):
        require(
            isinstance(analysis[name], str)
            and re.fullmatch(r"fnv1a64-v1:[0-9a-f]{16}", analysis[name])
            is not None,
            f"{name} syntax",
        )

    provider = analysis["provider"]
    require(isinstance(provider, dict), "provider must be an object")
    require_exact_keys(
        provider, {"identity", "clang", "llvm", "target", "flags"}, "provider"
    )
    for name, value in provider.items():
        require_text(value, f"provider {name}")

    profile = analysis["runtime_profile"]
    require(isinstance(profile, dict), "runtime profile must be an object")
    require_exact_keys(profile, {"state", "identity"}, "runtime profile")
    require(
        profile["state"] in {"not-configured", "unavailable", "available"},
        "runtime profile state",
    )
    if profile["state"] == "not-configured":
        require(profile["identity"] is None, "unconfigured profile has identity")
    else:
        require_text(profile["identity"], "runtime profile identity")

    units = analysis["translation_units"]
    require(isinstance(units, list) and bool(units), "translation units")
    for index, unit in enumerate(units):
        require_relative_path(unit, f"translation unit {index}")

    domain_keys = {
        "source_locations": {
            "identity",
            "file",
            "line",
            "column",
            "end_line",
            "end_column",
            "kind",
            "spelling_identity",
        },
        "declarations": {
            "identity",
            "name",
            "translation_unit",
            "location_identity",
            "kind",
            "linkage",
            "definition",
        },
        "calls": {"identity", "caller", "callee", "location_identity", "kind"},
        "control_flows": {
            "identity",
            "function",
            "from",
            "to",
            "location_identity",
            "kind",
        },
        "data_flows": {
            "identity",
            "function",
            "declaration",
            "location_identity",
            "kind",
        },
        "compiler_optimizations": {
            "identity",
            "pass",
            "function",
            "location_identity",
            "message",
            "disposition",
        },
        "runtime_hotspots": {
            "identity",
            "workload",
            "function",
            "location_identity",
            "metric",
            "value",
        },
        "opportunities": {
            "rank",
            "kind",
            "location_identity",
            "missed_optimization_count",
            "runtime_sample_count",
        },
    }
    for domain, keys in domain_keys.items():
        records = analysis[domain]
        require(isinstance(records, list), f"{domain} must be an array")
        for index, record in enumerate(records):
            require(isinstance(record, dict), f"{domain} {index} must be an object")
            require_exact_keys(record, keys, f"{domain} {index}")

    require(bool(analysis["source_locations"]), "source locations are empty")
    for index, record in enumerate(analysis["source_locations"]):
        require_text(record["identity"], f"source location {index} identity")
        require_relative_path(record["file"], f"source location {index} file")
        for field in ("line", "column", "end_line", "end_column"):
            require_uint(record[field], f"source location {index} {field}", 1)
        require(
            (record["end_line"], record["end_column"])
            >= (record["line"], record["column"]),
            f"source location {index} range",
        )
        require(record["kind"] in {"spelling", "macro-expansion"}, "location kind")
        if record["spelling_identity"] is not None:
            require_text(record["spelling_identity"], "spelling identity")

    require(bool(analysis["declarations"]), "declarations are empty")
    for index, record in enumerate(analysis["declarations"]):
        for field in ("identity", "name", "location_identity"):
            require_text(record[field], f"declaration {index} {field}")
        require_relative_path(
            record["translation_unit"], f"declaration {index} translation unit"
        )
        require(record["kind"] in {"function", "variable", "type"}, "declaration kind")
        require(record["linkage"] in {"none", "internal", "external"}, "linkage")
        require(type(record["definition"]) is bool, "definition must be boolean")

    relation_text_fields = {
        "calls": ("identity", "caller", "callee", "location_identity"),
        "control_flows": (
            "identity",
            "function",
            "from",
            "to",
            "location_identity",
        ),
        "data_flows": (
            "identity",
            "function",
            "declaration",
            "location_identity",
        ),
    }
    relation_kinds = {
        "calls": {"direct", "indirect"},
        "control_flows": {
            "fallthrough",
            "branch-true",
            "branch-false",
            "back-edge",
            "return",
        },
        "data_flows": {"read", "write", "address", "escape"},
    }
    for domain, fields in relation_text_fields.items():
        for index, record in enumerate(analysis[domain]):
            for field in fields:
                require_text(record[field], f"{domain} {index} {field}")
            require(record["kind"] in relation_kinds[domain], f"{domain} kind")

    for index, record in enumerate(analysis["compiler_optimizations"]):
        for field in ("identity", "pass", "function", "location_identity", "message"):
            require_text(record[field], f"optimization {index} {field}")
        require(
            record["disposition"] in {"passed", "missed", "analysis"},
            "optimization disposition",
        )
    for index, record in enumerate(analysis["runtime_hotspots"]):
        for field in ("identity", "workload", "function", "location_identity"):
            require_text(record[field], f"runtime hotspot {index} {field}")
        require(record["metric"] == "sample-count", "runtime metric")
        require_uint(record["value"], f"runtime hotspot {index} value", 1)
    require(
        profile["state"] == "available" or not analysis["runtime_hotspots"],
        "runtime records require an available profile",
    )
    for index, record in enumerate(analysis["opportunities"]):
        require_uint(record["rank"], f"opportunity {index} rank", 1)
        require_text(record["location_identity"], "opportunity location")
        require(
            record["kind"]
            in {
                "compiler-missed",
                "runtime-hotspot",
                "compiler-missed-and-runtime-hotspot",
            },
            "opportunity kind",
        )
        require_uint(
            record["missed_optimization_count"],
            f"opportunity {index} missed count",
        )
        if record["runtime_sample_count"] is not None:
            require_uint(
                record["runtime_sample_count"],
                f"opportunity {index} runtime count",
                1,
            )

    hra = analysis["human_readable_abstraction"]
    require(isinstance(hra, dict), "human-readable abstraction must be an object")
    require_exact_keys(
        hra,
        {"reference_form", "projection", "complete", "probabilistic_authority"},
        "human-readable abstraction",
    )
    require(type(analysis["source_modified"]) is bool, "source_modified type")
    require(
        type(analysis["evolutionary_operator_invoked"]) is bool,
        "evolutionary_operator_invoked type",
    )


def identities(records: list[dict[str, Any]], name: str) -> list[str]:
    result = [record["identity"] for record in records]
    require(result == sorted(result), f"{name} order is not canonical")
    require(len(result) == len(set(result)), f"{name} identities repeat")
    return result


def validate_references(analysis: dict[str, Any]) -> None:
    units = analysis["translation_units"]
    locations = analysis["source_locations"]
    declarations = analysis["declarations"]
    location_ids = set(identities(locations, "source locations"))
    declaration_ids = set(identities(declarations, "declarations"))
    function_ids = {
        record["identity"]
        for record in declarations
        if record["kind"] == "function"
    }

    require(units == sorted(units), "translation-unit order is not canonical")
    require(len(units) == len(set(units)), "translation units repeat")
    for record in locations:
        require(record["kind"] in {"spelling", "macro-expansion"}, "location kind")
        if record["kind"] == "spelling":
            require(record["spelling_identity"] is None, "spelling has parent")
        else:
            require(
                record["spelling_identity"] in location_ids,
                "macro spelling reference is missing",
            )
            spelling = next(
                item
                for item in locations
                if item["identity"] == record["spelling_identity"]
            )
            require(spelling["kind"] == "spelling", "macro parent is not spelling")
    for record in declarations:
        require(record["translation_unit"] in units, "declaration unit is missing")
        require(record["location_identity"] in location_ids, "declaration location")
    for record in analysis["calls"]:
        require(record["caller"] in function_ids, "call caller is not a function")
        require(record["callee"] in function_ids, "call callee is not a function")
        require(record["location_identity"] in location_ids, "call location")
    for domain in ("control_flows", "data_flows", "compiler_optimizations"):
        for record in analysis[domain]:
            require(record["function"] in function_ids, f"{domain} function")
            require(record["location_identity"] in location_ids, f"{domain} location")
    for record in analysis["data_flows"]:
        require(record["declaration"] in declaration_ids, "data declaration")
    for record in analysis["runtime_hotspots"]:
        require(record["function"] in function_ids, "runtime function")
        require(record["location_identity"] in location_ids, "runtime location")
        require(record["metric"] == "sample-count", "runtime metric")
        require(isinstance(record["value"], int) and record["value"] > 0, "runtime value")


def expected_opportunities(analysis: dict[str, Any]) -> list[dict[str, Any]]:
    accumulated: dict[str, dict[str, Any]] = {}
    for record in analysis["compiler_optimizations"]:
        if record["disposition"] == "missed":
            current = accumulated.setdefault(
                record["location_identity"], {"missed": 0, "samples": 0, "runtime": False}
            )
            current["missed"] += 1
    for record in analysis["runtime_hotspots"]:
        current = accumulated.setdefault(
            record["location_identity"], {"missed": 0, "samples": 0, "runtime": False}
        )
        current["runtime"] = True
        current["samples"] += record["value"]
    ordered = sorted(
        accumulated.items(),
        key=lambda item: (
            0 if item[1]["runtime"] else 1,
            -item[1]["samples"],
            -item[1]["missed"],
            item[0],
        ),
    )
    result: list[dict[str, Any]] = []
    for rank, (location, evidence) in enumerate(ordered, 1):
        if evidence["runtime"] and evidence["missed"]:
            kind = "compiler-missed-and-runtime-hotspot"
        elif evidence["runtime"]:
            kind = "runtime-hotspot"
        else:
            kind = "compiler-missed"
        result.append(
            {
                "rank": rank,
                "kind": kind,
                "location_identity": location,
                "missed_optimization_count": evidence["missed"],
                "runtime_sample_count": evidence["samples"] if evidence["runtime"] else None,
            }
        )
    return result


def analysis_fingerprint(analysis: dict[str, Any]) -> str:
    fingerprint = Fingerprint()
    provider = analysis["provider"]
    profile = analysis["runtime_profile"]
    fingerprint.text("catalyst.evo-project-analysis.v1")
    fingerprint.text(analysis["baseline_fingerprint"])
    for field in ("identity", "clang", "llvm", "target", "flags"):
        fingerprint.text(provider[field])
    fingerprint.u64({"not-configured": 1, "unavailable": 2, "available": 3}[profile["state"]])
    fingerprint.text(profile["identity"])
    fingerprint.u64(len(analysis["translation_units"]))
    for unit in analysis["translation_units"]:
        fingerprint.text(unit)

    locations = analysis["source_locations"]
    fingerprint.u64(len(locations))
    for record in locations:
        fingerprint.text(record["identity"])
        fingerprint.text(record["file"])
        for field in ("line", "column", "end_line", "end_column"):
            fingerprint.u64(record[field])
        fingerprint.u64({"spelling": 1, "macro-expansion": 2}[record["kind"]])
        fingerprint.text(record["spelling_identity"])

    declarations = analysis["declarations"]
    fingerprint.u64(len(declarations))
    for record in declarations:
        for field in ("identity", "name", "translation_unit", "location_identity"):
            fingerprint.text(record[field])
        fingerprint.u64({"function": 1, "variable": 2, "type": 3}[record["kind"]])
        fingerprint.u64({"none": 1, "internal": 2, "external": 3}[record["linkage"]])
        fingerprint.u64(1 if record["definition"] else 0)

    relation_fields = (
        (
            "calls",
            ("identity", "caller", "callee", "location_identity"),
            {"direct": 1, "indirect": 2},
        ),
        (
            "control_flows",
            ("identity", "function", "from", "to", "location_identity"),
            {
                "fallthrough": 1,
                "branch-true": 2,
                "branch-false": 3,
                "back-edge": 4,
                "return": 5,
            },
        ),
        (
            "data_flows",
            ("identity", "function", "declaration", "location_identity"),
            {"read": 1, "write": 2, "address": 3, "escape": 4},
        ),
    )
    for domain, fields, kinds in relation_fields:
        fingerprint.u64(len(analysis[domain]))
        for record in analysis[domain]:
            for field in fields:
                fingerprint.text(record[field])
            fingerprint.u64(kinds[record["kind"]])

    optimizations = analysis["compiler_optimizations"]
    fingerprint.u64(len(optimizations))
    for record in optimizations:
        for field in ("identity", "pass", "function", "location_identity", "message"):
            fingerprint.text(record[field])
        fingerprint.u64({"passed": 1, "missed": 2, "analysis": 3}[record["disposition"]])

    runtime = analysis["runtime_hotspots"]
    fingerprint.u64(len(runtime))
    for record in runtime:
        for field in ("identity", "workload", "function", "location_identity"):
            fingerprint.text(record[field])
        fingerprint.u64(1)
        fingerprint.u64(record["value"])

    opportunities = analysis["opportunities"]
    fingerprint.u64(len(opportunities))
    for record in opportunities:
        fingerprint.u64(record["rank"])
        fingerprint.text(record["location_identity"])
        fingerprint.u64(record["missed_optimization_count"])
        fingerprint.u64(0 if record["runtime_sample_count"] is None else 1)
        fingerprint.u64(record["runtime_sample_count"] or 0)
    return f"fnv1a64-v1:{fingerprint.value:016x}"


def validate(path: Path) -> None:
    schema = load_json(SCHEMA)
    analysis = load_json(path)
    validate_shape(analysis)
    require(schema["properties"]["schema"]["const"] == analysis["schema"], "schema identity")
    require(analysis["schema_version"] == 1, "schema version")
    require(
        re.fullmatch(
            r"fnv1a64-v1:[0-9a-f]{16}", analysis["analysis_fingerprint"]
        )
        is not None,
        "analysis fingerprint syntax",
    )
    for domain in (
        "calls",
        "control_flows",
        "data_flows",
        "compiler_optimizations",
        "runtime_hotspots",
    ):
        identities(analysis[domain], domain)
    validate_references(analysis)
    require(
        analysis["opportunities"] == expected_opportunities(analysis),
        "opportunity derivation or order",
    )
    require(
        analysis["analysis_fingerprint"] == analysis_fingerprint(analysis),
        "independent fingerprint",
    )
    require(
        analysis["analysis_fingerprint"] == "fnv1a64-v1:2cc6038835197dba",
        "reviewed golden fingerprint",
    )
    require(
        analysis["human_readable_abstraction"]
        == {
            "reference_form": "complete-ordered-record-arrays-and-direct-scans",
            "projection": "analysis.md",
            "complete": True,
            "probabilistic_authority": False,
        },
        "human-readable abstraction declaration",
    )
    require(analysis["source_modified"] is False, "source modification claim")
    require(analysis["evolutionary_operator_invoked"] is False, "operator claim")
    print("project-analysis golden validation passed")


if __name__ == "__main__":
    validate(Path(sys.argv[1]) if len(sys.argv) == 2 else DEFAULT_GOLDEN)
