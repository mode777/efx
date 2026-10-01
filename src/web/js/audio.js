    /* --------------------------------------------- F14 audio */

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

    api.audio = {
        /* loadAudioData/loadAudioStream/playAudio are installed by the
         * shared prelude (ADR 0049) */
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
        setPointLight: function (slot, enabled, px, py, pz, r, g, b, a, range) {
            bridge['_efx_bridge_set_point_light'](slot, enabled, px, py, pz,
                                                  r, g, b, a, range);
        },
        setDirectionalLight: function (enabled, dx, dy, dz, r, g, b, a) {
            bridge['_efx_bridge_set_directional_light'](enabled, dx, dy, dz,
                                                        r, g, b, a);
        },
        setCamera2D: function (frameW, frameH, x, y, zoom, rotation) {
            bridge['_efx_bridge_set_camera'](frameW, frameH, x, y, zoom,
                                             rotation);
        },
        setCamera3D: function (px, py, pz, tx, ty, tz, fov, nearZ, farZ) {
            bridge['_efx_bridge_set_camera3d'](px, py, pz, tx, ty, tz, fov,
                                               nearZ, farZ);
        },
        checkImageData: function (v) {
            liveImageData(v);
        },
        createImageData: function (w, h, bytes) {
            var n = w * h * 4;
            var ptr = bridge['_efx_bridge_imagedata_alloc'](n);
            if (!ptr) {
                throw new Error('out of memory');
            }
            try {
                HEAPU8.set(bytes, ptr);
                var id = bridge['_efx_bridge_imagedata_commit'](w, h, ptr);
                if (!id) {
                    throw new Error('out of memory');
                }
                return new EfxImageData(id);
            } catch (e) {
                bridge['_free'](ptr); /* not committed: still ours */
                throw e;
            }
        },
        createTexture: function (image, wrap, filter, mipmaps) {
            var handle = bridge['_efx_bridge_texture_create'](
                image.__id, wrap, filter, mipmaps);
            if (!handle) {
                throw new Error('texture upload failed (no GPU context?)');
            }
            return new EfxTexture(handle, false);
        },
        createRenderTarget: function (w, h) {
            var handle = bridge['_efx_bridge_target_create'](w, h);
            if (!handle) {
                throw new Error('render target creation failed (no GPU context?)');
            }
            return new EfxRenderTarget(handle);
        },
        loadImage: function (path) {
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_load_image'](p);
            bridge['_free'](p);
            if (id < 0) {
                return id;
            }
            if (!id) {
                return -100;
            }
            return new EfxImageData(id);
        },
        loadMeshData: function (path, hasMesh, isName, index, name) {
            var pathPtr = __efxAllocCStr(path);
            var namePtr = name !== null && name !== undefined
                ? __efxAllocCStr(name) : 0;
            var id = bridge['_efx_bridge_load_meshdata'](pathPtr, hasMesh,
                                                         isName, index,
                                                         namePtr);
            bridge['_free'](pathPtr);
            bridge['_free'](namePtr);
            if (id < 0) {
                return id;
            }
            if (!id) {
                return -1;
            }
            return new EfxMeshData(id);
        },
        createMeshData: function (count, lens, pos, nrm, uv, col, joints,
                                  weights, idx, blocks, maps, matHas) {
            var id = bridge['_efx_bridge_meshdata_create'](count);
            if (!id) {
                throw new Error('out of memory');
            }
            var bad = 0;
            try {
                var L = lens, o = [0, 0, 0, 0, 0, 0, 0];
                for (var i = 0; i < count; i++) {
                    var pPtr = mallocCopyF32(pos.subarray(o[0], o[0] += L[i * 7]));
                    var nPtr = mallocCopyF32(nrm.subarray(o[1], o[1] += L[i * 7 + 1]));
                    var uPtr = mallocCopyF32(uv.subarray(o[2], o[2] += L[i * 7 + 2]));
                    var cPtr = mallocCopyF32(col.subarray(o[3], o[3] += L[i * 7 + 3]));
                    var jPtr = mallocCopyU32(joints.subarray(o[4], o[4] += L[i * 7 + 4]));
                    var wPtr = mallocCopyF32(weights.subarray(o[5], o[5] += L[i * 7 + 5]));
                    var iPtr = mallocCopyU32(idx.subarray(o[6], o[6] += L[i * 7 + 6]));
                    var rc = bridge['_efx_bridge_meshdata_surface'](id, i,
                        pPtr, L[i * 7], nPtr, L[i * 7 + 1], uPtr, L[i * 7 + 2],
                        cPtr, L[i * 7 + 3], jPtr, L[i * 7 + 4], wPtr,
                        L[i * 7 + 5], iPtr, L[i * 7 + 6]);
                    bridge['_free'](pPtr);
                    bridge['_free'](nPtr);
                    bridge['_free'](uPtr);
                    bridge['_free'](cPtr);
                    bridge['_free'](jPtr);
                    bridge['_free'](wPtr);
                    bridge['_free'](iPtr);
                    if (rc !== 0) {
                        bad = 1; /* deferred: materials errors come first (D3) */
                    }
                }
                if (blocks) {
                    for (var mi = 0; mi < count; mi++) {
                        if (!matHas[mi]) {
                            continue;
                        }
                        var mptr = mallocCopyF32(blocks.subarray(mi * 17,
                                                                 mi * 17 + 17));
                        var mapsptr = mallocCopyF64(maps.subarray(mi * 5,
                                                                  mi * 5 + 5));
                        bridge['_efx_bridge_meshdata_set_material'](
                            id, mi, mptr, mapsptr, 1);
                        bridge['_free'](mptr);
                        bridge['_free'](mapsptr);
                    }
                }
                var crc = bridge['_efx_bridge_meshdata_commit'](id);
                if (bad || crc !== 0) {
                    throw new RangeError('invalid mesh data');
                }
                return new EfxMeshData(id);
            } catch (e) {
                bridge['_efx_bridge_meshdata_destroy'](id);
                throw e;
            }
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
        checkAudioSource: function (v) {
            if (v instanceof EfxAudioData) {
                if (!v.__alive) {
                    throw new Error('AudioData was destroyed');
                }
                return;
            }
            if (v instanceof EfxAudioStream) {
                if (!v.__alive) {
                    throw new Error('AudioStream was destroyed');
                }
                return;
            }
            throw new TypeError('playAudio requires an AudioData or AudioStream');
        },
        loadAudioData: function (path) {
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_audio_load_data'](p);
            bridge['_free'](p);
            if (id < 0) {
                return id;
            }
            if (!id) {
                return -2;
            }
            return new EfxAudioData(id);
        },
        loadAudioStream: function (path) {
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_audio_load_stream'](p);
            bridge['_free'](p);
            if (id < 0) {
                return id;
            }
            if (!id) {
                return -2;
            }
            return new EfxAudioStream(id);
        },
        playAudio: function (source, volume, pan, pitch, loop) {
            var playFn = (source instanceof EfxAudioData)
                ? bridge['_efx_bridge_audio_play_data']
                : bridge['_efx_bridge_audio_play_stream'];
            var id = playFn(source.__id, volume, pan, pitch, loop ? 1 : 0);
            if (!id) {
                return null;
            }
            return new EfxAudio(id, volume, pan, pitch, loop);
        },
    };
    var preludeSrc = UTF8ToString(bridge['_efx_bridge_js_prelude']());
    st.createModuleRuntime = new Function('efx', 'natives', preludeSrc)(api, natives);

    globalThis['efx'] = api;
    st.api = api;
    st.quitSentinel = new Object();
    return st;
}
