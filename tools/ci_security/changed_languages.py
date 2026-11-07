#!/usr/bin/env python3
"""Which languages a change touches, so each gets its own security check (EVO ADR-0049).

Prints GitHub Actions outputs:

- ``codeql``: a JSON list of CodeQL languages whose files changed;
- ``shell``: ``true`` when shell scripts changed;
- ``evo_native``: ``true`` when anything outside ``rea/`` changed, so EVO's own
  C build, test and analysis workflows must run (a ``rea/``-only change skips
  them; the ``rea/`` scope has its own checks);
- ``uncovered``: a JSON list of changed languages no check here covers yet;
- ``all``: ``true`` when every check runs because the change could not be
  computed (a manual run, a new branch, or a base commit that is missing).

``--list FILE`` also writes the changed paths, one per line, for the SARIF gate.
Standard library only; needs ``git``.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path, PurePosixPath

CODEQL = {
    "c-cpp": {".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".hxx", ".m", ".mm", ".inl"},
    "javascript-typescript": {".js", ".mjs", ".cjs", ".jsx", ".ts", ".mts", ".cts", ".tsx", ".vue", ".html"},
    "python": {".py", ".pyi"},
    "java-kotlin": {".java", ".kt", ".kts"},
    "csharp": {".cs"},
    "go": {".go"},
    "rust": {".rs"},
    "ruby": {".rb"},
}
SHELL = {".sh", ".bash"}
UNCOVERED = {".swift": "swift"}  # CodeQL Swift needs a macOS build; not run here
ZERO = "0" * 40


def language_flags(paths: list[str]) -> dict:
    codeql, uncovered = set(), set()
    shell = native = False
    for path in paths:
        pure = PurePosixPath(path)
        suffix = pure.suffix.lower()
        for language, suffixes in CODEQL.items():
            if suffix in suffixes:
                codeql.add(language)
        if path.startswith(".github/workflows/") and suffix in {".yml", ".yaml"} or (
                path.startswith(".github/actions/") and pure.name in {"action.yml", "action.yaml"}):
            codeql.add("actions")
        if suffix in SHELL:
            shell = True
        if suffix in UNCOVERED:
            uncovered.add(UNCOVERED[suffix])
        if not path.startswith("rea/"):
            native = True
    return {"codeql": sorted(codeql), "shell": shell, "evo_native": native, "uncovered": sorted(uncovered)}


def everything(cwd: Path) -> dict:
    """Every check for the languages the repository actually contains.

    CodeQL fails on a language with no source files, so a whole-tree run lists
    only languages present in ``git ls-files`` (all of them if that fails)."""
    done = subprocess.run(["git", "-C", str(cwd), "ls-files", "-z"], capture_output=True, text=True)
    if done.returncode != 0:
        return {"codeql": sorted([*CODEQL, "actions"]), "shell": True, "evo_native": True, "uncovered": []}
    flags = language_flags([p for p in done.stdout.split("\0") if p])
    flags["evo_native"] = True
    return flags


def changed_paths(base: str, head: str, cwd: Path) -> list[str] | None:
    """Paths changed from ``base`` to ``head``; None when that cannot be computed."""
    if not base or base == ZERO or not head:
        return None
    done = subprocess.run(["git", "-C", str(cwd), "diff", "--name-only", "-z", f"{base}...{head}"],
                          capture_output=True, text=True)
    if done.returncode != 0:
        done = subprocess.run(["git", "-C", str(cwd), "diff", "--name-only", "-z", base, head],
                              capture_output=True, text=True)
    if done.returncode != 0:
        return None
    return sorted(p for p in done.stdout.split("\0") if p)


def outputs(flags: dict, all_checks: bool) -> list[str]:
    return [f"codeql={json.dumps(flags['codeql'])}",
            f"shell={'true' if flags['shell'] else 'false'}",
            f"evo_native={'true' if flags['evo_native'] else 'false'}",
            f"uncovered={json.dumps(flags['uncovered'])}",
            f"all={'true' if all_checks else 'false'}"]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--event", required=True, help="github.event_name")
    parser.add_argument("--base", default="", help="base commit (pull_request base, or push before)")
    parser.add_argument("--head", default="", help="head commit")
    parser.add_argument("--repo", type=Path, default=Path("."))
    parser.add_argument("--list", type=Path, help="write the changed paths here")
    args = parser.parse_args(argv)
    paths = changed_paths(args.base, args.head, args.repo) if args.event in ("pull_request", "push") else None
    flags = everything(args.repo) if paths is None else language_flags(paths)
    if args.list:
        args.list.write_text("".join(f"{p}\n" for p in (paths or [])), encoding="utf-8")
    print("\n".join(outputs(flags, paths is None)))
    detail = "every check (change unknown)" if paths is None else f"{len(paths)} changed path(s)"
    print(f"{detail}: codeql {flags['codeql']}, shell {flags['shell']}, evo_native {flags['evo_native']}"
          + (f", not covered: {flags['uncovered']}" if flags["uncovered"] else ""), file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
