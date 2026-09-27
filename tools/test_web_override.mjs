#!/usr/bin/env node
/*
 * Verifies the host entry-source hook (ADR 0030): with
 * `globalThis.__efx_main_js` set before the Node web player is loaded, that
 * source runs instead of the resource-root `main.js`; the channel is consumed
 * (deleted) before evaluation.
 *
 * The Node web player boots asynchronously, so the assertion runs from a
 * preload's process-exit handler (tools/web-override-preload.cjs).
 *
 * Usage: node tools/test_web_override.mjs <path-to-player.js>
 */
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const player = process.argv[2];
if (!player) {
    console.error('usage: node tools/test_web_override.mjs <player.js>');
    process.exit(2);
}

const preload = path.join(
    path.dirname(fileURLToPath(import.meta.url)),
    'web-override-preload.cjs'
);

const result = spawnSync(process.execPath, ['--require', preload, path.resolve(player)], {
    encoding: 'utf8',
});
if (result.stdout) process.stdout.write(result.stdout);
if (result.stderr) process.stderr.write(result.stderr);

if (result.status !== 0) {
    console.error(`web override test FAILED (exit ${result.status})`);
    process.exit(1);
}
console.log('web override test PASSED: host entry source ran and channel was consumed');
