#!/usr/bin/env python3
"""Fail on high or critical security results in the files a change touches (EVO ADR-0049).

Reads CodeQL SARIF files and the changed paths (one per line, from
changed_languages.py --list). A result blocks when it is in a changed file and
its rule's ``security-severity`` is at or above the threshold (7.0: high and
critical). Results elsewhere, and lower severities, are reported but do not
block: they stay visible in code scanning without making every change pay for
existing code.

With no changed-path list (a manual or scheduled run) nothing blocks: the run
is a report of the whole tree. A SARIF file that records a failed analysis
always blocks, so a broken analysis never reads as clean.

Writes a Markdown summary to --summary (e.g. $GITHUB_STEP_SUMMARY).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

THRESHOLD = 7.0


def _severity(rule: dict) -> float | None:
    value = (rule.get("properties") or {}).get("security-severity")
    try:
        return float(value) if value is not None else None
    except (TypeError, ValueError):
        return None


def _relative(uri: str) -> str:
    path = uri.removeprefix("file://")
    while path.startswith("./"):
        path = path[2:]
    return path


def results(document: dict) -> list[dict]:
    """Flatten SARIF results with path, line, rule, message and security severity."""
    found = []
    runs = document.get("runs")
    if document.get("version") != "2.1.0" or not isinstance(runs, list) or not runs:
        raise ValueError("not a SARIF 2.1.0 document with runs")
    for run in runs:
        invocations = run.get("invocations") or []
        if any(item.get("executionSuccessful") is False for item in invocations):
            raise ValueError("SARIF records a failed analysis")
        driver = (run.get("tool") or {}).get("driver") or {}
        rules = {rule.get("id"): rule for rule in driver.get("rules") or []}
        by_index = list(driver.get("rules") or [])
        for result in run.get("results") or []:
            if result.get("suppressions"):
                continue
            rule = rules.get(result.get("ruleId"))
            index = (result.get("rule") or {}).get("index")
            if rule is None and isinstance(index, int) and 0 <= index < len(by_index):
                rule = by_index[index]
            location = ((result.get("locations") or [{}])[0].get("physicalLocation") or {})
            uri = (location.get("artifactLocation") or {}).get("uri", "")
            found.append({
                "path": _relative(uri),
                "line": (location.get("region") or {}).get("startLine"),
                "rule": result.get("ruleId") or (rule or {}).get("id") or "?",
                "message": ((result.get("message") or {}).get("text") or "").splitlines()[0][:160],
                "severity": _severity(rule or {}),
            })
    return found


def gate(found: list[dict], changed: set[str] | None, threshold: float = THRESHOLD) -> list[dict]:
    if changed is None:
        return []
    return [r for r in found if r["path"] in changed and r["severity"] is not None and r["severity"] >= threshold]


def summary(found: list[dict], blocking: list[dict], changed: set[str] | None, threshold: float) -> str:
    lines = ["## Security analysis", ""]
    if changed is None:
        lines.append(f"Whole-tree report: {len(found)} result(s); nothing blocks on a run without a change list.")
    else:
        touched = [r for r in found if r["path"] in changed]
        lines.append(f"{len(found)} result(s) in total, {len(touched)} in the {len(changed)} changed file(s); "
                     f"{len(blocking)} at security severity {threshold} or higher block this change.")
    shown = blocking or [r for r in found if changed is not None and r["path"] in changed][:50]
    if shown:
        lines += ["", "| Severity | File | Line | Rule | Message |", "| --- | --- | --- | --- | --- |"]
        for r in shown:
            sev = "" if r["severity"] is None else f"{r['severity']:.1f}"
            msg = r["message"].replace("|", "\\|")
            lines.append(f"| {sev} | `{r['path']}` | {r['line'] or ''} | `{r['rule']}` | {msg} |")
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("sarif", nargs="+", type=Path)
    parser.add_argument("--changed", type=Path, help="changed paths, one per line; omit for a whole-tree report")
    parser.add_argument("--threshold", type=float, default=THRESHOLD)
    parser.add_argument("--summary", type=Path)
    args = parser.parse_args(argv)
    try:
        found = []
        for path in args.sarif:
            found += results(json.loads(path.read_text(encoding="utf-8")))
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    changed = None
    if args.changed is not None:
        changed = {line.strip() for line in args.changed.read_text(encoding="utf-8").splitlines() if line.strip()}
    blocking = gate(found, changed, args.threshold)
    text = summary(found, blocking, changed, args.threshold)
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as stream:
            stream.write(text)
    print(text)
    for r in blocking:
        print(f"::error file={r['path']},line={r['line'] or 1}::{r['rule']} (security severity "
              f"{r['severity']:.1f}): {r['message']}")
    return 1 if blocking else 0


if __name__ == "__main__":
    raise SystemExit(main())
