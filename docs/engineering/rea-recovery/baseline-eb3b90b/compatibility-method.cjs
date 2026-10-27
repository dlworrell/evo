#!/usr/bin/env node
// Read metadata only. The caller supplies a trusted analysis copy of semver;
// no target modules, package scripts, native bindings, or providers are loaded.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { execFileSync } = require('node:child_process');
const [rootArg, outputArg, semverArg] = process.argv.slice(2);
if (!rootArg || !outputArg || !semverArg) throw Error('Usage: ROOT OUTPUT TRUSTED_SEMVER_MODULE');
const root = path.resolve(rootArg), output = path.resolve(outputArg);
if (output === root || output.startsWith(root + path.sep)) throw Error('Output must be outside target');
const semver = require(path.resolve(semverArg));
const git = (...args) => execFileSync('git', ['-C', root, ...args], { encoding: 'utf8' });
const OLD = '9e22da845bf8b447f81539e0ba57d5deb59e9103';
const NEW = 'eb3b90beb4ac9245bac415696aa0de5a1a66c6d2';
if (git('rev-parse', 'HEAD').trim() !== NEW || git('status', '--porcelain=v1')) throw Error('Wrong or dirty source');
const pkg = JSON.parse(git('show', NEW + ':package.json'));
const before = JSON.parse(git('show', OLD + ':package-lock.json')).packages;
const after = JSON.parse(git('show', NEW + ':package-lock.json')).packages;
const changed = new Set(Object.keys(after).filter(p => p && JSON.stringify(before[p]) !== JSON.stringify(after[p])));
function resolveDependency(from, name) {
  for (let prefix = from; ; ) {
    const candidate = (prefix ? prefix + '/' : '') + 'node_modules/' + name;
    if (Object.hasOwn(after, candidate)) return candidate;
    if (!prefix) return null;
    const index = prefix.lastIndexOf('/node_modules/');
    prefix = index >= 0 ? prefix.slice(0, index) : '';
  }
}
const edges = [];
for (const [from, entry] of Object.entries(after)) {
  for (const field of ['dependencies', 'optionalDependencies', 'peerDependencies']) {
    for (const [name, range] of Object.entries(entry[field] || {})) {
      const target = resolveDependency(from, name);
      if (!changed.has(from) && !changed.has(target)) continue;
      const optional = field === 'optionalDependencies' || (field === 'peerDependencies' && entry.peerDependenciesMeta?.[name]?.optional === true);
      const version = target ? after[target].version : null;
      const valid = semver.validRange(range);
      const status = !target ? (optional ? 'optional-not-present' : 'unresolved')
        : !valid ? 'non-semver-review' : semver.satisfies(version, range) ? 'satisfied' : 'range-mismatch';
      edges.push({ from, field, name, range, resolved_path: target, version, optional, status });
    }
  }
}
const pinnedNode = git('show', NEW + ':.nvmrc').trim();
const engines = ['', ...changed].flatMap(p => {
  const range = (p ? after[p] : pkg).engines?.node;
  return range ? [{ path: p, range, pinned_node: pinnedNode,
                   pinned_node_satisfies: semver.satisfies(pinnedNode, range),
                   analysis_node: process.version,
                   analysis_node_satisfies: semver.satisfies(process.version, range) }] : [];
});
const report = { subject_commit: NEW, original_commit: OLD, analysis_node: process.version,
  semver_module_sha256: crypto.createHash('sha256').update(fs.readFileSync(semverArg)).digest('hex'),
  changed_nonroot_entries: changed.size, edges, node_engine_checks: engines,
  manifest_root_lock_agreement: ['dependencies', 'devDependencies'].every(k => JSON.stringify(pkg[k]) === JSON.stringify(after[''][k]) ||
    Object.keys(pkg[k] || {}).length === Object.keys(after[''][k] || {}).length &&
    Object.entries(pkg[k] || {}).every(([n, v]) => after[''][k]?.[n] === v)),
  declared_package_manager: pkg.packageManager, audit_npm: '11.9.0',
  project_toolchain_match: 'not-established; analysis Node differs from .nvmrc and audit npm differs from declared npm',
  semantic_compatibility: 'not-run; range agreement is metadata evidence only',
  build_tests_and_providers: 'not-run', target_unchanged: git('status', '--porcelain=v1') === '' };
fs.writeFileSync(path.join(output, 'compatibility.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({ edge_counts: edges.reduce((a, e) => ({ ...a, [e.status]: (a[e.status] || 0) + 1 }), {}),
  engine_checks: engines.length, engine_mismatches: engines.filter(e => !e.pinned_node_satisfies || !e.analysis_node_satisfies).length,
  manifest_root_lock_agreement: report.manifest_root_lock_agreement }));
