"""Unit tests for tools/project_zero_commit.py (AES-DEV-001-R005).

Run with: python3 -m unittest discover -s tests
"""

from __future__ import annotations

import base64
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("project_zero_commit", ROOT / "tools" / "project_zero_commit.py")
pzc = importlib.util.module_from_spec(SPEC)
sys.modules["project_zero_commit"] = pzc
SPEC.loader.exec_module(pzc)


class ParseNameStatusTest(unittest.TestCase):
    def test_additions_and_deletions(self) -> None:
        out = b"A\0SECURITY.md\0M\0repo.yaml\0D\0old file.md\0T\0link\0"
        changes = pzc.parse_name_status(out)
        self.assertEqual(changes.additions, ["SECURITY.md", "repo.yaml", "link"])
        self.assertEqual(changes.deletions, ["old file.md"])

    def test_empty(self) -> None:
        self.assertTrue(pzc.parse_name_status(b"").empty())

    def test_rejects_unknown_status(self) -> None:
        with self.assertRaises(pzc.CommitError):
            pzc.parse_name_status(b"R100\0a\0")

    def test_rejects_malformed(self) -> None:
        with self.assertRaises(pzc.CommitError):
            pzc.parse_name_status(b"A\0")


class ValidateBranchTest(unittest.TestCase):
    def test_accepts_remediation_branch(self) -> None:
        pzc.validate_branch("p0/remediation-1", "main", "main")

    def test_rejects_base_and_default(self) -> None:
        for branch, base, default in [
            ("main", "main", "main"),
            ("main", "refs/heads/main", "trunk"),
            ("trunk", "feature", "trunk"),
            ("refs/heads/x", "main", "main"),
            ("", "main", "main"),
            (" x", "main", "main"),
        ]:
            with self.subTest(branch=branch, base=base, default=default):
                with self.assertRaises(pzc.CommitError):
                    pzc.validate_branch(branch, base, default)


class PayloadTest(unittest.TestCase):
    def test_split_message(self) -> None:
        self.assertEqual(pzc.split_message("fix: x"), {"headline": "fix: x"})
        self.assertEqual(
            pzc.split_message("fix: x\n\nbody line\n"), {"headline": "fix: x", "body": "body line"}
        )
        with self.assertRaises(pzc.CommitError):
            pzc.split_message("\n\n")

    def test_build_file_changes_base64(self) -> None:
        changes = pzc.FileChanges(additions=["a.md"], deletions=["b.md"])
        payload = pzc.build_file_changes(changes, lambda path: b"\x00hello\n")
        self.assertEqual(
            payload,
            {
                "additions": [{"path": "a.md", "contents": base64.b64encode(b"\x00hello\n").decode()}],
                "deletions": [{"path": "b.md"}],
            },
        )
        self.assertEqual(pzc.build_file_changes(pzc.FileChanges(additions=["a"]), lambda p: b"")
                         .keys(), {"additions"})

    def test_build_mutation_input(self) -> None:
        value = pzc.build_mutation_input("o/r", "p0/x", "abc", "fix: y", {"additions": []})
        self.assertEqual(value["branch"], {"repositoryNameWithOwner": "o/r", "branchName": "p0/x"})
        self.assertEqual(value["expectedHeadOid"], "abc")
        self.assertEqual(value["message"], {"headline": "fix: y"})


def _git(root: Path, *args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(root), "-c", "commit.gpgsign=false", *args],
        check=True, stdout=subprocess.PIPE, text=True,
    ).stdout.strip()


class FakeGitHub:
    def __init__(self, refs: dict[str, str], default: str = "main") -> None:
        self.refs = dict(refs)
        self.default = default
        self.created_refs: list[tuple[str, str]] = []
        self.mutations: list[dict] = []

    def default_branch(self, repository: str) -> str:
        return self.default

    def ref_sha(self, repository: str, branch: str):
        return self.refs.get(branch)

    def create_ref(self, repository: str, branch: str, sha: str) -> None:
        self.created_refs.append((branch, sha))
        self.refs[branch] = sha

    def create_commit(self, mutation_input: dict) -> dict:
        self.mutations.append(mutation_input)
        return {"oid": "f" * 40, "url": "https://example.invalid/commit"}


class MainFlowTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        _git(self.root, "init", "-q", "-b", "main")
        _git(self.root, "config", "user.email", "t@example.invalid")
        _git(self.root, "config", "user.name", "t")
        (self.root / "README.md").write_text("x\n")
        (self.root / "old.md").write_text("old\n")
        _git(self.root, "add", ".")
        _git(self.root, "commit", "-q", "-m", "init")
        self.head = _git(self.root, "rev-parse", "HEAD")
        self.env = mock.patch.dict(os.environ, {"GITHUB_TOKEN": "t", "GITHUB_EVENT_NAME": "workflow_dispatch"})
        self.env.start()
        os.environ.pop("GITHUB_OUTPUT", None)

    def tearDown(self) -> None:
        self.env.stop()
        self.tmp.cleanup()

    def run_main(self, fake: FakeGitHub, *paths: str) -> int:
        with mock.patch.object(pzc, "GitHub", lambda *a, **k: fake):
            return pzc.main([
                "--repository", "o/r", "--branch", "p0/remediation-1", "--base", "main",
                "--message", "fix(p0): x", "--root", str(self.root), *paths,
            ])

    def test_noop_when_nothing_changed(self) -> None:
        fake = FakeGitHub({"main": self.head})
        self.assertEqual(self.run_main(fake, "README.md", "SECURITY.md"), 0)
        self.assertEqual(fake.mutations, [])
        self.assertEqual(fake.created_refs, [])

    def test_creates_branch_and_commit(self) -> None:
        (self.root / "SECURITY.md").write_text("sec\n")
        (self.root / "old.md").unlink()
        fake = FakeGitHub({"main": self.head})
        self.assertEqual(self.run_main(fake, "SECURITY.md", "old.md", "GOVERNANCE.md"), 0)
        self.assertEqual(fake.created_refs, [("p0/remediation-1", self.head)])
        (mutation,) = fake.mutations
        self.assertEqual(mutation["expectedHeadOid"], self.head)
        self.assertEqual(
            mutation["fileChanges"],
            {
                "additions": [{"path": "SECURITY.md", "contents": base64.b64encode(b"sec\n").decode()}],
                "deletions": [{"path": "old.md"}],
            },
        )

    def test_refuses_divergent_existing_branch(self) -> None:
        (self.root / "SECURITY.md").write_text("sec\n")
        fake = FakeGitHub({"main": self.head, "p0/remediation-1": "a" * 40})
        with self.assertRaisesRegex(pzc.CommitError, "differs from the checked-out base"):
            self.run_main(fake, "SECURITY.md")
        self.assertEqual(fake.created_refs, [])
        self.assertEqual(fake.mutations, [])

    def test_refuses_when_base_moved(self) -> None:
        (self.root / "SECURITY.md").write_text("sec\n")
        fake = FakeGitHub({"main": "b" * 40})
        with self.assertRaises(pzc.CommitError):
            self.run_main(fake, "SECURITY.md")
        self.assertEqual(fake.mutations, [])

    def test_refuses_default_branch(self) -> None:
        (self.root / "SECURITY.md").write_text("sec\n")
        fake = FakeGitHub({"main": self.head}, default="p0/remediation-1")
        with self.assertRaises(pzc.CommitError):
            self.run_main(fake, "SECURITY.md")
        self.assertEqual(fake.mutations, [])

    def test_refuses_pull_request_target(self) -> None:
        os.environ["GITHUB_EVENT_NAME"] = "pull_request_target"
        with self.assertRaises(pzc.CommitError):
            self.run_main(FakeGitHub({"main": self.head}), "SECURITY.md")


class ApiVerificationTest(unittest.TestCase):
    def test_rejects_unsigned_missing_invalid_and_wrong_ancestry(self):
        oid, parent = "f" * 40, "a" * 40
        request = pzc.build_mutation_input("o/r", "p0/x", parent, "fix: x", {})
        good = {"sha": oid, "commit": {"verification": {"verified": True, "reason": "valid"}},
                "parents": [{"sha": parent}]}
        import copy
        cases = []
        for verification in ({}, {"verified": False, "reason": "unsigned"},
                             {"verified": True, "reason": "unknown_key"},
                             {"verified": 1, "reason": "valid"}):
            item = copy.deepcopy(good)
            item["commit"]["verification"] = verification
            cases.append(item)
        wrong_parent = copy.deepcopy(good)
        wrong_parent["parents"] = [{"sha": "b" * 40}]
        cases.append(wrong_parent)
        wrong_oid = copy.deepcopy(good)
        wrong_oid["sha"] = "b" * 40
        cases.append(wrong_oid)
        for observed in cases:
            with self.subTest(observed=observed):
                api = pzc.GitHub("test", "https://api.invalid", "https://graphql.invalid")
                with mock.patch.object(api, "request", return_value=(200, {
                        "data": {"createCommitOnBranch": {"commit": {"oid": oid, "url": "u"}}}})), \
                     mock.patch.object(api, "rest", return_value=(200, observed)):
                    with self.assertRaises(pzc.CommitError):
                        api.create_commit(request)
        api = pzc.GitHub("test", "https://api.invalid", "https://graphql.invalid")
        with mock.patch.object(api, "request", return_value=(200, {
                "data": {"createCommitOnBranch": {"commit": {"oid": oid, "url": "u"}}}})), \
             mock.patch.object(api, "rest", return_value=(200, good)):
            self.assertEqual(api.create_commit(request)["oid"], oid)


if __name__ == "__main__":
    unittest.main()
