from pathlib import Path
import json


def replace(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing optimization-flags anchor in {path}: {old[:160]!r}")
    p.write_text(text.replace(old, new, 1))


replace(
    "src/internal/project_measurement.h",
    "    const char *compiler_identity;\n"
    "    const char *linker_identity;\n"
    "    const char *environment_identity;",
    "    const char *compiler_identity;\n"
    "    const char *linker_identity;\n"
    "    const char *optimization_flags_identity;\n"
    "    const char *environment_identity;",
)

replace(
    "src/project_measurement_model.c",
    "        !evo_measurement_text_valid(\n"
    "            config->condition.linker_identity, config->limits.max_string_bytes) ||\n"
    "        !evo_measurement_text_valid(\n"
    "            config->condition.environment_identity,",
    "        !evo_measurement_text_valid(\n"
    "            config->condition.linker_identity, config->limits.max_string_bytes) ||\n"
    "        !evo_measurement_text_valid(\n"
    "            config->condition.optimization_flags_identity,\n"
    "            config->limits.max_string_bytes) ||\n"
    "        !evo_measurement_text_valid(\n"
    "            config->condition.environment_identity,",
)
replace(
    "src/project_measurement_model.c",
    "    evo_project_fingerprint_string(&fingerprint, config->condition.linker_identity);\n"
    "    evo_project_fingerprint_string(\n"
    "        &fingerprint, config->condition.environment_identity);",
    "    evo_project_fingerprint_string(&fingerprint, config->condition.linker_identity);\n"
    "    evo_project_fingerprint_string(\n"
    "        &fingerprint, config->condition.optimization_flags_identity);\n"
    "    evo_project_fingerprint_string(\n"
    "        &fingerprint, config->condition.environment_identity);",
)

replace(
    "src/project_measurement_runtime.c",
    "        !evo_measurement_append_json_field(\n"
    "            json, \"linker_identity\", config->condition.linker_identity, true) ||\n"
    "        !evo_measurement_append_json_field(\n"
    "            json, \"environment_identity\", config->condition.environment_identity, true) ||",
    "        !evo_measurement_append_json_field(\n"
    "            json, \"linker_identity\", config->condition.linker_identity, true) ||\n"
    "        !evo_measurement_append_json_field(\n"
    "            json,\n"
    "            \"optimization_flags_identity\",\n"
    "            config->condition.optimization_flags_identity,\n"
    "            true) ||\n"
    "        !evo_measurement_append_json_field(\n"
    "            json, \"environment_identity\", config->condition.environment_identity, true) ||",
)
replace(
    "src/project_measurement_runtime.c",
    "        !evo_candidate_buffer_append_text(markdown, \"`\\n- Linker: `\") ||\n"
    "        !evo_candidate_buffer_append_text(markdown, config->condition.linker_identity) ||\n"
    "        !evo_candidate_buffer_append_text(markdown, \"`\\n- Environment: `\") ||",
    "        !evo_candidate_buffer_append_text(markdown, \"`\\n- Linker: `\") ||\n"
    "        !evo_candidate_buffer_append_text(markdown, config->condition.linker_identity) ||\n"
    "        !evo_candidate_buffer_append_text(\n"
    "            markdown, \"`\\n- Optimization flags: `\") ||\n"
    "        !evo_candidate_buffer_append_text(\n"
    "            markdown, config->condition.optimization_flags_identity) ||\n"
    "        !evo_candidate_buffer_append_text(markdown, \"`\\n- Environment: `\") ||",
)

schema_path = Path("docs/schemas/evo-project-measurement-v1.schema.json")
schema = json.loads(schema_path.read_text())
condition = schema["properties"]["condition"]
required = condition["required"]
if "optimization_flags_identity" not in required:
    required.insert(required.index("environment_identity"), "optimization_flags_identity")
condition["properties"]["optimization_flags_identity"] = {
    "type": "string",
    "minLength": 1,
}
schema_path.write_text(json.dumps(schema, indent=2) + "\n")

replace(
    "tests/project_measurement_test.c",
    "    config.condition.compiler_identity = \"compiler:test\";\n"
    "    config.condition.linker_identity = \"linker:test\";\n"
    "    config.condition.environment_identity = \"environment:clean\";",
    "    config.condition.compiler_identity = \"compiler:test\";\n"
    "    config.condition.linker_identity = \"linker:test\";\n"
    "    config.condition.optimization_flags_identity =\n"
    "        \"optimization-flags:-O3-fixed\";\n"
    "    config.condition.environment_identity = \"environment:clean\";",
)
replace(
    "tests/project_measurement_test.c",
    "    CHECK(strstr(measurement.audit_markdown, \"hardware:test-host\") != NULL);\n"
    "    CHECK(strstr(measurement.audit_markdown, \"| performance |\") != NULL);",
    "    CHECK(strstr(measurement.audit_markdown, \"hardware:test-host\") != NULL);\n"
    "    CHECK(strstr(\n"
    "              measurement.audit_markdown,\n"
    "              \"optimization-flags:-O3-fixed\") != NULL);\n"
    "    CHECK(strstr(measurement.audit_markdown, \"| performance |\") != NULL);",
)

validator_path = Path("tests/validate_project_measurement.py")
validator = validator_path.read_text()
validator = validator.replace(
    '    require("fitness_weights" in header, "explicit fitness weights missing")\n',
    '    require("fitness_weights" in header, "explicit fitness weights missing")\n'
    '    require("optimization_flags_identity" in header, "optimization-flags condition identity missing")\n',
    1,
)
validator = validator.replace(
    '    require(schema["properties"]["correctness_preserved"].get("const") is True, "schema must preserve correctness authority")\n',
    '    condition_required = set(schema["properties"]["condition"]["required"])\n'
    '    require("optimization_flags_identity" in condition_required, "schema optimization-flags identity missing")\n'
    '    require(schema["properties"]["correctness_preserved"].get("const") is True, "schema must preserve correctness authority")\n',
    1,
)
validator_path.write_text(validator)

replace(
    "docs/adr/ADR-0041-reproducible-candidate-performance-fitness.md",
    "   condition fingerprint covering hardware, operating system, compiler/linker,\n"
    "   environment, dataset, and binary identities.",
    "   condition fingerprint covering hardware, operating system, compiler/linker,\n"
    "   optimization flags, environment, dataset, and binary identities.",
)
replace(
    "docs/engineering/reports/EVO-HRA-013-candidate-performance-fitness-audit.md",
    "| Condition identity | Exact hardware/OS/toolchain/environment/dataset/binary strings | Condition fingerprint plus named identities |",
    "| Condition identity | Exact hardware/OS/compiler/linker/optimization-flags/environment/dataset/binary strings | Condition fingerprint plus named identities |",
)
replace(
    "docs/engineering/reports/EVO-HRA-013-candidate-performance-fitness-audit.md",
    "Hardware, operating system, compiler/linker, environment, dataset, baseline\n"
    "binary, candidate binary, baseline identity, and candidate identity are bound",
    "Hardware, operating system, compiler/linker, optimization flags, environment,\n"
    "dataset, baseline binary, candidate binary, baseline identity, and candidate\n"
    "identity are bound",
)
