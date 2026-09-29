#!/usr/bin/env node
/*
 * Cross-runtime comparison smoke test (f2b task 2.1, ADR 0022): runs the
 * same portable script through the desktop player (embedded quickjs,
 * --script mode) and the web player (native bridge, host engine = Node,
 * resource-root mode), then compares exit code and stdout line-for-line.
 * Stderr is compared on the first line only (stack traces differ by
 * engine).
 *
 * Usage: node tools/run_web_compare.mjs
 * Env:   NATIVE_PLAYER    (default build/player)
 *        WEB_PLAYER_BUILD (default build-em)
 */
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import url from 'node:url';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const NATIVE = process.env.NATIVE_PLAYER ?? path.join(ROOT, 'build', 'player');
const WEB_BUILD = process.env.WEB_PLAYER_BUILD ?? path.join(ROOT, 'build-em');
const WEB = path.join(WEB_BUILD, 'player.js');

if (!fs.existsSync(NATIVE)) {
    console.error(`native player not found: ${NATIVE} (set NATIVE_PLAYER)`);
    process.exit(2);
}
if (!fs.existsSync(WEB)) {
    console.error(`web player not found: ${WEB} (set WEB_PLAYER_BUILD)`);
    process.exit(2);
}

const CASES = [
    { name: 'log', script: 'tests/scripts/s_log.js', args: [] },
    { name: 'quit3', script: 'tests/scripts/s_quit3.js', args: [] },
    { name: 'args', script: 'tests/scripts/s_args.js', args: ['one', 'two'] },
    { name: 'portable', script: 'tests/scripts/s_portable.js', args: [] },
    { name: 'throw', script: 'tests/scripts/s_throw.js', args: [], stderrFirstLine: true },
    { name: '2d_validation', script: 'tests/scripts/s_2d_validation.js', args: [] },
    { name: 'resource_lifecycle', script: 'tests/scripts/s_resource_lifecycle.js', args: [] },
    { name: '3d_validation', script: 'tests/scripts/s_3d_validation.js', args: [] },
    { name: '3d_math', script: 'tests/scripts/s_3d_math.js', args: [] },
    { name: '4a_validation', script: 'tests/scripts/s_4a_validation.js', args: [] },
    { name: '4b_validation', script: 'tests/scripts/s_4b_validation.js', args: [] },
    { name: '5a_validation', script: 'tests/scripts/s_5a_validation.js', args: [] },
    { name: '5b_validation', script: 'tests/scripts/s_5b_validation.js', args: [] },
    { name: '6a_resource', script: 'tests/scripts/s_6a_resource.js', args: [],
      assets: ['tests/scripts/resource_probe.txt', 'tests/scripts/resource_probe.png'] },
    { name: '6b_gltf', script: 'tests/scripts/s_6b_gltf.js', args: [],
      assets: ['tests/scripts/gltf_probe.glb', 'tests/scripts/gltf_corrupt.gltf'] },
    { name: '6c_skin', script: 'tests/scripts/s_6c_skin.js', args: [] },
    { name: '7_skin_pose', script: 'tests/scripts/s_7_skin_pose.js', args: [],
      assets: ['tests/scripts/skin.gltf', 'tests/scripts/skin.bin'] },
    { name: '9_input', script: 'tests/scripts/s_9_input.js', args: [] },
    { name: '8a_text', root: 'tests/fixtures/web/text_root' },
    { name: '10_modules', root: 'tests/fixtures/modules' },
    { name: '10_nohost', script: 'tests/scripts/s_10_nohost.js', args: [] },
    { name: '11_particles', script: 'tests/scripts/s_11_particles.js', args: [] },
    { name: '12_physics', script: 'tests/scripts/physics_smoke.js', args: [] },
    { name: '13_gamepad', script: 'tests/scripts/s_13_gamepad.js', args: [] },
];

function run(cmd, args) {
    let code = 0;
    let out = '';
    let err = '';
    try {
        out = execFileSync(cmd, args, { encoding: 'utf8', timeout: 60000 });
    } catch (e) {
        code = e.status ?? 1;
        out = e.stdout ?? '';
        err = e.stderr ?? '';
    }
    const lines = (s) => s.split('\n').map((l) => l.replace(/\r$/, '')).filter((l) => l.length > 0);
    return { code, out: lines(out), err: lines(err) };
}

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'efx-compare-'));
let failures = 0;

for (const c of CASES) {
    process.stdout.write(`compare/${c.name}: `);
    let native;
    let web;
    if (c.root) {
        /* resource-root mode on both runtimes (multi-module fixture graph) */
        const rootPath = path.join(ROOT, c.root);
        native = run(NATIVE, [rootPath]);
        web = run(process.execPath, [WEB, rootPath]);
    } else {
        const script = path.join(ROOT, c.script);
        native = run(NATIVE, ['--script', script, ...c.args]);
        const rootDir = path.join(tmp, c.name);
        fs.mkdirSync(rootDir, { recursive: true });
        fs.copyFileSync(script, path.join(rootDir, 'main.js'));
        for (const a of c.assets || []) {
            fs.copyFileSync(path.join(ROOT, a), path.join(rootDir, path.basename(a)));
        }
        web = run(process.execPath, [WEB, rootDir, ...c.args]);
    }

    const problems = [];
    if (native.code !== web.code) {
        problems.push(`exit ${native.code} vs ${web.code}`);
    }
    if (JSON.stringify(native.out) !== JSON.stringify(web.out)) {
        problems.push(`stdout ${JSON.stringify(native.out)} vs ${JSON.stringify(web.out)}`);
    }
    if (c.stderrFirstLine) {
        // A module error names the failing module path; desktop runs the entry
        // via --script (its file name) while web stages it as main.js, so strip
        // that path prefix and compare the underlying diagnostic.
        const norm = (l) => l.replace(/^uncaught exception: module '[^']*': /,
                                      'uncaught exception: ');
        const a = norm(native.err[0] ?? '');
        const b = norm(web.err[0] ?? '');
        if (!a.startsWith('uncaught exception:') || !b.startsWith('uncaught exception:') ||
            a.split(':')[1] !== b.split(':')[1]) {
            problems.push(`stderr-first ${JSON.stringify(a)} vs ${JSON.stringify(b)}`);
        }
    }
    if (problems.length > 0) {
        console.log('FAIL (' + problems.join('; ') + ')');
        failures++;
    } else {
        console.log(`PASS (exit ${native.code}, ${native.out.length} stdout line(s))`);
    }
}

fs.rmSync(tmp, { recursive: true, force: true });
console.log(failures === 0 ? 'all cross-runtime comparisons match' : `${failures} comparison failure(s)`);
process.exit(failures === 0 ? 0 : 1);
