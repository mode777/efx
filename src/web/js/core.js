/* eslint-disable */
// Native-JS runtime glue (f2b, ADR 0022): loads <root>/main.js with the
// host JS engine, exposes the same `efx` namespace contract as the desktop
// quickjs binding, and mirrors the desktop error/exit-code contract.
// Only hoisted function declarations live at top level: on synchronous
// (Node) builds the postRun boot fires before this file's statements run.

function __efxState() {
    var st = globalThis['__efx_state'];
    if (!st) {
        st = { api: null, quitSentinel: null, updateHooks: [], renderHooks: [], started: false,
               keyboardDown: [], keyboardUp: [], keyboardChar: [],
               mouseDown: [], mouseUp: [], mouseMove: [], mouseWheel: [],
               gamepadConnect: [], gamepadDisconnect: [] };
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

function __efxFiniteNumber(v, what) {
    if (typeof v !== 'number' || !isFinite(v)) {
        throw new TypeError(what + ' must be a finite number');
    }
    return v;
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

/* throw for a non-zero bridge return code. `codes` maps each code the call
   site handles to [ErrorClass, message], or to `true` for the shared render
   message; any other code throws Error('<where> failed') */
function __efxRc(rc, where, codes) {
    if (rc === 0) {
        return;
    }
    var e = codes[rc];
    if (e === true) {
        e = {
            1: [RangeError, 'display list budget exceeded'],
            4: [Error, 'no render surface (draw calls need a window)'],
            9: [TypeError, 'cannot sample the render target being drawn into'],
        }[rc];
    }
    if (!e) {
        e = [Error, where + ' failed'];
    }
    throw new e[0](e[1]);
}

/* validate a sourceRect against a sample source's size -> [x, y, w, h] */
function __efxSourceRect(tex, v) {
    if (!__efxIsObject(v)) {
        throw new TypeError('sourceRect must be an object');
    }
    var keys = ['x', 'y', 'w', 'h'];
    var src = [0, 0, 0, 0];
    for (var j = 0; j < 4; j++) {
        src[j] = __efxFinite(v[keys[j]], 'sourceRect fields must be finite numbers');
    }
    if (src[2] <= 0 || src[3] <= 0) {
        throw new RangeError('sourceRect extent must be > 0');
    }
    if (src[0] < 0 || src[1] < 0 ||
        src[0] + src[2] > tex.w || src[1] + src[3] > tex.h) {
        throw new RangeError('sourceRect outside texture bounds');
    }
    return src;
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

function __efxMods(mask) {
    var out = [];
    if (mask & 1) { out.push('shift'); }
    if (mask & 2) { out.push('ctrl'); }
    if (mask & 4) { out.push('alt'); }
    if (mask & 8) { out.push('super'); }
    return out;
}

/* invoke an input callback list with one plain event object; mirrors the
   lifecycle dispatch's error/quit contract (0 ok, 1 quit, 2 error) */
function __efxCallHooks(st, hooks, ev) {
    for (var i = 0; i < hooks.length; i++) {
        var entry = hooks[i];
        if (!entry.active) {
            continue;
        }
        try {
            entry.fn(ev);
        } catch (e) {
            if (st.quitSentinel !== null && e === st.quitSentinel) {
                return 1;
            }
            Module['_efx_bridge_set_error']();
            __efxReportError(e);
            __efxSyncExit();
            return 2;
        }
    }
    return 0;
}

/* F13: the gamepad pad view — a plain JS object whose methods call the C core
   (one source shared with the desktop binding) */
function __efxGamepadView(slot) {
    var bridge = Module;
    function buttonId(name) {
        if (typeof name !== 'string') {
            throw new TypeError('gamepad button query requires a button name');
        }
        var p = __efxAllocCStr(name);
        var id = bridge['_efx_input_gamepad_button_id'](p);
        bridge['_free'](p);
        if (id < 0) {
            throw new TypeError('unknown gamepad button');
        }
        return id;
    }
    function axisId(name) {
        if (typeof name !== 'string') {
            throw new TypeError('gamepad axis query requires an axis name');
        }
        var p = __efxAllocCStr(name);
        var id = bridge['_efx_input_gamepad_axis_id'](p);
        bridge['_free'](p);
        if (id < 0) {
            throw new TypeError('unknown gamepad axis');
        }
        return id;
    }
    return {
        index: slot,
        connected: !!bridge['_efx_input_gamepad_connected'](slot),
        name: UTF8ToString(bridge['_efx_bridge_gamepad_name'](slot)),
        mapped: !!bridge['_efx_input_gamepad_mapped'](slot),
        isDown: function (b) {
            return !!bridge['_efx_input_gamepad_button_is_down'](slot, buttonId(b));
        },
        isPressed: function (b) {
            return !!bridge['_efx_input_gamepad_button_is_pressed'](slot, buttonId(b));
        },
        isReleased: function (b) {
            return !!bridge['_efx_input_gamepad_button_is_released'](slot, buttonId(b));
        },
        axis: function (a) {
            return bridge['_efx_input_gamepad_axis'](slot, axisId(a));
        },
        rawButton: function (i) {
            if (typeof i !== 'number') {
                throw new TypeError('gamepad raw query requires an index');
            }
            return bridge['_efx_input_gamepad_raw_button'](slot, i | 0);
        },
        rawAxis: function (i) {
            if (typeof i !== 'number') {
                throw new TypeError('gamepad raw query requires an index');
            }
            return bridge['_efx_input_gamepad_raw_axis'](slot, i | 0);
        },
    };
}

/* one persistent 80-byte wasm scratch: per-draw uniforms (16 + 4 floats) and
   the batched input reads (10 doubles) */
function __efxScratch() {
    var st = __efxState();
    if (!st.scratch) {
        st.scratch = Module['_malloc'](80);
    }
    return st.scratch;
}

/* F9: drain the frame's staged input events into the registered callbacks,
   in arrival order, before the update hooks run (ADR 0036) */
function __efxDispatchInput(st) {
    var bridge = Module;
    var n = bridge['_efx_input_event_count']();
    var buf = __efxScratch();
    for (var i = 0; i < n; i++) {
        /* type, key, button, repeat, mods, codepoint, x, y, dx, dy */
        bridge['_efx_bridge_input_event'](i, buf);
        var e = Array.prototype.slice.call(HEAPF64, buf >> 3, (buf >> 3) + 10);
        var type = e[0];
        var hooks = null;
        var ev = null;
        if (type === 0) {
            hooks = st.keyboardDown;
            ev = {
                key: UTF8ToString(bridge['_efx_input_key_name'](e[1])),
                repeat: !!e[3],
                mods: __efxMods(e[4]),
            };
        } else if (type === 1) {
            hooks = st.keyboardUp;
            ev = {
                key: UTF8ToString(bridge['_efx_input_key_name'](e[1])),
                mods: __efxMods(e[4]),
            };
        } else if (type === 2) {
            hooks = st.keyboardChar;
            ev = { char: String.fromCodePoint(e[5]) };
        } else if (type === 3 || type === 4) {
            hooks = type === 3 ? st.mouseDown : st.mouseUp;
            ev = {
                button: UTF8ToString(bridge['_efx_input_button_name'](e[2])),
                x: e[6],
                y: e[7],
                mods: __efxMods(e[4]),
            };
        } else if (type === 5) {
            hooks = st.mouseMove;
            ev = { x: e[6], y: e[7], dx: e[8], dy: e[9] };
        } else if (type === 6) {
            hooks = st.mouseWheel;
            ev = { dx: e[8], dy: e[9] };
        }
        if (hooks === null) {
            continue;
        }
        var rc = __efxCallHooks(st, hooks, ev);
        if (rc !== 0) {
            bridge['_efx_input_clear_events']();
            return rc;
        }
    }
    bridge['_efx_input_clear_events']();

    /* F13: gamepad connect/disconnect callbacks fire with the pad view */
    var cn = bridge['_efx_input_gamepad_connect_count']();
    for (var c = 0; c < cn; c++) {
        var slot = bridge['_efx_input_gamepad_connect_at'](c);
        var rcc = __efxCallHooks(st, st.gamepadConnect, __efxGamepadView(slot));
        if (rcc !== 0) {
            return rcc;
        }
    }
    var dn = bridge['_efx_input_gamepad_disconnect_count']();
    for (var d = 0; d < dn; d++) {
        var dslot = bridge['_efx_input_gamepad_disconnect_at'](d);
        var rcd = __efxCallHooks(st, st.gamepadDisconnect, __efxGamepadView(dslot));
        if (rcd !== 0) {
            return rcd;
        }
    }
    return 0;
}

function __efxEnsureApi() {
    var st = __efxState();
    if (st.api) {
        return st;
    }
    var bridge = Module;
    /* one known-field check for every option bag: unknown keys throw a
     * TypeError naming the field. `where` names the bag (may be empty),
     * `useKeys` selects Object.keys instead of getOwnPropertyNames, and
     * `noName` reproduces the two legacy messages that omit the field. */
    function __efxCheckKnown(obj, known, where, useKeys, noName) {
        var names = useKeys ? Object.keys(obj) : Object.getOwnPropertyNames(obj);
        for (var i = 0; i < names.length; i++) {
            var k = names[i];
            var ok = (known instanceof Array) ? known.indexOf(k) >= 0 : known[k];
            if (!ok) {
                if (noName) {
                    throw new TypeError('unknown ' + (where ? where + ' option' : 'option'));
                }
                var prefix = where ? where + ' option ' : 'option ';
                throw new TypeError('unknown ' + prefix + "'" + k + "'");
            }
        }
    }

    st.dispatch = function (which, dt) {
        /* F9: input callbacks fire before the update hooks each frame */
        if (which) {
            var irc = __efxDispatchInput(st);
            if (irc !== 0) {
                return irc;
            }
        }
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
    // it is running (desktop parity)
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

    /* one factory for the native-backed resource wrappers. It owns the
     * `__alive` flag, the idempotent `destroy()` guard and the liveness check
     * shared by guarded accessors and methods. `init` seeds the instance
     * fields (run with `__alive` already true); `destroy` is the release body
     * the factory calls once, after `check`; each `getters`/`methods` entry is
     * a function wrapped with the liveness guard, or `{ raw: true, ... }` for
     * the few Audio members that tolerate a dead handle. `typeMsg`/`deadMsg`
     * preserve each class's exact wording. */
    function __efxResourceClass(name, spec) {
        function Ctor() {
            this.__alive = true;
            if (spec.init) {
                spec.init.apply(this, arguments);
            }
        }
        Object.defineProperty(Ctor, 'name', { value: name });
        function live(v) {
            if (!(v instanceof Ctor)) {
                throw new TypeError(spec.typeMsg);
            }
            if (!v.__alive) {
                throw new TypeError(spec.deadMsg || 'using a destroyed resource');
            }
            return v;
        }
        Ctor.__live = live;
        var proto = Ctor.prototype;
        proto.destroy = function () {
            if (!(this instanceof Ctor)) {
                throw new TypeError(spec.notObjectMsg || 'not a resource object');
            }
            if (!this.__alive) {
                return;
            }
            if (spec.check) {
                spec.check.call(this);
            }
            this.__alive = false;
            spec.destroy.call(this);
        };
        var k;
        if (spec.methods) {
            for (k in spec.methods) {
                if (Object.prototype.hasOwnProperty.call(spec.methods, k)) {
                    (function (m) {
                        var d = spec.methods[m];
                        if (d && d.raw) {
                            proto[m] = d.fn;
                        } else {
                            proto[m] = function () {
                                return d.apply(live(this), arguments);
                            };
                        }
                    })(k);
                }
            }
        }
        if (spec.getters) {
            for (k in spec.getters) {
                if (Object.prototype.hasOwnProperty.call(spec.getters, k)) {
                    (function (g) {
                        var d = spec.getters[g];
                        var acc = {};
                        if (d.get) {
                            acc.get = d.raw ? d.get : function () {
                                return d.get.call(live(this));
                            };
                        }
                        if (d.set) {
                            acc.set = d.raw ? d.set : function (v) {
                                return d.set.call(live(this), v);
                            };
                        }
                        Object.defineProperty(proto, g, acc);
                    })(k);
                }
            }
        }
        return Ctor;
    }

    var EfxImageData = __efxResourceClass('EfxImageData', {
        typeMsg: 'expected an ImageData',
        init: function (id) {
            this.__id = id;
        },
        destroy: function () {
            bridge['_efx_bridge_imagedata_destroy'](this.__id);
        },
        getters: {
            width: {
                get: function () {
                    return bridge['_efx_bridge_imagedata_width'](this.__id);
                },
            },
            height: {
                get: function () {
                    return bridge['_efx_bridge_imagedata_height'](this.__id);
                },
            },
        },
    });

    var EfxTexture = __efxResourceClass('EfxTexture', {
        typeMsg: 'expected a Texture',
        init: function (handle, permanent) {
            this.__handle = handle;
            this.__permanent = !!permanent;
        },
        /* engine-owned textures (the white texture) refuse destruction */
        check: function () {
            if (this.__permanent) {
                throw new TypeError('cannot destroy an engine-owned texture');
            }
        },
        destroy: function () {
            bridge['_efx_bridge_texture_destroy'](this.__handle);
        },
        getters: {
            width: {
                get: function () {
                    return bridge['_efx_bridge_texture_width'](this.__handle);
                },
            },
            height: {
                get: function () {
                    return bridge['_efx_bridge_texture_height'](this.__handle);
                },
            },
        },
    });

    /* F5a: a render target is a native-backed class like Texture —
       deterministic destroy, GC-reachable via JS, read-only size */
    var EfxRenderTarget = __efxResourceClass('EfxRenderTarget', {
        typeMsg: 'expected a RenderTarget',
        init: function (handle) {
            this.__handle = handle;
        },
        destroy: function () {
            bridge['_efx_bridge_target_destroy'](this.__handle);
        },
        getters: {
            width: {
                get: function () {
                    return bridge['_efx_bridge_target_width'](this.__handle);
                },
            },
            height: {
                get: function () {
                    return bridge['_efx_bridge_target_height'](this.__handle);
                },
            },
        },
    });

    var EfxMeshData = __efxResourceClass('EfxMeshData', {
        typeMsg: 'expected a MeshData',
        init: function (id) {
            this.__id = id;
        },
        destroy: function () {
            bridge['_efx_bridge_meshdata_destroy'](this.__id);
        },
        getters: {
            surfaceCount: {
                get: function () {
                    return bridge['_efx_bridge_meshdata_surface_count'](this.__id);
                },
            },
        },
    });

    var EfxMesh = __efxResourceClass('EfxMesh', {
        typeMsg: 'expected a Mesh',
        init: function (handle) {
            this.__handle = handle;
        },
        destroy: function () {
            bridge['_efx_bridge_mesh_destroy'](this.__handle);
        },
        getters: {
            surfaceCount: {
                get: function () {
                    return bridge['_efx_bridge_mesh_surface_count'](this.__handle);
                },
            },
        },
    });

    /* F8a: FontData (parsed font) and Font (baked atlas) native-backed
       classes, id-based like ImageData */
    var EfxFontData = __efxResourceClass('EfxFontData', {
        init: function (id) {
            this.__id = id;
        },
        destroy: function () {
            bridge['_efx_bridge_fontdata_destroy'](this.__id);
        },
    });

    var EfxFont = __efxResourceClass('EfxFont', {
        typeMsg: 'expected a Font',
        init: function (id) {
            this.__id = id;
        },
        destroy: function () {
            bridge['_efx_bridge_font_destroy'](this.__id);
        },
        getters: {
            size: {
                get: function () {
                    return bridge['_efx_bridge_font_size'](this.__id);
                },
            },
            lineHeight: {
                get: function () {
                    return bridge['_efx_bridge_font_line_height'](this.__id);
                },
            },
            ascent: {
                get: function () {
                    return bridge['_efx_bridge_font_ascent'](this.__id);
                },
            },
            descent: {
                get: function () {
                    return bridge['_efx_bridge_font_descent'](this.__id);
                },
            },
        },
    });

    function __efxAlign(v) {
        if (v === 'left') { return 0; }
        if (v === 'center') { return 1; }
        if (v === 'right') { return 2; }
        if (v === 'justify') { return 3; }
        throw new TypeError("align must be 'left', 'center', 'right' or 'justify'");
    }
    function __efxValign(v) {
        if (v === 'top') { return 0; }
        if (v === 'middle') { return 1; }
        if (v === 'bottom') { return 2; }
        throw new TypeError("valign must be 'top', 'middle' or 'bottom'");
    }
    function __efxTextLayout(opts) {
        var out = { align: 0, valign: 0, hasWidth: 0, width: 0, hasLh: 0,
                    lh: 0, scale: 1, rotation: 0 };
        if (opts === undefined || opts === null) {
            return out;
        }
        if (!__efxIsObject(opts)) {
            throw new TypeError('text options must be an object');
        }
        var known = { align: 1, valign: 1, width: 1, lineHeight: 1, color: 1,
                      outlineColor: 1, shadowColor: 1, rotation: 1, scale: 1 };
                __efxCheckKnown(opts, known, '');
        if (opts.align !== undefined) { out.align = __efxAlign(opts.align); }
        if (opts.valign !== undefined) { out.valign = __efxValign(opts.valign); }
        if (opts.width !== undefined) {
            var w = __efxFinite(opts.width, 'width must be a finite number');
            if (!(w > 0)) { throw new RangeError('width must be > 0'); }
            out.hasWidth = 1;
            out.width = w;
        }
        if (opts.lineHeight !== undefined) {
            var lh = __efxFinite(opts.lineHeight, 'lineHeight must be a finite number');
            if (!(lh > 0)) { throw new RangeError('lineHeight must be > 0'); }
            out.hasLh = 1;
            out.lh = lh;
        }
        if (opts.rotation !== undefined) {
            out.rotation = __efxFinite(opts.rotation, 'rotation must be a finite number');
        }
        if (opts.scale !== undefined) {
            var s = __efxFinite(opts.scale, 'scale must be a finite number');
            if (!(s > 0)) { throw new RangeError('scale must be > 0'); }
            out.scale = s;
        }
        if (out.align === 3 && !out.hasWidth) {
            throw new TypeError('justify alignment requires a width');
        }
        return out;
    }

    function liveMeshData(v) {
        return EfxMeshData.__live(v);
    }

    function liveMesh(v) {
        return EfxMesh.__live(v);
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
                __efxCheckKnown(v, known, 'material');
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
            __efxCheckKnown(ch, ck, chan[ci]);
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


    function liveImageData(v) {
        return EfxImageData.__live(v);
    }

    function liveTexture(v) {
        return EfxTexture.__live(v);
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

    /* --------------------------------------------- F11 (billboards + particles) */

    function __efxPartVec(v, what, allow2, shapeMsg) {
        if (!Array.isArray(v)) {
            throw new TypeError(what + ' must be an array');
        }
        if (v.length !== 3 && !(allow2 && v.length === 2)) {
            throw new TypeError(shapeMsg || (what + ' must be [x,y] or [x,y,z]'));
        }
        var out = [0, 0, 0];
        for (var i = 0; i < v.length; i++) {
            out[i] = __efxFinite(v[i], what + ' entries must be finite numbers');
        }
        return out;
    }

    function __efxPartRange(v, what) {
        if (Array.isArray(v)) {
            return __efxFloatArray(v, 2);
        }
        var d = __efxFinite(v, what + ' must be a finite number');
        return [d, d];
    }

    function __efxPartEnum(v, map, dflt, what) {
        if (v === undefined) {
            return dflt;
        }
        if (typeof v !== 'string' || !Object.prototype.hasOwnProperty.call(map, v)) {
            throw new TypeError(what + ' has an unknown value');
        }
        return map[v];
    }

    function __efxSprite(tex, e) {
        if (!__efxIsObject(e)) {
            throw new TypeError('each sprite must be an object');
        }
        var known = { x: 1, y: 1, size: 1, color: 1, rotation: 1, scale: 1,
                      sourceRect: 1, origin: 1 };
                __efxCheckKnown(e, known, 'drawSprites', false, true);
        var out = {
            x: __efxFinite(e['x'], 'sprite x and y must be finite numbers'),
            y: __efxFinite(e['y'], 'sprite x and y must be finite numbers'),
            color: [1, 1, 1, 1],
            rotation: 0,
            scale: 1,
            src: [0, 0, 0, 0],
            hasSrc: 0,
            origin: [0, 0],
            hasOrigin: 0,
        };
        if (e['color'] !== undefined) {
            out.color = __efxFloatArray(e['color'], 4);
        }
        if (e['rotation'] !== undefined) {
            out.rotation = __efxFinite(e['rotation'], 'rotation must be a finite number');
        }
        if (e['scale'] !== undefined) {
            out.scale = __efxFinite(e['scale'], 'scale must be a finite number');
            if (out.scale <= 0) {
                throw new RangeError('scale must be > 0');
            }
        }
        if (e['size'] !== undefined) {
            out.size = __efxFloatArray(e['size'], 2);
            if (out.size[0] <= 0 || out.size[1] <= 0) {
                throw new RangeError('size entries must be > 0');
            }
            out.hasSize = 1;
        }
        if (e['origin'] !== undefined) {
            out.origin = __efxFloatArray(e['origin'], 2);
            out.hasOrigin = 1;
        }
        if (e['sourceRect'] !== undefined) {
            out.src = __efxSourceRect(tex, e['sourceRect']);
            out.hasSrc = 1;
        }
        if (out.hasSize) {
            out.w = out.size[0];
            out.h = out.size[1];
        } else if (out.hasSrc) {
            out.w = out.src[2];
            out.h = out.src[3];
        } else {
            out.w = tex.w;
            out.h = tex.h;
        }
        out.ox = out.hasOrigin ? out.origin[0] : out.w * 0.5;
        out.oy = out.hasOrigin ? out.origin[1] : out.h * 0.5;
        return out;
    }

    function livePS(v) {
        return EfxParticleSystem.__live(v);
    }

    var EfxParticleSystem = __efxResourceClass('EfxParticleSystem', {
        typeMsg: 'expected a ParticleSystem',
        init: function (handle) {
            this.__handle = handle;
        },
        destroy: function () {
            bridge['_efx_bridge_particles_destroy'](this.__handle);
        },
        methods: {
            emit: function (n) {
                if (arguments.length < 1 || typeof n !== 'number' || !isFinite(n) ||
                    n !== Math.floor(n) || n < 0) {
                    throw new RangeError('emit count must be a non-negative integer');
                }
                var rc = bridge['_efx_bridge_particles_emit'](this.__handle, n);
                if (rc !== 0) {
                    throw new Error('emit failed');
                }
            },
            start: function () {
                bridge['_efx_bridge_particles_start'](this.__handle);
            },
            stop: function () {
                bridge['_efx_bridge_particles_stop'](this.__handle);
            },
            pause: function () {
                bridge['_efx_bridge_particles_pause'](this.__handle);
            },
            reset: function () {
                bridge['_efx_bridge_particles_reset'](this.__handle);
            },
        },
        getters: {
            count: {
                get: function () {
                    return bridge['_efx_bridge_particles_count'](this.__handle);
                },
            },
            speedScale: {
                get: function () {
                    return bridge['_efx_bridge_particles_speed_scale'](this.__handle);
                },
                set: function (v) {
                    if (typeof v !== 'number' || !isFinite(v) || v <= 0) {
                        throw new RangeError('speedScale must be a finite number > 0');
                    }
                    bridge['_efx_bridge_particles_set_speed_scale'](this.__handle, v);
                },
            },
        },
    });

