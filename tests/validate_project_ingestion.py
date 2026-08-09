#!/usr/bin/env python3
"""Validate the v1 project-ingestion fixtures and their independent golden.

The C implementation and this bounded Python reference intentionally compute
the same semantic manifest and baseline diagnostics through separate code.
Neither FNV label is authentication or sole identity authority; the complete
fixture file registry and retained snapshot bytes remain exact authority.
"""

from __future__ import annotations

import argparse
import json
import os
import stat
import sys
from pathlib import Path
from typing import Any

FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211
MASK64 = (1 << 64) - 1
SCHEMA = "catalyst.evo-project-manifest.v1"
PROVIDER = "evo-test-execution-provider-v1"


def fnv_bytes(value: int, data: bytes) -> int:
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & MASK64
    return value


def fnv_u64(value: int, number: int) -> int:
    return fnv_bytes(value, number.to_bytes(8, "little", signed=False))


def fnv_string(value: int, text: str) -> int:
    encoded = text.encode("utf-8")
    return fnv_bytes(fnv_u64(value, len(encoded)), encoded)


def fnv_field(value: int, name: str, text: str) -> int:
    return fnv_string(fnv_string(value, name), text)


def fingerprint(value: int) -> str:
    return f"fnv1a64-v1:{value:016x}"


def string_array(value: int, field: str, values: list[str]) -> int:
    value = fnv_string(value, field)
    value = fnv_u64(value, len(values))
    for item in values:
        value = fnv_string(value, item)
    return value


def named_array(value: int, field: str, values: list[dict[str, str]]) -> int:
    ordered = sorted(values, key=lambda item: (item["name"], item["identity"]))
    value = fnv_string(value, field)
    value = fnv_u64(value, len(ordered))
    for item in ordered:
        value = fnv_string(value, item["name"])
        value = fnv_string(value, item["identity"])
    return value


def manifest_fingerprint(manifest: dict[str, Any]) -> int:
    source = manifest["source"]
    build = manifest["build"]
    target = manifest["target"]
    search = manifest["search"]
    budgets = manifest["budgets"]
    artifacts = manifest["artifacts"]
    value = FNV_OFFSET
    value = fnv_field(value, "schema", manifest["schema"])
    value = fnv_field(value, "manifest_id", manifest["manifest_id"])
    value = fnv_field(value, "source_identity", source["declared_identity"])
    value = string_array(value, "permitted_roots", sorted(source["permitted_roots"]))
    value = fnv_field(value, "compilation_database", source["compilation_database"])
    value = fnv_field(value, "generated_source_policy", source["generated_source_policy"])
    value = fnv_field(value, "build_frontend", build["frontend"])
    for command in ("configure", "compile", "correctness", "benchmark"):
        value = fnv_string(value, command)
        value = string_array(value, "argv", build[command])
    value = fnv_u64(value, int(build["benchmark_required"]))
    value = fnv_field(value, "language", target["language"])
    value = string_array(value, "targets", sorted(target["platforms"]))
    value = named_array(value, "dependencies", manifest["dependencies"])
    value = named_array(value, "toolchains", manifest["toolchains"])
    environment = sorted(manifest["environment"], key=lambda item: item["name"])
    value = fnv_string(value, "environment")
    value = fnv_u64(value, len(environment))
    for item in environment:
        value = fnv_string(value, item["name"])
        value = fnv_string(value, item["value"])
    value = string_array(value, "workloads", sorted(manifest["workloads"]))
    value = string_array(value, "constraints", sorted(manifest["constraints"]))
    value = fnv_string(value, "search")
    for field in ("seed", "population", "generations", "workers"):
        value = fnv_u64(value, search[field])
    value = fnv_string(value, "budgets")
    for field in (
        "max_files",
        "max_file_bytes",
        "max_total_bytes",
        "max_path_bytes",
        "max_compilation_database_bytes",
        "max_command_output_bytes",
        "max_evidence_bytes",
        "command_timeout_ms",
        "max_memory_bytes",
        "max_processes",
        "max_storage_bytes",
    ):
        value = fnv_u64(value, budgets[field])
    value = fnv_u64(value, int(budgets["network_access"]))
    value = fnv_field(value, "retention", artifacts["retention"])
    value = fnv_field(value, "cleanup", artifacts["cleanup"])
    return value


def expand_files(project: Path, roots: list[str]) -> list[Path]:
    files: list[Path] = []
    for relative in sorted(roots):
        root = project / relative
        if root.is_symlink() or not root.exists():
            raise ValueError(f"invalid fixture root: {relative}")
        if root.is_file():
            files.append(root)
            continue
        for directory, directory_names, file_names in os.walk(root, followlinks=False):
            directory_names.sort()
            file_names.sort()
            directory_path = Path(directory)
            for name in directory_names:
                if (directory_path / name).is_symlink():
                    raise ValueError(f"fixture symlink: {directory_path / name}")
            for name in file_names:
                path = directory_path / name
                if path.is_symlink() or not path.is_file():
                    raise ValueError(f"invalid fixture file: {path}")
                files.append(path)
    return sorted(files, key=lambda path: path.relative_to(project).as_posix().encode("utf-8"))


def file_records(project: Path, manifest: dict[str, Any]) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for path in expand_files(project, manifest["source"]["permitted_roots"]):
        data = path.read_bytes()
        records.append(
            {
                "path": path.relative_to(project).as_posix(),
                "size": len(data),
                "mode": f"{stat.S_IMODE(path.stat().st_mode):04o}",
                "content_fingerprint": fingerprint(fnv_bytes(FNV_OFFSET, data)),
            }
        )
    return records


def normalize_project_path(project: Path, directory: str, value: str) -> str:
    project_text = project.as_posix()
    if directory == project_text:
        normalized_directory = "."
    elif directory.startswith(project_text + "/"):
        normalized_directory = directory[len(project_text) + 1 :]
    else:
        normalized_directory = directory
    if value.startswith(project_text + "/"):
        normalized = value[len(project_text) + 1 :]
    elif normalized_directory == ".":
        normalized = value
    else:
        normalized = f"{normalized_directory}/{value}"
    path = Path(normalized)
    if path.is_absolute() or not normalized or any(part in ("", ".", "..") for part in path.parts):
        raise ValueError(f"invalid compilation-database path: {value}")
    return path.as_posix()


def compilation_units(project: Path, manifest: dict[str, Any]) -> list[dict[str, Any]]:
    database_path = project / manifest["source"]["compilation_database"]
    database = json.loads(database_path.read_text(encoding="utf-8"))
    units: list[dict[str, Any]] = []
    if not isinstance(database, list) or not database:
        raise ValueError("empty compilation database")
    for entry in database:
        if not isinstance(entry, dict) or not {"directory", "file"} <= entry.keys():
            raise ValueError("malformed compilation-database entry")
        if ("arguments" in entry) == ("command" in entry):
            raise ValueError("ambiguous compilation-database command")
        directory = entry["directory"]
        project_text = project.as_posix()
        if directory == project_text:
            directory = "."
        elif directory.startswith(project_text + "/"):
            directory = directory[len(project_text) + 1 :]
        unit: dict[str, Any] = {
            "file": normalize_project_path(project, directory, entry["file"]),
            "directory": directory,
            "output": None
            if "output" not in entry
            else normalize_project_path(project, directory, entry["output"]),
        }
        if "arguments" in entry:
            unit["form"] = "arguments"
            unit["arguments"] = entry["arguments"]
        else:
            unit["form"] = "command"
            unit["command"] = entry["command"]
        units.append(unit)
    units.sort(
        key=lambda unit: (
            unit["file"],
            unit["directory"],
            unit["output"] or "",
            unit["form"],
            tuple(unit.get("arguments", [])),
            unit.get("command", ""),
        )
    )
    if len({unit["file"] for unit in units}) != len(units):
        raise ValueError("ambiguous compilation-database source")
    return units


def build_fingerprint(units: list[dict[str, Any]]) -> int:
    value = FNV_OFFSET
    value = fnv_string(value, "catalyst.evo-project-build-description.v1")
    value = fnv_u64(value, len(units))
    for unit in units:
        value = fnv_string(value, unit["file"])
        value = fnv_string(value, unit["directory"])
        value = fnv_string(value, unit["output"] or "")
        form = 1 if unit["form"] == "arguments" else 2
        value = fnv_u64(value, form)
        if form == 1:
            value = fnv_u64(value, len(unit["arguments"]))
            for argument in unit["arguments"]:
                value = fnv_string(value, argument)
        else:
            value = fnv_string(value, unit["command"])
    return value


def baseline_fingerprint(
    manifest_value: int,
    records: list[dict[str, Any]],
    normalized_build_value: int,
) -> int:
    value = FNV_OFFSET
    value = fnv_string(value, "catalyst.evo-project-baseline.v1")
    value = fnv_u64(value, manifest_value)
    value = fnv_string(value, PROVIDER)
    value = fnv_u64(value, len(records))
    for record in records:
        value = fnv_string(value, record["path"])
        value = fnv_u64(value, record["size"])
        value = fnv_u64(value, int(record["mode"], 8))
        value = fnv_u64(value, int(record["content_fingerprint"].split(":")[1], 16))
    value = fnv_u64(value, normalized_build_value)
    return value


def validate_fixture(root: Path, golden: dict[str, Any]) -> None:
    fixture = root / golden["frontend"]
    manifest = json.loads((fixture / "manifest.json").read_text(encoding="utf-8"))
    if manifest["schema"] != SCHEMA or manifest["manifest_id"] != golden["id"]:
        raise ValueError(f"manifest identity mismatch: {golden['id']}")
    records = file_records(fixture / "project", manifest)
    units = compilation_units(fixture / "project", manifest)
    manifest_value = manifest_fingerprint(manifest)
    normalized_build_value = build_fingerprint(units)
    baseline_value = baseline_fingerprint(
        manifest_value, records, normalized_build_value
    )
    if fingerprint(manifest_value) != golden["manifest_fingerprint"]:
        raise ValueError(f"manifest golden drift: {golden['id']}")
    if fingerprint(baseline_value) != golden["baseline_fingerprint"]:
        raise ValueError(f"baseline golden drift: {golden['id']}")
    if fingerprint(normalized_build_value) != golden["normalized_build_fingerprint"]:
        raise ValueError(f"normalized build golden drift: {golden['id']}")
    if records != golden["files"]:
        raise ValueError(f"file registry golden drift: {golden['id']}")
    if units != golden["compilation_units"]:
        raise ValueError(f"compilation-unit registry golden drift: {golden['id']}")

    reordered_path = fixture / "manifest-reordered.json"
    if reordered_path.exists():
        reordered = json.loads(reordered_path.read_text(encoding="utf-8"))
        if manifest_fingerprint(reordered) != manifest_value:
            raise ValueError("semantic manifest ordering changed the identity")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--fixture-root",
        type=Path,
        default=Path(__file__).parent / "fixtures" / "project-ingestion",
    )
    arguments = parser.parse_args()
    root = arguments.fixture_root.resolve()
    golden = json.loads((root / "golden-v1.json").read_text(encoding="utf-8"))
    if golden.get("schema") != "catalyst.evo-project-ingestion-golden.v1":
        raise ValueError("unsupported project-ingestion golden schema")
    if golden.get("execution_provider") != PROVIDER:
        raise ValueError("golden execution-provider drift")
    for fixture in golden["fixtures"]:
        validate_fixture(root, fixture)
    print(f"validated {len(golden['fixtures'])} project-ingestion fixtures")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f"project-ingestion validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
