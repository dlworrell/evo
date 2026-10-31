#!/usr/bin/env python3
"""Companion manifest v0.1 validator for REA-assisted recovery (EVO-004 section 3).

Standard library only. It checks a companion manifest against the retained
artifacts it references and refuses anything EVO-004 stage E1 says must be
refused: corrupt or unknown Evidence IDs, broken references, unsupported
contract versions, a subject (REA pin) mismatch, rewritten native bundles and
claims the evidence cannot support. It never runs REA and never modifies an
artifact. Acceptance means the manifest is internally consistent with the bytes
it names; it does not make any compatibility, measurement or safety claim true.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys

CONTRACT = "evo.rea-companion-manifest"
SUPPORTED_VERSIONS = ("0.1",)
CONTRACT_PIN = "9e22da845bf8b447f81539e0ba57d5deb59e9103"
NATIVE_ARRAYS = ("artifacts", "providers", "environments", "scenarios",
                 "captures", "unknowns", "records")
STAGES = ("E1", "C1", "C2", "A1", "O1", "P1", "H1")
ROLES = ("native-bundle", "capture", "manual", "specification", "calibration",
         "implementation", "test", "derivative", "report")
ACCESS = ("public", "restricted")
DISPOSITIONS = ("pass", "fail", "indeterminate", "not-run")
CALIBRATION = ("known", "unknown", "not-applicable")
TOP_LEVEL = {"contract", "version", "manifest_id", "revision", "owner",
             "created_by", "stage", "access_scope", "synthetic", "rea",
             "artifacts", "subject", "interface", "calibration", "claims",
             "unknowns"}
SHA = re.compile(r"[0-9a-f]{64}\Z")
COMMIT = re.compile(r"[0-9a-f]{40}\Z")


class ManifestError(ValueError):
    pass


def _safe_locator(value: object) -> bool:
    if not isinstance(value, str) or not value or "\\" in value:
        return False
    path = PurePosixPath(value)
    return not path.is_absolute() and ".." not in path.parts and str(path) == value


def _nonempty(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _check_bundle(data: bytes, locator: str, wanted: list) -> list[str]:
    """A native bundle must parse, keep its seven arrays and contain every listed ID."""
    try:
        bundle = json.loads(data)
    except (ValueError, UnicodeDecodeError):
        return [f"native bundle is not JSON: {locator}"]
    if not isinstance(bundle, dict) or set(bundle) != set(NATIVE_ARRAYS) or not all(
            isinstance(bundle[key], list) for key in NATIVE_ARRAYS):
        return [f"native bundle does not have exactly the seven native arrays: {locator}"]
    ids = [r.get("evidence_id") for r in bundle["records"] if isinstance(r, dict)]
    if len(ids) != len(bundle["records"]) or not all(_nonempty(i) for i in ids):
        return [f"native bundle record without an evidence_id: {locator}"]
    problems = [f"duplicate evidence_id in native bundle {locator}: {i}"
                for i in sorted({i for i in ids if ids.count(i) > 1})]
    problems += [f"evidence_id not in native bundle {locator}: {i}"
                 for i in sorted(set(wanted) - set(ids))]
    return problems


def validate(manifest: object, store: Path, pin: str = CONTRACT_PIN) -> dict:
    """Validate ``manifest`` against artifacts under ``store`` (read-only)."""
    problems: list[str] = []
    if not isinstance(manifest, dict):
        return _report("refused", ["manifest is not a JSON object"], None)
    if manifest.get("contract") != CONTRACT:
        problems.append(f"contract is not {CONTRACT}")
    if manifest.get("version") not in SUPPORTED_VERSIONS:
        return _report("refused", problems + [f"unsupported version: {manifest.get('version')!r}"],
                       manifest)
    problems += [f"unknown top-level field: {k}" for k in sorted(set(manifest) - TOP_LEVEL)]
    problems += [f"missing field: {k}" for k in sorted(TOP_LEVEL - set(manifest))]
    if problems:
        return _report("refused", problems, manifest)

    for key in ("manifest_id", "owner", "created_by"):
        if not _nonempty(manifest[key]):
            problems.append(f"{key} must be a non-empty string")
    revision = manifest["revision"]
    if not isinstance(revision, int) or isinstance(revision, bool) or revision < 1:
        problems.append("revision must be a positive integer")
    if manifest["stage"] not in STAGES:
        problems.append(f"stage must be one of {', '.join(STAGES)}")
    if manifest["access_scope"] not in ACCESS:
        problems.append("access_scope must be public or restricted")
    synthetic = manifest["synthetic"]
    if not isinstance(synthetic, bool):
        problems.append("synthetic must be true or false")

    rea = manifest["rea"]
    if not isinstance(rea, dict) or not (isinstance(rea.get("commit"), str) and COMMIT.match(rea["commit"])):
        problems.append("rea.commit must be a full 40-character commit")
    elif rea["commit"] != pin:
        problems.append(f"REA subject mismatch: manifest names {rea['commit']}, supported pin is {pin}")

    artifacts = manifest["artifacts"]
    digests: set[str] = set()
    if not isinstance(artifacts, list) or not artifacts:
        problems.append("artifacts must be a non-empty list")
        artifacts = []
    for index, art in enumerate(artifacts):
        where = f"artifacts[{index}]"
        if not isinstance(art, dict):
            problems.append(f"{where} is not an object")
            continue
        locator = art.get("locator")
        if not _safe_locator(locator):
            problems.append(f"{where} has an unsafe or missing locator")
            continue
        if art.get("role") not in ROLES:
            problems.append(f"{where} has an unsupported role: {art.get('role')!r}")
        if art.get("access") not in ACCESS:
            problems.append(f"{where} access must be public or restricted")
        elif art["access"] == "restricted" and manifest["access_scope"] == "public":
            problems.append(f"restricted artifact in a public manifest: {locator}")
        if not (isinstance(art.get("sha256"), str) and SHA.match(art["sha256"])):
            problems.append(f"{where} sha256 is malformed")
            continue
        size = art.get("bytes")
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            problems.append(f"{where} bytes is malformed")
            continue
        digests.add(art["sha256"])
        path = store / locator
        if path.is_symlink() or not path.is_file():
            problems.append(f"broken reference: {locator}")
            continue
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != art["sha256"] or len(data) != size:
            problems.append(f"artifact bytes do not match the manifest: {locator}")
            continue
        if art.get("role") == "native-bundle":
            ids = art.get("evidence_ids")
            if not isinstance(ids, list) or not ids or not all(_nonempty(i) for i in ids):
                problems.append(f"{where} native bundle must list the evidence_ids it uses")
            else:
                problems += _check_bundle(data, locator, ids)
    for index, art in enumerate(artifacts):
        if isinstance(art, dict) and art.get("role") == "derivative" and art.get("derived_from") not in digests:
            problems.append(f"artifacts[{index}] derivative must name the sha256 of an artifact in this manifest")

    calibration = manifest["calibration"]
    if not isinstance(calibration, dict) or calibration.get("status") not in CALIBRATION:
        problems.append("calibration.status must be known, unknown or not-applicable")
    elif calibration["status"] != "known":
        if not _nonempty(calibration.get("rationale")):
            problems.append("calibration that is not known needs a rationale")
        if "values" in calibration:
            problems.append("calibration that is not known must not carry values")

    claims = manifest["claims"]
    if not isinstance(claims, list):
        problems.append("claims must be a list")
        claims = []
    claim_ids = []
    for index, claim in enumerate(claims):
        if not isinstance(claim, dict) or not _nonempty(claim.get("id")):
            problems.append(f"claims[{index}] needs an id")
            continue
        claim_ids.append(claim["id"])
        if claim.get("disposition") not in DISPOSITIONS:
            problems.append(f"claim {claim['id']} has an unsupported disposition")
        elif claim["disposition"] == "pass":
            evidence = claim.get("evidence")
            if not isinstance(evidence, list) or not evidence:
                problems.append(f"claim {claim['id']} passes without evidence")
            elif not set(evidence) <= {a.get("locator") for a in artifacts if isinstance(a, dict)}:
                problems.append(f"claim {claim['id']} cites evidence not in this manifest")
            if synthetic is True and manifest["stage"] not in ("E1", "C2"):
                problems.append(f"claim {claim['id']}: synthetic artifacts cannot pass stage {manifest['stage']}")
    problems += [f"duplicate claim id: {c}" for c in sorted({c for c in claim_ids if claim_ids.count(c) > 1})]
    if not isinstance(manifest["unknowns"], list):
        problems.append("unknowns must be a list")
    return _report("refused" if problems else "accepted", problems, manifest)


def _report(status: str, problems: list[str], manifest: object) -> dict:
    synthetic = manifest.get("synthetic") if isinstance(manifest, dict) else None
    return {"schema": "evo.rea-companion-manifest-check.v1", "status": status,
            "synthetic": synthetic, "problems": problems,
            "limits": "Accepted means the manifest matches the bytes it names; it is not "
                      "compatibility, measurement, timing or safety evidence."}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("manifest")
    parser.add_argument("--store", required=True, help="directory the artifact locators are relative to")
    parser.add_argument("--pin", default=CONTRACT_PIN, help="supported REA commit (default: the EVO-004 pin)")
    args = parser.parse_args(argv)
    store = Path(args.store)
    try:
        if store.is_symlink() or not store.is_dir():
            raise ManifestError(f"{store} is not a directory")
        manifest = json.loads(Path(args.manifest).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    report = validate(manifest, store, args.pin)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "accepted" else 3


if __name__ == "__main__":
    sys.exit(main())
