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
        var id = bridge['_efx_bridge_gamepad_button_id'](p);
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
        var id = bridge['_efx_bridge_gamepad_axis_id'](p);
        bridge['_free'](p);
        if (id < 0) {
            throw new TypeError('unknown gamepad axis');
        }
        return id;
    }
    return {
        index: slot,
        connected: !!bridge['_efx_bridge_gamepad_connected'](slot),
        name: UTF8ToString(bridge['_efx_bridge_gamepad_name'](slot)),
        mapped: !!bridge['_efx_bridge_gamepad_mapped'](slot),
        isDown: function (b) {
            return !!bridge['_efx_bridge_gamepad_button_down'](slot, buttonId(b));
        },
        isPressed: function (b) {
            return !!bridge['_efx_bridge_gamepad_button_pressed'](slot, buttonId(b));
        },
        isReleased: function (b) {
            return !!bridge['_efx_bridge_gamepad_button_released'](slot, buttonId(b));
        },
        axis: function (a) {
            return bridge['_efx_bridge_gamepad_axis'](slot, axisId(a));
        },
        rawButton: function (i) {
            if (typeof i !== 'number') {
                throw new TypeError('gamepad raw query requires an index');
            }
            return bridge['_efx_bridge_gamepad_raw_button'](slot, i | 0);
        },
        rawAxis: function (i) {
            if (typeof i !== 'number') {
                throw new TypeError('gamepad raw query requires an index');
            }
            return bridge['_efx_bridge_gamepad_raw_axis'](slot, i | 0);
        },
    };
}

/* F9: drain the frame's staged input events into the registered callbacks,
   in arrival order, before the update hooks run (design D2/D3) */
function __efxDispatchInput(st) {
    var bridge = Module;
    var n = bridge['_efx_bridge_input_count']();
    for (var i = 0; i < n; i++) {
        var type = bridge['_efx_bridge_input_type'](i);
        var hooks = null;
        var ev = null;
        if (type === 0) {
            hooks = st.keyboardDown;
            ev = {
                key: UTF8ToString(bridge['_efx_bridge_key_name'](
                    bridge['_efx_bridge_input_key'](i))),
                repeat: !!bridge['_efx_bridge_input_repeat'](i),
                mods: __efxMods(bridge['_efx_bridge_input_mods'](i)),
            };
        } else if (type === 1) {
            hooks = st.keyboardUp;
            ev = {
                key: UTF8ToString(bridge['_efx_bridge_key_name'](
                    bridge['_efx_bridge_input_key'](i))),
                mods: __efxMods(bridge['_efx_bridge_input_mods'](i)),
            };
        } else if (type === 2) {
            hooks = st.keyboardChar;
            ev = { char: String.fromCodePoint(bridge['_efx_bridge_input_char'](i)) };
        } else if (type === 3 || type === 4) {
            hooks = type === 3 ? st.mouseDown : st.mouseUp;
            ev = {
                button: UTF8ToString(bridge['_efx_bridge_button_name'](
                    bridge['_efx_bridge_input_button'](i))),
                x: bridge['_efx_bridge_input_x'](i),
                y: bridge['_efx_bridge_input_y'](i),
                mods: __efxMods(bridge['_efx_bridge_input_mods'](i)),
            };
        } else if (type === 5) {
            hooks = st.mouseMove;
            ev = {
                x: bridge['_efx_bridge_input_x'](i),
                y: bridge['_efx_bridge_input_y'](i),
                dx: bridge['_efx_bridge_input_dx'](i),
                dy: bridge['_efx_bridge_input_dy'](i),
            };
        } else if (type === 6) {
            hooks = st.mouseWheel;
            ev = {
                dx: bridge['_efx_bridge_input_dx'](i),
                dy: bridge['_efx_bridge_input_dy'](i),
            };
        }
        if (hooks === null) {
            continue;
        }
        var rc = __efxCallHooks(st, hooks, ev);
        if (rc !== 0) {
            bridge['_efx_bridge_input_clear']();
            return rc;
        }
    }
    bridge['_efx_bridge_input_clear']();

    /* F13: gamepad connect/disconnect callbacks fire with the pad view */
    var cn = bridge['_efx_bridge_gamepad_connect_count']();
    for (var c = 0; c < cn; c++) {
        var slot = bridge['_efx_bridge_gamepad_connect_at'](c);
        var rcc = __efxCallHooks(st, st.gamepadConnect, __efxGamepadView(slot));
        if (rcc !== 0) {
            return rcc;
        }
    }
    var dn = bridge['_efx_bridge_gamepad_disconnect_count']();
    for (var d = 0; d < dn; d++) {
        var dslot = bridge['_efx_bridge_gamepad_disconnect_at'](d);
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
    /* one known-field check for every option bag (P8): unknown keys throw a
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

    /* P9: one factory for the native-backed resource wrappers. It owns the
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
    function __efxTextError(rc) {
        if (rc === 4) {
            throw new RangeError('text layout failed');
        }
        throw new Error('text operation failed');
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
                __efxCheckKnown(v, known, 'post effect');
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

    /* particle wire layout (floats); kept in sync with src/web/bridge_particles.c */
    var EFX_PART_WIRE_LEN = 352;

    function __efxPartVec(v, what, allow2) {
        if (!Array.isArray(v)) {
            throw new TypeError(what + ' must be an array');
        }
        if (v.length !== 3 && !(allow2 && v.length === 2)) {
            throw new TypeError(what + ' must be [x,y] or [x,y,z]');
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
            var srcv = e['sourceRect'];
            if (!__efxIsObject(srcv)) {
                throw new TypeError('sourceRect must be an object');
            }
            var skeys = ['x', 'y', 'w', 'h'];
            for (var j = 0; j < 4; j++) {
                out.src[j] = __efxFinite(srcv[skeys[j]],
                    'sourceRect fields must be finite numbers');
            }
            if (out.src[2] <= 0 || out.src[3] <= 0) {
                throw new RangeError('sourceRect extent must be > 0');
            }
            if (out.src[0] < 0 || out.src[1] < 0 ||
                out.src[0] + out.src[2] > tex.w ||
                out.src[1] + out.src[3] > tex.h) {
                throw new RangeError('sourceRect outside texture bounds');
            }
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

    /* parse + validate a particle options object into the wire layout;
       mirrors the desktop binding's validation and error types */
    function __efxParticleWire(opts) {
        if (!__efxIsObject(opts)) {
            throw new TypeError('createParticleSystem requires an options object');
        }
        var known = { texture: 1, max: 1, space: 1, facing: 1, normal: 1,
            blend: 1, lifetime: 1, emissionRate: 1, emitterLifetime: 1,
            position: 1, direction: 1, spread: 1, speed: 1, gravity: 1,
            linearAcceleration: 1, radialAcceleration: 1,
            tangentialAcceleration: 1, linearDamping: 1, sizes: 1,
            sizeVariation: 1, colors: 1, rotation: 1, spin: 1, spinVariation: 1,
            relativeRotation: 1, emissionShape: 1, quads: 1, insertMode: 1,
            speedScale: 1 };
        __efxCheckKnown(opts, known, 'createParticleSystem');
        var w = new Float32Array(EFX_PART_WIRE_LEN);
        /* defaults mirror efx_render_particles_create */
        w[4] = 1; w[5] = 1; w[7] = -1; w[8] = 1; w[10] = 1; w[12] = 1;
        w[25] = 1; w[44] = 1; w[52] = 1; w[53] = 1; w[54] = 1; w[55] = 1;
        w[344] = 1;

        if (opts['texture'] === undefined) {
            throw new TypeError('createParticleSystem requires a texture');
        }
        var tex = liveSample(opts['texture']);

        if (opts['max'] === undefined) {
            throw new TypeError('createParticleSystem requires max');
        }
        if (typeof opts['max'] !== 'number' || !isFinite(opts['max']) ||
            opts['max'] !== Math.floor(opts['max'])) {
            throw new TypeError('max must be an integer');
        }
        if (opts['max'] < 1 || opts['max'] > 65536) {
            throw new RangeError('max must be in 1..65536');
        }
        w[0] = opts['max'];

        w[1] = __efxPartEnum(opts['space'], { world: 0, screen: 1 }, 0, 'space');
        w[2] = __efxPartEnum(opts['facing'],
                             { view: 0, y: 1, plane: 2 }, 0, 'facing');
        if (w[1] === 1 && w[2] !== 0) {
            throw new TypeError("facing must be 'view' for screen space");
        }
        w[3] = __efxPartEnum(opts['blend'],
                             { alpha: 0, additive: 1, subtractive: 2 }, 0,
                             'blend');
        if (opts['normal'] !== undefined) {
            var n = __efxPartVec(opts['normal'], 'normal', false);
            w[343] = n[0]; w[344] = n[1]; w[345] = n[2];
        }
        if (opts['lifetime'] === undefined) {
            throw new TypeError('createParticleSystem requires lifetime');
        }
        var life = __efxPartRange(opts['lifetime'], 'lifetime');
        w[4] = life[0]; w[5] = life[1];
        if (opts['emissionRate'] !== undefined) {
            w[6] = __efxFinite(opts['emissionRate'], 'emissionRate must be a finite number');
        }
        if (opts['emitterLifetime'] !== undefined) {
            w[7] = __efxFinite(opts['emitterLifetime'], 'emitterLifetime must be a finite number');
        }
        if (opts['speedScale'] !== undefined) {
            w[8] = __efxFinite(opts['speedScale'], 'speedScale must be a finite number');
        }
        if (opts['spread'] !== undefined) {
            w[9] = __efxFinite(opts['spread'], 'spread must be a finite number');
        }
        if (opts['position'] !== undefined) {
            var p = __efxPartVec(opts['position'], 'position', true);
            w[21] = p[0]; w[22] = p[1]; w[23] = p[2];
        }
        if (opts['direction'] !== undefined) {
            var dir = __efxPartVec(opts['direction'], 'direction', true);
            w[24] = dir[0]; w[25] = dir[1]; w[26] = dir[2];
        }
        if (opts['speed'] !== undefined) {
            var sp = __efxPartRange(opts['speed'], 'speed');
            w[27] = sp[0]; w[28] = sp[1];
        }
        if (opts['gravity'] !== undefined) {
            var g = __efxPartVec(opts['gravity'], 'gravity', true);
            w[29] = g[0]; w[30] = g[1]; w[31] = g[2];
        }
        if (opts['linearAcceleration'] !== undefined) {
            var la = __efxPartVec(opts['linearAcceleration'], 'linearAcceleration', false);
            for (i = 0; i < 3; i++) { w[32 + i] = la[i]; w[35 + i] = la[i]; }
        }
        if (opts['radialAcceleration'] !== undefined) {
            var ra = __efxPartRange(opts['radialAcceleration'], 'radialAcceleration');
            w[38] = ra[0]; w[39] = ra[1];
        }
        if (opts['tangentialAcceleration'] !== undefined) {
            var ta = __efxPartRange(opts['tangentialAcceleration'], 'tangentialAcceleration');
            w[40] = ta[0]; w[41] = ta[1];
        }
        if (opts['linearDamping'] !== undefined) {
            var ld = __efxPartRange(opts['linearDamping'], 'linearDamping');
            w[42] = ld[0]; w[43] = ld[1];
        }
        if (opts['sizes'] !== undefined) {
            var sizes = Array.isArray(opts['sizes']) ? opts['sizes'] : [opts['sizes']];
            if (sizes.length < 1 || sizes.length > 8) {
                throw new RangeError('sizes must hold 1..8 entries');
            }
            for (i = 0; i < sizes.length; i++) {
                var sv = __efxFinite(sizes[i], 'sizes must be finite numbers');
                if (sv <= 0) {
                    throw new RangeError('sizes must be > 0');
                }
                w[44 + i] = sv;
            }
            w[10] = sizes.length;
        }
        if (opts['sizeVariation'] !== undefined) {
            w[11] = __efxFinite(opts['sizeVariation'], 'sizeVariation must be a finite number');
        }
        if (opts['colors'] !== undefined) {
            var cols = (Array.isArray(opts['colors']) && opts['colors'].length &&
                        Array.isArray(opts['colors'][0]))
                ? opts['colors'] : [opts['colors']];
            if (cols.length < 1 || cols.length > 8) {
                throw new RangeError('colors must hold 1..8 entries');
            }
            for (i = 0; i < cols.length; i++) {
                var col = __efxFloatArray(cols[i], 4);
                for (var k = 0; k < 4; k++) {
                    w[52 + i * 4 + k] = col[k];
                }
            }
            w[12] = cols.length;
        }
        if (opts['rotation'] !== undefined) {
            var ro = __efxPartRange(opts['rotation'], 'rotation');
            w[16] = ro[0]; w[17] = ro[1];
        }
        if (opts['spin'] !== undefined) {
            var spin = __efxPartRange(opts['spin'], 'spin');
            w[18] = spin[0]; w[19] = spin[1];
        }
        if (opts['spinVariation'] !== undefined) {
            w[20] = __efxFinite(opts['spinVariation'], 'spinVariation must be a finite number');
        }
        if (opts['relativeRotation'] !== undefined) {
            if (typeof opts['relativeRotation'] !== 'boolean') {
                throw new TypeError('relativeRotation must be a boolean');
            }
            w[13] = opts['relativeRotation'] ? 1 : 0;
        }
        if (opts['emissionShape'] !== undefined) {
            var es = opts['emissionShape'];
            if (!__efxIsObject(es)) {
                throw new TypeError('emissionShape must be an object');
            }
            var ekn = { shape: 1, size: 1 };
                        __efxCheckKnown(es, ekn, 'emissionShape');
            w[14] = __efxPartEnum(es['shape'],
                { point: 0, box: 1, sphere: 2, sphereSurface: 3, disc: 4 }, 0,
                'emissionShape.shape');
            if (es['size'] !== undefined) {
                var ss = __efxPartVec(es['size'], 'emissionShape.size', false);
                w[84] = ss[0]; w[85] = ss[1]; w[86] = ss[2];
            }
        }
        if (opts['quads'] !== undefined) {
            var quads = opts['quads'];
            if (!Array.isArray(quads)) {
                throw new TypeError('quads must be an array');
            }
            if (quads.length > 64) {
                throw new RangeError('quads must hold at most 64 entries');
            }
            for (i = 0; i < quads.length; i++) {
                var q = quads[i];
                var rect;
                if (Array.isArray(q)) {
                    rect = __efxFloatArray(q, 4);
                } else if (__efxIsObject(q)) {
                    rect = [
                        __efxFinite(q['x'], 'quad rect fields must be finite numbers'),
                        __efxFinite(q['y'], 'quad rect fields must be finite numbers'),
                        __efxFinite(q['w'], 'quad rect fields must be finite numbers'),
                        __efxFinite(q['h'], 'quad rect fields must be finite numbers'),
                    ];
                } else {
                    throw new TypeError('each quad must be an object or [x,y,w,h]');
                }
                for (k = 0; k < 4; k++) {
                    w[87 + i * 4 + k] = rect[k];
                }
            }
            w[15] = quads.length;
        }
        if (opts['insertMode'] !== undefined) {
            w[346] = __efxPartEnum(opts['insertMode'],
                { top: 0, bottom: 1, random: 2 }, 0, 'insertMode');
        }
        return { wire: w, texture: tex.handle };
    }

    function livePS(v) {
        return EfxParticleSystem.__live(v);
    }

    var EfxParticleSystem = __efxResourceClass('EfxParticleSystem', {
        typeMsg: 'expected a ParticleSystem',
        init: function (handle, texture, opts) {
            this.__handle = handle;
            this.__texture = texture;
            this.__opts = opts;
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
            set: function (opts) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('set requires an options object');
                }
                var merged = {};
                var k;
                for (k in this.__opts) {
                    if (Object.prototype.hasOwnProperty.call(this.__opts, k)) {
                        merged[k] = this.__opts[k];
                    }
                }
                for (k in opts) {
                    if (Object.prototype.hasOwnProperty.call(opts, k)) {
                        merged[k] = opts[k];
                    }
                }
                var parsed = __efxParticleWire(merged);
                var ptr = mallocCopyF32(parsed.wire);
                var rc = bridge['_efx_bridge_particles_set'](this.__handle, ptr,
                                                             parsed.texture);
                bridge['_free'](ptr);
                if (rc !== 0) {
                    throw new RangeError('invalid particle configuration');
                }
                this.__opts = merged;
                this.__texture = parsed.texture;
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
                    if (this.__opts) {
                        this.__opts['speedScale'] = v;
                    }
                },
            },
        },
    });

