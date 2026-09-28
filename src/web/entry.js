/* eslint-disable */
// Native-JS runtime glue (f2b, ADR 0022): loads <root>/main.js with the
// host JS engine, exposes the same `efx` namespace contract as the desktop
// quickjs binding, and mirrors the desktop error/exit-code contract.
// Only hoisted function declarations live at top level: on synchronous
// (Node) builds the postRun boot fires before this file's statements run.

function __efxState() {
    var st = globalThis['__efx_state'];
    if (!st) {
        st = { api: null, quitSentinel: null, updateHooks: [], renderHooks: [], started: false };
        globalThis['__efx_state'] = st;
    }
    return st;
}

function __efxExitCode() {
    return Module['_efx_bridge_exit_code']();
}

function __efxSyncExit() {
    Module['efxExitCode'] = __efxExitCode();
}

function __efxMarkEnded() {
    Module['efxRunEnded'] = true;
}

function __efxCStr(v) {
    try {
        return String(v);
    } catch (e) {
        return null;
    }
}

/* allocate a NUL-terminated UTF-8 copy of a JS string in wasm memory */
function __efxAllocCStr(s) {
    var len = lengthBytesUTF8(s) + 1;
    var ptr = Module['_malloc'](len);
    stringToUTF8(s, ptr, len);
    return ptr;
}

function __efxReportError(e) {
    var msg = (e && typeof e.message === 'string') ? e.message : null;
    var printed = null;
    if (msg !== null) {
        printed = msg;
    } else {
        printed = __efxCStr(e);
    }
    console.error('uncaught exception: ' + (printed === null ? '<unprintable throw value>' : printed));
    if (e && e.stack) {
        console.error(String(e.stack));
    }
}

function __efxNumber(v, typeMsg) {
    try {
        return Number(v);
    } catch (e) {
        throw new TypeError(typeMsg);
    }
}

function __efxFinite(v, typeMsg) {
    var d = __efxNumber(v, typeMsg);
    if (!isFinite(d)) {
        throw new TypeError(typeMsg);
    }
    return d;
}

function __efxFloatArray(v, n) {
    var i, out;
    if (v instanceof Uint8Array) {
        if (v.length !== n) {
            throw new RangeError('wrong buffer length');
        }
        out = new Array(n);
        for (i = 0; i < n; i++) {
            out[i] = v[i];
        }
        return out;
    }
    if (Array.isArray(v)) {
        if (v.length !== n) {
            throw new RangeError('wrong array length');
        }
        out = new Array(n);
        for (i = 0; i < n; i++) {
            var d;
            try {
                d = Number(v[i]);
            } catch (e) {
                throw new RangeError('array elements must be finite numbers');
            }
            if (!isFinite(d)) {
                throw new RangeError('array elements must be finite numbers');
            }
            out[i] = d;
        }
        return out;
    }
    throw new TypeError('expected an array');
}

function __efxIsObject(v) {
    return v !== null && (typeof v === 'object' || typeof v === 'function');
}

function __efxEnsureApi() {
    var st = __efxState();
    if (st.api) {
        return st;
    }
    var bridge = Module;

    st.dispatch = function (which, dt) {
        var hooks = which ? st.updateHooks : st.renderHooks;
        for (var i = 0; i < hooks.length; i++) {
            var entry = hooks[i];
            if (!entry.active) {
                continue;
            }
            try {
                if (which) {
                    entry.fn(dt);
                } else {
                    entry.fn();
                }
            } catch (e) {
                if (st.quitSentinel !== null && e === st.quitSentinel) {
                    return 1;
                }
                bridge['_efx_bridge_set_error']();
                __efxReportError(e);
                __efxSyncExit();
                return 2;
            }
        }
        return 0;
    };
    globalThis['__efxDispatchHook'] = st.dispatch;

    // explicit hook registration (ADR 0016): entries are marked inactive on
    // unsubscribe instead of spliced, so a hook may unsubscribe itself while
    // it is running (desktop parity, design D1)
    function makeRegister(which) {
        return function (fn) {
            if (typeof fn !== 'function') {
                throw new TypeError('hook must be a function');
            }
            var entry = { fn: fn, active: true };
            (which ? st.updateHooks : st.renderHooks).push(entry);
            return function () {
                entry.active = false;
            };
        };
    }

    function EfxImageData(id) {
        this.__id = id;
        this.__alive = true;
    }
    EfxImageData.prototype.destroy = function () {
        if (!(this instanceof EfxImageData)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_imagedata_destroy'](this.__id);
    };
    Object.defineProperty(EfxImageData.prototype, 'width', {
        get: function () {
            if (!(this instanceof EfxImageData)) {
                throw new TypeError('expected an ImageData');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_imagedata_width'](this.__id);
        },
    });
    Object.defineProperty(EfxImageData.prototype, 'height', {
        get: function () {
            if (!(this instanceof EfxImageData)) {
                throw new TypeError('expected an ImageData');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_imagedata_height'](this.__id);
        },
    });

    function EfxTexture(handle, permanent) {
        this.__handle = handle;
        this.__alive = true;
        this.__permanent = !!permanent;
    }
    EfxTexture.prototype.destroy = function () {
        if (!(this instanceof EfxTexture)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        if (this.__permanent) {
            throw new TypeError('cannot destroy an engine-owned texture');
        }
        this.__alive = false;
        bridge['_efx_bridge_texture_destroy'](this.__handle);
    };

    /* read-only query properties, resolved through the render layer's
       texture registry at read time (parity with the desktop binding) */
    Object.defineProperty(EfxTexture.prototype, 'width', {
        get: function () {
            if (!(this instanceof EfxTexture)) {
                throw new TypeError('expected a Texture');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_texture_width'](this.__handle);
        },
    });
    Object.defineProperty(EfxTexture.prototype, 'height', {
        get: function () {
            if (!(this instanceof EfxTexture)) {
                throw new TypeError('expected a Texture');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_texture_height'](this.__handle);
        },
    });

    /* F5a: a render target is a native-backed class like Texture —
       deterministic destroy, GC-reachable via JS, read-only size */
    function EfxRenderTarget(handle) {
        this.__handle = handle;
        this.__alive = true;
    }
    EfxRenderTarget.prototype.destroy = function () {
        if (!(this instanceof EfxRenderTarget)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_target_destroy'](this.__handle);
    };
    Object.defineProperty(EfxRenderTarget.prototype, 'width', {
        get: function () {
            if (!(this instanceof EfxRenderTarget)) {
                throw new TypeError('expected a RenderTarget');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_target_width'](this.__handle);
        },
    });
    Object.defineProperty(EfxRenderTarget.prototype, 'height', {
        get: function () {
            if (!(this instanceof EfxRenderTarget)) {
                throw new TypeError('expected a RenderTarget');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_target_height'](this.__handle);
        },
    });

    function EfxMeshData(id) {
        this.__id = id;
        this.__alive = true;
    }    EfxMeshData.prototype.destroy = function () {
        if (!(this instanceof EfxMeshData)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_meshdata_destroy'](this.__id);
    };
    Object.defineProperty(EfxMeshData.prototype, 'surfaceCount', {
        get: function () {
            if (!(this instanceof EfxMeshData)) {
                throw new TypeError('expected a MeshData');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_meshdata_surface_count'](this.__id);
        },
    });

    function EfxMesh(handle) {
        this.__handle = handle;
        this.__alive = true;
    }
    EfxMesh.prototype.destroy = function () {
        if (!(this instanceof EfxMesh)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_mesh_destroy'](this.__handle);
    };
    Object.defineProperty(EfxMesh.prototype, 'surfaceCount', {
        get: function () {
            if (!(this instanceof EfxMesh)) {
                throw new TypeError('expected a Mesh');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_mesh_surface_count'](this.__handle);
        },
    });

    function liveMeshData(v) {
        if (!(v instanceof EfxMeshData)) {
            throw new TypeError('expected a MeshData');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed resource');
        }
        return v;
    }

    function liveMesh(v) {
        if (!(v instanceof EfxMesh)) {
            throw new TypeError('expected a Mesh');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed resource');
        }
        return v;
    }

    /* flat number array (JS array or typed array) -> Float32Array copy;
       element rules mirror the desktop binding: non-number TypeError,
       non-finite RangeError */
    function __efxFloat32Array(v, what) {
        if (!Array.isArray(v) && !ArrayBuffer.isView(v)) {
            throw new TypeError(what + ' must be an array');
        }
        var n = v.length;
        var out = new Float32Array(n);
        for (var i = 0; i < n; i++) {
            var d = v[i];
            if (typeof d !== 'number') {
                throw new TypeError('array elements must be numbers');
            }
            if (!isFinite(d)) {
                throw new RangeError('array elements must be finite numbers');
            }
            out[i] = d;
        }
        return out;
    }

    function __efxUint32Array(v, what) {
        what = what || 'indices';
        if (!Array.isArray(v) && !ArrayBuffer.isView(v)) {
            throw new TypeError(what + ' must be an array');
        }
        var n = v.length;
        var out = new Uint32Array(n);
        for (var i = 0; i < n; i++) {
            var d = v[i];
            if (typeof d !== 'number') {
                throw new TypeError('array elements must be numbers');
            }
            if (!isFinite(d) || d < 0 || d > 4294967295 || d !== Math.floor(d)) {
                throw new RangeError('array elements must be integers in [0, 2^32-1]');
            }
            out[i] = d;
        }
        return out;
    }

    /* Parse a Phong material object into the F4a 17-float wire layout
       [ambient(4), diffuse(4), specular(4), emissive(4), shininess] plus the
       F4b map-handle vector [ambient, diffuse, specular, emissive, alphaMask]
       (doubles; 0 = absent). Desktop parity: unknown fields throw; a map/
       alphaMask must be a live Texture or RenderTarget (F5a). */
    function __efxMaterial(v) {
        if (!__efxIsObject(v)) {
            throw new TypeError('material must be an object');
        }
        var known = { ambient: 1, diffuse: 1, specular: 1, emissive: 1,
                      alphaMask: 1 };
        var names = Object.getOwnPropertyNames(v);
        for (var i = 0; i < names.length; i++) {
            if (!known[names[i]]) {
                throw new TypeError("unknown material option '" + names[i] + "'");
            }
        }
        var out = new Float32Array(17);
        out[0] = 0; out[1] = 0; out[2] = 0; out[3] = 1;   /* ambient */
        out[4] = 1; out[5] = 1; out[6] = 1; out[7] = 1;   /* diffuse */
        out[8] = 0; out[9] = 0; out[10] = 0; out[11] = 1; /* specular */
        out[12] = 0; out[13] = 0; out[14] = 0; out[15] = 1; /* emissive */
        out[16] = 32;                                     /* shininess */
        var maps = new Float64Array(5);                   /* all absent (0) */
        var chan = ['ambient', 'diffuse', 'specular', 'emissive'];
        for (var ci = 0; ci < 4; ci++) {
            var ch = v[chan[ci]];
            if (ch === undefined || ch === null) {
                continue;
            }
            if (!__efxIsObject(ch)) {
                throw new TypeError(chan[ci] + ' channel must be an object');
            }
            var ck = ci === 2 ? { color: 1, shininess: 1, map: 1 }
                              : { color: 1, map: 1 };
            var cnames = Object.getOwnPropertyNames(ch);
            for (var k = 0; k < cnames.length; k++) {
                if (!ck[cnames[k]]) {
                    throw new TypeError("unknown " + chan[ci] +
                        " option '" + cnames[k] + "'");
                }
            }
            if (ch.color === undefined) {
                throw new TypeError(chan[ci] + ' channel requires color');
            }
            var c = __efxFloatArray(ch.color, 4);
            out[ci * 4] = c[0];
            out[ci * 4 + 1] = c[1];
            out[ci * 4 + 2] = c[2];
            out[ci * 4 + 3] = c[3];
            if (ch.map !== undefined && ch.map !== null) {
                maps[ci] = liveSample(ch.map).handle;
            }
            if (ci === 2 && ch.shininess !== undefined) {
                if (typeof ch.shininess !== 'number') {
                    throw new TypeError('shininess must be a number');
                }
                if (!isFinite(ch.shininess) || ch.shininess <= 0) {
                    throw new RangeError('shininess must be Finite and > 0');
                }
                out[16] = ch.shininess;
            }
        }
        if (v.alphaMask !== undefined && v.alphaMask !== null) {
            maps[4] = liveSample(v.alphaMask).handle;
        }
        return { blocks: out, maps: maps };
    }

    function mallocCopyF32(arr) {
        var ptr = bridge['_malloc'](arr.length * 4);
        HEAPF32.set(arr, ptr >> 2);
        return ptr;
    }

    function mallocCopyF64(arr) {
        var ptr = bridge['_malloc'](arr.length * 8);
        HEAPF64.set(arr, ptr >> 3);
        return ptr;
    }

    function mallocCopyU32(arr) {
        var ptr = bridge['_malloc'](arr.length * 4);
        HEAPU32.set(arr, ptr >> 2);
        return ptr;
    }

    /* allocate a NUL-terminated UTF-8 copy of a JS string in wasm memory */
    function __efxAllocCStr(s) {
        var len = lengthBytesUTF8(s) + 1;
        var ptr = bridge['_malloc'](len);
        stringToUTF8(s, ptr, len);
        return ptr;
    }


    /* F5b: parse one post-effect chain entry into the 9-float wire layout
       (desktop parity: unknown field -> TypeError, non-number -> TypeError,
       non-finite -> RangeError; bounds are enforced engine-side). */
    function __efxPostNumber(v, what) {
        if (typeof v !== 'number') {
            throw new TypeError(what + ' must be a number');
        }
        if (!isFinite(v)) {
            throw new RangeError(what + ' must be a finite number');
        }
        return v;
    }
    function __efxPostEntry(v) {
        if (!__efxIsObject(v)) {
            throw new TypeError('post-effect entry must be an object');
        }
        var effect = v['effect'];
        if (typeof effect !== 'string') {
            throw new TypeError('post-effect entry requires an effect name');
        }
        var known;
        var out = new Float32Array(9);
        out[1] = 1;
        if (effect === 'colorFilter') {
            known = { effect: 1, mix: 1, brightness: 1, contrast: 1,
                      saturation: 1, tint: 1 };
            out[0] = 0; out[2] = 1; out[3] = 1; out[4] = 1;
            out[5] = 1; out[6] = 1; out[7] = 1; out[8] = 1;
        } else if (effect === 'blur') {
            known = { effect: 1, mix: 1, radius: 1 };
            out[0] = 1; out[2] = 1;
        } else if (effect === 'bloom') {
            known = { effect: 1, mix: 1, threshold: 1, strength: 1 };
            out[0] = 2; out[2] = 0.8; out[3] = 0.5;
        } else {
            throw new TypeError('unknown post effect');
        }
        var names = Object.getOwnPropertyNames(v);
        for (var i = 0; i < names.length; i++) {
            if (!known[names[i]]) {
                throw new TypeError("unknown post effect option '" + names[i] + "'");
            }
        }
        if (v['mix'] !== undefined) {
            out[1] = __efxPostNumber(v['mix'], 'mix');
        }
        if (effect === 'colorFilter') {
            if (v['brightness'] !== undefined) {
                out[2] = __efxPostNumber(v['brightness'], 'brightness');
            }
            if (v['contrast'] !== undefined) {
                out[3] = __efxPostNumber(v['contrast'], 'contrast');
            }
            if (v['saturation'] !== undefined) {
                out[4] = __efxPostNumber(v['saturation'], 'saturation');
            }
            if (v['tint'] !== undefined) {
                var t = __efxFloatArray(v['tint'], 4);
                for (var k = 0; k < 4; k++) {
                    out[5 + k] = t[k];
                }
            }
        } else if (effect === 'blur') {
            if (v['radius'] !== undefined) {
                out[2] = __efxPostNumber(v['radius'], 'radius');
            }
        } else {
            if (v['threshold'] !== undefined) {
                out[2] = __efxPostNumber(v['threshold'], 'threshold');
            }
            if (v['strength'] !== undefined) {
                out[3] = __efxPostNumber(v['strength'], 'strength');
            }
        }
        return out;
    }

    /* persistent scratch for per-draw uniforms (drawMesh is a hot path):
       16 floats transform + 4 floats color, allocated once */
    var drawScratch = 0;
    function drawScratchPtr() {
        if (!drawScratch) {
            drawScratch = bridge['_malloc'](20 * 4);
        }
        return drawScratch;
    }

    function liveImageData(v) {
        if (!(v instanceof EfxImageData)) {
            throw new TypeError('expected an ImageData');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed resource');
        }
        return v;
    }

    function liveTexture(v) {
        if (!(v instanceof EfxTexture)) {
            throw new TypeError('expected a Texture');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed resource');
        }
        return v;
    }

    /* F5a texture coercion: a live Texture or a live RenderTarget is
       accepted wherever a sampling source is required. Returns
       { handle, w, h } with the size resolved through the owning registry. */
    function liveSample(v) {
        if (v instanceof EfxTexture) {
            if (!v.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return {
                handle: v.__handle,
                w: bridge['_efx_bridge_texture_width'](v.__handle),
                h: bridge['_efx_bridge_texture_height'](v.__handle),
            };
        }
        if (v instanceof EfxRenderTarget) {
            if (!v.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return {
                handle: v.__handle,
                w: bridge['_efx_bridge_target_width'](v.__handle),
                h: bridge['_efx_bridge_target_height'](v.__handle),
            };
        }
        throw new TypeError('expected a Texture or RenderTarget');
    }

    var api = {
        log: function (msg) {
            var s = null;
            if (arguments.length > 0) {
                s = __efxCStr(msg);
            }
            console.log(s === null ? '' : s);
        },
        quit: function (code) {
            var c = 0;
            if (arguments.length > 0) {
                try {
                    c = Number(code) | 0;
                } catch (e) {
                    c = 0;
                }
            }
            bridge['_efx_bridge_quit'](c);
            throw __efxState().quitSentinel;
        },
        args: function () {
            var n = bridge['_efx_bridge_arg_count']();
            var out = new Array(n);
            for (var i = 0; i < n; i++) {
                out[i] = UTF8ToString(bridge['_efx_bridge_arg'](i));
            }
            return out;
        },
        registerUpdateHook: makeRegister(1),
        registerRenderHook: makeRegister(0),
        setClearColor: function (color) {
            if (arguments.length < 1) {
                throw new TypeError('setClearColor requires a [r,g,b,a] array');
            }
            var c = __efxFloatArray(color, 4);
            bridge['_efx_bridge_set_clear_color'](c[0], c[1], c[2], c[3]);
        },
        setCamera2D: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('setCamera2D requires an options object');
            }
            var frameW = 0, frameH = 0, x = NaN, y = NaN, zoom = 1, rotation = 0;
            var frame = opts['frame'];
            if (frame !== undefined) {
                var f = __efxFloatArray(frame, 2);
                if (!(f[0] > 0 && f[1] > 0)) {
                    throw new RangeError('frame must be positive');
                }
                frameW = f[0];
                frameH = f[1];
                if (isNaN(x)) {
                    x = frameW * 0.5;
                }
                if (isNaN(y)) {
                    y = frameH * 0.5;
                }
            }
            var xv = opts['x'];
            if (xv !== undefined) {
                x = __efxFinite(xv, 'camera fields must be finite numbers');
            }
            var yv = opts['y'];
            if (yv !== undefined) {
                y = __efxFinite(yv, 'camera fields must be finite numbers');
            }
            var zv = opts['zoom'];
            if (zv !== undefined) {
                zoom = __efxFinite(zv, 'camera fields must be finite numbers');
            }
            var rv = opts['rotation'];
            if (rv !== undefined) {
                rotation = __efxFinite(rv, 'camera fields must be finite numbers');
            }
            if (!(zoom > 0)) {
                throw new RangeError('zoom must be > 0');
            }
            bridge['_efx_bridge_set_camera'](frameW, frameH, x, y, zoom, rotation);
        },
        createImageData: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('createImageData requires an options object');
            }
            var w, h, bad = false;
            try {
                w = Number(opts['width']) | 0;
                h = Number(opts['height']) | 0;
            } catch (e) {
                bad = true;
            }
            if (bad || w <= 0 || h <= 0) {
                throw new RangeError('width and height must be positive');
            }
            var n = w * h * 4;
            if (n > 0x7fffffff) {
                throw new RangeError('image too large');
            }
            var pixels = opts['pixels'];
            if (pixels === undefined) {
                throw new TypeError('createImageData requires pixels');
            }
            var ptr = bridge['_efx_bridge_imagedata_alloc'](n);
            if (!ptr) {
                throw new Error('out of memory');
            }
            try {
                if (Array.isArray(pixels)) {
                    for (var i = 0; i < n; i++) {
                        var d;
                        try {
                            d = Number(pixels[i]);
                        } catch (e) {
                            throw new RangeError('pixel bytes must be integers 0..255');
                        }
                        if (d < 0 || d > 255 || d !== (d | 0)) {
                            throw new RangeError('pixel bytes must be integers 0..255');
                        }
                        HEAPU8[ptr + i] = d;
                    }
                } else if (pixels instanceof Uint8Array) {
                    if (pixels.length !== n) {
                        throw new RangeError('pixels length must be width*height*4');
                    }
                    HEAPU8.set(pixels, ptr);
                } else {
                    throw new TypeError('pixels must be an array or typed array');
                }
            } catch (e) {
                bridge['_efx_bridge_mem_free'](ptr);
                throw e;
            }
            var fmt = opts['format'];
            if (fmt !== undefined) {
                var fs = __efxCStr(fmt);
                if (fs === null || fs !== 'rgba8') {
                    bridge['_efx_bridge_mem_free'](ptr);
                    throw new RangeError("unsupported image format (only 'rgba8')");
                }
            }
            var known = { width: 1, height: 1, pixels: 1, format: 1 };
            var names = Object.getOwnPropertyNames(opts);
            var unknown = null;
            for (var k = 0; k < names.length; k++) {
                if (!known[names[k]]) {
                    unknown = names[k];
                }
            }
            if (unknown !== null) {
                bridge['_efx_bridge_mem_free'](ptr);
                throw new TypeError("unknown option '" + unknown + "'");
            }
            var id = bridge['_efx_bridge_imagedata_commit'](w, h, ptr);
            if (!id) {
                bridge['_efx_bridge_mem_free'](ptr);
                throw new Error('out of memory');
            }
            return new EfxImageData(id);
        },
        loadText: function (path) {
            if (arguments.length < 1 || typeof path !== 'string') {
                throw new TypeError('loadText requires a path string');
            }
            var p = __efxAllocCStr(path);
            var ptr = bridge['_efx_bridge_load_text'](p);
            bridge['_efx_bridge_mem_free'](p);
            if (!ptr) {
                throw new Error('resource not found');
            }
            var s = UTF8ToString(ptr);
            bridge['_efx_bridge_mem_free'](ptr);
            return s;
        },
        loadImage: function (path) {
            if (arguments.length < 1 || typeof path !== 'string') {
                throw new TypeError('loadImage requires a path string');
            }
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_load_image'](p);
            bridge['_efx_bridge_mem_free'](p);
            if (!id) {
                throw new Error('image decode failed');
            }
            return new EfxImageData(id);
        },
        createTexture: function (imageData, opts) {
            if (arguments.length < 1) {
                throw new TypeError('createTexture requires an ImageData');
            }
            var d = liveImageData(imageData);
            var wrap = 0, filter = 1, mipmaps = 0;
            if (arguments.length >= 2 && opts !== undefined && opts !== null) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('createTexture options must be an object');
                }
                var known = { wrap: 1, filter: 1, mipmaps: 1 };
                var names = Object.getOwnPropertyNames(opts);
                for (var i = 0; i < names.length; i++) {
                    if (!known[names[i]]) {
                        throw new TypeError("unknown createTexture option '" + names[i] + "'");
                    }
                }
                if (opts['wrap'] !== undefined) {
                    var w = opts['wrap'];
                    if (w === 'repeat') {
                        wrap = 0;
                    } else if (w === 'clamp') {
                        wrap = 1;
                    } else if (w === 'mirror') {
                        wrap = 2;
                    } else {
                        throw new TypeError('unknown wrap mode');
                    }
                }
                if (opts['filter'] !== undefined) {
                    var f = opts['filter'];
                    if (f === 'nearest') {
                        filter = 0;
                    } else if (f === 'linear') {
                        filter = 1;
                    } else {
                        throw new TypeError('unknown filter');
                    }
                }
                if (opts['mipmaps'] !== undefined) {
                    if (typeof opts['mipmaps'] !== 'boolean') {
                        throw new TypeError('mipmaps must be a boolean');
                    }
                    mipmaps = opts['mipmaps'] ? 1 : 0;
                }
            }
            var handle = bridge['_efx_bridge_texture_create'](d.__id, wrap, filter,
                                                              mipmaps);
            if (!handle) {
                throw new Error('texture upload failed (no GPU context?)');
            }
            return new EfxTexture(handle, false);
        },
        loadMeshData: function (path, opts) {
            if (arguments.length < 1 || typeof path !== 'string') {
                throw new TypeError('loadMeshData requires a path string');
            }
            var hasMesh = 0, isName = 0, index = 0, namePtr = 0;
            var pathPtr = __efxAllocCStr(path);
            if (opts !== undefined && opts !== null) {
                if (!__efxIsObject(opts)) {
                    bridge['_efx_bridge_mem_free'](pathPtr);
                    throw new TypeError('loadMeshData options must be an object');
                }
                var known = { mesh: 1 };
                var names = Object.getOwnPropertyNames(opts);
                for (var i = 0; i < names.length; i++) {
                    if (!known[names[i]]) {
                        bridge['_efx_bridge_mem_free'](pathPtr);
                        throw new TypeError("unknown loadMeshData option '" + names[i] + "'");
                    }
                }
                if (opts['mesh'] !== undefined) {
                    var mv = opts['mesh'];
                    hasMesh = 1;
                    if (typeof mv === 'string') {
                        isName = 1;
                        namePtr = __efxAllocCStr(mv);
                    } else if (typeof mv === 'number' && isFinite(mv) &&
                               mv === Math.floor(mv) && mv >= 0) {
                        index = mv | 0;
                    } else {
                        bridge['_efx_bridge_mem_free'](pathPtr);
                        throw new TypeError('mesh must be a non-negative integer or a name');
                    }
                }
            }
            var id = bridge['_efx_bridge_load_meshdata'](pathPtr, hasMesh, isName,
                                                         index, namePtr);
            bridge['_efx_bridge_mem_free'](pathPtr);
            if (namePtr) {
                bridge['_efx_bridge_mem_free'](namePtr);
            }
            if (!id) {
                throw new Error('glTF import failed');
            }
            return new EfxMeshData(id);
        },
        createRenderTarget: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('createRenderTarget requires an options object');
            }
            var known = { width: 1, height: 1 };
            var names = Object.getOwnPropertyNames(opts);
            for (var i = 0; i < names.length; i++) {
                if (!known[names[i]]) {
                    throw new TypeError("unknown createRenderTarget option '" +
                        names[i] + "'");
                }
            }
            var dims = [];
            for (var k = 0; k < 2; k++) {
                var key = k === 0 ? 'width' : 'height';
                var v = opts[key];
                if (v === undefined) {
                    throw new TypeError('createRenderTarget requires width and height');
                }
                if (typeof v !== 'number' || !isFinite(v) || v <= 0 ||
                    (v | 0) !== v || v > 4096) {
                    throw new RangeError('width and height must be integers in 1..4096');
                }
                dims.push(v | 0);
            }
            var handle = bridge['_efx_bridge_target_create'](dims[0], dims[1]);
            if (!handle) {
                throw new Error('render target creation failed (no GPU context?)');
            }
            return new EfxRenderTarget(handle);
        },
        beginRenderTarget: function (rt) {
            if (arguments.length < 1) {
                throw new TypeError('beginRenderTarget requires a RenderTarget');
            }
            if (!(rt instanceof EfxRenderTarget)) {
                throw new TypeError('expected a RenderTarget');
            }
            if (!rt.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            var rc = bridge['_efx_bridge_target_begin'](rt.__handle);
            if (rc === 7) {
                throw new TypeError('a render target is already active');
            }
            if (rc === 1) {
                throw new RangeError('display list budget exceeded');
            }
            if (rc !== 0) {
                throw new Error('beginRenderTarget failed');
            }
        },
        endRenderTarget: function () {
            var rc = bridge['_efx_bridge_target_end']();
            if (rc === 8) {
                throw new TypeError('no render target is active');
            }
            if (rc === 1) {
                throw new RangeError('display list budget exceeded');
            }
            if (rc !== 0) {
                throw new Error('endRenderTarget failed');
            }
        },
        setPostEffects: function (list) {
            if (arguments.length < 1) {
                throw new TypeError('setPostEffects requires an array or null');
            }
            if (list === null || list === undefined) {
                bridge['_efx_bridge_set_post_effects'](0, 0);
                return;
            }
            if (!Array.isArray(list)) {
                throw new TypeError('setPostEffects requires an array or null');
            }
            if (list.length > 8) {
                throw new RangeError('post-effect chain is limited to 8 entries');
            }
            var wire = new Float32Array(list.length * 9);
            for (var i = 0; i < list.length; i++) {
                var e = __efxPostEntry(list[i]);
                wire.set(e, i * 9);
            }
            var ptr = wire.length ? mallocCopyF32(wire) : 0;
            var rc = bridge['_efx_bridge_set_post_effects'](ptr, list.length);
            if (ptr) {
                bridge['_efx_bridge_mem_free'](ptr);
            }
            if (rc === 1) {
                throw new TypeError('unknown post effect');
            }
            if (rc === 2) {
                throw new RangeError('post-effect chain is limited to 8 entries');
            }
            if (rc === 3) {
                throw new RangeError('post-effect option out of range');
            }
            if (rc !== 0) {
                throw new Error('setPostEffects failed');
            }
        },
        setRenderScale: function (scale, opts) {
            if (arguments.length < 1) {
                throw new TypeError('setRenderScale requires a scale number');
            }
            if (typeof scale !== 'number') {
                throw new TypeError('scale must be a number');
            }
            if (!isFinite(scale) || scale <= 0 || scale > 2) {
                throw new RangeError('scale must be in (0, 2]');
            }
            var filter = 1;
            if (opts !== undefined && opts !== null) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('setRenderScale options must be an object');
                }
                var kn = { filter: 1 };
                var names = Object.getOwnPropertyNames(opts);
                for (var i = 0; i < names.length; i++) {
                    if (!kn[names[i]]) {
                        throw new TypeError("unknown setRenderScale option '" + names[i] + "'");
                    }
                }
                if (opts['filter'] !== undefined) {
                    if (opts['filter'] === 'nearest') {
                        filter = 0;
                    } else if (opts['filter'] === 'linear') {
                        filter = 1;
                    } else {
                        throw new TypeError('unknown filter');
                    }
                }
            }
            var rc = bridge['_efx_bridge_set_render_scale'](scale, filter);
            if (rc === 3) {
                throw new RangeError('scale must be in (0, 2]');
            }
            if (rc === 4) {
                throw new TypeError('unknown filter');
            }
            if (rc !== 0) {
                throw new Error('setRenderScale failed');
            }
        },
        drawQuad: function (x, y, texture, opts) {
            if (arguments.length < 3) {
                throw new TypeError('drawQuad requires (x, y, texture, opts?)');
            }
            var fx = __efxNumber(x, 'x and y must be numbers');
            var fy = __efxNumber(y, 'x and y must be numbers');
            if (!isFinite(fx) || !isFinite(fy)) {
                throw new RangeError('x and y must be finite');
            }
            var tex = liveSample(texture); /* Texture or RenderTarget (F5a) */
            var color = [1, 1, 1, 1];
            var rotation = 0, scale = 1;
            var src = [0, 0, 0, 0];
            var hasSrc = false;
            var size = [0, 0];
            var hasSize = false;
            var origin = [0, 0];
            var hasOrigin = false;
            if (arguments.length >= 4 && opts !== undefined) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('opts must be an object');
                }
                var known = { color: 1, rotation: 1, scale: 1, sourceRect: 1, size: 1, origin: 1 };
                var names = Object.getOwnPropertyNames(opts);
                for (var i = 0; i < names.length; i++) {
                    if (!known[names[i]]) {
                        throw new TypeError('unknown drawQuad option');
                    }
                }
                var cv = opts['color'];
                if (cv !== undefined) {
                    color = __efxFloatArray(cv, 4);
                }
                var rv = opts['rotation'];
                if (rv !== undefined) {
                    rotation = __efxFinite(rv, 'rotation must be a finite number');
                }
                var sv = opts['scale'];
                if (sv !== undefined) {
                    scale = __efxFinite(sv, 'scale must be a finite number');
                    if (scale <= 0) {
                        throw new RangeError('scale must be > 0');
                    }
                }
                var zv = opts['size'];
                if (zv !== undefined) {
                    size = __efxFloatArray(zv, 2);
                    if (size[0] <= 0 || size[1] <= 0) {
                        throw new RangeError('size entries must be > 0');
                    }
                    hasSize = true;
                }
                var ov = opts['origin'];
                if (ov !== undefined) {
                    origin = __efxFloatArray(ov, 2);
                    hasOrigin = true;
                }
                var srcv = opts['sourceRect'];
                if (srcv !== undefined) {
                    if (!__efxIsObject(srcv)) {
                        throw new TypeError('sourceRect must be an object');
                    }
                    var skeys = ['x', 'y', 'w', 'h'];
                    for (var j = 0; j < 4; j++) {
                        src[j] = __efxFinite(srcv[skeys[j]], 'sourceRect fields must be finite numbers');
                    }
                    if (src[2] <= 0 || src[3] <= 0) {
                        throw new RangeError('sourceRect extent must be > 0');
                    }
                    if (src[0] < 0 || src[1] < 0 ||
                        src[0] + src[2] > tex.w || src[1] + src[3] > tex.h) {
                        throw new RangeError('sourceRect outside texture bounds');
                    }
                    hasSrc = true;
                }
            }
            var fw, fh;
            if (hasSize) {
                fw = size[0];
                fh = size[1];
            } else if (hasSrc) {
                fw = src[2];
                fh = src[3];
            } else {
                fw = tex.w;
                fh = tex.h;
            }
            var ox = hasOrigin ? origin[0] : fw * 0.5;
            var oy = hasOrigin ? origin[1] : fh * 0.5;
            var rc = bridge['_efx_bridge_draw_quad'](tex.handle, fx, fy, fw, fh,
                color[0], color[1], color[2], color[3], rotation, scale,
                src[0], src[1], src[2], src[3], hasSrc ? 1 : 0, ox, oy);
            if (rc === 1) {
                throw new RangeError('display list budget exceeded');
            }
            if (rc === 4) {
                throw new Error('no render surface (draw calls need a window)');
            }
            if (rc === 9) {
                throw new TypeError('cannot sample the render target being drawn into');
            }
            if (rc !== 0) {
                throw new Error('drawQuad failed');
            }
        },
        setBlendMode: function (mode) {
            if (arguments.length < 1) {
                throw new TypeError('setBlendMode requires a mode string');
            }
            var s = __efxCStr(mode);
            if (s === null) {
                throw new TypeError('setBlendMode requires a mode string');
            }
            var m;
            if (s === 'alpha') {
                m = 0;
            } else if (s === 'additive') {
                m = 1;
            } else if (s === 'subtractive') {
                m = 2;
            } else {
                throw new TypeError('unknown blend mode');
            }
            bridge['_efx_bridge_set_blend'](m);
        },
        setCamera3D: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('setCamera3D requires an options object');
            }
            var known = { pos: 1, target: 1, fov: 1, near: 1, far: 1 };
            var names = Object.getOwnPropertyNames(opts);
            for (var i = 0; i < names.length; i++) {
                if (!known[names[i]]) {
                    throw new TypeError("unknown setCamera3D option '" + names[i] + "'");
                }
            }
            var pos = opts['pos'];
            var target = opts['target'];
            if (pos === undefined || target === undefined) {
                throw new TypeError('setCamera3D requires pos and target');
            }
            var p = __efxFloat32Array(pos, 'pos');
            var t = __efxFloat32Array(target, 'target');
            if (p.length !== 3 || t.length !== 3) {
                throw new RangeError('pos and target must hold 3 numbers');
            }
            var fov = opts['fov'];
            if (fov === undefined) {
                throw new TypeError('setCamera3D requires fov');
            }
            if (typeof fov !== 'number') {
                throw new TypeError('fov must be a number');
            }
            if (!isFinite(fov)) {
                throw new RangeError('fov must be finite');
            }
            var nearZ = 0.1, farZ = 100;
            var nv = opts['near'];
            if (nv !== undefined) {
                if (typeof nv !== 'number') {
                    throw new TypeError('near and far must be numbers');
                }
                if (!isFinite(nv)) {
                    throw new RangeError('near and far must be finite');
                }
                nearZ = nv;
            }
            var fv = opts['far'];
            if (fv !== undefined) {
                if (typeof fv !== 'number') {
                    throw new TypeError('near and far must be numbers');
                }
                if (!isFinite(fv)) {
                    throw new RangeError('near and far must be finite');
                }
                farZ = fv;
            }
            bridge['_efx_bridge_set_camera3d'](p[0], p[1], p[2],
                t[0], t[1], t[2], fov, nearZ, farZ);
        },
        createMeshData: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('createMeshData requires an options object');
            }
            var bagKnown = { surfaces: 1, positions: 1, normals: 1,
                uvs: 1, colors: 1, joints: 1, weights: 1, indices: 1,
                materials: 1 };
            var bagNames = Object.getOwnPropertyNames(opts);
            for (var bi = 0; bi < bagNames.length; bi++) {
                if (!bagKnown[bagNames[bi]]) {
                    throw new TypeError("unknown createMeshData option '" + bagNames[bi] + "'");
                }
            }
            var surfaces = opts['surfaces'];
            var shorthand = opts['positions'] !== undefined;
            if (surfaces !== undefined && shorthand) {
                throw new TypeError('pass either surfaces or single-surface fields');
            }
            if (surfaces === undefined && !shorthand) {
                throw new TypeError('createMeshData requires surfaces');
            }
            var list;
            if (surfaces !== undefined) {
                if (!Array.isArray(surfaces)) {
                    throw new TypeError('surfaces must be an array');
                }
                if (surfaces.length < 1 || surfaces.length > 16) {
                    throw new RangeError('surfaces must hold 1..16 entries');
                }
                list = surfaces;
            } else {
                list = [opts];
            }
            var surfKnown = { positions: 1, normals: 1, uvs: 1,
                colors: 1, joints: 1, weights: 1, indices: 1 };
            /* the shorthand form passes the whole bag as the surface, so the
               bag-level materials field is allowed there (desktop parity) */
            if (surfaces === undefined) {
                surfKnown.materials = 1;
            }
            var id = bridge['_efx_bridge_meshdata_create'](list.length);
            if (!id) {
                throw new Error('out of memory');
            }
            for (var i = 0; i < list.length; i++) {
                var sv = list[i];
                var bad = null;
                if (!__efxIsObject(sv)) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new TypeError('surfaces must be objects');
                }
                var names = Object.getOwnPropertyNames(sv);
                for (var k = 0; k < names.length; k++) {
                    if (!surfKnown[names[k]]) {
                        bad = "unknown surface option '" + names[k] + "'";
                    }
                }
                if (bad !== null) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new TypeError(bad);
                }
                if (sv['positions'] === undefined) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new TypeError('surface requires positions');
                }
                var pos = __efxFloat32Array(sv['positions'], 'positions');
                var nrm = sv['normals'] !== undefined
                    ? __efxFloat32Array(sv['normals'], 'normals') : new Float32Array(0);
                var uvs = sv['uvs'] !== undefined
                    ? __efxFloat32Array(sv['uvs'], 'uvs') : new Float32Array(0);
                var cols = sv['colors'] !== undefined
                    ? __efxFloat32Array(sv['colors'], 'colors') : new Float32Array(0);
                var joints = sv['joints'] !== undefined
                    ? __efxUint32Array(sv['joints'], 'joints') : new Uint32Array(0);
                var weights = sv['weights'] !== undefined
                    ? __efxFloat32Array(sv['weights'], 'weights') : new Float32Array(0);
                var idx = sv['indices'] !== undefined
                    ? __efxUint32Array(sv['indices']) : new Uint32Array(0);
                var pPtr = mallocCopyF32(pos);
                var nPtr = mallocCopyF32(nrm);
                var uPtr = mallocCopyF32(uvs);
                var cPtr = mallocCopyF32(cols);
                var jPtr = mallocCopyU32(joints);
                var wPtr = mallocCopyF32(weights);
                var iPtr = mallocCopyU32(idx);
                var rc = bridge['_efx_bridge_meshdata_surface'](id, i,
                    pPtr, pos.length, nPtr, nrm.length, uPtr, uvs.length,
                    cPtr, cols.length, jPtr, joints.length, wPtr, weights.length,
                    iPtr, idx.length);
                bridge['_efx_bridge_mem_free'](pPtr);
                bridge['_efx_bridge_mem_free'](nPtr);
                bridge['_efx_bridge_mem_free'](uPtr);
                bridge['_efx_bridge_mem_free'](cPtr);
                bridge['_efx_bridge_mem_free'](jPtr);
                bridge['_efx_bridge_mem_free'](wPtr);
                bridge['_efx_bridge_mem_free'](iPtr);
                if (rc !== 0) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new RangeError('invalid mesh data');
                }
            }
            rc = bridge['_efx_bridge_meshdata_commit'](id);
            if (rc !== 0) {
                bridge['_efx_bridge_meshdata_destroy'](id);
                throw new RangeError('invalid mesh data');
            }
            var materials = opts['materials'];
            if (materials !== undefined) {
                if (!Array.isArray(materials)) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new TypeError('materials must be an array');
                }
                if (materials.length !== list.length) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new RangeError('materials must have one entry per surface');
                }
                for (var mi = 0; mi < materials.length; mi++) {
                    var mv = materials[mi];
                    if (mv === null || mv === undefined) {
                        continue;
                    }
                    var mf = __efxMaterial(mv);
                    var mptr = mallocCopyF32(mf.blocks);
                    var mapsptr = mallocCopyF64(mf.maps);
                    bridge['_efx_bridge_meshdata_set_material'](id, mi, mptr,
                                                                mapsptr, 1);
                    bridge['_efx_bridge_mem_free'](mptr);
                    bridge['_efx_bridge_mem_free'](mapsptr);
                }
            }
            return new EfxMeshData(id);
        },
        createMesh: function (meshData) {
            if (arguments.length < 1) {
                throw new TypeError('createMesh requires a MeshData');
            }
            var md = liveMeshData(meshData);
            var handle = bridge['_efx_bridge_mesh_create'](md.__id);
            if (!handle) {
                throw new Error('mesh upload failed (no GPU context?)');
            }
            return new EfxMesh(handle);
        },
        drawMesh: function (mesh, opts) {
            if (arguments.length < 1) {
                throw new TypeError('drawMesh requires a mesh');
            }
            var m = liveMesh(mesh);
            var transform = null, color = null, skinned = 0;
            if (arguments.length >= 2 && opts !== undefined) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('drawMesh options must be an object');
                }
                var known = { transform: 1, color: 1, skinned: 1 };
                var names = Object.getOwnPropertyNames(opts);
                for (var i = 0; i < names.length; i++) {
                    if (!known[names[i]]) {
                        throw new TypeError("unknown drawMesh option '" + names[i] + "'");
                    }
                }
                var tv = opts['transform'];
                if (tv !== undefined) {
                    transform = __efxFloat32Array(tv, 'transform');
                    if (transform.length !== 16) {
                        throw new RangeError('transform must hold 16 numbers');
                    }
                }
                var cv = opts['color'];
                if (cv !== undefined) {
                    color = __efxFloat32Array(cv, 'color');
                    if (color.length !== 4) {
                        throw new RangeError('color must hold 4 numbers');
                    }
                }
                var sv = opts['skinned'];
                if (sv !== undefined) {
                    if (typeof sv !== 'boolean') {
                        throw new TypeError('skinned must be a boolean');
                    }
                    skinned = sv ? 1 : 0;
                }
            }
            var tPtr = 0, cPtr = 0;
            if (transform !== null || color !== null) {
                var base = drawScratchPtr();
                if (transform !== null) {
                    tPtr = base;
                    HEAPF32.set(transform, tPtr >> 2);
                }
                if (color !== null) {
                    cPtr = base + 16 * 4;
                    HEAPF32.set(color, cPtr >> 2);
                }
            }
            var rc = bridge['_efx_bridge_draw_mesh'](m.__handle, tPtr, cPtr, skinned);
            if (rc === 1) {
                throw new RangeError('display list budget exceeded');
            }
            if (rc === 2) {
                throw new TypeError('expected a live Mesh');
            }
            if (rc === 11) {
                throw new TypeError('mesh has no rig to draw skinned');
            }
            if (rc === 9) {
                throw new TypeError('cannot sample the render target being drawn into');
            }
            if (rc !== 0) {
                throw new Error('drawMesh failed');
            }
        },
        poseMesh: function (mesh, pose) {
            if (arguments.length < 2) {
                throw new TypeError('poseMesh requires (mesh, pose)');
            }
            var m = liveMesh(mesh);
            var list;
            if (Array.isArray(pose)) {
                list = pose;
            } else if (__efxIsObject(pose)) {
                list = [pose];
            } else {
                throw new TypeError('pose must be a sample or an array of samples');
            }
            var wire = new Float32Array(list.length * 3);
            for (var i = 0; i < list.length; i++) {
                var s = list[i];
                if (!__efxIsObject(s)) {
                    throw new TypeError('pose samples must be objects');
                }
                var sk = { clip: 1, time: 1, weight: 1 };
                var sn = Object.getOwnPropertyNames(s);
                for (var k = 0; k < sn.length; k++) {
                    if (!sk[sn[k]]) {
                        throw new TypeError("unknown pose sample option '" + sn[k] + "'");
                    }
                }
                var cv = s['clip'];
                if (cv === undefined) {
                    throw new TypeError('pose sample requires clip');
                }
                var clipIndex;
                if (typeof cv === 'string') {
                    var namePtr = __efxAllocCStr(cv);
                    clipIndex = bridge['_efx_bridge_find_clip'](m.__handle, namePtr);
                    bridge['_efx_bridge_mem_free'](namePtr);
                    if (clipIndex < 0) {
                        throw new Error('unknown clip name');
                    }
                } else if (typeof cv === 'number' && isFinite(cv) &&
                           cv === Math.floor(cv) && cv >= 0) {
                    clipIndex = cv | 0;
                } else {
                    throw new TypeError('clip must be a name or index');
                }
                var tv = s['time'];
                if (typeof tv !== 'number') {
                    throw new TypeError('pose sample requires a numeric time');
                }
                if (!isFinite(tv)) {
                    throw new RangeError('time must be finite');
                }
                var wv = 1;
                if (s['weight'] !== undefined) {
                    if (typeof s['weight'] !== 'number') {
                        throw new TypeError('weight must be a number');
                    }
                    if (!isFinite(s['weight']) || s['weight'] < 0) {
                        throw new RangeError('weight must be finite and >= 0');
                    }
                    wv = s['weight'];
                }
                wire[i * 3] = clipIndex;
                wire[i * 3 + 1] = tv;
                wire[i * 3 + 2] = wv;
            }
            var ptr = list.length ? mallocCopyF32(wire) : 0;
            var rc = bridge['_efx_bridge_pose_mesh'](m.__handle, ptr, list.length);
            if (ptr) {
                bridge['_efx_bridge_mem_free'](ptr);
            }
            if (rc === 2) {
                throw new TypeError('poseMesh requires a Mesh with a rig');
            }
            if (rc === 6) {
                throw new RangeError('clip index out of range');
            }
            if (rc !== 0) {
                throw new Error('poseMesh failed');
            }
        },
        setLight: function (slot, opts) {
            if (arguments.length < 2) {
                throw new TypeError('setLight requires (slot, opts)');
            }
            if (typeof slot !== 'number' || (slot | 0) !== slot) {
                throw new RangeError('light slot must be an integer 0..3');
            }
            if (slot < 0 || slot > 3) {
                throw new RangeError('light slot out of range (0..3)');
            }
            if (opts === null || opts === undefined) {
                bridge['_efx_bridge_set_point_light'](slot, 0, 0, 0, 0,
                    0, 0, 0, 0, 0);
                return;
            }
            if (!__efxIsObject(opts)) {
                throw new TypeError('setLight options must be an object or null');
            }
            var lk = { pos: 1, color: 1, range: 1 };
            var lnames = Object.getOwnPropertyNames(opts);
            for (var li = 0; li < lnames.length; li++) {
                if (!lk[lnames[li]]) {
                    throw new TypeError("unknown setLight option '" + lnames[li] + "'");
                }
            }
            if (opts['pos'] === undefined) {
                throw new TypeError('setLight requires pos');
            }
            var pv = __efxFloat32Array(opts['pos'], 'pos');
            if (pv.length !== 3) {
                throw new RangeError('pos must hold 3 numbers');
            }
            if (opts['color'] === undefined) {
                throw new TypeError('setLight requires color');
            }
            var lc = __efxFloatArray(opts['color'], 4);
            var range = 0;
            if (opts['range'] !== undefined) {
                if (typeof opts['range'] !== 'number') {
                    throw new TypeError('range must be a number');
                }
                if (!isFinite(opts['range']) || opts['range'] < 0) {
                    throw new RangeError('range must be a finite number >= 0');
                }
                range = opts['range'];
            }
            bridge['_efx_bridge_set_point_light'](slot, 1,
                pv[0], pv[1], pv[2], lc[0], lc[1], lc[2], lc[3], range);
        },
        setDirectionalLight: function (opts) {
            if (arguments.length < 1) {
                throw new TypeError('setDirectionalLight requires an options object or null');
            }
            if (opts === null || opts === undefined) {
                bridge['_efx_bridge_set_directional_light'](0, 0, 0, 0, 0, 0, 0, 0);
                return;
            }
            if (!__efxIsObject(opts)) {
                throw new TypeError('setDirectionalLight options must be an object or null');
            }
            var dk = { dir: 1, color: 1 };
            var dnames = Object.getOwnPropertyNames(opts);
            for (var di = 0; di < dnames.length; di++) {
                if (!dk[dnames[di]]) {
                    throw new TypeError("unknown setDirectionalLight option '" + dnames[di] + "'");
                }
            }
            if (opts['dir'] === undefined) {
                throw new TypeError('setDirectionalLight requires dir');
            }
            var dv = __efxFloat32Array(opts['dir'], 'dir');
            if (dv.length !== 3) {
                throw new RangeError('dir must hold 3 numbers');
            }
            if (dv[0] === 0 && dv[1] === 0 && dv[2] === 0) {
                throw new TypeError('dir must be non-zero');
            }
            if (opts['color'] === undefined) {
                throw new TypeError('setDirectionalLight requires color');
            }
            var dc = __efxFloatArray(opts['color'], 4);
            bridge['_efx_bridge_set_directional_light'](1,
                dv[0], dv[1], dv[2], dc[0], dc[1], dc[2], dc[3]);
        },
        setMeshSurfaceMaterial: function (mesh, index, mat) {
            if (arguments.length < 3) {
                throw new TypeError(
                    'setMeshSurfaceMaterial requires (mesh, surfaceIndex, mat)');
            }
            var m = liveMesh(mesh);
            if (typeof index !== 'number' || (index | 0) !== index) {
                throw new RangeError('surfaceIndex must be an integer');
            }
            var count = bridge['_efx_bridge_mesh_surface_count'](m.__handle);
            if (index < 0 || index >= count) {
                throw new RangeError('surfaceIndex out of range');
            }
            if (mat === null || mat === undefined) {
                bridge['_efx_bridge_mesh_set_material'](m.__handle, index, 0, 0, 0);
                return;
            }
            var f = __efxMaterial(mat);
            var ptr = mallocCopyF32(f.blocks);
            var mapsptr = mallocCopyF64(f.maps);
            bridge['_efx_bridge_mesh_set_material'](m.__handle, index, ptr,
                                                    mapsptr, 1);
            bridge['_efx_bridge_mem_free'](ptr);
            bridge['_efx_bridge_mem_free'](mapsptr);
        },
    };

    var whiteTex = null;
    Object.defineProperty(api, 'whiteTexture', {
        get: function () {
            if (!whiteTex) {
                var handle = bridge['_efx_bridge_white_texture']();
                if (!handle) {
                    throw new Error('white texture unavailable');
                }
                whiteTex = new EfxTexture(handle, true);
            }
            return whiteTex;
        },
    });

    /* engine-bundled pure-JS layer (F3 math + primitives): the same
       embedded source the desktop quickjs runtime evaluates (ADR 0022) */
    var preludeSrc = UTF8ToString(bridge['_efx_bridge_js_prelude']());
    new Function('efx', preludeSrc)(api);

    globalThis['efx'] = api;
    st.api = api;
    st.quitSentinel = new Object();
    return st;
}

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
        Module['_efx_bridge_mem_free'](p);
        if (!ok) {
            throw new Error('asset archive could not be opened');
        }
        __efxEvaluateEntry();
    }).catch(function (e) {
        __efxFail('player: asset root fetch failed: ' +
            (e && e.message ? e.message : e));
    });
}

function __efxEvaluateEntry() {
    var st = __efxState();
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
        Module['_efx_bridge_mem_free'](mp);
        if (!mptr) {
            __efxFail('player: no main.js in resource root: ' + root);
            return;
        }
        code = UTF8ToString(mptr);
        Module['_efx_bridge_mem_free'](mptr);
    }
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
    var paramNames = ['efx'].concat(hostGlobals).concat(['globalThis']);
    var epilogue = ';return { u: typeof update === "function" ? update : null,'
        + ' r: typeof render === "function" ? render : null };';
    var hooks;
    try {
        var factory = new Function(paramNames.join(','), code + epilogue);
        var callArgs = [st.api];
        for (var i = 0; i < hostGlobals.length; i++) {
            callArgs.push(undefined);
        }
        callArgs.push(shadowGlobal);
        hooks = factory.apply(null, callArgs);
    } catch (e) {
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
        return;
    }
    if (hooks && typeof hooks.u === 'function') {
        st.updateHooks.push({ fn: hooks.u, active: true });
    }
    if (hooks && typeof hooks.r === 'function') {
        st.renderHooks.push({ fn: hooks.r, active: true });
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
