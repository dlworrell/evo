#!/usr/bin/env python3
"""Tests for the EVO/REA fused build runner and vault (ADR-0048). Synthetic data and throwaway age keys only."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("evo_rea_build", ROOT / "tools/rea_recovery/evo_rea_build.py")
rb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rb)
HAVE_AGE = bool(shutil.which("age") and shutil.which("age-keygen"))


def git(cwd, *args):
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


class Discovery(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.rea = self.tmp / "rea"
        self.rea.mkdir()
        (self.rea / "package.json").write_text(json.dumps({"engines": {"node": ">=1.0.0"}}))
        git(self.rea, "init", "-q")
        git(self.rea, "-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false",
            "add", "-A")
        git(self.rea, "-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false",
            "commit", "-qm", "synthetic")
        self.head = git(self.rea, "rev-parse", "HEAD")
        self.evo = self.tmp / "evo"
        self.evo.mkdir()
        (self.evo / "CMakeLists.txt").write_text("project(catalyst_evo VERSION 9.9.9 LANGUAGES C)\n")

    def test_node_range_matching(self):
        engines = "^22.19.0 || ^24.11.0 || >=26.0.0"
        self.assertTrue(rb.node_satisfies("v22.22.0", engines))
        self.assertTrue(rb.node_satisfies("v26.1.0", engines))
        self.assertFalse(rb.node_satisfies("v22.18.9", engines))
        self.assertFalse(rb.node_satisfies("v23.0.0", engines))
        self.assertFalse(rb.node_satisfies("garbage", engines))

    def test_pinned_clean_checkout_and_writable_destination(self):
        report = rb.discover(self.evo, self.rea, self.tmp / "out", None, pin=self.head)
        self.assertEqual(report["checks"]["rea_source"]["status"], "ok")
        self.assertEqual(report["checks"]["evo_source"]["evidence"], f"{self.evo}: catalyst_evo 9.9.9")
        self.assertEqual(report["checks"]["destination"]["status"], "ok")
        self.assertNotIn("rea_source", report["blocking"])

    def test_pin_mismatch_and_dirty_tree_block(self):
        report = rb.discover(self.evo, self.rea, self.tmp / "out", None)
        self.assertEqual(report["checks"]["rea_source"]["status"], "mismatch")
        self.assertIn("rea_source", report["blocking"])
        (self.rea / "extra.txt").write_text("x")
        report = rb.discover(self.evo, self.rea, self.tmp / "out", None, pin=self.head)
        self.assertIn("local changes", report["checks"]["rea_source"]["evidence"])

    def test_missing_checkouts_block_build(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = rb.main(["build", "--evo", str(self.tmp / "no-evo"), "--rea", str(self.tmp / "nope"),
                            "--dest", str(self.tmp / "out")])
        self.assertEqual(code, 3)
        blocking = json.loads(out.getvalue())["blocking"]
        self.assertIn("rea_source", blocking)
        self.assertIn("evo_source", blocking)

    def test_build_plan_builds_both_into_one_destination(self):
        dest = self.tmp / "out"
        plan = [(cwd, " ".join(cmd)) for cwd, cmd in rb.build_plan(self.evo, self.rea, dest)]
        self.assertEqual([c for c, _ in plan[:4]], [self.evo] * 4)
        self.assertIn(f"-DCMAKE_INSTALL_PREFIX={dest}", plan[0][1])
        self.assertEqual(plan[3][1], f"cmake --install {dest / 'build' / 'evo'}")
        self.assertEqual([r for _, r in plan[4:]],
                         ["npm ci", "npm run build:cached", "node scripts/rea.mjs doctor --json"])
        self.assertEqual([c for c, _ in plan[4:]], [self.rea] * 3)

    def test_ui_uses_the_recorded_layout(self):
        dest = self.tmp / "out"
        dest.mkdir()
        rb.write_layout(self.evo, self.rea, dest, self.head)
        self.assertEqual(rb.ui_command(dest, "evo", ["--help"]), (None, [str(dest / "bin" / "evo"), "--help"]))
        cwd, cmd = rb.ui_command(dest, "rea", [])
        self.assertEqual((cwd, cmd), (self.rea.resolve(), ["node", "scripts/rea.mjs", "mcp"]))
        with self.assertRaises(ValueError):
            rb.ui_command(dest, "rea", ["setup"])

    def test_agent_configuring_commands_are_refused(self):
        for forbidden in ("setup", "update", "uninstall"):
            with self.assertRaises(ValueError):
                rb.rea_command([forbidden])
        self.assertEqual(rb.rea_command(["mcp"]), ["node", "scripts/rea.mjs", "mcp"])


class PublishedKeys(unittest.TestCase):
    KEY = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIL5nlTi5t/zI6AWP54uSPH17XxUOJLiSoAtsn1dPtwvF"

    def opener(self, body):
        seen = []

        class Response(io.BytesIO):
            def __enter__(self):
                return self

            def __exit__(self, *exc):
                return False

        def fake(request, timeout):
            self.assertEqual(request.get_header("Accept"), "text/plain")
            seen.append(request.full_url)
            return Response(body.encode())
        return fake, seen

    def test_sshid_and_github_urls_and_ed25519_only(self):
        fake, seen = self.opener(f"{self.KEY} #SSH ID - @someone\nssh-rsa AAAAB3Nza rsa-key\n{self.KEY}\n")
        url, keys = rb.fetch_public_keys("sshid", "someone", opener=fake)
        self.assertEqual((url, keys), ("https://sshid.io/someone", [self.KEY]))
        rb.fetch_public_keys("github", "someone", opener=fake)
        self.assertEqual(seen[-1], "https://github.com/someone.keys")

    def test_bad_source_username_or_empty_page_is_refused(self):
        fake, _ = self.opener("")
        for source, user in (("gitlab", "a"), ("sshid", "../etc"), ("sshid", "a/b")):
            with self.assertRaises(ValueError):
                rb.fetch_public_keys(source, user, opener=fake)
        with self.assertRaises(ValueError):
            rb.fetch_public_keys("sshid", "someone", opener=fake)

    def test_add_recipients_records_provenance_and_skips_duplicates(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "recipients.txt"
            self.assertEqual(rb.add_recipients(path, "Someone", "https://sshid.io/someone", [self.KEY]), [self.KEY])
            self.assertEqual(rb.add_recipients(path, "Someone", "https://sshid.io/someone", [self.KEY]), [])
            text = path.read_text()
            self.assertIn("from https://sshid.io/someone", text)
            self.assertEqual(rb._recipients(path), [self.KEY])


@unittest.skipUnless(HAVE_AGE, "age not installed")
class Vault(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.source = self.tmp / "source"
        (self.source / "captures").mkdir(parents=True)
        (self.source / "captures/synthetic.bin").write_bytes(b"SYNTHETIC capture \x00\x01")
        (self.source / "notes.txt").write_text("synthetic notes\n")
        self.keys, self.recipients = [], self.tmp / "recipients.txt"
        lines = ["# synthetic test recipients"]
        for name in ("alice", "bob"):
            identity = self.tmp / f"{name}.key"
            subprocess.run(["age-keygen", "-o", str(identity)], check=True, capture_output=True)
            public = subprocess.run(["age-keygen", "-y", str(identity)], check=True,
                                    capture_output=True, text=True).stdout.strip()
            self.keys.append(identity)
            lines.append(public)
        self.recipients.write_text("\n".join(lines) + "\n")
        self.vault = self.tmp / "vault"

    def test_seal_verify_open_round_trip_for_each_recipient(self):
        index = rb.vault_seal(self.source, self.vault, self.recipients)
        self.assertEqual(len(index["recipients"]), 2)
        self.assertNotIn(b"SYNTHETIC", (self.vault / "captures/synthetic.bin.age").read_bytes())
        self.assertEqual(rb.vault_verify(self.vault), [])
        for identity in self.keys:
            out = self.tmp / f"open-{identity.stem}"
            self.assertEqual(sorted(rb.vault_open(self.vault, identity, out)),
                             ["captures/synthetic.bin", "notes.txt"])
            self.assertEqual((out / "captures/synthetic.bin").read_bytes(), b"SYNTHETIC capture \x00\x01")

    def test_tampered_ciphertext_and_wrong_key_are_refused(self):
        rb.vault_seal(self.source, self.vault, self.recipients)
        stranger = self.tmp / "stranger.key"
        subprocess.run(["age-keygen", "-o", str(stranger)], check=True, capture_output=True)
        with self.assertRaises(RuntimeError):
            rb.vault_open(self.vault, stranger, self.tmp / "out")
        target = self.vault / "notes.txt.age"
        target.write_bytes(target.read_bytes() + b"x")
        self.assertEqual(rb.vault_verify(self.vault), ["changed: notes.txt.age"])
        with self.assertRaises(ValueError):
            rb.vault_open(self.vault, self.keys[0], self.tmp / "out2")

    def test_bad_recipients_and_vault_inside_source_are_refused(self):
        bad = self.tmp / "bad.txt"
        bad.write_text("not-a-key\n")
        with self.assertRaises(ValueError):
            rb.vault_seal(self.source, self.vault, bad)
        with self.assertRaises(ValueError):
            rb.vault_seal(self.source, self.source / "vault", self.recipients)

    def test_ssh_ed25519_recipient_round_trip(self):
        try:
            from cryptography.hazmat.primitives import serialization
            from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        except ImportError:
            self.skipTest("cryptography not installed")
        key = Ed25519PrivateKey.generate()
        identity = self.tmp / "id_ed25519"
        identity.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.OpenSSH,
                                               serialization.NoEncryption()))
        identity.chmod(0o600)
        public = key.public_key().public_bytes(serialization.Encoding.OpenSSH,
                                               serialization.PublicFormat.OpenSSH).decode()
        recipients = self.tmp / "ssh-recipients.txt"
        recipients.write_text(f"{public} #SSH ID - synthetic\n")
        rb.vault_seal(self.source, self.vault, recipients)
        out = self.tmp / "ssh-open"
        self.assertIn("notes.txt", rb.vault_open(self.vault, identity, out))
        self.assertEqual((out / "notes.txt").read_text(), "synthetic notes\n")

    def test_index_records_no_private_key_material(self):
        index = rb.vault_seal(self.source, self.vault, self.recipients)
        text = (self.vault / rb.INDEX).read_text()
        self.assertNotIn("AGE-SECRET-KEY", text)
        self.assertTrue(all(len(f) == 16 for f in index["recipients"]))


if __name__ == "__main__":
    unittest.main()
