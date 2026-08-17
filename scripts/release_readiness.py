#!/usr/bin/env python3
"""Build exact-SHA EVO release-readiness evidence from GitHub Actions authority."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

SCHEMA = "catalyst.release-readiness.v2"
RELEASE_TYPES = ("candidate", "production", "documentation")
PRODUCT_RELEASES = frozenset(("candidate", "production"))
ALL_RELEASES = frozenset(RELEASE_TYPES)


@dataclass(frozen=True)
class Gate:
    gate_id: str
    display_name: str
    workflow_path: str
    required_for: frozenset[str]
    category: str
    not_applicable_reason: str = ""


GATES: tuple[Gate, ...] = (
    Gate("repository-verification", "Verify Repository", ".github/workflows/verify.yml",
         PRODUCT_RELEASES, "repository"),
    Gate("cross-platform-ci", "CI", ".github/workflows/ci.yml",
         PRODUCT_RELEASES, "build"),
    Gate("aes-bld-001", "AES-BLD-001 Native Build Parity",
         ".github/workflows/aes-bld-001.yml", PRODUCT_RELEASES, "governance"),
    Gate("aes-sec-001", "AES-SEC-001 Governance",
         ".github/workflows/aes-sec-001-governance.yml", ALL_RELEASES, "security"),
    Gate("sanitizers", "Sanitizers", ".github/workflows/sanitizers.yml",
         PRODUCT_RELEASES, "security"),
    Gate("code-quality", "Code Quality", ".github/workflows/quality.yml",
         PRODUCT_RELEASES, "quality"),
    Gate("repository-compliance", "Repository Compliance",
         ".github/workflows/compliance.yml", ALL_RELEASES, "governance"),
    Gate("documentation", "Documentation Report", ".github/workflows/documentation.yml",
         ALL_RELEASES, "documentation"),
    Gate("core-benchmarks", "Core Benchmarks", ".github/workflows/benchmarks.yml",
         PRODUCT_RELEASES, "performance"),
    Gate("reference-adapters", "Reference Adapters",
         ".github/workflows/reference-adapters.yml", PRODUCT_RELEASES, "consumer"),
    Gate("installed-core-version-parity", "Installed Core Version Parity",
         ".github/workflows/version-parity.yml", PRODUCT_RELEASES, "consumer"),
    Gate("project-ingestion", "Project Ingestion",
         ".github/workflows/project-ingestion.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("project-analysis", "Project Analysis",
         ".github/workflows/project-analysis.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("project-recipe", "Project Recipe",
         ".github/workflows/project-recipe.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("project-transformation", "Project Transformation",
         ".github/workflows/project-transformation.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("project-candidate", "Project Candidate",
         ".github/workflows/project-candidate.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("project-assurance", "Project Assurance",
         ".github/workflows/project-assurance.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("project-measurement", "Project Measurement",
         ".github/workflows/project-measurement.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("project-search", "Project Search",
         ".github/workflows/project-search.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("project-orchestration", "Project Orchestration",
         ".github/workflows/project-orchestration.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate(
        "project-zero",
        "Project Zero",
        ".github/workflows/project-zero.yml",
        frozenset(),
        "oversight",
        "EVO-P0-002 is retained onboarding evidence; Project Zero is not a routine release gate.",
    ),
)


class GitHubApi:
    def __init__(self, repository: str, token: str, api_url: str) -> None:
        if "/" not in repository:
            raise ValueError("repository must be owner/name")
        self.repository = repository
        self.token = token
        self.api_url = api_url.rstrip("/")

    def _url(self, path: str, params: dict[str, str] | None = None) -> str:
        base = f"{self.api_url}{path}"
        if params:
            return f"{base}?{urllib.parse.urlencode(params)}"
        return base

    def get_json(self, path: str, params: dict[str, str] | None = None) -> Any:
        request = urllib.request.Request(
            self._url(path, params),
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {self.token}",
                "X-GitHub-Api-Version": "2022-11-28",
                "User-Agent": "evo-release-readiness",
            },
        )
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                return json.load(response)
        except urllib.error.HTTPError as exc:
            body = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"GitHub API {exc.code} for {request.full_url}: {body}") from exc
        except urllib.error.URLError as exc:
            raise RuntimeError(f"GitHub API unavailable for {request.full_url}: {exc}") from exc

    def list_runs_for_commit(self, commit: str) -> list[dict[str, Any]]:
        runs: list[dict[str, Any]] = []
        page = 1
        while True:
            payload = self.get_json(
                f"/repos/{self.repository}/actions/runs",
                {"head_sha": commit, "per_page": "100", "page": str(page)},
            )
            batch = payload.get("workflow_runs", [])
            if not isinstance(batch, list):
                raise RuntimeError("GitHub workflow-runs response is malformed")
            runs.extend(item for item in batch if isinstance(item, dict))
            if len(batch) < 100:
                break
            page += 1
        return runs


def git_output(repository_root: Path, *args: str) -> str:
    process = subprocess.run(
        ["git", *args],
        cwd=repository_root,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if process.returncode != 0:
        raise RuntimeError(
            f"git {' '.join(args)} failed: {process.stderr.strip() or process.stdout.strip()}"
        )
    return process.stdout.strip()


def verify_checkout(repository_root: Path, commit: str) -> None:
    head = git_output(repository_root, "rev-parse", "HEAD")
    if head != commit:
        raise RuntimeError(f"checked-out HEAD {head} does not match release commit {commit}")


def workflow_blob_sha(repository_root: Path, commit: str, workflow_path: str) -> str:
    sha = git_output(repository_root, "rev-parse", f"{commit}:{workflow_path}")
    if not sha:
        raise RuntimeError(f"workflow blob identity missing for {workflow_path}")
    return sha


def normalized_workflow_path(run: dict[str, Any]) -> str:
    path = str(run.get("path") or "")
    return path.split("@", 1)[0]


def run_order_key(run: dict[str, Any]) -> tuple[int, int, int]:
    return (
        int(run.get("run_number") or 0),
        int(run.get("run_attempt") or 0),
        int(run.get("id") or 0),
    )


def select_authoritative_run(
    runs: Iterable[dict[str, Any]], workflow_path: str
) -> dict[str, Any] | None:
    matches = [run for run in runs if normalized_workflow_path(run) == workflow_path]
    if not matches:
        return None
    return max(matches, key=run_order_key)


def classify_run(run: dict[str, Any] | None, commit: str) -> tuple[str, str]:
    if run is None:
        return "missing", "no workflow run exists for the exact release commit"
    if str(run.get("head_sha") or "") != commit:
        return "failed", "selected workflow evidence is associated with another commit"
    if str(run.get("status") or "") != "completed":
        return "failed", f"workflow is not completed (status={run.get('status')!r})"
    conclusion = str(run.get("conclusion") or "")
    if conclusion == "success":
        return "passed", "completed successfully for the exact release commit"
    if conclusion == "skipped":
        return "skipped", "required workflow was skipped"
    return "failed", f"workflow conclusion is {conclusion or 'missing'}"


def referenced_workflows(run: dict[str, Any]) -> list[dict[str, str]]:
    refs = run.get("referenced_workflows") or []
    output: list[dict[str, str]] = []
    if not isinstance(refs, list):
        return output
    for item in refs:
        if not isinstance(item, dict):
            continue
        output.append(
            {
                "path": str(item.get("path") or ""),
                "sha": str(item.get("sha") or ""),
                "ref": str(item.get("ref") or ""),
            }
        )
    output.sort(key=lambda item: (item["path"], item["sha"], item["ref"]))
    return output


def build_gate_record(
    gate: Gate,
    release_type: str,
    commit: str,
    runs: list[dict[str, Any]],
    blob_sha: str | None,
    identity_error: str | None = None,
) -> dict[str, Any]:
    required = release_type in gate.required_for
    if not required:
        return {
            "id": gate.gate_id,
            "name": gate.display_name,
            "category": gate.category,
            "required": False,
            "status": "not-applicable",
            "reason": gate.not_applicable_reason
            or f"not required for release_type={release_type}",
            "workflow": {
                "path": gate.workflow_path,
                "blob_sha": blob_sha,
            },
            "evidence": None,
        }

    selected = select_authoritative_run(runs, gate.workflow_path)
    status, reason = classify_run(selected, commit)
    if identity_error:
        status = "failed" if selected is not None else "missing"
        reason = identity_error

    evidence: dict[str, Any] | None = None
    if selected is not None:
        evidence = {
            "workflow_id": selected.get("workflow_id"),
            "run_id": selected.get("id"),
            "run_number": selected.get("run_number"),
            "run_attempt": selected.get("run_attempt"),
            "event": selected.get("event"),
            "status": selected.get("status"),
            "conclusion": selected.get("conclusion"),
            "head_sha": selected.get("head_sha"),
            "created_at": selected.get("created_at"),
            "updated_at": selected.get("updated_at"),
            "html_url": selected.get("html_url"),
            "check_suite_id": selected.get("check_suite_id"),
            "referenced_workflows": referenced_workflows(selected),
        }

    if blob_sha is None:
        status = "failed" if selected is not None else "missing"
        reason = identity_error or "workflow file identity could not be verified at the release commit"

    return {
        "id": gate.gate_id,
        "name": gate.display_name,
        "category": gate.category,
        "required": True,
        "status": status,
        "reason": reason,
        "workflow": {
            "path": gate.workflow_path,
            "blob_sha": blob_sha,
        },
        "evidence": evidence,
    }


def summarize(gates: list[dict[str, Any]]) -> dict[str, int]:
    summary = {
        "passed": 0,
        "not-applicable": 0,
        "skipped": 0,
        "missing": 0,
        "failed": 0,
    }
    for gate in gates:
        status = str(gate["status"])
        summary[status] = summary.get(status, 0) + 1
    summary["required"] = sum(1 for gate in gates if gate["required"])
    summary["required_passed"] = sum(
        1 for gate in gates if gate["required"] and gate["status"] == "passed"
    )
    return summary


def render_report(manifest: dict[str, Any]) -> str:
    lines = [
        "# Release Readiness",
        "",
        f"Version: `{manifest['version']}`",
        "",
        f"Type: `{manifest['release_type']}`",
        "",
        f"Commit: `{manifest['commit']}`",
        "",
        f"Verification: **{manifest['verification']}**",
        "",
        "| Gate | Category | Required | Status | Workflow | Run |",
        "|---|---|:---:|---|---|---:|",
    ]
    for gate in manifest["gates"]:
        evidence = gate.get("evidence") or {}
        run_id = evidence.get("run_id")
        lines.append(
            f"| {gate['name']} | {gate['category']} | "
            f"{'yes' if gate['required'] else 'no'} | {gate['status']} | "
            f"`{gate['workflow']['path']}` | {run_id if run_id is not None else '—'} |"
        )
    lines.extend(
        [
            "",
            "The gate rows above are an audit projection of the exact GitHub Actions "
            "workflow-run authority retained in `manifest.json`; the summary is not "
            "independent release authority.",
            "",
            "This workflow does not create tags or publish releases.",
            "",
        ]
    )
    return "\n".join(lines)


def collect_manifest(
    api: GitHubApi,
    repository_root: Path,
    repository: str,
    commit: str,
    version: str,
    release_type: str,
) -> dict[str, Any]:
    if release_type not in RELEASE_TYPES:
        raise ValueError(f"unsupported release type: {release_type}")

    verify_checkout(repository_root, commit)
    runs = api.list_runs_for_commit(commit)
    gates: list[dict[str, Any]] = []

    for gate in GATES:
        blob_sha: str | None = None
        identity_error: str | None = None
        try:
            blob_sha = workflow_blob_sha(
                repository_root, commit, gate.workflow_path
            )
        except RuntimeError as exc:
            identity_error = str(exc)

        gates.append(
            build_gate_record(
                gate,
                release_type,
                commit,
                runs,
                blob_sha,
                identity_error,
            )
        )

    summary = summarize(gates)
    verification = (
        "passed"
        if summary["required"] == summary["required_passed"]
        else "failed"
    )
    return {
        "schema": SCHEMA,
        "repository": repository,
        "commit": commit,
        "version": version,
        "release_type": release_type,
        "verification": verification,
        "gate_summary": summary,
        "gates": gates,
        "tag_created": False,
        "release_published": False,
    }


def write_outputs(output: Path, manifest: dict[str, Any]) -> None:
    output.mkdir(parents=True, exist_ok=True)
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    (output / "report.md").write_text(render_report(manifest), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--release-type", choices=RELEASE_TYPES, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, default=Path("."))
    parser.add_argument(
        "--api-url", default=os.environ.get("GITHUB_API_URL", "https://api.github.com")
    )
    return parser.parse_args()


def failure_manifest(args: argparse.Namespace, message: str) -> dict[str, Any]:
    return {
        "schema": SCHEMA,
        "repository": args.repository,
        "commit": args.commit,
        "version": args.version,
        "release_type": args.release_type,
        "verification": "failed",
        "collector_error": message,
        "gate_summary": {
            "passed": 0,
            "not-applicable": 0,
            "skipped": 0,
            "missing": 0,
            "failed": 1,
            "required": 0,
            "required_passed": 0,
        },
        "gates": [],
        "tag_created": False,
        "release_published": False,
    }


def main() -> int:
    args = parse_args()
    token = os.environ.get("GITHUB_TOKEN", "")
    if not token:
        manifest = failure_manifest(args, "GITHUB_TOKEN is required")
        write_outputs(args.output, manifest)
        print("GITHUB_TOKEN is required", file=sys.stderr)
        return 2

    try:
        api = GitHubApi(args.repository, token, args.api_url)
        manifest = collect_manifest(
            api,
            args.repository_root,
            args.repository,
            args.commit,
            args.version,
            args.release_type,
        )
    except (RuntimeError, ValueError) as exc:
        manifest = failure_manifest(args, str(exc))
        write_outputs(args.output, manifest)
        print(str(exc), file=sys.stderr)
        return 1

    write_outputs(args.output, manifest)
    return 0 if manifest["verification"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
