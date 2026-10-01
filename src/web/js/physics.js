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
    function __physBodyVec3(fn, handle) {
        var ptr = bridge['_malloc'](12);
        var base = ptr >> 2;
        fn(handle, ptr);
        var out = [HEAPF32[base], HEAPF32[base + 1], HEAPF32[base + 2]];
        bridge['_free'](ptr);
        return out;
    }

    var EfxBody = __efxResourceClass('EfxBody', {
        typeMsg: 'expected a Body',
        deadMsg: 'using a destroyed Body',
        notObjectMsg: 'expected a Body',
        init: function (handle) {
            this.__handle = handle;
        },
        destroy: function () {
            bridge['_efx_bridge_physics_body_destroy'](this.__handle);
            physBodies['delete'](this.__handle);
        },
        methods: {
            applyImpulse: function (v) {
                var a = __efxFloatArray(v, 3);
                if (!bridge['_efx_bridge_physics_body_apply_impulse'](
                        this.__handle, a[0], a[1], a[2])) {
                    throw new TypeError('applyImpulse requires a dynamic body');
                }
            },
            applyForce: function (v) {
                var a = __efxFloatArray(v, 3);
                if (!bridge['_efx_bridge_physics_body_apply_force'](
                        this.__handle, a[0], a[1], a[2])) {
                    throw new TypeError('applyForce requires a dynamic body');
                }
            },
        },
        getters: {
            position: {
                get: function () {
                    return __physBodyVec3(
                        bridge['_efx_bridge_physics_body_position'],
                        this.__handle);
                },
            },
            velocity: {
                get: function () {
                    return __physBodyVec3(
                        bridge['_efx_bridge_physics_body_get_velocity'],
                        this.__handle);
                },
                set: function (v) {
                    var a = __efxFloatArray(v, 3);
                    bridge['_efx_bridge_physics_body_set_velocity'](
                        this.__handle, a[0], a[1], a[2]);
                },
            },
            transform: {
                get: function () {
                    var p = __physBodyVec3(
                        bridge['_efx_bridge_physics_body_position'],
                        this.__handle);
                    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, p[0], p[1], p[2], 1];
                },
            },
            contacts: {
                get: function () {
                    var n = bridge['_efx_bridge_physics_body_contact_count'](this.__handle);
                    var out = [];
                    if (n > 0) {
                        var ptr = bridge['_malloc'](11 * 8);
                        var base = ptr >> 3;
                        for (var i = 0; i < n; i++) {
                            if (!bridge['_efx_bridge_physics_body_contact'](
                                    this.__handle, i, ptr)) {
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
                        bridge['_free'](ptr);
                    }
                    return out;
                },
            },
        },
    });

    var EfxCharacter = __efxResourceClass('EfxCharacter', {
        typeMsg: 'expected a Character',
        deadMsg: 'using a destroyed Character',
        notObjectMsg: 'expected a Character',
        init: function (handle) {
            this.__handle = handle;
        },
        destroy: function () {
            bridge['_efx_bridge_physics_character_destroy'](this.__handle);
            physCharacters['delete'](this.__handle);
        },
        methods: {
            moveAndSlide: function (motion) {
                var m = __efxFloatArray(motion, 3);
                var ptr = bridge['_malloc'](10 * 8);
                var base = ptr >> 3;
                if (!bridge['_efx_bridge_physics_character_move'](
                        this.__handle, m[0], m[1], m[2], ptr)) {
                    bridge['_free'](ptr);
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
                            this.__handle, i, ptr)) {
                        continue;
                    }
                    cols.push({
                        body: physBodies.get(HEAPF64[base]) || null,
                        normal: [HEAPF64[base + 1], HEAPF64[base + 2], HEAPF64[base + 3]],
                        point: [HEAPF64[base + 4], HEAPF64[base + 5], HEAPF64[base + 6]],
                    });
                }
                bridge['_free'](ptr);
                return {
                    position: pos, onFloor: onFloor, onWall: onWall,
                    onCeiling: onCeiling, floorNormal: fn, collisions: cols,
                };
            },
        },
        getters: {
            position: {
                get: function () {
                    return __physBodyVec3(
                        bridge['_efx_bridge_physics_character_position'],
                        this.__handle);
                },
            },
            velocity: {
                get: function () {
                    return __physBodyVec3(
                        bridge['_efx_bridge_physics_character_get_velocity'],
                        this.__handle);
                },
                set: function (v) {
                    var a = __efxFloatArray(v, 3);
                    bridge['_efx_bridge_physics_character_set_velocity'](
                        this.__handle, a[0], a[1], a[2]);
                },
            },
            onFloor: {
                get: function () {
                    return !!bridge['_efx_bridge_physics_character_on_floor'](
                        this.__handle);
                },
            },
        },
    });

    function __physBodyCommonOpts(opts) {
        var sensor = opts.sensor === undefined ? false : !!opts.sensor;
        var friction = opts.friction === undefined ? 0.5
                                                   : __efxFiniteNumber(opts.friction, 'friction');
        var restitution = opts.restitution === undefined
                              ? 0
                              : __efxFiniteNumber(opts.restitution, 'restitution');
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
                                               : __efxFiniteNumber(opts.mass, 'mass');
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
                                    : __efxFiniteNumber(opts.floorMaxAngle, 'floorMaxAngle');
            var snap = opts.floorSnapLength === undefined
                           ? 0.1
                           : __efxFiniteNumber(opts.floorSnapLength, 'floorSnapLength');
            var step = opts.stepHeight === undefined
                           ? 0.3
                           : __efxFiniteNumber(opts.stepHeight, 'stepHeight');
            var safe = opts.safeMargin === undefined
                           ? 0.001
                           : __efxFiniteNumber(opts.safeMargin, 'safeMargin');
            var maxSlides = opts.maxSlides === undefined
                                ? 6
                                : __efxFiniteNumber(opts.maxSlides, 'maxSlides');
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
            bridge['_free'](ptr);
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
            bridge['_free'](ptr);
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
                bridge['_free'](ptr);
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
            bridge['_free'](ptr);
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

