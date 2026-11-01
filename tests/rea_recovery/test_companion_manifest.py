#!/usr/bin/env python3
"""Synthetic-only tests for the companion manifest v0.1 validator (EVO-004 stage E1)."""
import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "companion_manifest", ROOT / "tools/rea_recovery/companion_manifest.py")
cm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cm)
FIXTURE = ROOT / "tests/fixtures/rea-recovery/synthetic-v0.1"


class CompanionManifestTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.store = Path(tmp.name) / "store"
        shutil.copytree(FIXTURE / "store", self.store)
        self.manifest = json.loads((FIXTURE / "manifest.json").read_text())

    def check(self, manifest=None, pin=cm.CONTRACT_PIN):
        return cm.validate(self.manifest if manifest is None else manifest, self.store, pin)

    def refused_with(self, text, manifest=None):
        report = self.check(manifest)
        self.assertEqual(report["status"], "refused")
        self.assertTrue(any(text in p for p in report["problems"]), report["problems"])

    def rewrite_bundle(self, bundle):
        data = json.dumps(bundle).encode()
        (self.store / "native-bundle.json").write_bytes(data)
        self.manifest["artifacts"][0]["sha256"] = hashlib.sha256(data).hexdigest()
        self.manifest["artifacts"][0]["bytes"] = len(data)

    def test_committed_fixture_is_accepted_and_labelled_synthetic(self):
        report = self.check()
        self.assertEqual((report["status"], report["problems"], report["synthetic"]),
                         ("accepted", [], True))
        self.assertIn("not compatibility", report["limits"])

    def test_corrupt_or_unknown_evidence_id_is_refused(self):
        self.manifest["artifacts"][0]["evidence_ids"].append("synthetic:record-9999")
        self.refused_with("evidence_id not in native bundle native-bundle.json: synthetic:record-9999")
        bundle = json.loads((FIXTURE / "store/native-bundle.json").read_text())
        bundle["records"][1]["evidence_id"] = bundle["records"][0]["evidence_id"]
        self.setUp()
        self.rewrite_bundle(bundle)
        self.refused_with("duplicate evidence_id in native bundle")

    def test_broken_references_are_refused(self):
        (self.store / "native-bundle.json").write_text("{}")
        self.refused_with("artifact bytes do not match the manifest: native-bundle.json")
        (self.store / "native-bundle.json").unlink()
        self.refused_with("broken reference: native-bundle.json")
        for bad in ("../outside.json", "/etc/passwd", "a\\b"):
            manifest = copy.deepcopy(self.manifest)
            manifest["artifacts"][0]["locator"] = bad
            self.refused_with("unsafe or missing locator", manifest)

    @unittest.skipUnless(hasattr(os, "symlink"), "symlinks unavailable")
    def test_symlinked_artifact_is_refused(self):
        real = self.store / "real.json"
        (self.store / "native-bundle.json").rename(real)
        os.symlink(real, self.store / "native-bundle.json")
        self.refused_with("broken reference: native-bundle.json")

    def test_unsupported_version_and_unknown_fields_are_refused(self):
        self.refused_with("unsupported version: '0.2'", dict(self.manifest, version="0.2"))
        self.refused_with("unknown top-level field: extra", dict(self.manifest, extra=1))
        manifest = dict(self.manifest)
        del manifest["claims"]
        self.refused_with("missing field: claims", manifest)

    def test_rea_subject_mismatch_is_refused(self):
        candidate = "eb3b90beb4ac9245bac415696aa0de5a1a66c6d2"
        self.refused_with("REA subject mismatch", dict(self.manifest, rea={"commit": candidate}))
        self.assertEqual(self.check(dict(self.manifest, rea={"commit": candidate}), pin=candidate)["status"],
                         "accepted")

    def test_native_bundle_must_keep_its_seven_arrays(self):
        bundle = json.loads((FIXTURE / "store/native-bundle.json").read_text())
        bundle["equipment"] = {"model": "enriched"}
        self.rewrite_bundle(bundle)
        self.refused_with("does not have exactly the seven native arrays")

    def test_calibration_unknown_needs_rationale_and_no_values(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["calibration"] = {"status": "unknown"}
        self.refused_with("needs a rationale", manifest)
        manifest["calibration"] = {"status": "unknown", "rationale": "x", "values": [1.0]}
        self.refused_with("must not carry values", manifest)

    def test_claims_need_evidence_and_synthetic_cannot_pass_c1(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["claims"][0]["evidence"] = []
        self.refused_with("passes without evidence", manifest)
        manifest["claims"][0]["evidence"] = ["elsewhere.json"]
        self.refused_with("cites evidence not in this manifest", manifest)
        self.refused_with("synthetic artifacts cannot pass stage C1", dict(self.manifest, stage="C1"))

    def test_restricted_artifact_cannot_sit_in_a_public_manifest(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["artifacts"][0]["access"] = "restricted"
        self.refused_with("restricted artifact in a public manifest", manifest)

    def test_derivative_must_name_its_original(self):
        manifest = copy.deepcopy(self.manifest)
        derivative = dict(manifest["artifacts"][0], role="derivative", derived_from="0" * 64)
        derivative.pop("evidence_ids")
        manifest["artifacts"].append(derivative)
        self.refused_with("derivative must name the sha256", manifest)
        derivative["derived_from"] = manifest["artifacts"][0]["sha256"]
        self.assertEqual(self.check(manifest)["status"], "accepted")

    def test_cli_exit_codes(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(cm.main([str(FIXTURE / "manifest.json"), "--store", str(self.store)]), 0)
            (self.store / "native-bundle.json").write_text("{}")
            self.assertEqual(cm.main([str(FIXTURE / "manifest.json"), "--store", str(self.store)]), 3)
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(cm.main([str(FIXTURE / "manifest.json"), "--store", str(self.store / "nope")]), 2)


if __name__ == "__main__":
    unittest.main()
