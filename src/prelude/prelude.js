/* EFX engine-bundled pure-JS layer (F3).
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
    return efx.graphics.createMeshData([{
        positions: positions, normals: normals, uvs: uvs, indices: indices,
    }], __efxPrimMaterial(opts));
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
    return efx.graphics.createMeshData([{ positions: positions, uvs: uvs, indices: indices }],
        __efxPrimMaterial(opts));
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
    return efx.graphics.createMeshData([{
        positions: positions, normals: normals, uvs: uvs, indices: indices,
    }], __efxPrimMaterial(opts));
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
    return efx.graphics.createMeshData([{
        positions: positions, normals: normals, uvs: uvs, indices: indices,
    }], __efxPrimMaterial(opts));
}

/* ------------------------------------------ F10 CommonJS module runtime
 *
 * A small, synchronous, provider-backed CommonJS implementation shared by both
 * bindings (ADR 0037). `require(path)` loads a file under the resource root via
 * the synchronous `efx.io.loadText` provider, evaluates it in a wrapper scope that
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
            src = efx.io.loadText(norm);
        } catch (e) {
            src = null;
        }
        if (src !== null) {
            return { path: norm, source: src };
        }
        if (!/\.js$/.test(norm) && !/\.json$/.test(norm)) {
            try {
                src = efx.io.loadText(norm + '.js');
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
            src = efx.io.loadText(path);
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

function __efxFinite(v, typeMsg) {
    /* strict numbers (ADR 0049 D6): a number-typed field accepts only
     * typeof === 'number'; non-finite values keep the documented class */
    if (typeof v !== 'number') {
        throw new TypeError(typeMsg);
    }
    if (!isFinite(v)) {
        throw new TypeError(typeMsg);
    }
    return v;
}

/* throw for a non-zero native return code (ADR 0049 D4). `codes` maps each
 * code the call site handles to [ErrorClass, message], or to `true` for the
 * shared render messages; any other code throws Error('<where> failed') */
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

function __efxPartEnum(v, map, dflt, what, msg) {
    if (v === undefined) {
        return dflt;
    }
    if (typeof v !== 'string' || !Object.prototype.hasOwnProperty.call(map, v)) {
        throw new TypeError(msg || (what + ' has an unknown value'));
    }
    return map[v];
}

/* particle wire layout (floats); kept in sync with the desktop wire native
 * (api_particles.c) and src/web/bridge_particles.c */
var EFX_PART_WIRE_LEN = 352;

/* per-system option snapshots for ParticleSystem.set's merge (web parity:
 * set re-validates the merged bag; keyed weakly so wrappers stay
 * GC-finalized, ADR 0011) */
var __efxPsBags = new WeakMap();

/* shallow copy of a bag's own properties (the create snapshot must not
 * alias the caller's object) */
function __efxSnapshotOpts(opts) {
    var out = {};
    for (var k in opts) {
        if (Object.prototype.hasOwnProperty.call(opts, k)) {
            out[k] = opts[k];
        }
    }
    return out;
}

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

    /* enum messages follow the canonical desktop texts (ADR 0049 D5) */
    w[1] = __efxPartEnum(opts['space'], { world: 0, screen: 1 }, 0, 'space',
                         "space must be 'world' or 'screen'");
    w[2] = __efxPartEnum(opts['facing'],
                         { view: 0, y: 1, plane: 2 }, 0, 'facing',
                         "facing must be 'view', 'y', or 'plane'");
    if (w[1] === 1 && w[2] !== 0) {
        throw new TypeError("facing must be 'view' for screen space");
    }
    w[3] = __efxPartEnum(opts['blend'],
                         { alpha: 0, additive: 1, subtractive: 2 }, 0,
                         'blend',
                         "blend must be 'alpha', 'additive', or 'subtractive'");
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
        if (!Array.isArray(opts['colors'])) {
            throw new TypeError('colors must be a color or an array of colors');
        }
        var cols = (opts['colors'].length && Array.isArray(opts['colors'][0]))
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
            'emissionShape.shape', 'unknown emission shape');
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
            { top: 0, bottom: 1, random: 2 }, 0, 'insertMode',
            "insertMode must be 'top', 'bottom', or 'random'");
    }
    return { wire: w, texture: texHandle };
}

/* validate a sourceRect against a sample source's size -> [x, y, w, h]
 * (same messages as the bindings' __efxSourceRect; the size comes from the
 * binding-resolved sample) */
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

/* ------------------------------------------ post effects (F5b)
 *
 * One chain-entry validator for both runtimes (ADR 0049): the 9-float wire
 * layout is the desktop twin of src/web/bridge_target_post.c's reader.
 * Bounds stay engine-side (post_entry_valid); classes/messages match the
 * desktop binding. */

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

/* ------------------------------------------ fonts (F8a)
 *
 * The createFont option bag is validated once here (ADR 0049); the native
 * bakes the atlas. Layout options for drawText/measureText stay native
 * (hot path, ADR 0049). */

function __efxCreateFontOpts(natives, fontData, size, opts) {
    if (size === undefined) {
        throw new TypeError('createFont requires size');
    }
    var sz = __efxFinite(size, 'size must be a finite number');
    if (!(sz > 0)) {
        throw new RangeError('size must be > 0');
    }
    if (opts === undefined || opts === null) {
        opts = {};
    }
    if (!__efxIsObject(opts)) {
        throw new TypeError('createFont options must be an object');
    }
    __efxCheckKnown(opts, { glyphs: 1, padding: 1, filter: 1,
                            outline: 1, shadow: 1 }, 'createFont');
    var glyphs = opts['glyphs'];
    if (glyphs !== undefined) {
        if (typeof glyphs !== 'string') {
            throw new TypeError('glyphs must be a string');
        }
        if (glyphs.length === 0) {
            throw new RangeError('glyphs must not be empty');
        }
    }
    var padding = 1;
    if (opts['padding'] !== undefined) {
        var pv = __efxFinite(opts['padding'], 'padding must be a finite number');
        if (pv < 0 || pv !== Math.floor(pv)) {
            throw new RangeError('padding must be a non-negative integer');
        }
        padding = pv | 0;
    }
    var filter = 1;
    if (opts['filter'] !== undefined) {
        if (opts['filter'] === 'linear') {
            filter = 1;
        } else if (opts['filter'] === 'nearest') {
            filter = 0;
        } else {
            throw new TypeError("filter must be 'linear' or 'nearest'");
        }
    }
    var hasOutline = 0, outlineWidth = 0;
    if (opts['outline'] !== undefined && opts['outline'] !== null) {
        if (!__efxIsObject(opts['outline'])) {
            throw new TypeError('outline must be an object or null');
        }
        __efxCheckKnown(opts['outline'], { width: 1 }, 'outline');
        if (opts['outline']['width'] === undefined) {
            throw new TypeError('outline requires a numeric width');
        }
        outlineWidth = __efxFinite(opts['outline']['width'],
                                   'outline width must be a finite number');
        if (!(outlineWidth > 0)) {
            throw new RangeError('outline width must be > 0');
        }
        hasOutline = 1;
    }
    var hasShadow = 0, shadowBlur = 0, offX = 0, offY = 0;
    if (opts['shadow'] !== undefined && opts['shadow'] !== null) {
        if (!__efxIsObject(opts['shadow'])) {
            throw new TypeError('shadow must be an object or null');
        }
        __efxCheckKnown(opts['shadow'], { blur: 1, offset: 1 }, 'shadow');
        if (opts['shadow']['blur'] === undefined) {
            throw new TypeError('shadow requires a numeric blur');
        }
        shadowBlur = __efxFinite(opts['shadow']['blur'],
                                 'shadow blur must be a finite number');
        if (!(shadowBlur > 0)) {
            throw new RangeError('shadow blur must be > 0');
        }
        if (opts['shadow']['offset'] !== undefined) {
            var off = __efxFloatArray(opts['shadow']['offset'], 2);
            offX = off[0];
            offY = off[1];
        }
        hasShadow = 1;
    }
    return natives.createFont(fontData, sz,
                              glyphs !== undefined ? glyphs : null,
                              padding, filter, hasOutline, outlineWidth,
                              hasShadow, shadowBlur, offX, offY);
}

/* ------------------------------------------ physics (F12)
 *
 * Shape, body, character, static-mesh and query option validation lives once
 * here (ADR 0049); shapes marshal to the flat (type, radius, hx, hy, hz,
 * height, mesh) form the bindings' bridges already use. Numbers are strict
 * (D6); messages are the canonical texts of ADR 0049's table. */

function __efxPhysNumber(v, what) {
    if (typeof v !== 'number' || !isFinite(v)) {
        throw new TypeError(what + ' must be a finite number');
    }
    return v;
}

function __efxPhysMask(v, what) {
    if (typeof v !== 'number' || !isFinite(v)) {
        throw new TypeError(what + ' must be a finite number');
    }
    if (Math.floor(v) !== v || v < 0 || v > 4294967295) {
        throw new RangeError('layer/mask must be a 32-bit unsigned integer');
    }
    return v;
}

function __efxPhysVec3(v, what) {
    return __efxFloatArray(v, 3);
}

function __efxPhysShape(v, natives) {
    if (!__efxIsObject(v) || Array.isArray(v)) {
        throw new TypeError('shape must be an options object');
    }
    if (v['type'] === 'sphere') {
        __efxCheckKnown(v, ['type', 'radius'], 'shape', true);
        if (typeof v['radius'] !== 'number') {
            throw new TypeError('sphere shapes require a radius');
        }
        if (!(v['radius'] > 0)) {
            throw new RangeError('radius must be positive');
        }
        return { t: 0, r: v['radius'], hx: 0, hy: 0, hz: 0, height: 0,
                 mesh: null };
    }
    if (v['type'] === 'box') {
        __efxCheckKnown(v, ['type', 'size'], 'shape', true);
        var s = __efxFloatArray(v['size'], 3);
        if (!(s[0] > 0 && s[1] > 0 && s[2] > 0)) {
            throw new RangeError('box size components must be positive');
        }
        return { t: 1, r: 0, hx: s[0], hy: s[1], hz: s[2], height: 0,
                 mesh: null };
    }
    if (v['type'] === 'capsule') {
        __efxCheckKnown(v, ['type', 'radius', 'height'], 'shape', true);
        if (typeof v['radius'] !== 'number' || typeof v['height'] !== 'number') {
            throw new TypeError('capsule shapes require radius and height');
        }
        if (!(v['radius'] > 0)) {
            throw new RangeError('radius must be positive');
        }
        if (!(v['height'] >= 2 * v['radius'])) {
            throw new RangeError('capsule height must be at least 2 * radius');
        }
        return { t: 2, r: v['radius'], hx: 0, hy: 0, hz: 0, height: v['height'],
                 mesh: null };
    }
    if (v['type'] === 'mesh') {
        __efxCheckKnown(v, ['type', 'mesh'], 'shape', true);
        if (v['mesh'] === undefined) {
            throw new TypeError('expected a Mesh');
        }
        natives.checkMesh(v['mesh']);
        return { t: 3, r: 0, hx: 0, hy: 0, hz: 0, height: 0, mesh: v['mesh'] };
    }
    throw new TypeError('unknown shape type');
}

function __efxPhysCommonOpts(opts) {
    var sensor = !!opts['sensor'];
    var friction = opts['friction'] === undefined
        ? 0.5
        : __efxPhysNumber(opts['friction'], 'friction');
    var restitution = opts['restitution'] === undefined
        ? 0
        : __efxPhysNumber(opts['restitution'], 'restitution');
    if (friction < 0) {
        throw new RangeError('friction must not be negative');
    }
    if (restitution < 0 || restitution > 1) {
        throw new RangeError('restitution must be in [0, 1]');
    }
    var position = opts['position'] === undefined
        ? [0, 0, 0]
        : __efxPhysVec3(opts['position'], 'position');
    var layer = opts['layer'] === undefined ? 4294967295
                                            : __efxPhysMask(opts['layer'], 'layer');
    var mask = opts['mask'] === undefined ? 4294967295
                                          : __efxPhysMask(opts['mask'], 'mask');
    return { sensor: sensor, friction: friction, restitution: restitution,
             position: position, layer: layer, mask: mask };
}

/* ------------------------------------------ audio (F14)
 *
 * Loader argument checks and playAudio options live once here (ADR 0049);
 * loader failures come back as codes and map to the canonical messages
 * (D4): -1 unreadable, -2 undecodable. */

function __efxLoadAudio(natives, fn, path, what) {
    if (typeof path !== 'string') {
        throw new TypeError(what + ' requires a path string');
    }
    var r = fn(path);
    if (typeof r === 'number') {
        throw new Error((r === -1 ? 'cannot read audio: '
                                  : 'cannot decode audio: ') + path);
    }
    return r;
}

function __efxPlayAudio(natives, source, opts) {
    natives.checkAudioSource(source);
    var volume = 1, pan = 0, pitch = 1, loop = false;
    if (opts !== undefined && opts !== null) {
        if (!__efxIsObject(opts)) {
            throw new TypeError('playAudio options must be an object');
        }
        __efxCheckKnown(opts, ['volume', 'pan', 'pitch', 'loop'], 'playAudio');
        if (opts['volume'] !== undefined) {
            volume = __efxFinite(opts['volume'], 'volume must be a finite number');
            if (volume < 0) {
                throw new RangeError('volume must be a non-negative number');
            }
        }
        if (opts['pan'] !== undefined) {
            pan = __efxFinite(opts['pan'], 'pan must be a finite number');
        }
        if (opts['pitch'] !== undefined) {
            pitch = __efxFinite(opts['pitch'], 'pitch must be a finite number');
            if (pitch <= 0) {
                pitch = 1;
            }
        }
        if (opts['loop'] !== undefined) {
            if (typeof opts['loop'] !== 'boolean') {
                throw new TypeError('loop must be a boolean');
            }
            loop = opts['loop'];
        }
    }
    return natives.playAudio(source, volume, pan, pitch, loop);
}

/* ------------------------------------- resource construction (F2/F3/F5a/
 * F6a/F6b). The option bags are validated once here (ADR 0049); the natives
 * unpack the marshalled form. Loader failures come back as negative engine
 * error codes and map to the canonical messages (D4). */

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

/* createImageData: -> { w, h, bytes } (bytes is a fresh Uint8Array) */
function __efxImageDataOpts(width, height, pixels, opts) {
    var w = Number(width) | 0;
    var h = Number(height) | 0;
    if (w <= 0 || h <= 0) {
        throw new RangeError('width and height must be positive');
    }
    var n = w * h * 4;
    if (n > 0x7fffffff) {
        throw new RangeError('image too large');
    }
    if (pixels === undefined) {
        throw new TypeError('createImageData requires pixels');
    }
    var bytes;
    if (Array.isArray(pixels)) {
        bytes = new Uint8Array(n);
        for (var i = 0; i < n; i++) {
            var d = Number(pixels[i]);
            if (!(d >= 0 && d <= 255 && d === (d | 0))) {
                throw new RangeError('pixel bytes must be integers 0..255');
            }
            bytes[i] = d;
        }
    } else if (pixels instanceof Uint8Array) {
        if (pixels.length !== n) {
            throw new RangeError('pixels length must be width*height*4');
        }
        bytes = new Uint8Array(n);
        bytes.set(pixels);
    } else {
        throw new TypeError('pixels must be an array or typed array');
    }
    var fmt;
    if (opts !== undefined && opts !== null) {
        if (!__efxIsObject(opts)) {
            throw new TypeError('createImageData options must be an object');
        }
        var names = Object.getOwnPropertyNames(opts);
        for (var k = 0; k < names.length; k++) {
            if (names[k] !== 'format') {
                throw new TypeError("unknown option '" + names[k] + "'");
            }
        }
        fmt = opts['format'];
    }
    if (fmt !== undefined && String(fmt) !== 'rgba8') {
        throw new RangeError("unsupported image format (only 'rgba8')");
    }
    return { w: w, h: h, bytes: bytes };
}

/* createTexture options -> [wrap, filter, mipmaps] */
function __efxTextureOpts(opts) {
    var wrap = 0, filter = 1, mipmaps = 0;
    if (opts !== undefined && opts !== null) {
        if (!__efxIsObject(opts)) {
            throw new TypeError('createTexture options must be an object');
        }
        __efxCheckKnown(opts, { wrap: 1, filter: 1, mipmaps: 1 },
                        'createTexture');
        if (opts['wrap'] !== undefined) {
            if (opts['wrap'] === 'repeat') {
                wrap = 0;
            } else if (opts['wrap'] === 'clamp') {
                wrap = 1;
            } else if (opts['wrap'] === 'mirror') {
                wrap = 2;
            } else {
                throw new TypeError('unknown wrap mode');
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
        if (opts['mipmaps'] !== undefined) {
            if (typeof opts['mipmaps'] !== 'boolean') {
                throw new TypeError('mipmaps must be a boolean');
            }
            mipmaps = opts['mipmaps'] ? 1 : 0;
        }
    }
    return [wrap, filter, mipmaps];
}

/* createRenderTarget(width, height) -> [width, height] */
function __efxRenderTargetOpts(width, height) {
    var dims = [];
    var vals = [width, height];
    for (var k = 0; k < 2; k++) {
        var v = vals[k];
        if (v === undefined) {
            throw new TypeError('createRenderTarget requires width and height');
        }
        if (typeof v !== 'number' || !isFinite(v) || v <= 0 ||
            (v | 0) !== v || v > 4096) {
            throw new RangeError('width and height must be integers in 1..4096');
        }
        dims.push(v | 0);
    }
    return dims;
}

/* sample results are binding-shaped: the web returns {handle, w, h}, the
 * desktop a plain handle number (ADR 0049 D2) */
function __efxSampleHandle(sample) {
    return (sample !== null && typeof sample === 'object') ? sample.handle
                                                           : sample;
}

/* Phong material -> the 17-float block + 5 map-handles wire (the layout of
 * the bindings' material marshalling); `sample` resolves map resources */
function __efxMaterialWire(v, sample) {
    if (!__efxIsObject(v)) {
        throw new TypeError('material must be an object');
    }
    __efxCheckKnown(v, { ambient: 1, diffuse: 1, specular: 1, emissive: 1,
                         alphaMask: 1 }, 'material');
    var out = new Float32Array(17);
    out[0] = 0; out[1] = 0; out[2] = 0; out[3] = 1;     /* ambient */
    out[4] = 1; out[5] = 1; out[6] = 1; out[7] = 1;     /* diffuse */
    out[8] = 0; out[9] = 0; out[10] = 0; out[11] = 1;   /* specular */
    out[12] = 0; out[13] = 0; out[14] = 0; out[15] = 1; /* emissive */
    out[16] = 32;                                       /* shininess */
    var maps = new Float64Array(5);                     /* all absent (0) */
    var chan = ['ambient', 'diffuse', 'specular', 'emissive'];
    for (var ci = 0; ci < 4; ci++) {
        var ch = v[chan[ci]];
        if (ch === undefined || ch === null) {
            continue;
        }
        if (!__efxIsObject(ch)) {
            throw new TypeError(chan[ci] + ' channel must be an object');
        }
        __efxCheckKnown(ch, ci === 2 ? { color: 1, shininess: 1, map: 1 }
                                     : { color: 1, map: 1 }, chan[ci]);
        if (ch['color'] === undefined) {
            throw new TypeError(chan[ci] + ' channel requires color');
        }
        var c = __efxFloatArray(ch['color'], 4);
        out[ci * 4] = c[0];
        out[ci * 4 + 1] = c[1];
        out[ci * 4 + 2] = c[2];
        out[ci * 4 + 3] = c[3];
        if (ch['map'] !== undefined && ch['map'] !== null) {
            maps[ci] = __efxSampleHandle(sample(ch['map']));
        }
        if (ci === 2 && ch['shininess'] !== undefined) {
            if (typeof ch['shininess'] !== 'number') {
                throw new TypeError('shininess must be a number');
            }
            if (!isFinite(ch['shininess']) || ch['shininess'] <= 0) {
                throw new RangeError('shininess must be Finite and > 0');
            }
            out[16] = ch['shininess'];
        }
    }
    if (v['alphaMask'] !== undefined && v['alphaMask'] !== null) {
        maps[4] = __efxSampleHandle(sample(v['alphaMask']));
    }
    return { blocks: out, maps: maps };
}

/* createMeshData(surfaces, materials?): validate the positional surface list
 * and marshal the concatenated-buffers wire (7 attribute streams +
 * per-surface lengths + per-surface materials) */
function __efxMeshDataWire(surfaces, materials, natives) {
    if (surfaces === undefined) {
        throw new TypeError('createMeshData requires surfaces');
    }
    if (!Array.isArray(surfaces)) {
        throw new TypeError('surfaces must be an array');
    }
    if (surfaces.length < 1 || surfaces.length > 16) {
        throw new RangeError('surfaces must hold 1..16 entries');
    }
    var list = surfaces;
    var surfKnown = { positions: 1, normals: 1, uvs: 1, colors: 1,
                      joints: 1, weights: 1, indices: 1 };
    var count = list.length;
    var lens = new Int32Array(count * 7);
    var posAll = [], nrmAll = [], uvAll = [], colAll = [], jntAll = [];
    var wgtAll = [], idxAll = [];
    if (materials !== undefined && materials !== null) {
        if (!Array.isArray(materials)) {
            throw new TypeError('materials must be an array');
        }
        if (materials.length !== count) {
            throw new RangeError('materials must have one entry per surface');
        }
    } else {
        materials = undefined;
    }
    for (var i = 0; i < count; i++) {
        var sv = list[i];
        if (!__efxIsObject(sv)) {
            throw new TypeError('surfaces must be objects');
        }
        var names = Object.getOwnPropertyNames(sv);
        for (var k = 0; k < names.length; k++) {
            if (!surfKnown[names[k]]) {
                throw new TypeError("unknown surface option '" + names[k] + "'");
            }
        }
        if (sv['positions'] === undefined) {
            throw new TypeError('surface requires positions');
        }
        var pos = __efxFloat32Array(sv['positions'], 'positions');
        var nrm = sv['normals'] !== undefined
            ? __efxFloat32Array(sv['normals'], 'normals') : [];
        var uvs = sv['uvs'] !== undefined
            ? __efxFloat32Array(sv['uvs'], 'uvs') : [];
        var cols = sv['colors'] !== undefined
            ? __efxFloat32Array(sv['colors'], 'colors') : [];
        var joints = sv['joints'] !== undefined
            ? __efxUint32Array(sv['joints'], 'joints') : [];
        var weights = sv['weights'] !== undefined
            ? __efxFloat32Array(sv['weights'], 'weights') : [];
        var idx = sv['indices'] !== undefined
            ? __efxUint32Array(sv['indices']) : [];
        lens[i * 7] = pos.length;
        lens[i * 7 + 1] = nrm.length;
        lens[i * 7 + 2] = uvs.length;
        lens[i * 7 + 3] = cols.length;
        lens[i * 7 + 4] = joints.length;
        lens[i * 7 + 5] = weights.length;
        lens[i * 7 + 6] = idx.length;
        for (var p = 0; p < pos.length; p++) posAll.push(pos[p]);
        for (p = 0; p < nrm.length; p++) nrmAll.push(nrm[p]);
        for (p = 0; p < uvs.length; p++) uvAll.push(uvs[p]);
        for (p = 0; p < cols.length; p++) colAll.push(cols[p]);
        for (p = 0; p < joints.length; p++) jntAll.push(joints[p]);
        for (p = 0; p < weights.length; p++) wgtAll.push(weights[p]);
        for (p = 0; p < idx.length; p++) idxAll.push(idx[p]);
    }
    var blocks = null, maps = null, matHas = new Int32Array(count);
    if (materials !== undefined) {
        blocks = new Float32Array(count * 17);
        maps = new Float64Array(count * 5);
        for (var mi = 0; mi < count; mi++) {
            var mv = materials[mi];
            if (mv === null || mv === undefined) {
                continue;
            }
            var mf = __efxMaterialWire(mv, natives.liveSample);
            blocks.set(mf.blocks, mi * 17);
            maps.set(mf.maps, mi * 5);
            matHas[mi] = 1;
        }
    }
    return {
        count: count,
        lens: lens,
        pos: new Float32Array(posAll),
        nrm: new Float32Array(nrmAll),
        uv: new Float32Array(uvAll),
        col: new Float32Array(colAll),
        joints: new Uint32Array(jntAll),
        weights: new Float32Array(wgtAll),
        idx: new Uint32Array(idxAll),
        blocks: blocks,
        maps: maps,
        matHas: matHas,
    };
}

/* loader failure code -> canonical message (D4); codes are the negated
 * engine error enums */
var __efxResourceMsgs = {
    1: 'resource root could not be opened',
    2: 'resource not found',
    3: 'invalid resource path',
    4: 'resource read failed',
    5: 'out of memory',
    100: 'image decode failed',
};

var __efxGltfMsgs = {
    1: 'invalid or malformed glTF asset',
    2: 'glTF asset requires an unsupported extension',
    3: 'glTF mesh selection matched no mesh',
    4: 'glTF mesh exceeds the surface count limit',
    5: 'glTF image decode failed',
    6: 'out of memory',
    7: 'glTF resource could not be read',
};

/* ------------------------------------- lights and cameras (F4a/F2/F3)
 *
 * Set-once configuration APIs; validation lives once here (ADR 0049). The
 * vec3 length messages name the field (canonical web texts, ADR 0049 D5);
 * the light slot uses the single desktop message for every failure. */

function __efxVec3Field(v, what, lenMsg) {
    var out = __efxFloat32Array(v, what);
    if (out.length !== 3) {
        throw new RangeError(lenMsg || (what + ' must hold 3 numbers'));
    }
    return out;
}

function __efxLightSlot(slot) {
    if (typeof slot !== 'number' || !isFinite(slot) ||
        slot !== Math.floor(slot) || slot < 0 || slot > 3) {
        throw new RangeError('light slot must be an integer 0..3');
    }
    return slot | 0;
}

function __efxPreludeInstall(efx, natives) {
    /* graphics sub-namespace (ADR 0050): created by the bindings; the
     * prelude installs its members onto the same object in place. */
    var g = efx.graphics;
    if (!g) {
        g = {};
        efx.graphics = g;
    }
    efx.math = {
        mat4: {
            identity: __efxM4Identity,
            perspective: __efxM4Perspective,
            ortho: __efxM4Ortho,
            translate: __efxM4Translate,
            rotate: __efxM4Rotate,
            scale: __efxM4Scale,
            multiply: __efxM4Mul,
        },
        vec3: {
            add: __efxV3Add,
            sub: __efxV3Sub,
            scale: __efxV3Scale,
            normalize: __efxV3Normalize,
            cross: __efxV3Cross,
            dot: __efxV3Dot,
        },
        quat: {
            identity: __efxQuatIdentity,
            fromAxisAngle: __efxQuatFromAxisAngle,
            multiply: __efxQuatMultiply,
            toMat4: __efxQuatToMat4,
        },
    };
    /* named color constants (CSS basic 16 + transparent): frozen plain
     * [r, g, b, a] data, no functions. The 128/192 sRGB levels are
     * expressed as 0.5/0.75 for readability. */
    efx.color = {
        aqua: Object.freeze([0, 1, 1, 1]),
        black: Object.freeze([0, 0, 0, 1]),
        blue: Object.freeze([0, 0, 1, 1]),
        fuchsia: Object.freeze([1, 0, 1, 1]),
        gray: Object.freeze([0.5, 0.5, 0.5, 1]),
        green: Object.freeze([0, 0.5, 0, 1]),
        lime: Object.freeze([0, 1, 0, 1]),
        maroon: Object.freeze([0.5, 0, 0, 1]),
        navy: Object.freeze([0, 0, 0.5, 1]),
        olive: Object.freeze([0.5, 0.5, 0, 1]),
        purple: Object.freeze([0.5, 0, 0.5, 1]),
        red: Object.freeze([1, 0, 0, 1]),
        silver: Object.freeze([0.75, 0.75, 0.75, 1]),
        teal: Object.freeze([0, 0.5, 0.5, 1]),
        white: Object.freeze([1, 1, 1, 1]),
        yellow: Object.freeze([1, 1, 0, 1]),
        transparent: Object.freeze([0, 0, 0, 0]),
    };
    g.makeCube = __efxMakeCube;
    g.makePlane = __efxMakePlane;
    g.makeSphere = __efxMakeSphere;
    g.makeCapsule = __efxMakeCapsule;
    if (natives && natives.setPointLight) {
        g.setLight = function (slot, opts) {
            if (arguments.length < 2) {
                throw new TypeError('setLight requires (slot, opts)');
            }
            var s = __efxLightSlot(slot);
            if (opts === null || opts === undefined) {
                natives.setPointLight(s, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                return;
            }
            if (!__efxIsObject(opts)) {
                throw new TypeError('setLight options must be an object or null');
            }
            __efxCheckKnown(opts, { pos: 1, color: 1, range: 1 }, 'setLight');
            if (opts['pos'] === undefined) {
                throw new TypeError('setLight requires pos');
            }
            var pv = __efxVec3Field(opts['pos'], 'pos');
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
            natives.setPointLight(s, 1, pv[0], pv[1], pv[2],
                                  lc[0], lc[1], lc[2], lc[3], range);
        };
        g.setDirectionalLight = function (opts) {
            if (arguments.length < 1) {
                throw new TypeError('setDirectionalLight requires an options object or null');
            }
            if (opts === null || opts === undefined) {
                natives.setDirectionalLight(0, 0, 0, 0, 0, 0, 0, 0);
                return;
            }
            if (!__efxIsObject(opts)) {
                throw new TypeError('setDirectionalLight options must be an object or null');
            }
            __efxCheckKnown(opts, { dir: 1, color: 1 }, 'setDirectionalLight');
            if (opts['dir'] === undefined) {
                throw new TypeError('setDirectionalLight requires dir');
            }
            var dv = __efxVec3Field(opts['dir'], 'dir');
            if (dv[0] === 0 && dv[1] === 0 && dv[2] === 0) {
                throw new TypeError('dir must be non-zero');
            }
            if (opts['color'] === undefined) {
                throw new TypeError('setDirectionalLight requires color');
            }
            var dc = __efxFloatArray(opts['color'], 4);
            natives.setDirectionalLight(1, dv[0], dv[1], dv[2],
                                        dc[0], dc[1], dc[2], dc[3]);
        };
    }
    if (natives && natives.setCamera2D) {
        g.setCamera2D = function (opts) {
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
            natives.setCamera2D(frameW, frameH, x, y, zoom, rotation);
        };
    }
    if (natives && natives.setCamera3D) {
        g.setCamera3D = function (pos, target, fov, opts) {
            if (pos === undefined || target === undefined) {
                throw new TypeError('setCamera3D requires pos and target');
            }
            var p = __efxVec3Field(pos, 'pos', 'pos and target must hold 3 numbers');
            var t = __efxVec3Field(target, 'target',
                                   'pos and target must hold 3 numbers');
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
            if (opts !== undefined && opts !== null) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('setCamera3D options must be an object');
                }
                __efxCheckKnown(opts, { near: 1, far: 1 }, 'setCamera3D');
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
            }
            natives.setCamera3D(p[0], p[1], p[2], t[0], t[1], t[2], fov,
                                nearZ, farZ);
        };
    }
    if (natives && natives.createImageData) {
        g.createImageData = function (width, height, pixels, opts) {
            var im = __efxImageDataOpts(width, height, pixels, opts);
            return natives.createImageData(im.w, im.h, im.bytes);
        };
        g.createTexture = function (imageData, opts) {
            if (arguments.length < 1) {
                throw new TypeError('createTexture requires an ImageData');
            }
            natives.checkImageData(imageData);
            var t = __efxTextureOpts(opts);
            return natives.createTexture(imageData, t[0], t[1], t[2]);
        };
    }
    if (natives && natives.createRenderTarget) {
        g.createRenderTarget = function (width, height) {
            var dims = __efxRenderTargetOpts(width, height);
            return natives.createRenderTarget(dims[0], dims[1]);
        };
    }
    if (natives && natives.loadImage) {
        g.loadImage = function (path) {
            if (arguments.length < 1 || typeof path !== 'string') {
                throw new TypeError('loadImage requires a path string');
            }
            var r = natives.loadImage(path);
            if (typeof r === 'number') {
                throw new Error(__efxResourceMsgs[-r] || 'resource error');
            }
            return r;
        };
        g.loadMeshData = function (path, opts) {
            if (arguments.length < 1 || typeof path !== 'string') {
                throw new TypeError('loadMeshData requires a path string');
            }
            var hasMesh = 0, index = 0, name = null;
            if (opts !== undefined && opts !== null) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('loadMeshData options must be an object');
                }
                __efxCheckKnown(opts, { mesh: 1 }, 'loadMeshData');
                if (opts['mesh'] !== undefined) {
                    var mv = opts['mesh'];
                    hasMesh = 1;
                    if (typeof mv === 'string') {
                        name = mv;
                    } else if (typeof mv === 'number' && isFinite(mv) &&
                               mv === Math.floor(mv) && mv >= 0) {
                        index = mv | 0;
                    } else {
                        throw new TypeError('mesh must be a non-negative integer or a name');
                    }
                }
            }
            var r = natives.loadMeshData(path, hasMesh,
                                         name !== null ? 1 : 0, index, name);
            if (typeof r === 'number') {
                throw new Error(__efxGltfMsgs[-r] || 'glTF import failed');
            }
            return r;
        };
    }
    if (natives && natives.createMeshData) {
        g.createMeshData = function (surfaces, materials) {
            var w = __efxMeshDataWire(surfaces, materials, natives);
            return natives.createMeshData(w.count, w.lens, w.pos, w.nrm, w.uv,
                                          w.col, w.joints, w.weights, w.idx,
                                          w.blocks, w.maps, w.matHas);
        };
    }
    if (natives && natives.playAudio) {
        efx.audio.loadAudioData = function (path) {
            return __efxLoadAudio(natives, natives.loadAudioData, path,
                                  'loadAudioData');
        };
        efx.audio.loadAudioStream = function (path) {
            return __efxLoadAudio(natives, natives.loadAudioStream, path,
                                  'loadAudioStream');
        };
        efx.audio.playAudio = function (source, opts) {
            return __efxPlayAudio(natives, source, opts);
        };
    }
    if (natives && natives.physicsStep) {
        var phys = efx.physics;
        phys.step = function (dt) {
            if (arguments.length < 1 || typeof dt !== 'number' || !isFinite(dt)) {
                throw new TypeError('dt must be a finite number');
            }
            natives.physicsStep(dt);
        };
        phys.createBody = function (shape, opts) {
            if (shape === undefined) {
                throw new TypeError('shape must be an options object');
            }
            if (opts === undefined || opts === null) {
                opts = {};
            }
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('createBody requires an options object');
            }
            __efxCheckKnown(opts,
                ['dynamic', 'sensor', 'position', 'mass',
                 'friction', 'restitution', 'layer', 'mask'],
                'createBody', true);
            var sh = __efxPhysShape(shape, natives);
            var dynamic = !!opts['dynamic'];
            var mass = opts['mass'] === undefined
                ? 1
                : __efxPhysNumber(opts['mass'], 'mass');
            if (dynamic && !(mass > 0)) {
                throw new RangeError('dynamic bodies require a positive mass');
            }
            var c = __efxPhysCommonOpts(opts);
            return natives.createBody(dynamic, c.sensor ? 1 : 0, sh.t, sh.r,
                sh.hx, sh.hy, sh.hz, sh.height, c.position[0], c.position[1],
                c.position[2], mass, c.friction, c.restitution, c.layer,
                c.mask, sh.mesh);
        };
        phys.createStaticMesh = function (mesh, opts) {
            if (arguments.length < 1) {
                throw new TypeError('createStaticMesh requires a Mesh');
            }
            natives.checkMesh(mesh);
            opts = opts === undefined ? {} : opts;
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('createStaticMesh options must be an object');
            }
            __efxCheckKnown(opts,
                ['position', 'sensor', 'friction', 'restitution', 'layer',
                 'mask'],
                'createStaticMesh', true);
            var c = __efxPhysCommonOpts(opts);
            return natives.createStaticMesh(mesh, c.position[0],
                c.position[1], c.position[2], c.sensor ? 1 : 0, c.friction,
                c.restitution, c.layer, c.mask);
        };
        phys.createCharacter = function (radius, height, opts) {
            if (typeof radius !== 'number' || typeof height !== 'number') {
                throw new TypeError('createCharacter requires radius and height');
            }
            if (!(radius > 0)) {
                throw new RangeError('radius must be positive');
            }
            if (!(height >= 2 * radius)) {
                throw new RangeError('height must be at least 2 * radius');
            }
            if (opts === undefined || opts === null) {
                opts = {};
            }
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('createCharacter requires an options object');
            }
            __efxCheckKnown(opts,
                ['position', 'up', 'floorMaxAngle',
                 'floorSnapLength', 'stepHeight', 'maxSlides', 'safeMargin',
                 'layer', 'mask'],
                'createCharacter', true);
            var position = opts['position'] === undefined
                ? [0, 0, 0]
                : __efxPhysVec3(opts['position'], 'position');
            var up = opts['up'] === undefined
                ? [0, 1, 0]
                : __efxPhysVec3(opts['up'], 'up');
            if (up[0] === 0 && up[1] === 0 && up[2] === 0) {
                throw new RangeError('up must be non-zero');
            }
            var floorMaxAngle = opts['floorMaxAngle'] === undefined
                ? 45
                : __efxPhysNumber(opts['floorMaxAngle'], 'floorMaxAngle');
            var snap = opts['floorSnapLength'] === undefined
                ? 0.1
                : __efxPhysNumber(opts['floorSnapLength'], 'floorSnapLength');
            var step = opts['stepHeight'] === undefined
                ? 0.3
                : __efxPhysNumber(opts['stepHeight'], 'stepHeight');
            var safe = opts['safeMargin'] === undefined
                ? 0.001
                : __efxPhysNumber(opts['safeMargin'], 'safeMargin');
            var maxSlides = opts['maxSlides'] === undefined
                ? 6
                : __efxPhysNumber(opts['maxSlides'], 'maxSlides');
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
            var layer = opts['layer'] === undefined ? 4294967295
                                                    : __efxPhysMask(opts['layer'], 'layer');
            var mask = opts['mask'] === undefined ? 4294967295
                                                  : __efxPhysMask(opts['mask'], 'mask');
            return natives.createCharacter(radius, height,
                position[0], position[1], position[2], up[0], up[1], up[2],
                floorMaxAngle, snap, step, safe, maxSlides, layer, mask);
        };
        phys.raycast = function (origin, direction, maxDistance, opts) {
            if (arguments.length < 2) {
                throw new TypeError('raycast requires origin and direction');
            }
            var o = __efxPhysVec3(origin, 'origin');
            var d = __efxPhysVec3(direction, 'direction');
            if (typeof maxDistance !== 'number' || !isFinite(maxDistance) ||
                !(maxDistance > 0)) {
                throw new TypeError('raycast requires a positive maxDistance');
            }
            opts = opts === undefined || opts === null ? {} : opts;
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('raycast options must be an object');
            }
            __efxCheckKnown(opts, ['mask', 'all', 'sensors'],
                            'raycast', true);
            var mask = opts['mask'] === undefined ? 4294967295
                                                  : __efxPhysMask(opts['mask'], 'mask');
            return natives.raycast(o[0], o[1], o[2], d[0], d[1], d[2], maxDistance,
                                   mask, !!opts['sensors'], !!opts['all']);
        };
        phys.overlap = function (shape, opts) {
            if (arguments.length < 1) {
                throw new TypeError('overlap requires a shape');
            }
            var sh = __efxPhysShape(shape, natives);
            opts = opts === undefined ? {} : opts;
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('overlap options must be an object');
            }
            __efxCheckKnown(opts, ['position', 'mask'], 'overlap', true);
            var p = opts['position'] === undefined
                ? [0, 0, 0]
                : __efxPhysVec3(opts['position'], 'position');
            var mask = opts['mask'] === undefined ? 4294967295
                                                  : __efxPhysMask(opts['mask'], 'mask');
            return natives.overlap(sh.t, sh.r, sh.hx, sh.hy, sh.hz, sh.height,
                                   sh.mesh, p[0], p[1], p[2], mask);
        };
        phys.shapeCast = function (shape, from, motion, opts) {
            if (arguments.length < 3) {
                throw new TypeError('shapeCast requires shape, from and motion');
            }
            var sh = __efxPhysShape(shape, natives);
            var f = __efxPhysVec3(from, 'from');
            var m = __efxPhysVec3(motion, 'motion');
            opts = opts === undefined ? {} : opts;
            if (!__efxIsObject(opts) || Array.isArray(opts)) {
                throw new TypeError('shapeCast options must be an object');
            }
            __efxCheckKnown(opts, ['mask', 'sensors'], 'shapeCast', true);
            var mask = opts['mask'] === undefined ? 4294967295
                                                  : __efxPhysMask(opts['mask'], 'mask');
            return natives.shapeCast(sh.t, sh.r, sh.hx, sh.hy, sh.hz,
                                     sh.height, sh.mesh, f[0], f[1], f[2],
                                     m[0], m[1], m[2], mask,
                                     !!opts['sensors']);
        };
    }
    if (natives && natives.createFont) {
        g.createFont = function (fontData, size, opts) {
            if (arguments.length < 1) {
                throw new TypeError('createFont requires a FontData');
            }
            natives.checkFontData(fontData);
            return __efxCreateFontOpts(natives, fontData, size, opts);
        };
    }
    if (natives && natives.setPostEffects) {
        g.setPostEffects = function (list) {
            if (arguments.length < 1) {
                throw new TypeError('setPostEffects requires an array or null');
            }
            if (list === null || list === undefined) {
                natives.setPostEffects(null, 0);
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
                wire.set(__efxPostEntry(list[i]), i * 9);
            }
            natives.setPostEffects(wire, list.length);
        };
    }
    if (natives && natives.createParticleSystemWire) {
        g.createParticleSystem = function (texture, max, lifetime, opts) {
            var merged = { texture: texture, max: max, lifetime: lifetime };
            if (opts !== undefined && opts !== null) {
                if (!__efxIsObject(opts) || Array.isArray(opts)) {
                    throw new TypeError('createParticleSystem options must be an object');
                }
                for (var k in opts) {
                    if (Object.prototype.hasOwnProperty.call(opts, k)) {
                        merged[k] = opts[k];
                    }
                }
            }
            var parsed = __efxParticleWire(merged, natives.liveSample);
            var ps = natives.createParticleSystemWire(parsed.wire,
                                                      parsed.texture);
            __efxPsBags.set(ps, __efxSnapshotOpts(merged));
            return ps;
        };
        var psProto = natives.psProto();
        if (psProto) {
            /* set: merge over the create snapshot, re-validate the whole
             * bag through the wire, then hand the native the result */
            psProto.set = function (opts) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('set requires an options object');
                }
                var merged = {};
                var k;
                var prev = __efxPsBags.get(this);
                if (prev) {
                    for (k in prev) {
                        if (Object.prototype.hasOwnProperty.call(prev, k)) {
                            merged[k] = prev[k];
                        }
                    }
                }
                for (k in opts) {
                    if (Object.prototype.hasOwnProperty.call(opts, k)) {
                        merged[k] = opts[k];
                    }
                }
                var parsed = __efxParticleWire(merged, natives.liveSample);
                natives.psSet(this, parsed.wire, parsed.texture);
                __efxPsBags.set(this, merged);
            };
            /* speedScale writes must reach the snapshot too, or a later
             * set() would resurrect the creation-time value */
            var sd = Object.getOwnPropertyDescriptor(psProto, 'speedScale');
            if (sd && sd.set) {
                var nativeSetSpeed = sd.set;
                Object.defineProperty(psProto, 'speedScale', {
                    get: sd.get,
                    set: function (v) {
                        nativeSetSpeed.call(this, v);
                        var bag = __efxPsBags.get(this);
                        if (bag) {
                            bag['speedScale'] = v;
                        }
                    },
                });
            }
        }
    }
}

__efxPreludeInstall(efx, natives);

/* the bindings capture this factory and call it with `(efx, opts)` to create
   the shared module runtime; on web `opts` carries the shadowed host globals */
return __efxCreateModuleRuntime;
