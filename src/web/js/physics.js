    /* hot accessor helper (ADR 0049: stays with the native-backed accessors) */
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
                try {
                    if (!bridge['_efx_bridge_physics_character_move'](
                            this.__handle, m[0], m[1], m[2], ptr)) {
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
                    return {
                        position: pos, onFloor: onFloor, onWall: onWall,
                        onCeiling: onCeiling, floorNormal: fn, collisions: cols,
                    };
                } finally {
                    bridge['_free'](ptr);
                }
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

    api.physics = {
        /* createBody/createStaticMesh/createCharacter/step/raycast/overlap/
         * shapeCast are installed by the shared prelude (ADR 0049) */
        clear: function () {
            bridge['_efx_bridge_physics_clear']();
            physBodies.clear();
            physCharacters.clear();
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

