#!/usr/bin/env node
/*
 * F6a web asset-root harness: serves a fixture zip over http, runs the Node
 * web player with `globalThis.__efx_assets` pointing at it, and asserts the
 * async boot fetched + mounted the archive and evaluated the entry script
 * from inside it (which loads a resource and quits 0).
 *
 * Usage: node tools/test_web_assets.mjs <path-to-player.js>
 */
import http from 'node:http';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const player = process.argv[2];
if (!player) {
    console.error('usage: node tools/test_web_assets.mjs <player.js>');
    process.exit(2);
}

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const ZIP = path.join(ROOT, 'tests', 'fixtures', 'root_zip.zip');
const PRELOAD = path.join(ROOT, 'tools', 'web-assets-preload.cjs');

const server = http.createServer((req, res) => {
    if (req.url.split('?')[0] !== '/root_zip.zip') {
        res.statusCode = 404;
        res.end('nope');
        return;
    }
    res.setHeader('Content-Type', 'application/zip');
    res.end(fs.readFileSync(ZIP));
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
