#!/usr/bin/env python3
"""EVO/REA fused build runner and encrypted source vault (EVO ADR-0048, proposed).

Builds the fused EVO + REA product into one destination: EVO from its own
checkout with its documented CMake build, and REA from a separate checkout
pinned by ADR-0046 with its documented npm build, without changing REA's files.
Commands: ``discover`` (environment report), ``build`` (plan; ``--execute`` to
run), ``pdf`` (render and optionally print a report such as ``evo report``
output), ``ui`` (EVO CLI, REA CLI or REA's stdio MCP server), and
``vault seal|verify|open`` (age encryption, one key pair per authorised person).

It never runs ``rea setup``, ``rea update`` or ``rea uninstall`` and never edits
agent/client configuration. Private keys are read only from the identity file
the caller names and are never written, printed or copied.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from datetime import date

CONTRACT_PIN = "9e22da845bf8b447f81539e0ba57d5deb59e9103"
FORBIDDEN_REA_COMMANDS = ("setup", "update", "uninstall")
PDF_BACKENDS = ("pandoc", "chromium")
PRINT_COMMANDS = ("lp", "lpr")
INDEX = "vault-index.json"
LAYOUT = "evo-rea.json"
MIN_CMAKE = (3, 20)


def _run(cmd: list[str], cwd: Path | None = None) -> tuple[int, str]:
    try:
        done = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired) as error:
        return 127, str(error)
    return done.returncode, (done.stdout or done.stderr).strip()


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def node_satisfies(version: str, engines: str) -> bool:
    """True if ``version`` (e.g. v22.22.0) meets an npm range of ``^x.y.z`` / ``>=x.y.z`` alternatives."""
    found = re.match(r"v?(\d+)\.(\d+)\.(\d+)", version or "")
    if not found:
        return False
    have = tuple(int(p) for p in found.groups())
    for alt in engines.split("||"):
        alt = alt.strip()
        part = re.match(r"(\^|>=)\s*(\d+)\.(\d+)\.(\d+)$", alt)
        if not part:
            continue
        want = tuple(int(p) for p in part.groups()[1:])
        if part.group(1) == ">=" and have >= want:
            return True
        if part.group(1) == "^" and have[0] == want[0] and have >= want:
            return True
    return False


def _chromium() -> str | None:
    for name in ("chromium", "chromium-browser", "google-chrome"):
        if shutil.which(name):
            return shutil.which(name)
    root = Path("/opt/pw-browsers")
    for candidate in sorted(root.glob("chromium-*/chrome-linux/chrome")) if root.is_dir() else []:
        return str(candidate)
    return None


def _cmake_version() -> tuple[int, ...] | None:
    if not shutil.which("cmake"):
        return None
    code, text = _run(["cmake", "--version"])
    found = re.search(r"(\d+)\.(\d+)", text) if code == 0 else None
    return tuple(int(p) for p in found.groups()) if found else None


def discover(evo: Path, rea: Path, destination: Path, vaults: Path | None,
             pin: str = CONTRACT_PIN) -> dict:
    checks: dict[str, dict] = {}
    cmake_lists = evo / "CMakeLists.txt"
    project = re.search(r"project\(catalyst_evo VERSION ([0-9.]+)",
                        cmake_lists.read_text(encoding="utf-8")) if cmake_lists.is_file() else None
    checks["evo_source"] = ({"status": "ok", "evidence": f"{evo}: catalyst_evo {project.group(1)}"} if project
                            else {"status": "missing", "evidence": f"{evo} is not an EVO checkout"})
    compiler = next((shutil.which(c) for c in ("clang", "gcc", "cc") if shutil.which(c)), None)
    checks["c_compiler"] = {"status": "ok" if compiler else "missing", "evidence": compiler or "no clang/gcc/cc"}
    cmake = _cmake_version()
    if cmake is None:
        checks["cmake"] = {"status": "missing", "evidence": "cmake not on PATH"}
    else:
        checks["cmake"] = {"status": "ok" if cmake >= MIN_CMAKE else "mismatch",
                           "evidence": f"cmake {'.'.join(map(str, cmake))} (EVO needs >= 3.20)"}
    bwrap = shutil.which("bwrap")
    checks["sandbox"] = {"status": "ok" if bwrap else "missing",
                         "evidence": bwrap or "no Bubblewrap; evo analyze's isolated provider is unavailable"}
    checks["platform"] = {"status": "ok", "evidence": f"{platform.system()} {platform.machine()} python {platform.python_version()}"}
    engines = None
    package = rea / "package.json"
    if package.is_file():
        try:
            engines = json.loads(package.read_text(encoding="utf-8")).get("engines", {}).get("node")
        except ValueError:
            engines = None
    code, node = _run(["node", "--version"]) if shutil.which("node") else (127, "")
    if code != 0:
        checks["node"] = {"status": "missing", "evidence": "node not on PATH"}
    elif engines and not node_satisfies(node, engines):
        checks["node"] = {"status": "mismatch", "evidence": f"{node} does not satisfy {engines}"}
    else:
        checks["node"] = {"status": "ok", "evidence": f"{node} (REA requires {engines or 'unknown'})"}
    checks["npm"] = {"status": "ok", "evidence": shutil.which("npm")} if shutil.which("npm") else {"status": "missing", "evidence": "npm not on PATH"}
    if not (rea / ".git").exists():
        checks["rea_source"] = {"status": "missing", "evidence": f"{rea} is not a git checkout"}
    else:
        _, head = _run(["git", "rev-parse", "HEAD"], rea)
        _, dirty = _run(["git", "status", "--porcelain"], rea)
        if head != pin:
            checks["rea_source"] = {"status": "mismatch", "evidence": f"HEAD {head or '?'} is not the pin {pin}"}
        elif dirty:
            checks["rea_source"] = {"status": "mismatch", "evidence": "checkout has local changes"}
        else:
            checks["rea_source"] = {"status": "ok", "evidence": f"{rea} at {pin}, clean"}
    if destination.exists() and not destination.is_dir():
        checks["destination"] = {"status": "mismatch", "evidence": f"{destination} is not a directory"}
    else:
        probe = destination if destination.exists() else destination.parent
        writable = probe.is_dir() and os.access(probe, os.W_OK)
        checks["destination"] = {"status": "ok" if writable else "mismatch",
                                 "evidence": f"{destination} {'writable' if writable else 'not writable'}"}
    pdf = [name for name in PDF_BACKENDS if (shutil.which(name) if name == "pandoc" else _chromium())]
    checks["pdf"] = {"status": "ok" if pdf else "missing", "evidence": ", ".join(pdf) or "no pandoc or chromium"}
    printer = next((shutil.which(c) for c in PRINT_COMMANDS if shutil.which(c)), None)
    checks["printing"] = {"status": "ok" if printer else "missing", "evidence": printer or "no lp/lpr; PDF files only"}
    display = os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")
    checks["ui"] = {"status": "ok", "evidence": f"CLI and stdio MCP; display {'available' if display else 'not available'}"}
    age = shutil.which("age")
    if not age:
        checks["vault"] = {"status": "missing", "evidence": "age not on PATH"}
    elif vaults is None or not vaults.is_file():
        checks["vault"] = {"status": "missing", "evidence": "no vault definitions file"}
    else:
        config = load_vaults(vaults)
        openable = sorted(n for n, v in config["vaults"].items() if any(m["can_decrypt"] for m in v["members"]))
        locked = sorted(set(config["vaults"]) - set(openable))
        checks["vault"] = {"status": "ok" if openable and not locked else "missing",
                           "evidence": f"age; vaults that someone can open: {', '.join(openable) or 'none'}"
                                       + (f"; no decrypting member yet: {', '.join(locked)}" if locked else "")}
    blocking = sorted(k for k in ("evo_source", "c_compiler", "cmake", "node", "npm", "rea_source", "destination")
                      if checks[k]["status"] != "ok")
    return {"schema": "evo.rea-build-discovery.v1", "pin": pin, "checks": checks, "blocking": blocking}


def build_plan(evo: Path, rea: Path, destination: Path) -> list[tuple[Path, list[str]]]:
    """Each project's own documented build, then install into ``destination``."""
    work = destination / "build" / "evo"
    return [
        (evo, ["cmake", "-S", str(evo), "-B", str(work), "-DCMAKE_BUILD_TYPE=Release",
               f"-DCMAKE_INSTALL_PREFIX={destination}"]),
        (evo, ["cmake", "--build", str(work)]),
        (evo, ["ctest", "--test-dir", str(work), "--output-on-failure"]),
        (evo, ["cmake", "--install", str(work)]),
        (rea, ["npm", "ci"]),
        (rea, ["npm", "run", "build:cached"]),
        (rea, ["node", "scripts/rea.mjs", "doctor", "--json"]),
    ]


def write_layout(evo: Path, rea: Path, destination: Path, pin: str) -> Path:
    """Record where the fused product's parts live, for ``ui``."""
    layout = {"schema": "evo.rea-fused-layout.v1", "evo_binary": str(destination / "bin" / "evo"),
              "evo_source": str(evo.resolve()), "rea_checkout": str(rea.resolve()), "rea_pin": pin}
    path = destination / LAYOUT
    path.write_text(json.dumps(layout, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return path


def rea_command(args: list[str]) -> list[str]:
    if args and args[0] in FORBIDDEN_REA_COMMANDS:
        raise ValueError(f"rea {args[0]} changes agent configuration or installs; refused")
    return ["node", "scripts/rea.mjs", *args]


def ui_command(destination: Path, component: str, args: list[str]) -> tuple[Path | None, list[str]]:
    """The command and working directory for the EVO CLI or REA (CLI or stdio MCP)."""
    layout = json.loads((destination / LAYOUT).read_text(encoding="utf-8"))
    if component == "evo":
        return None, [layout["evo_binary"], *args]
    if component == "rea":
        return Path(layout["rea_checkout"]), rea_command(args or ["mcp"])
    raise ValueError("component must be evo or rea")


def render_pdf(source: Path, output: Path) -> str:
    if shutil.which("pandoc"):
        code, text = _run(["pandoc", str(source), "-o", str(output)])
        if code == 0:
            return "pandoc"
    browser = _chromium()
    if browser:
        html = source
        if source.suffix.lower() in (".md", ".markdown") and shutil.which("pandoc"):
            html = output.with_suffix(".html")
            _run(["pandoc", "-s", str(source), "-o", str(html)])
        code, text = _run([browser, "--headless", "--no-sandbox", f"--print-to-pdf={output}", html.resolve().as_uri()])
        if code == 0 and output.is_file():
            return "chromium"
    raise RuntimeError("no PDF backend succeeded (need pandoc with a PDF engine, or chromium)")


AGE_KEY = re.compile(r"age1[0-9a-z]{58}")
SSH_KEY = re.compile(r"ssh-ed25519 [A-Za-z0-9+/]{68}")


def _public_key(text: str) -> str:
    """Normalise an ``age1...`` or ``ssh-ed25519 ...`` public key; trailing comments are dropped."""
    text = text.strip()
    if AGE_KEY.fullmatch(text):
        return text
    ssh = SSH_KEY.match(text)
    if ssh and (len(text) == ssh.end() or text[ssh.end()] == " "):
        return ssh.group(0)
    raise ValueError(f"not an age or ssh-ed25519 public key: {text[:12]}...")


def _fingerprint(key: str) -> str:
    return hashlib.sha256(key.encode()).hexdigest()[:16]


ROLES = ("view", "edit")
VAULT_NAME = re.compile(r"[a-z0-9][a-z0-9-]{0,62}")
DEFAULT_VAULTS = Path(__file__).resolve().parent / "vaults.json"


def load_vaults(path: Path) -> dict:
    """Termius-style vault definitions: named vaults, each with members who can view or edit."""
    config = json.loads(path.read_text(encoding="utf-8"))
    if config.get("schema") != "evo.rea-vaults.v1" or not isinstance(config.get("vaults"), dict):
        raise ValueError(f"{path} is not an evo.rea-vaults.v1 file")
    for name, vault in config["vaults"].items():
        if not VAULT_NAME.fullmatch(name):
            raise ValueError(f"bad vault name: {name!r}")
        members = vault.get("members")
        if not isinstance(members, list):
            raise ValueError(f"vault {name}: members must be a list")
        seen = set()
        for member in members:
            if member.get("role") not in ROLES:
                raise ValueError(f"vault {name}: role must be view or edit")
            if not isinstance(member.get("can_decrypt"), bool) or not member.get("label"):
                raise ValueError(f"vault {name}: every member needs a label and can_decrypt true/false")
            key = _public_key(member.get("key", ""))
            if key in seen:
                raise ValueError(f"vault {name}: duplicate member key {_fingerprint(key)}")
            seen.add(key)
        if members and not any(m["role"] == "edit" for m in members):
            raise ValueError(f"vault {name}: needs at least one member who can edit")
    return config


def vault_members(config: dict, name: str) -> list[dict]:
    if name not in config["vaults"]:
        raise ValueError(f"no vault named {name!r}; known: {', '.join(sorted(config['vaults']))}")
    return config["vaults"][name]["members"]


KEY_SOURCES = {"sshid": "https://sshid.io/{user}", "github": "https://github.com/{user}.keys"}
USERNAME = re.compile(r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,38})")


def fetch_public_keys(source: str, user: str, opener=urllib.request.urlopen) -> tuple[str, list[str]]:
    """Public keys published for ``user`` at sshid.io or GitHub; only ssh-ed25519 keys are kept."""
    if source not in KEY_SOURCES or not USERNAME.fullmatch(user):
        raise ValueError("source must be sshid or github, with a plain username")
    url = KEY_SOURCES[source].format(user=user)
    request = urllib.request.Request(url, headers={"Accept": "text/plain", "User-Agent": "evo-rea-build"})
    with opener(request, timeout=20) as response:
        text = response.read(65536).decode("utf-8", "replace")
    keys = []
    for line in text.splitlines():
        found = SSH_KEY.match(line.strip())
        if found and found.group(0) not in keys:
            keys.append(found.group(0))
    if not keys:
        raise ValueError(f"no ssh-ed25519 public key published at {url}")
    return url, keys


def add_members(path: Path, name: str, label: str, role: str, source: str, keys: list[str],
                can_decrypt: bool, description: str = "") -> list[str]:
    """Add members to vault ``name`` (created if new). Returns the fingerprints added."""
    if role not in ROLES or not VAULT_NAME.fullmatch(name):
        raise ValueError("role must be view or edit, and the vault name lower-case letters, digits and dashes")
    config = load_vaults(path) if path.is_file() else {"schema": "evo.rea-vaults.v1", "vaults": {}}
    vault = config["vaults"].setdefault(name, {"description": description, "members": []})
    existing = {_public_key(m["key"]) for m in vault["members"]}
    added = []
    for key in (_public_key(k) for k in keys):
        if key not in existing:
            vault["members"].append({"label": label, "role": role, "key": key, "source": source,
                                     "can_decrypt": can_decrypt, "added": date.today().isoformat()})
            added.append(_fingerprint(key))
    if not any(m["role"] == "edit" for m in vault["members"]):
        raise ValueError(f"vault {name} needs at least one member who can edit; add an editor first")
    path.write_text(json.dumps(config, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return added


def vault_seal(source: Path, vault: Path, members: list[dict], name: str = "") -> dict:
    keys = [_public_key(m["key"]) for m in members]
    if not keys:
        raise ValueError("the vault has no members")
    if not any(m.get("can_decrypt") for m in members):
        raise ValueError("no member of this vault can decrypt (device-bound keys such as SSH ID passkeys "
                         "cannot open age files); add an age1 key or a recovery key first")
    if vault.resolve() == source.resolve() or source.resolve() in vault.resolve().parents:
        raise ValueError("the vault must be outside the source directory")
    vault.mkdir(parents=True, exist_ok=True)
    entries = []
    for path in sorted(p for p in source.rglob("*") if p.is_file() or p.is_symlink()):
        rel = path.relative_to(source).as_posix()
        if path.is_symlink():
            raise ValueError(f"symlink not allowed in vault source: {rel}")
        target = vault / f"{rel}.age"
        target.parent.mkdir(parents=True, exist_ok=True)
        cmd = ["age", "--encrypt", "-o", str(target)]
        for key in keys:
            cmd += ["-r", key]
        code, text = _run(cmd + [str(path)])
        if code != 0:
            raise RuntimeError(f"age failed for {rel}: {text}")
        entries.append({"path": rel, "sha256": _sha256(path), "bytes": path.stat().st_size,
                        "ciphertext": f"{rel}.age", "ciphertext_sha256": _sha256(target)})
    index = {"schema": "evo.rea-source-vault.v1", "tool": "age", "vault": name,
             "recipients": sorted(_fingerprint(k) for k in keys),
             "members": sorted(({"label": m["label"], "role": m["role"], "fingerprint": _fingerprint(_public_key(m["key"])),
                                 "can_decrypt": m["can_decrypt"]} for m in members),
                               key=lambda m: m["fingerprint"]),
             "files": entries,
             "limits": "Encryption protects files at rest and in transit, not after an authorised person decrypts them."}
    (vault / INDEX).write_text(json.dumps(index, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return index


def _index(vault: Path) -> dict:
    index = json.loads((vault / INDEX).read_text(encoding="utf-8"))
    for entry in index["files"]:
        if PurePosixPath(entry["ciphertext"]).is_absolute() or ".." in PurePosixPath(entry["ciphertext"]).parts:
            raise ValueError(f"unsafe path in vault index: {entry['ciphertext']}")
    return index


def vault_verify(vault: Path) -> list[str]:
    problems = []
    for entry in _index(vault)["files"]:
        path = vault / entry["ciphertext"]
        if path.is_symlink() or not path.is_file():
            problems.append(f"missing: {entry['ciphertext']}")
        elif _sha256(path) != entry["ciphertext_sha256"]:
            problems.append(f"changed: {entry['ciphertext']}")
    return problems


def vault_open(vault: Path, identity: Path, destination: Path) -> list[str]:
    problems = vault_verify(vault)
    if problems:
        raise ValueError("vault failed verification: " + "; ".join(problems))
    opened = []
    for entry in _index(vault)["files"]:
        out = destination / entry["path"]
        out.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=out.parent, delete=False) as tmp:
            tmp_path = Path(tmp.name)
        code, text = _run(["age", "--decrypt", "-i", str(identity), "-o", str(tmp_path), str(vault / entry["ciphertext"])])
        if code != 0 or _sha256(tmp_path) != entry["sha256"]:
            tmp_path.unlink(missing_ok=True)
            raise RuntimeError(f"could not open {entry['path']} with this identity")
        tmp_path.replace(out)
        opened.append(entry["path"])
    return opened


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("discover", "build"):
        p = sub.add_parser(name)
        p.add_argument("--evo", default=Path(__file__).resolve().parents[2], type=Path,
                       help="EVO checkout (default: this repository)")
        p.add_argument("--rea", required=True, type=Path, help="REA checkout")
        p.add_argument("--dest", required=True, type=Path, help="install destination for the fused product")
        p.add_argument("--vaults", type=Path, default=DEFAULT_VAULTS, help="vault definitions (vaults.json)")
        p.add_argument("--pin", default=CONTRACT_PIN)
        if name == "build":
            p.add_argument("--execute", action="store_true")
    p = sub.add_parser("pdf")
    p.add_argument("source", type=Path)
    p.add_argument("--out", required=True, type=Path)
    p.add_argument("--print", action="store_true", dest="send_to_printer")
    p = sub.add_parser("ui")
    p.add_argument("component", choices=("evo", "rea"))
    p.add_argument("--dest", required=True, type=Path, help="fused product destination from build")
    p.add_argument("--execute", action="store_true")
    p.add_argument("args", nargs=argparse.REMAINDER)
    v = sub.add_parser("vault").add_subparsers(dest="action", required=True)
    s = v.add_parser("list", help="show each vault, its members, roles and whether anyone can open it")
    s.add_argument("--vaults", type=Path, default=DEFAULT_VAULTS)
    s = v.add_parser("seal")
    s.add_argument("name", help="vault name from vaults.json")
    s.add_argument("source", type=Path)
    s.add_argument("--out", required=True, type=Path, help="where the encrypted vault is written")
    s.add_argument("--vaults", type=Path, default=DEFAULT_VAULTS)
    s = v.add_parser("add-member", help="add a person's key to a vault, with a view or edit role")
    s.add_argument("name", help="vault name (created if new)")
    s.add_argument("source", choices=sorted(KEY_SOURCES) + ["age"])
    s.add_argument("who", help="sshid/github username, or an age1 public key")
    s.add_argument("--label", required=True, help="the person's name")
    s.add_argument("--role", choices=ROLES, default="view")
    s.add_argument("--can-decrypt", action="store_true",
                   help="for github keys: the owner confirms the private key is a file age can use")
    s.add_argument("--vaults", type=Path, default=DEFAULT_VAULTS)
    s.add_argument("--yes", action="store_true", help="write after showing the fingerprints")
    s = v.add_parser("verify")
    s.add_argument("vault", type=Path)
    s = v.add_parser("open")
    s.add_argument("vault", type=Path)
    s.add_argument("--identity", required=True, type=Path)
    s.add_argument("--dest", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command in ("discover", "build"):
            report = discover(args.evo, args.rea, args.dest, args.vaults, args.pin)
            if args.command == "discover":
                print(json.dumps(report, indent=2, sort_keys=True))
                return 0 if not report["blocking"] else 3
            plan = build_plan(args.evo, args.rea, args.dest)
            print(json.dumps({"blocking": report["blocking"], "execute": args.execute,
                              "plan": [{"cwd": str(cwd), "run": " ".join(cmd)} for cwd, cmd in plan]}, indent=2))
            if report["blocking"]:
                return 3
            if args.execute:
                args.dest.mkdir(parents=True, exist_ok=True)
                for cwd, cmd in plan:
                    if subprocess.run(cmd, cwd=cwd).returncode != 0:
                        return 1
                print(write_layout(args.evo, args.rea, args.dest, args.pin))
            return 0
        if args.command == "pdf":
            backend = render_pdf(args.source, args.out)
            print(f"{args.out} ({backend})")
            if args.send_to_printer:
                printer = next((c for c in PRINT_COMMANDS if shutil.which(c)), None)
                if not printer:
                    print("no lp/lpr found; PDF written but not printed", file=sys.stderr)
                    return 3
                return subprocess.run([printer, str(args.out)]).returncode
            return 0
        if args.command == "ui":
            cwd, cmd = ui_command(args.dest, args.component, args.args)
            print(" ".join(cmd))
            return subprocess.run(cmd, cwd=cwd).returncode if args.execute else 0
        if args.action == "list":
            for name, vault in sorted(load_vaults(args.vaults)["vaults"].items()):
                can_open = any(m["can_decrypt"] for m in vault["members"])
                print(f"{name}: {vault.get('description', '')} ({'openable' if can_open else 'NOBODY CAN OPEN YET'})")
                for m in vault["members"]:
                    print(f"  {m['label']} [{m['role']}] {_fingerprint(_public_key(m['key']))}"
                          f"{'' if m['can_decrypt'] else ' (encrypt-only, device-bound)'}")
            return 0
        if args.action == "seal":
            members = vault_members(load_vaults(args.vaults), args.name)
            index = vault_seal(args.source, args.out, members, args.name)
            print(f"sealed {len(index['files'])} file(s) into vault {args.name} for {len(index['members'])} member(s)")
            return 0
        if args.action == "add-member":
            if args.source == "age":
                url, keys, can_decrypt = "age-keygen", [_public_key(args.who)], True
            else:
                url, keys = fetch_public_keys(args.source, args.who)
                # Termius SSH ID passkeys are device-bound and cannot be exported, so age cannot decrypt with them.
                can_decrypt = args.source == "github" and args.can_decrypt
            for key in keys:
                print(f"{url}: {key.split()[0]} fingerprint {_fingerprint(key)}"
                      f"{'' if can_decrypt else ' (encrypt-only)'}")
            if not args.yes:
                print("compare the fingerprints with the key owner, then rerun with --yes to add")
                return 0
            added = add_members(args.vaults, args.name, args.label, args.role, url, keys, can_decrypt)
            print(f"added {len(added)} key(s) to vault {args.name}" if added else "already a member")
            return 0
        if args.action == "verify":
            problems = vault_verify(args.vault)
            print("\n".join(problems) or "vault verified")
            return 3 if problems else 0
        opened = vault_open(args.vault, args.identity, args.dest)
        print(f"opened {len(opened)} file(s) into {args.dest}")
        return 0
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
