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

    var EfxAudioData = __efxResourceClass('EfxAudioData', {
        init: function (id) {
            this.__id = id;
        },
        destroy: function () {
            bridge['_efx_bridge_audio_data_destroy'](this.__id);
        },
    });

    var EfxAudioStream = __efxResourceClass('EfxAudioStream', {
        init: function (id) {
            this.__id = id;
        },
        destroy: function () {
            bridge['_efx_bridge_audio_stream_destroy'](this.__id);
        },
    });

    /* F14: a playback handle. Its members tolerate a dead id (queries read
       false, verbs no-op) rather than throwing, so they are all raw. */
    var EfxAudio = __efxResourceClass('EfxAudio', {
        init: function (id, volume, pan, pitch, loop) {
            this.__id = id;
            this.__volume = volume;
            this.__pan = pan;
            this.__pitch = pitch;
            this.__loop = loop;
        },
        destroy: function () {
            bridge['_efx_bridge_audio_handle_destroy'](this.__id);
        },
        methods: {
            stop: { raw: true, fn: function () {
                if (this.__alive) {
                    bridge['_efx_bridge_audio_handle_stop'](this.__id);
                }
            } },
            pause: { raw: true, fn: function () {
                if (this.__alive) {
                    bridge['_efx_bridge_audio_handle_pause'](this.__id, 1);
                }
            } },
            resume: { raw: true, fn: function () {
                if (this.__alive) {
                    bridge['_efx_bridge_audio_handle_pause'](this.__id, 0);
                }
            } },
        },
        getters: {
            playing: { raw: true, get: function () {
                return this.__alive &&
                    !!bridge['_efx_bridge_audio_handle_playing'](this.__id);
            } },
            paused: { raw: true, get: function () {
                return this.__alive &&
                    !!bridge['_efx_bridge_audio_handle_paused'](this.__id);
            } },
            volume: {
                raw: true,
                get: function () { return this.__volume; },
                set: function (v) {
                    var n = __efxFiniteNumber(v, 'volume');
                    if (n < 0) throw new RangeError('volume must be a non-negative number');
                    this.__volume = n;
                    bridge['_efx_bridge_audio_handle_set_volume'](this.__id, n);
                },
            },
            pan: {
                raw: true,
                get: function () { return this.__pan; },
                set: function (v) {
                    var n = __efxFiniteNumber(v, 'pan');
                    this.__pan = n;
                    bridge['_efx_bridge_audio_handle_set_pan'](this.__id, n);
                },
            },
            pitch: {
                raw: true,
                get: function () { return this.__pitch; },
                set: function (v) {
                    var n = __efxFiniteNumber(v, 'pitch');
                    if (n <= 0) throw new RangeError('pitch must be a positive number');
                    this.__pitch = n;
                    bridge['_efx_bridge_audio_handle_set_pitch'](this.__id, n);
                },
            },
            loop: {
                raw: true,
                get: function () { return this.__loop; },
                set: function (v) {
                    this.__loop = !!v;
                    bridge['_efx_bridge_audio_handle_set_loop'](this.__id, v ? 1 : 0);
                },
            },
        },
    });

    function __efxAudioPlay(playFn, src, opts) {
        var o = __efxAudioOpts(opts, ['volume', 'pan', 'pitch', 'loop'],
                               'playAudio');
        var volume = (o.volume === undefined) ? 1
            : __efxFiniteNumber(o.volume, 'volume');
        if (volume < 0) {
            throw new RangeError('volume must be a non-negative number');
        }
        var pan = (o.pan === undefined) ? 0 : __efxFiniteNumber(o.pan, 'pan');
        var pitch = (o.pitch === undefined) ? 1 : __efxFiniteNumber(o.pitch, 'pitch');
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
            bridge['_free'](p);
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
            bridge['_free'](p);
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
            bridge['_efx_audio_request_resume']();
        },
    };
    Object.defineProperty(api.audio, 'volume', {
        get: function () {
            return bridge['_efx_audio_master_volume']();
        },
        set: function (v) {
            var n = __efxFiniteNumber(v, 'volume');
            if (n < 0) throw new RangeError('volume must be a non-negative number');
            bridge['_efx_audio_set_master_volume'](n);
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
       shadow. The second parameter is the binding-provided natives object
       (ADR 0049): the shared prelude validators call these to resolve
       resources and marshal normalized forms. */
    var natives = {
        liveSample: liveSample,
        psProto: EfxParticleSystem.prototype,
        createParticleSystemWire: function (wire, texHandle) {
            var ptr = mallocCopyF32(wire);
            var handle = bridge['_efx_bridge_particles_create'](ptr, texHandle);
            bridge['_free'](ptr);
            if (!handle) {
                throw new RangeError('invalid particle configuration');
            }
            return new EfxParticleSystem(handle);
        },
        psSet: function (v, wire, texHandle) {
            if (!v.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            var ptr = mallocCopyF32(wire);
            var rc = bridge['_efx_bridge_particles_set'](v.__handle, ptr,
                                                         texHandle);
            bridge['_free'](ptr);
            if (rc !== 0) {
                throw new RangeError('invalid particle configuration');
            }
        },
        setPostEffects: function (wire, count) {
            var ptr = wire ? mallocCopyF32(wire) : 0;
            var rc = bridge['_efx_bridge_set_post_effects'](ptr, count);
            if (ptr) {
                bridge['_free'](ptr);
            }
            __efxRc(rc, 'setPostEffects', {
                1: [TypeError, 'unknown post effect'],
                2: [RangeError, 'post-effect chain is limited to 8 entries'],
                3: [RangeError, 'post-effect option out of range'],
            });
        },
        checkFontData: function (v) {
            if (!(v instanceof EfxFontData)) {
                throw new TypeError('createFont requires a FontData');
            }
            if (!v.__alive) {
                throw new TypeError('using a destroyed resource');
            }
        },
        createFont: function (fontData, size, glyphs, padding, filter,
                              hasOutline, outlineWidth, hasShadow,
                              shadowBlur, offX, offY) {
            var glyphsPtr = glyphs !== null && glyphs !== undefined
                ? __efxAllocCStr(glyphs) : 0;
            var id = bridge['_efx_bridge_create_font'](
                fontData.__id, size, glyphsPtr, padding, filter, hasOutline,
                outlineWidth, hasShadow, shadowBlur, offX, offY);
            bridge['_free'](glyphsPtr);
            if (!id) {
                throw new Error('font could not be baked');
            }
            return new EfxFont(id);
        },
        checkMesh: function (v) {
            liveMesh(v);
        },
        createBody: function (dynamic, sensor, t, r, hx, hy, hz, height,
                              px, py, pz, mass, friction, restitution,
                              layer, mask, mesh) {
            var handle;
            if (t === 3) {
                handle = bridge['_efx_bridge_physics_create_static_mesh'](
                    mesh.__handle, px, py, pz, sensor, friction, restitution,
                    layer, mask);
            } else {
                handle = bridge['_efx_bridge_physics_create_body'](
                    dynamic, sensor, t, r, hx, hy, hz, height, px, py, pz,
                    mass, friction, restitution, layer, mask);
            }
            if (!handle) {
                throw new Error('failed to create body');
            }
            var b = new EfxBody(handle);
            physBodies.set(handle, b);
            return b;
        },
        createStaticMesh: function (mesh, px, py, pz, sensor, friction,
                                    restitution, layer, mask) {
            var handle = bridge['_efx_bridge_physics_create_static_mesh'](
                mesh.__handle, px, py, pz, sensor, friction, restitution,
                layer, mask);
            if (!handle) {
                throw new Error('failed to create mesh collider');
            }
            var b = new EfxBody(handle);
            physBodies.set(handle, b);
            return b;
        },
        createCharacter: function (radius, height, px, py, pz, ux, uy, uz,
                                   floorMaxAngle, snap, step, safe,
                                   maxSlides, layer, mask) {
            var handle = bridge['_efx_bridge_physics_create_character'](
                radius, height, px, py, pz, ux, uy, uz, floorMaxAngle, snap,
                step, safe, maxSlides, layer, mask);
            if (!handle) {
                throw new Error('failed to create character');
            }
            var c = new EfxCharacter(handle);
            physCharacters.set(handle, c);
            return c;
        },
        physicsStep: function (dt) {
            bridge['_efx_bridge_physics_step'](dt);
        },
        raycast: function (ox, oy, oz, dx, dy, dz, maxd, mask, sensors, all) {
            var count = bridge['_efx_bridge_physics_raycast'](
                ox, oy, oz, dx, dy, dz, maxd, mask, sensors ? 1 : 0,
                all ? 1 : 0, 0);
            if (count <= 0) {
                return all ? [] : null;
            }
            var ptr = bridge['_malloc'](count * 10 * 8);
            var base = ptr >> 3;
            bridge['_efx_bridge_physics_raycast'](
                ox, oy, oz, dx, dy, dz, maxd, mask, sensors ? 1 : 0,
                all ? 1 : 0, ptr);
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
        overlap: function (t, r, hx, hy, hz, height, mesh, px, py, pz, mask) {
            var meshHandle = mesh ? mesh.__handle : 0;
            var count = bridge['_efx_bridge_physics_overlap'](
                t, r, hx, hy, hz, height, meshHandle, px, py, pz, mask, 0);
            if (count <= 0) {
                return [];
            }
            var ptr = bridge['_malloc'](count * 3 * 8);
            var base = ptr >> 3;
            bridge['_efx_bridge_physics_overlap'](
                t, r, hx, hy, hz, height, meshHandle, px, py, pz, mask, ptr);
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
        shapeCast: function (t, r, hx, hy, hz, height, mesh, fx, fy, fz,
                             mx, my, mz, mask, sensors) {
            var meshHandle = mesh ? mesh.__handle : 0;
            var ptr = bridge['_malloc'](10 * 8);
            var base = ptr >> 3;
            var rc = bridge['_efx_bridge_physics_shape_cast'](
                t, r, hx, hy, hz, height, meshHandle, fx, fy, fz, mx, my, mz,
                mask, sensors ? 1 : 0, ptr);
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
    var preludeSrc = UTF8ToString(bridge['_efx_bridge_js_prelude']());
    st.createModuleRuntime = new Function('efx', 'natives', preludeSrc)(api, natives);

    globalThis['efx'] = api;
    st.api = api;
    st.quitSentinel = new Object();
    return st;
}
