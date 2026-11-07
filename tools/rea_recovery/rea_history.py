#!/usr/bin/env python3
"""Keep REA's upstream git history in Dropbox as git bundles (EVO ADR-0046 Amendment 1).

The owner runs this on their own machine (decision of 2026-11-06, "B"). Each run:

1. updates a local bare mirror of upstream branches and tags (outside Dropbox,
   because file sync corrupts a live ``.git``);
2. writes one bundle into the Dropbox folder: a full bundle the first time
   (``rea-full-<date>-<sha>.bundle``), then an incremental bundle with only the
   new history (``rea-incr-<date>-<from>-<to>.bundle``); nothing when upstream
   has not changed;
3. checks the new bundle with ``git bundle verify`` before it is published, and
   records it in ``MANIFEST.json`` with its SHA-256, size, refs and
   prerequisites.

Files are written under a temporary name and renamed into place, so Dropbox
never syncs a half-written bundle or manifest. ``--verify`` re-hashes every
listed bundle against the manifest.

Restore the history with ``git clone rea-full-....bundle rea`` and then
``git -C rea fetch ../rea-incr-....bundle 'refs/*:refs/*'`` for each incremental
bundle in manifest order.

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

UPSTREAM = "https://github.com/morluto/rea"
DEFAULT_DROPBOX = Path.home() / "Dropbox" / "repo" / "Rea"
DEFAULT_CACHE = Path.home() / ".cache" / "evo" / "rea-history.git"
MANIFEST = "MANIFEST.json"
REFSPECS = ("+refs/heads/*:refs/heads/*", "+refs/tags/*:refs/tags/*")
SCHEMA = "evo.rea-history.v1"


class HistoryError(RuntimeError):
    pass


def git(repo: Path, *args: str) -> str:
    result = subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True)
    if result.returncode != 0:
        raise HistoryError(f"git {' '.join(args)} failed: {result.stderr.strip() or result.stdout.strip()}")
    return result.stdout


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def update_mirror(cache: Path, upstream: str, log=print) -> None:
    if not (cache / "HEAD").exists():
        cache.mkdir(parents=True, exist_ok=True)
        git(cache, "init", "--bare", "--quiet")
        git(cache, "remote", "add", "origin", upstream)
        log(f"created mirror {cache}")
    elif git(cache, "remote", "get-url", "origin").strip() != upstream:
        raise HistoryError(f"{cache} mirrors a different upstream; use another --cache")
    git(cache, "fetch", "--quiet", "--no-prune", "origin", *REFSPECS)
    # Point HEAD at upstream's main branch so a clone of the bundle checks it out.
    for name in ("refs/heads/main", "refs/heads/master"):
        if subprocess.run(["git", "-C", str(cache), "show-ref", "--verify", "--quiet", name]).returncode == 0:
            git(cache, "symbolic-ref", "HEAD", name)
            break


def current_refs(cache: Path) -> dict[str, str]:
    out = git(cache, "for-each-ref", "--format=%(refname) %(objectname)", "refs/heads", "refs/tags")
    return dict(line.split(" ", 1) for line in out.splitlines() if line)


def head_sha(cache: Path, refs: dict[str, str]) -> str:
    for name in ("refs/heads/main", "refs/heads/master"):
        if name in refs:
            return refs[name]
    return sorted(refs.values())[0]


def load_manifest(folder: Path) -> dict:
    path = folder / MANIFEST
    if not path.exists():
        return {"schema": SCHEMA, "upstream": None, "bundles": []}
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != SCHEMA or not isinstance(data.get("bundles"), list):
        raise HistoryError(f"{path} is not a {SCHEMA} manifest")
    return data


def write_atomic(path: Path, data: bytes) -> None:
    partial = path.with_name(path.name + ".partial")
    partial.write_bytes(data)
    os.replace(partial, path)


def run(dropbox: Path, cache: Path, upstream: str, now: dt.datetime, dry_run: bool = False, log=print) -> dict | None:
    """Add one bundle for new upstream history; return the manifest entry, or None if nothing changed."""
    if not dropbox.parent.exists():
        raise HistoryError(f"{dropbox.parent} does not exist; is Dropbox installed and synced here?")
    dropbox.mkdir(exist_ok=True)
    manifest = load_manifest(dropbox)
    if manifest["upstream"] not in (None, upstream):
        raise HistoryError(f"{dropbox / MANIFEST} records upstream {manifest['upstream']}, not {upstream}")
    update_mirror(cache, upstream, log)
    refs = current_refs(cache)
    if not refs:
        raise HistoryError("upstream has no branches or tags")
    previous = manifest["bundles"][-1]["refs"] if manifest["bundles"] else None
    if previous == refs:
        log("upstream unchanged since the last bundle; nothing written")
        return None
    head = head_sha(cache, refs)
    stamp = now.strftime("%Y%m%d")
    if previous is None:
        kind, name, exclude = "full", f"rea-full-{stamp}-{head[:12]}.bundle", []
    else:
        known = set(git(cache, "rev-list", "--all").split())
        exclude = sorted({sha for sha in previous.values() if sha in known})
        old_head = manifest["bundles"][-1]["head"]
        kind, name = "incremental", f"rea-incr-{stamp}-{old_head[:12]}-{head[:12]}.bundle"
    if dry_run:
        log(f"dry run: would write {name} ({kind}) for {len(refs)} refs")
        return {"file": name, "kind": kind}
    target = dropbox / name
    if target.exists():
        raise HistoryError(f"{target} already exists; refusing to overwrite")
    partial = target.with_name(target.name + ".partial")
    created = subprocess.run(["git", "-C", str(cache), "bundle", "create", str(partial), "--all",
                              *[f"^{sha}" for sha in exclude]], capture_output=True, text=True)
    if created.returncode != 0:
        partial.unlink(missing_ok=True)
        if "empty bundle" in created.stderr:
            # Only ref moves to commits already bundled (or deletions): record them without a file.
            name, kind = None, "refs-only"
        else:
            raise HistoryError(f"git bundle create failed: {created.stderr.strip()}")
    entry = {"file": name, "kind": kind, "created_at": now.isoformat(timespec="seconds"),
             "head": head, "refs": refs, "prerequisites": exclude}
    if name is not None:
        git(cache, "bundle", "verify", "--quiet", str(partial))
        entry["sha256"] = sha256(partial)
        entry["size"] = partial.stat().st_size
        os.replace(partial, target)
        log(f"wrote {target} ({entry['size']:,} bytes, sha256 {entry['sha256'][:16]}…)")
    else:
        log("refs moved to history already bundled; recorded in the manifest only")
    manifest["upstream"] = upstream
    manifest["bundles"].append(entry)
    write_atomic(dropbox / MANIFEST, (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode())
    return entry


def verify(dropbox: Path, log=print) -> int:
    manifest = load_manifest(dropbox)
    bad = 0
    for entry in manifest["bundles"]:
        if entry["file"] is None:
            continue
        path = dropbox / entry["file"]
        if not path.exists():
            log(f"MISSING {entry['file']}")
            bad += 1
        elif sha256(path) != entry["sha256"]:
            log(f"CHANGED {entry['file']}")
            bad += 1
        else:
            log(f"ok {entry['file']}")
    return 1 if bad else 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dropbox", type=Path, default=DEFAULT_DROPBOX, help=f"default {DEFAULT_DROPBOX}")
    parser.add_argument("--cache", type=Path, default=DEFAULT_CACHE, help=f"local mirror, default {DEFAULT_CACHE}")
    parser.add_argument("--upstream", default=UPSTREAM)
    parser.add_argument("--dry-run", action="store_true", help="fetch and report, write nothing to Dropbox")
    parser.add_argument("--verify", action="store_true", help="re-hash every bundle listed in the manifest")
    args = parser.parse_args(argv)
    try:
        if args.verify:
            return verify(args.dropbox.expanduser())
        run(args.dropbox.expanduser(), args.cache.expanduser(), args.upstream,
            dt.datetime.now(dt.timezone.utc), dry_run=args.dry_run)
        return 0
    except HistoryError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
