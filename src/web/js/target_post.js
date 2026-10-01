        createRenderTarget: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('createRenderTarget requires an options object');
            }
            var known = { width: 1, height: 1 };
            __efxCheckKnown(opts, known, 'createRenderTarget');
            var dims = [];
            for (var k = 0; k < 2; k++) {
                var key = k === 0 ? 'width' : 'height';
                var v = opts[key];
                if (v === undefined) {
                    throw new TypeError('createRenderTarget requires width and height');
                }
                if (typeof v !== 'number' || !isFinite(v) || v <= 0 ||
                    (v | 0) !== v || v > 4096) {
                    throw new RangeError('width and height must be integers in 1..4096');
                }
                dims.push(v | 0);
            }
            var handle = bridge['_efx_bridge_target_create'](dims[0], dims[1]);
            if (!handle) {
                throw new Error('render target creation failed (no GPU context?)');
            }
            return new EfxRenderTarget(handle);
        },
        beginRenderTarget: function (rt) {
            if (arguments.length < 1) {
                throw new TypeError('beginRenderTarget requires a RenderTarget');
            }
            if (!(rt instanceof EfxRenderTarget)) {
                throw new TypeError('expected a RenderTarget');
            }
            if (!rt.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            var rc = bridge['_efx_bridge_target_begin'](rt.__handle);
            __efxRc(rc, 'beginRenderTarget', {
                1: true,
                7: [TypeError, 'a render target is already active'],
            });
        },
        endRenderTarget: function () {
            var rc = bridge['_efx_bridge_target_end']();
            __efxRc(rc, 'endRenderTarget', {
                1: true,
                8: [TypeError, 'no render target is active'],
            });
        },
        setPostEffects: function (list) {
            if (arguments.length < 1) {
                throw new TypeError('setPostEffects requires an array or null');
            }
            if (list === null || list === undefined) {
                bridge['_efx_bridge_set_post_effects'](0, 0);
                return;
            }
            if (!Array.isArray(list)) {
                throw new TypeError('setPostEffects requires an array or null');
            }
            if (list.length > 8) {
                throw new RangeError('post-effect chain is limited to 8 entries');
            }
            var wire = new Float32Array(list.length * 9);
            for (var i = 0; i < list.length; i++) {
                var e = __efxPostEntry(list[i]);
                wire.set(e, i * 9);
            }
            var ptr = wire.length ? mallocCopyF32(wire) : 0;
            var rc = bridge['_efx_bridge_set_post_effects'](ptr, list.length);
            if (ptr) {
                bridge['_free'](ptr);
            }
            __efxRc(rc, 'setPostEffects', {
                1: [TypeError, 'unknown post effect'],
                2: [RangeError, 'post-effect chain is limited to 8 entries'],
                3: [RangeError, 'post-effect option out of range'],
            });
        },
        setRenderScale: function (scale, opts) {
            if (arguments.length < 1) {
                throw new TypeError('setRenderScale requires a scale number');
            }
            if (typeof scale !== 'number') {
                throw new TypeError('scale must be a number');
            }
            if (!isFinite(scale) || scale <= 0 || scale > 2) {
                throw new RangeError('scale must be in (0, 2]');
            }
            var filter = 1;
            if (opts !== undefined && opts !== null) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('setRenderScale options must be an object');
                }
                var kn = { filter: 1 };
                                __efxCheckKnown(opts, kn, 'setRenderScale');
                if (opts['filter'] !== undefined) {
                    if (opts['filter'] === 'nearest') {
                        filter = 0;
                    } else if (opts['filter'] === 'linear') {
                        filter = 1;
                    } else {
                        throw new TypeError('unknown filter');
                    }
                }
            }
            var rc = bridge['_efx_bridge_set_render_scale'](scale, filter);
            __efxRc(rc, 'setRenderScale', {
                3: [RangeError, 'scale must be in (0, 2]'],
                4: [TypeError, 'unknown filter'],
            });
        },
