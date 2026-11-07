"""Upstream REA record for the rea/ scope (tools/rea_recovery/rea_upstream.py).

U1 record writes UPSTREAM.json with commit, tree digest, license files and submodules not ingested;
U2 check passes when rea/ matches the recorded tree;
U3 an unrecorded edit, added or removed file fails the check;
U4 the same edit passes once LOCAL-CHANGES.md records a local change;
U5 executable bits are part of the digest;
U6 record refuses a tree that does not match the checkout, and a dirty checkout;
U7 a malformed UPSTREAM.json is refused (exit 2);
U8 EVO-owned ci/ and docs/engineering/ are outside the digest, and record refuses upstream files there.
"""
from __future__ import annotations

import datetime as dt
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "rea_recovery"))
import rea_upstream as ru  # noqa: E402

NOW = dt.datetime(2026, 11, 7, 4, 0, tzinfo=dt.timezone.utc)
LOCAL_TEMPLATE = """# REA local changes

| Commit | Paths | Reason | Upstream status |
| --- | --- | --- | --- |
{rows}
| Path | Date | Reason | Last upstream commit taken |
| --- | --- | --- | --- |
"""


def sh(cwd: Path, *args: str) -> str:
    return subprocess.run(["git", "-C", str(cwd), *args], check=True, capture_output=True, text=True).stdout


class ReaUpstreamTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(self.enterContext(tempfile.TemporaryDirectory()))
        self.up = self.root / "upstream"
        (self.up / "src").mkdir(parents=True)
        (self.up / "LICENSE").write_text("MIT\n")
        (self.up / "licenses").mkdir()
        (self.up / "licenses" / "Apache-2.0.txt").write_text("apache\n")
        (self.up / "src" / "a.ts").write_text("export const a = 1;\n")
        tool = self.up / "run.sh"
        tool.write_text("#!/bin/sh\n")
        tool.chmod(0o755)
        sh(self.up, "init", "--quiet", "-b", "main")
        sh(self.up, "config", "user.email", "t@example.invalid")
        sh(self.up, "config", "user.name", "Test")
        sh(self.up, "add", ".")
        sh(self.up, "commit", "--quiet", "-m", "one")
        # A submodule pointer, as upstream REA has for third_party projects.
        sh(self.up, "update-index", "--add", "--cacheinfo", f"160000,{'c' * 40},third_party/dep")
        (self.up / "third_party" / "dep").mkdir(parents=True)  # an uninitialised submodule, as in a plain clone
        (self.up / ".gitmodules").write_text(
            '[submodule "third_party/dep"]\n\tpath = third_party/dep\n\turl = https://example.invalid/dep.git\n')
        sh(self.up, "add", ".gitmodules")
        sh(self.up, "commit", "--quiet", "-m", "dep")
        self.evo = self.root / "evo"
        self.evo.mkdir()
        sh(self.evo, "init", "--quiet", "-b", "main")
        self.rea = self.evo / "rea"
        self.rea.mkdir()
        archive = subprocess.run(["git", "-C", str(self.up), "archive", "HEAD"], check=True, capture_output=True).stdout
        subprocess.run(["tar", "-x", "-C", str(self.rea)], input=archive, check=True)
        shutil.rmtree(self.rea / "third_party", ignore_errors=True)
        (self.rea / "LOCAL-CHANGES.md").write_text(LOCAL_TEMPLATE.format(rows=""))

    def track(self) -> None:
        sh(self.evo, "add", "-A")

    def test_u1_record(self) -> None:
        data = ru.record(self.up, "https://example.invalid/rea", self.rea, NOW)
        on_disk = json.loads((self.rea / "UPSTREAM.json").read_text())
        self.assertEqual(on_disk, data)
        self.assertEqual(data["commit"], sh(self.up, "rev-parse", "HEAD").strip())
        self.assertEqual(data["file_count"], 5)  # LICENSE, licenses/…, src/a.ts, run.sh, .gitmodules
        self.assertEqual(data["license_files"], ["LICENSE", "licenses/Apache-2.0.txt"])
        self.assertEqual(data["submodules_not_ingested"],
                         [{"path": "third_party/dep", "commit": "c" * 40, "url": "https://example.invalid/dep.git"}])

    def test_u2_check_matches(self) -> None:
        ru.record(self.up, "https://example.invalid/rea", self.rea, NOW)
        self.track()
        self.assertEqual(ru.check(self.rea)[0], True)
        self.assertEqual(ru.main(["check", "--rea", str(self.rea)]), 0)

    def test_u3_unrecorded_changes_fail(self) -> None:
        ru.record(self.up, "https://example.invalid/rea", self.rea, NOW)
        self.track()
        sh(self.evo, "-c", "user.email=t@example.invalid", "-c", "user.name=Test", "commit", "--quiet", "-m", "base")
        for change in ("edit", "add", "remove"):
            with self.subTest(change=change):
                if change == "edit":
                    (self.rea / "src" / "a.ts").write_text("export const a = 2;\n")
                elif change == "add":
                    (self.rea / "src" / "b.ts").write_text("x\n")
                else:
                    (self.rea / "LICENSE").unlink()
                self.track()
                ok, message = ru.check(self.rea)
                self.assertFalse(ok)
                self.assertIn("records no local change", message)
                sh(self.evo, "reset", "--quiet", "--hard")
                sh(self.evo, "clean", "--quiet", "-fd")
                self.assertTrue(ru.check(self.rea)[0])

    def test_u4_recorded_local_change_passes(self) -> None:
        ru.record(self.up, "https://example.invalid/rea", self.rea, NOW)
        (self.rea / "src" / "a.ts").write_text("export const a = 2;\n")
        (self.rea / "LOCAL-CHANGES.md").write_text(LOCAL_TEMPLATE.format(
            rows="| abc1234 | src/a.ts | fix off-by-one | not offered |"))
        self.track()
        ok, message = ru.check(self.rea)
        self.assertTrue(ok)
        self.assertIn("1 local change", message)

    def test_u5_mode_is_digested(self) -> None:
        files = ["run.sh"]
        before = ru.tree_digest(self.rea, files)
        (self.rea / "run.sh").chmod(0o644)
        self.assertNotEqual(ru.tree_digest(self.rea, files), before)

    def test_u6_record_refusals(self) -> None:
        (self.rea / "src" / "a.ts").write_text("changed\n")
        with self.assertRaises(ru.UpstreamError):
            ru.record(self.up, "https://example.invalid/rea", self.rea, NOW)
        (self.up / "LICENSE").write_text("dirty\n")
        with self.assertRaises(ru.UpstreamError):
            ru.record(self.up, "https://example.invalid/rea", self.rea, NOW)

    def test_u7_malformed_record(self) -> None:
        (self.rea / "UPSTREAM.json").write_text(json.dumps({"schema": "other"}))
        err = io.StringIO()
        with redirect_stderr(err), redirect_stdout(io.StringIO()):
            self.assertEqual(ru.main(["check", "--rea", str(self.rea)]), 2)
        self.assertIn("is not a evo.rea-upstream.v1 record", err.getvalue())

    def test_u8_evo_owned_paths(self) -> None:
        ru.record(self.up, "https://example.invalid/rea", self.rea, NOW)
        (self.rea / "ci").mkdir()
        (self.rea / "ci" / "check.sh").write_text("#!/bin/sh\n")
        (self.rea / "docs" / "engineering").mkdir(parents=True)
        (self.rea / "docs" / "engineering" / "SECURE-C-CXX.md").write_text("profile\n")
        self.track()
        self.assertTrue(ru.check(self.rea)[0])
        (self.up / "ci").mkdir()
        (self.up / "ci" / "upstream.sh").write_text("x\n")
        sh(self.up, "add", ".")
        sh(self.up, "commit", "--quiet", "-m", "ci")
        with self.assertRaisesRegex(ru.UpstreamError, "EVO-owned paths"):
            ru.record(self.up, "https://example.invalid/rea", self.rea, NOW)


if __name__ == "__main__":
    unittest.main()
