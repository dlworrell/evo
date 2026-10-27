#!/usr/bin/env python3
"""Exercise an installed EVO binary through PATH, never an in-tree test provider.

The default mode tests provider-free behavior and fail-closed runtime preflight.
Hosted qualification must use --require-providers; that mode never skips work.
Synthetic JSON below tests the report envelope reader, not provider execution.
"""

from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, separators=(",", ":")) + "\n", encoding="utf-8")


def snapshot(root: Path) -> dict[str, bytes]:
    return {str(path.relative_to(root)): path.read_bytes()
            for path in root.rglob("*") if path.is_file()}


def descendants(parent: int) -> set[int]:
    pending = [parent]
    found: set[int] = set()
    while pending:
        current = pending.pop()
        try:
            children = Path(f"/proc/{current}/task/{current}/children").read_text().split()
        except FileNotFoundError:
            continue
        for child in map(int, children):
            if child not in found:
                found.add(child)
                pending.append(child)
                assert len(found) <= 128, "unexpectedly unbounded fixture process tree"
    return found


def remove_readonly(function, path, _exc_info, root: Path):
    current = Path(path)
    if current.parent.is_relative_to(root):
        current.parent.chmod(0o700)
    if not current.is_symlink():
        current.chmod(0o700)
    function(path)


class InstalledContract:
    def __init__(self, prefix: Path, root: Path):
        self.root = root
        self.cwd = root / "unrelated"
        self.cwd.mkdir()
        # No LD_LIBRARY_PATH, HOME, compiler flags, private headers or source paths.
        self.environment = {"PATH": str(prefix / "bin"), "LANG": "C", "LC_ALL": "C"}
        self.checks = 0

    def run(self, *arguments: str, status: int = 0, cwd: Path | None = None):
        result = subprocess.run(["evo", *map(str, arguments)], cwd=cwd or self.cwd,
                                env=self.environment, capture_output=True,
                                text=True, timeout=90, check=False)
        if result.returncode != status:
            raise AssertionError(f"evo {arguments}: wanted {status}, got {result.returncode}\n"
                                 f"stdout={result.stdout}\nstderr={result.stderr}")
        if status and result.stdout:
            raise AssertionError(f"failure corrupted machine stdout: {result.stdout}")
        self.checks += 1
        return result

    def no_partial(self, output: Path) -> None:
        assert not output.exists(), f"partial successful output: {output}"
        assert not list(output.parent.glob(".evo-run-*")), "owned stage leaked"

    def fixture(self):
        project = self.root / "project"
        project.mkdir()
        (project / "main.c").write_text(
            "static int answer(void) { return 42; }\n"
            "int main(void) { return answer() == 42 ? 0 : 1; }\n", encoding="ascii")
        (project / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\nproject(installed_contract C)\n"
            "set(CMAKE_C_STANDARD 17)\nadd_executable(sample main.c)\n", encoding="ascii")
        write_json(project / "compile_commands.json", [{
            "directory": ".", "file": "main.c",
            "arguments": ["cc", "-std=c17", "-c", "main.c"],
        }])
        manifest = {
            "schema": "catalyst.evo-project-manifest.v1", "manifest_id": "installed-cli-v1",
            "source": {"declared_identity": "installed-contract-source-v1",
                       "permitted_roots": ["CMakeLists.txt", "compile_commands.json", "main.c"],
                       "compilation_database": "compile_commands.json",
                       "generated_source_policy": "reject"},
            "build": {"frontend": "cmake", "configure": ["cmake", "-S", ".", "-B", "build"],
                      "compile": ["cmake", "--build", "build", "--parallel", "1"],
                      "correctness": ["build/sample"], "benchmark": ["/bin/true"],
                      "benchmark_required": False},
            "target": {"language": "c17", "platforms": ["host-c17"]},
            "dependencies": [{"name": "c-runtime", "identity": "iso-c17"}],
            "toolchains": [{"name": "c-compiler", "identity": "host-cc-and-clang"}],
            "environment": [{"name": "LANG", "value": "C"}, {"name": "LC_ALL", "value": "C"}],
            "workloads": ["independently-expected-answer-42"],
            "constraints": ["immutable-input", "no-network"],
            "search": {"seed": 17, "population": 2, "generations": 1, "workers": 1},
            "budgets": {"max_files": 32, "max_file_bytes": 65536, "max_total_bytes": 262144,
                        "max_path_bytes": 512, "max_compilation_database_bytes": 65536,
                        "max_command_output_bytes": 1048576, "max_evidence_bytes": 4194304,
                        "command_timeout_ms": 30000, "max_memory_bytes": 1073741824,
                        "max_processes": 32, "max_storage_bytes": 33554432, "network_access": False},
            "artifacts": {"retention": "baseline-only", "cleanup": "remove-derived-workspace"},
        }
        path = self.root / "manifest.json"
        write_json(path, manifest)
        return project, path, manifest

    def parser_and_paths(self, project: Path, manifest: Path) -> None:
        assert "analyze" in self.run("--help").stdout
        assert "standalone development" in self.run("--version").stdout
        for operation in ("analyze", "evolve", "replay", "report"):
            assert f"evo {operation}" in self.run(operation, "--help").stdout
        for arguments in ((), ("unknown",), ("--version", "extra"),
                          ("analyze",), ("report", "--output"),
                          ("report", "--output", "x", "--output", "y", "--evidence", "z"),
                          ("report", "--evidence", "x", "--output", "y", "--resume"),
                          ("report", "--evidence", "x", "--output", "y", "--unknown", "z")):
            self.run(*arguments, status=2)
        output = self.root / "invalid-output"
        bad = self.root / "invalid.json"
        bad.write_text("{broken", encoding="ascii")
        self.run("analyze", "--manifest", bad, "--input", project, "--output", output, status=3)
        self.no_partial(output)
        bad.write_bytes(b" " * 1048577)
        self.run("analyze", "--manifest", bad, "--input", project, "--output", output, status=70)
        self.no_partial(output)
        alias = self.root / "manifest-alias.json"
        alias.symlink_to(manifest)
        self.run("analyze", "--manifest", alias, "--input", project, "--output", output, status=3)
        self.run("analyze", "--manifest", manifest, "--input", project,
                 "--output", self.root / ".." / "escape", status=3)
        self.run("analyze", "--manifest", manifest, "--input", project,
                 "--output", project / "nested", status=3)
        output.mkdir()
        marker = output / "marker"
        marker.write_bytes(b"keep-existing")
        self.run("analyze", "--manifest", manifest, "--input", project, "--output", output, status=3)
        assert marker.read_bytes() == b"keep-existing"
        linked = self.root / "linked-output"
        linked.symlink_to(output, target_is_directory=True)
        self.run("report", "--evidence", "missing", "--output", linked, status=3)
        unsafe = self.root / "unsafe"
        unsafe.mkdir(mode=0o700)
        unsafe.chmod(0o777)
        self.run("report", "--evidence", "missing", "--output", unsafe / "result", status=3)
        for operation in ("evolve", "replay"):
            result = self.run(operation, "--manifest", "absent", "--input", "absent",
                              "--evidence", "absent", "--output", self.root / operation, status=3)
            assert "operation-unavailable" in result.stderr
            self.no_partial(self.root / operation)

    def report_reader(self) -> None:
        evidence = self.root / "synthetic-envelope"
        (evidence / "baseline").mkdir(parents=True)
        (evidence / "analysis").mkdir()
        label = "fnv1a64-v1:0123456789abcdef"
        baseline = {"schema": "catalyst.evo-project-baseline.v1", "schema_version": 1,
                    "state": "eligible", "manifest": {},
                    "baseline": {"fingerprint": label,
                                 "execution_provider": "catalyst.evo.provider.linux-bwrap.v1",
                                 "file_count": 0, "total_file_bytes": 0, "compilation_unit_count": 0},
                    "files": [], "compilation_units": [], "commands": [], "human_readable_abstraction": {}}
        analysis = {"schema": "catalyst.evo-project-analysis.v1", "schema_version": 1,
                    "baseline_fingerprint": label, "analysis_fingerprint": label,
                    "provider": {"identity": "catalyst.evo.provider.clang-analysis.v1"},
                    "runtime_profile": {}, "human_readable_abstraction": {},
                    "source_modified": False, "evolutionary_operator_invoked": False}
        for field in ("translation_units", "source_locations", "declarations", "calls", "control_flows",
                      "data_flows", "compiler_optimizations", "runtime_hotspots", "opportunities"):
            analysis[field] = []
        write_json(evidence / "baseline/baseline.json", baseline)
        analysis_path = evidence / "analysis/analysis.json"
        write_json(analysis_path, analysis)
        output = self.root / "synthetic-report"
        projection = json.loads(self.run("report", "--evidence", evidence, "--output", output).stdout)
        assert projection["derived_projection"] and not projection["optimization_claim"]
        assert projection == json.loads((output / "summary.json").read_text())
        assert output.stat().st_mode & 0o777 == 0o700
        assert (output / "summary.json").stat().st_mode & 0o777 == 0o600
        for mutate in (lambda value: value.update(schema_version=2),
                       lambda value: value.update(baseline_fingerprint="fnv1a64-v1:ffffffffffffffff"),
                       lambda value: value.update(translation_units=["unexpected.c"]),
                       lambda value: value.update(source_modified=True),
                       lambda value: value.pop("runtime_profile"),
                       lambda value: value.update(unknown=True)):
            changed = copy.deepcopy(analysis)
            mutate(changed)
            write_json(analysis_path, changed)
            result = self.root / "rejected-report"
            self.run("report", "--evidence", evidence, "--output", result, status=3)
            self.no_partial(result)
        write_json(analysis_path, analysis)
        original = analysis_path.read_text()
        analysis_path.write_text(original.replace('"schema_version":1', '"schema_version":1,"schema_version":1'))
        self.run("report", "--evidence", evidence, "--output", self.root / "duplicate-report", status=3)
        analysis_path.unlink()
        analysis_path.symlink_to(evidence / "baseline/baseline.json")
        self.run("report", "--evidence", evidence, "--output", self.root / "symlink-report", status=3)

    def execute(self, project: Path, path: Path, manifest: dict, require_providers: bool) -> None:
        original = snapshot(project)
        output = self.root / "real-analysis"
        args = ("analyze", "--manifest", path, "--input", project, "--output", output)
        if not require_providers:
            # Availability is observable, not simulated. An unavailable sandbox must fail closed.
            result = subprocess.run(["evo", *map(str, args)], cwd=self.cwd, env=self.environment,
                                    capture_output=True, text=True, timeout=90, check=False)
            assert result.returncode in (0, 3), result.stderr
            if result.returncode == 3:
                assert "requires working Clang and Bubblewrap isolation" in result.stderr, result.stderr
                assert not result.stdout
                self.no_partial(output)
                assert snapshot(project) == original
                self.checks += 1
                print("Runtime preflight failed closed; real-provider qualification remains required.")
                return
            self.checks += 1
        else:
            result = self.run(*args)
        summary = json.loads(result.stdout)
        assert summary["translation_units"] == 1 and summary["declarations"] >= 2
        assert summary["source_files"] == 3 and not summary["optimization_claim"]
        assert (output / "baseline/snapshot/main.c").read_bytes() == original["main.c"]
        native = json.loads((output / "analysis/analysis.json").read_text())
        assert native["provider"]["identity"] == "catalyst.evo.provider.clang-analysis.v1"
        assert "clang version" in native["provider"]["clang"]
        assert summary == json.loads((output / "summary.json").read_text())
        second_cwd = self.root / "another-unrelated"
        second_cwd.mkdir()
        report = self.run("report", "--evidence", output, "--output", self.root / "real-report", cwd=second_cwd)
        assert json.loads(report.stdout) == summary
        assert snapshot(project) == original

        for name, field, command, expected in (
            ("build-failed", "compile", ["/bin/false"], 11),
            ("correctness-failed", "correctness", ["/bin/false"], 12),
            ("input-write-blocked", "configure", ["/bin/sh", "-c", f"printf changed > {project}/main.c"], 11),
        ):
            changed = copy.deepcopy(manifest)
            changed["build"][field] = command
            write_json(path, changed)
            destination = self.root / name
            self.run("analyze", "--manifest", path, "--input", project, "--output", destination, status=expected)
            self.no_partial(destination)
            assert snapshot(project) == original
        changed = copy.deepcopy(manifest)
        changed["build"]["configure"] = ["/bin/sh", "-c", "dd if=/dev/zero of=oversized bs=1048576 count=40; sleep 2"]
        write_json(path, changed)
        destination = self.root / "storage-limit"
        result = self.run("analyze", "--manifest", path, "--input", project, "--output", destination, status=70)
        assert " / storage" in result.stderr, result.stderr
        self.no_partial(destination)
        assert snapshot(project) == original
        changed = copy.deepcopy(manifest)
        changed["build"]["configure"] = ["/bin/sleep", "5"]
        changed["budgets"]["command_timeout_ms"] = 100
        write_json(path, changed)
        destination = self.root / "resource-limit"
        self.run("analyze", "--manifest", path, "--input", project, "--output", destination, status=70)
        self.no_partial(destination)
        for interrupt in (signal.SIGINT, signal.SIGTERM):
            changed = copy.deepcopy(manifest)
            changed["build"]["configure"] = ["/bin/sh", "-c", "printf ready > started; sleep 30"]
            write_json(path, changed)
            destination = self.root / f"signal-{interrupt.name}"
            process = subprocess.Popen(["evo", "analyze", "--manifest", str(path), "--input", str(project),
                                        "--output", str(destination)], cwd=self.cwd, env=self.environment,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic() + 20
                while time.monotonic() < deadline and process.poll() is None:
                    if list(self.root.glob(".evo-run-*/baseline/.evo-stage-v1/workspace/started")):
                        break
                    time.sleep(0.025)
                else:
                    raise AssertionError("sandboxed configure did not become active")
                active = descendants(process.pid)
                assert active, "no active sandbox descendant observed before interruption"
                process.send_signal(interrupt)
                stdout, stderr = process.communicate(timeout=10)
                assert process.returncode == 130 and not stdout, (process.returncode, stdout, stderr)
                deadline = time.monotonic() + 2
                while any(Path(f"/proc/{child}").exists() for child in active) and time.monotonic() < deadline:
                    time.sleep(0.025)
                assert not any(Path(f"/proc/{child}").exists() for child in active), "sandbox descendant survived cancellation"
                self.no_partial(destination)
                assert snapshot(project) == original
                self.checks += 1
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=10)
        write_json(path, manifest)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--require-providers", action="store_true")
    arguments = parser.parse_args()
    prefix = arguments.prefix.resolve(strict=True)
    assert (prefix / "bin/evo").is_file(), "installed canonical executable missing"
    root = Path(tempfile.mkdtemp(prefix="evo-installed-contract-")).resolve()
    try:
        contract = InstalledContract(prefix, root)
        project, manifest_path, manifest = contract.fixture()
        contract.parser_and_paths(project, manifest_path)
        contract.report_reader()
        contract.execute(project, manifest_path, manifest, arguments.require_providers)
        print(f"Installed CLI: {contract.checks} checks passed; required providers={arguments.require_providers}.")
    finally:
        shutil.rmtree(root, onerror=lambda function, path, error: remove_readonly(function, path, error, root))


if __name__ == "__main__":
    main()
