        loadText: function (path) {
            if (arguments.length < 1 || typeof path !== 'string') {
                throw new TypeError('loadText requires a path string');
            }
            var p = __efxAllocCStr(path);
            var ptr = bridge['_efx_bridge_load_text'](p);
            bridge['_free'](p);
            if (!ptr) {
                throw new Error('resource not found');
            }
            var s = UTF8ToString(ptr);
            bridge['_free'](ptr);
            return s;
        },
        loadImage: function (path) {
            if (arguments.length < 1 || typeof path !== 'string') {
                throw new TypeError('loadImage requires a path string');
            }
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_load_image'](p);
            bridge['_free'](p);
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
            var pathPtr = __efxAllocCStr(path);
            var namePtr = name !== null ? __efxAllocCStr(name) : 0;
            var id = bridge['_efx_bridge_load_meshdata'](pathPtr, hasMesh,
                                                         name !== null ? 1 : 0,
                                                         index, namePtr);
            bridge['_free'](pathPtr);
            bridge['_free'](namePtr);
            if (!id) {
                throw new Error('glTF import failed');
            }
            return new EfxMeshData(id);
        },
