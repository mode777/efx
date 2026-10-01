/* EmotionFX engine-bundled pure-JS layer (F3).
 *
 * Runs identically on both bindings: desktop quickjs evaluates this against
 * the `efx` namespace at runtime init; the web bridge exports the same
 * source and the page's JS engine evaluates it after the bridge boots.
 *
 * Rules (js-api two-layer contract): plain ES6 only — no host APIs; plain
 * JS data in/out (ADR 0010); angles in degrees; matrices are flat 16-number
 * column-major arrays; every helper is a pure function (never mutates its
 * arguments).
 */

function __efxM4Mul(a, b) {
    /* column-major: out[c*4+r] = sum_k a[k*4+r] * b[c*4+k]  (a·b) */
    var out = new Array(16);
    for (var c = 0; c < 4; c++) {
        for (var r = 0; r < 4; r++) {
            out[c * 4 + r] = a[r] * b[c * 4] +
                             a[4 + r] * b[c * 4 + 1] +
                             a[8 + r] * b[c * 4 + 2] +
                             a[12 + r] * b[c * 4 + 3];
        }
    }
    return out;
}

function __efxM4Identity() {
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
}

function __efxM4Perspective(fovYDeg, aspect, nearZ, farZ) {
    /* right-handed, GL depth range (-1..1) — matches the engine camera */
    var f = 1 / Math.tan((fovYDeg / 2) * Math.PI / 180);
    var m = new Array(16);
    m[0] = f / aspect; m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = f; m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = (farZ + nearZ) / (nearZ - farZ); m[11] = -1;
    m[12] = 0; m[13] = 0; m[14] = (2 * farZ * nearZ) / (nearZ - farZ); m[15] = 0;
    return m;
}

function __efxM4Ortho(width, height, nearZ, farZ) {
    var m = new Array(16);
    m[0] = 2 / width; m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = 2 / height; m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = 2 / (nearZ - farZ); m[11] = 0;
    m[12] = 0; m[13] = 0; m[14] = (nearZ + farZ) / (nearZ - farZ); m[15] = 1;
    return m;
}

function __efxM4Translate(m, v) {
    var out = m.slice();
    for (var r = 0; r < 4; r++) {
        out[12 + r] = m[12 + r] + m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2];
    }
    return out;
}

function __efxM4Rotate(m, deg, axis) {
    /* Rodrigues; right-handed CCW about axis (looking down the axis toward
       the origin); composes m·R (rotation applied first) */
    var len = Math.sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (len === 0) {
        return m.slice();
    }
    var x = axis[0] / len, y = axis[1] / len, z = axis[2] / len;
    var rad = deg * Math.PI / 180;
    var c = Math.cos(rad), s = Math.sin(rad), t = 1 - c;
    var r = new Array(16);
    r[0] = t * x * x + c;     r[1] = t * y * x + s * z; r[2] = t * z * x - s * y; r[3] = 0;
    r[4] = t * x * y - s * z; r[5] = t * y * y + c;     r[6] = t * z * y + s * x; r[7] = 0;
    r[8] = t * x * z + s * y; r[9] = t * y * z - s * x; r[10] = t * z * z + c;    r[11] = 0;
    r[12] = 0; r[13] = 0; r[14] = 0; r[15] = 1;
    return __efxM4Mul(m, r);
}

function __efxM4Scale(m, v) {
    var out = m.slice();
    for (var r = 0; r < 4; r++) {
        out[r] = m[r] * v[0];
        out[4 + r] = m[4 + r] * v[1];
        out[8 + r] = m[8 + r] * v[2];
    }
    return out;
}

function __efxV3Add(a, b) { return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]; }
function __efxV3Sub(a, b) { return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]; }
function __efxV3Scale(v, s) { return [v[0] * s, v[1] * s, v[2] * s]; }
function __efxV3Cross(a, b) {
    return [a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]];
}
function __efxV3Dot(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
function __efxV3Normalize(v) {
    var len = Math.sqrt(__efxV3Dot(v, v));
    if (len === 0) {
        return [0, 0, 0];
    }
    return [v[0] / len, v[1] / len, v[2] / len];
}

function __efxQuatIdentity() { return [0, 0, 0, 1]; }

function __efxQuatFromAxisAngle(deg, axis) {
    var len = Math.sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (len === 0) {
        return [0, 0, 0, 1];
    }
    var half = (deg / 2) * Math.PI / 180;
    var s = Math.sin(half) / len;
    return [axis[0] * s, axis[1] * s, axis[2] * s, Math.cos(half)];
}

function __efxQuatMultiply(a, b) {
    return [
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    ];
}

function __efxQuatToMat4(q) {
    var x = q[0], y = q[1], z = q[2], w = q[3];
    var x2 = x + x, y2 = y + y, z2 = z + z;
    var xx = x * x2, xy = x * y2, xz = x * z2;
    var yy = y * y2, yz = y * z2, zz = z * z2;
    var wx = w * x2, wy = w * y2, wz = w * z2;
    return [
        1 - (yy + zz), xy + wz, xz - wy, 0,
        xy - wz, 1 - (xx + zz), yz + wx, 0,
        xz + wy, yz - wx, 1 - (xx + yy), 0,
        0, 0, 0, 1,
    ];
}

/* option validation shared by the primitives (spec: finite positive
   size/radius -> RangeError; positive integer segments -> RangeError;
   unknown fields -> TypeError; missing object takes all defaults) */
function __efxPrimOpts(opts, keys, floats, ints, defaults) {
    var out = {};
    for (var d = 0; d < keys.length; d++) {
        out[keys[d]] = defaults[d];
    }
    if (opts === undefined || opts === null) {
        return out;
    }
    if (typeof opts !== 'object') {
        throw new TypeError('primitive options must be an object');
    }
    var names = Object.getOwnPropertyNames(opts);
    for (var i = 0; i < names.length; i++) {
        var known = false;
        for (var k = 0; k < keys.length; k++) {
            if (names[i] === keys[k]) {
                known = true;
                break;
            }
        }
        if (!known) {
            throw new TypeError("unknown primitive option '" + names[i] + "'");
        }
    }
    for (var f = 0; f < floats.length; f++) {
        var key = floats[f];
        if (opts[key] !== undefined) {
            var d = Number(opts[key]);
            if (!isFinite(d) || d <= 0) {
                throw new RangeError(key + ' must be > 0');
            }
            out[key] = d;
        }
    }
    for (var n = 0; n < ints.length; n++) {
        var ikey = ints[n];
        if (opts[ikey] !== undefined) {
            var iv = Number(opts[ikey]);
            if (!isFinite(iv) || iv <= 0 || iv !== Math.floor(iv)) {
                throw new RangeError(ikey + ' must be a positive integer');
            }
            out[ikey] = iv;
        }
    }
    return out;
}

/* primitive material option: `material` (a material object or null) is
   bound to the primitive's single surface at createMeshData time; undefined
   leaves the engine default. The material is validated/snapshotted by the
   existing createMeshData materials path. */
function __efxPrimMaterial(opts) {
    if (opts === undefined || opts === null) {
        return undefined;
    }
    return opts.material === undefined ? undefined : [opts.material];
}

/* axis-aligned cube centered on the origin; per-face normals and per-face
   0..1 uvs; outward CCW winding; 24 verts / 36 indices */
function __efxMakeCube(opts) {
    var o = __efxPrimOpts(opts, ['size', 'material'], ['size'], [], [1]);
    var h = o.size / 2;
    /* normal, edge u, edge v with cross(u, v) = normal */
    var faces = [
        [[1, 0, 0], [0, 0, -1], [0, 1, 0]],
        [[-1, 0, 0], [0, 0, 1], [0, 1, 0]],
        [[0, 1, 0], [1, 0, 0], [0, 0, -1]],
        [[0, -1, 0], [1, 0, 0], [0, 0, 1]],
        [[0, 0, 1], [1, 0, 0], [0, 1, 0]],
        [[0, 0, -1], [-1, 0, 0], [0, 1, 0]],
    ];
    var positions = [], normals = [], uvs = [], indices = [];
    var cornerUVs = [[0, 0], [1, 0], [1, 1], [0, 1]];
    for (var f = 0; f < 6; f++) {
        var n = faces[f][0], u = faces[f][1], v = faces[f][2];
        var base = f * 4;
        for (var c = 0; c < 4; c++) {
            var su = (c === 1 || c === 2) ? 1 : -1;
            var sv = (c === 2 || c === 3) ? 1 : -1;
            positions.push(
                n[0] * h + u[0] * h * su + v[0] * h * sv,
                n[1] * h + u[1] * h * su + v[1] * h * sv,
                n[2] * h + u[2] * h * su + v[2] * h * sv);
            normals.push(n[0], n[1], n[2]);
            uvs.push(cornerUVs[c][0], cornerUVs[c][1]);
        }
        indices.push(base, base + 1, base + 2, base, base + 2, base + 3);
    }
    return efx.createMeshData({
        positions: positions, normals: normals, uvs: uvs, indices: indices,
        materials: __efxPrimMaterial(opts),
    });
}

/* plane in the XZ plane facing +Y, centered; segments x segments quads;
   uv spans 0..1; CCW seen from above */
function __efxMakePlane(opts) {
    var o = __efxPrimOpts(opts, ['size', 'segments', 'material'], ['size'], ['segments'], [1, 1]);
    var S = o.segments, h = o.size / 2;
    var step = o.size / S;
    var positions = [], uvs = [], indices = [];
    for (var r = 0; r <= S; r++) {
        for (var c = 0; c <= S; c++) {
            positions.push(-h + c * step, 0, -h + r * step);
            uvs.push(c / S, r / S);
        }
    }
    for (var rr = 0; rr < S; rr++) {
        for (var cc = 0; cc < S; cc++) {
            var a = rr * (S + 1) + cc;
            var b = a + 1;
            var d = a + (S + 1);
            var e = d + 1;
            indices.push(a, d, e, a, e, b);
        }
    }
    return efx.createMeshData({ positions: positions, uvs: uvs, indices: indices,
        materials: __efxPrimMaterial(opts) });
}

/* UV sphere centered on the origin; segments latitude rings x segments
   longitude slices; normals = normalized positions; equirectangular uv */
function __efxMakeSphere(opts) {
    var o = __efxPrimOpts(opts, ['radius', 'segments', 'material'], ['radius'], ['segments'], [1, 16]);
    var S = o.segments, R = o.radius;
    var positions = [], normals = [], uvs = [], indices = [];
    for (var i = 0; i <= S; i++) {
        var vRow = i / S;
        var theta = vRow * Math.PI;
        var sinT = Math.sin(theta), cosT = Math.cos(theta);
        for (var j = 0; j < S; j++) {
            var uCol = j / S;
            var phi = uCol * 2 * Math.PI;
            var x = sinT * Math.cos(phi);
            var y = cosT;
            var z = sinT * Math.sin(phi);
            positions.push(x * R, y * R, z * R);
            normals.push(x, y, z);
            uvs.push(uCol, vRow);
        }
    }
    for (var ii = 0; ii < S; ii++) {
        for (var jj = 0; jj < S; jj++) {
            var a = ii * S + jj;
            var b = ii * S + ((jj + 1) % S);
            var c2 = (ii + 1) * S + jj;
            var d = (ii + 1) * S + ((jj + 1) % S);
            indices.push(a, d, c2, a, b, d);
        }
    }
    return efx.createMeshData({
        positions: positions, normals: normals, uvs: uvs, indices: indices,
        materials: __efxPrimMaterial(opts),
    });
}

/* vertical capsule centered on the origin (Y axis); `height` is the total
   tip-to-tip length including the hemispherical caps and must be at least
   2*radius; normals point outward; CCW winding; revolved with `segments`
   longitude slices and a similar latitude resolution on each cap */
function __efxMakeCapsule(opts) {
    var o = __efxPrimOpts(opts, ['radius', 'height', 'segments', 'material'],
                          ['radius', 'height'], ['segments'], [1, 2, 16]);
    var R = o.radius, H = o.height, S = o.segments;
    if (!(H >= 2 * R)) {
        throw new RangeError('capsule height must be at least 2 * radius');
    }
    var hh = H / 2 - R;                 /* half segment length */
    var half = Math.max(1, Math.ceil(S / 2));
    /* profile rows top->bottom: [y, radial fraction, normal radial, normal y] */
    var rows = [[hh + R, 0, 0, 1]];
    var i, a;
    for (i = 1; i < half; i++) {
        a = (i / half) * (Math.PI / 2);
        rows.push([hh + R * Math.cos(a), Math.sin(a), Math.sin(a), Math.cos(a)]);
    }
    rows.push([hh, 1, 1, 0]);
    rows.push([-hh, 1, 1, 0]);
    for (i = 1; i < half; i++) {
        a = (i / half) * (Math.PI / 2);
        rows.push([-hh - R * Math.cos(a), Math.sin(a), Math.sin(a),
                   -Math.cos(a)]);
    }
    rows.push([-hh - R, 0, 0, -1]);
    var positions = [], normals = [], uvs = [], indices = [];
    var rowCount = rows.length;
    for (var r = 0; r < rowCount; r++) {
        var y = rows[r][0], rr = rows[r][1] * R, nr = rows[r][2], ny = rows[r][3];
        for (var k = 0; k < S; k++) {
            var phi = (k / S) * 2 * Math.PI;
            var cp = Math.cos(phi), sp = Math.sin(phi);
            positions.push(rr * cp, y, rr * sp);
            normals.push(nr * cp, ny, nr * sp);
            uvs.push(k / S, r / (rowCount - 1));
        }
    }
    for (var q = 0; q < rowCount - 1; q++) {
        for (var m = 0; m < S; m++) {
            var p0 = q * S + m;
            var p1 = q * S + ((m + 1) % S);
            var p2 = (q + 1) * S + m;
            var p3 = (q + 1) * S + ((m + 1) % S);
            indices.push(p0, p3, p2, p0, p1, p3);
        }
    }
    return efx.createMeshData({
        positions: positions, normals: normals, uvs: uvs, indices: indices,
        materials: __efxPrimMaterial(opts),
    });
}

/* ------------------------------------------ F10 CommonJS module runtime
 *
 * A small, synchronous, provider-backed CommonJS implementation shared by both
 * bindings (ADR 0037). `require(path)` loads a file under the resource root via
 * the synchronous `efx.loadText` provider, evaluates it in a wrapper scope that
 * exposes `require`/`module`/`exports`, caches `module.exports` by resolved
 * path, and returns it. Specifiers are relative (`./`, `../`) or root-relative;
 * resolution tries the exact path then a deterministic `.js` fallback, and
 * `.json` files load as parsed JSON modules. There is no Node environment:
 * bare packages and Node built-ins simply do not resolve.
 *
 * `new Function` compiles each module body. On the web binding the host globals
 * are passed as shadowing parameters so module code never reaches them; the
 * desktop binding has no host globals to shadow. */

function __efxModuleDirname(p) {
    var i = p.lastIndexOf('/');
    return i < 0 ? '' : p.slice(0, i);
}

function __efxModuleNormalize(p) {
    var segs = String(p).split('/');
    var out = [];
    for (var i = 0; i < segs.length; i++) {
        var s = segs[i];
        if (s === '' || s === '.') {
            continue;
        }
        if (s === '..') {
            if (out.length === 0) {
                throw new Error("module path escapes the resource root: '" + p + "'");
            }
            out.pop();
        } else {
            out.push(s);
        }
    }
    return out.join('/');
}

function __efxModuleResolve(fromPath, spec) {
    if (typeof spec !== 'string' || spec.length === 0) {
        throw new TypeError('require: specifier must be a non-empty string');
    }
    if (spec.indexOf('\\') >= 0 ||
        /^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(spec) || spec.charAt(0) === '/') {
        throw new Error("unsupported module specifier '" + spec + "'");
    }
    var base;
    if (spec === '.' || spec === '..' ||
        spec.slice(0, 2) === './' || spec.slice(0, 3) === '../') {
        var d = __efxModuleDirname(fromPath);
        base = d ? d + '/' + spec : spec;
    } else {
        base = spec;
    }
    return __efxModuleNormalize(base);
}

function __efxModuleError(e, path) {
    if (!(e instanceof Error) || e.__efxModulePath) {
        return e; /* sentinels, non-Error throws and already-annotated errors */
    }
    var msg = (e.message !== undefined) ? String(e.message) : '';
    var err = new Error("module '" + path + "': " + msg);
    err.__efxModulePath = path;
    if (e.stack) {
        err.stack = e.stack;
    }
    return err;
}

function __efxCreateModuleRuntime(efx, opts) {
    var hostGlobals = (opts && opts.hostGlobals) ? opts.hostGlobals : [];
    var globalObject = (opts && opts.globalObject !== undefined)
        ? opts.globalObject
        : (typeof globalThis !== 'undefined' ? globalThis : undefined);
    var cache = {};

    function resolvePath(fromPath, spec) {
        var norm = __efxModuleResolve(fromPath, spec);
        var src = null;
        try {
            src = efx.loadText(norm);
        } catch (e) {
            src = null;
        }
        if (src !== null) {
            return { path: norm, source: src };
        }
        if (!/\.js$/.test(norm) && !/\.json$/.test(norm)) {
            try {
                src = efx.loadText(norm + '.js');
            } catch (e) {
                src = null;
            }
            if (src !== null) {
                return { path: norm + '.js', source: src };
            }
        }
        throw new Error("cannot find module '" + spec + "' required from '" +
            (fromPath || '<entry>') + "'");
    }

    function makeRequire(path) {
        function req(spec) {
            var res = resolvePath(path, spec);
            return loadModule(res.path, res.source, false);
        }
        req.resolve = function (spec) {
            return resolvePath(path, spec).path;
        };
        req.cache = cache;
        return req;
    }

    function compile(body, path) {
        var params = ['efx', 'require', 'module', 'exports',
                      '__filename', '__dirname'].concat(hostGlobals)
                      .concat(['globalThis']);
        var src = body + '\n//# sourceURL=' + path + '\n';
        return new Function(params.join(','), src);
    }

    function loadModule(path, source, isEntry) {
        if (cache[path]) {
            if (isEntry) {
                return { exports: cache[path].exports, update: null, render: null };
            }
            return cache[path].exports;
        }
        var moduleObj = { id: path, exports: {}, loaded: false };
        cache[path] = moduleObj;
        var src = source;
        if (src === undefined || src === null) {
            src = efx.loadText(path);
        }
        if (/\.json$/.test(path)) {
            var parsed;
            try {
                parsed = JSON.parse(src);
            } catch (e) {
                throw new Error("invalid JSON in module '" + path + "': " +
                    (e && e.message ? e.message : e));
            }
            moduleObj.exports = parsed;
            moduleObj.loaded = true;
            return isEntry ? { exports: parsed, update: null, render: null } : parsed;
        }
        var body = src;
        if (isEntry) {
            body = src + '\n;return { e: module.exports,'
                + ' u: (typeof update === "function" ? update : null),'
                + ' r: (typeof render === "function" ? render : null) };';
        }
        var args = [efx, makeRequire(path), moduleObj, moduleObj.exports,
                    path, __efxModuleDirname(path)];
        for (var i = 0; i < hostGlobals.length; i++) {
            args.push(undefined);
        }
        args.push(globalObject);
        var ret;
        try {
            var fn = compile(body, path);
            ret = fn.apply(undefined, args);
        } catch (e) {
            throw __efxModuleError(e, path);
        }        moduleObj.loaded = true;
        if (!isEntry) {
            return moduleObj.exports;
        }
        var exp = (ret && ret.e !== undefined) ? ret.e : moduleObj.exports;
        moduleObj.exports = exp;
        var u = (exp && typeof exp.update === 'function') ? exp.update
              : ((ret && typeof ret.u === 'function') ? ret.u : null);
        var r = (exp && typeof exp.render === 'function') ? exp.render
              : ((ret && typeof ret.r === 'function') ? ret.r : null);
        return { exports: exp, update: u, render: r };
    }

    return {
        runEntry: function (path, source) {
            var norm = __efxModuleNormalize(path);
            var src = source;
            if (src === undefined || src === null) {
                var res = resolvePath('', norm);
                norm = res.path;
                src = res.source;
            }
            return loadModule(norm, src, true);
        },
        resolve: resolvePath,
        cache: cache,
    };
}

/* ------------------------------------------ R22 spike: particles domain
 *
 * The shared option-bag validators live here once (this spike moves only the
 * particles wire); the natives object carries the binding-provided entry
 * points. Spike-only timing references (createParticleSystemC, drawQuadJS,
 * drawQuadUnpacked) exist on the throwaway branch only. */

function __efxIsObject(v) {
    return v !== null && (typeof v === 'object' || typeof v === 'function');
}

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

/* particle wire layout (floats); kept in sync with the desktop wire native
 * (api_particles.c) and src/web/bridge_particles.c */
var EFX_PART_WIRE_LEN = 352;

/* spike probe: the wire is written and consumed synchronously by the native,
 * so one reusable buffer serves every call (single-threaded runtimes) */
var __efxPartScratch = null;

/* parse + validate a particle options object into the wire layout;
 * `liveSample` resolves a Texture/RenderTarget argument to its handle
 * (binding-provided through natives, D1/D3) */
function __efxParticleWire(opts, liveSample) {
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
    if (!__efxPartScratch) {
        __efxPartScratch = new Float32Array(EFX_PART_WIRE_LEN);
    }
    var w = __efxPartScratch;
    /* a failed call may leave stale non-zero entries behind; every field is
     * written below from defaults or the bag before the native reads it */
    w.fill(0);
    /* defaults mirror efx_render_particles_create */
    w[4] = 1; w[5] = 1; w[7] = -1; w[8] = 1; w[10] = 1; w[12] = 1;
    w[25] = 1; w[44] = 1; w[52] = 1; w[53] = 1; w[54] = 1; w[55] = 1;
    w[344] = 1;

    if (opts['texture'] === undefined) {
        throw new TypeError('createParticleSystem requires a texture');
    }
    var tex = liveSample(opts['texture']);
    var texHandle = (tex !== null && typeof tex === 'object') ? tex.handle : tex;

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
        for (var i = 0; i < 3; i++) { w[32 + i] = la[i]; w[35 + i] = la[i]; }
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
        for (var s = 0; s < sizes.length; s++) {
            var sv = __efxFinite(sizes[s], 'sizes must be finite numbers');
            if (sv <= 0) {
                throw new RangeError('sizes must be > 0');
            }
            w[44 + s] = sv;
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
        for (var c = 0; c < cols.length; c++) {
            var col = __efxFloatArray(cols[c], 4);
            for (var k = 0; k < 4; k++) {
                w[52 + c * 4 + k] = col[k];
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
        for (var q = 0; q < quads.length; q++) {
            var qe = quads[q];
            var rect;
            if (Array.isArray(qe)) {
                rect = __efxFloatArray(qe, 4);
            } else if (__efxIsObject(qe)) {
                rect = [
                    __efxFinite(qe['x'], 'quad rect fields must be finite numbers'),
                    __efxFinite(qe['y'], 'quad rect fields must be finite numbers'),
                    __efxFinite(qe['w'], 'quad rect fields must be finite numbers'),
                    __efxFinite(qe['h'], 'quad rect fields must be finite numbers'),
                ];
            } else {
                throw new TypeError('each quad must be an object or [x,y,w,h]');
            }
            for (var k2 = 0; k2 < 4; k2++) {
                w[87 + q * 4 + k2] = rect[k2];
            }
        }
        w[15] = quads.length;
    }
    if (opts['insertMode'] !== undefined) {
        w[346] = __efxPartEnum(opts['insertMode'],
            { top: 0, bottom: 1, random: 2 }, 0, 'insertMode');
    }
    return { wire: w, texture: texHandle };
}

/* R22 spike-only: a JS copy of the desktop drawQuad option reading, used to
 * measure the hypothetical prelude-drawQuad cost against the native one */
function __efxQuadOptsJS(opts, sample) {
    var color = [1, 1, 1, 1];
    var rotation = 0, scale = 1;
    var src = [0, 0, 0, 0];
    var hasSrc = 0;
    var size = [0, 0];
    var hasSize = 0;
    var origin = [0, 0];
    var hasOrigin = 0;
    if (opts !== undefined) {
        var known = { color: 1, rotation: 1, scale: 1, sourceRect: 1,
                      size: 1, origin: 1 };
        __efxCheckKnown(opts, known, '', false, true);
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
            hasSize = 1;
        }
        var ov = opts['origin'];
        if (ov !== undefined) {
            origin = __efxFloatArray(ov, 2);
            hasOrigin = 1;
        }
        var srcv = opts['sourceRect'];
        if (srcv !== undefined) {
            src = __efxSourceRect(sample, srcv);
            hasSrc = 1;
        }
    }
    return { color: color, rotation: rotation, scale: scale, src: src,
             hasSrc: hasSrc, size: size, hasSize: hasSize, origin: origin,
             hasOrigin: hasOrigin };
}

/* spike-only sourceRect helper for __efxQuadOptsJS (same messages as the
 * web binding's __efxSourceRect; the sample size comes from liveSample) */
function __efxSourceRect(sample, v) {
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
        src[0] + src[2] > sample.w || src[1] + src[3] > sample.h) {
        throw new RangeError('sourceRect outside texture bounds');
    }
    return src;
}

function __efxPreludeInstall(efx, natives) {
    efx.mat4 = {
        identity: __efxM4Identity,
        perspective: __efxM4Perspective,
        ortho: __efxM4Ortho,
        translate: __efxM4Translate,
        rotate: __efxM4Rotate,
        scale: __efxM4Scale,
        multiply: __efxM4Mul,
    };
    efx.vec3 = {
        add: __efxV3Add,
        sub: __efxV3Sub,
        scale: __efxV3Scale,
        normalize: __efxV3Normalize,
        cross: __efxV3Cross,
        dot: __efxV3Dot,
    };
    efx.quat = {
        identity: __efxQuatIdentity,
        fromAxisAngle: __efxQuatFromAxisAngle,
        multiply: __efxQuatMultiply,
        toMat4: __efxQuatToMat4,
    };
    efx.makeCube = __efxMakeCube;
    efx.makePlane = __efxMakePlane;
    efx.makeSphere = __efxMakeSphere;
    efx.makeCapsule = __efxMakeCapsule;
    if (natives && natives.createParticleSystemWire) {
        efx.createParticleSystem = function (opts) {
            var parsed = __efxParticleWire(opts, natives.liveSample);
            return natives.createParticleSystemWire(parsed.wire, parsed.texture);
        };
        /* ---- R22 spike-only timing references (throwaway branch) ---- */
        if (natives.createParticleSystemOpts) {
            efx.createParticleSystemC = natives.createParticleSystemOpts;
            efx.__r22wireOnly = function (opts) {
                return __efxParticleWire(opts, natives.liveSample);
            };
        }
        if (natives.drawQuadUnpacked) {
            efx.drawQuadJS = function (x, y, tex, opts) {
                if (arguments.length < 3) {
                    throw new TypeError('drawQuad requires (x, y, texture, opts?)');
                }
                var fx = __efxNumber(x, 'x and y must be numbers');
                var fy = __efxNumber(y, 'x and y must be numbers');
                if (!isFinite(fx) || !isFinite(fy)) {
                    throw new RangeError('x and y must be finite');
                }
                var sample = natives.liveSample(tex);
                var texHandle = (sample !== null && typeof sample === 'object')
                    ? sample.handle : sample;
                var o = __efxQuadOptsJS(opts, sample);
                var fw, fh;
                if (o.hasSize) {
                    fw = o.size[0]; fh = o.size[1];
                } else if (o.hasSrc) {
                    fw = o.src[2]; fh = o.src[3];
                } else {
                    fw = sample.w; fh = sample.h;
                }
                var ox = o.hasOrigin ? o.origin[0] : fw * 0.5;
                var oy = o.hasOrigin ? o.origin[1] : fh * 0.5;
                natives.drawQuadUnpacked(texHandle, fx, fy, fw, fh,
                    o.color[0], o.color[1], o.color[2], o.color[3],
                    o.rotation, o.scale, o.src[0], o.src[1], o.src[2],
                    o.src[3], o.hasSrc, ox, oy);
            };
        }
        /* ---- end spike-only ---- */
    }
}

__efxPreludeInstall(efx, natives);

/* the bindings capture this factory and call it with `(efx, opts)` to create
   the shared module runtime; on web `opts` carries the shadowed host globals */
return __efxCreateModuleRuntime;
