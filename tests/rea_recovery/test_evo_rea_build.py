#!/usr/bin/env python3
"""Tests for the EVO/REA fused build runner and vault (ADR-0048). Synthetic data and throwaway age keys only."""
import contextlib
from datetime import date, timedelta
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

    OTHER = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIrZ0sm6dCnV0hT3zXl2E0ew9fqlfXbwbXKzQz8Gq1Yq"

    def test_people_are_listed_once_and_granted_per_vault(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "vaults.json"
            with self.assertRaisesRegex(ValueError, "unknown person"):
                rb.grant(path, "staging", "ops", "edit")
            self.assertEqual(len(rb.add_person_keys(path, "ops", "Ops", "https://sshid.io/ops", [self.KEY], False)), 1)
            self.assertEqual(rb.add_person_keys(path, "ops", "", "https://sshid.io/ops", [self.KEY], False), [])
            rb.add_person_keys(path, "qa", "QA", "https://sshid.io/qa", [self.OTHER], False)
            with self.assertRaisesRegex(ValueError, "at least one member who can edit"):
                rb.grant(path, "staging", "qa", "view")
            self.assertFalse(path.read_text().count("staging"))  # refused change never written
            rb.grant(path, "staging", "ops", "edit")
            rb.grant(path, "staging", "qa", "view")
            rb.grant(path, "production", "ops", "edit")
            self.assertEqual(rb.grant(path, "production", "ops", "edit"), "unchanged")
            config = rb.load_vaults(path)
            self.assertEqual(rb.person_access(config, "qa"), {"staging": "view"})
            self.assertEqual(rb.person_access(config, "ops"), {"production": "edit", "staging": "edit"})
            self.assertEqual([m["person"] for m in rb.vault_members(config, "production")], ["ops"])
            with self.assertRaises(ValueError):
                rb.vault_members(config, "nowhere")
            with self.assertRaises(ValueError):
                rb.add_person_keys(path, "Bad Id!", "x", "u", [self.KEY], False)

    def test_one_key_cannot_belong_to_two_people(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "vaults.json"
            rb.add_person_keys(path, "ops", "Ops", "u", [self.KEY], False)
            with self.assertRaisesRegex(ValueError, "belongs to both"):
                rb.add_person_keys(path, "qa", "QA", "u", [self.KEY], False)

    def test_revoke_keeps_an_editor_and_names_the_limit(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "vaults.json"
            rb.add_person_keys(path, "ops", "Ops", "u", [self.KEY], False)
            rb.add_person_keys(path, "qa", "QA", "u", [self.OTHER], False)
            rb.grant(path, "staging", "ops", "edit")
            rb.grant(path, "staging", "qa", "view")
            with self.assertRaisesRegex(ValueError, "last editor"):
                rb.revoke(path, "staging", "ops")
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                self.assertEqual(rb.main(["vault", "revoke", "staging", "qa", "--vaults", str(path)]), 0)
            self.assertIn("Re-seal staging", out.getvalue())
            self.assertEqual(rb.person_access(rb.load_vaults(path), "qa"), {})
            with self.assertRaisesRegex(ValueError, "not a member"):
                rb.revoke(path, "staging", "qa")

    def test_one_key_per_device_and_remove_a_lost_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "vaults.json"
            rb.add_person_keys(path, "ops", "Ops", "age-keygen", [self.KEY], True, device="iphone")
            rb.add_person_keys(path, "ops", "", "age-keygen", [self.OTHER], True, device="mac")
            rb.grant(path, "staging", "ops", "edit")
            members = rb.vault_members(rb.load_vaults(path), "staging")
            self.assertEqual(sorted(m["device"] for m in members), ["iphone", "mac"])
            lost = rb._fingerprint(self.KEY)
            with self.assertRaisesRegex(ValueError, "no key with fingerprint"):
                rb.remove_person_key(path, "ops", "0" * 16)
            self.assertEqual(rb.remove_person_key(path, "ops", lost), ["staging"])
            self.assertEqual([m["device"] for m in rb.vault_members(rb.load_vaults(path), "staging")], ["mac"])
            with self.assertRaisesRegex(ValueError, "last key"):
                rb.remove_person_key(path, "ops", rb._fingerprint(self.OTHER))
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                self.assertEqual(rb.main(["vault", "list", "--vaults", str(path)]), 0)
            self.assertIn("on mac", out.getvalue())
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(rb.main(["vault", "person", "remove-key", "ops", "--vaults", str(path)]), 2)

    def test_legacy_v1_file_is_upgraded(self):
        legacy = {"schema": "evo.rea-vaults.v1", "vaults": {
            "team": {"description": "t", "members": [
                {"label": "Ops Person", "role": "edit", "key": self.KEY, "source": "s", "can_decrypt": False,
                 "added": "2026-10-31"}]},
            "staging": {"description": "s", "members": [
                {"label": "Ops Person", "role": "edit", "key": self.KEY, "source": "s", "can_decrypt": False,
                 "added": "2026-10-31"}]}}}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "vaults.json"
            path.write_text(json.dumps(legacy))
            config = rb.load_vaults(path)
            self.assertEqual(config["schema"], "evo.rea-vaults.v2")
            self.assertEqual(list(config["people"]), ["ops-person"])
            self.assertEqual(rb.person_access(config, "ops-person"), {"staging": "edit", "team": "edit"})
            rb.add_person_keys(path, "qa", "QA", "u", [self.OTHER], False)
            self.assertEqual(json.loads(path.read_text())["schema"], "evo.rea-vaults.v2")

    def test_committed_vaults_file_is_valid_and_public_only(self):
        config = rb.load_vaults(rb.DEFAULT_VAULTS)
        self.assertIn("evo-rea-sources", config["vaults"])
        self.assertEqual(config["schema"], "evo.rea-vaults.v2")
        self.assertEqual(json.loads(rb.DEFAULT_VAULTS.read_text())["schema"], "evo.rea-vaults.v2")
        self.assertNotIn("SECRET", rb.DEFAULT_VAULTS.read_text().upper())


class AgentAccess(unittest.TestCase):
    """ADR-0048 section 6: agents get their own age1 key, a human sponsor, view only, and an expiry."""
    HUMAN = "age1" + "q" * 58
    AGENT = "age1" + "p" * 58
    PASSKEY = PublishedKeys.KEY
    TODAY = date(2026, 11, 1)

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.path = Path(tmp.name) / "vaults.json"
        rb.add_person_keys(self.path, "ops", "Ops", "age-keygen", [self.HUMAN], True)
        rb.grant(self.path, "team", "ops", "edit")

    def test_agent_is_registered_with_its_own_age_key_and_a_human_sponsor(self):
        self.assertEqual(rb.add_agent(self.path, "agent-claude", "Claude", "ops", self.AGENT),
                         rb._fingerprint(self.AGENT))
        agent = rb.load_vaults(self.path)["people"]["agent-claude"]
        self.assertEqual((agent["kind"], agent["sponsor"], agent["keys"][0]["can_decrypt"]), ("agent", "ops", True))
        self.assertEqual(rb.person_access(rb.load_vaults(self.path), "agent-claude"), {})  # adding grants nothing
        refusals = {
            "SSH ID passkey": lambda: rb.add_agent(self.path, "agent-x", "", "ops", self.PASSKEY),
            "id without the agent- prefix": lambda: rb.add_agent(self.path, "claude", "", "ops", self.AGENT),
            "unknown sponsor": lambda: rb.add_agent(self.path, "agent-y", "", "nobody", "age1" + "z" * 58),
            "agent sponsoring an agent": lambda: rb.add_agent(self.path, "agent-z", "", "agent-claude",
                                                              "age1" + "y" * 58),
            "re-adding": lambda: rb.add_agent(self.path, "agent-claude", "", "ops", "age1" + "x" * 58),
            "person add with an agent id": lambda: rb.add_person_keys(self.path, "agent-w", "", "age-keygen",
                                                                      ["age1" + "w" * 58], True),
        }
        for label, attempt in refusals.items():
            with self.subTest(label), self.assertRaises(ValueError):
                attempt()

    def test_agent_grants_are_view_only_bounded_and_expire(self):
        rb.add_agent(self.path, "agent-claude", "Claude", "ops", self.AGENT)
        today = self.TODAY
        for role, expires, message in (("view", "", "needs --expires"),
                                       ("edit", "2026-11-30", "only view grants expire"),
                                       ("view", "2026-10-31", "already past"),
                                       ("view", "2027-03-01", "at most 90 days"),
                                       ("view", "soon", "a date like")):
            with self.subTest(role=role, expires=expires), self.assertRaisesRegex(ValueError, message):
                rb.grant(self.path, "team", "agent-claude", role, expires=expires, today=today)
        self.assertEqual(rb.grant(self.path, "team", "agent-claude", "view", expires="2026-11-30", today=today),
                         "agent-claude is now view on team until 2026-11-30")
        config = rb.load_vaults(self.path)
        self.assertEqual(rb.person_access(config, "agent-claude", today), {"team": "view until 2026-11-30"})
        self.assertIn("agent-claude", [m["person"] for m in rb.vault_members(config, "team", today)])
        self.assertTrue(rb.vault_members(config, "team", date(2026, 11, 30)))  # last day still counts
        lapsed = date(2026, 12, 1)
        self.assertNotIn("agent-claude", [m["person"] for m in rb.vault_members(config, "team", lapsed)])
        self.assertEqual(rb.person_access(config, "agent-claude", lapsed), {"team": "expired 2026-11-30"})
        self.assertEqual(rb.grant(self.path, "team", "agent-claude", "view", expires="2026-11-30", today=today),
                         "unchanged")
        self.assertIn("until 2027-01-15",
                      rb.grant(self.path, "team", "agent-claude", "view", expires="2027-01-15", today=today))

    def test_sponsor_must_edit_every_vault_the_agent_sees(self):
        rb.add_person_keys(self.path, "qa", "QA", "age-keygen", ["age1" + "r" * 58], True)
        rb.grant(self.path, "staging", "qa", "edit")
        rb.add_agent(self.path, "agent-claude", "Claude", "ops", self.AGENT)
        with self.assertRaisesRegex(ValueError, "sponsor ops must be an editor"):
            rb.grant(self.path, "staging", "agent-claude", "view", expires="2026-11-30", today=self.TODAY)
        rb.grant(self.path, "team", "qa", "edit")
        rb.grant(self.path, "team", "agent-claude", "view", expires="2026-11-30", today=self.TODAY)
        with self.assertRaisesRegex(ValueError, "sponsor ops must be an editor"):
            rb.revoke(self.path, "team", "ops")  # revoke the agent first
        rb.revoke(self.path, "team", "agent-claude")
        rb.revoke(self.path, "team", "ops")

    def test_hand_edited_files_cannot_widen_agent_access(self):
        rb.add_agent(self.path, "agent-claude", "Claude", "ops", self.AGENT)
        rb.grant(self.path, "team", "agent-claude", "view", expires="2026-11-30", today=self.TODAY)
        good = json.loads(self.path.read_text())
        edits = {
            "agent with edit": lambda c: c["vaults"]["team"]["members"][1].update(role="edit"),
            "agent without expiry": lambda c: c["vaults"]["team"]["members"][1].pop("expires"),
            "agent with an ssh key": lambda c: c["people"]["agent-claude"]["keys"].append(
                {"key": self.PASSKEY, "can_decrypt": False}),
            "agent without sponsor": lambda c: c["people"]["agent-claude"].pop("sponsor"),
            "agent kind dropped": lambda c: c["people"]["agent-claude"].pop("kind"),
            "sponsor on a human": lambda c: c["people"]["ops"].update(sponsor="ops"),
            "unknown kind": lambda c: c["people"]["ops"].update(kind="robot"),
        }
        for label, edit in edits.items():
            config = json.loads(json.dumps(good))
            edit(config)
            self.path.write_text(json.dumps(config))
            with self.subTest(label), self.assertRaises(ValueError):
                rb.load_vaults(self.path)

    def test_cli_add_needs_yes_and_grant_needs_expires(self):
        args = ["vault", "agent", "add", "agent-claude", self.AGENT, "--sponsor", "ops", "--vaults", str(self.path)]
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(rb.main(args), 0)
        self.assertIn(rb._fingerprint(self.AGENT), out.getvalue())
        self.assertNotIn("agent-claude", self.path.read_text())  # shown, not written
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(rb.main(args + ["--yes"]), 0)
        with contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertEqual(rb.main(["vault", "grant", "team", "agent-claude", "--vaults", str(self.path)]), 2)
        self.assertIn("needs --expires", err.getvalue())
        until = (date.today() + timedelta(days=30)).isoformat()
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(rb.main(["vault", "grant", "team", "agent-claude", "--expires", until,
                                      "--vaults", str(self.path)]), 0)
            self.assertEqual(rb.main(["vault", "list", "--vaults", str(self.path)]), 0)
        self.assertIn(f"agent until {until}", out.getvalue())


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
        self.keys, self.members = [], []
        for name, role in (("alice", "edit"), ("bob", "view")):
            identity = self.tmp / f"{name}.key"
            subprocess.run(["age-keygen", "-o", str(identity)], check=True, capture_output=True)
            public = subprocess.run(["age-keygen", "-y", str(identity)], check=True,
                                    capture_output=True, text=True).stdout.strip()
            self.keys.append(identity)
            self.members.append({"label": name, "role": role, "key": public, "can_decrypt": True})
        self.vault = self.tmp / "vault"

    def test_seal_verify_open_round_trip_for_each_recipient(self):
        index = rb.vault_seal(self.source, self.vault, self.members, "synthetic")
        self.assertEqual(len(index["recipients"]), 2)
        self.assertNotIn(b"SYNTHETIC", (self.vault / "objects/000001.age").read_bytes())
        public = "\n".join(str(p.relative_to(self.vault)) for p in self.vault.rglob("*"))
        public += (self.vault / rb.INDEX).read_text()
        for name in ("captures", "synthetic.bin", "notes"):
            self.assertNotIn(name, public)  # names and structure stay inside manifest.age
        self.assertEqual(rb.vault_verify(self.vault), [])
        for identity in self.keys:
            out = self.tmp / f"open-{identity.stem}"
            self.assertEqual(sorted(rb.vault_open(self.vault, identity, out)),
                             ["captures/synthetic.bin", "notes.txt"])
            self.assertEqual((out / "captures/synthetic.bin").read_bytes(), b"SYNTHETIC capture \x00\x01")

    def test_tampered_ciphertext_and_wrong_key_are_refused(self):
        rb.vault_seal(self.source, self.vault, self.members, "synthetic")
        stranger = self.tmp / "stranger.key"
        subprocess.run(["age-keygen", "-o", str(stranger)], check=True, capture_output=True)
        with self.assertRaises(RuntimeError):
            rb.vault_open(self.vault, stranger, self.tmp / "out")
        target = self.vault / "objects/000002.age"
        target.write_bytes(target.read_bytes() + b"x")
        self.assertEqual(rb.vault_verify(self.vault), ["changed: objects/000002.age"])
        with self.assertRaises(ValueError):
            rb.vault_open(self.vault, self.keys[0], self.tmp / "out2")

    def test_bad_keys_vault_inside_source_and_unopenable_vaults_are_refused(self):
        with self.assertRaises(ValueError):
            rb.vault_seal(self.source, self.vault, [dict(self.members[0], key="not-a-key")])
        with self.assertRaises(ValueError):
            rb.vault_seal(self.source, self.source / "vault", self.members)
        device_bound = [dict(m, can_decrypt=False) for m in self.members]
        with self.assertRaisesRegex(ValueError, "no member of this vault can decrypt"):
            rb.vault_seal(self.source, self.vault, device_bound)

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
        member = {"label": "ssh", "role": "edit", "key": f"{public} #synthetic", "can_decrypt": True}
        rb.vault_seal(self.source, self.vault, [member])
        out = self.tmp / "ssh-open"
        self.assertIn("notes.txt", rb.vault_open(self.vault, identity, out))
        self.assertEqual((out / "notes.txt").read_text(), "synthetic notes\n")

    def test_access_is_per_vault_not_per_key(self):
        path = self.tmp / "vaults.json"
        for identity, member in zip(self.keys, self.members):
            rb.add_person_keys(path, member["label"], member["label"], "age-keygen", [member["key"]], True)
        rb.grant(path, "team", "alice", "edit")
        rb.grant(path, "team", "bob", "view")
        rb.grant(path, "production", "alice", "edit")
        config = rb.load_vaults(path)
        rb.vault_seal(self.source, self.tmp / "production", rb.vault_members(config, "production"), "production")
        self.assertIn("notes.txt", rb.vault_open(self.tmp / "production", self.keys[0], self.tmp / "a"))
        with self.assertRaises(RuntimeError):
            rb.vault_open(self.tmp / "production", self.keys[1], self.tmp / "b")
        index = rb.vault_seal(self.source, self.tmp / "team", rb.vault_members(config, "team"), "team")
        self.assertEqual(sorted(m["person"] for m in index["members"]), ["alice", "bob"])
        self.assertIn("notes.txt", rb.vault_open(self.tmp / "team", self.keys[1], self.tmp / "c"))

    def test_expired_agent_grant_is_left_out_of_the_next_seal(self):
        path = self.tmp / "vaults.json"
        alice, agent = self.members[0]["key"], self.members[1]["key"]
        rb.add_person_keys(path, "alice", "Alice", "age-keygen", [alice], True)
        rb.add_agent(path, "agent-claude", "Claude", "alice", agent)
        rb.grant(path, "team", "alice", "edit")
        today = date.today()
        rb.grant(path, "team", "agent-claude", "view", expires=(today + timedelta(days=1)).isoformat())
        config = rb.load_vaults(path)
        rb.vault_seal(self.source, self.tmp / "now", rb.vault_members(config, "team", today), "team")
        self.assertIn("notes.txt", rb.vault_open(self.tmp / "now", self.keys[1], self.tmp / "a"))
        later = today + timedelta(days=2)
        index = rb.vault_seal(self.source, self.tmp / "later", rb.vault_members(config, "team", later), "team")
        self.assertEqual([m["person"] for m in index["members"]], ["alice"])
        with self.assertRaises(RuntimeError):
            rb.vault_open(self.tmp / "later", self.keys[1], self.tmp / "b")

    def test_reseal_needs_an_empty_directory_and_manifest_must_match(self):
        rb.vault_seal(self.source, self.vault, self.members, "synthetic")
        with self.assertRaisesRegex(ValueError, "not empty"):
            rb.vault_seal(self.source, self.vault, self.members, "synthetic")
        index = json.loads((self.vault / rb.INDEX).read_text())
        index["files"].pop()
        (self.vault / rb.INDEX).write_text(json.dumps(index))
        with self.assertRaisesRegex(ValueError, "does not match the index"):
            rb.vault_open(self.vault, self.keys[0], self.tmp / "out")

    def test_legacy_v1_vault_still_opens(self):
        legacy = self.tmp / "legacy"
        legacy.mkdir()
        files = []
        for rel in ("notes.txt", "captures/synthetic.bin"):
            (legacy / rel).parent.mkdir(parents=True, exist_ok=True)
            rb._age_encrypt([self.members[0]["key"]], self.source / rel, legacy / f"{rel}.age")
            files.append({"path": rel, "sha256": rb._sha256(self.source / rel), "ciphertext": f"{rel}.age",
                          "ciphertext_sha256": rb._sha256(legacy / f"{rel}.age")})
        (legacy / rb.INDEX).write_text(json.dumps({"schema": "evo.rea-source-vault.v1", "files": files}))
        self.assertEqual(sorted(rb.vault_open(legacy, self.keys[0], self.tmp / "o")),
                         ["captures/synthetic.bin", "notes.txt"])

    def test_index_records_no_private_key_material(self):
        index = rb.vault_seal(self.source, self.vault, self.members, "synthetic")
        text = (self.vault / rb.INDEX).read_text()
        self.assertNotIn("AGE-SECRET-KEY", text)
        self.assertTrue(all(len(f) == 16 for f in index["recipients"]))
        self.assertEqual(index["vault"], "synthetic")
        self.assertEqual(sorted(m["role"] for m in index["members"]), ["edit", "view"])


if __name__ == "__main__":
    unittest.main()
