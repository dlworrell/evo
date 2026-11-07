#!/usr/bin/env python3
"""Record and check the upstream REA tree ingested under ``rea/`` (EVO ADR-0046 Amendment 1).

``record`` writes ``rea/UPSTREAM.json`` from a clean checkout of upstream REA:
repository, commit, commit date, retrieval time, the SHA-256 of the tree, its
license files, and the upstream submodules that are *not* ingested (git
submodules are pointers to other repositories; their pinned commits are
recorded so a later ingestion can decide about them).

``check`` recomputes the tree digest over the files EVO tracks under ``rea/``.
A match means ``rea/`` is exactly the recorded upstream tree. A difference is
allowed only when ``rea/LOCAL-CHANGES.md`` records at least one local change
(``REA-Local`` commit or divergence); otherwise it fails, because an
unrecorded edit to upstream code must not slip in.

Tree digest: SHA-256 over one line per file, sorted by path,
``<mode> <path>\\0<sha256 of the checked-out bytes>\\n`` where mode is 100755 for
executable files and 100644 otherwise. EVO-owned paths are excluded:
UPSTREAM.json and LOCAL-CHANGES.md, and the scope's ``ci/`` and
``docs/engineering/`` folders (Catylist ADR-0004 scope automation and
AES-SEC-001 evidence). ``record`` refuses an upstream tree that has files there.

Standard library only; needs ``git`` on PATH.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

SCHEMA = "evo.rea-upstream.v1"
EVO_FILES = ("UPSTREAM.json", "LOCAL-CHANGES.md")
EVO_DIRS = ("ci/", "docs/engineering/")
LICENSE_FILES = ("LICENSE", "THIRD_PARTY_NOTICES.md", "licenses")


class UpstreamError(RuntimeError):
    pass


def evo_owned(rel: str) -> bool:
    return rel in EVO_FILES or rel.startswith(EVO_DIRS)


def git(repo: Path, *args: str) -> str:
    done = subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True)
    if done.returncode != 0:
        raise UpstreamError(f"git {' '.join(args)}: {done.stderr.strip() or done.stdout.strip()}")
    return done.stdout


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def tree_digest(root: Path, files: list[str]) -> str:
    """Digest of ``files`` (paths relative to ``root``), as the module docstring defines."""
    lines = []
    for rel in sorted(f for f in files if not evo_owned(f)):
        path = root / rel
        if path.is_symlink() or not path.is_file():
            raise UpstreamError(f"{rel}: not a regular file")
        mode = "100755" if os.access(path, os.X_OK) else "100644"
        lines.append(f"{mode} {rel}\0{_sha256(path)}\n".encode())
    return hashlib.sha256(b"".join(lines)).hexdigest()


def upstream_files(checkout: Path) -> tuple[list[str], list[dict]]:
    """Regular files and submodule pointers tracked at an upstream checkout's HEAD."""
    files, submodules = [], []
    for record in git(checkout, "ls-files", "-s", "-z").split("\0"):
        if not record:
            continue
        meta, path = record.split("\t", 1)
        mode, sha = meta.split()[:2]
        if mode == "160000":
            submodules.append({"path": path, "commit": sha})
        elif mode == "120000":
            raise UpstreamError(f"{path}: symbolic links are not ingested; resolve before recording")
        else:
            files.append(path)
    if (checkout / ".gitmodules").is_file():
        urls = {}
        for line in git(checkout, "config", "-f", ".gitmodules", "--get-regexp", r"^submodule\..*\.(path|url)$").splitlines():
            key, value = line.split(" ", 1)
            name, field = key[len("submodule."):].rsplit(".", 1)
            urls.setdefault(name, {})[field] = value
        by_path = {entry.get("path"): entry.get("url") for entry in urls.values()}
        for sub in submodules:
            sub["url"] = by_path.get(sub["path"])
    return files, submodules


def evo_files(rea: Path) -> list[str]:
    """Files EVO tracks under ``rea`` (relative to it)."""
    out = git(rea, "ls-files", "-z", "--full-name", "--", ".")
    prefix = git(rea, "rev-parse", "--show-prefix").strip()
    return [p[len(prefix):] for p in out.split("\0") if p]


def record(checkout: Path, repository: str, rea: Path, now: dt.datetime) -> dict:
    if git(checkout, "status", "--porcelain", "--untracked-files=no").strip():
        raise UpstreamError(f"{checkout} has local changes")
    commit = git(checkout, "rev-parse", "HEAD").strip()
    files, submodules = upstream_files(checkout)
    clash = sorted(f for f in files if evo_owned(f))
    if clash:
        raise UpstreamError(f"upstream now has files in EVO-owned paths: {', '.join(clash[:5])}; "
                            "move EVO's files before syncing")
    digest = tree_digest(checkout, files)
    ingested = tree_digest(rea, files)
    if ingested != digest:
        raise UpstreamError(f"{rea} does not match {checkout} at {commit[:12]}; copy the tree first")
    data = {
        "schema": SCHEMA,
        "repository": repository,
        "commit": commit,
        "commit_date": git(checkout, "show", "-s", "--format=%cI", "HEAD").strip(),
        "retrieved_at": now.isoformat(timespec="seconds"),
        "tree_sha256": digest,
        "file_count": len(files),
        "license_files": sorted(f for f in files if f in LICENSE_FILES or f.split("/", 1)[0] == "licenses"),
        "submodules_not_ingested": sorted(submodules, key=lambda s: s["path"]),
    }
    (rea / "UPSTREAM.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return data


def load(rea: Path) -> dict:
    path = rea / "UPSTREAM.json"
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise UpstreamError(f"{path}: {exc}") from exc
    missing = [k for k in ("repository", "commit", "tree_sha256") if not isinstance(data.get(k), str)]
    if data.get("schema") != SCHEMA or missing:
        raise UpstreamError(f"{path} is not a {SCHEMA} record (missing {', '.join(missing) or 'schema'})")
    return data


def local_changes(rea: Path) -> int:
    """Rows recorded in the tables of rea/LOCAL-CHANGES.md, header rows excluded."""
    path = rea / "LOCAL-CHANGES.md"
    if not path.is_file():
        return 0
    rows = [line for line in path.read_text(encoding="utf-8").splitlines() if line.startswith("|")]
    entries = [r for r in rows if not set(r.replace("|", "").strip()) <= set("-: ")]
    headers = [r for r in entries if r.replace(" ", "").lower().startswith(("|commit|", "|path|"))]
    return len(entries) - len(headers)


def check(rea: Path) -> tuple[bool, str]:
    data = load(rea)
    digest = tree_digest(rea, evo_files(rea))
    if digest == data["tree_sha256"]:
        return True, f"rea/ matches {data['repository']} at {data['commit'][:12]}"
    count = local_changes(rea)
    if count:
        return True, f"rea/ differs from {data['commit'][:12]} with {count} local change(s) recorded in LOCAL-CHANGES.md"
    return False, (f"rea/ differs from {data['repository']} at {data['commit'][:12]} and LOCAL-CHANGES.md "
                   "records no local change; record the change (REA-Local) or restore the upstream file")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    rec = sub.add_parser("record", help="write rea/UPSTREAM.json from a clean upstream checkout")
    rec.add_argument("checkout", type=Path)
    rec.add_argument("--repository", default="https://github.com/morluto/rea")
    rec.add_argument("--rea", type=Path, default=Path(__file__).resolve().parents[2] / "rea")
    chk = sub.add_parser("check", help="verify rea/ against rea/UPSTREAM.json")
    chk.add_argument("--rea", type=Path, default=Path(__file__).resolve().parents[2] / "rea")
    args = parser.parse_args(argv)
    try:
        if args.command == "record":
            data = record(args.checkout, args.repository, args.rea, dt.datetime.now(dt.timezone.utc))
            print(f"recorded {data['repository']} {data['commit']} ({data['file_count']} files, "
                  f"tree {data['tree_sha256'][:16]}…)")
            return 0
        ok, message = check(args.rea)
        print(message, file=sys.stdout if ok else sys.stderr)
        return 0 if ok else 1
    except UpstreamError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
