import json
import unittest
from pathlib import Path


SCHEMA_DIR = Path(__file__).resolve().parents[1] / "docs" / "schemas"

EXPECTED = {
    "analyze": {
        "file": "evo-command-analyze-v1.schema.json",
        "schema": "catalyst.evo.command.analyze.v1",
        "required": {
            "schema",
            "operation",
            "manifest",
            "input",
            "output",
            "providerPolicy",
            "providers",
        },
        "provider_count": 2,
    },
    "evolve": {
        "file": "evo-command-evolve-v1.schema.json",
        "schema": "catalyst.evo.command.evolve.v1",
        "required": {
            "schema",
            "operation",
            "manifest",
            "input",
            "evidence",
            "output",
            "providerPolicy",
            "providers",
        },
        "provider_count": 4,
    },
    "replay": {
        "file": "evo-command-replay-v1.schema.json",
        "schema": "catalyst.evo.command.replay.v1",
        "required": {
            "schema",
            "operation",
            "manifest",
            "input",
            "evidence",
            "output",
            "providerPolicy",
            "providers",
            "replayIdentityComplete",
            "externalInputsDeclared",
        },
        "provider_count": 4,
    },
    "report": {
        "file": "evo-command-report-v1.schema.json",
        "schema": "catalyst.evo.command.report.v1",
        "required": {"schema", "operation", "evidence", "output"},
        "provider_count": 0,
    },
}


class ProductCommandSchemaTests(unittest.TestCase):
    def load(self, name):
        path = SCHEMA_DIR / EXPECTED[name]["file"]
        with path.open("r", encoding="utf-8") as handle:
            return json.load(handle)

    def test_exact_schema_set(self):
        paths = sorted(path.name for path in SCHEMA_DIR.glob("evo-command-*-v1.schema.json"))
        self.assertEqual(paths, sorted(value["file"] for value in EXPECTED.values()))

    def test_operation_contracts(self):
        for operation, expected in EXPECTED.items():
            with self.subTest(operation=operation):
                document = self.load(operation)
                self.assertEqual(
                    document["$schema"],
                    "https://json-schema.org/draft/2020-12/schema",
                )
                self.assertEqual(document["type"], "object")
                self.assertFalse(document["additionalProperties"])
                self.assertEqual(document["properties"]["schema"]["const"], expected["schema"])
                self.assertEqual(document["properties"]["operation"]["const"], operation)
                self.assertEqual(set(document["required"]), expected["required"])

                if expected["provider_count"] == 0:
                    self.assertNotIn("providers", document["properties"])
                    self.assertNotIn("providerPolicy", document["properties"])
                else:
                    providers = document["properties"]["providers"]
                    self.assertEqual(providers["minItems"], expected["provider_count"])
                    self.assertEqual(providers["maxItems"], expected["provider_count"])
                    self.assertEqual(
                        document["properties"]["providerPolicy"]["const"],
                        "catalyst.evo.provider-policy.v1",
                    )

    def test_replay_is_fail_closed_at_schema_boundary(self):
        replay = self.load("replay")
        self.assertIs(replay["properties"]["replayIdentityComplete"]["const"], True)
        self.assertIs(replay["properties"]["externalInputsDeclared"]["const"], True)

    def test_checkpoint_surface_is_bounded(self):
        self.assertNotIn("checkpoint", self.load("analyze")["properties"])
        self.assertIn("checkpoint", self.load("evolve")["properties"])
        self.assertIn("checkpoint", self.load("replay")["properties"])
        self.assertNotIn("checkpoint", self.load("report")["properties"])


if __name__ == "__main__":
    unittest.main()
