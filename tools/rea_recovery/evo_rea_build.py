#!/usr/bin/env python3
"""EVO/REA fused build runner and encrypted source vault (EVO ADR-0048, proposed).

Builds the fused EVO + REA product into one destination: EVO from its own
checkout with its documented CMake build, and REA from a separate checkout
pinned by ADR-0046 with its documented npm build, without changing REA's files.
Commands: ``discover`` (environment report), ``build`` (plan; ``--execute`` to
run), ``pdf`` (render and optionally print a report such as ``evo report``
output), ``ui`` (EVO CLI, REA CLI or REA's stdio MCP server), and
``vault`` (age encryption; people listed once by sshid username and granted
view or edit per vault).

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
        openable = sorted(n for n in config["vaults"] if any(m["can_decrypt"] for m in vault_members(config, n)))
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
PERSON_ID = re.compile(r"[a-z0-9](?:[a-z0-9-]{0,38})")
DEFAULT_VAULTS = Path(__file__).resolve().parent / "vaults.json"
SCHEMA_V1, SCHEMA = "evo.rea-vaults.v1", "evo.rea-vaults.v2"


def _upgrade_v1(config: dict) -> dict:
    """v1 listed whole keys inside each vault; v2 lists people once and grants them per vault."""
    people: dict = {}
    vaults: dict = {}
    for name, vault in config["vaults"].items():
        grants = []
        for member in vault.get("members", []):
            pid = re.sub(r"[^a-z0-9-]+", "-", str(member.get("label", "")).lower()).strip("-")[:39]
            person = people.setdefault(pid, {"name": member.get("label"), "keys": []})
            if member.get("key") not in [k["key"] for k in person["keys"]]:
                person["keys"].append({k: member.get(k) for k in ("key", "source", "can_decrypt", "added")})
            role = member.get("role")
            existing = next((g for g in grants if g["person"] == pid), None)
            if existing is None:
                grants.append({"person": pid, "role": role})
            elif role == "edit":
                existing["role"] = "edit"
        vaults[name] = {"description": vault.get("description", ""), "members": grants}
    return {"schema": SCHEMA, "people": people, "vaults": vaults}


def load_vaults(path: Path) -> dict:
    """Termius-style vaults: people are listed once with their keys and granted view or edit per vault.

    A v1 file (keys listed inside each vault) is upgraded in memory and written back as v2.
    """
    config = json.loads(path.read_text(encoding="utf-8"))
    if config.get("schema") == SCHEMA_V1 and isinstance(config.get("vaults"), dict):
        config = _upgrade_v1(config)
    if config.get("schema") != SCHEMA or not isinstance(config.get("vaults"), dict) \
            or not isinstance(config.get("people"), dict):
        raise ValueError(f"{path} is not an {SCHEMA} file")
    owners: dict = {}
    for pid, person in config["people"].items():
        if not PERSON_ID.fullmatch(pid) or not person.get("name"):
            raise ValueError(f"bad person id or missing name: {pid!r}")
        keys = person.get("keys")
        if not isinstance(keys, list) or not keys:
            raise ValueError(f"person {pid}: needs at least one key")
        for entry in keys:
            if not isinstance(entry.get("can_decrypt"), bool):
                raise ValueError(f"person {pid}: every key needs can_decrypt true/false")
            key = _public_key(entry.get("key", ""))
            if key in owners:
                raise ValueError(f"key {_fingerprint(key)} belongs to both {owners[key]} and {pid}")
            owners[key] = pid
    for name, vault in config["vaults"].items():
        if not VAULT_NAME.fullmatch(name):
            raise ValueError(f"bad vault name: {name!r}")
        members = vault.get("members")
        if not isinstance(members, list):
            raise ValueError(f"vault {name}: members must be a list")
        seen = set()
        for grant in members:
            if grant.get("person") not in config["people"]:
                raise ValueError(f"vault {name}: unknown person {grant.get('person')!r}")
            if grant.get("role") not in ROLES:
                raise ValueError(f"vault {name}: role must be view or edit")
            if grant["person"] in seen:
                raise ValueError(f"vault {name}: {grant['person']} is listed twice")
            seen.add(grant["person"])
        if members and not any(g["role"] == "edit" for g in members):
            raise ValueError(f"vault {name}: needs at least one member who can edit")
    return config


def save_vaults(path: Path, config: dict) -> None:
    """Validate, then replace ``path``; an invalid change never reaches the file."""
    with tempfile.NamedTemporaryFile("w", dir=path.parent, suffix=".json", delete=False,
                                     encoding="utf-8") as tmp:
        tmp.write(json.dumps(config, indent=2, sort_keys=True) + "\n")
    try:
        load_vaults(Path(tmp.name))
        Path(tmp.name).replace(path)
    finally:
        Path(tmp.name).unlink(missing_ok=True)


def person_access(config: dict, pid: str) -> dict[str, str]:
    """The vaults ``pid`` is granted, and the role in each: access is per vault, not per key."""
    return {name: g["role"] for name, vault in sorted(config["vaults"].items())
            for g in vault["members"] if g["person"] == pid}


def vault_members(config: dict, name: str) -> list[dict]:
    """Every key of every person granted vault ``name``, with that person's role in it."""
    if name not in config["vaults"]:
        raise ValueError(f"no vault named {name!r}; known: {', '.join(sorted(config['vaults']))}")
    members = []
    for grant in config["vaults"][name]["members"]:
        person = config["people"][grant["person"]]
        for entry in person["keys"]:
            members.append({"person": grant["person"], "label": person["name"], "role": grant["role"],
                            "key": entry["key"], "can_decrypt": entry["can_decrypt"],
                            "device": entry.get("device", "")})
    return members


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


def _load_or_new(path: Path) -> dict:
    return load_vaults(path) if path.is_file() else {"schema": SCHEMA, "people": {}, "vaults": {}}


def add_person_keys(path: Path, pid: str, name: str, source: str, keys: list[str],
                    can_decrypt: bool, device: str = "") -> list[str]:
    """Add keys to person ``pid`` (created if new). The id is the person's sshid username."""
    pid = pid.lower()
    if not PERSON_ID.fullmatch(pid):
        raise ValueError("person id must be their sshid username: lower-case letters, digits and dashes")
    config = _load_or_new(path)
    person = config["people"].setdefault(pid, {"name": name or pid, "keys": []})
    if name:
        person["name"] = name
    existing = {_public_key(k["key"]) for k in person["keys"]}
    added = []
    for key in (_public_key(k) for k in keys):
        if key not in existing:
            entry = {"key": key, "source": source, "can_decrypt": can_decrypt, "added": date.today().isoformat()}
            if device:
                entry["device"] = device
            person["keys"].append(entry)
            added.append(_fingerprint(key))
    save_vaults(path, config)
    return added


def remove_person_key(path: Path, pid: str, fingerprint: str) -> list[str]:
    """Remove one key (say, a lost phone's) from ``pid``. Returns the vaults that must be re-sealed.

    The person keeps their other keys and grants. A person's last key cannot be removed:
    revoke their vault grants instead.
    """
    config = load_vaults(path)
    if pid not in config["people"]:
        raise ValueError(f"unknown person {pid!r}")
    keys = config["people"][pid]["keys"]
    remaining = [k for k in keys if _fingerprint(_public_key(k["key"])) != fingerprint]
    if len(remaining) == len(keys):
        raise ValueError(f"{pid} has no key with fingerprint {fingerprint}; see `vault list`")
    if not remaining:
        raise ValueError(f"that is {pid}'s last key; revoke their vault grants instead")
    config["people"][pid]["keys"] = remaining
    save_vaults(path, config)
    return list(person_access(config, pid))


def grant(path: Path, vault: str, pid: str, role: str, description: str = "") -> str:
    """Grant person ``pid`` view or edit on ``vault`` (created if new). Returns what changed."""
    if role not in ROLES or not VAULT_NAME.fullmatch(vault):
        raise ValueError("role must be view or edit, and the vault name lower-case letters, digits and dashes")
    config = _load_or_new(path)
    if pid not in config["people"]:
        raise ValueError(f"unknown person {pid!r}; add their key first with `vault person add`")
    entry = config["vaults"].setdefault(vault, {"description": description, "members": []})
    current = next((g for g in entry["members"] if g["person"] == pid), None)
    if current and current["role"] == role:
        return "unchanged"
    if current:
        current["role"] = role
    else:
        entry["members"].append({"person": pid, "role": role})
    if not any(g["role"] == "edit" for g in entry["members"]):
        raise ValueError(f"vault {vault} needs at least one member who can edit; grant an editor first")
    save_vaults(path, config)
    return f"{pid} is now {role} on {vault}"


def revoke(path: Path, vault: str, pid: str) -> None:
    """Remove ``pid`` from ``vault``. Only later seals exclude them; copies they already had stay readable."""
    config = load_vaults(path)
    if vault not in config["vaults"]:
        raise ValueError(f"no vault named {vault!r}")
    members = config["vaults"][vault]["members"]
    remaining = [g for g in members if g["person"] != pid]
    if len(remaining) == len(members):
        raise ValueError(f"{pid} is not a member of {vault}")
    if remaining and not any(g["role"] == "edit" for g in remaining):
        raise ValueError(f"{pid} is the last editor of {vault}; grant another editor first")
    config["vaults"][vault]["members"] = remaining
    save_vaults(path, config)


MANIFEST = "manifest.age"


def _age_encrypt(keys: list[str], source: Path, target: Path) -> None:
    cmd = ["age", "--encrypt", "-o", str(target)]
    for key in keys:
        cmd += ["-r", key]
    code, text = _run(cmd + [str(source)])
    if code != 0:
        raise RuntimeError(f"age failed: {text}")


def vault_seal(source: Path, vault: Path, members: list[dict], name: str = "") -> dict:
    """Encrypt every file under ``source`` into ``vault``.

    File names and directory structure are sensitive too: ciphertext is stored as
    numbered objects, and the map from object to path lives only in the encrypted
    ``manifest.age``. The public index holds object hashes, the vault name and
    member fingerprints, so ``vault verify`` works without a key.
    """
    keys = [_public_key(m["key"]) for m in members]
    if not keys:
        raise ValueError("the vault has no members")
    if not any(m.get("can_decrypt") for m in members):
        raise ValueError("no member of this vault can decrypt (device-bound keys such as SSH ID passkeys "
                         "cannot open age files); add an age1 key or a recovery key first")
    if vault.resolve() == source.resolve() or source.resolve() in vault.resolve().parents:
        raise ValueError("the vault must be outside the source directory")
    if vault.exists() and any(vault.iterdir()):
        raise ValueError(f"{vault} is not empty; seal into a new directory and replace the old vault after verify")
    files = sorted(p for p in source.rglob("*") if p.is_file() or p.is_symlink())
    for path in files:
        if path.is_symlink():
            raise ValueError(f"symlink not allowed in vault source: {path.relative_to(source).as_posix()}")
    (vault / "objects").mkdir(parents=True, exist_ok=True)
    manifest, objects = [], []
    for number, path in enumerate(files, 1):
        obj = f"objects/{number:06d}.age"
        _age_encrypt(keys, path, vault / obj)
        manifest.append({"path": path.relative_to(source).as_posix(), "sha256": _sha256(path),
                         "bytes": path.stat().st_size, "ciphertext": obj})
        objects.append({"ciphertext": obj, "ciphertext_sha256": _sha256(vault / obj)})
    with tempfile.TemporaryDirectory() as tmp:
        plain = Path(tmp) / "manifest.json"
        plain.write_text(json.dumps({"vault": name, "files": manifest}, indent=2, sort_keys=True), encoding="utf-8")
        _age_encrypt(keys, plain, vault / MANIFEST)
    index = {"schema": "evo.rea-source-vault.v2", "tool": "age", "vault": name,
             "manifest": {"ciphertext": MANIFEST, "ciphertext_sha256": _sha256(vault / MANIFEST)},
             "recipients": sorted(_fingerprint(k) for k in keys),
             "members": sorted(({"person": m.get("person", ""), "label": m["label"], "role": m["role"],
                                 "fingerprint": _fingerprint(_public_key(m["key"])),
                                 "can_decrypt": m["can_decrypt"]} for m in members),
                               key=lambda m: m["fingerprint"]),
             "files": objects,
             "limits": "Names and structure are encrypted; the number of files and their approximate sizes are not. "
                       "Encryption protects files at rest and in transit, not after an authorised person decrypts them."}
    (vault / INDEX).write_text(json.dumps(index, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return index


def _safe(rel: str) -> str:
    path = PurePosixPath(rel)
    if not rel or "\\" in rel or path.is_absolute() or ".." in path.parts:
        raise ValueError(f"unsafe path in vault: {rel}")
    return rel


def _index(vault: Path) -> dict:
    index = json.loads((vault / INDEX).read_text(encoding="utf-8"))
    for entry in index["files"] + ([index["manifest"]] if "manifest" in index else []):
        _safe(entry["ciphertext"])
    return index


def vault_verify(vault: Path) -> list[str]:
    """Check every ciphertext against the public index, without a key."""
    index = _index(vault)
    problems = []
    for entry in index["files"] + ([index["manifest"]] if "manifest" in index else []):
        path = vault / entry["ciphertext"]
        if path.is_symlink() or not path.is_file():
            problems.append(f"missing: {entry['ciphertext']}")
        elif _sha256(path) != entry["ciphertext_sha256"]:
            problems.append(f"changed: {entry['ciphertext']}")
    return problems


def _decrypt(identity: Path, source: Path, target: Path) -> bool:
    code, _ = _run(["age", "--decrypt", "-i", str(identity), "-o", str(target), str(source)])
    return code == 0


def vault_open(vault: Path, identity: Path, destination: Path) -> list[str]:
    problems = vault_verify(vault)
    if problems:
        raise ValueError("vault failed verification: " + "; ".join(problems))
    index = _index(vault)
    if "manifest" in index:
        with tempfile.TemporaryDirectory() as tmp:
            plain = Path(tmp) / "manifest.json"
            if not _decrypt(identity, vault / index["manifest"]["ciphertext"], plain):
                raise RuntimeError("could not open this vault with this identity")
            entries = json.loads(plain.read_text(encoding="utf-8"))["files"]
        listed = {e["ciphertext"] for e in index["files"]}
        if sorted(e["ciphertext"] for e in entries) != sorted(listed):
            raise ValueError("vault manifest does not match the index")
    else:  # evo.rea-source-vault.v1: plaintext paths in the index
        entries = index["files"]
    opened = []
    for entry in entries:
        out = destination / _safe(entry["path"])
        out.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=out.parent, delete=False) as tmp:
            tmp_path = Path(tmp.name)
        if not _decrypt(identity, vault / entry["ciphertext"], tmp_path) or _sha256(tmp_path) != entry["sha256"]:
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
    s = v.add_parser("person", help="register a person by their sshid username and add their public keys")
    s.add_argument("op", choices=("add", "remove-key"))
    s.add_argument("person", help="the person's sshid username (their id in every vault)")
    s.add_argument("source", nargs="?", choices=sorted(KEY_SOURCES) + ["age"],
                   help="add: where the key comes from")
    s.add_argument("who", nargs="?", help="add: github username or age1 key (sshid uses the person id)")
    s.add_argument("--device", default="", help="add: which device holds this key, e.g. iphone or mac")
    s.add_argument("--fingerprint", default="", help="remove-key: the key's fingerprint from `vault list`")
    s.add_argument("--name", default="", help="the person's display name")
    s.add_argument("--can-decrypt", action="store_true",
                   help="github only: the key is a file-based ed25519 key age can decrypt with")
    s.add_argument("--vaults", type=Path, default=DEFAULT_VAULTS)
    s.add_argument("--yes", action="store_true", help="write after showing the fingerprints")
    s = v.add_parser("grant", help="give a person view or edit on one vault")
    s.add_argument("name", help="vault name (created if new)")
    s.add_argument("person")
    s.add_argument("--role", choices=ROLES, default="view")
    s.add_argument("--description", default="")
    s.add_argument("--vaults", type=Path, default=DEFAULT_VAULTS)
    s = v.add_parser("revoke", help="remove a person from one vault (re-seal afterwards)")
    s.add_argument("name")
    s.add_argument("person")
    s.add_argument("--vaults", type=Path, default=DEFAULT_VAULTS)
    s = v.add_parser("access", help="show which vaults a person can open or edit")
    s.add_argument("person")
    s.add_argument("--vaults", type=Path, default=DEFAULT_VAULTS)
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
            config = load_vaults(args.vaults)
            for name, vault in sorted(config["vaults"].items()):
                members = vault_members(config, name)
                can_open = sorted({m["person"] for m in members if m["can_decrypt"]})
                print(f"{name}: {vault.get('description', '')} "
                      f"({'can open: ' + ', '.join(can_open) if can_open else 'NOBODY CAN OPEN YET'})")
                for m in members:
                    print(f"  {m['label']} ({m['person']}) [{m['role']}] {_fingerprint(_public_key(m['key']))}"
                          f"{' on ' + m['device'] if m['device'] else ''}"
                          f"{'' if m['can_decrypt'] else ' (encrypt-only, device-bound)'}")
            return 0
        if args.action == "seal":
            members = vault_members(load_vaults(args.vaults), args.name)
            index = vault_seal(args.source, args.out, members, args.name)
            print(f"sealed {len(index['files'])} file(s) into vault {args.name} for {len(index['members'])} key(s)")
            return 0
        if args.action == "person" and args.op == "remove-key":
            if not args.fingerprint:
                raise ValueError("remove-key needs --fingerprint (see `vault list`)")
            vaults = remove_person_key(args.vaults, args.person, args.fingerprint)
            print(f"removed key {args.fingerprint} from {args.person}. Re-seal now: {', '.join(vaults) or 'no vaults'}. "
                  "Until then the old sealed copies still open with that key.")
            return 0
        if args.action == "person":
            if not args.source:
                raise ValueError("add needs a source: sshid, github or age")
            if args.source == "age":
                url, keys, can_decrypt = "age-keygen", [_public_key(args.who or "")], True
            else:
                user = args.person if args.source == "sshid" else args.who
                if not user:
                    raise ValueError("github needs the person's GitHub username")
                url, keys = fetch_public_keys(args.source, user)
                # Termius SSH ID passkeys are device-bound and cannot be exported, so age cannot decrypt with them.
                can_decrypt = args.source == "github" and args.can_decrypt
            for key in keys:
                print(f"{url}: {key.split()[0]} fingerprint {_fingerprint(key)}"
                      f"{'' if can_decrypt else ' (encrypt-only)'}")
            if not args.yes:
                print("compare the fingerprints with the key owner, then rerun with --yes to add")
                return 0
            added = add_person_keys(args.vaults, args.person, args.name, url, keys, can_decrypt, args.device)
            print(f"added {len(added)} key(s) to {args.person.lower()}; grant vaults with `vault grant`"
                  if added else "already registered")
            return 0
        if args.action == "grant":
            print(grant(args.vaults, args.name, args.person, args.role, args.description))
            return 0
        if args.action == "revoke":
            revoke(args.vaults, args.name, args.person)
            print(f"removed {args.person} from {args.name}. Re-seal {args.name} now; files {args.person} "
                  "already copied or decrypted stay readable to them, so rotate anything secret in them.")
            return 0
        if args.action == "access":
            config = load_vaults(args.vaults)
            if args.person not in config["people"]:
                raise ValueError(f"unknown person {args.person!r}")
            person = config["people"][args.person]
            opens = any(k["can_decrypt"] for k in person["keys"])
            for name, role in person_access(config, args.person).items():
                print(f"{name}: {role}{'' if opens else ' (cannot open yet: no decrypting key)'}")
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
