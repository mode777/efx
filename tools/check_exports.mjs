#!/usr/bin/env node
/*
 * Advisory dead-export guard (docs/refactoring.md P0b).
 *
 * Reports:
 *   1. web exports never referenced (as `_name` or `name`) from src/web/**
 *      or tools/**: EMSCRIPTEN_KEEPALIVE functions in src/web/bridge_*.c
 *      and the core functions in CMakeLists.txt's EFX_WEB_CORE_EXPORTS.
 *   2. external `efx_*` functions declared in src/**<slash>.h whose only
 *      occurrences across src/** and tests/** are the declaration and (if
 *      any) the definition.
 *
 * This is a maintenance aid, not a gate: it is read-only, prints candidates,
 * and always exits 0. Some symbols are intentional test seams or are reached
 * through a macro/function pointer, so confirm each hit by hand before
 * deleting it.
 *
 * Usage: node tools/check_exports.mjs
 */
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');

function walk(dir, exts, out = []) {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
        const p = path.join(dir, entry.name);
        if (entry.isDirectory()) {
            if (entry.name === 'node_modules' || entry.name === '.git') continue;
            walk(p, exts, out);
        } else if (exts.some((e) => entry.name.endsWith(e))) {
            out.push(p);
        }
    }
    return out;
}

const read = (p) => fs.readFileSync(p, 'utf8');

const stripComments = (s) =>
    s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');

/* ---- 1. unused EMSCRIPTEN_KEEPALIVE exports ---------------------------- */

const bridgePaths = walk(path.join(ROOT, 'src/web'), ['.c'])
    .filter((p) => /bridge_.*\.c$/.test(p));
const bridge = bridgePaths.map(read).join('\n');

const keepalive = [];
for (const line of bridge.split('\n')) {
    if (!line.includes('EMSCRIPTEN_KEEPALIVE')) continue;
    const m = line.match(/\b(efx_[A-Za-z0-9_]+)\s*\(/);
    if (m) keepalive.push(m[1]);
}
const coreExports = read(path.join(ROOT, 'CMakeLists.txt'))
    .match(/set\(EFX_WEB_CORE_EXPORTS([^)]*)\)/);
if (coreExports) {
    for (const t of coreExports[1].split(/\s+/)) {
        if (t.startsWith('_efx_')) keepalive.push(t.slice(1));
    }
}

const webRefFiles = walk(path.join(ROOT, 'src/web'), ['.js', '.c', '.h'])
    .filter((p) => !bridgePaths.includes(p))
    .concat(walk(path.join(ROOT, 'tools'), ['.mjs', '.js', '.cjs']));
const webRefText = webRefFiles.map(read).join('\n');

const unusedKeepalive = keepalive.filter((name) => {
    const re = new RegExp('\\b_?' + name + '\\b');
    return !re.test(webRefText);
});

/* ---- 2. unused external efx_* C declarations -------------------------- */

const headerFiles = walk(path.join(ROOT, 'src'), ['.h']);
const allText = walk(path.join(ROOT, 'src'), ['.c', '.h', '.cpp', '.js'])
    .concat(walk(path.join(ROOT, 'tests'), ['.c', '.h', '.js', '.mjs']))
    .map(read)
    .join('\n');

const declared = new Set();
for (const h of headerFiles) {
    const lines = stripComments(read(h)).split('\n');
    let buf = '';
    for (const raw of lines) {
        const t = raw.trim();
        if (!t || t.startsWith('#')) {
            buf = '';
            continue;
        }
        buf = buf ? buf + ' ' + t : t;
        if (!/[;{}]/.test(t)) continue;
        const m = buf.match(/^(?!static\b|typedef\b)[A-Za-z_][\w \t*]*?\b(efx_[a-z0-9_]+)\s*\([^;{]*\)\s*;/);
        if (m) declared.add(m[1]);
        buf = '';
    }
}

const unusedC = [];
for (const name of declared) {
    if (name.startsWith('efx_js_')) continue; // quickjs binding entry points
    const count = (allText.match(new RegExp('\\b_?' + name + '\\b', 'g')) || []).length;
    // one occurrence = declaration only; two = declaration + definition
    if (count <= 2) unusedC.push(name);
}

/* ---- report ----------------------------------------------------------- */

console.log('check_exports: advisory dead-export scan\n');
console.log(`unused web exports (${unusedKeepalive.length}):`);
for (const n of unusedKeepalive.sort()) console.log('  ' + n);
console.log(`\nunused external efx_* C declarations (${unusedC.length}):`);
for (const n of unusedC.sort()) console.log('  ' + n);
console.log('\n(advisory only; confirm test seams / macro-reached symbols by hand)');

process.exit(0);
