#!/usr/bin/env python3
"""Reproduce the read-only REA source baseline; never import target code."""
from __future__ import annotations

import argparse
import ast
import collections
from concurrent.futures import ThreadPoolExecutor
import csv
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

PIN = "9e22da845bf8b447f81539e0ba57d5deb59e9103"
AEMS = "e620f4bd3cc45f05f2f18b61e3a087303eb2b4bd"
SCANNER_DIGESTS = {
    "aes_sec_001_scan.py": "2af99be2c49dbdc384cbe6642ce9c37d02503280915e3408aff70563983358a8",
    "aes_sec_002_scan.py": "89f6b9529c2bd292c24c101ed5e54160739e9f622331082d5d2171973e9bae0c",
}
MAX_FILE_BYTES = 20_000_000
CHILD_ENV = {"PATH": os.environ["PATH"], "LANG": "C.UTF-8", "PYTHONDONTWRITEBYTECODE": "1"}


def command(argv, *, timeout=30):
    return subprocess.run(argv, env=CHILD_ENV, capture_output=True, timeout=timeout, check=False)


def load_scanner(directory, filename):
    path = directory / filename
    assert hashlib.sha256(path.read_bytes()).hexdigest() == SCANNER_DIGESTS[filename]
    name = "baseline_" + path.stem
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def git(root, *args):
    result = command(["git", "-C", str(root), *args])
    assert result.returncode == 0, "Git read failed"
    return result.stdout


def write_json(path, value):
    path.write_text(json.dumps(value, sort_keys=True, indent=2) + "\n")


def category(path):
    if path.startswith("tests/fixtures/") or "/fixtures/" in path:
        return "fixture"
    if path.startswith("third_party/"):
        return "third-party"
    if path.startswith("tests/"):
        return "test"
    if path.startswith("docs/") or Path(path).suffix.lower() in {".md", ".rst", ".txt"}:
        return "documentation"
    if path.startswith(".github/"):
        return "ci"
    return "source-or-configuration"


def sec1_coverage(path, module):
    p = Path(path)
    if any(part in module.DEFAULT_EXCLUDED_DIRS for part in p.parts[:-1]):
        return "excluded-directory"
    if any(path == prefix or path.startswith(prefix + "/") for prefix in module.DEFAULT_EXCLUDED_PATH_PREFIXES):
        return "excluded-fixture"
    if p.name in module.GENERATED_REPORT_NAMES:
        return "excluded-generated-report"
    return "native-api" if p.suffix in module.NATIVE_SOURCE_EXTENSIONS else "signals-only"


def sec2_coverage(path, size, module):
    p = Path(path)
    if any(part in module.EXCLUDED_DIRECTORIES for part in p.parts[:-1]):
        return "excluded-directory"
    if p.name not in module.TEXT_NAMES and p.suffix.lower() not in module.TEXT_EXTENSIONS:
        return "unsupported-extension"
    return "text-signals" if size <= module.TEXT_LIMIT else "size-limit"


EFFECTS = {
    "process": r"\b(?:spawn|spawnSync|execFile|execFileSync|execSync|subprocess|Popen|ProcessBuilder|posix_spawn|CreateProcess)\b",
    "network": r"\b(?:fetch|WebSocket|XMLHttpRequest|urllib|requests|https?\.request|connect)\b|https?://",
    "dynamic-code": r"\beval\s*\(|\bnew\s+Function\s*\(",
    "filesystem-write": r"\b(?:writeFile|writeFileSync|writeFileAtomic|createWriteStream|unlink|rmdir|rmSync|chmod)\b",
}
TOKEN_PATTERNS = {
    "private-key-marker": r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----",
    "github-token-shaped": r"\bgh[pousr]_[A-Za-z0-9]{36,255}\b",
    "aws-access-key-shaped": r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--aems-scripts", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root, output = args.root.resolve(), args.output.resolve()
    assert root not in output.parents and root != output, "Outputs must be outside source"
    assert git(root, "rev-parse", "HEAD").decode().strip() == PIN
    assert not git(root, "status", "--porcelain=v1"), "Input checkout must be clean"
    m1 = load_scanner(args.aems_scripts.resolve(), "aes_sec_001_scan.py")
    m2 = load_scanner(args.aems_scripts.resolve(), "aes_sec_002_scan.py")
    output.mkdir(parents=True, exist_ok=True)
    rows, syntax, effect_hits, secret_candidates, submodules, lfs = [], [], [], [], [], []
    native_findings, typed_jobs = [], []
    expressions = {key: re.compile(value) for key, value in EFFECTS.items()}
    token_expressions = {key: re.compile(value) for key, value in TOKEN_PATTERNS.items()}
    entries = []
    for raw in git(root, "ls-tree", "-rlz", "--full-tree", "HEAD").split(b"\0"):
        if not raw:
            continue
        metadata, name = raw.split(b"\t", 1)
        mode, kind, oid, size = metadata.decode().split()
        entries.append((name.decode(), mode, kind, oid, None if size == "-" else int(size)))
    for name, mode, kind, oid, size in sorted(entries):
        row = {"path": name, "kind": kind, "mode": mode, "git_oid": oid,
               "bytes": size, "sha256": "", "encoding": "not-applicable",
               "category": category(name), "sec001_default": "not-applicable",
               "sec002_default": "not-applicable", "native_overlay": "not-applicable",
               "syntax": "not-applicable", "effects": "", "token_candidates": 0}
        if kind == "commit":
            submodules.append({"path": name, "commit": oid, "status": "referenced-content-not-scanned"})
            row["syntax"] = "dependency-content-unresolved"
            rows.append(row)
            continue
        assert kind == "blob" and mode in {"100644", "100755"}, "Unsupported tracked object"
        path = root / name
        assert not path.is_symlink() and path.is_file() and size <= MAX_FILE_BYTES
        value = path.read_bytes()
        assert len(value) == size
        assert hashlib.sha1(b"blob " + str(size).encode() + b"\0" + value).hexdigest() == oid, name
        row["sha256"] = hashlib.sha256(value).hexdigest()
        try:
            text = value.decode("utf-8")
            row["encoding"] = "utf-8" if "\0" not in text else "binary-nul"
        except UnicodeDecodeError:
            text = None
            row["encoding"] = "binary-or-non-utf8"
        row["sec001_default"] = sec1_coverage(name, m1)
        row["sec002_default"] = sec2_coverage(name, size, m2)
        if path.suffix in m1.NATIVE_SOURCE_EXTENSIONS:
            assert text is not None and size <= m1.TEXT_FILE_LIMIT_BYTES
            row["native_overlay"] = "scanned"
            for finding in m1.scan_source_for_apis(root, path, True):
                native_findings.append({k: v for k, v in finding.__dict__.items() if k != "text"})
        if text is not None and row["encoding"] == "utf-8":
            tags = []
            for tag, expression in expressions.items():
                matches = list(expression.finditer(text))
                if matches:
                    tags.append(tag)
                    effect_hits.append({"path": name, "category": row["category"], "signal": tag,
                                        "count": len(matches), "lines": sorted({text.count("\n", 0, x.start()) + 1 for x in matches})})
            row["effects"] = ";".join(tags)
            for label, expression in token_expressions.items():
                for match in expression.finditer(text):
                    row["token_candidates"] += 1
                    secret_candidates.append({"path": name, "line": text.count("\n", 0, match.start()) + 1,
                                              "pattern": label, "status": "unverified-shape-candidate",
                                              "value": "not-retained"})
            if text.startswith("version https://git-lfs.github.com/spec/v1\n"):
                lfs.append({"path": name, "status": "lfs-payload-not-scanned"})
            check, status = None, "not-applicable"
            try:
                if path.suffix == ".json":
                    check = "python-json-syntax"
                    json.loads(text)
                    status = "pass"
                elif path.suffix == ".py":
                    check = "python-ast-parse"
                    ast.parse(text, filename=name)
                    status = "pass"
                elif path.suffix in {".js", ".mjs", ".cjs"}:
                    check = "node-check"
                    status = "pass" if command(["node", "--check", str(path)]).returncode == 0 else "parse-review"
                elif path.suffix in {".sh", ".bash"}:
                    check = "bash-noexec-parse"
                    status = "pass" if command(["bash", "-n", str(path)]).returncode == 0 else "parse-review"
            except (SyntaxError, ValueError, RecursionError):
                status = "parse-review"
            if check:
                syntax.append({"path": name, "category": row["category"], "check": check, "status": status})
                row["syntax"] = check + ":" + status
            elif path.suffix in {".ts", ".cts", ".mts"}:
                row["syntax"] = "queued-node-typescript-syntax"
                typed_jobs.append((row, path))
            elif path.suffix in {".tsx", ".jsx"}:
                row["syntax"] = "typed-language-analysis-not-run"
        else:
            row["syntax"] = "binary-content-semantic-analysis-not-run"
        rows.append(row)

    def check_typed(job):
        row, path = job
        result = command(["node", "--experimental-transform-types", "--check", str(path)])
        return row, "pass" if result.returncode == 0 else "parse-review"
    with ThreadPoolExecutor(max_workers=4) as pool:
        for row, status in pool.map(check_typed, typed_jobs):
            row["syntax"] = "node-typescript-syntax:" + status
            syntax.append({"path": row["path"], "category": row["category"],
                           "check": "node-typescript-syntax", "status": status})
    syntax.sort(key=lambda item: item["path"])
    sec1 = m1.scan(root, "dlworrell/rea", True).to_dict()
    sec2 = m2.scan(root, repository="dlworrell/rea", entry={
        "applicability": "in-scope", "rationale": "Diagnostic inclusion only, not governance enrollment: REA remains external-reference/excluded.",
        "platforms": ["linux", "macos", "windows"],
        "languages": ["TypeScript", "JavaScript", "Python", "Java", "Swift", "C", "C++"],
    }).to_dict()
    sec1["root"] = sec2["root"] = "<pinned-REA-checkout>"
    for finding in sec1["findings"]:
        finding.pop("text", None)
    wrapper = {"subject_commit": PIN, "governance_classification": "external-reference",
               "governance_default_branch_policy": "excluded", "mode": "diagnostic-report-only",
               "aems_commit": AEMS, "scanner_sha256": SCANNER_DIGESTS,
               "note": "Scanner adoption/absence results are signals, not obligations imposed on this external reference."}
    write_json(output / "aems-sec001.json", {**wrapper, "scanner_result": sec1})
    write_json(output / "aems-sec002.json", {**wrapper, "scanner_result": sec2})
    write_json(output / "findings.json", {**wrapper, "all_native_api_candidates": native_findings,
               "syntax_checks": syntax, "effect_signals": effect_hits, "token_shape_candidates": secret_candidates,
               "interpretation": "Candidates/signals require disposition; test fixtures may intentionally fail parsing. No vulnerability or secret validity is asserted."})

    package = json.loads((root / "package.json").read_text())
    lock = json.loads((root / "package-lock.json").read_text())
    packages = lock.get("packages", {})
    dependencies = []
    for path, entry in sorted(packages.items()):
        if not path:
            continue
        dependencies.append({"path": path, "version": entry.get("version"),
            "resolved": entry.get("resolved"), "integrity": entry.get("integrity"),
            "license": entry.get("license"), "hasInstallScript": entry.get("hasInstallScript", False),
            "optional": entry.get("optional", False), "dev": entry.get("dev", False)})
    gitmodules = (root / ".gitmodules").read_text()
    module_urls = dict(re.findall(r"path\s*=\s*([^\n]+)\n\s*url\s*=\s*([^\n]+)", gitmodules))
    for entry in submodules:
        entry["url"] = module_urls.get(entry["path"])
    workflows = []
    for path in sorted((root / ".github/workflows").glob("*")):
        if not path.is_file():
            continue
        for line, text in enumerate(path.read_text().splitlines(), 1):
            match = re.search(r"\buses:\s*[\"']?([^\s\"'#]+)", text)
            if match:
                use = match.group(1)
                workflows.append({"path": path.relative_to(root).as_posix(), "line": line, "uses": use,
                    "immutable_commit_ref": bool(re.search(r"@[a-fA-F0-9]{40}$", use)),
                    "kind": "local" if use.startswith("./") else "container" if use.startswith("docker://") else "external-action"})
    write_json(output / "dependencies.json", {"subject_commit": PIN, "package_name": package["name"],
        "version": package["version"], "declared_license": package.get("license"),
        "node_engines": package.get("engines"), "package_manager": package.get("packageManager"),
        "direct_dependencies": package.get("dependencies", {}), "dev_dependencies": package.get("devDependencies", {}),
        "root_lifecycle_hooks": {k: package["scripts"][k] for k in ("prepare", "prepack", "prepublishOnly") if k in package["scripts"]},
        "lockfile_version": lock.get("lockfileVersion"), "locked_packages": dependencies,
        "submodules": submodules, "lfs_pointers": lfs, "workflow_uses": workflows,
        "vulnerability_database_scan": "not-run", "transitive_license_approval": "not-established"})
    fields = list(rows[0])
    with (output / "file-registry.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    assert len({row["path"] for row in rows}) == len(entries)
    assert not git(root, "status", "--porcelain=v1"), "Scan mutated its input"
    for row in rows:
        if row["kind"] == "blob":
            assert hashlib.sha256((root / row["path"]).read_bytes()).hexdigest() == row["sha256"], "Source changed during scan"
    summary = {**wrapper, "tree": git(root, "rev-parse", "HEAD^{tree}").decode().strip(),
        "tracked_entries": len(rows), "verified_blobs": sum(row["kind"] == "blob" for row in rows),
        "gitlinks": len(submodules), "total_blob_bytes": sum(row["bytes"] or 0 for row in rows),
        "encoding_counts": dict(collections.Counter(row["encoding"] for row in rows)),
        "category_counts": dict(collections.Counter(row["category"] for row in rows)),
        "syntax_counts": dict(collections.Counter(item["status"] for item in syntax)),
        "sec001_default_coverage": dict(collections.Counter(row["sec001_default"] for row in rows)),
        "sec002_default_coverage": dict(collections.Counter(row["sec002_default"] for row in rows)),
        "all_native_files_scanned": sum(row["native_overlay"] == "scanned" for row in rows),
        "native_api_candidates": dict(collections.Counter(f["severity"] for f in native_findings)),
        "token_shape_candidates": len(secret_candidates), "locked_package_count": len(dependencies),
        "package_install_script_count": sum(x["hasInstallScript"] for x in dependencies),
        "locked_packages_without_integrity": sum(not x["integrity"] for x in dependencies),
        "external_action_references": sum(x["kind"] == "external-action" for x in workflows),
        "external_action_refs_without_full_commit": sum(x["kind"] == "external-action" and not x["immutable_commit_ref"] for x in workflows),
        "method_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "tools": {"python": sys.version.split()[0], "node": command(["node", "--version"]).stdout.decode().strip(),
                  "git": command(["git", "--version"]).stdout.decode().strip()},
        "source_unchanged": True, "dependency_closure_complete": False,
        "admission": "blocked-pending-applicable-review-and-unrun-checks",
        "not_run": ["submodule contents", "full dependency vulnerability and license review", "TypeScript semantic/type analysis",
                    "CodeQL/Semgrep", "full secret/history scan", "build/tests", "REA provider execution", "installed EVO integration", "hardware validation"]}
    write_json(output / "summary.json", summary)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
