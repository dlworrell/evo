"""Exercise real ELF packaging, extracted execution, and failure cleanup."""

import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import unittest

from scripts.package_standalone import package


@unittest.skipUnless(platform.system() == "Linux" and platform.machine() == "x86_64",
                     "the downloadable binary profile is Linux x86-64")
class StandalonePackageTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.prefix = self.root / "installed"
        (self.prefix / "bin").mkdir(parents=True)
        self.binary = self.prefix / "bin/evo"
        self.output = self.root / "download"
        self.compiler = shutil.which("cc")
        self.assertIsNotNone(self.compiler, "packaging regression requires a C compiler")
        source = self.root / "fixture.c"
        source.write_text("#include <stdio.h>\nint main(void) { "
                          "puts(\"package fixture 42\"); return 0; }\n")
        subprocess.run([self.compiler, str(source), "-o", str(self.binary)], check=True)

    def build(self, **overrides):
        options = dict(prefix=self.prefix, output=self.output, source_commit="a" * 40,
                       frontend="cmake-clang", compiler=self.compiler)
        options.update(overrides)
        return package(**options)

    def test_archive_contains_executable_provenance_and_complete_checksums(self):
        archive = self.build()
        with tarfile.open(archive) as bundle:
            members = {item.name: item for item in bundle.getmembers()}
            root = "evo-linux-x86_64-cmake-clang/"
            executable = members[root + "bin/evo"]
            self.assertTrue(executable.mode & 0o111)
            self.assertEqual(bundle.extractfile(executable).read(), self.binary.read_bytes())
            manifest = json.load(bundle.extractfile(root + "manifest.json"))
            self.assertEqual(manifest["source_commit"], "a" * 40)
            self.assertEqual(manifest["platform"], "linux-x86_64")
            self.assertIn("libc.so.6", manifest["needed_libraries"])
            self.assertTrue(manifest["required_glibc_versions"])
            self.assertFalse(manifest["release_qualification"])
            checksums = bundle.extractfile(root + "SHA256SUMS").read().decode().splitlines()
            checked = set()
            for line in checksums:
                expected, relative = line.split("  ", 1)
                checked.add(root + relative)
                self.assertEqual(hashlib.sha256(
                    bundle.extractfile(root + relative).read()).hexdigest(), expected)
            self.assertEqual(checked, {name for name, item in members.items()
                                     if item.isfile() and name != root + "SHA256SUMS"})
        expected = archive.with_name(archive.name + ".sha256").read_text().split()[0]
        self.assertEqual(hashlib.sha256(archive.read_bytes()).hexdigest(), expected)

    def test_autotools_archive_runs_after_original_installation_is_removed(self):
        archive = self.build(frontend="autotools-gcc")
        destination = self.root / "relocated-evo"
        with tarfile.open(archive) as bundle:
            content = bundle.extractfile("evo-linux-x86_64-autotools-gcc/bin/evo").read()
        destination.write_bytes(content)
        destination.chmod(0o755)
        shutil.rmtree(self.prefix)
        result = subprocess.run([str(destination), "--version"], cwd=self.root,
                                env={"PATH": "", "LC_ALL": "C"}, capture_output=True,
                                text=True, check=True)
        self.assertEqual(result.stdout, "package fixture 42\n")

    def test_missing_binary_cannot_produce_report_only_archive(self):
        self.binary.unlink()
        with self.assertRaisesRegex(ValueError, "regular executable"):
            self.build()
        self.assertFalse(self.output.exists())

    def test_nonexecutable_and_symlink_inputs_are_rejected(self):
        self.binary.chmod(0o644)
        with self.assertRaisesRegex(ValueError, "regular executable"):
            self.build()
        self.binary.rename(self.prefix / "bin/real-evo")
        self.binary.symlink_to("real-evo")
        with self.assertRaisesRegex(ValueError, "regular executable"):
            self.build()
        self.assertFalse(self.output.exists())

    def test_text_input_is_rejected(self):
        self.binary.write_text("#!/bin/sh\nexit 0\n")
        with self.assertRaisesRegex(ValueError, "ELF executable"):
            self.build()

    def test_existing_download_is_not_overwritten(self):
        self.output.mkdir()
        marker = self.output / "keep"
        marker.write_text("previous download")
        with self.assertRaises(FileExistsError):
            self.build()
        self.assertEqual(marker.read_text(), "previous download")

    def test_invalid_provenance_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "full lowercase Git"):
            self.build(source_commit="main")
        self.assertFalse(self.output.exists())

    def test_failed_inspection_removes_incomplete_download(self):
        with self.assertRaises(FileNotFoundError):
            self.build(compiler=str(self.root / "missing-compiler"))
        self.assertFalse(self.output.exists())

    def test_failed_extracted_execution_removes_the_archive(self):
        source = self.root / "broken.c"
        source.write_text("int main(void) { return 17; }\n")
        subprocess.run([self.compiler, str(source), "-o", str(self.binary)], check=True)
        with self.assertRaises(subprocess.CalledProcessError):
            self.build()
        self.assertFalse(self.output.exists())

    def test_build_tree_search_path_is_rejected(self):
        subprocess.run([self.compiler, str(self.root / "fixture.c"), "-o", str(self.binary),
                        "-Wl,-rpath,/unexpected/build-tree"], check=True)
        with self.assertRaisesRegex(ValueError, "RPATH/RUNPATH"):
            self.build()
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
