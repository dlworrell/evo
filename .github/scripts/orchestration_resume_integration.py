from __future__ import annotations

import json
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"missing {label}")
    return text.replace(old, new, 1)


# Canonical CTest registration.
path = Path("tests/CMakeLists.txt")
text = path.read_text()
if "evo_project_orchestration_resume_test" not in text:
    marker = '''add_test(
    NAME evo_project_orchestration_checkpoint_test
    COMMAND evo_project_orchestration_checkpoint_test
)
'''
    block = marker + '''
add_executable(
    evo_project_orchestration_resume_test
    project_orchestration_resume_test.c
)
target_include_directories(
    evo_project_orchestration_resume_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(
    evo_project_orchestration_resume_test
    PRIVATE
        catalyst_evo_project_foundation
)
add_test(
    NAME evo_project_orchestration_resume_test
    COMMAND evo_project_orchestration_resume_test
)
'''
    text = replace_once(text, marker, block, "checkpoint CTest block")
path.write_text(text)


# Autotools parity.
path = Path("Makefile.am")
text = path.read_text()
if "tests/evo_project_orchestration_resume_test" not in text:
    marker = '''\ttests/evo_project_orchestration_test \\
\ttests/evo_project_orchestration_checkpoint_test \\
\tbenchmarks/evo_core_benchmark \\
'''
    replacement = '''\ttests/evo_project_orchestration_test \\
\ttests/evo_project_orchestration_checkpoint_test \\
\ttests/evo_project_orchestration_resume_test \\
\tbenchmarks/evo_core_benchmark \\
'''
    text = replace_once(text, marker, replacement, "Autotools check program block")
if "tests_evo_project_orchestration_resume_test_SOURCES" not in text:
    marker = '''tests_evo_project_orchestration_checkpoint_test_SOURCES = \\
\ttests/project_orchestration_checkpoint_test.c
tests_evo_project_orchestration_checkpoint_test_LDADD = \\
\tlibevo_project_foundation.la
'''
    block = marker + '''

tests_evo_project_orchestration_resume_test_SOURCES = \\
\ttests/project_orchestration_resume_test.c
tests_evo_project_orchestration_resume_test_LDADD = \\
\tlibevo_project_foundation.la
'''
    text = replace_once(text, marker, block, "Autotools checkpoint test block")
path.write_text(text)


# AES-BLD-001 normative inventory.
path = Path(".aems/aes-bld-001.json")
data = json.loads(path.read_text())
normative = data["build"]["normative_tests"]
if not any(item.get("id") == "project-orchestration-resume" for item in normative):
    insert_at = next(
        index + 1
        for index, item in enumerate(normative)
        if item.get("id") == "project-orchestration-checkpoint"
    )
    normative.insert(
        insert_at,
        {
            "id": "project-orchestration-resume",
            "source": "tests/project_orchestration_resume_test.c",
            "cmake": "evo_project_orchestration_resume_test",
            "autotools": "tests/evo_project_orchestration_resume_test",
        },
    )
path.write_text(json.dumps(data, indent=2) + "\n")


# Independent structural validation must require the resume differential and
# stale-preflight oracle, not merely the checkpoint serializer unit tests.
path = Path("tests/validate_project_orchestration.py")
text = path.read_text()
if 'resume_test = read("tests/project_orchestration_resume_test.c")' not in text:
    text = replace_once(
        text,
        '    checkpoint_test = read("tests/project_orchestration_checkpoint_test.c")\n',
        '    checkpoint_test = read("tests/project_orchestration_checkpoint_test.c")\n'
        '    resume_test = read("tests/project_orchestration_resume_test.c")\n',
        "validator checkpoint test read",
    )
if "resumed bounded external evaluation matches uninterrupted execution" not in text:
    marker = '''    for fixture in (
        "same committed boundary replays byte-identical checkpoint evidence",
        "stale toolchain rejects before resume state is published",
        "stale baseline rejects",
        "stale catalogue rejects",
        "stale workload rejects",
        "stale orchestration policy rejects",
        "nested checkpoint corruption fails integrity before resume",
        "unsupported product checkpoint version rejects",
        "truncated product checkpoint rejects",
    ):
        require(fixture in checkpoint_test, f"checkpoint oracle missing: {fixture}")
'''
    block = marker + '''    for fixture in (
        "stale product identity rejects before external candidate execution",
        "resumed bounded external evaluation matches uninterrupted execution",
        "resume schedules only post-checkpoint generations",
    ):
        require(fixture in resume_test, f"resume oracle missing: {fixture}")
'''
    text = replace_once(text, marker, block, "checkpoint validator oracle block")
if '"evo_project_orchestration_resume_test",' not in text:
    marker = '''    for target in (
        "evo_project_orchestration_test",
        "evo_project_orchestration_checkpoint_test",
    ):
'''
    replacement = '''    for target in (
        "evo_project_orchestration_test",
        "evo_project_orchestration_checkpoint_test",
        "evo_project_orchestration_resume_test",
    ):
'''
    text = replace_once(text, marker, replacement, "validator target list")
path.write_text(text)


# Hosted project-orchestration workflow executes the resume differential under
# Clang+sanitizers, GCC/Autotools, and Apple Clang.
path = Path(".github/workflows/project-orchestration.yml")
text = path.read_text()
text = text.replace(
    "evo_project_orchestration_test evo_project_orchestration_checkpoint_test evo_project_search_test",
    "evo_project_orchestration_test evo_project_orchestration_checkpoint_test evo_project_orchestration_resume_test evo_project_search_test",
)
text = text.replace(
    "tests/evo_project_orchestration_test tests/evo_project_orchestration_checkpoint_test tests/evo_project_search_test",
    "tests/evo_project_orchestration_test tests/evo_project_orchestration_checkpoint_test tests/evo_project_orchestration_resume_test tests/evo_project_search_test",
)
text = text.replace(
    "TESTS='tests/evo_project_orchestration_test tests/evo_project_orchestration_checkpoint_test tests/evo_project_search_test'",
    "TESTS='tests/evo_project_orchestration_test tests/evo_project_orchestration_checkpoint_test tests/evo_project_orchestration_resume_test tests/evo_project_search_test'",
)
text = text.replace(
    "^evo_project_(orchestration|orchestration_checkpoint|search)_test$",
    "^evo_project_(orchestration|orchestration_checkpoint|orchestration_resume|search)_test$",
)
path.write_text(text)
