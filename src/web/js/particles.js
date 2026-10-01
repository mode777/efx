        drawQuad: function (x, y, texture, opts) {
            if (arguments.length < 3) {
                throw new TypeError('drawQuad requires (x, y, texture, opts?)');
            }
            var fx = __efxNumber(x, 'x and y must be numbers');
            var fy = __efxNumber(y, 'x and y must be numbers');
            if (!isFinite(fx) || !isFinite(fy)) {
                throw new RangeError('x and y must be finite');
            }
            var tex = liveSample(texture); /* Texture or RenderTarget (F5a) */
            var color = [1, 1, 1, 1];
            var rotation = 0, scale = 1;
            var src = [0, 0, 0, 0];
            var hasSrc = false;
            var size = [0, 0];
            var hasSize = false;
            var origin = [0, 0];
            var hasOrigin = false;
            if (arguments.length >= 4 && opts !== undefined) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('opts must be an object');
                }
                var known = { color: 1, rotation: 1, scale: 1, sourceRect: 1, size: 1, origin: 1 };
                                __efxCheckKnown(opts, known, 'drawQuad', false, true);
                var cv = opts['color'];
                if (cv !== undefined) {
                    color = __efxFloatArray(cv, 4);
                }
                var rv = opts['rotation'];
                if (rv !== undefined) {
                    rotation = __efxFinite(rv, 'rotation must be a finite number');
                }
                var sv = opts['scale'];
                if (sv !== undefined) {
                    scale = __efxFinite(sv, 'scale must be a finite number');
                    if (scale <= 0) {
                        throw new RangeError('scale must be > 0');
                    }
                }
                var zv = opts['size'];
                if (zv !== undefined) {
                    size = __efxFloatArray(zv, 2);
                    if (size[0] <= 0 || size[1] <= 0) {
                        throw new RangeError('size entries must be > 0');
                    }
                    hasSize = true;
                }
                var ov = opts['origin'];
                if (ov !== undefined) {
                    origin = __efxFloatArray(ov, 2);
                    hasOrigin = true;
                }
                var srcv = opts['sourceRect'];
                if (srcv !== undefined) {
                    src = __efxSourceRect(tex, srcv);
                    hasSrc = true;
                }
            }
            var fw, fh;
            if (hasSize) {
                fw = size[0];
                fh = size[1];
            } else if (hasSrc) {
                fw = src[2];
                fh = src[3];
            } else {
                fw = tex.w;
                fh = tex.h;
            }
            var ox = hasOrigin ? origin[0] : fw * 0.5;
            var oy = hasOrigin ? origin[1] : fh * 0.5;
            var rc = bridge['_efx_bridge_draw_quad'](tex.handle, fx, fy, fw, fh,
                color[0], color[1], color[2], color[3], rotation, scale,
                src[0], src[1], src[2], src[3], hasSrc ? 1 : 0, ox, oy);
            __efxRc(rc, 'drawQuad', { 1: true, 4: true, 9: true });
        },
        drawBillboard: function (pos, opts) {
            if (arguments.length < 2) {
                throw new TypeError('drawBillboard requires (pos, opts)');
            }
            var p = __efxPartVec(pos, 'pos', false);
            if (!__efxIsObject(opts)) {
                throw new TypeError('drawBillboard options must be an object');
            }
            var known = { texture: 1, size: 1, color: 1, sourceRect: 1,
                          rotation: 1, facing: 1, depthTest: 1, normal: 1 };
                        __efxCheckKnown(opts, known, 'drawBillboard');
            if (opts['texture'] === undefined) {
                throw new TypeError('drawBillboard requires a texture');
            }
            var tex = liveSample(opts['texture']);
            var size = [1, 1];
            if (opts['size'] !== undefined) {
                if (Array.isArray(opts['size'])) {
                    size = __efxFloatArray(opts['size'], 2);
                } else {
                    var sd = __efxFinite(opts['size'], 'size must be a number or [w,h]');
                    size = [sd, sd];
                }
            }
            if (size[0] <= 0 || size[1] <= 0) {
                throw new RangeError('size entries must be > 0');
            }
            var color = [1, 1, 1, 1];
            if (opts['color'] !== undefined) {
                color = __efxFloatArray(opts['color'], 4);
            }
            var rotation = 0;
            if (opts['rotation'] !== undefined) {
                rotation = __efxFinite(opts['rotation'], 'rotation must be a finite number');
            }
            var facing = __efxPartEnum(opts['facing'], { view: 0, y: 1 }, 0, 'facing');
            var normal = [0, 1, 0];
            if (opts['normal'] !== undefined) {
                normal = __efxPartVec(opts['normal'], 'normal', false);
            }
            var depth = 1;
            if (opts['depthTest'] !== undefined) {
                if (typeof opts['depthTest'] !== 'boolean') {
                    throw new TypeError('depthTest must be a boolean');
                }
                depth = opts['depthTest'] ? 1 : 0;
            }
            var src = [0, 0, 0, 0];
            var hasSrc = 0;
            if (opts['sourceRect'] !== undefined) {
                src = __efxSourceRect(tex, opts['sourceRect']);
                hasSrc = 1;
            }
            var pPtr = mallocCopyF32(p);
            var nPtr = mallocCopyF32(normal);
            var rc = bridge['_efx_bridge_draw_billboard'](tex.handle, pPtr,
                size[0], size[1], color[0], color[1], color[2], color[3],
                rotation, facing, nPtr, depth, src[0], src[1], src[2], src[3],
                hasSrc);
            bridge['_free'](pPtr);
            bridge['_free'](nPtr);
            __efxRc(rc, 'drawBillboard', {
                1: true,
                2: [TypeError, 'expected a Texture or RenderTarget'],
                10: [RangeError, 'invalid billboard size or facing'],
            });
        },
        drawSprites: function (texture, sprites) {
            if (arguments.length < 2) {
                throw new TypeError('drawSprites requires (texture, sprites)');
            }
            var tex = liveSample(texture);
            if (!Array.isArray(sprites)) {
                throw new TypeError('sprites must be an array');
            }
            var parsed = [];
            for (var i = 0; i < sprites.length; i++) {
                parsed.push(__efxSprite(tex, sprites[i]));
            }
            for (i = 0; i < parsed.length; i++) {
                var s = parsed[i];
                var rc = bridge['_efx_bridge_draw_sprite'](tex.handle, s.x, s.y,
                    s.w, s.h, s.color[0], s.color[1], s.color[2], s.color[3],
                    s.rotation, s.scale, s.src[0], s.src[1], s.src[2], s.src[3],
                    s.hasSrc, s.ox, s.oy);
                __efxRc(rc, 'drawSprites', { 1: true, 4: true });
            }
        },
        createParticleSystem: function (opts) {
            var parsed = __efxParticleWire(opts);
            var ptr = mallocCopyF32(parsed.wire);
            var handle = bridge['_efx_bridge_particles_create'](ptr, parsed.texture);
            bridge['_free'](ptr);
            if (!handle) {
                throw new RangeError('invalid particle configuration');
            }
            var snapshot = {};
            for (var k in opts) {
                if (Object.prototype.hasOwnProperty.call(opts, k)) {
                    snapshot[k] = opts[k];
                }
            }
            return new EfxParticleSystem(handle, parsed.texture, snapshot);
        },
        drawParticles: function (sys) {
            livePS(sys);
            var rc = bridge['_efx_bridge_particles_draw'](sys.__handle);
            __efxRc(rc, 'drawParticles', {
                1: true,
                2: [TypeError, 'expected a live ParticleSystem'],
                9: true,
            });
        },
        setBlendMode: function (mode) {
            if (arguments.length < 1) {
                throw new TypeError('setBlendMode requires a mode string');
            }
            var s = __efxCStr(mode);
            if (s === null) {
                throw new TypeError('setBlendMode requires a mode string');
            }
            var m;
            if (s === 'alpha') {
                m = 0;
            } else if (s === 'additive') {
                m = 1;
            } else if (s === 'subtractive') {
                m = 2;
            } else {
                throw new TypeError('unknown blend mode');
            }
            bridge['_efx_bridge_set_blend'](m);
        },
