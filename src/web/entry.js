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
        bridge['_efx_bridge_mem_free'](p);
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
        bridge['_efx_bridge_mem_free'](p);
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

    /* F8a: FontData (parsed font) and Font (baked atlas) native-backed
       classes, id-based like ImageData */
    function EfxFontData(id) {
        this.__id = id;
        this.__alive = true;
    }
    EfxFontData.prototype.destroy = function () {
        if (!(this instanceof EfxFontData)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_fontdata_destroy'](this.__id);
    };

    function EfxFont(id) {
        this.__id = id;
        this.__alive = true;
    }
    EfxFont.prototype.destroy = function () {
        if (!(this instanceof EfxFont)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_font_destroy'](this.__id);
    };
    function __efxFontGetter(name) {
        return {
            get: function () {
                if (!(this instanceof EfxFont)) {
                    throw new TypeError('expected a Font');
                }
                if (!this.__alive) {
                    throw new TypeError('using a destroyed resource');
                }
                return bridge[name](this.__id);
            },
        };
    }
    Object.defineProperty(EfxFont.prototype, 'size',
        __efxFontGetter('_efx_bridge_font_size'));
    Object.defineProperty(EfxFont.prototype, 'lineHeight',
        __efxFontGetter('_efx_bridge_font_line_height'));
    Object.defineProperty(EfxFont.prototype, 'ascent',
        __efxFontGetter('_efx_bridge_font_ascent'));
    Object.defineProperty(EfxFont.prototype, 'descent',
        __efxFontGetter('_efx_bridge_font_descent'));

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

    /* --------------------------------------------- F11 (billboards + particles) */

    /* particle wire layout (floats); kept in sync with src/web/bridge.c */
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
        if (!(v instanceof EfxParticleSystem)) {
            throw new TypeError('expected a ParticleSystem');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed resource');
        }
        return v;
    }

    function EfxParticleSystem(handle, texture, opts) {
        this.__handle = handle;
        this.__texture = texture;
        this.__opts = opts;
        this.__alive = true;
    }
    EfxParticleSystem.prototype.destroy = function () {
        if (!(this instanceof EfxParticleSystem)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_particles_destroy'](this.__handle);
    };
    EfxParticleSystem.prototype.emit = function (n) {
        livePS(this);
        if (arguments.length < 1 || typeof n !== 'number' || !isFinite(n) ||
            n !== Math.floor(n) || n < 0) {
            throw new RangeError('emit count must be a non-negative integer');
        }
        var rc = bridge['_efx_bridge_particles_emit'](this.__handle, n);
        if (rc !== 0) {
            throw new Error('emit failed');
        }
    };
    EfxParticleSystem.prototype.start = function () {
        livePS(this);
        bridge['_efx_bridge_particles_start'](this.__handle);
    };
    EfxParticleSystem.prototype.stop = function () {
        livePS(this);
        bridge['_efx_bridge_particles_stop'](this.__handle);
    };
    EfxParticleSystem.prototype.pause = function () {
        livePS(this);
        bridge['_efx_bridge_particles_pause'](this.__handle);
    };
    EfxParticleSystem.prototype.reset = function () {
        livePS(this);
        bridge['_efx_bridge_particles_reset'](this.__handle);
    };
    EfxParticleSystem.prototype.set = function (opts) {
        livePS(this);
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
        bridge['_efx_bridge_mem_free'](ptr);
        if (rc !== 0) {
            throw new RangeError('invalid particle configuration');
        }
        this.__opts = merged;
        this.__texture = parsed.texture;
    };
    Object.defineProperty(EfxParticleSystem.prototype, 'count', {
        get: function () {
            livePS(this);
            return bridge['_efx_bridge_particles_count'](this.__handle);
        },
    });
    Object.defineProperty(EfxParticleSystem.prototype, 'speedScale', {
        get: function () {
            livePS(this);
            return bridge['_efx_bridge_particles_speed_scale'](this.__handle);
        },
        set: function (v) {
            livePS(this);
            if (typeof v !== 'number' || !isFinite(v) || v <= 0) {
                throw new RangeError('speedScale must be a finite number > 0');
            }
            bridge['_efx_bridge_particles_set_speed_scale'](this.__handle, v);
            if (this.__opts) {
                this.__opts['speedScale'] = v;
            }
        },
    });

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
                                __efxCheckKnown(opts, known, 'createTexture');
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
                try {
                    __efxCheckKnown(opts, known, 'loadMeshData');
                } catch (e) {
                    bridge['_efx_bridge_mem_free'](pathPtr);
                    throw e;
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
        loadFontData: function (path) {
            if (arguments.length < 1 || typeof path !== 'string') {
                throw new TypeError('loadFontData requires a path string');
            }
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_load_fontdata'](p);
            bridge['_efx_bridge_mem_free'](p);
            if (!id) {
                throw new Error('font could not be loaded');
            }
            return new EfxFontData(id);
        },
        createFont: function (fontData, opts) {
            if (arguments.length < 1 || !(fontData instanceof EfxFontData)) {
                throw new TypeError('createFont requires a FontData');
            }
            if (!fontData.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            if (arguments.length < 2 || !__efxIsObject(opts)) {
                throw new TypeError('createFont requires an options object');
            }
            var known = { size: 1, glyphs: 1, padding: 1, filter: 1, outline: 1,
                          shadow: 1 };
                        __efxCheckKnown(opts, known, 'createFont');
            if (opts.size === undefined) {
                throw new TypeError('createFont requires size');
            }
            var size = __efxFinite(opts.size, 'size must be a finite number');
            if (!(size > 0)) {
                throw new RangeError('size must be > 0');
            }
            var glyphsPtr = 0;
            if (opts.glyphs !== undefined) {
                if (typeof opts.glyphs !== 'string') {
                    throw new TypeError('glyphs must be a string');
                }
                if (opts.glyphs.length === 0) {
                    throw new RangeError('glyphs must not be empty');
                }
                glyphsPtr = __efxAllocCStr(opts.glyphs);
            }
            var padding = 1;
            if (opts.padding !== undefined) {
                var pv = __efxFinite(opts.padding, 'padding must be a finite number');
                if (pv < 0 || pv !== Math.floor(pv)) {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw new RangeError('padding must be a non-negative integer');
                }
                padding = pv | 0;
            }
            var filter = 1;
            if (opts.filter !== undefined) {
                if (opts.filter === 'linear') {
                    filter = 1;
                } else if (opts.filter === 'nearest') {
                    filter = 0;
                } else {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw new TypeError("filter must be 'linear' or 'nearest'");
                }
            }
            var hasOutline = 0, outlineWidth = 0;
            if (opts.outline !== undefined && opts.outline !== null) {
                if (!__efxIsObject(opts.outline)) {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw new TypeError('outline must be an object or null');
                }
                var oKnown = { width: 1 };
                try {
                    __efxCheckKnown(opts.outline, oKnown, 'outline');
                } catch (e) {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw e;
                }
                if (opts.outline.width === undefined) {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw new TypeError('outline requires a numeric width');
                }
                outlineWidth = __efxFinite(opts.outline.width,
                                           'outline width must be a finite number');
                if (!(outlineWidth > 0)) {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw new RangeError('outline width must be > 0');
                }
                hasOutline = 1;
            }
            var hasShadow = 0, shadowBlur = 0, offX = 0, offY = 0;
            if (opts.shadow !== undefined && opts.shadow !== null) {
                if (!__efxIsObject(opts.shadow)) {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw new TypeError('shadow must be an object or null');
                }
                var sKnown = { blur: 1, offset: 1 };
                try {
                    __efxCheckKnown(opts.shadow, sKnown, 'shadow');
                } catch (e) {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw e;
                }
                if (opts.shadow.blur === undefined) {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw new TypeError('shadow requires a numeric blur');
                }
                shadowBlur = __efxFinite(opts.shadow.blur,
                                         'shadow blur must be a finite number');
                if (!(shadowBlur > 0)) {
                    if (glyphsPtr) { bridge['_efx_bridge_mem_free'](glyphsPtr); }
                    throw new RangeError('shadow blur must be > 0');
                }
                if (opts.shadow.offset !== undefined) {
                    var off = __efxFloatArray(opts.shadow.offset, 2);
                    offX = off[0];
                    offY = off[1];
                }
                hasShadow = 1;
            }
            var id = bridge['_efx_bridge_create_font'](
                fontData.__id, size, glyphsPtr, padding, filter, hasOutline,
                outlineWidth, hasShadow, shadowBlur, offX, offY);
            if (glyphsPtr) {
                bridge['_efx_bridge_mem_free'](glyphsPtr);
            }
            if (!id) {
                throw new Error('font could not be baked');
            }
            return new EfxFont(id);
        },
        measureText: function (text, font, opts) {
            if (arguments.length < 2 || typeof text !== 'string') {
                throw new TypeError('measureText requires (text, font, opts?)');
            }
            if (!(font instanceof EfxFont)) {
                throw new TypeError('measureText requires a live Font');
            }
            if (!font.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            var lo = __efxTextLayout(opts);
            var tptr = __efxAllocCStr(text);
            var optr = bridge['_malloc'](12);
            var rc = bridge['_efx_bridge_text_measure'](
                tptr, font.__id, lo.align, lo.valign, lo.hasWidth, lo.width,
                lo.hasLh, lo.lh, lo.scale, lo.rotation, optr);
            bridge['_efx_bridge_mem_free'](tptr);
            var b = { width: HEAPF32[optr >> 2],
                      height: HEAPF32[(optr >> 2) + 1],
                      lines: HEAPF32[(optr >> 2) + 2] };
            bridge['_efx_bridge_mem_free'](optr);
            if (rc !== 0) {
                __efxTextError(rc);
            }
            return b;
        },
        drawText: function (text, font, x, y, opts) {
            if (arguments.length < 4 || typeof text !== 'string') {
                throw new TypeError('drawText requires (text, font, x, y, opts?)');
            }
            if (!(font instanceof EfxFont)) {
                throw new TypeError('drawText requires a live Font');
            }
            if (!font.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            x = __efxFinite(x, 'drawText requires finite x and y');
            y = __efxFinite(y, 'drawText requires finite x and y');
            var lo = __efxTextLayout(opts);
            var color = [1, 1, 1, 1], oc = [0, 0, 0, 1], sc = [0, 0, 0, 1];
            if (__efxIsObject(opts)) {
                if (opts.color !== undefined) { color = __efxFloatArray(opts.color, 4); }
                if (opts.outlineColor !== undefined) { oc = __efxFloatArray(opts.outlineColor, 4); }
                if (opts.shadowColor !== undefined) { sc = __efxFloatArray(opts.shadowColor, 4); }
            }
            var tptr = __efxAllocCStr(text);
            var optr = bridge['_malloc'](12);
            var rc = bridge['_efx_bridge_text_draw'](
                tptr, font.__id, x, y, lo.align, lo.valign, lo.hasWidth,
                lo.width, lo.hasLh, lo.lh, lo.scale, lo.rotation,
                color[0], color[1], color[2], color[3],
                oc[0], oc[1], oc[2], oc[3],
                sc[0], sc[1], sc[2], sc[3], optr);
            bridge['_efx_bridge_mem_free'](tptr);
            var b = { width: HEAPF32[optr >> 2],
                      height: HEAPF32[(optr >> 2) + 1],
                      lines: HEAPF32[(optr >> 2) + 2] };
            bridge['_efx_bridge_mem_free'](optr);
            if (rc !== 0) {
                __efxTextError(rc);
            }
            return b;
        },
        createRenderTarget: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('createRenderTarget requires an options object');
            }
            var known = { width: 1, height: 1 };
            __efxCheckKnown(opts, known, 'createRenderTarget');
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
                                __efxCheckKnown(opts, kn, 'setRenderScale');
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
                                __efxCheckKnown(opts, known, 'drawQuad', false, true);
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
        drawBillboard: function (pos, opts) {
            if (arguments.length < 2) {
                throw new TypeError('drawBillboard requires (pos, opts)');
            }
            var p = __efxPartVec(pos, 'pos', false);
            if (!__efxIsObject(opts)) {
                throw new TypeError('drawBillboard options must be an object');
            }
            var known = { texture: 1, size: 1, color: 1, sourceRect: 1,
                          rotation: 1, facing: 1, depthTest: 1, normal: 1 };
                        __efxCheckKnown(opts, known, 'drawBillboard');
            if (opts['texture'] === undefined) {
                throw new TypeError('drawBillboard requires a texture');
            }
            var tex = liveSample(opts['texture']);
            var size = [1, 1];
            if (opts['size'] !== undefined) {
                if (Array.isArray(opts['size'])) {
                    size = __efxFloatArray(opts['size'], 2);
                } else {
                    var sd = __efxFinite(opts['size'], 'size must be a number or [w,h]');
                    size = [sd, sd];
                }
            }
            if (size[0] <= 0 || size[1] <= 0) {
                throw new RangeError('size entries must be > 0');
            }
            var color = [1, 1, 1, 1];
            if (opts['color'] !== undefined) {
                color = __efxFloatArray(opts['color'], 4);
            }
            var rotation = 0;
            if (opts['rotation'] !== undefined) {
                rotation = __efxFinite(opts['rotation'], 'rotation must be a finite number');
            }
            var facing = __efxPartEnum(opts['facing'], { view: 0, y: 1 }, 0, 'facing');
            var normal = [0, 1, 0];
            if (opts['normal'] !== undefined) {
                normal = __efxPartVec(opts['normal'], 'normal', false);
            }
            var depth = 1;
            if (opts['depthTest'] !== undefined) {
                if (typeof opts['depthTest'] !== 'boolean') {
                    throw new TypeError('depthTest must be a boolean');
                }
                depth = opts['depthTest'] ? 1 : 0;
            }
            var src = [0, 0, 0, 0];
            var hasSrc = 0;
            if (opts['sourceRect'] !== undefined) {
                var srcv = opts['sourceRect'];
                if (!__efxIsObject(srcv)) {
                    throw new TypeError('sourceRect must be an object');
                }
                var skeys = ['x', 'y', 'w', 'h'];
                for (var j = 0; j < 4; j++) {
                    src[j] = __efxFinite(srcv[skeys[j]],
                        'sourceRect fields must be finite numbers');
                }
                if (src[2] <= 0 || src[3] <= 0) {
                    throw new RangeError('sourceRect extent must be > 0');
                }
                if (src[0] < 0 || src[1] < 0 ||
                    src[0] + src[2] > tex.w || src[1] + src[3] > tex.h) {
                    throw new RangeError('sourceRect outside texture bounds');
                }
                hasSrc = 1;
            }
            var pPtr = mallocCopyF32(p);
            var nPtr = mallocCopyF32(normal);
            var rc = bridge['_efx_bridge_draw_billboard'](tex.handle, pPtr,
                size[0], size[1], color[0], color[1], color[2], color[3],
                rotation, facing, nPtr, depth, src[0], src[1], src[2], src[3],
                hasSrc);
            bridge['_efx_bridge_mem_free'](pPtr);
            bridge['_efx_bridge_mem_free'](nPtr);
            if (rc === 1) {
                throw new RangeError('display list budget exceeded');
            }
            if (rc === 2) {
                throw new TypeError('expected a Texture or RenderTarget');
            }
            if (rc === 10) {
                throw new RangeError('invalid billboard size or facing');
            }
            if (rc !== 0) {
                throw new Error('drawBillboard failed');
            }
        },
        drawSprites: function (texture, sprites) {
            if (arguments.length < 2) {
                throw new TypeError('drawSprites requires (texture, sprites)');
            }
            var tex = liveSample(texture);
            if (!Array.isArray(sprites)) {
                throw new TypeError('sprites must be an array');
            }
            var parsed = [];
            for (var i = 0; i < sprites.length; i++) {
                parsed.push(__efxSprite(tex, sprites[i]));
            }
            for (i = 0; i < parsed.length; i++) {
                var s = parsed[i];
                var rc = bridge['_efx_bridge_draw_sprite'](tex.handle, s.x, s.y,
                    s.w, s.h, s.color[0], s.color[1], s.color[2], s.color[3],
                    s.rotation, s.scale, s.src[0], s.src[1], s.src[2], s.src[3],
                    s.hasSrc, s.ox, s.oy);
                if (rc === 1) {
                    throw new RangeError('display list budget exceeded');
                }
                if (rc === 4) {
                    throw new Error('no render surface (draw calls need a window)');
                }
                if (rc !== 0) {
                    throw new Error('drawSprites failed');
                }
            }
        },
        createParticleSystem: function (opts) {
            var parsed = __efxParticleWire(opts);
            var ptr = mallocCopyF32(parsed.wire);
            var handle = bridge['_efx_bridge_particles_create'](ptr, parsed.texture);
            bridge['_efx_bridge_mem_free'](ptr);
            if (!handle) {
                throw new RangeError('invalid particle configuration');
            }
            var snapshot = {};
            for (var k in opts) {
                if (Object.prototype.hasOwnProperty.call(opts, k)) {
                    snapshot[k] = opts[k];
                }
            }
            return new EfxParticleSystem(handle, parsed.texture, snapshot);
        },
        drawParticles: function (sys) {
            livePS(sys);
            var rc = bridge['_efx_bridge_particles_draw'](sys.__handle);
            if (rc === 1) {
                throw new RangeError('display list budget exceeded');
            }
            if (rc === 2) {
                throw new TypeError('expected a live ParticleSystem');
            }
            if (rc === 9) {
                throw new TypeError('cannot sample the render target being drawn into');
            }
            if (rc !== 0) {
                throw new Error('drawParticles failed');
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
                        __efxCheckKnown(opts, known, 'setCamera3D');
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
                        __efxCheckKnown(opts, bagKnown, 'createMeshData');
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
                                __efxCheckKnown(opts, known, 'drawMesh');
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
                                __efxCheckKnown(s, sk, 'pose sample');
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
                        __efxCheckKnown(opts, lk, 'setLight');
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
                        __efxCheckKnown(opts, dk, 'setDirectionalLight');
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

    /* F9 input namespaces (desktop parity: one C source for names/state) */
    function makeInputRegister(list) {
        return function (fn) {
            if (typeof fn !== 'function') {
                throw new TypeError('hook must be a function');
            }
            var entry = { fn: fn, active: true };
            list.push(entry);
            return function () {
                entry.active = false;
            };
        };
    }
    function __keyId(name) {
        if (typeof name !== 'string') {
            throw new TypeError('keyboard query requires a key name');
        }
        var p = __efxAllocCStr(name);
        var id = bridge['_efx_bridge_key_id'](p);
        bridge['_efx_bridge_mem_free'](p);
        if (id < 0) {
            throw new TypeError('unknown key');
        }
        return id;
    }
    function __buttonId(name) {
        if (typeof name !== 'string') {
            throw new TypeError('mouse query requires a button name');
        }
        var p = __efxAllocCStr(name);
        var id = bridge['_efx_bridge_button_id'](p);
        bridge['_efx_bridge_mem_free'](p);
        if (id < 0) {
            throw new TypeError('unknown mouse button');
        }
        return id;
    }
    api.keyboard = {
        isDown: function (key) {
            return !!bridge['_efx_bridge_key_down'](__keyId(key));
        },
        isPressed: function (key) {
            return !!bridge['_efx_bridge_key_pressed'](__keyId(key));
        },
        isReleased: function (key) {
            return !!bridge['_efx_bridge_key_released'](__keyId(key));
        },
        onDown: makeInputRegister(st.keyboardDown),
        onUp: makeInputRegister(st.keyboardUp),
        onChar: makeInputRegister(st.keyboardChar),
    };
    api.mouse = {
        isDown: function (b) {
            return !!bridge['_efx_bridge_button_down'](__buttonId(b));
        },
        isPressed: function (b) {
            return !!bridge['_efx_bridge_button_pressed'](__buttonId(b));
        },
        isReleased: function (b) {
            return !!bridge['_efx_bridge_button_released'](__buttonId(b));
        },
        onDown: makeInputRegister(st.mouseDown),
        onUp: makeInputRegister(st.mouseUp),
        onMove: makeInputRegister(st.mouseMove),
        onWheel: makeInputRegister(st.mouseWheel),
    };
    Object.defineProperty(api.mouse, 'position', {
        get: function () {
            return [bridge['_efx_bridge_mouse_x'](), bridge['_efx_bridge_mouse_y']()];
        },
    });
    Object.defineProperty(api.mouse, 'x', {
        get: function () { return bridge['_efx_bridge_mouse_x'](); },
    });
    Object.defineProperty(api.mouse, 'y', {
        get: function () { return bridge['_efx_bridge_mouse_y'](); },
    });
    Object.defineProperty(api.mouse, 'delta', {
        get: function () {
            return [bridge['_efx_bridge_mouse_dx'](), bridge['_efx_bridge_mouse_dy']()];
        },
    });
    Object.defineProperty(api.mouse, 'wheel', {
        get: function () {
            return [bridge['_efx_bridge_wheel_dx'](), bridge['_efx_bridge_wheel_dy']()];
        },
    });
    api.window = {};
    Object.defineProperty(api.window, 'size', {
        get: function () {
            return [bridge['_efx_bridge_window_width'](),
                    bridge['_efx_bridge_window_height']()];
        },
    });
    Object.defineProperty(api.window, 'width', {
        get: function () { return bridge['_efx_bridge_window_width'](); },
    });
    Object.defineProperty(api.window, 'height', {
        get: function () { return bridge['_efx_bridge_window_height'](); },
    });
    Object.defineProperty(api.window, 'dpiScale', {
        get: function () { return bridge['_efx_bridge_window_dpi'](); },
    });

    /* F13 gamepad namespace (desktop parity: same C core, same errors) */
    api.gamepad = {
        get: function (index) {
            if (typeof index !== 'number') {
                throw new TypeError('gamepad.get requires an index');
            }
            var i = index | 0;
            if (!bridge['_efx_bridge_gamepad_connected'](i)) {
                return null;
            }
            return __efxGamepadView(i);
        },
        onConnect: makeInputRegister(st.gamepadConnect),
        onDisconnect: makeInputRegister(st.gamepadDisconnect),
    };
    Object.defineProperty(api.gamepad, 'count', {
        get: function () { return bridge['_efx_bridge_gamepad_count'](); },
    });

    /* ---------------------------------------------- F12 physics */

    var physBodies = new Map();
    var physCharacters = new Map();

    function __physNumber(v, what) {
        if (typeof v !== 'number' || !isFinite(v)) {
            throw new TypeError(what + ' must be a finite number');
        }
        return v;
    }
    function __physMask(v, what) {
        if (typeof v !== 'number' || !isFinite(v) || Math.floor(v) !== v ||
            v < 0 || v > 4294967295) {
            throw new RangeError(what + ' must be a 32-bit unsigned integer');
        }
        return v;
    }
    function __physShape(v) {
        if (!__efxIsObject(v) || Array.isArray(v)) {
            throw new TypeError('shape must be an options object');
        }
        if (v.type === 'sphere') {
            __efxCheckKnown(v, ['type', 'radius'], 'shape', true);
            if (typeof v.radius !== 'number') {
                throw new TypeError('sphere shapes require a radius');
            }
            if (!(v.radius > 0)) {
                throw new RangeError('radius must be positive');
            }
            return { t: 0, r: v.radius, hx: 0, hy: 0, hz: 0, height: 0, mesh: null };
        }
        if (v.type === 'box') {
            __efxCheckKnown(v, ['type', 'size'], 'shape', true);
            var s = __efxFloatArray(v.size, 3);
            if (!(s[0] > 0 && s[1] > 0 && s[2] > 0)) {
                throw new RangeError('box size components must be positive');
            }
            return { t: 1, r: 0, hx: s[0], hy: s[1], hz: s[2], height: 0, mesh: null };
        }
        if (v.type === 'capsule') {
            __efxCheckKnown(v, ['type', 'radius', 'height'], 'shape', true);
            if (typeof v.radius !== 'number' || typeof v.height !== 'number') {
                throw new TypeError('capsule shapes require radius and height');
            }
            if (!(v.radius > 0)) {
                throw new RangeError('radius must be positive');
            }
            if (!(v.height >= 2 * v.radius)) {
                throw new RangeError('capsule height must be at least 2 * radius');
            }
            return { t: 2, r: v.radius, hx: 0, hy: 0, hz: 0, height: v.height, mesh: null };
        }
        if (v.type === 'mesh') {
            __efxCheckKnown(v, ['type', 'mesh'], 'shape', true);
            return { t: 3, r: 0, hx: 0, hy: 0, hz: 0, height: 0, mesh: liveMesh(v.mesh) };
        }
        throw new TypeError('unknown shape type');
    }
    function __physBodyLive(v) {
        if (!(v instanceof EfxBody)) {
            throw new TypeError('expected a Body');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed Body');
        }
        return v;
    }
    function __physCharLive(v) {
        if (!(v instanceof EfxCharacter)) {
            throw new TypeError('expected a Character');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed Character');
        }
        return v;
    }

    function EfxBody(handle) {
        this.__handle = handle;
        this.__alive = true;
    }
    EfxBody.prototype.destroy = function () {
        if (!(this instanceof EfxBody)) {
            throw new TypeError('expected a Body');
        }
        if (this.__alive) {
            this.__alive = false;
            bridge['_efx_bridge_physics_body_destroy'](this.__handle);
            physBodies['delete'](this.__handle);
        }
    };
    EfxBody.prototype.applyImpulse = function (v) {
        var b = __physBodyLive(this);
        var a = __efxFloatArray(v, 3);
        if (!bridge['_efx_bridge_physics_body_apply_impulse'](
                b.__handle, a[0], a[1], a[2])) {
            throw new TypeError('applyImpulse requires a dynamic body');
        }
    };
    EfxBody.prototype.applyForce = function (v) {
        var b = __physBodyLive(this);
        var a = __efxFloatArray(v, 3);
        if (!bridge['_efx_bridge_physics_body_apply_force'](
                b.__handle, a[0], a[1], a[2])) {
            throw new TypeError('applyForce requires a dynamic body');
        }
    };
    function __physBodyVec3(fn, handle) {
        var ptr = bridge['_malloc'](12);
        var base = ptr >> 2;
        fn(handle, ptr);
        var out = [HEAPF32[base], HEAPF32[base + 1], HEAPF32[base + 2]];
        bridge['_efx_bridge_mem_free'](ptr);
        return out;
    }
    Object.defineProperty(EfxBody.prototype, 'position', {
        get: function () {
            var b = __physBodyLive(this);
            return __physBodyVec3(bridge['_efx_bridge_physics_body_position'],
                                  b.__handle);
        },
    });
    Object.defineProperty(EfxBody.prototype, 'velocity', {
        get: function () {
            var b = __physBodyLive(this);
            return __physBodyVec3(bridge['_efx_bridge_physics_body_get_velocity'],
                                  b.__handle);
        },
        set: function (v) {
            var b = __physBodyLive(this);
            var a = __efxFloatArray(v, 3);
            bridge['_efx_bridge_physics_body_set_velocity'](
                b.__handle, a[0], a[1], a[2]);
        },
    });
    Object.defineProperty(EfxBody.prototype, 'transform', {
        get: function () {
            var b = __physBodyLive(this);
            var p = __physBodyVec3(bridge['_efx_bridge_physics_body_position'],
                                   b.__handle);
            return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, p[0], p[1], p[2], 1];
        },
    });
    Object.defineProperty(EfxBody.prototype, 'contacts', {
        get: function () {
            var b = __physBodyLive(this);
            var n = bridge['_efx_bridge_physics_body_contact_count'](b.__handle);
            var out = [];
            if (n > 0) {
                var ptr = bridge['_malloc'](11 * 8);
                var base = ptr >> 3;
                for (var i = 0; i < n; i++) {
                    if (!bridge['_efx_bridge_physics_body_contact'](
                            b.__handle, i, ptr)) {
                        continue;
                    }
                    out.push({
                        body: physBodies.get(HEAPF64[base + 8]) || null,
                        sensor: !!HEAPF64[base + 10],
                        normal: [HEAPF64[base], HEAPF64[base + 1], HEAPF64[base + 2]],
                        point: [HEAPF64[base + 3], HEAPF64[base + 4], HEAPF64[base + 5]],
                        depth: HEAPF64[base + 6],
                        impulse: HEAPF64[base + 7],
                    });
                }
                bridge['_efx_bridge_mem_free'](ptr);
            }
            return out;
        },
    });

    function EfxCharacter(handle) {
        this.__handle = handle;
        this.__alive = true;
    }
    EfxCharacter.prototype.destroy = function () {
        if (!(this instanceof EfxCharacter)) {
            throw new TypeError('expected a Character');
        }
        if (this.__alive) {
            this.__alive = false;
            bridge['_efx_bridge_physics_character_destroy'](this.__handle);
            physCharacters['delete'](this.__handle);
        }
    };
    EfxCharacter.prototype.moveAndSlide = function (motion) {
        var c = __physCharLive(this);
        var m = __efxFloatArray(motion, 3);
        var ptr = bridge['_malloc'](10 * 8);
        var base = ptr >> 3;
        if (!bridge['_efx_bridge_physics_character_move'](
                c.__handle, m[0], m[1], m[2], ptr)) {
            bridge['_efx_bridge_mem_free'](ptr);
            throw new TypeError('moveAndSlide failed');
        }
        var pos = [HEAPF64[base], HEAPF64[base + 1], HEAPF64[base + 2]];
        var onFloor = !!HEAPF64[base + 3];
        var onWall = !!HEAPF64[base + 4];
        var onCeiling = !!HEAPF64[base + 5];
        var fn = [HEAPF64[base + 6], HEAPF64[base + 7], HEAPF64[base + 8]];
        var count = HEAPF64[base + 9];
        var cols = [];
        for (var i = 0; i < count; i++) {
            if (!bridge['_efx_bridge_physics_move_collision'](
                    c.__handle, i, ptr)) {
                continue;
            }
            cols.push({
                body: physBodies.get(HEAPF64[base]) || null,
                normal: [HEAPF64[base + 1], HEAPF64[base + 2], HEAPF64[base + 3]],
                point: [HEAPF64[base + 4], HEAPF64[base + 5], HEAPF64[base + 6]],
            });
        }
        bridge['_efx_bridge_mem_free'](ptr);
        return {
            position: pos, onFloor: onFloor, onWall: onWall,
            onCeiling: onCeiling, floorNormal: fn, collisions: cols,
        };
    };
    Object.defineProperty(EfxCharacter.prototype, 'position', {
        get: function () {
            var c = __physCharLive(this);
            return __physBodyVec3(
                bridge['_efx_bridge_physics_character_position'], c.__handle);
        },
    });
    Object.defineProperty(EfxCharacter.prototype, 'velocity', {
        get: function () {
            var c = __physCharLive(this);
            return __physBodyVec3(
                bridge['_efx_bridge_physics_character_get_velocity'],
                c.__handle);
        },
        set: function (v) {
            var c = __physCharLive(this);
            var a = __efxFloatArray(v, 3);
            bridge['_efx_bridge_physics_character_set_velocity'](
                c.__handle, a[0], a[1], a[2]);
        },
    });
    Object.defineProperty(EfxCharacter.prototype, 'onFloor', {
        get: function () {
            var c = __physCharLive(this);
            return !!bridge['_efx_bridge_physics_character_on_floor'](c.__handle);
        },
    });

    function __physBodyCommonOpts(opts) {
        var sensor = opts.sensor === undefined ? false : !!opts.sensor;
        var friction = opts.friction === undefined ? 0.5
                                                   : __physNumber(opts.friction, 'friction');
        var restitution = opts.restitution === undefined
                              ? 0
                              : __physNumber(opts.restitution, 'restitution');
        if (friction < 0) {
            throw new RangeError('friction must not be negative');
        }
        if (restitution < 0 || restitution > 1) {
            throw new RangeError('restitution must be in [0, 1]');
        }
        var position = opts.position === undefined
                           ? [0, 0, 0]
                           : __efxFloatArray(opts.position, 3);
        var layer = opts.layer === undefined ? 4294967295
                                             : __physMask(opts.layer, 'layer');
        var mask = opts.mask === undefined ? 4294967295
                                           : __physMask(opts.mask, 'mask');
        return {
            sensor: sensor, friction: friction, restitution: restitution,
            position: position, layer: layer, mask: mask,
        };
    }

    api.physics = {
        createBody: function (opts) {
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('createBody requires an options object');
            }
            __efxCheckKnown(opts, ['dynamic', 'sensor', 'shape', 'position', 'mass',
                              'friction', 'restitution', 'layer', 'mask'],
                       'createBody', true);
            if (opts.shape === undefined) {
                throw new TypeError('createBody requires a shape');
            }
            var sh = __physShape(opts.shape);
            var dynamic = !!opts.dynamic;
            var mass = opts.mass === undefined ? 1
                                               : __physNumber(opts.mass, 'mass');
            if (dynamic && !(mass > 0)) {
                throw new RangeError('dynamic bodies require a positive mass');
            }
            var c = __physBodyCommonOpts(opts);
            var handle;
            if (sh.t === 3) {
                if (dynamic) {
                    throw new TypeError('mesh colliders are static only');
                }
                handle = bridge['_efx_bridge_physics_create_static_mesh'](
                    sh.mesh.__handle, c.position[0], c.position[1],
                    c.position[2], c.sensor ? 1 : 0, c.friction, c.restitution,
                    c.layer, c.mask);
            } else {
                handle = bridge['_efx_bridge_physics_create_body'](
                    dynamic ? 1 : 0, c.sensor ? 1 : 0, sh.t, sh.r, sh.hx, sh.hy,
                    sh.hz, sh.height, c.position[0], c.position[1],
                    c.position[2], mass, c.friction, c.restitution, c.layer,
                    c.mask);
            }
            if (!handle) {
                throw new Error('failed to create body');
            }
            var b = new EfxBody(handle);
            physBodies.set(handle, b);
            return b;
        },
        createStaticMesh: function (mesh, opts) {
            var m = liveMesh(mesh);
            opts = opts === undefined ? {} : opts;
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('createStaticMesh options must be an object');
            }
            __efxCheckKnown(opts, ['position', 'sensor', 'friction', 'restitution',
                              'layer', 'mask'],
                       'createStaticMesh', true);
            var c = __physBodyCommonOpts(opts);
            var handle = bridge['_efx_bridge_physics_create_static_mesh'](
                m.__handle, c.position[0], c.position[1], c.position[2],
                c.sensor ? 1 : 0, c.friction, c.restitution, c.layer, c.mask);
            if (!handle) {
                throw new Error('failed to create mesh collider');
            }
            var b = new EfxBody(handle);
            physBodies.set(handle, b);
            return b;
        },
        createCharacter: function (opts) {
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('createCharacter requires an options object');
            }
            __efxCheckKnown(opts, ['radius', 'height', 'position', 'up',
                              'floorMaxAngle', 'floorSnapLength', 'stepHeight',
                              'maxSlides', 'safeMargin', 'layer', 'mask'],
                       'createCharacter', true);
            if (typeof opts.radius !== 'number' ||
                typeof opts.height !== 'number') {
                throw new TypeError('createCharacter requires radius and height');
            }
            if (!(opts.radius > 0)) {
                throw new RangeError('radius must be positive');
            }
            if (!(opts.height >= 2 * opts.radius)) {
                throw new RangeError('height must be at least 2 * radius');
            }
            var position = opts.position === undefined
                               ? [0, 0, 0]
                               : __efxFloatArray(opts.position, 3);
            var up = opts.up === undefined ? [0, 1, 0]
                                           : __efxFloatArray(opts.up, 3);
            if (up[0] === 0 && up[1] === 0 && up[2] === 0) {
                throw new RangeError('up must be non-zero');
            }
            var floorMaxAngle = opts.floorMaxAngle === undefined
                                    ? 45
                                    : __physNumber(opts.floorMaxAngle, 'floorMaxAngle');
            var snap = opts.floorSnapLength === undefined
                           ? 0.1
                           : __physNumber(opts.floorSnapLength, 'floorSnapLength');
            var step = opts.stepHeight === undefined
                           ? 0.3
                           : __physNumber(opts.stepHeight, 'stepHeight');
            var safe = opts.safeMargin === undefined
                           ? 0.001
                           : __physNumber(opts.safeMargin, 'safeMargin');
            var maxSlides = opts.maxSlides === undefined
                                ? 6
                                : __physNumber(opts.maxSlides, 'maxSlides');
            if (!(maxSlides >= 1) || Math.floor(maxSlides) !== maxSlides) {
                throw new RangeError('maxSlides must be a positive integer');
            }
            if (snap < 0) {
                throw new RangeError('floorSnapLength must not be negative');
            }
            if (step < 0) {
                throw new RangeError('stepHeight must not be negative');
            }
            if (safe < 0) {
                throw new RangeError('safeMargin must not be negative');
            }
            var layer = opts.layer === undefined ? 4294967295
                                                 : __physMask(opts.layer, 'layer');
            var mask = opts.mask === undefined ? 4294967295
                                               : __physMask(opts.mask, 'mask');
            var handle = bridge['_efx_bridge_physics_create_character'](
                opts.radius, opts.height, position[0], position[1], position[2],
                up[0], up[1], up[2], floorMaxAngle, snap, step, safe, maxSlides,
                layer, mask);
            if (!handle) {
                throw new Error('failed to create character');
            }
            var c = new EfxCharacter(handle);
            physCharacters.set(handle, c);
            return c;
        },
        step: function (dt) {
            if (typeof dt !== 'number' || !isFinite(dt)) {
                throw new TypeError('dt must be a finite number');
            }
            bridge['_efx_bridge_physics_step'](dt);
        },
        clear: function () {
            bridge['_efx_bridge_physics_clear']();
            physBodies.clear();
            physCharacters.clear();
        },
        raycast: function (origin, direction, opts) {
            var o = __efxFloatArray(origin, 3);
            var d = __efxFloatArray(direction, 3);
            opts = opts === undefined ? {} : opts;
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('raycast options must be an object');
            }
            __efxCheckKnown(opts, ['maxDistance', 'mask', 'all', 'sensors'],
                       'raycast', true);
            if (typeof opts.maxDistance !== 'number' ||
                !isFinite(opts.maxDistance) || !(opts.maxDistance > 0)) {
                throw new TypeError('raycast requires a positive maxDistance');
            }
            var mask = opts.mask === undefined ? 4294967295
                                               : __physMask(opts.mask, 'mask');
            var all = !!opts.all;
            var sensors = !!opts.sensors;
            var count = bridge['_efx_bridge_physics_raycast'](
                o[0], o[1], o[2], d[0], d[1], d[2], opts.maxDistance, mask,
                sensors ? 1 : 0, all ? 1 : 0, 0);
            if (count <= 0) {
                return all ? [] : null;
            }
            var ptr = bridge['_malloc'](count * 10 * 8);
            var base = ptr >> 3;
            bridge['_efx_bridge_physics_raycast'](
                o[0], o[1], o[2], d[0], d[1], d[2], opts.maxDistance, mask,
                sensors ? 1 : 0, all ? 1 : 0, ptr);
            var out = [];
            for (var i = 0; i < count; i++) {
                var b0 = base + i * 10;
                var body = null;
                if (HEAPF64[b0 + 8]) {
                    body = physCharacters.get(HEAPF64[b0 + 8]) || null;
                } else {
                    body = physBodies.get(HEAPF64[b0 + 7]) || null;
                }
                out.push({
                    point: [HEAPF64[b0], HEAPF64[b0 + 1], HEAPF64[b0 + 2]],
                    normal: [HEAPF64[b0 + 3], HEAPF64[b0 + 4], HEAPF64[b0 + 5]],
                    distance: HEAPF64[b0 + 6], body: body,
                });
            }
            bridge['_efx_bridge_mem_free'](ptr);
            return all ? out : out[0];
        },
        overlap: function (shape, opts) {
            var sh = __physShape(shape);
            opts = opts === undefined ? {} : opts;
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('overlap options must be an object');
            }
            __efxCheckKnown(opts, ['position', 'mask'], 'overlap', true);
            var p = opts.position === undefined
                        ? [0, 0, 0]
                        : __efxFloatArray(opts.position, 3);
            var mask = opts.mask === undefined ? 4294967295
                                               : __physMask(opts.mask, 'mask');
            var meshHandle = sh.mesh ? sh.mesh.__handle : 0;
            var count = bridge['_efx_bridge_physics_overlap'](
                sh.t, sh.r, sh.hx, sh.hy, sh.hz, sh.height, meshHandle, p[0],
                p[1], p[2], mask, 0);
            if (count <= 0) {
                return [];
            }
            var ptr = bridge['_malloc'](count * 3 * 8);
            var base = ptr >> 3;
            bridge['_efx_bridge_physics_overlap'](
                sh.t, sh.r, sh.hx, sh.hy, sh.hz, sh.height, meshHandle, p[0],
                p[1], p[2], mask, ptr);
            var out = [];
            for (var i = 0; i < count; i++) {
                var b0 = base + i * 3;
                if (HEAPF64[b0 + 1]) {
                    var ch = physCharacters.get(HEAPF64[b0 + 1]);
                    if (ch) {
                        out.push(ch);
                    }
                } else {
                    var bd = physBodies.get(HEAPF64[b0]);
                    if (bd) {
                        out.push(bd);
                    }
                }
            }
            bridge['_efx_bridge_mem_free'](ptr);
            return out;
        },
        shapeCast: function (shape, from, motion, opts) {
            var sh = __physShape(shape);
            var f = __efxFloatArray(from, 3);
            var m = __efxFloatArray(motion, 3);
            opts = opts === undefined ? {} : opts;
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('shapeCast options must be an object');
            }
            __efxCheckKnown(opts, ['mask', 'sensors'], 'shapeCast', true);
            var mask = opts.mask === undefined ? 4294967295
                                               : __physMask(opts.mask, 'mask');
            var sensors = !!opts.sensors;
            var meshHandle = sh.mesh ? sh.mesh.__handle : 0;
            var ptr = bridge['_malloc'](10 * 8);
            var base = ptr >> 3;
            var rc = bridge['_efx_bridge_physics_shape_cast'](
                sh.t, sh.r, sh.hx, sh.hy, sh.hz, sh.height, meshHandle, f[0],
                f[1], f[2], m[0], m[1], m[2], mask, sensors ? 1 : 0, ptr);
            if (!rc) {
                bridge['_efx_bridge_mem_free'](ptr);
                return null;
            }
            var body = null;
            if (HEAPF64[base + 8]) {
                body = physCharacters.get(HEAPF64[base + 8]) || null;
            } else {
                body = physBodies.get(HEAPF64[base + 7]) || null;
            }
            var out = {
                point: [HEAPF64[base], HEAPF64[base + 1], HEAPF64[base + 2]],
                normal: [HEAPF64[base + 3], HEAPF64[base + 4], HEAPF64[base + 5]],
                fraction: HEAPF64[base + 6], body: body,
            };
            bridge['_efx_bridge_mem_free'](ptr);
            return out;
        },
    };
    Object.defineProperty(api.physics, 'gravity', {
        get: function () {
            return [bridge['_efx_bridge_physics_gravity'](0),
                    bridge['_efx_bridge_physics_gravity'](1),
                    bridge['_efx_bridge_physics_gravity'](2)];
        },
        set: function (v) {
            var a = __efxFloatArray(v, 3);
            bridge['_efx_bridge_physics_set_gravity'](a[0], a[1], a[2]);
        },
    });
    Object.defineProperty(api.physics, 'iterations', {
        get: function () {
            return bridge['_efx_bridge_physics_iterations']();
        },
        set: function (v) {
            if (typeof v !== 'number' || !isFinite(v) || Math.floor(v) !== v ||
                v < 1) {
                throw new RangeError('iterations must be a positive integer');
            }
            bridge['_efx_bridge_physics_set_iterations'](v);
        },
    });

    /* --------------------------------------------- F14 audio */
    function __efxAudioOpts(opts, known, where) {
        if (opts === undefined || opts === null) {
            return {};
        }
        if (typeof opts !== 'object') {
            throw new TypeError(where + ' options must be an object');
        }
        var out = {};
        for (var k in opts) {
            if (Object.prototype.hasOwnProperty.call(opts, k)) {
                if (known.indexOf(k) < 0) {
                    throw new TypeError("unknown " + where + " option '" + k + "'");
                }
                out[k] = opts[k];
            }
        }
        return out;
    }
    function __efxAudioNum(v, name) {
        if (typeof v !== 'number' || !isFinite(v)) {
            throw new TypeError(name + ' must be a finite number');
        }
        return v;
    }

    function EfxAudioData(id) {
        this.__id = id;
        this.__alive = true;
    }
    EfxAudioData.prototype.destroy = function () {
        if (!(this instanceof EfxAudioData)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_audio_data_destroy'](this.__id);
    };

    function EfxAudioStream(id) {
        this.__id = id;
        this.__alive = true;
    }
    EfxAudioStream.prototype.destroy = function () {
        if (!(this instanceof EfxAudioStream)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_audio_stream_destroy'](this.__id);
    };

    function EfxAudio(id, volume, pan, pitch, loop) {
        this.__id = id;
        this.__alive = true;
        this.__volume = volume;
        this.__pan = pan;
        this.__pitch = pitch;
        this.__loop = loop;
    }
    Object.defineProperty(EfxAudio.prototype, 'playing', {
        get: function () {
            return this.__alive &&
                !!bridge['_efx_bridge_audio_handle_playing'](this.__id);
        },
    });
    Object.defineProperty(EfxAudio.prototype, 'paused', {
        get: function () {
            return this.__alive &&
                !!bridge['_efx_bridge_audio_handle_paused'](this.__id);
        },
    });
    Object.defineProperty(EfxAudio.prototype, 'volume', {
        get: function () { return this.__volume; },
        set: function (v) {
            var n = __efxAudioNum(v, 'volume');
            if (n < 0) throw new RangeError('volume must be a non-negative number');
            this.__volume = n;
            bridge['_efx_bridge_audio_handle_set_volume'](this.__id, n);
        },
    });
    Object.defineProperty(EfxAudio.prototype, 'pan', {
        get: function () { return this.__pan; },
        set: function (v) {
            var n = __efxAudioNum(v, 'pan');
            this.__pan = n;
            bridge['_efx_bridge_audio_handle_set_pan'](this.__id, n);
        },
    });
    Object.defineProperty(EfxAudio.prototype, 'pitch', {
        get: function () { return this.__pitch; },
        set: function (v) {
            var n = __efxAudioNum(v, 'pitch');
            if (n <= 0) throw new RangeError('pitch must be a positive number');
            this.__pitch = n;
            bridge['_efx_bridge_audio_handle_set_pitch'](this.__id, n);
        },
    });
    Object.defineProperty(EfxAudio.prototype, 'loop', {
        get: function () { return this.__loop; },
        set: function (v) {
            this.__loop = !!v;
            bridge['_efx_bridge_audio_handle_set_loop'](this.__id, v ? 1 : 0);
        },
    });
    EfxAudio.prototype.stop = function () {
        if (this.__alive) {
            bridge['_efx_bridge_audio_handle_stop'](this.__id);
        }
    };
    EfxAudio.prototype.pause = function () {
        if (this.__alive) {
            bridge['_efx_bridge_audio_handle_pause'](this.__id, 1);
        }
    };
    EfxAudio.prototype.resume = function () {
        if (this.__alive) {
            bridge['_efx_bridge_audio_handle_pause'](this.__id, 0);
        }
    };
    EfxAudio.prototype.destroy = function () {
        if (!(this instanceof EfxAudio)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_audio_handle_destroy'](this.__id);
    };

    function __efxAudioPlay(playFn, src, opts) {
        var o = __efxAudioOpts(opts, ['volume', 'pan', 'pitch', 'loop'],
                               'playAudio');
        var volume = (o.volume === undefined) ? 1
            : __efxAudioNum(o.volume, 'volume');
        if (volume < 0) {
            throw new RangeError('volume must be a non-negative number');
        }
        var pan = (o.pan === undefined) ? 0 : __efxAudioNum(o.pan, 'pan');
        var pitch = (o.pitch === undefined) ? 1 : __efxAudioNum(o.pitch, 'pitch');
        if (pitch <= 0) pitch = 1;
        var loop = o.loop ? 1 : 0;
        var id = playFn(src.__id, volume, pan, pitch, loop);
        if (!id) return null;
        return new EfxAudio(id, volume, pan, pitch, !!o.loop);
    }

    api.audio = {
        loadAudioData: function (path) {
            if (typeof path !== 'string') {
                throw new TypeError('loadAudioData requires a path string');
            }
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_audio_load_data'](p);
            bridge['_efx_bridge_mem_free'](p);
            if (!id) {
                throw new Error('cannot decode audio: ' + path);
            }
            return new EfxAudioData(id);
        },
        loadAudioStream: function (path) {
            if (typeof path !== 'string') {
                throw new TypeError('loadAudioStream requires a path string');
            }
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_audio_load_stream'](p);
            bridge['_efx_bridge_mem_free'](p);
            if (!id) {
                throw new Error('cannot decode audio: ' + path);
            }
            return new EfxAudioStream(id);
        },
        playAudio: function (source, opts) {
            if (source instanceof EfxAudioData) {
                if (!source.__alive) throw new Error('AudioData was destroyed');
                return __efxAudioPlay(
                    bridge['_efx_bridge_audio_play_data'], source, opts);
            }
            if (source instanceof EfxAudioStream) {
                if (!source.__alive) throw new Error('AudioStream was destroyed');
                return __efxAudioPlay(
                    bridge['_efx_bridge_audio_play_stream'], source, opts);
            }
            throw new TypeError('playAudio requires an AudioData or AudioStream');
        },
        resume: function () {
            bridge['_efx_bridge_audio_resume']();
        },
    };
    Object.defineProperty(api.audio, 'volume', {
        get: function () {
            return bridge['_efx_bridge_audio_master_volume']();
        },
        set: function (v) {
            var n = __efxAudioNum(v, 'volume');
            if (n < 0) throw new RangeError('volume must be a non-negative number');
            bridge['_efx_bridge_audio_set_master_volume'](n);
        },
    });

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

    /* engine-bundled pure-JS layer (F3 math + primitives, F10 CommonJS
       runtime): the same embedded source the desktop quickjs runtime
       evaluates (ADR 0022). The IIFE returns the module-runtime factory,
       which is instantiated per entry below with the web host-global
       shadow. */
    var preludeSrc = UTF8ToString(bridge['_efx_bridge_js_prelude']());
    st.createModuleRuntime = new Function('efx', preludeSrc)(api);

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
        Module['_efx_bridge_mem_free'](mp);
        if (!mptr) {
            __efxFail('player: no main.js in resource root: ' + root);
            return;
        }
        code = UTF8ToString(mptr);
        Module['_efx_bridge_mem_free'](mptr);
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
