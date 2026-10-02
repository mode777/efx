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
        args: function () {
            var n = bridge['_efx_bridge_arg_count']();
            var out = new Array(n);
            for (var i = 0; i < n; i++) {
                out[i] = UTF8ToString(bridge['_efx_bridge_arg'](i));
            }
            return out;
        },
        registerUpdateHook: makeRegister(1),
        registerRenderHook: makeRegister(0),
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
