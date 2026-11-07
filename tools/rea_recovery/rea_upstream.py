#!/usr/bin/env python3
"""Record and check the upstream REA tree ingested under ``rea/`` (EVO ADR-0046 Amendment 1).

``record`` writes ``rea/UPSTREAM.json`` from a clean checkout of upstream REA:
repository, commit, commit date, retrieval time, the SHA-256 of the tree and its
license files. REA's git submodules point at other projects' repositories.
Each one copied into ``rea/<path>`` is recorded under ``vendored_submodules``
with its repository, pinned commit, tree SHA-256, license files and any Git LFS
files left out (``--vendored PATH=CHECKOUT``, a clean checkout at the pinned
commit). The others are listed under ``submodules_not_ingested``.

``check`` recomputes the tree digest over the files EVO tracks under ``rea/``,
and each vendored submodule's digest over the files under its path.
A match means ``rea/`` is exactly the recorded upstream tree. A difference is
allowed only when ``rea/LOCAL-CHANGES.md`` records at least one local change
(``REA-Local`` commit or divergence); otherwise it fails, because an
unrecorded edit to upstream code must not slip in.

Tree digest: SHA-256 over one line per file, sorted by path,
``<mode> <path>\\0<sha256 of the checked-out bytes>\\n`` where mode is 100755 for
executable files and 100644 otherwise. EVO-owned paths are excluded:
UPSTREAM.json, LOCAL-CHANGES.md, third_party/.gitattributes (keeps vendored
bytes exact), and the scope's ``ci/`` and
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
EVO_FILES = ("UPSTREAM.json", "LOCAL-CHANGES.md", "third_party/.gitattributes")
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


def tree_digest(root: Path, files: list[str], skip_evo_owned: bool = True) -> str:
    """Digest of ``files`` (paths relative to ``root``), as the module docstring defines."""
    lines = []
    for rel in sorted(f for f in files if not (skip_evo_owned and evo_owned(f))):
        path = root / rel
        if path.is_symlink() or not path.is_file():
            raise UpstreamError(f"{rel}: not a regular file")
        mode = "100755" if os.access(path, os.X_OK) else "100644"
        lines.append(f"{mode} {rel}\0{_sha256(path)}\n".encode())
    return hashlib.sha256(b"".join(lines)).hexdigest()


def upstream_files(checkout: Path, symlinks: list[str] | None = None) -> tuple[list[str], list[dict]]:
    """Regular files and submodule pointers tracked at an upstream checkout's HEAD.

    Symbolic links are refused unless ``symlinks`` is a list, which then collects them."""
    files, submodules = [], []
    for record in git(checkout, "ls-files", "-s", "-z").split("\0"):
        if not record:
            continue
        meta, path = record.split("\t", 1)
        mode, sha = meta.split()[:2]
        if mode == "160000":
            submodules.append({"path": path, "commit": sha})
        elif mode == "120000" and symlinks is not None:
            symlinks.append(path)
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


def lfs_files(checkout: Path, files: list[str]) -> list[str]:
    """Files git treats with the LFS filter (their checked-out bytes are only pointers here)."""
    if not files:
        return []
    done = subprocess.run(["git", "-C", str(checkout), "check-attr", "-z", "--stdin", "filter"],
                          input="\0".join(files) + "\0", capture_output=True, text=True)
    if done.returncode != 0:
        raise UpstreamError(f"git check-attr: {done.stderr.strip()}")
    parts = done.stdout.split("\0")
    return sorted(parts[i] for i in range(0, len(parts) - 2, 3) if parts[i + 2] == "lfs")


def vendor_record(sub: dict, checkout: Path, rea: Path) -> dict:
    """Record one submodule copied into ``rea/<path>`` from a clean checkout at its pinned commit."""
    head = git(checkout, "rev-parse", "HEAD").strip()
    if head != sub["commit"]:
        raise UpstreamError(f"{checkout} is at {head[:12]}, but REA pins {sub['path']} to {sub['commit'][:12]}")
    if git(checkout, "status", "--porcelain", "--untracked-files=no").strip():
        raise UpstreamError(f"{checkout} has local changes")
    links: list[str] = []
    tracked, nested = upstream_files(checkout, links)
    if nested:
        raise UpstreamError(f"{sub['path']} has its own submodules; vendor them separately")
    lfs = lfs_files(checkout, tracked)
    files = sorted(set(tracked) - set(lfs))
    digest = tree_digest(checkout, files, skip_evo_owned=False)
    if tree_digest(rea / sub["path"], files, skip_evo_owned=False) != digest:
        raise UpstreamError(f"rea/{sub['path']} does not match {checkout}; copy the tree first")
    on_disk = [f for f in lfs + links if os.path.lexists(rea / sub["path"] / f)]
    if on_disk:
        raise UpstreamError(f"rea/{sub['path']} holds LFS pointers or links, e.g. {on_disk[0]}; remove them")
    entry = {"path": sub["path"], "repository": sub.get("url"), "commit": sub["commit"],
             "tree_sha256": digest, "file_count": len(files),
             "license_files": sorted(f for f in files if "/" not in f and f.upper().startswith(("LICENSE", "COPYING", "NOTICE")))}
    if links:
        entry["symlinks_not_ingested"] = sorted(links)
    if lfs:
        entry["lfs_files_not_ingested"] = {
            "count": len(lfs),
            "reason": "Git LFS objects were not fetched; only pointer files existed. Fetch them with git lfs at the pinned commit.",
            "top_directories": sorted({"/".join(f.split("/")[:2]) for f in lfs}),
        }
    return entry


def record(checkout: Path, repository: str, rea: Path, now: dt.datetime,
           vendored: dict[str, Path] | None = None) -> dict:
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
    vendored = dict(vendored or {})
    unknown = sorted(set(vendored) - {s["path"] for s in submodules})
    if unknown:
        raise UpstreamError(f"not a submodule of {commit[:12]}: {', '.join(unknown)}")
    vendored_entries = [vendor_record(s, vendored[s["path"]], rea) for s in submodules if s["path"] in vendored]
    retrieved = now.isoformat(timespec="seconds")
    if (rea / "UPSTREAM.json").is_file():
        try:
            previous = load(rea)
        except UpstreamError:
            previous = {}
        if previous.get("commit") == commit and previous.get("retrieved_at"):
            retrieved = previous["retrieved_at"]  # same upstream commit: keep when it was first taken
    data = {
        "schema": SCHEMA,
        "repository": repository,
        "commit": commit,
        "commit_date": git(checkout, "show", "-s", "--format=%cI", "HEAD").strip(),
        "retrieved_at": retrieved,
        "tree_sha256": digest,
        "file_count": len(files),
        "license_files": sorted(f for f in files if f in LICENSE_FILES or f.split("/", 1)[0] == "licenses"),
        "vendored_submodules": vendored_entries,
        "submodules_not_ingested": sorted((s for s in submodules if s["path"] not in vendored),
                                          key=lambda s: s["path"]),
    }
    (rea / "UPSTREAM.json").write_text(dumps(data) + "\n", encoding="utf-8")
    return data


def dumps(value, indent: int = 0, used: int = 0) -> str:
    """JSON the way REA's formatter (oxfmt, printWidth 80) writes it, so rea/UPSTREAM.json
    passes REA's format:check: objects one key per line, scalar arrays on one line when
    the whole line fits in 80 columns. ``used`` counts the columns already on the line,
    including a trailing comma."""
    pad = " " * indent
    if isinstance(value, dict):
        if not value:
            return "{}"
        items = []
        for i, (key, item) in enumerate(value.items()):
            lead = f"{pad}  {json.dumps(key)}: "
            comma = 1 if i < len(value) - 1 else 0
            items.append(lead + dumps(item, indent + 2, len(lead) + comma))
        return "{\n" + ",\n".join(items) + f"\n{pad}}}"
    if isinstance(value, list):
        if not value:
            return "[]"
        if all(not isinstance(v, (dict, list)) for v in value):
            inline = "[" + ", ".join(json.dumps(v) for v in value) + "]"
            if used + len(inline) <= 80:
                return inline
        items = [f"{pad}  " + dumps(v, indent + 2, indent + 2 + (1 if i < len(value) - 1 else 0))
                 for i, v in enumerate(value)]
        return "[\n" + ",\n".join(items) + f"\n{pad}]"
    return json.dumps(value)


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
    files = evo_files(rea)
    vendored = data.get("vendored_submodules") or []
    differs = []
    for entry in vendored:
        prefix = entry["path"].rstrip("/") + "/"
        inside = [f[len(prefix):] for f in files if f.startswith(prefix)]
        files = [f for f in files if not f.startswith(prefix)]
        if tree_digest(rea / entry["path"], inside, skip_evo_owned=False) != entry["tree_sha256"]:
            differs.append(entry["path"])
    if tree_digest(rea, files) != data["tree_sha256"]:
        differs.insert(0, "REA")
    if not differs:
        extra = f" and {len(vendored)} vendored submodule(s)" if vendored else ""
        return True, f"rea/ matches {data['repository']} at {data['commit'][:12]}{extra}"
    count = local_changes(rea)
    where = ", ".join(differs)
    if count:
        return True, f"rea/ differs from its upstream record ({where}) with {count} local change(s) recorded in LOCAL-CHANGES.md"
    return False, (f"rea/ differs from its upstream record ({where}) and LOCAL-CHANGES.md records no local "
                   "change; record the change (REA-Local) or restore the upstream file")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    rec = sub.add_parser("record", help="write rea/UPSTREAM.json from a clean upstream checkout")
    rec.add_argument("checkout", type=Path)
    rec.add_argument("--repository", default="https://github.com/morluto/rea")
    rec.add_argument("--rea", type=Path, default=Path(__file__).resolve().parents[2] / "rea")
    rec.add_argument("--vendored", action="append", default=[], metavar="PATH=CHECKOUT",
                     help="a submodule copied into rea/PATH, from a clean checkout at its pinned commit")
    chk = sub.add_parser("check", help="verify rea/ against rea/UPSTREAM.json")
    chk.add_argument("--rea", type=Path, default=Path(__file__).resolve().parents[2] / "rea")
    args = parser.parse_args(argv)
    try:
        if args.command == "record":
            vendored = {}
            for item in args.vendored:
                path, sep, where = item.partition("=")
                if not sep or not path or not where:
                    raise UpstreamError(f"--vendored needs PATH=CHECKOUT, got {item!r}")
                vendored[path.strip("/")] = Path(where)
            data = record(args.checkout, args.repository, args.rea, dt.datetime.now(dt.timezone.utc), vendored)
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
