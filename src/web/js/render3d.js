                /* setCamera3D is installed by the shared prelude (ADR 0049) */
                /* createMeshData is installed by the shared prelude (ADR 0049) */
        screenRay: function (x, y) {
            if (arguments.length < 2) {
                throw new TypeError('screenRay requires (x, y)');
            }
            /* same coercion + messages as the desktop binding */
            x = Number(x);
            y = Number(y);
            if (!isFinite(x) || !isFinite(y)) {
                throw new RangeError('x and y must be finite');
            }
            /* the point is in the space mouse input reports: the active 2D
               frame, falling back to the surface when no frame was set
               (batched input state: [..., w, h, dpi, frameW, frameH]) */
            var buf = __efxScratch();
            bridge['_efx_bridge_input_state'](buf);
            var fw = HEAPF64[(buf >> 3) + 9];
            var fh = HEAPF64[(buf >> 3) + 10];
            if (!(fw > 0) || !(fh > 0)) {
                fw = HEAPF64[(buf >> 3) + 6];
                fh = HEAPF64[(buf >> 3) + 7];
            }
            var obuf = bridge['_malloc'](6 * 8);
            var rc = bridge['_efx_bridge_screen_ray'](x, y, fw, fh, obuf);
            if (rc !== 0) {
                bridge['_free'](obuf);
                throw new Error(rc === -1
                    ? 'screenRay requires a 3D camera'
                    : 'screenRay: degenerate 3D camera');
            }
            var base = obuf >> 3;
            var out = {
                origin: [HEAPF64[base], HEAPF64[base + 1], HEAPF64[base + 2]],
                direction: [HEAPF64[base + 3], HEAPF64[base + 4],
                            HEAPF64[base + 5]],
            };
            bridge['_free'](obuf);
            return out;
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
            var transform = null, color = null, skinned = 0, depthWrite = 1;
            if (arguments.length >= 2 && opts !== undefined) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('drawMesh options must be an object');
                }
                var known = { transform: 1, color: 1, skinned: 1, depthWrite: 1 };
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
                var dw = opts['depthWrite'];
                if (dw !== undefined) {
                    if (typeof dw !== 'boolean') {
                        throw new TypeError('depthWrite must be a boolean');
                    }
                    depthWrite = dw ? 1 : 0;
                }
            }
            var tPtr = 0, cPtr = 0;
            if (transform !== null || color !== null) {
                var base = __efxScratch();
                if (transform !== null) {
                    tPtr = base;
                    HEAPF32.set(transform, tPtr >> 2);
                }
                if (color !== null) {
                    cPtr = base + 16 * 4;
                    HEAPF32.set(color, cPtr >> 2);
                }
            }
            var rc = bridge['_efx_bridge_draw_mesh'](m.__handle, tPtr, cPtr, skinned,
                                                    depthWrite);
            __efxRc(rc, 'drawMesh', {
                1: true,
                2: [TypeError, 'expected a live Mesh'],
                9: true,
                11: [TypeError, 'mesh has no rig to draw skinned'],
            });
        },
        /* pose is a Mesh prototype method (ADR 0055) */
                /* setLight/setDirectionalLight are installed by the shared prelude (ADR 0049) */
                /* setSurfaceMaterial is a Mesh prototype method (ADR 0055) */
        }, /* efx.graphics (ADR 0050) */
    };

