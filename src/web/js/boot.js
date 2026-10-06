
function __efxFail(msg) {
    console.error(msg);
    Module['_efx_bridge_fail']();
    __efxSyncExit();
    __efxMarkEnded();
    __efxNodeExit();
}

function __efxNodeExit() {
    try {
        if (typeof process !== 'undefined' && process.exitCode !== undefined) {
            process.exitCode = __efxExitCode();
        }
    } catch (e) {}
}

function __efxBoot() {
    var st = __efxState();
    if (st.started) {
        return;
    }
    st.started = true;
    /* F6d: --repl has no stdin console on the web build; report it through
       the exit contract without booting the entry script */
    if (Module['_efx_bridge_repl_requested'] &&
        Module['_efx_bridge_repl_requested']()) {
        __efxSyncExit();
        __efxMarkEnded();
        __efxNodeExit();
        return;
    }
    __efxEnsureApi();
    __efxSyncExit();
    __efxDropInstall();
    __efxResolveAssets();
}

/* ---------------------------------------------- F6a web drag-and-drop
   A zip dropped on the canvas is validated, stashed in IndexedDB under a
   one-shot token, and the document reloads with the token in the URL. Boot
   consumes the token, mounts the archive, and evaluates its main.js. This is
   the web analogue of the desktop relaunch (ADR 0056); an invalid or
   un-storable drop is reported on the error channel and the running game is
   left alone. */
var __EFX_DROP_TOKEN = '__efx_drop';
var __EFX_DROP_DB = 'efx-drop';
var __EFX_DROP_STORE = 'roots';
var __EFX_DROP_MAX = 256 * 1024 * 1024;

function __efxDropError(msg) {
    console.error('player: ' + msg);
}

function __efxIdbOpen() {
    return new Promise(function (resolve, reject) {
        var req = indexedDB.open(__EFX_DROP_DB, 1);
        req.onupgradeneeded = function () {
            req.result.createObjectStore(__EFX_DROP_STORE);
        };
        req.onsuccess = function () { resolve(req.result); };
        req.onerror = function () { reject(req.error); };
    });
}

function __efxIdbOp(mode, run) {
    return __efxIdbOpen().then(function (db) {
        return new Promise(function (resolve, reject) {
            var tx = db.transaction(__EFX_DROP_STORE, mode);
            var out = run(tx.objectStore(__EFX_DROP_STORE));
            tx.oncomplete = function () {
                resolve(out && out.result !== undefined ? out.result : undefined);
            };
            tx.onerror = function () { reject(tx.error); };
        });
    });
}

function __efxDropInstall() {
    if (typeof document === 'undefined') {
        return;
    }
    var canvas = document.getElementById('canvas') || document.body;
    if (!canvas || canvas['__efxDropBound']) {
        return;
    }
    canvas['__efxDropBound'] = true;
    canvas.addEventListener('dragover', function (ev) {
        ev.preventDefault();
        if (ev.dataTransfer) {
            ev.dataTransfer.dropEffect = 'copy';
        }
    });
    canvas.addEventListener('drop', function (ev) {
        ev.preventDefault();
        var files = ev.dataTransfer && ev.dataTransfer.files;
        if (files && files.length > 0) {
            __efxHandleDrop(files[0]);
        }
    });
}

function __efxHandleDrop(file) {
    if (typeof indexedDB === 'undefined') {
        __efxDropError('dropped archive cannot be stored (no IndexedDB)');
        return;
    }
    if (file.size > __EFX_DROP_MAX) {
        __efxDropError('dropped archive exceeds 256 MiB');
        return;
    }
    file.arrayBuffer().then(function (buf) {
        var bytes = new Uint8Array(buf);
        var scratch = '/__efx_drop_probe.zip';
        FS.writeFile(scratch, bytes);
        var p = __efxAllocCStr(scratch);
        var ok = Module['_efx_bridge_probe_root'](p);
        Module['_free'](p);
        try { FS.unlink(scratch); } catch (e) {}
        if (!ok) {
            __efxDropError('dropped file is not a game (no main.js)');
            return;
        }
        var token = String(Date.now()) + '-' +
            Math.random().toString(36).slice(2);
        __efxIdbOp('readwrite', function (store) {
            return store.put(bytes, token);
        }).then(function () {
            var url = new URL(location.href);
            url.searchParams.set(__EFX_DROP_TOKEN, token);
            location.replace(url.toString());
        }).catch(function (e) {
            __efxDropError('could not store dropped archive: ' +
                (e && e.message ? e.message : e));
        });
    }).catch(function (e) {
        __efxDropError('could not read dropped file: ' +
            (e && e.message ? e.message : e));
    });
}

function __efxConsumeDropToken() {
    var token = null;
    try {
        token = new URLSearchParams(location.search).get(__EFX_DROP_TOKEN);
    } catch (e) {
        token = null;
    }
    if (!token) {
        return Promise.resolve(false);
    }
    /* clear the token from the address bar so it is not re-consumed or
       observable after boot */
    try {
        var clean = new URL(location.href);
        clean.searchParams.delete(__EFX_DROP_TOKEN);
        history.replaceState(null, '', clean.toString());
    } catch (e) {}
    return __efxIdbOp('readonly', function (store) {
        return store.get(token);
    }).then(function (bytes) {
        __efxIdbOp('readwrite', function (store) {
            return store.delete(token);
        }).catch(function () {});
        if (!bytes) {
            return false;
        }
        FS.writeFile('__efx_assets.zip', new Uint8Array(bytes));
        var p = __efxAllocCStr('__efx_assets.zip');
        var ok = Module['_efx_bridge_set_root'](p);
        Module['_free'](p);
        if (!ok) {
            __efxFail('player: dropped archive could not be opened');
        }
        return true;
    });
}

/* F6a async boot. When a host asset-root URL is supplied, fetch the single
   zip, write it into the filesystem, point the provider at it, and only then
   evaluate the entry script; the script-facing load API stays synchronous.
   With no URL the existing resource-root path is unchanged. */
function __efxResolveAssets() {
    /* a dropped archive, stashed by the previous document and named by the
       URL token, takes precedence over the host asset URL */
    __efxConsumeDropToken().then(function (used) {
        if (used) {
            __efxStartAfterAssets();
            return;
        }
        __efxResolveAssetsFromUrl();
    }).catch(function (e) {
        __efxFail('player: dropped archive restore failed: ' +
            (e && e.message ? e.message : e));
    });
}

function __efxResolveAssetsFromUrl() {
    var url = null;
    try {
        var v = globalThis['__efx_assets'];
        if (typeof v === 'string') {
            url = v;
        }
    } catch (e) {}
    if (url === null) {
        try {
            url = new URLSearchParams(location.search).get('assets');
        } catch (e) {
            url = null;
        }
    }
    if (url === null || url === '') {
        __efxStartAfterAssets();
        return;
    }
    /* consume the host channel before anything else runs */
    try {
        delete globalThis['__efx_assets'];
    } catch (e) {}
    fetch(url).then(function (resp) {
        if (!resp.ok) {
            throw new Error('HTTP ' + resp.status);
        }
        return resp.arrayBuffer();
    }).then(function (buf) {
        FS.writeFile('__efx_assets.zip', new Uint8Array(buf));
        var p = __efxAllocCStr('__efx_assets.zip');
        var ok = Module['_efx_bridge_set_root'](p);
        Module['_free'](p);
        if (!ok) {
            throw new Error('asset archive could not be opened');
        }
        __efxStartAfterAssets();
    }).catch(function (e) {
        __efxFail('player: asset root fetch failed: ' +
            (e && e.message ? e.message : e));
    });
}

/* ADR 0016: on the DOM the sokol loop starts first and the C init callback
   evaluates the entry once WebGL and the engine pipelines are up; the Node
   harness has no rendering surface, so it evaluates directly and drives the
   frame loop itself. */
function __efxStartAfterAssets() {
    var dom = false;
    try {
        dom = typeof document !== 'undefined';
    } catch (e) {
        dom = false;
    }
    if (dom) {
        Module['_efx_web_start_loop']();
        return;
    }
    __efxEvaluateEntry();
    __efxNodeFrameLoop();
}

function __efxNodeFrameLoop() {
    __efxSyncExit();
    var maxFrames = 100000;
    try {
        if (typeof process !== 'undefined' && process.env && process.env['EFX_WEB_MAX_FRAMES']) {
            maxFrames = parseInt(process.env['EFX_WEB_MAX_FRAMES'], 10) || maxFrames;
        }
    } catch (e) {}
    var guard = 0;
    while (guard < maxFrames && Module['_efx_bridge_frame']() === 0) {
        guard++;
    }
    __efxSyncExit();
    __efxMarkEnded();
    __efxNodeExit();
}

/* F10: report a module-runtime/entry failure through the same error/exit
   contract the classic entry used, preserving the `efx.quit` sentinel. */
function __efxModuleFail(st, e) {
    if (st.quitSentinel !== null && e === st.quitSentinel) {
        __efxSyncExit();
        __efxMarkEnded();
        __efxNodeExit();
        return;
    }
    Module['_efx_bridge_set_error']();
    __efxReportError(e);
    __efxSyncExit();
    __efxMarkEnded();
    __efxNodeExit();
}

function __efxEvaluateEntry() {
    var st = __efxState();
    if (st.evaluated) {
        return;
    }
    st.evaluated = true;
    /* host-global shadow: the entry (and every module it requires) is
       evaluated with these free globals denied, so host/browser/Node
       facilities stay unreachable even though module bodies compile through
       the page's global scope. `require`/`module`/`exports` are module-scoped
       parameters and are only denied as free globals. */
    var hostGlobals = ['window', 'document', 'require', 'process', 'fetch',
        'XMLHttpRequest', 'module', 'exports', 'Buffer', 'global'];
    var deny = {};
    for (var gi = 0; gi < hostGlobals.length; gi++) {
        deny[hostGlobals[gi]] = 1;
    }
    var shadowGlobal = new Proxy(globalThis, {
        has: function (t, k) {
            return !deny[k] && (k in t);
        },
        get: function (t, k) {
            if (k === 'globalThis') {
                return shadowGlobal;
            }
            if (deny[k]) {
                return undefined;
            }
            return t[k];
        },
        set: function (t, k, v) {
            t[k] = v;
            return true;
        },
    });
    var moduleHostGlobals = ['window', 'document', 'process', 'fetch',
        'XMLHttpRequest', 'Buffer', 'global'];
    var runtime;
    try {
        runtime = st.createModuleRuntime(st.api, {
            hostGlobals: moduleHostGlobals,
            globalObject: shadowGlobal,
        });
    } catch (e) {
        __efxModuleFail(st, e);
        return;
    }
    /* Host-provided entry source (web gallery embedding): when the embedding
       page supplies `globalThis.__efx_main_js` before boot it replaces the
       resource-root `main.js`. The channel is consumed and deleted before the
       script is evaluated so the entry script can never observe it. */
    var code = null;
    var hostSource = null;
    try {
        hostSource = globalThis['__efx_main_js'];
    } catch (e) {
        hostSource = null;
    }
    if (typeof hostSource === 'string') {
        try {
            delete globalThis['__efx_main_js'];
        } catch (e) {}
        code = hostSource;
    } else {
        var root = UTF8ToString(Module['_efx_web_root']());
        var isDir = false;
        var isFile = false;
        try {
            var stat = FS.stat(root);
            isDir = FS.isDir(stat.mode);
            isFile = !isDir;
        } catch (e) {
            isDir = false;
            isFile = false;
        }
        if (!isDir && !isFile) {
            __efxFail('player: resource root is not a directory: ' + root);
            return;
        }
        /* read main.js through the provider so directory and zip roots work
           identically (F6a) */
        var mp = __efxAllocCStr('main.js');
        var mptr = Module['_efx_bridge_load_text'](mp);
        Module['_free'](mp);
        if (!mptr) {
            __efxFail('player: no main.js in resource root: ' + root);
            return;
        }
        code = UTF8ToString(mptr);
        Module['_free'](mptr);
    }
    var res;
    try {
        res = runtime.runEntry('main.js', code);
    } catch (e) {
        __efxModuleFail(st, e);
        return;
    }
    /* explicit hooks registered during evaluation stay first; the entry's
       exported (or module-local) hooks are appended once each */
    if (res && typeof res.update === 'function') {
        st.updateHooks.push({ fn: res.update, active: true });
    }
    if (res && typeof res.render === 'function') {
        st.renderHooks.push({ fn: res.render, active: true });
    }
    __efxSyncExit();
}
