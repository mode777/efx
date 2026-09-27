#!/usr/bin/env node
/*
 * Gallery smoke test (design D11): serves the built gallery bundle and drives
 * it in pinned headless Chrome. Asserts that the shell boots, a sample runs in
 * the isolated runner iframe (a sized canvas, no console errors), and several
 * samples run in sequence (no WebGL-context exhaustion).
 *
 * Usage: node tools/run_gallery_smoke.mjs
 * Env:   CHROME_SHELL_PATH  path to chrome-headless-shell (required)
 *        GALLERY_DIST       built site dir (default: gallery/dist)
 *        GALLERY_PORT       http port (default: 18124)
 */
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

let puppeteer;
try {
    puppeteer = (await import('puppeteer-core')).default;
} catch {
    console.error('puppeteer-core not installed: npm install --no-save puppeteer-core');
    process.exit(2);
}

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const DIST = path.resolve(process.env.GALLERY_DIST ?? path.join(ROOT, 'gallery', 'dist'));
const PORT = Number(process.env.GALLERY_PORT ?? 18124);
const CHROME = process.env.CHROME_SHELL_PATH;
const RUNS = Number(process.env.GALLERY_RUNS ?? 8);

if (!CHROME || !fs.existsSync(CHROME)) {
    console.error(`CHROME_SHELL_PATH not set or missing: ${CHROME ?? '<unset>'}`);
    process.exit(2);
}
if (!fs.existsSync(path.join(DIST, 'index.html'))) {
    console.error(`gallery not built at ${DIST} (run npm --prefix gallery run build)`);
    process.exit(2);
}

const MIME = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.mjs': 'text/javascript',
    '.css': 'text/css',
    '.json': 'application/json',
    '.wasm': 'application/wasm',
    '.data': 'application/octet-stream',
    '.png': 'image/png',
    '.svg': 'image/svg+xml',
};

const server = http.createServer((req, res) => {
    let rel = decodeURIComponent((req.url ?? '/').split('?')[0]);
    if (rel === '/' || rel === '') rel = '/index.html';
    const file = path.join(DIST, rel);
    if (!file.startsWith(DIST) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
        res.statusCode = 404;
        res.end('not found');
        return;
    }
    res.setHeader('Content-Type', MIME[path.extname(file)] ?? 'application/octet-stream');
    fs.createReadStream(file).pipe(res);
});

const fails = [];
function check(ok, label) {
    console.log(`${ok ? 'ok  ' : 'FAIL'}  ${label}`);
    if (!ok) fails.push(label);
}

await new Promise((r) => server.listen(PORT, r));
console.log(`serving ${DIST} on :${PORT}`);

const browser = await puppeteer.launch({
    executablePath: CHROME,
    headless: 'shell',
    args: [
        '--no-sandbox',
        '--disable-dev-shm-usage',
        '--use-gl=swiftshader',
        '--enable-unsafe-swiftshader',
        '--enable-webgl',
    ],
});

const consoleErrors = [];
const pageErrors = [];
try {
    const page = await browser.newPage();
    page.on('console', (m) => {
        if (m.type() === 'error') consoleErrors.push(m.text());
    });
    page.on('pageerror', (e) => pageErrors.push(e.message));

    await page.goto(`http://localhost:${PORT}/index.html`, { waitUntil: 'load', timeout: 30000 });
    await page.waitForSelector('iframe.frame', { timeout: 20000 });
    check(true, 'shell loads and creates the runner iframe');

    const runner = () => page.frames().find((f) => f.url().includes('runner.html'));

    async function waitBoot(timeout = 20000) {
        const start = Date.now();
        while (Date.now() - start < timeout) {
            const frame = runner();
            if (frame) {
                const ready = await frame
                    .evaluate(() => typeof globalThis.efx !== 'undefined')
                    .catch(() => false);
                if (ready) return frame;
            }
            await new Promise((r) => setTimeout(r, 150));
        }
        return null;
    }

    // First sample (auto-selected at load).
    let frame = await waitBoot();
    check(!!frame, 'first sample boots the engine (global efx present)');

    if (frame) {
        const dims = await frame.evaluate(() => {
            const c = document.getElementById('canvas');
            return c ? { w: c.width, h: c.height } : null;
        });
        check(!!dims && dims.w > 0 && dims.h > 0, `canvas has a surface (${JSON.stringify(dims)})`);
    }

    // Run several samples in sequence by clicking catalog entries.
    const total = await page.evaluate(() => document.querySelectorAll('.item').length);
    check(total > 1, `catalog has multiple samples (${total})`);
    const n = Math.min(total, RUNS);
    for (let i = 1; i < n; i++) {
        await page.evaluate((idx) => {
            const items = document.querySelectorAll('.item');
            if (items[idx]) items[idx].click();
        }, i);
        const ok = !!(await waitBoot());
        check(ok, `sample ${i + 1}/${n} runs (no context exhaustion)`);
    }

    // Editor surface + edit -> Run + inline error surfacing.
    async function setEditor(src) {
        return page.evaluate((code) => {
            const w = window;
            if (w.monaco && w.monaco.editor && w.monaco.editor.getModels().length) {
                w.monaco.editor.getModels()[0].setValue(code);
                return 'monaco';
            }
            const t = document.querySelector('textarea.fallback');
            if (t) {
                const setter = Object.getOwnPropertyDescriptor(
                    HTMLTextAreaElement.prototype,
                    'value'
                ).set;
                setter.call(t, code);
                t.dispatchEvent(new Event('input', { bubbles: true }));
                return 'fallback';
            }
            return null;
        }, src);
    }
    async function clickRun() {
        await page.evaluate(() => {
            const run = [...document.querySelectorAll('.code .btn')].find((b) =>
                /run/i.test(b.textContent || '')
            );
            if (run) run.click();
        });
    }

    const editorKind = await setEditor(
        "globalThis.__edited_marker = 7;\nfunction update() {}\nfunction render() {}\n"
    );
    check(editorKind !== null, `editor surface present (${editorKind ?? 'none'})`);
    await clickRun();
    const editedFrame = await waitBoot();
    const editApplied = editedFrame
        ? await editedFrame.evaluate(() => globalThis.__edited_marker === 7).catch(() => false)
        : false;
    check(editApplied, 'edited source runs after Run');

    // Compositor screenshot of the runner box should not be a single flat fill.
    try {
        const el = await page.$('iframe.frame');
        const shot = el ? await el.screenshot() : Buffer.alloc(0);
        check(shot.length > 300, `runner renders content (screenshot ${shot.length} bytes)`);
    } catch (e) {
        check(false, `runner screenshot failed: ${e.message}`);
    }

    check(consoleErrors.length === 0, `no console errors (${consoleErrors.length})`);
    check(pageErrors.length === 0, `no page errors (${pageErrors.length})`);

    // Intentional script error must surface inline and not break the shell.
    // Runs last: it legitimately logs to the console.
    await setEditor("throw new Error('gallery-boom');\n");
    await clickRun();
    await page
        .waitForFunction(() => !!document.querySelector('.error'), { timeout: 15000 })
        .catch(() => {});
    const errText = await page.evaluate(
        () => document.querySelector('.error')?.textContent ?? ''
    );
    check(/gallery-boom/.test(errText), 'script error surfaced inline');
    check(
        (await page.evaluate(() => document.querySelectorAll('.item').length)) > 0,
        'shell remains usable after a script error'
    );
} finally {
    await browser.close();
    server.close();
}

if (consoleErrors.length)
    console.error(
        'console errors (may include the intentional error test):\n' + consoleErrors.join('\n')
    );
if (pageErrors.length)
    console.error('page errors (may include the intentional error test):\n' + pageErrors.join('\n'));

if (fails.length) {
    console.error(`\ngallery smoke FAILED: ${fails.length} check(s)`);
    process.exit(1);
}
console.log('\ngallery smoke PASSED');
