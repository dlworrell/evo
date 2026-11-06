"""REA history bundles in Dropbox (tools/rea_recovery/rea_history.py).

H1 the first run writes a full bundle and a manifest; the bundle clones to upstream's head;
H2 an unchanged upstream writes nothing;
H3 new upstream commits give an incremental bundle; full + incremental restore the new head;
H4 a ref moved to already-bundled history is recorded without a bundle;
H5 a missing Dropbox folder parent is refused;
H6 a manifest for a different upstream is refused;
H7 --verify reports a changed bundle;
H8 --dry-run writes nothing to the Dropbox folder.
"""
from __future__ import annotations

import datetime as dt
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "rea_recovery"))
import rea_history  # noqa: E402

NOW = dt.datetime(2026, 11, 7, 2, 0, tzinfo=dt.timezone.utc)
LATER = dt.datetime(2026, 11, 8, 2, 0, tzinfo=dt.timezone.utc)


def sh(cwd: Path, *args: str) -> str:
    return subprocess.run(["git", "-C", str(cwd), *args], check=True, capture_output=True, text=True).stdout.strip()


class ReaHistoryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(self.enterContext(tempfile.TemporaryDirectory()))
        self.upstream = self.root / "upstream"
        self.upstream.mkdir()
        sh(self.upstream, "init", "--quiet", "-b", "main")
        sh(self.upstream, "config", "user.email", "t@example.invalid")
        sh(self.upstream, "config", "user.name", "Test")
        self.commit("one")
        sh(self.upstream, "tag", "v1")
        (self.root / "Dropbox" / "repo").mkdir(parents=True)
        self.dropbox = self.root / "Dropbox" / "repo" / "Rea"
        self.cache = self.root / "cache" / "rea.git"
        self.url = self.upstream.as_uri()
        self.logs: list[str] = []

    def commit(self, name: str) -> str:
        (self.upstream / f"{name}.txt").write_text(name)
        sh(self.upstream, "add", ".")
        sh(self.upstream, "commit", "--quiet", "-m", name)
        return sh(self.upstream, "rev-parse", "HEAD")

    def run_once(self, now=NOW, **kw):
        return rea_history.run(self.dropbox, self.cache, self.url, now, log=self.logs.append, **kw)

    def manifest(self) -> dict:
        return json.loads((self.dropbox / "MANIFEST.json").read_text())

    def test_h1_full_bundle_and_manifest(self) -> None:
        entry = self.run_once()
        self.assertEqual(entry["kind"], "full")
        bundle = self.dropbox / entry["file"]
        self.assertTrue(entry["file"].startswith("rea-full-20261107-"))
        self.assertEqual(entry["sha256"], rea_history.sha256(bundle))
        clone = self.root / "restore"
        subprocess.run(["git", "clone", "--quiet", str(bundle), str(clone)], check=True)
        self.assertEqual(sh(clone, "rev-parse", "HEAD"), sh(self.upstream, "rev-parse", "HEAD"))
        self.assertEqual(self.manifest()["upstream"], self.url)
        self.assertIn("refs/tags/v1", self.manifest()["bundles"][0]["refs"])
        self.assertEqual(sorted(p.name for p in self.dropbox.iterdir()), sorted([entry["file"], "MANIFEST.json"]))

    def test_h2_unchanged_upstream_writes_nothing(self) -> None:
        self.run_once()
        before = sorted(p.name for p in self.dropbox.iterdir())
        self.assertIsNone(self.run_once(LATER))
        self.assertEqual(sorted(p.name for p in self.dropbox.iterdir()), before)
        self.assertEqual(len(self.manifest()["bundles"]), 1)

    def test_h3_incremental_restores_new_head(self) -> None:
        full = self.run_once()
        new_head = self.commit("two")
        incr = self.run_once(LATER)
        self.assertEqual(incr["kind"], "incremental")
        self.assertIn(full["head"], incr["prerequisites"])
        self.assertLess(incr["size"], full["size"] + 1024)
        clone = self.root / "restore"
        subprocess.run(["git", "clone", "--quiet", str(self.dropbox / full["file"]), str(clone)], check=True)
        sh(clone, "fetch", "--quiet", str(self.dropbox / incr["file"]), "refs/heads/main:refs/remotes/bundle/main")
        self.assertEqual(sh(clone, "rev-parse", "refs/remotes/bundle/main"), new_head)

    def test_h4_ref_only_change_is_recorded_without_bundle(self) -> None:
        self.run_once()
        sh(self.upstream, "branch", "side")  # points at an already-bundled commit
        entry = self.run_once(LATER)
        self.assertEqual(entry["kind"], "refs-only")
        self.assertIsNone(entry["file"])
        self.assertIn("refs/heads/side", self.manifest()["bundles"][-1]["refs"])

    def test_h5_missing_dropbox_parent_is_refused(self) -> None:
        with self.assertRaises(rea_history.HistoryError):
            rea_history.run(self.root / "nowhere" / "Rea", self.cache, self.url, NOW, log=self.logs.append)

    def test_h6_manifest_for_other_upstream_is_refused(self) -> None:
        self.run_once()
        data = self.manifest()
        data["upstream"] = "https://example.invalid/other"
        (self.dropbox / "MANIFEST.json").write_text(json.dumps(data))
        with self.assertRaises(rea_history.HistoryError):
            self.run_once(LATER)

    def test_h7_verify_reports_changed_bundle(self) -> None:
        entry = self.run_once()
        self.assertEqual(rea_history.verify(self.dropbox, log=self.logs.append), 0)
        with (self.dropbox / entry["file"]).open("ab") as stream:
            stream.write(b"x")
        self.assertEqual(rea_history.verify(self.dropbox, log=self.logs.append), 1)
        self.assertTrue(any(line.startswith("CHANGED") for line in self.logs))

    def test_h8_dry_run_writes_nothing(self) -> None:
        entry = self.run_once(dry_run=True)
        self.assertEqual(entry["kind"], "full")
        self.assertEqual(list(self.dropbox.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
