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
        setClearColor: function (color) {
            if (arguments.length < 1) {
                throw new TypeError('setClearColor requires a [r,g,b,a] array');
            }
            var c = __efxFloatArray(color, 4);
            bridge['_efx_bridge_set_clear_color'](c[0], c[1], c[2], c[3]);
        },
        setCamera2D: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('setCamera2D requires an options object');
            }
            var frameW = 0, frameH = 0, x = NaN, y = NaN, zoom = 1, rotation = 0;
            var frame = opts['frame'];
            if (frame !== undefined) {
                var f = __efxFloatArray(frame, 2);
                if (!(f[0] > 0 && f[1] > 0)) {
                    throw new RangeError('frame must be positive');
                }
                frameW = f[0];
                frameH = f[1];
                if (isNaN(x)) {
                    x = frameW * 0.5;
                }
                if (isNaN(y)) {
                    y = frameH * 0.5;
                }
            }
            var xv = opts['x'];
            if (xv !== undefined) {
                x = __efxFinite(xv, 'camera fields must be finite numbers');
            }
            var yv = opts['y'];
            if (yv !== undefined) {
                y = __efxFinite(yv, 'camera fields must be finite numbers');
            }
            var zv = opts['zoom'];
            if (zv !== undefined) {
                zoom = __efxFinite(zv, 'camera fields must be finite numbers');
            }
            var rv = opts['rotation'];
            if (rv !== undefined) {
                rotation = __efxFinite(rv, 'camera fields must be finite numbers');
            }
            if (!(zoom > 0)) {
                throw new RangeError('zoom must be > 0');
            }
            bridge['_efx_bridge_set_camera'](frameW, frameH, x, y, zoom, rotation);
        },
        /* createImageData is installed by the shared prelude (ADR 0049) */
