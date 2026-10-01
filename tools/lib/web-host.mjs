/*
 * Shared helpers for the puppeteer-based web test runners (P17):
 * loadPuppeteer(), serveStatic(root, routes, opts), launchBrowser(opts) and
 * hostPage(). Each runner keeps its own CLI, env vars and exit codes; only
 * the duplicated dynamic import, static server, browser-launch wiring and
 * host page live here.
 */
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';

export const MIME = {
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

export function mimeFor(file) {
    return MIME[path.extname(file)] ?? 'application/octet-stream';
}

/* dynamic import of puppeteer-core; exits 2 with `message` when unavailable */
export async function loadPuppeteer(
    message = 'puppeteer-core not installed: npm install puppeteer-core'
) {
    try {
        return (await import('puppeteer-core')).default;
    } catch {
        console.error(message);
        process.exit(2);
    }
}

/*
 * Static file server rooted at `root`.
 *  - `routes` maps an exact URL path to a body string, or { contentType, body }.
 *  - `opts.index` names a file served for '/' (e.g. 'index.html').
 *  - `opts.log` logs each served/404 request (goldens' [serve] lines).
 *  - `opts.notFound` is the 404 body (default 'nope').
 */
export function serveStatic(root, routes = {}, opts = {}) {
    const { log = false, index = null, notFound = 'nope' } = opts;
    return http.createServer((req, res) => {
        let urlPath = decodeURIComponent((req.url ?? '/').split('?')[0]);
        if (urlPath === '' || urlPath === '/') {
            if (index) {
                urlPath = '/' + index;
            } else if (Object.prototype.hasOwnProperty.call(routes, '/')) {
                const r = routes['/'];
                const body = typeof r === 'string' ? r : r.body;
                const type =
                    typeof r === 'string' ? 'text/html' : r.contentType ?? 'text/html';
                res.setHeader('Content-Type', type);
                res.end(body);
                return;
            }
        }
        if (Object.prototype.hasOwnProperty.call(routes, urlPath)) {
            const r = routes[urlPath];
            const body = typeof r === 'string' ? r : r.body;
            const type = typeof r === 'string' ? 'text/html' : r.contentType ?? 'text/html';
            res.setHeader('Content-Type', type);
            res.end(body);
            return;
        }
        const file = path.join(root, urlPath.slice(1));
        if (!file.startsWith(root)) {
            res.statusCode = 404;
            res.end(notFound);
            return;
        }
        try {
            const data = fs.readFileSync(file);
            res.setHeader('Content-Type', mimeFor(file));
            if (log) console.log('[serve]', req.url, data.length, 'bytes');
            res.end(data);
        } catch {
            if (log) console.log('[serve] 404:', req.url);
            res.statusCode = 404;
            res.end(notFound);
        }
    });
}

/* launch the pinned headless-shell browser with the given args */
export async function launchBrowser(opts = {}) {
    const puppeteer = opts.puppeteer ?? (await loadPuppeteer(opts.message));
    return puppeteer.launch({
        executablePath: opts.executablePath,
        headless: opts.headless ?? 'shell',
        args: opts.args ?? [
            '--no-sandbox',
            '--use-gl=angle',
            '--use-angle=swiftshader',
            '--enable-unsafe-swiftshader',
            '--disable-gpu-sandbox',
            '--enable-begin-frame-control',
            '--run-all-compositor-stages-before-draw',
        ],
    });
}

/*
 * The shared host page: the canvas the engine renders into, a keep-alive
 * animation so headless Chrome keeps producing BeginFrames (rAF — and with
 * it the emscripten main loop — only ticks inside them), and a counting rAF
 * wrapper. `verbose: true` adds the goldens runner's first-three-tick
 * logging, the rAF-callback stack and the unhandledrejection hook.
 */
export function hostPage({ title, script, verbose = false }) {
    const tickLog = verbose
        ? "\n    if (window.__rafCount <= 3) console.log('[raf] tick ' + window.__rafCount);"
        : '';
    const cbThrow = verbose
        ? "console.log('[raf-cb-throw]', e && (e.message || e), e && e.stack)"
        : "console.log('[raf-cb-throw]', e && (e.message || e))";
    const rejection = verbose
        ? "\nwindow.addEventListener('unhandledrejection', (e) => console.log('[rejection]', e.reason && (e.reason.message || e.reason)));"
        : '';
    return `<!doctype html>
<html><head><meta charset="utf-8"><title>${title}</title></head>
<body><canvas id="canvas" width="640" height="480"></canvas>
<style>@keyframes k { from { transform: translateY(0); } to { transform: translateY(1px); } }</style>
<!-- keep the compositor producing BeginFrames in headless so rAF (and the
     emscripten main loop) keeps ticking; DOM is not part of the GL readback -->
<div style="position:fixed;width:1px;height:1px;background:#123;animation:k 0.016s linear infinite alternate;"></div>
<script>
window.__rafCount = 0;
const __raf = window.requestAnimationFrame.bind(window);
window.requestAnimationFrame = (cb) => {
    window.__rafCount++;${tickLog}
    return __raf((t) => {
        try { cb(t); } catch (e) { ${cbThrow}; throw e; }
    });
};${rejection}
window.addEventListener('error', (e) => console.log('[page-err]', e.message));
</script>
<script src="${script}"></script></body></html>`;
}
