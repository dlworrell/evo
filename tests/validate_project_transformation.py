#!/usr/bin/env python3
"""Independently validate the EVO C transformation catalogue and fixtures."""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests/fixtures/project-transformation"
CATALOGUE_GOLDEN = FIXTURES / "catalogue-golden-v1.json"
APPLICATION_GOLDEN = FIXTURES / "application-golden-v1.json"
CATALOGUE_SCHEMA = (
    ROOT / "docs/schemas/evo-c-transformation-catalogue-v1.schema.json"
)
APPLICATION_SCHEMA = (
    ROOT / "docs/schemas/evo-c-transformation-application-v1.schema.json"
)
FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211
MASK64 = (1 << 64) - 1


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"project-transformation validation failed: {message}")


def unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(
        path.read_text(encoding="utf-8"), object_pairs_hook=unique_object
    )
    require(isinstance(value, dict), f"{path} must contain an object")
    return value


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


def sorted_unique(values: Any, name: str, allow_empty: bool = False) -> list[str]:
    require(isinstance(values, list), f"{name} must be an array")
    result = [text(value, f"{name} value") for value in values]
    require(allow_empty or bool(result), f"{name} must not be empty")
    require(result == sorted(set(result)), f"{name} must be sorted and unique")
    return result


def fnv_bytes(value: int, data: bytes) -> int:
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & MASK64
    return value


def fnv_u64(value: int, number: int) -> int:
    return fnv_bytes(value, (number & MASK64).to_bytes(8, "little"))


def fnv_text(value: int, item: str) -> int:
    encoded = item.encode("utf-8")
    return fnv_bytes(fnv_u64(value, len(encoded)), encoded)


def raw_fingerprint(data: bytes) -> str:
    return f"fnv1a64-v1:{fnv_bytes(FNV_OFFSET, data):016x}"


def validate_schema(schema: dict[str, Any], expected_id: str, name: str) -> None:
    require(
        schema.get("$schema") == "https://json-schema.org/draft/2020-12/schema",
        f"{name} dialect",
    )
    require(schema.get("$id", "").endswith(expected_id), f"{name} id")
    require(schema.get("type") == "object", f"{name} root type")
    require(schema.get("additionalProperties") is False, f"{name} closed root")
    required = schema.get("required")
    properties = schema.get("properties")
    require(isinstance(required, list) and required, f"{name} required fields")
    require(isinstance(properties, dict), f"{name} properties")
    require(set(required) == set(properties), f"{name} root fields differ")


def validate_versioned_identity(value: Any, name: str) -> tuple[str, int]:
    item = exact_keys(value, {"identity", "version"}, name)
    return (
        text(item["identity"], f"{name} identity"),
        uint(item["version"], f"{name} version", 1, (1 << 32) - 1),
    )


def validate_catalogue(catalogue: dict[str, Any]) -> dict[str, dict[str, Any]]:
    exact_keys(
        catalogue,
        {
            "schema",
            "schema_version",
            "catalogue",
            "ast_evidence_schema_version",
            "source_edit_schema_version",
            "entries",
            "human_readable_abstraction",
        },
        "catalogue",
    )
    require(
        catalogue["schema"] == "catalyst.evo-c-transformation-catalogue.v1",
        "catalogue schema",
    )
    require(catalogue["schema_version"] == 1, "catalogue schema version")
    require(
        validate_versioned_identity(catalogue["catalogue"], "catalogue identity")
        == ("catalyst.evo.c.ast-transformations", 1),
        "catalogue versioned identity",
    )
    require(catalogue["ast_evidence_schema_version"] == 1, "AST schema version")
    require(catalogue["source_edit_schema_version"] == 1, "edit schema version")
    require(
        catalogue["human_readable_abstraction"]
        == {
            "reference_form": "stable-capability-arrays-and-direct-dispatch",
            "projection": "complete-catalogue-json-and-derived-markdown",
            "complete": True,
            "probabilistic_authority": False,
        },
        "catalogue HRA declaration",
    )
    entries = catalogue["entries"]
    require(isinstance(entries, list), "catalogue entries")
    expected = {
        "catalyst.evo.c.assignment-to-compound": {
            "parameter": ("operator", "choice"),
            "forms": ["assignment-binary", "assignment-compound"],
            "formatting": "canonical-c17-spaces-v1",
        },
        "catalyst.evo.c.double-negation-condition": {
            "parameter": ("context", "choice"),
            "forms": ["double-negated-condition", "scalar-condition"],
            "formatting": "preserve-operand-spelling-v1",
        },
        "catalyst.evo.c.unsigned-multiply-to-shift": {
            "parameter": ("maximum-shift", "integer"),
            "forms": [
                "unsigned-multiply-power-of-two",
                "unsigned-shift-power-of-two",
            ],
            "formatting": "parenthesized-c17-shift-v1",
        },
    }
    by_identity: dict[str, dict[str, Any]] = {}
    for index, value in enumerate(entries):
        name = f"catalogue entry {index}"
        entry = exact_keys(
            value,
            {
                "identity",
                "version",
                "allowed_location_kinds",
                "parameter_schemas",
                "preconditions",
                "dependencies",
                "conflicts",
                "capability",
            },
            name,
        )
        identity = text(entry["identity"], f"{name} identity")
        require(identity not in by_identity, f"duplicate {identity}")
        require(identity in expected, f"unknown built-in {identity}")
        require(entry["version"] == 1, f"{name} version")
        require(entry["allowed_location_kinds"] == ["spelling"], f"{name} locations")
        require(entry["dependencies"] == [] and entry["conflicts"] == [], f"{name} edges")
        sorted_unique(entry["preconditions"], f"{name} preconditions")
        parameters = entry["parameter_schemas"]
        require(isinstance(parameters, list) and len(parameters) == 1, f"{name} parameter")
        parameter = exact_keys(
            parameters[0],
            {
                "identity",
                "kind",
                "required",
                "minimum_integer",
                "maximum_integer",
                "choices",
            },
            f"{name} parameter",
        )
        require(
            (parameter["identity"], parameter["kind"])
            == expected[identity]["parameter"],
            f"{name} parameter identity and kind",
        )
        require(parameter["required"] is True, f"{name} required parameter")
        if parameter["kind"] == "choice":
            require(
                parameter["minimum_integer"] is None
                and parameter["maximum_integer"] is None,
                f"{name} choice bounds",
            )
            sorted_unique(parameter["choices"], f"{name} choices")
        else:
            require(
                parameter["minimum_integer"] == 1
                and parameter["maximum_integer"] == 63
                and parameter["choices"] == [],
                f"{name} integer bounds",
            )
        capability = exact_keys(
            entry["capability"],
            {
                "provider_contract_version",
                "ast_forms",
                "formatting_policy",
                "idempotence_policy",
                "semantic_assumptions",
                "validation_obligations",
                "unsupported",
            },
            f"{name} capability",
        )
        require(capability["provider_contract_version"] == 1, f"{name} provider")
        require(capability["ast_forms"] == expected[identity]["forms"], f"{name} forms")
        require(
            capability["formatting_policy"] == expected[identity]["formatting"],
            f"{name} formatting",
        )
        require(
            capability["idempotence_policy"]
            == "already-satisfied-no-change-v1",
            f"{name} idempotence",
        )
        sorted_unique(capability["semantic_assumptions"], f"{name} assumptions")
        require(
            sorted_unique(capability["validation_obligations"], f"{name} obligations")
            == ["baseline-build", "baseline-correctness", "c17-syntax", "sanitizers"],
            f"{name} obligations",
        )
        require(
            capability["unsupported"]
            == {
                "comments": "reject",
                "macros": "reject",
                "language_extensions": "reject",
                "alias_assumptions": "reject",
            },
            f"{name} rejection policy",
        )
        by_identity[identity] = entry
    require(list(by_identity) == sorted(expected), "catalogue stable identity order")
    return by_identity


def validate_application_fingerprint(application: dict[str, Any]) -> None:
    ast_form_values = {
        "assignment-binary": 1,
        "assignment-compound": 2,
        "unsigned-multiply-power-of-two": 3,
        "unsigned-shift-power-of-two": 4,
        "double-negated-condition": 5,
        "scalar-condition": 6,
    }
    operator_values = {
        "none": 0,
        "add": 1,
        "bitwise-and": 2,
        "bitwise-or": 3,
        "bitwise-xor": 4,
        "multiply": 5,
        "subtract": 6,
        "shift-left": 7,
    }
    context_values = {"none": 0, "do-while": 1, "for": 2, "if": 3, "while": 4}
    disposition_values = {"edit": 1, "already-satisfied": 2}
    value = FNV_OFFSET
    for item in (
        "catalyst.evo-c-transformation-application.v1",
        application["baseline_fingerprint"],
        application["analysis_fingerprint"],
        application["recipe_fingerprint"],
        application["catalogue"]["identity"],
    ):
        value = fnv_text(value, item)
    value = fnv_u64(value, application["catalogue"]["version"])
    for item in (application["record_identity"], application["transformation"]["identity"]):
        value = fnv_text(value, item)
    value = fnv_u64(value, application["transformation"]["version"])
    parameter_kind_values = {"integer": 1, "boolean": 2, "choice": 3}
    value = fnv_u64(value, len(application["parameters"]))
    for parameter in application["parameters"]:
        value = fnv_text(value, parameter["identity"])
        value = fnv_u64(value, parameter_kind_values[parameter["kind"]])
        if parameter["kind"] == "integer":
            value = fnv_u64(value, parameter["integer_value"])
        elif parameter["kind"] == "boolean":
            value = fnv_u64(value, 1 if parameter["boolean_value"] else 0)
        else:
            value = fnv_text(value, parameter["choice_value"])
    value = fnv_text(value, application["provider"]["identity"])
    value = fnv_u64(value, application["provider"]["version"])
    for item in (
        application["provider"]["clang"],
        application["target"]["location_identity"],
        application["target"]["file"],
    ):
        value = fnv_text(value, item)
    before = application["edit"]["before"]
    after = application["edit"]["after"]
    for number in (
        before["start"],
        before["end"],
        ast_form_values[application["ast"]["form"]],
        operator_values[application["ast"]["operator"]],
        context_values[application["ast"]["condition_context"]],
    ):
        value = fnv_u64(value, number)
    ast = application["ast"]
    for name in ("primary", "duplicate_primary", "operand", "literal"):
        item = ast["ranges"][name]
        value = fnv_u64(value, 0 if item is None else item["start"])
        value = fnv_u64(value, 0 if item is None else item["end"])
    for name in (
        "primary_declaration_identity",
        "duplicate_declaration_identity",
    ):
        if ast[name] is None:
            value = fnv_u64(value, 0)
        else:
            value = fnv_text(value, ast[name])
    value = fnv_u64(value, ast["literal_value"])
    value = fnv_u64(value, ast["result_width_bits"])
    for name in (
        "primary_plain_identifier",
        "volatile_access",
        "result_unsigned_integer",
        "result_type_matches_primary",
        "scalar_operand",
        "contains_macro",
        "contains_comment",
        "contains_preprocessor",
        "language_extension",
        "ambiguous_target",
        "alias_assumption_required",
    ):
        value = fnv_u64(value, 1 if ast["facts"][name] else 0)
    value = fnv_u64(value, disposition_values[application["disposition"]])
    value = fnv_text(value, before["text"])
    value = fnv_u64(value, after["replacement_size"])
    if after["replacement_text"] is None:
        value = fnv_u64(value, 0)
    else:
        value = fnv_text(value, after["replacement_text"])
    value = fnv_text(value, application["formatting_policy"])
    value = fnv_text(value, application["idempotence_policy"])
    for field in ("semantic_assumptions", "validation_obligations"):
        value = fnv_u64(value, len(application[field]))
        for item in application[field]:
            value = fnv_text(value, item)
    require(
        application["application_fingerprint"] == f"fnv1a64-v1:{value:016x}",
        "application fingerprint vector",
    )


def validate_application(
    application: dict[str, Any], catalogue: dict[str, dict[str, Any]]
) -> None:
    exact_keys(
        application,
        {
            "schema",
            "schema_version",
            "application_fingerprint",
            "baseline_fingerprint",
            "analysis_fingerprint",
            "recipe_fingerprint",
            "catalogue",
            "record_identity",
            "transformation",
            "parameters",
            "provider",
            "target",
            "ast",
            "disposition",
            "edit",
            "formatting_policy",
            "idempotence_policy",
            "semantic_assumptions",
            "validation_obligations",
            "human_readable_abstraction",
            "snapshot_modified",
            "candidate_materialized",
        },
        "application",
    )
    require(
        application["schema"] == "catalyst.evo-c-transformation-application.v1",
        "application schema",
    )
    require(application["schema_version"] == 1, "application schema version")
    for field in (
        "application_fingerprint",
        "baseline_fingerprint",
        "analysis_fingerprint",
        "recipe_fingerprint",
    ):
        fingerprint(application[field], field)
    require(
        validate_versioned_identity(application["catalogue"], "application catalogue")
        == ("catalyst.evo.c.ast-transformations", 1),
        "application catalogue binding",
    )
    transformation = validate_versioned_identity(
        application["transformation"], "application transformation"
    )
    require(transformation[0] in catalogue and transformation[1] == 1, "known transform")
    parameters = application["parameters"]
    require(isinstance(parameters, list) and len(parameters) == 1, "application parameters")
    parameter = exact_keys(
        parameters[0],
        {"identity", "kind", "integer_value", "boolean_value", "choice_value"},
        "application parameter",
    )
    parameter_schema = catalogue[transformation[0]]["parameter_schemas"][0]
    require(
        (parameter["identity"], parameter["kind"])
        == (parameter_schema["identity"], parameter_schema["kind"]),
        "application parameter schema binding",
    )
    if parameter["kind"] == "integer":
        require(type(parameter["integer_value"]) is int, "integer parameter value")
        require(
            parameter_schema["minimum_integer"]
            <= parameter["integer_value"]
            <= parameter_schema["maximum_integer"],
            "integer parameter bounds",
        )
        require(
            parameter["boolean_value"] is None and parameter["choice_value"] is None,
            "integer parameter null fields",
        )
    elif parameter["kind"] == "boolean":
        require(type(parameter["boolean_value"]) is bool, "boolean parameter value")
        require(
            parameter["integer_value"] is None and parameter["choice_value"] is None,
            "boolean parameter null fields",
        )
    else:
        require(
            parameter["choice_value"] in parameter_schema["choices"],
            "choice parameter value",
        )
        require(
            parameter["integer_value"] is None and parameter["boolean_value"] is None,
            "choice parameter null fields",
        )
    text(application["record_identity"], "application record")
    provider = exact_keys(
        application["provider"], {"identity", "version", "clang"}, "provider"
    )
    text(provider["identity"], "provider identity")
    uint(provider["version"], "provider version", 1, (1 << 32) - 1)
    text(provider["clang"], "provider clang identity")
    target = exact_keys(
        application["target"],
        {
            "location_identity",
            "file",
            "line",
            "column",
            "end_line",
            "end_column",
            "kind",
            "spelling_identity",
            "byte_range",
        },
        "target",
    )
    text(target["location_identity"], "target identity")
    path = text(target["file"], "target path")
    require(not path.startswith("/") and "\\" not in path and ":" not in path, "relative target")
    require(target["kind"] == "spelling" and target["spelling_identity"] is None, "spelling target")
    for field in ("line", "column", "end_line", "end_column"):
        uint(target[field], f"target {field}", 1, (1 << 32) - 1)
    byte_range = exact_keys(target["byte_range"], {"start", "end"}, "target byte range")
    start = uint(byte_range["start"], "target start")
    end = uint(byte_range["end"], "target end", 1)
    require(start < end, "nonempty target range")
    ast = exact_keys(
        application["ast"],
        {
            "form",
            "operator",
            "condition_context",
            "ranges",
            "primary_declaration_identity",
            "duplicate_declaration_identity",
            "literal_value",
            "result_width_bits",
            "facts",
        },
        "AST",
    )
    capability = catalogue[transformation[0]]["capability"]
    require(ast["form"] in capability["ast_forms"], "AST form capability")
    ast_ranges = exact_keys(
        ast["ranges"], {"primary", "duplicate_primary", "operand", "literal"}, "AST ranges"
    )
    normalized_ranges: dict[str, tuple[int, int] | None] = {}
    for name, item in ast_ranges.items():
        if item is None:
            normalized_ranges[name] = None
            continue
        item = exact_keys(item, {"start", "end"}, f"AST {name} range")
        item_start = uint(item["start"], f"AST {name} start")
        item_end = uint(item["end"], f"AST {name} end", 1)
        require(start <= item_start < item_end <= end, f"AST {name} containment")
        normalized_ranges[name] = (item_start, item_end)
    require(
        normalized_ranges
        == {
            "primary": (181, 186),
            "duplicate_primary": (189, 194),
            "operand": (197, 202),
            "literal": None,
        },
        "assignment AST range vector",
    )
    require(
        text(ast["primary_declaration_identity"], "primary declaration")
        == ast["duplicate_declaration_identity"]
        == "declaration-total",
        "same declaration proof",
    )
    require(ast["literal_value"] == 0 and ast["result_width_bits"] == 0, "assignment numeric facts")
    facts = exact_keys(
        ast["facts"],
        {
            "primary_plain_identifier",
            "volatile_access",
            "result_unsigned_integer",
            "result_type_matches_primary",
            "scalar_operand",
            "contains_macro",
            "contains_comment",
            "contains_preprocessor",
            "language_extension",
            "ambiguous_target",
            "alias_assumption_required",
        },
        "AST facts",
    )
    require(all(type(value) is bool for value in facts.values()), "AST fact types")
    require(facts["primary_plain_identifier"] is True, "plain identifier proof")
    require(facts["result_type_matches_primary"] is True, "type proof")
    require(
        all(
            facts[name] is False
            for name in (
                "volatile_access",
                "contains_macro",
                "contains_comment",
                "contains_preprocessor",
                "language_extension",
                "ambiguous_target",
                "alias_assumption_required",
            )
        ),
        "unsupported AST facts",
    )
    require(application["disposition"] == "edit", "golden disposition")
    edit = exact_keys(application["edit"], {"before", "after"}, "edit")
    before = exact_keys(
        edit["before"], {"start", "end", "size", "fingerprint", "text"}, "before edit"
    )
    after = exact_keys(
        edit["after"],
        {"start", "end", "replacement_size", "replacement_fingerprint", "replacement_text"},
        "after edit",
    )
    require((before["start"], before["end"]) == (start, end), "target/edit range binding")
    before_bytes = text(before["text"], "before text").encode("utf-8")
    replacement_bytes = text(after["replacement_text"], "replacement text").encode("utf-8")
    require(before["size"] == len(before_bytes) == end - start, "before size")
    require(after["start"] == start, "after start")
    require(after["replacement_size"] == len(replacement_bytes), "replacement size")
    require(after["end"] == start + len(replacement_bytes), "after range")
    require(before["fingerprint"] == raw_fingerprint(before_bytes), "before fingerprint")
    require(
        after["replacement_fingerprint"] == raw_fingerprint(replacement_bytes),
        "replacement fingerprint",
    )
    require(application["formatting_policy"] == capability["formatting_policy"], "formatting binding")
    require(application["idempotence_policy"] == capability["idempotence_policy"], "idempotence binding")
    require(application["semantic_assumptions"] == capability["semantic_assumptions"], "assumption binding")
    require(application["validation_obligations"] == capability["validation_obligations"], "obligation binding")
    require(
        application["human_readable_abstraction"]
        == {
            "reference_form": "exact-source-edit-and-direct-dispatch",
            "projection": "complete-application-json-and-derived-markdown",
            "complete": True,
            "probabilistic_authority": False,
        },
        "application HRA declaration",
    )
    require(application["snapshot_modified"] is False, "snapshot modification claim")
    require(application["candidate_materialized"] is False, "candidate claim")
    validate_application_fingerprint(application)


def validate_fixture_composition(application: dict[str, Any]) -> None:
    before = (FIXTURES / "before.c").read_bytes()
    after = (FIXTURES / "after.c").read_bytes()
    declared = [
        (b"total = total + ready", b"total += ready"),
        (b"!!ready", b"ready"),
        (b"value * 8U", b"(value << 3)"),
    ]
    edits: list[tuple[int, int, bytes]] = []
    for old, new in declared:
        require(before.count(old) == 1, f"unique fixture target {old!r}")
        position = before.index(old)
        edits.append((position, position + len(old), new))
    output = bytearray(before)
    for start, end, replacement in sorted(edits, reverse=True):
        output[start:end] = replacement
    require(bytes(output) == after, "declared half-open edits compose to after.c")
    golden_before = application["edit"]["before"]
    golden_after = application["edit"]["after"]
    require(
        before[golden_before["start"] : golden_before["end"]]
        == golden_before["text"].encode("utf-8"),
        "application range addresses before.c",
    )
    require(
        golden_after["replacement_text"].encode("utf-8") == declared[0][1],
        "application replacement fixture",
    )


def main() -> int:
    catalogue_schema = load_json(CATALOGUE_SCHEMA)
    application_schema = load_json(APPLICATION_SCHEMA)
    validate_schema(
        catalogue_schema,
        "evo-c-transformation-catalogue-v1.schema.json",
        "catalogue schema",
    )
    validate_schema(
        application_schema,
        "evo-c-transformation-application-v1.schema.json",
        "application schema",
    )
    catalogue = validate_catalogue(load_json(CATALOGUE_GOLDEN))
    application = load_json(APPLICATION_GOLDEN)
    validate_application(application, catalogue)
    validate_fixture_composition(application)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
