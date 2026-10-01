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

