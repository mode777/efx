#!/usr/bin/env node
/*
 * Emscripten golden-image driver (ADR 0020 / design D9):
 *  - serves the capture build over http,
 *  - loads player_web_golden.js in pinned headless Chrome (SwiftShader WebGL),
 *  - for each golden scene: callMain(['--capture-frame','2',...]), waits for
 *    the MEMFS capture, pulls it out as base64,
 *  - compares with the native efx_imgdiff binary against the committed golden.
 *
 * Usage: node tools/run_web_goldens.mjs
 * Env:   CHROME_PATH (optional path to a chrome binary)
 */
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';
import { loadPuppeteer, serveStatic, launchBrowser, hostPage } from './lib/web-host.mjs';

const puppeteer = await loadPuppeteer();

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const BUILD = process.env.WEB_GOLDEN_BUILD ?? path.join(ROOT, 'build-web-golden');
const NATIVE_BUILD = process.env.NATIVE_BUILD ?? path.join(ROOT, 'build-golden-tools');
const SCENES_DIR = path.join(ROOT, 'tests', 'goldens');
const OUT_DIR = path.join(BUILD, 'goldens');
const PORT = 18123;

const PAGE_HTML = hostPage({
    title: 'efx golden capture',
    script: '/player_web_golden.js',
    verbose: true,
});

const scenes = fs
    .readdirSync(SCENES_DIR)
    .filter((s) => fs.existsSync(path.join(SCENES_DIR, s, 'main.js')));
if (scenes.length === 0) {
    console.error('no golden scenes found');
    process.exit(2);
}
fs.mkdirSync(OUT_DIR, { recursive: true });

const server = serveStatic(BUILD, { '/': PAGE_HTML }, { log: true });
await new Promise((r) => server.listen(PORT, r));

const browser = await launchBrowser({
    // chrome-headless-shell (old headless): supports
    // HeadlessExperimental.beginFrame and continuous rAF (ADR 0020)
    puppeteer,
    executablePath: process.env.CHROME_SHELL_PATH || process.env.CHROME_PATH || undefined,
});
const page = await browser.newPage();
page.on('console', (m) => console.log('[chrome]', m.text()));
page.on('pageerror', (e) => console.error('[pageerror]', e.message));
const cdp = await page.createCDPSession();

// one deterministic BeginFrame; rAF callbacks run inside it
async function beginFrame() {
    try {
        await cdp.send('HeadlessExperimental.beginFrame', {});
    } catch (e) {
        console.log('[beginFrame-error]', e.message);
    }
}

let failures = 0;
for (const scene of scenes) {
    process.stdout.write(`golden_web/${scene}: `);
    await page.goto(`http://localhost:${PORT}/?scene=${scene}`, { waitUntil: 'load' });
    // headless shell fires rAF only inside explicit BeginFrames, so the
    // driver drives the frame loop until the capture appears (frame 2)
    let b64 = null;
    for (let i = 0; i < 60 && !b64; i++) {
        b64 = await page.evaluate(() => window.Module?.['webGoldenCapture'] ?? null);
        if (!b64) {
            await beginFrame();
        }
    }
    if (!b64) {
        console.log('FAIL (capture timed out)');
        failures++;
        continue;
    }
    const actual = path.join(OUT_DIR, `${scene}-actual.png`);
    const diff = path.join(OUT_DIR, `${scene}-diff.png`);
    fs.writeFileSync(actual, Buffer.from(b64, 'base64'));
    const imgdiff = path.join(NATIVE_BUILD, 'tests', 'efx_imgdiff');
    try {
        execFileSync(imgdiff, [actual, path.join(SCENES_DIR, scene, 'golden.png'), diff], { stdio: 'inherit' });
        console.log('PASS');
    } catch {
        console.log('FAIL (mismatch; diff: ' + diff + ')');
        failures++;
    }
}

await browser.close();
server.close();
console.log(failures === 0 ? 'all web goldens pass' : `${failures} web golden failure(s)`);
process.exit(failures === 0 ? 0 : 1);
