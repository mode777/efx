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
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';
import { loadPuppeteer, serveStatic, launchBrowser } from './lib/web-host.mjs';

const puppeteer = await loadPuppeteer(
    'puppeteer-core not installed: npm install --no-save puppeteer-core'
);

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

const server = serveStatic(
    DIST,
    {
        '/__drop_fixture.zip': {
            contentType: 'application/zip',
            body: fs.readFileSync(path.join(ROOT, 'tests', 'fixtures', 'root_zip.zip')),
        },
    },
    { index: 'index.html', notFound: 'not found' }
);

const fails = [];
function check(ok, label) {
    console.log(`${ok ? 'ok  ' : 'FAIL'}  ${label}`);
    if (!ok) fails.push(label);
}

await new Promise((r) => server.listen(PORT, r));
console.log(`serving ${DIST} on :${PORT}`);

const browser = await launchBrowser({
    puppeteer,
    executablePath: CHROME,
    args: [
        '--no-sandbox',
        '--disable-dev-shm-usage',
        '--use-gl=swiftshader',
        '--enable-unsafe-swiftshader',
        '--enable-webgl',
    ],
});

const consoleErrors = [];
const consoleAll = [];
const navigations = [];
const pageErrors = [];
try {
    const page = await browser.newPage();
    page.on('console', (m) => {
        consoleAll.push(m.text());
        if (m.type() === 'error') consoleErrors.push(m.text());
    });
    page.on('framenavigated', (f) => navigations.push(f.url()));
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

    // Keyboard must reach the running sample through the embed once the
    // application area is focused: Sokol's key listeners live on the runner
    // document's window, so the click must move focus into the iframe
    // (ADR 0043). The sample records both the onDown callback and the held
    // query into globals the smoke reads back.
    await setEditor(
        "globalThis.__kb_event = false;\n" +
            "globalThis.__kb_down = false;\n" +
            "efx.keyboard.onDown(function (e) { if (e.key === 'a') globalThis.__kb_event = true; });\n" +
            "function update() { if (efx.keyboard.isDown('a')) globalThis.__kb_down = true; }\n" +
            "function render() {}\n"
    );
    await clickRun();
    const kbFrame = await waitBoot();
    check(!!kbFrame, 'keyboard sample boots');
    if (kbFrame) {
        const fbox = await page.evaluate(() => {
            const r = document.querySelector('iframe.frame').getBoundingClientRect();
            return { x: r.x, y: r.y, w: r.width, h: r.height };
        });
        await page.mouse.click(fbox.x + fbox.w / 2, fbox.y + fbox.h / 2);
        await new Promise((r) => setTimeout(r, 200));
        const focused = await kbFrame.evaluate(() => document.hasFocus()).catch(() => false);
        check(focused, 'clicking the application area focuses the runner document');

        await page.keyboard.down('KeyA');
        await new Promise((r) => setTimeout(r, 200));
        const seen = await kbFrame
            .evaluate(() => ({ ev: globalThis.__kb_event, down: globalThis.__kb_down }))
            .catch(() => ({}));
        await page.keyboard.up('KeyA');
        check(
            seen.ev === true || seen.down === true,
            `keyboard reaches the running sample (${JSON.stringify(seen)})`
        );
    }

    check(consoleErrors.length === 0, `no console errors (${consoleErrors.length})`);
    check(pageErrors.length === 0, `no page errors (${pageErrors.length})`);

    // A dropped archive loads as the active sample (ADR 0056): the boot glue
    // validates it, stashes it, and reloads the runner bound to it; an
    // unusable drop is reported and leaves the running game alone.
    async function dropIntoRunner(bytes, name) {
        const target = runner();
        if (!target) return false;
        return target
            .evaluate(
                async (arr, n) => {
                    const file = new File([new Uint8Array(arr)], n, { type: 'application/zip' });
                    const dt = new DataTransfer();
                    dt.items.add(file);
                    const canvas = document.getElementById('canvas') || document.body;
                    canvas.dispatchEvent(
                        new DragEvent('dragover', { dataTransfer: dt, bubbles: true, cancelable: true })
                    );
                    canvas.dispatchEvent(
                        new DragEvent('drop', { dataTransfer: dt, bubbles: true, cancelable: true })
                    );
                    return true;
                },
                Array.from(bytes),
                name
            )
            .catch(() => false);
    }

    const dropZip = fs.readFileSync(path.join(ROOT, 'tests', 'fixtures', 'root_zip.zip'));
    async function waitFor(pred, timeout = 15000) {
        const start = Date.now();
        while (Date.now() - start < timeout) {
            if (pred()) return true;
            await new Promise((r) => setTimeout(r, 200));
        }
        return false;
    }
    await dropIntoRunner(dropZip, 'root_zip.zip');
    const reloaded = await waitFor(() => navigations.some((u) => u.includes('__efx_drop=')));
    check(reloaded, 'dropped archive reloads the runner bound to it');
    await waitBoot();
    const droppedRan = await waitFor(() => consoleAll.some((t) => t.includes('zip-entry-ok')));
    check(droppedRan, 'dropped archive runs as the active sample');

    const beforeInvalid = runner() ? runner().url() : '';
    await dropIntoRunner(new Uint8Array([0, 1, 2, 3, 4, 5]), 'bad.bin');
    await new Promise((r) => setTimeout(r, 500));
    const afterInvalid = runner() ? runner().url() : '';
    check(
        consoleAll.some((t) => t.includes('not a game')),
        'unusable drop is reported on the error channel'
    );
    check(afterInvalid === beforeInvalid, 'unusable drop does not reload the runner');

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

    // The game gallery series must each boot cleanly. Run last so selecting
    // samples cannot perturb the editor checks above; any console/page error
    // raised by a game is measured as a delta from this point.
    const errBase = consoleErrors.length;
    const pageErrBase = pageErrors.length;
    const GAME_TITLES = [
        'Neon Pong',
        'Bloom Breakout',
        'Glow Gauntlet',
        'Mini Golf',
        'Sky Steps',
        'Bot Arena',
    ];
    for (const title of GAME_TITLES) {
        const clicked = await page.evaluate((t) => {
            const items = [...document.querySelectorAll('.item')];
            const hit = items.find(
                (el) => ((el.querySelector('.name') || {}).textContent || '').trim() === t
            );
            if (hit) hit.click();
            return !!hit;
        }, title);
        check(clicked, `game sample present: ${title}`);
        if (clicked) {
            const ok = !!(await waitBoot());
            check(ok, `game sample runs: ${title}`);
        }
    }
    check(consoleErrors.length === errBase, `game samples add no console errors (${consoleErrors.length - errBase})`);
    check(pageErrors.length === pageErrBase, `game samples add no page errors (${pageErrors.length - pageErrBase})`);
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
