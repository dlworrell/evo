#!/usr/bin/env python3
"""Collect registry metadata and verify changed npm archives without installation.

Does not extract archive members, import package code, execute lifecycle hooks,
or establish publisher provenance, license approval, or application compatibility.
"""
from __future__ import annotations

import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import ssl
import subprocess
import tarfile
import urllib.parse
import urllib.request

OLD = "9e22da845bf8b447f81539e0ba57d5deb59e9103"
NEW = "eb3b90beb4ac9245bac415696aa0de5a1a66c6d2"
MAX_METADATA = 2_000_000
MAX_ARCHIVE = 64_000_000
MAX_UNPACKED = 512_000_000
MAX_MEMBER = 5_000_000


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], timeout=30)


def write(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def name_of(path):
    return path.rsplit("node_modules/", 1)[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ca-file", type=Path)
    args = parser.parse_args()
    root, output = args.root.resolve(), args.output.resolve()
    assert output != root and root not in output.parents
    assert git(root, "rev-parse", "HEAD").decode().strip() == NEW
    assert not git(root, "status", "--porcelain=v1")
    original = json.loads(git(root, "show", OLD + ":package-lock.json"))["packages"]
    current = json.loads(git(root, "show", NEW + ":package-lock.json"))["packages"]
    changed = sorted(p for p in original.keys() | current.keys()
                     if p and original.get(p) != current.get(p))
    context = ssl.create_default_context(cafile=str(args.ca_file) if args.ca_file else None)

    def get(url, bound):
        assert urllib.parse.urlsplit(url).scheme == "https"
        assert urllib.parse.urlsplit(url).hostname == "registry.npmjs.org"
        with urllib.request.urlopen(url, context=context, timeout=30) as response:
            assert urllib.parse.urlsplit(response.geturl()).hostname == "registry.npmjs.org"
            value = response.read(bound + 1)
        assert len(value) <= bound, "Registry input exceeded bound"
        return value

    def collect(job):
        stage, path, lock = job
        name, version = name_of(path), lock["version"]
        url = "https://registry.npmjs.org/" + urllib.parse.quote(name, safe="@") + "/" + version
        result = {"stage": stage, "path": path, "name": name, "version": version,
                  "metadata_url": url, "observed_at": datetime.now(timezone.utc).isoformat()}
        try:
            raw = get(url, MAX_METADATA)
            meta = json.loads(raw)
            assert meta["name"] == name and meta["version"] == version
            dist = meta["dist"]
            assert dist["integrity"] == lock["integrity"], "Registry/lock integrity mismatch"
            assert dist["tarball"] == lock["resolved"], "Registry/lock URL mismatch"
            result.update(status="metadata-verified", metadata_sha256=hashlib.sha256(raw).hexdigest(),
                          metadata_bytes=len(raw), declared_license=meta.get("license"),
                          declared_engines=meta.get("engines"), package_scripts=meta.get("scripts", {}),
                          dist={k: dist[k] for k in ("integrity", "shasum", "tarball", "unpackedSize",
                                                    "fileCount", "signatures", "attestations") if k in dist},
                          provenance_verification="not-run; metadata availability is not verification")
            if stage == "after":
                archive = get(dist["tarball"], MAX_ARCHIVE)
                sri = "sha512-" + base64.b64encode(hashlib.sha512(archive).digest()).decode()
                assert sri == lock["integrity"], "Archive SRI mismatch"
                assert hashlib.sha1(archive).hexdigest() == dist["shasum"], "Archive registry SHA1 mismatch"
                licenses, manifest, entries, total = [], None, 0, 0
                with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as package:
                    for member in package:
                        entries += 1
                        total += member.size
                        assert entries <= 20_000 and total <= MAX_UNPACKED
                        basename = Path(member.name).name.lower()
                        wanted = member.name == "package/package.json" or basename.startswith(("license", "licence", "notice", "copying"))
                        if not wanted or not member.isfile():
                            continue
                        assert member.size <= MAX_MEMBER
                        stream = package.extractfile(member)
                        value = stream.read(MAX_MEMBER + 1)
                        assert len(value) == member.size
                        if member.name == "package/package.json":
                            manifest = json.loads(value)
                        else:
                            text = value.decode("utf-8", errors="replace")
                            licenses.append({"path": member.name, "bytes": len(value),
                                             "sha256": hashlib.sha256(value).hexdigest(),
                                             "mit_grant_phrase_present": "Permission is hereby granted, free of charge" in text,
                                             "interpretation": "notice located and hashed; complete legal review not inferred"})
                assert manifest is not None and manifest["name"] == name and manifest["version"] == version
                result.update(status="archive-integrity-verified", archive_bytes=len(archive),
                              archive_sha256=hashlib.sha256(archive).hexdigest(),
                              archive_sri=sri, archive_entries=entries, archive_unpacked_bytes=total,
                              archive_declared_license=manifest.get("license"),
                              archive_lifecycle_hooks={k: v for k, v in manifest.get("scripts", {}).items()
                                                       if k in {"preinstall", "install", "postinstall", "prepare"}},
                              license_notices=sorted(licenses, key=lambda x: x["path"]),
                              archive_manifest_sha256=hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest())
        except Exception as error:
            result.update(status="verification-gap", diagnostic=type(error).__name__ + ": " + str(error))
        return result

    jobs = [(stage, path, packages[path]) for stage, packages in [("before", original), ("after", current)]
            for path in changed if path in packages]
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(collect, jobs))
    assert not git(root, "status", "--porcelain=v1")
    output.mkdir(parents=True, exist_ok=True)
    report = {"original_commit": OLD, "subject_commit": NEW, "registry": "https://registry.npmjs.org",
              "tls_verification": "enabled; system or explicitly selected CA bundle",
              "method_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "affected_lock_entries": len(changed), "results": results,
              "archive_execution": "none", "license_approval": "not-established",
              "scope": "before/after metadata for changed lock entries; current archives only; original archive notices not compared"}
    write(output / "registry-evidence.json", report)
    print(json.dumps({"affected_lock_entries": len(changed),
                      "metadata_records": len(results),
                      "verified_current_archives": sum(x["status"] == "archive-integrity-verified" for x in results),
                      "gaps": [{"path": x["path"], "stage": x["stage"], "diagnostic": x.get("diagnostic")}
                               for x in results if x["status"] == "verification-gap"]}, indent=2))


if __name__ == "__main__":
    main()
