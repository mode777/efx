                /* createRenderTarget is installed by the shared prelude (ADR 0049) */
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
            var rc = bridge['_efx_render_end_target']();
            __efxRc(rc, 'endRenderTarget', {
                1: true,
                8: [TypeError, 'no render target is active'],
            });
        },
        /* setPostEffects is installed by the shared prelude (ADR 0049) */
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
            var rc = bridge['_efx_render_set_render_scale'](scale, filter);
            __efxRc(rc, 'setRenderScale', {
                3: [RangeError, 'scale must be in (0, 2]'],
                4: [TypeError, 'unknown filter'],
            });
        },
