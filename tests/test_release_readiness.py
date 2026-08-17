import unittest

from scripts.release_readiness import (
    GATES,
    build_gate_record,
    classify_run,
    select_authoritative_run,
    summarize,
)


COMMIT = "a" * 40


def run_for(path, *, run_id=1, run_number=1, attempt=1,
            status="completed", conclusion="success", head_sha=COMMIT):
    return {
        "id": run_id,
        "workflow_id": 1000 + run_id,
        "run_number": run_number,
        "run_attempt": attempt,
        "name": "fixture",
        "path": path,
        "event": "push",
        "status": status,
        "conclusion": conclusion,
        "head_sha": head_sha,
        "created_at": "2026-08-17T00:00:00Z",
        "updated_at": "2026-08-17T00:01:00Z",
        "html_url": f"https://example.invalid/runs/{run_id}",
        "check_suite_id": 2000 + run_id,
        "referenced_workflows": [],
    }


class ReleaseReadinessTest(unittest.TestCase):
    def test_release_type_gate_sets_are_explicit(self):
        candidate = {gate.gate_id for gate in GATES if "candidate" in gate.required_for}
        production = {gate.gate_id for gate in GATES if "production" in gate.required_for}
        documentation = {
            gate.gate_id for gate in GATES if "documentation" in gate.required_for
        }

        self.assertEqual(candidate, production)
        self.assertEqual(len(candidate), 22)
        self.assertTrue(
            {"production-providers", "production-provider-async"}.issubset(candidate)
        )
        self.assertEqual(
            documentation,
            {"aes-sec-001", "repository-compliance", "documentation"},
        )
        self.assertNotIn("project-zero", candidate)
        self.assertNotIn("project-zero", documentation)

    def test_latest_run_is_authoritative(self):
        path = ".github/workflows/verify.yml"
        older_success = run_for(path, run_id=10, run_number=10)
        newer_failure = run_for(
            path, run_id=11, run_number=11, conclusion="failure"
        )

        selected = select_authoritative_run(
            [older_success, newer_failure], path
        )
        self.assertEqual(selected["id"], 11)
        self.assertEqual(classify_run(selected, COMMIT)[0], "failed")

    def test_skipped_and_missing_are_distinct(self):
        path = ".github/workflows/verify.yml"
        skipped = run_for(path, conclusion="skipped")

        self.assertEqual(classify_run(skipped, COMMIT)[0], "skipped")
        self.assertEqual(classify_run(None, COMMIT)[0], "missing")

    def test_wrong_commit_fails_closed(self):
        run = run_for(
            ".github/workflows/verify.yml",
            head_sha="b" * 40,
        )
        status, reason = classify_run(run, COMMIT)
        self.assertEqual(status, "failed")
        self.assertIn("another commit", reason)

    def test_incomplete_run_fails_closed(self):
        run = run_for(
            ".github/workflows/verify.yml",
            status="in_progress",
            conclusion=None,
        )
        status, reason = classify_run(run, COMMIT)
        self.assertEqual(status, "failed")
        self.assertIn("not completed", reason)

    def test_not_applicable_gate_has_no_evidence_authority(self):
        gate = next(g for g in GATES if g.gate_id == "project-zero")
        record = build_gate_record(
            gate,
            "candidate",
            COMMIT,
            [],
            "deadbeef",
        )

        self.assertFalse(record["required"])
        self.assertEqual(record["status"], "not-applicable")
        self.assertIsNone(record["evidence"])

    def test_required_success_preserves_run_identity(self):
        gate = next(g for g in GATES if g.gate_id == "repository-verification")
        run = run_for(gate.workflow_path, run_id=42, run_number=77)
        run["referenced_workflows"] = [
            {
                "path": "owner/repo/.github/workflows/reusable.yml@main",
                "sha": "c" * 40,
                "ref": "refs/heads/main",
            }
        ]

        record = build_gate_record(
            gate,
            "candidate",
            COMMIT,
            [run],
            "feedface",
        )

        self.assertTrue(record["required"])
        self.assertEqual(record["status"], "passed")
        self.assertEqual(record["workflow"]["blob_sha"], "feedface")
        self.assertEqual(record["evidence"]["run_id"], 42)
        self.assertEqual(record["evidence"]["head_sha"], COMMIT)
        self.assertEqual(
            record["evidence"]["referenced_workflows"][0]["sha"],
            "c" * 40,
        )

    def test_missing_required_gate_prevents_complete_summary(self):
        passed = {
            "required": True,
            "status": "passed",
        }
        missing = {
            "required": True,
            "status": "missing",
        }
        not_applicable = {
            "required": False,
            "status": "not-applicable",
        }

        result = summarize([passed, missing, not_applicable])
        self.assertEqual(result["required"], 2)
        self.assertEqual(result["required_passed"], 1)
        self.assertEqual(result["missing"], 1)
        self.assertEqual(result["not-applicable"], 1)


if __name__ == "__main__":
    unittest.main()
