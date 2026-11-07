"""Language-matched security checks (tools/ci_security, EVO ADR-0049).

L1 each language's files select its CodeQL language; workflow files select actions; shell selects ShellCheck;
L2 a rea/-only change leaves evo_native false; anything outside rea/ sets it;
L3 a manual run, a new branch or a missing base runs every check;
L4 the CLI diffs a real repository and writes the changed-path list;
G1 a high result in a changed file is flagged: it blocks in block mode and only warns in report mode (the
   default); the same result in an unchanged file, or a medium one, is not flagged;
G2 a whole-tree run (no change list) never blocks;
G3 suppressed results are ignored; a failed analysis or a non-SARIF file is an error (exit 2);
G4 rules are found by index when ruleId is absent, and ./ prefixes are normalised.
"""
from __future__ import annotations

import io
import json
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import changed_languages as cl  # noqa: E402
import sarif_gate as sg  # noqa: E402


def sarif(results, rules=None, ok=True):
    rules = rules if rules is not None else [
        {"id": "js/sql", "properties": {"security-severity": "8.8"}},
        {"id": "js/info", "properties": {"security-severity": "4.0"}},
    ]
    return {"version": "2.1.0", "runs": [{"tool": {"driver": {"name": "CodeQL", "rules": rules}},
                                          "invocations": [{"executionSuccessful": ok}], "results": results}]}


def result(rule, path, line=3, **extra):
    return {"ruleId": rule, "message": {"text": f"{rule} here"},
            "locations": [{"physicalLocation": {"artifactLocation": {"uri": path}, "region": {"startLine": line}}}],
            **extra}


class ChangedLanguagesTests(unittest.TestCase):
    def test_l1_languages(self):
        flags = cl.language_flags(["src/a.c", "include/b.h", "rea/src/x.ts", "rea/scripts/y.mjs",
                                   "rea/bridge/z.py", "rea/third_party/binwalk/src/m.rs",
                                   "rea/bridge/ghidra/G.java", "rea/ci/node-checks.sh",
                                   ".github/workflows/ci.yml", "rea/bridge/native/U.swift", "README.md"])
        self.assertEqual(flags["codeql"], ["actions", "c-cpp", "java-kotlin", "javascript-typescript",
                                           "python", "rust"])
        self.assertTrue(flags["shell"])
        self.assertEqual(flags["uncovered"], ["swift"])
        self.assertEqual(cl.language_flags(["docs/x.md"])["codeql"], [])

    def test_l2_rea_only_skips_evo_native(self):
        self.assertFalse(cl.language_flags(["rea/src/a.ts", "rea/README.md"])["evo_native"])
        self.assertTrue(cl.language_flags(["rea/src/a.ts", "docs/adr/x.md"])["evo_native"])

    def test_l3_unknown_change_runs_everything(self):
        for event, base in (("workflow_dispatch", "abc"), ("push", cl.ZERO), ("pull_request", "")):
            out = io.StringIO()
            with redirect_stdout(out), redirect_stderr(io.StringIO()):
                cl.main(["--event", event, "--base", base, "--head", "HEAD"])
            lines = dict(line.split("=", 1) for line in out.getvalue().splitlines())
            self.assertEqual(lines["all"], "true", event)
            self.assertIn("c-cpp", json.loads(lines["codeql"]))
            self.assertEqual(lines["evo_native"], "true")

    def test_l4_cli_against_git(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            git = lambda *a: subprocess.run(["git", "-C", str(repo), "-c", "user.email=t@example.invalid",
                                             "-c", "user.name=T", *a], check=True, capture_output=True, text=True).stdout
            git("init", "-q", "-b", "main")
            (repo / "README.md").write_text("x\n")
            git("add", "."); git("commit", "-qm", "base")
            base = git("rev-parse", "HEAD").strip()
            (repo / "rea").mkdir()
            (repo / "rea" / "a.ts").write_text("x\n")
            git("add", "."); git("commit", "-qm", "ts")
            listing = repo / "changed.txt"
            out = io.StringIO()
            with redirect_stdout(out), redirect_stderr(io.StringIO()):
                cl.main(["--event", "pull_request", "--base", base, "--head", "HEAD", "--repo", str(repo),
                         "--list", str(listing)])
            lines = dict(line.split("=", 1) for line in out.getvalue().splitlines())
            self.assertEqual(json.loads(lines["codeql"]), ["javascript-typescript"])
            self.assertEqual((lines["evo_native"], lines["all"]), ("false", "false"))
            self.assertEqual(listing.read_text(), "rea/a.ts\n")


class SarifGateTests(unittest.TestCase):
    def run_gate(self, document, changed, mode="block"):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "x.sarif"
            path.write_text(json.dumps(document))
            argv = [str(path), "--mode", mode]
            if changed is not None:
                (Path(tmp) / "changed.txt").write_text("".join(f"{c}\n" for c in changed))
                argv += ["--changed", str(Path(tmp) / "changed.txt")]
            out = io.StringIO()
            with redirect_stdout(out), redirect_stderr(io.StringIO()):
                code = sg.main(argv)
            return code, out.getvalue()

    def test_g1_high_in_changed_file_blocks(self):
        doc = sarif([result("js/sql", "rea/src/a.ts"), result("js/info", "rea/src/b.ts"),
                     result("js/sql", "rea/src/old.ts")])
        code, out = self.run_gate(doc, ["rea/src/a.ts", "rea/src/b.ts"])
        self.assertEqual(code, 1)
        self.assertIn("::error file=rea/src/a.ts,line=3::js/sql", out)
        self.assertNotIn("old.ts,line", out)
        self.assertEqual(self.run_gate(doc, ["rea/src/b.ts"])[0], 0)
        code, out = self.run_gate(doc, ["rea/src/a.ts"], mode="report")
        self.assertEqual(code, 0)
        self.assertIn("::warning file=rea/src/a.ts,line=3::js/sql", out)
        self.assertIn("report-only until promoted", out)

    def test_g2_whole_tree_never_blocks(self):
        code, out = self.run_gate(sarif([result("js/sql", "rea/src/a.ts")]), None)
        self.assertEqual(code, 0)
        self.assertIn("Whole-tree report: 1 result", out)

    def test_g3_suppressed_failed_and_invalid(self):
        doc = sarif([result("js/sql", "a.ts", suppressions=[{"kind": "inSource"}])])
        self.assertEqual(self.run_gate(doc, ["a.ts"])[0], 0)
        self.assertEqual(self.run_gate(sarif([], ok=False), ["a.ts"])[0], 2)
        self.assertEqual(self.run_gate(sarif([], ok=False), ["a.ts"], mode="report")[0], 2)
        self.assertEqual(self.run_gate({"version": "1.0"}, ["a.ts"])[0], 2)

    def test_g4_rule_index_and_prefixes(self):
        res = result(None, "./.github/workflows/ci.yml")
        res["rule"] = {"index": 0}
        found = sg.results(sarif([res], rules=[{"id": "actions/x", "properties": {"security-severity": "9.1"}}]))
        self.assertEqual((found[0]["path"], found[0]["rule"], found[0]["severity"]),
                         (".github/workflows/ci.yml", "actions/x", 9.1))


if __name__ == "__main__":
    unittest.main()
