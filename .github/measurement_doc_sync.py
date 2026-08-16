from pathlib import Path


def replace(path: str, old: str, new: str, count: int = 1) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing documentation anchor in {path}: {old[:120]!r}")
    p.write_text(text.replace(old, new, count))


replace(
    "README.md",
    "- `docs/adr/ADR-0040-isolated-candidate-correctness-gates.md`\n",
    "- `docs/adr/ADR-0040-isolated-candidate-correctness-gates.md`\n"
    "- `docs/adr/ADR-0041-reproducible-candidate-performance-fitness.md`\n",
)
replace(
    "README.md",
    "complete ordered gate trace. EVO-HRA-012 retains this assurance-specific\n"
    "audit.\n",
    "complete ordered gate trace. EVO-HRA-012 retains this assurance-specific\n"
    "audit. ADR-0041 assesses 0.40.0 candidate performance measurement: exact\n"
    "ordered warmup/recorded samples and declared measurement policy remain\n"
    "authority, condition identity binds hardware/toolchain/environment/dataset\n"
    "and binary provenance, incomplete or unstable evidence yields no ranking\n"
    "fitness, and JSON/Markdown preserve every sample, exclusion, aggregate, and\n"
    "fitness derivation. EVO-HRA-013 retains this measurement-specific audit.\n",
)
replace(
    "README.md",
    "- Reproducible baseline-versus-candidate performance measurement\n",
    "- Implemented reproducible baseline-versus-candidate performance measurement\n"
    "  and finite fitness mapping\n",
)
replace(
    "README.md",
    "candidate assurance with exact ordered fast/finalist gate authority through\n"
    "a caller-supplied isolated execution provider. Performance measurement,\n"
    "whole-run orchestration, and the installed executable remain dependency-\n"
    "ordered roadmap work; their absence is an explicit boundary, not an implicit\n"
    "feature claim.\n",
    "candidate assurance with exact ordered fast/finalist gate authority through\n"
    "a caller-supplied isolated execution provider, and 0.40.0 reproducible\n"
    "baseline-versus-candidate measurement with ordered raw samples, explicit\n"
    "condition identity, deterministic aggregation, stability/tolerance policy,\n"
    "and finite EVO fitness only for complete stable evidence. Whole-run\n"
    "orchestration and the installed executable remain dependency-ordered roadmap\n"
    "work; their absence is an explicit boundary, not an implicit feature claim.\n",
)
replace(
    "README.md",
    "installed-core sources, 21 private source-foundation sources, and 38\n"
    "normative tests.",
    "installed-core sources, 24 private source-foundation sources, and 39\n"
    "normative tests.",
)
p = Path("README.md")
text = p.read_text()
start = "**Current implementation boundary:** EVO 0.39.0 packages the deterministic\n"
end = "final stabilization (#56) remain later dependency-ordered work.\n"
si = text.find(start)
ei = text.find(end, si)
if si < 0 or ei < 0:
    raise SystemExit("README current-boundary paragraph anchor missing")
ei += len(end)
current = (
    "**Current implementation boundary:** EVO 0.40.0 packages the deterministic\n"
    "evolutionary-search core plus the private source-optimizer foundations for\n"
    "strict C-project ingestion, immutable baselines, normalized Clang/LLVM\n"
    "analysis, canonical transformation recipes, three AST-aware C\n"
    "transformations, isolated candidate materialization, candidate assurance,\n"
    "and reproducible candidate performance measurement. Measurement consumes only\n"
    "performance-eligible assurance evidence, executes deterministic paired\n"
    "baseline/candidate warmup and recorded requests through a caller-supplied\n"
    "provider, binds every sample to one exact condition identity, retains explicit\n"
    "exclusions and raw evidence, rejects incomplete or unstable measurements from\n"
    "ranking, and derives finite EVO fitness from recorded component values and\n"
    "caller-declared weights without changing correctness authority. The boundary\n"
    "remains private and uninstalled. Recipe evolution/orchestration (#65-#66),\n"
    "product commands and installed executable (#67/#93), artifact publication\n"
    "(#68), end-to-end proof (#69), and final stabilization (#56) remain later\n"
    "dependency-ordered work.\n"
)
p.write_text(text[:si] + current + text[ei:])

replace("docs/roadmap.md", "EVO 0.39.0 contains", "EVO 0.40.0 contains")
replace(
    "docs/roadmap.md",
    "the first six private source-optimizer foundations:",
    "the first seven private source-optimizer foundations:",
)
replace(
    "docs/roadmap.md",
    "both declared build profiles control champion admission.\n\nCore issues",
    "both declared build profiles control champion admission. The seventh adds\n"
    "reproducible baseline-versus-candidate performance measurement: deterministic\n"
    "paired sample order, explicit condition identity, bounded warmup/repetition\n"
    "policy, visible outlier handling, stability and tolerance checks, and finite\n"
    "EVO fitness only when complete stable evidence is available; correctness\n"
    "authority remains unchanged from candidate assurance.\n\nCore issues",
)
replace(
    "docs/roadmap.md",
    "implements candidate build/correctness assurance. Issue #64 is the next\n"
    "dependency-ready source-optimizer implementation work.",
    "implements candidate build/correctness assurance. Issue #64 implements\n"
    "reproducible candidate performance measurement and fitness. Issue #65 is the\n"
    "next dependency-ready source-optimizer implementation work.",
)

replace(
    "docs/architecture.md",
    "filesystem, network, resource, cleanup, and immutable-input obligations.\n\n"
    "Source genomes",
    "filesystem, network, resource, cleanup, and immutable-input obligations.\n"
    "Version 0.40.0 adds a private measurement transaction over performance-eligible\n"
    "assurance results. It records deterministic paired baseline/candidate samples,\n"
    "exact platform/toolchain/environment/dataset/binary condition identity,\n"
    "declared outlier/stability/tolerance policy, deterministic aggregates, and\n"
    "finite EVO fitness only for complete stable evidence.\n\nSource genomes",
)
replace(
    "docs/architecture.md",
    "Correctness and admissibility are hard gates. Performance evidence cannot make\n"
    "an invalid candidate valid, and a candidate cannot become the published\n"
    "champion until it passes every configured finalist gate.\n",
    "Correctness and admissibility are hard gates. Performance evidence cannot make\n"
    "an invalid candidate valid, and a candidate cannot become the published\n"
    "champion until it passes every configured finalist gate. Performance\n"
    "measurement consumes only a performance-eligible assurance result. Raw\n"
    "ordered samples remain authority; warmups are retained but excluded from\n"
    "aggregation, declared exclusions remain visible, and incomplete or unstable\n"
    "measurement produces no ranking fitness. Runtime, memory, binary size,\n"
    "reliability, and maintainability remain separately reconstructable before\n"
    "caller-declared weights derive the scalar EVO fitness.\n",
)
replace(
    "docs/architecture.md",
    "Version 0.39.0 contains the evolutionary-search core, its bounded reference",
    "Version 0.40.0 contains the evolutionary-search core, its bounded reference",
)
replace(
    "docs/architecture.md",
    "and private candidate assurance. Candidate process execution remains owned by\n"
    "the caller-supplied execution provider; EVO validates the declared policy and\n"
    "commits its exact attested outcome rather than pretending to provide a portable\n"
    "OS sandbox itself. Target-code measurement, product commands, the installed\n"
    "standalone executable, and final optimized-patch artifacts remain planned by\n"
    "issues #64 through #69 and #93.",
    "private candidate assurance, and private reproducible candidate measurement\n"
    "with finite fitness mapping. Candidate process execution and target workload\n"
    "sampling remain owned by caller-supplied providers; EVO validates declared\n"
    "policy, condition identity, ordered outcomes, and exact evidence rather than\n"
    "pretending to provide a portable OS sandbox or universal timing environment.\n"
    "Whole-run orchestration, product commands, the installed standalone executable,\n"
    "and final optimized-patch artifacts remain planned by issues #65 through #69\n"
    "and #93.",
)
replace(
    "docs/architecture.md",
    "authority. EVO-HRA-012 audits this 0.39.0 boundary. The current implementation\n"
    "therefore has no opaque accelerated authority requiring remediation.",
    "authority. EVO-HRA-012 audits this 0.39.0 boundary. Candidate measurement\n"
    "likewise retains direct bounded workload/sample arrays and exact provider\n"
    "outcomes; deterministic medians, exclusions, stability, comparisons, and\n"
    "fitness are derived from that complete record, with no cache, sketch, or\n"
    "probabilistic ranking authority. EVO-HRA-013 audits this 0.40.0 boundary. The\n"
    "current implementation therefore has no opaque accelerated authority requiring\n"
    "remediation.",
)

p = Path("docs/algorithms.md")
text = p.read_text()
start = "This document distinguishes algorithms implemented by the reusable\n"
end = "next source-optimizer algorithm boundary.\n"
si = text.find(start)
ei = text.find(end, si)
if si < 0 or ei < 0:
    raise SystemExit("algorithms introduction anchor missing")
ei += len(end)
intro = (
    "This document distinguishes algorithms implemented by the reusable\n"
    "`catalyst_evo` core from the structured program transformations and evaluation\n"
    "algorithm required by the EVO 1.0 source optimizer. Version 0.40.0 implements\n"
    "the core plus project ingestion, immutable-baseline preparation, normalized\n"
    "Clang/LLVM analysis and hotspot ranking, canonical transformation recipes,\n"
    "three AST-aware source-transformation applications, deterministic isolated\n"
    "candidate materialization, exact candidate build/correctness assurance, and\n"
    "reproducible baseline-versus-candidate performance measurement. The\n"
    "measurement algorithm consumes only performance-eligible assurance evidence,\n"
    "requests deterministic alternating baseline/candidate warmup and recorded\n"
    "samples, binds every provider result to one exact condition fingerprint,\n"
    "retains raw samples and declared exclusions, aggregates included samples by\n"
    "deterministic median, classifies unstable/incomplete evidence without fitness,\n"
    "and maps complete stable runtime/memory/binary/reliability/maintainability\n"
    "evidence into finite EVO fitness through explicit caller weights. Recipe\n"
    "evolution and whole-run orchestration remain the next source-optimizer\n"
    "algorithm boundary.\n"
)
p.write_text(text[:si] + intro + text[ei:])

replace("repo.yaml", "status: implemented-0.39.0", "status: implemented-0.40.0")
replace(
    "repo.yaml",
    "status: candidate-assurance-implemented-0.39.0",
    "status: performance-measurement-implemented-0.40.0",
)

replace("docs/specs/EVO-001-library-contract.md", "Version: 0.39.0", "Version: 0.40.0")
replace(
    "docs/specs/EVO-001-library-contract.md",
    "core packaged through version 0.39.0. Version 0.39.0 changes no installed\n"
    "core semantics; its new implementation is confined to the private source-\n"
    "optimizer candidate-assurance foundation.",
    "core packaged through version 0.40.0. Version 0.40.0 changes no installed\n"
    "core semantics; its new implementation is confined to the private source-\n"
    "optimizer candidate-measurement and fitness foundation.",
)

replace(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "Status: Implemented through the 0.39.0 candidate-assurance boundary; draft 1.0 target",
    "Status: Implemented through the 0.40.0 candidate-measurement boundary; draft 1.0 target",
)
replace("docs/specs/EVO-002-source-optimizer-contract.md", "Version: 0.39.0", "Version: 0.40.0")
replace(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "ADR-0039, and ADR-0040",
    "ADR-0039, ADR-0040, and ADR-0041",
)
replace(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "the complete source optimizer is implemented in version 0.39.0. This release\n"
    "implements strict project ingestion and immutable baselines, normalized\n"
    "analysis/hotspot evidence, canonical transformation recipes, the initial AST-\n"
    "aware C transformation catalogue, deterministic isolated candidate\n"
    "materialization, and candidate build/correctness assurance. Assurance consumes\n"
    "exact results from a caller-supplied isolated execution provider; portable OS\n"
    "sandbox implementation remains provider responsibility. Performance fitness\n"
    "and every later roadmap boundary remain a 1.0 target until their issues land.",
    "the complete source optimizer is implemented in version 0.40.0. This release\n"
    "implements strict project ingestion and immutable baselines, normalized\n"
    "analysis/hotspot evidence, canonical transformation recipes, the initial AST-\n"
    "aware C transformation catalogue, deterministic isolated candidate\n"
    "materialization, candidate build/correctness assurance, and reproducible\n"
    "baseline-versus-candidate performance measurement with finite fitness mapping.\n"
    "Assurance and measurement consume exact results from caller-supplied providers;\n"
    "portable OS sandboxing and target workload execution remain provider\n"
    "responsibilities. Whole-run recipe evolution/orchestration and every later\n"
    "roadmap boundary remain a 1.0 target until their issues land.",
)
replace(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "ADR-0040 and EVO-HRA-012 retain this issue-specific assessment.\n\n"
    "## Optimization Manifest",
    "ADR-0040 and EVO-HRA-012 retain this issue-specific assessment.\n\n"
    "The implemented 0.40.0 candidate measurement boundary also introduces no\n"
    "accelerator. Direct bounded workload/sample arrays and exact provider outcomes\n"
    "remain authority. Warmups and excluded or incomplete samples remain present in\n"
    "canonical order; deterministic aggregates, stability classification,\n"
    "comparison, and finite fitness derive from that record only. Correctness\n"
    "authority remains the candidate-assurance result, and no timing outcome can\n"
    "rewrite it. ADR-0041 and EVO-HRA-013 retain this issue-specific assessment.\n\n"
    "## Optimization Manifest",
)

replace(
    "docs/adr/ADR-0041-reproducible-candidate-performance-fitness.md",
    "Status: Proposed",
    "Status: Accepted",
)
replace(
    "docs/engineering/reports/EVO-HRA-013-candidate-performance-fitness-audit.md",
    "Audited design: EVO 0.40.0 candidate measurement and fitness boundary",
    "Audited implementation: EVO 0.40.0 candidate measurement and fitness boundary",
)
