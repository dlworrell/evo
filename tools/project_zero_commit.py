#!/usr/bin/env python3
"""Create a GitHub-verified Project Zero remediation commit.

Implements AES-DEV-001-R005 (every commit that can reach a default branch is
GitHub-verified) for the Project Zero remediation workflow.

Instead of a local ``git commit`` + ``git push`` from the runner, which yields an
unsigned bot commit, the staged remediation changes are sent to the GraphQL
``createCommitOnBranch`` mutation using the workflow ``GITHUB_TOKEN``. GitHub
creates and signs that commit, so it is shown as "Verified" and satisfies
``required_signatures`` branch/ruleset protection.

Safety properties:

* never targets the base branch or the repository default branch;
* refuses to run for ``pull_request_target`` events;
* creates the remediation branch from the base branch HEAD (via REST) only if
  it is missing, and only when that HEAD is the commit the remediation was
  computed against;
* uses ``expectedHeadOid`` so a concurrent update of the branch fails instead of
  being overwritten;
* is a no-op when the requested paths contain no changes.

Only the Python standard library is used.
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

MUTATION = """\
mutation ($input: CreateCommitOnBranchInput!) {
  createCommitOnBranch(input: $input) {
    commit { oid url }
  }
}
"""


class CommitError(RuntimeError):
    """Raised when the verified commit cannot be created safely."""


@dataclass
class FileChanges:
    additions: list[str] = field(default_factory=list)
    deletions: list[str] = field(default_factory=list)

    def empty(self) -> bool:
        return not self.additions and not self.deletions


# --------------------------------------------------------------------------
# Pure helpers (unit tested in tests/test_project_zero_commit.py)
# --------------------------------------------------------------------------


def parse_name_status(output: bytes) -> FileChanges:
    """Parse ``git diff --name-status -z --no-renames`` output.

    Records are ``STATUS NUL PATH NUL``. Added, modified and type-changed paths
    become additions (full contents are uploaded); deleted paths become
    deletions. Renames/copies are not expected because ``--no-renames`` is used.
    """
    changes = FileChanges()
    fields = output.split(b"\0")
    if fields and fields[-1] == b"":
        fields.pop()
    if len(fields) % 2:
        raise CommitError("malformed git name-status output")
    for index in range(0, len(fields), 2):
        status = fields[index].decode("ascii")
        path = fields[index + 1].decode("utf-8")
        kind = status[:1]
        if kind in {"A", "M", "T"}:
            changes.additions.append(path)
        elif kind == "D":
            changes.deletions.append(path)
        else:
            raise CommitError(f"unsupported change status {status!r} for {path}")
    return changes


def validate_branch(branch: str, base: str, default_branch: str | None) -> None:
    """Reject branch names that would write to a protected/base branch."""
    if not branch or branch != branch.strip() or branch.startswith("refs/"):
        raise CommitError(f"invalid remediation branch name {branch!r}")
    name = branch
    if name == base.removeprefix("refs/heads/"):
        raise CommitError(f"refusing to commit to the base branch {base!r}")
    if default_branch is not None and name == default_branch:
        raise CommitError(f"refusing to commit to the default branch {default_branch!r}")


def split_message(message: str) -> dict[str, str]:
    """Split a commit message into the GraphQL headline/body pair."""
    headline, _, body = message.strip().partition("\n")
    headline = headline.strip()
    if not headline:
        raise CommitError("commit message headline is empty")
    result = {"headline": headline}
    body = body.strip("\n")
    if body:
        result["body"] = body
    return result


def build_file_changes(changes: FileChanges, read_bytes) -> dict[str, list[dict[str, str]]]:
    """Build the ``fileChanges`` input with base64-encoded addition contents."""
    payload: dict[str, list[dict[str, str]]] = {}
    if changes.additions:
        payload["additions"] = [
            {"path": path, "contents": base64.b64encode(read_bytes(path)).decode("ascii")}
            for path in changes.additions
        ]
    if changes.deletions:
        payload["deletions"] = [{"path": path} for path in changes.deletions]
    return payload


def build_mutation_input(
    repository: str,
    branch: str,
    expected_head_oid: str,
    message: str,
    file_changes: dict[str, list[dict[str, str]]],
) -> dict[str, Any]:
    """Build the ``CreateCommitOnBranchInput`` variable."""
    return {
        "branch": {"repositoryNameWithOwner": repository, "branchName": branch},
        "message": split_message(message),
        "expectedHeadOid": expected_head_oid,
        "fileChanges": file_changes,
    }


# --------------------------------------------------------------------------
# Side-effecting helpers
# --------------------------------------------------------------------------


def git(root: Path, *args: str) -> bytes:
    return subprocess.run(
        ["git", "-C", str(root), *args], check=True, stdout=subprocess.PIPE
    ).stdout


def staged_changes(root: Path, paths: list[str]) -> FileChanges:
    """Stage the requested paths and return their changes against HEAD."""
    existing = [path for path in paths if (root / path).exists()]
    tracked_deleted = [
        path
        for path in paths
        if not (root / path).exists()
        and git(root, "ls-files", "--", path).strip()
    ]
    targets = existing + tracked_deleted
    if targets:
        git(root, "add", "--", *targets)
    return parse_name_status(
        git(root, "diff", "--cached", "--name-status", "-z", "--no-renames", "HEAD", "--", *paths)
    )


class GitHub:
    def __init__(self, token: str, api_url: str, graphql_url: str) -> None:
        self.token = token
        self.api_url = api_url.rstrip("/")
        self.graphql_url = graphql_url

    def request(self, method: str, url: str, body: Any | None = None) -> tuple[int, Any]:
        data = None if body is None else json.dumps(body).encode("utf-8")
        req = urllib.request.Request(url, data=data, method=method)
        req.add_header("Authorization", f"Bearer {self.token}")
        req.add_header("Accept", "application/vnd.github+json")
        req.add_header("X-GitHub-Api-Version", "2022-11-28")
        if data is not None:
            req.add_header("Content-Type", "application/json")
        try:
            with urllib.request.urlopen(req, timeout=60) as response:
                raw = response.read()
                return response.status, json.loads(raw) if raw else None
        except urllib.error.HTTPError as error:
            raw = error.read()
            try:
                parsed = json.loads(raw) if raw else None
            except json.JSONDecodeError:
                parsed = raw.decode("utf-8", "replace")
            return error.code, parsed

    def rest(self, method: str, path: str, body: Any | None = None) -> tuple[int, Any]:
        return self.request(method, f"{self.api_url}{path}", body)

    def ref_sha(self, repository: str, branch: str) -> str | None:
        quoted = urllib.parse.quote(branch, safe="/")
        status, data = self.rest("GET", f"/repos/{repository}/git/ref/heads/{quoted}")
        if status == 404:
            return None
        if status != 200 or not isinstance(data, dict):
            raise CommitError(f"cannot read ref {branch!r}: HTTP {status} {data}")
        return data["object"]["sha"]

    def default_branch(self, repository: str) -> str:
        status, data = self.rest("GET", f"/repos/{repository}")
        if status != 200 or not isinstance(data, dict):
            raise CommitError(f"cannot read repository {repository!r}: HTTP {status} {data}")
        return data["default_branch"]

    def create_ref(self, repository: str, branch: str, sha: str) -> None:
        status, data = self.rest(
            "POST", f"/repos/{repository}/git/refs", {"ref": f"refs/heads/{branch}", "sha": sha}
        )
        if status != 201:
            raise CommitError(f"cannot create branch {branch!r}: HTTP {status} {data}")

    def create_commit(self, mutation_input: dict[str, Any]) -> dict[str, str]:
        status, data = self.request(
            "POST", self.graphql_url, {"query": MUTATION, "variables": {"input": mutation_input}}
        )
        if status != 200 or not isinstance(data, dict) or data.get("errors"):
            raise CommitError(f"createCommitOnBranch failed: HTTP {status} {data}")
        return data["data"]["createCommitOnBranch"]["commit"]


def write_output(name: str, value: str) -> None:
    target = os.environ.get("GITHUB_OUTPUT")
    if target:
        with open(target, "a", encoding="utf-8") as handle:
            handle.write(f"{name}={value}\n")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repository", default=os.environ.get("GITHUB_REPOSITORY"))
    parser.add_argument("--branch", required=True, help="remediation branch to commit to")
    parser.add_argument("--base", required=True, help="branch the remediation was computed from")
    parser.add_argument("--message", required=True)
    parser.add_argument("--root", default=".")
    parser.add_argument("--api-url", default=os.environ.get("GITHUB_API_URL", "https://api.github.com"))
    parser.add_argument(
        "--graphql-url", default=os.environ.get("GITHUB_GRAPHQL_URL", "https://api.github.com/graphql")
    )
    parser.add_argument("paths", nargs="+", help="repository-relative paths to commit")
    args = parser.parse_args(argv)
    if not args.repository:
        parser.error("--repository or GITHUB_REPOSITORY is required")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if os.environ.get("GITHUB_EVENT_NAME") == "pull_request_target":
        raise CommitError("refusing to create commits for pull_request_target events")
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if not token:
        raise CommitError("GITHUB_TOKEN or GH_TOKEN is required")

    root = Path(args.root)
    changes = staged_changes(root, args.paths)
    if changes.empty():
        print("No remediation changes to commit.")
        write_output("committed", "false")
        return 0

    api = GitHub(token, args.api_url, args.graphql_url)
    validate_branch(args.branch, args.base, api.default_branch(args.repository))

    local_head = git(root, "rev-parse", "HEAD").decode("ascii").strip()
    head = api.ref_sha(args.repository, args.branch)
    if head is None:
        base_head = api.ref_sha(args.repository, args.base)
        if base_head is None:
            raise CommitError(f"base branch {args.base!r} does not exist")
        if base_head != local_head:
            raise CommitError(
                f"base branch {args.base!r} moved from {local_head} to {base_head} "
                "after checkout; re-run Project Zero against the current HEAD"
            )
        api.create_ref(args.repository, args.branch, base_head)
        head = base_head
        print(f"Created branch {args.branch} at {head}.")

    file_changes = build_file_changes(changes, lambda path: (root / path).read_bytes())
    commit = api.create_commit(
        build_mutation_input(args.repository, args.branch, head, args.message, file_changes)
    )
    print(f"Created verified commit {commit['oid']} on {args.branch}: {commit['url']}")
    write_output("committed", "true")
    write_output("commit_oid", commit["oid"])
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CommitError as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
