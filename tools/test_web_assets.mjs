#!/usr/bin/env node
/*
 * F6a web asset-root harness: serves a fixture zip over http, runs the Node
 * web player with `globalThis.__efx_assets` pointing at it, and asserts the
 * async boot fetched + mounted the archive and evaluated the entry script
 * from inside it (which loads a resource and quits 0).
 *
 * Usage: node tools/test_web_assets.mjs <path-to-player.js>
 */
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { serveStatic } from './lib/web-host.mjs';

const player = process.argv[2];
if (!player) {
    console.error('usage: node tools/test_web_assets.mjs <player.js>');
    process.exit(2);
}

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const ZIP = path.join(ROOT, 'tests', 'fixtures', 'root_zip.zip');
const PRELOAD = path.join(ROOT, 'tools', 'web-assets-preload.cjs');

const server = serveStatic(ROOT, {
    '/root_zip.zip': { contentType: 'application/zip', body: fs.readFileSync(ZIP) },
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const port = server.address().port;

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'efx-assets-'));
const child = spawn(process.execPath, ['--require', PRELOAD, path.resolve(player)], {
    cwd: tmp,
    env: { ...process.env, EFX_ASSETS_URL: `http://127.0.0.1:${port}/root_zip.zip` },
});
let out = '';
let err = '';
child.stdout.on('data', (d) => { out += d; });
child.stderr.on('data', (d) => { err += d; });
const status = await new Promise((r) => child.on('exit', r));
server.close();
fs.rmSync(tmp, { recursive: true, force: true });

if (out) process.stdout.write(out);
if (err) process.stderr.write(err);

const ok = status === 0 && out.includes('zip-entry-ok');
if (!ok) {
    console.error(`web assets test FAILED (exit ${status})`);
    process.exit(1);
}
console.log('web assets test PASSED: fetched zip mounted and entry script ran');

// Failure case: an unreachable asset URL must surface a diagnostic and a
// non-zero exit code before the entry script runs.
const tmp2 = fs.mkdtempSync(path.join(os.tmpdir(), 'efx-assets-bad-'));
const bad = spawn(process.execPath, ['--require', PRELOAD, path.resolve(player)], {
    cwd: tmp2,
    env: { ...process.env, EFX_ASSETS_URL: 'http://127.0.0.1:1/nope.zip' },
});
let badErr = '';
bad.stderr.on('data', (d) => { badErr += d; });
const badStatus = await new Promise((r) => bad.on('exit', r));
fs.rmSync(tmp2, { recursive: true, force: true });

if (badStatus === 0 || !badErr.includes('asset root fetch failed')) {
    console.error(`web assets failure-case FAILED (exit ${badStatus})`);
    process.exit(1);
}
console.log('web assets failure case PASSED: fetch failure exits non-zero');

