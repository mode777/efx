                /* setCamera3D is installed by the shared prelude (ADR 0049) */
                /* createMeshData is installed by the shared prelude (ADR 0049) */
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
            var rc = bridge['_efx_bridge_draw_mesh'](m.__handle, tPtr, cPtr, skinned);
            __efxRc(rc, 'drawMesh', {
                1: true,
                2: [TypeError, 'expected a live Mesh'],
                9: true,
                11: [TypeError, 'mesh has no rig to draw skinned'],
            });
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
                    bridge['_free'](namePtr);
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
                bridge['_free'](ptr);
            }
            __efxRc(rc, 'poseMesh', {
                2: [TypeError, 'poseMesh requires a Mesh with a rig'],
                6: [RangeError, 'clip index out of range'],
            });
        },
                /* setLight/setDirectionalLight are installed by the shared prelude (ADR 0049) */
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
            bridge['_free'](ptr);
            bridge['_free'](mapsptr);
        },
    };

