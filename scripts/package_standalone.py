#!/usr/bin/env python3
"""Package the installed Linux CLI, preserving its executable mode in a tarball.

Dependency reports are observations of this build, not a portability or release
qualification claim. The calling workflow must pass the installed contract first.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def observe(*command: str) -> str:
    return subprocess.run(command, check=True, capture_output=True, text=True,
                          timeout=30, env={**os.environ, "LC_ALL": "C"}).stdout


def package(prefix: Path, output: Path, source_commit: str,
            frontend: str, compiler: str) -> Path:
    if platform.system() != "Linux":
        raise ValueError("the standalone binary package currently supports Linux only")
    if not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        raise ValueError("source commit must be a full lowercase Git SHA-1")
    if frontend not in ("cmake-clang", "cmake-gcc", "autotools-gcc"):
        raise ValueError("unsupported build frontend")
    binary = prefix / "bin" / "evo"
    if binary.is_symlink() or not binary.is_file() or not os.access(binary, os.X_OK):
        raise ValueError("installed bin/evo must be a regular executable file")
    with binary.open("rb") as source:
        if source.read(4) != b"\x7fELF":
            raise ValueError("installed bin/evo must be an ELF executable")

    # Read the target's ELF identity rather than labeling a cross-build as host-native.
    header = observe("readelf", "-h", str(binary))
    machine = re.search(r"^\s*Machine:\s*(.+)$", header, re.MULTILINE)
    if machine is None or machine.group(1) != "Advanced Micro Devices X86-64":
        raise ValueError("the standalone binary package currently supports x86-64 only")
    name = f"evo-linux-x86_64-{frontend}"
    output.mkdir(parents=True, exist_ok=False)
    try:
        with tempfile.TemporaryDirectory(prefix="evo-package-") as temporary:
            stage = Path(temporary) / name
            (stage / "bin").mkdir(parents=True)
            shutil.copyfile(binary, stage / "bin/evo")
            (stage / "bin/evo").chmod(0o755)
            shutil.copyfile(ROOT / "LICENSE", stage / "LICENSE")
            reports = {"evo-header.txt": header}
            for filename, option in (("evo-program-headers.txt", "-l"),
                                     ("evo-dynamic.txt", "-d"),
                                     ("evo-version-info.txt", "-V"),
                                     ("evo-notes.txt", "-n")):
                reports[filename] = observe("readelf", option, str(stage / "bin/evo"))
            dynamic = reports["evo-dynamic.txt"]
            if re.search(r"\((?:RPATH|RUNPATH)\)", dynamic):
                raise ValueError("standalone executable has an unexpected RPATH/RUNPATH")
            needed = re.findall(r"\(NEEDED\).*\[(.*?)\]", dynamic)
            if any("catalyst" in item or "foundation" in item for item in needed):
                raise ValueError("standalone executable depends on a private EVO library")
            reports["evo-dependencies.txt"] = observe("ldd", str(stage / "bin/evo"))
            if "not found" in reports["evo-dependencies.txt"]:
                raise ValueError("standalone executable has an unresolved shared library")
            for filename, content in reports.items():
                (stage / filename).write_text(content, encoding="utf-8")
            versions = sorted(set(re.findall(
                r"Name: (GLIBC_[0-9]+(?:\.[0-9]+)+)",
                reports["evo-version-info.txt"])),
                key=lambda item: tuple(map(int, item[len("GLIBC_"):].split("."))))
            manifest = {
                "schema": "catalyst.evo-standalone-package.v1",
                "repository": "dlworrell/evo",
                "source_commit": source_commit,
                "frontend": frontend,
                "platform": "linux-x86_64",
                "elf_machine": machine.group(1),
                "compiler": observe(compiler, "--version").strip(),
                "executable": "bin/evo",
                "executable_sha256": digest(stage / "bin/evo"),
                "needed_libraries": needed,
                "required_glibc_versions": versions,
                "maximum_required_glibc_version": versions[-1] if versions else None,
                "external_analysis_runtimes": ["Clang 18", "Bubblewrap",
                                               "working user/PID/network namespaces"],
                "release_qualification": False,
            }
            (stage / "manifest.json").write_text(
                json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            (stage / "README.md").write_text(
                "# EVO development binary\n\n"
                "From this extracted directory, run:\n\n"
                "```sh\nsha256sum -c SHA256SUMS\n./bin/evo --version\n"
                "./bin/evo --help\n```\n\n"
                "The archive preserves bin/evo's executable mode. No compiler or sandbox "
                "is bundled. The system loader and all required library/symbol versions "
                "in manifest.json must be available on the destination. The maximum "
                "GLIBC version is one compatibility check, not a complete host test.\n\n"
                "Analysis additionally requires Clang 18, Bubblewrap, working user/PID/"
                "network namespaces, and the project's declared build tools. A jailed "
                "host with user namespaces disabled cannot run analysis; no weaker "
                "sandbox fallback is enabled. Help/version and report do not require "
                "analysis providers. This is a development build, not a qualified "
                "release. Checksums detect corruption; they are not signatures or "
                "source-admission authority. Consult the repository's standalone CLI "
                "guide at the source_commit recorded in manifest.json.\n",
                encoding="utf-8")
            files = sorted(path for path in stage.rglob("*") if path.is_file())
            (stage / "SHA256SUMS").write_text("".join(
                f"{digest(path)}  {path.relative_to(stage).as_posix()}\n" for path in files),
                encoding="ascii")
            archive = output / f"{name}.tar.gz"
            with archive.open("xb") as destination:
                with tarfile.open(fileobj=destination, mode="w:gz") as bundle:
                    bundle.add(stage, arcname=name)
            # A package is not ready until its actual archive survives a round trip.
            with tempfile.TemporaryDirectory(prefix="evo-unpack-") as extracted:
                unpacked = Path(extracted)
                with tarfile.open(archive, "r:gz") as bundle:
                    # Only this helper's just-created, fixed-layout archive is read.
                    for member in bundle.getmembers():
                        relative = Path(member.name)
                        if relative.is_absolute() or ".." in relative.parts:
                            raise ValueError("unsafe archive member")
                        target = unpacked / relative
                        if member.isdir():
                            target.mkdir(parents=True, exist_ok=True)
                        elif member.isfile():
                            target.parent.mkdir(parents=True, exist_ok=True)
                            with bundle.extractfile(member) as source:
                                target.write_bytes(source.read())
                            target.chmod(member.mode)
                        else:
                            raise ValueError("unexpected non-regular archive member")
                restored = unpacked / name
                for entry in (restored / "SHA256SUMS").read_text().splitlines():
                    expected, relative = entry.split("  ", 1)
                    if digest(restored / relative) != expected:
                        raise ValueError(f"archive checksum mismatch: {relative}")
                for option in ("--version", "--help"):
                    subprocess.run([str(restored / "bin/evo"), option], cwd=restored,
                                   env={"PATH": str(restored / "bin"), "LANG": "C",
                                        "LC_ALL": "C"}, capture_output=True, check=True,
                                   timeout=30)
            (output / f"{archive.name}.sha256").write_text(
                f"{digest(archive)}  {archive.name}\n", encoding="ascii")
            return archive
    except Exception:
        # Never leave a partial archive looking like a successful download.
        shutil.rmtree(output)
        raise


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--frontend", required=True,
                        choices=("cmake-clang", "cmake-gcc", "autotools-gcc"))
    parser.add_argument("--compiler", required=True)
    args = parser.parse_args()
    try:
        print(package(args.prefix, args.output, args.source_commit,
                      args.frontend, args.compiler))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"standalone packaging failed: {error}\n")


if __name__ == "__main__":
    main()
