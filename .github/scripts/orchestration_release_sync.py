from __future__ import annotations

from pathlib import Path


def replace_once(path: str, old: str, new: str, label: str) -> None:
    file_path = Path(path)
    text = file_path.read_text()
    if old not in text:
        raise SystemExit(f"missing {label} in {path}")
    file_path.write_text(text.replace(old, new, 1))


def append_after_once(path: str, marker: str, addition: str, label: str) -> None:
    file_path = Path(path)
    text = file_path.read_text()
    if addition.strip() in text:
        return
    if marker not in text:
        raise SystemExit(f"missing {label} in {path}")
    file_path.write_text(text.replace(marker, marker + addition, 1))


# Package/version parity.
replace_once(
    "CMakeLists.txt",
    "project(catalyst_evo VERSION 0.41.0 LANGUAGES C)",
    "project(catalyst_evo VERSION 0.42.0 LANGUAGES C)",
    "CMake version",
)
replace_once(
    "configure.ac",
    "  [0.41.0],",
    "  [0.42.0],",
    "Autotools version",
)
replace_once(
    "include/catalyst/evo/evo.h",
    "#define EVO_VERSION_MINOR 37",
    "#define EVO_VERSION_MINOR 42",
    "public version macro",
)

# README: governing records, boundary, inventory, and current status.
append_after_once(
    "README.md",
    "- `docs/adr/ADR-0042-structured-recipe-evolution.md`\n",
    "- `docs/adr/ADR-0043-bounded-parallel-source-orchestration.md`\n",
    "ADR-0042 list entry",
)
append_after_once(
    "README.md",
    "EVO-HRA-014 retains this structured-search audit.\n",
    "ADR-0043 assesses 0.42.0 bounded source orchestration: external candidate\n"
    "work is admitted through explicit resource/capability policy, asynchronous\n"
    "completion remains diagnostic, committed evidence stays in stable candidate\n"
    "order, every started worker is canceled/joined before failure authority, and\n"
    "product checkpoints bind the complete source-optimizer dependency identity.\n"
    "Persistent search traces retain exact batch/job evidence without making a\n"
    "runtime queue, process handle, or completion timestamp authoritative.\n"
    "EVO-HRA-015 retains this orchestration-specific audit.\n",
    "structured-search HRA paragraph",
)
replace_once(
    "README.md",
    "- Bounded parallel compilation, checkpoint/resume, and deterministic replay\n",
    "- Implemented bounded external-process orchestration with stable result-commit\n"
    "  order, persistent worker traces, resource/capability policy, product\n"
    "  checkpoint identity, and deterministic resume\n",
    "source optimizer orchestration roadmap bullet",
)
replace_once(
    "README.md",
    "and finite EVO fitness only for complete stable evidence. Version 0.41.0 adds\n"
    "deterministic structured recipe populations, whole-record/typed-parameter\n"
    "mutation and crossover, exact repair/rejection policy, complete operator and\n"
    "lineage evidence, and fixed-seed replay through the measurement/fitness boundary.\n"
    "Bounded external-process orchestration and the installed executable remain\n"
    "dependency-ordered roadmap work; their absence is an explicit boundary, not an\n"
    "implicit feature claim.\n",
    "and finite EVO fitness only for complete stable evidence. Version 0.41.0 adds\n"
    "deterministic structured recipe populations, whole-record/typed-parameter\n"
    "mutation and crossover, exact repair/rejection policy, complete operator and\n"
    "lineage evidence, and fixed-seed replay through the measurement/fitness boundary.\n"
    "Version 0.42.0 adds bounded external-process source orchestration, deterministic\n"
    "candidate/workspace/logical-worker assignment, stable result commit independent\n"
    "of completion order, persistent exact worker traces, fail-closed cleanup, and\n"
    "product checkpoint identity with uninterrupted-versus-resumed differential\n"
    "evidence. The installed executable remains dependency-ordered roadmap work.\n",
    "README private foundation boundary",
)
replace_once(
    "README.md",
    "26\ninstalled-core sources, 27 private source-foundation sources, and 40\nnormative tests.",
    "26\ninstalled-core sources, 35 private source-foundation sources, and 44\nnormative tests.",
    "README build inventory counts",
)
replace_once(
    "README.md",
    "**Current implementation boundary:** EVO 0.41.0 packages the deterministic\n"
    "evolutionary-search core plus the private source-optimizer foundations for\n"
    "strict C-project ingestion, immutable baselines, normalized Clang/LLVM\n"
    "analysis, canonical transformation recipes, three AST-aware C\n"
    "transformations, isolated candidate materialization, candidate assurance,\n"
    "reproducible candidate performance measurement, and structured recipe search.\n",
    "**Current implementation boundary:** EVO 0.42.0 packages the deterministic\n"
    "evolutionary-search core plus the private source-optimizer foundations for\n"
    "strict C-project ingestion, immutable baselines, normalized Clang/LLVM\n"
    "analysis, canonical transformation recipes, three AST-aware C\n"
    "transformations, isolated candidate materialization, candidate assurance,\n"
    "reproducible candidate performance measurement, structured recipe search, and\n"
    "bounded external-process source orchestration.\n",
    "README current implementation heading",
)
replace_once(
    "README.md",
    "Fixed seeds replay the same search and exact\n"
    "fitness ties preserve the earlier stable winner. The boundary remains private\n"
    "and uninstalled. Bounded external-process orchestration (#66), product commands\n"
    "and installed executable (#67/#93), artifact publication (#68), end-to-end\n"
    "proof (#69), and final stabilization (#56) remain later dependency-ordered work.\n",
    "Fixed seeds replay the same search and exact fitness ties preserve the earlier\n"
    "stable winner. External candidate batches may complete asynchronously, but EVO\n"
    "commits only stable candidate order, retains complete worker assignment/cleanup\n"
    "traces, and resumes only after exact product dependency identity validation. The\n"
    "boundary remains private and uninstalled. Product commands and installed\n"
    "executable (#67/#93), artifact publication (#68), end-to-end proof (#69), and\n"
    "final stabilization (#56) remain later dependency-ordered work.\n",
    "README current status tail",
)

# Roadmap: mark the ninth private foundation and advance the dependency-ready edge.
replace_once(
    "docs/roadmap.md",
    "EVO 0.41.0 contains the completed deterministic C17 evolutionary-search core\n"
    "and the first eight private source-optimizer foundations:",
    "EVO 0.42.0 contains the completed deterministic C17 evolutionary-search core\n"
    "and the first nine private source-optimizer foundations:",
    "roadmap current version",
)
append_after_once(
    "docs/roadmap.md",
    "fixed-seed strict-improvement/exact-tie replay across multiple source files.\n",
    "The ninth adds bounded external-process orchestration over source evaluations:\n"
    "explicit resource/capability policy, deterministic logical worker/workspace\n"
    "assignment, asynchronous completion with stable candidate-order commit, complete\n"
    "cancel/join cleanup before failure publication, persistent ordered worker traces,\n"
    "and product checkpoints that bind baseline/analysis/catalogue/search/provider/\n"
    "toolchain/workload/artifact identities before deterministic resume.\n",
    "roadmap eighth foundation tail",
)
replace_once(
    "docs/roadmap.md",
    "Issue #65\nimplements deterministic structured recipe evolution and bounded search. Issue\n#66 is the next dependency-ready source-optimizer implementation work.\n",
    "Issue #65\nimplements deterministic structured recipe evolution and bounded search. Issue\n#66 implements bounded external-process orchestration, stable commit/cleanup\ntraces, and product checkpoint/resume authority. Issue #67 is the next\ndependency-ready source-optimizer implementation work.\n",
    "roadmap dependency-ready boundary",
)

# Algorithms: source orchestration is now implemented and #67 becomes next.
replace_once(
    "docs/algorithms.md",
    "algorithm required by the EVO 1.0 source optimizer. Version 0.41.0 implements\n"
    "the core plus project ingestion, immutable-baseline preparation, normalized\n"
    "Clang/LLVM analysis and hotspot ranking, canonical transformation recipes,\n"
    "three AST-aware source-transformation applications, deterministic isolated\n"
    "candidate materialization, exact candidate build/correctness assurance,\n"
    "reproducible baseline-versus-candidate performance measurement, and structured\n"
    "recipe evolution. The search algorithm initializes canonical recipes from live\n",
    "algorithm required by the EVO 1.0 source optimizer. Version 0.42.0 implements\n"
    "the core plus project ingestion, immutable-baseline preparation, normalized\n"
    "Clang/LLVM analysis and hotspot ranking, canonical transformation recipes,\n"
    "three AST-aware source-transformation applications, deterministic isolated\n"
    "candidate materialization, exact candidate build/correctness assurance,\n"
    "reproducible baseline-versus-candidate performance measurement, structured\n"
    "recipe evolution, and bounded external-process source orchestration. The search\n"
    "algorithm initializes canonical recipes from live\n",
    "algorithms current boundary",
)
replace_once(
    "docs/algorithms.md",
    "exact ties retain the earlier stable candidate. Bounded external-process\n"
    "orchestration remains the next source-optimizer algorithm boundary.\n",
    "exact ties retain the earlier stable candidate. External evaluation batches\n"
    "may finish out of order, but stable candidate-order commit, exact worker traces,\n"
    "fail-closed cleanup, and product checkpoint identity keep logical search and\n"
    "resume deterministic. Product command orchestration is the next boundary.\n",
    "algorithms future orchestration wording",
)

# Architecture: implementation and HRA boundary advance to 0.42.0.
append_after_once(
    "docs/architecture.md",
    "and complete ordered operator plus generation lineage evidence.\n",
    "Version 0.42.0 adds bounded external-process source orchestration distinct from\n"
    "the core's in-process evaluator workers. It assigns deterministic logical\n"
    "worker/workspace identities under explicit resource/capability policy, permits\n"
    "asynchronous completion while committing stable candidate order, joins/cancels\n"
    "all started work before failure authority, retains persistent exact batch/job\n"
    "traces, and wraps core checkpoint bytes with complete product dependency identity\n"
    "before resume.\n",
    "architecture 0.41 foundation tail",
)
replace_once(
    "docs/architecture.md",
    "Version 0.41.0 contains the evolutionary-search core, its bounded reference\n"
    "consumers, private project ingestion, a private normalized analysis and hotspot\n"
    "model, a private canonical transformation-recipe model, a private initial AST-\n"
    "aware C transformation catalogue, private isolated candidate materialization,\n"
    "private candidate assurance, private reproducible candidate measurement with\n"
    "finite fitness mapping, and private deterministic structured recipe search. Candidate process execution and target workload\n"
    "sampling remain owned by caller-supplied providers; EVO validates declared\n"
    "policy, condition identity, ordered outcomes, and exact evidence rather than\n"
    "pretending to provide a portable OS sandbox or universal timing environment.\n"
    "Bounded external-process orchestration, product commands, the installed\n"
    "standalone executable, and final optimized-patch artifacts remain planned by\n"
    "issues #66 through #69 and #93. Documentation of those planned boundaries is not an implementation claim.\n",
    "Version 0.42.0 contains the evolutionary-search core, its bounded reference\n"
    "consumers, private project ingestion, a private normalized analysis and hotspot\n"
    "model, a private canonical transformation-recipe model, a private initial AST-\n"
    "aware C transformation catalogue, private isolated candidate materialization,\n"
    "private candidate assurance, private reproducible candidate measurement with\n"
    "finite fitness mapping, private deterministic structured recipe search, and\n"
    "private bounded external-process source orchestration. Candidate process\n"
    "execution and target workload sampling remain owned by caller-supplied providers;\n"
    "EVO validates declared policy, condition identity, ordered outcomes, cleanup, and\n"
    "exact evidence rather than pretending to provide a portable OS sandbox or\n"
    "universal timing environment. Product commands, the installed standalone\n"
    "executable, and final optimized-patch artifacts remain planned by issues #67\n"
    "through #69 and #93. Documentation of those planned boundaries is not an\n"
    "implementation claim.\n",
    "architecture current conformance boundary",
)
replace_once(
    "docs/architecture.md",
    "termination. EVO-HRA-014 audits this 0.41.0 boundary. The current implementation\n"
    "therefore has no opaque accelerated authority requiring remediation. This audit does not pre-approve later\n"
    "variable pools, compressed checkpoints, persistent or distributed schedulers,\n"
    "transformation lookup indexes, candidate caches, orchestration, or artifact\n"
    "implementations.\n",
    "termination. EVO-HRA-014 audits this 0.41.0 boundary. Bounded source orchestration\n"
    "also uses complete bounded candidate-job arrays, complete committed lineage\n"
    "prefixes, explicit product identity records, and direct deterministic scans;\n"
    "runtime queues, process handles, completion timing, and OS scheduling are not\n"
    "authority. EVO-HRA-015 audits this 0.42.0 boundary. The current implementation\n"
    "therefore has no opaque accelerated authority requiring remediation. This audit\n"
    "does not pre-approve later variable pools, compressed checkpoints, distributed\n"
    "schedulers, transformation lookup indexes, candidate caches, or artifact\n"
    "implementations.\n",
    "architecture HRA-014 tail",
)

# Core contract: package version advances but installed core semantics remain unchanged.
replace_once(
    "docs/specs/EVO-001-library-contract.md",
    "Version: 0.41.0",
    "Version: 0.42.0",
    "EVO-001 version",
)
replace_once(
    "docs/specs/EVO-001-library-contract.md",
    "core packaged through version 0.41.0. Version 0.41.0 changes no installed\n"
    "core semantics; its new implementation is confined to the private source-\n"
    "optimizer structured recipe-search foundation.",
    "core packaged through version 0.42.0. Version 0.42.0 changes no installed\n"
    "core semantics; its new implementation is confined to the private source-\n"
    "optimizer bounded external-process orchestration foundation.",
    "EVO-001 scope version",
)

# Source-optimizer contract: orchestration is now an implemented private boundary.
replace_once(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "Status: Implemented through the 0.41.0 structured recipe-search boundary; draft 1.0 target\n"
    "Version: 0.41.0\n"
    "Owner: EVO\n"
    "Governing ADRs: ADR-0016, ADR-0026, ADR-0035, ADR-0036, ADR-0037, ADR-0038, ADR-0039, ADR-0040, ADR-0041, and ADR-0042\n",
    "Status: Implemented through the 0.42.0 bounded source-orchestration boundary; draft 1.0 target\n"
    "Version: 0.42.0\n"
    "Owner: EVO\n"
    "Governing ADRs: ADR-0016, ADR-0026, ADR-0035, ADR-0036, ADR-0037, ADR-0038, ADR-0039, ADR-0040, ADR-0041, ADR-0042, and ADR-0043\n",
    "EVO-002 heading",
)
replace_once(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "the complete source optimizer is implemented in version 0.41.0. This release\n"
    "implements strict project ingestion and immutable baselines, normalized\n"
    "analysis/hotspot evidence, canonical transformation recipes, the initial AST-\n"
    "aware C transformation catalogue, deterministic isolated candidate\n"
    "materialization, candidate build/correctness assurance, reproducible\n"
    "baseline-versus-candidate performance measurement with finite fitness mapping,\n"
    "and deterministic structured recipe evolution through the reusable EVO core.\n"
    "Assurance, measurement, and search evaluation consume exact results from\n"
    "caller-supplied providers; portable OS sandboxing and target workload execution\n"
    "remain provider responsibilities. Bounded external-process orchestration and\n"
    "every later roadmap boundary remain a 1.0 target until their issues land.\n",
    "the complete source optimizer is implemented in version 0.42.0. This release\n"
    "implements strict project ingestion and immutable baselines, normalized\n"
    "analysis/hotspot evidence, canonical transformation recipes, the initial AST-\n"
    "aware C transformation catalogue, deterministic isolated candidate\n"
    "materialization, candidate build/correctness assurance, reproducible\n"
    "baseline-versus-candidate performance measurement with finite fitness mapping,\n"
    "deterministic structured recipe evolution through the reusable EVO core, and\n"
    "bounded external-process source orchestration with stable result commit, cleanup\n"
    "evidence, persistent worker traces, and product checkpoint/resume authority.\n"
    "Assurance, measurement, and candidate execution consume exact results from\n"
    "caller-supplied providers; portable OS sandboxing and target workload execution\n"
    "remain provider responsibilities. Product commands, installed application\n"
    "delivery, artifact publication, and later roadmap boundaries remain 1.0 targets.\n",
    "EVO-002 purpose boundary",
)
append_after_once(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "fitness, and winner lineage. ADR-0042 and EVO-HRA-014 retain this issue-specific\n"
    "assessment.\n",
    "\nThe implemented 0.42.0 bounded source-orchestration boundary likewise\n"
    "introduces no accelerator. Complete bounded candidate-job arrays, committed\n"
    "lineage prefixes, exact provider outcomes, persistent batch/job traces, and\n"
    "explicit checkpoint identity records remain authority. Runtime queues, process\n"
    "handles, completion timing, and OS scheduling are diagnostic only; stable\n"
    "candidate order commits results and every started worker is joined or canceled\n"
    "before failure/checkpoint authority. ADR-0043 and EVO-HRA-015 retain this\n"
    "issue-specific assessment.\n",
    "EVO-002 structured-search HRA tail",
)
append_after_once(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "Any incompatible identity rejects resume before executing a candidate. Every\n"
    "worker is joined, terminated, and cleaned before failure or checkpoint state is\n"
    "published.\n",
    "\nThe implemented 0.42.0 private orchestration transaction applies that contract\n"
    "directly. Candidate requests receive deterministic generation/population,\n"
    "workspace, logical-worker, and dispatch-wave identities under caller-bounded CPU,\n"
    "address-space, process-count, storage, output, wall-time, filesystem, network,\n"
    "and descendant-cleanup policy. Provider completion may be asynchronous, but only\n"
    "stable candidate order becomes committed evaluation authority. The persistent\n"
    "trace retains all dispatched batch/job outcomes and cleanup state. Product\n"
    "checkpoint format 1 wraps exact Core checkpoint bytes with baseline, analysis,\n"
    "catalogue, recipe/search policy, provider, orchestration policy, toolchain,\n"
    "workload, artifact-schema, seed, generation, and lineage identities. Normative\n"
    "differential evidence proves supported serial/parallel logical equivalence and\n"
    "uninterrupted/resumed equivalence; stale identity rejects before external\n"
    "candidate execution.\n",
    "EVO-002 parallel checkpoint contract tail",
)
append_after_once(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "- `docs/adr/ADR-0038-ast-aware-c-transformation-catalogue.md`\n",
    "- `docs/adr/ADR-0039-isolated-candidate-materialization.md`\n"
    "- `docs/adr/ADR-0040-isolated-candidate-correctness-gates.md`\n"
    "- `docs/adr/ADR-0041-reproducible-candidate-performance-fitness.md`\n"
    "- `docs/adr/ADR-0042-structured-recipe-evolution.md`\n"
    "- `docs/adr/ADR-0043-bounded-parallel-source-orchestration.md`\n",
    "EVO-002 related ADR list",
)
append_after_once(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "- `docs/engineering/reports/EVO-HRA-010-c-transformation-catalogue-audit.md`\n",
    "- `docs/engineering/reports/EVO-HRA-011-candidate-materialization-audit.md`\n"
    "- `docs/engineering/reports/EVO-HRA-012-candidate-correctness-gates-audit.md`\n"
    "- `docs/engineering/reports/EVO-HRA-013-candidate-performance-fitness-audit.md`\n"
    "- `docs/engineering/reports/EVO-HRA-014-structured-recipe-evolution-audit.md`\n"
    "- `docs/engineering/reports/EVO-HRA-015-bounded-source-orchestration-audit.md`\n",
    "EVO-002 related HRA list",
)

# Repository metadata advances with the implemented private orchestration boundary.
replace_once(
    "repo.yaml",
    "status: implemented-0.41.0",
    "status: implemented-0.42.0",
    "core repo status",
)
replace_once(
    "repo.yaml",
    "status: structured-recipe-search-implemented-0.41.0",
    "status: bounded-source-orchestration-implemented-0.42.0",
    "source optimizer repo status",
)

# The issue-specific validator should require the release package version.
replace_once(
    "tests/validate_project_orchestration.py",
    'require(version_tuple >= (0, 41, 0), "package predates structured search dependency")',
    'require(version_tuple == (0, 42, 0), "package version is not EVO 0.42.0")',
    "orchestration validator package version",
)
