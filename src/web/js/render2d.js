    var api = {
        log: function (msg) {
            var s = null;
            if (arguments.length > 0) {
                s = __efxCStr(msg);
            }
            console.log(s === null ? '' : s);
        },
        quit: function (code) {
            var c = 0;
            if (arguments.length > 0) {
                try {
                    c = Number(code) | 0;
                } catch (e) {
                    c = 0;
                }
            }
            bridge['_efx_bridge_quit'](c);
            throw __efxState().quitSentinel;
        },
        registerUpdateHook: makeRegister(1),
        registerRenderHook: makeRegister(0),
        /* efx.io sub-namespace: resource loaders. `loadText` moved off the
         * root; `loadData` returns raw bytes as a Uint8Array copy. */
        io: {
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
            loadData: function (path) {
                if (arguments.length < 1 || typeof path !== 'string') {
                    throw new TypeError('loadData requires a path string');
                }
                var p = __efxAllocCStr(path);
                var lenPtr = bridge['_malloc'](4);
                var ptr = bridge['_efx_bridge_load_data'](p, lenPtr);
                bridge['_free'](p);
                if (!ptr) {
                    bridge['_free'](lenPtr);
                    throw new Error('resource not found');
                }
                var n = HEAPU32[lenPtr >> 2];
                var out = new Uint8Array(n);
                out.set(HEAPU8.subarray(ptr, ptr + n));
                bridge['_free'](ptr);
                bridge['_free'](lenPtr);
                return out;
            },
        },
        /* graphics sub-namespace (ADR 0050); prelude members are installed
         * onto this same object (ADR 0049) */
        graphics: {
            setClearColor: function (color) {
                if (arguments.length < 1) {
                    throw new TypeError('setClearColor requires a [r,g,b,a] array');
                }
                var c = __efxFloatArray(color, 4);
                bridge['_efx_bridge_set_clear_color'](c[0], c[1], c[2], c[3]);
            },
                /* setCamera2D is installed by the shared prelude (ADR 0049) */
                /* createImageData is installed by the shared prelude (ADR 0049) */
