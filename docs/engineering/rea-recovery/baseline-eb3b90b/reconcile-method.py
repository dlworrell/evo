#!/usr/bin/env python3
"""Verify both complete tracked snapshots and reconcile their retained scans.

Reads target Git blobs, never target application modules. Output must already
contain the full scan, connector source evidence and bounded registry evidence.
"""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess

OLD = "9e22da845bf8b447f81539e0ba57d5deb59e9103"
NEW = "eb3b90beb4ac9245bac415696aa0de5a1a66c6d2"


def git(root, *args, input=None):
    return subprocess.check_output(["git", "-C", str(root), *args], input=input, timeout=60)


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def registry(path):
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    assert len({r["path"] for r in rows}) == len(rows), "Duplicate registry paths"
    return {r["path"]: r for r in rows}


def tree(root, pin):
    result = {}
    for raw in git(root, "ls-tree", "-rlz", "--full-tree", pin).split(b"\0"):
        if not raw:
            continue
        fields, name = raw.split(b"\t", 1)
        mode, kind, oid, size = fields.decode().split()
        result[name.decode()] = {"mode": mode, "kind": kind, "git_oid": oid,
                                  "bytes": "" if size == "-" else size}
    return result


def verify(root, pin, rows):
    entries = tree(root, pin)
    assert entries.keys() == rows.keys(), "Git/registry path mismatch"
    blobs = []
    for path, entry in entries.items():
        assert all(rows[path][k] == value for k, value in entry.items()), path
        if entry["kind"] == "blob":
            blobs.append((path, entry))
        else:
            assert entry["kind"] == "commit" and not rows[path]["sha256"]
    packed = git(root, "cat-file", "--batch", input="".join(e["git_oid"] + "\n" for _, e in blobs).encode())
    stream = io.BytesIO(packed)
    for path, entry in blobs:
        oid, kind, size = stream.readline().decode().split()
        assert oid == entry["git_oid"] and kind == "blob" and size == entry["bytes"]
        value = stream.read(int(size))
        assert stream.read(1) == b"\n"
        assert hashlib.sha1(b"blob " + size.encode() + b"\0" + value).hexdigest() == oid
        assert hashlib.sha256(value).hexdigest() == rows[path]["sha256"], path
    assert not stream.read()
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root, original, output = args.root.resolve(), args.original.resolve(), args.output.resolve()
    assert root != output and root not in output.parents and original != output
    assert git(root, "rev-parse", "HEAD").decode().strip() == NEW
    assert not git(root, "status", "--porcelain=v1")
    manifest_entries = []
    for line in (original / "checksums.sha256").read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        assert Path(name).name == name and name != "checksums.sha256"
        assert hashlib.sha256((original / name).read_bytes()).hexdigest() == digest
        manifest_entries.append(name)
    assert set(manifest_entries) == {p.name for p in original.iterdir() if p.is_file()} - {"checksums.sha256"}
    before, after = registry(original / "file-registry.csv"), registry(output / "file-registry.csv")
    old_tree, new_tree = verify(root, OLD, before), verify(root, NEW, after)
    api = read(output / "github-source-evidence.json")
    connector = api["tree_response"]
    assert connector["truncated"] is False
    assert connector["sha"] == git(root, "rev-parse", NEW + "^{tree}").decode().strip()
    fetched = {e["path"]: {"mode": e["mode"], "kind": e["type"], "git_oid": e["sha"],
                            "bytes": str(e["size"]) if e["type"] == "blob" else ""}
               for e in connector["tree"] if e["type"] != "tree"}
    assert fetched == new_tree, "Connector/Git complete tree mismatch"
    assert api["source_commit"]["sha"] == NEW
    assert api["source_commit"]["verification"]["verified"] is True
    assert api["source_commit"]["verification"]["reason"] == "valid"
    delta, modes, kinds, content = [], [], [], []
    for path in sorted(before.keys() | after.keys()):
        a, b = before.get(path, {}), after.get(path, {})
        status = "added" if not a else "removed" if not b else "unchanged"
        if a and b:
            if a["mode"] != b["mode"]:
                modes.append(path)
            if a["kind"] != b["kind"]:
                kinds.append(path)
            if a["git_oid"] != b["git_oid"]:
                content.append(path)
            if path in modes or path in kinds or path in content:
                status = "changed"
        row = {"path": path, "status": status}
        for prefix, item in [("old", a), ("new", b)]:
            row.update({prefix + "_" + k: item.get(k, "") for k in ["kind", "mode", "git_oid", "bytes", "sha256"]})
        delta.append(row)
    with (output / "file-delta.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(delta[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(delta)
    raw_changes = git(root, "diff", "--name-status", "--find-renames", OLD, NEW, "--").decode().splitlines()
    renames = [line.split("\t") for line in raw_changes if line.startswith("R")]
    stats = [{"path": name, "additions": int(added), "deletions": int(deleted)}
             for added, deleted, name in (line.split("\t", 2) for line in
               git(root, "diff", "--numstat", OLD, NEW, "--").decode().splitlines())]
    assert sorted(content) == ["package-lock.json", "package.json"]
    assert {x["filename"] for x in api["comparison"]["files"]} == set(content)
    for row in stats:
        other = next(x for x in api["comparison"]["files"] if x["filename"] == row["path"])
        assert (row["additions"], row["deletions"]) == (other["additions"], other["deletions"])
    old_summary, new_summary = read(original / "summary.json"), read(output / "summary.json")
    assert old_summary["tools"] == new_summary["tools"]
    assert old_summary["scanner_sha256"] == new_summary["scanner_sha256"]
    assert old_summary["aems_commit"] == new_summary["aems_commit"]
    assert (original / "scan-method.py").read_text().replace(OLD, NEW) == (output / "scan-method.py").read_text()
    old_findings, new_findings = read(original / "findings.json"), read(output / "findings.json")
    equality = {k: old_findings[k] == new_findings[k] for k in
                ["all_native_api_candidates", "syntax_checks", "token_shape_candidates"]}
    assert all(equality.values())
    old_sec1, new_sec1 = read(original / "aems-sec001.json")["scanner_result"], read(output / "aems-sec001.json")["scanner_result"]
    old_sec2, new_sec2 = read(original / "aems-sec002.json")["scanner_result"], read(output / "aems-sec002.json")["scanner_result"]
    assert old_sec1 == new_sec1
    assert old_sec2["source_revision"] == OLD and new_sec2["source_revision"] == NEW
    old_sec2.pop("source_revision"); new_sec2.pop("source_revision")
    assert old_sec2 == new_sec2
    old_deps, new_deps = read(original / "dependencies.json"), read(output / "dependencies.json")
    unchanged_dependencies = {k: old_deps[k] == new_deps[k] for k in
                              ["submodules", "lfs_pointers", "workflow_uses", "root_lifecycle_hooks",
                               "node_engines", "package_manager", "declared_license", "dev_dependencies"]}
    assert all(unchanged_dependencies.values())
    effect_before = {json.dumps(x, sort_keys=True) for x in old_findings["effect_signals"]}
    effect_after = {json.dumps(x, sort_keys=True) for x in new_findings["effect_signals"]}
    changes_in_effects = {"removed": [json.loads(x) for x in sorted(effect_before - effect_after)],
                         "added": [json.loads(x) for x in sorted(effect_after - effect_before)]}
    assert all(x["path"] == "package-lock.json" and x["signal"] == "network"
               for values in changes_in_effects.values() for x in values)
    a = json.loads(git(root, "show", OLD + ":package-lock.json"))["packages"]
    b = json.loads(git(root, "show", NEW + ":package-lock.json"))["packages"]
    dependency_delta = []
    for path in sorted(a.keys() | b.keys()):
        if a.get(path) == b.get(path):
            continue
        x, y = a.get(path, {}), b.get(path, {})
        dependency_delta.append({"path": path, "status": "added" if not x else "removed" if not y else "changed",
                                 "changed_fields": sorted(k for k in x.keys() | y.keys() if x.get(k) != y.get(k)),
                                 "before": x, "after": y})
    write(output / "dependency-delta.json", {"original_commit": OLD, "subject_commit": NEW,
          "before_locked_packages": len(a) - 1, "after_locked_packages": len(b) - 1,
          "counts_including_root": dict(collections.Counter(x["status"] for x in dependency_delta)),
          "entries": dependency_delta,
          "scope": "complete raw lock entry comparison, including dependency, peer, platform, license and integrity metadata"})
    audits = {stage: read(output / ("npm-audit-" + stage + ".json")) for stage in ["before", "after"]}
    assert all("auditReportVersion" in item and "error" not in item for item in audits.values())
    advisories = {entry["url"] for item in audits["before"]["vulnerabilities"].values()
                  for entry in item["via"] if isinstance(entry, dict)}
    assert not audits["after"]["vulnerabilities"]
    report = {"original_commit": OLD, "subject_commit": NEW, "original_tree": old_summary["tree"],
              "subject_tree": new_summary["tree"], "original_baseline_integrity": "all manifested bytes verified; no baseline modifications",
              "method_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "snapshots_reverified_from_git_blobs": {"before": len(before), "after": len(after)},
              "connector_tree_matches_complete_git_tree": True,
              "file_delta_counts": dict(collections.Counter(row["status"] for row in delta)),
              "added": [r["path"] for r in delta if r["status"] == "added"],
              "removed": [r["path"] for r in delta if r["status"] == "removed"],
              "renamed": renames, "mode_changed": modes, "object_type_changed": kinds, "content_changed": content,
              "git_numstat": stats, "source_commits_ahead": int(git(root, "rev-list", "--count", OLD + ".." + NEW)),
              "tools_and_scanners_identical": True, "scan_method_change": "subject PIN only",
              "retained_finding_arrays_identical": equality, "sec001_result_identical": True,
              "sec002_result_identical_except_subject_revision": True,
              "unchanged_dependency_surfaces": unchanged_dependencies, "effect_signal_changes": changes_in_effects,
              "npm_audit": {stage: {"metadata": item["metadata"], "vulnerable_packages": sorted(item["vulnerabilities"])}
                            for stage, item in audits.items()},
              "original_advisory_count": len(advisories), "original_advisory_urls": sorted(advisories),
              "new_vulnerability_claim": "zero known findings in this dated registry query; no absence-of-vulnerabilities proof",
              "source_unchanged": not bool(git(root, "status", "--porcelain=v1")),
              "independent_admission_review": "not-established", "classification": "external-reference/excluded",
              "admission": "blocked-pending-applicable-review-and-unrun-checks"}
    write(output / "comparison.json", report)
    # Reconcile the collector's deliberately narrow dependency status with the
    # separately retained audit. The original baseline remains byte-for-byte.
    new_deps["vulnerability_database_scan"] = {
        "method": "npm audit --package-lock-only --ignore-scripts --json",
        "observed_date": "2026-10-27", "report": "npm-audit-after.json",
        "execution": "npm-audit-execution.json", "known_findings": 0,
        "interpretation": "dated registry lookup, not proof of absence or full security approval"}
    write(output / "dependencies.json", new_deps)
    new_summary["not_run"] = ["dependency provenance verification and comprehensive license approval"
                              if item == "full dependency vulnerability and license review" else item
                              for item in new_summary["not_run"]]
    new_summary["dependency_security_supplement"] = {
        "comparison": "comparison.json", "audit": "npm-audit-after.json",
        "registry_and_archive_evidence": "registry-evidence.json",
        "metadata_compatibility": "compatibility.json", "assessment": "dependency-assessment.json",
        "reconciliation_method_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "license_approval": "not-established", "admission": "blocked"}
    write(output / "summary.json", new_summary)
    print(json.dumps({"files": report["file_delta_counts"], "mode_changed": modes, "renamed": renames,
                      "lock_entries": len(b) - 1, "changed_or_added_nonroot_lock_entries": len(dependency_delta) - 1,
                      "original_advisories": len(advisories), "current_known_vulnerabilities": 0}, indent=2))


if __name__ == "__main__":
    main()
