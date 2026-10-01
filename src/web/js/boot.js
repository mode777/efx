
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
    __efxResolveAssets();
}

/* F6a async boot. When a host asset-root URL is supplied, fetch the single
   zip, write it into the filesystem, point the provider at it, and only then
   evaluate the entry script; the script-facing load API stays synchronous.
   With no URL the existing resource-root path is unchanged. */
function __efxResolveAssets() {
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
        __efxEvaluateEntry();
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
        __efxEvaluateEntry();
    }).catch(function (e) {
        __efxFail('player: asset root fetch failed: ' +
            (e && e.message ? e.message : e));
    });
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
