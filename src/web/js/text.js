        loadFontData: function (path) {
            if (arguments.length < 1 || typeof path !== 'string') {
                throw new TypeError('loadFontData requires a path string');
            }
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_load_fontdata'](p);
            bridge['_free'](p);
            if (!id) {
                throw new Error('font could not be loaded');
            }
            return new EfxFontData(id);
        },
        createFont: function (fontData, opts) {
            if (arguments.length < 1 || !(fontData instanceof EfxFontData)) {
                throw new TypeError('createFont requires a FontData');
            }
            if (!fontData.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            if (arguments.length < 2 || !__efxIsObject(opts)) {
                throw new TypeError('createFont requires an options object');
            }
            var known = { size: 1, glyphs: 1, padding: 1, filter: 1, outline: 1,
                          shadow: 1 };
                        __efxCheckKnown(opts, known, 'createFont');
            if (opts.size === undefined) {
                throw new TypeError('createFont requires size');
            }
            var size = __efxFinite(opts.size, 'size must be a finite number');
            if (!(size > 0)) {
                throw new RangeError('size must be > 0');
            }
            var glyphs = opts.glyphs;
            if (glyphs !== undefined) {
                if (typeof glyphs !== 'string') {
                    throw new TypeError('glyphs must be a string');
                }
                if (glyphs.length === 0) {
                    throw new RangeError('glyphs must not be empty');
                }
            }
            var padding = 1;
            if (opts.padding !== undefined) {
                var pv = __efxFinite(opts.padding, 'padding must be a finite number');
                if (pv < 0 || pv !== Math.floor(pv)) {
                    throw new RangeError('padding must be a non-negative integer');
                }
                padding = pv | 0;
            }
            var filter = 1;
            if (opts.filter !== undefined) {
                if (opts.filter === 'linear') {
                    filter = 1;
                } else if (opts.filter === 'nearest') {
                    filter = 0;
                } else {
                    throw new TypeError("filter must be 'linear' or 'nearest'");
                }
            }
            var hasOutline = 0, outlineWidth = 0;
            if (opts.outline !== undefined && opts.outline !== null) {
                if (!__efxIsObject(opts.outline)) {
                    throw new TypeError('outline must be an object or null');
                }
                __efxCheckKnown(opts.outline, { width: 1 }, 'outline');
                if (opts.outline.width === undefined) {
                    throw new TypeError('outline requires a numeric width');
                }
                outlineWidth = __efxFinite(opts.outline.width,
                                           'outline width must be a finite number');
                if (!(outlineWidth > 0)) {
                    throw new RangeError('outline width must be > 0');
                }
                hasOutline = 1;
            }
            var hasShadow = 0, shadowBlur = 0, offX = 0, offY = 0;
            if (opts.shadow !== undefined && opts.shadow !== null) {
                if (!__efxIsObject(opts.shadow)) {
                    throw new TypeError('shadow must be an object or null');
                }
                __efxCheckKnown(opts.shadow, { blur: 1, offset: 1 }, 'shadow');
                if (opts.shadow.blur === undefined) {
                    throw new TypeError('shadow requires a numeric blur');
                }
                shadowBlur = __efxFinite(opts.shadow.blur,
                                         'shadow blur must be a finite number');
                if (!(shadowBlur > 0)) {
                    throw new RangeError('shadow blur must be > 0');
                }
                if (opts.shadow.offset !== undefined) {
                    var off = __efxFloatArray(opts.shadow.offset, 2);
                    offX = off[0];
                    offY = off[1];
                }
                hasShadow = 1;
            }
            var glyphsPtr = glyphs !== undefined ? __efxAllocCStr(glyphs) : 0;
            var id = bridge['_efx_bridge_create_font'](
                fontData.__id, size, glyphsPtr, padding, filter, hasOutline,
                outlineWidth, hasShadow, shadowBlur, offX, offY);
            bridge['_free'](glyphsPtr);
            if (!id) {
                throw new Error('font could not be baked');
            }
            return new EfxFont(id);
        },
        measureText: function (text, font, opts) {
            if (arguments.length < 2 || typeof text !== 'string') {
                throw new TypeError('measureText requires (text, font, opts?)');
            }
            if (!(font instanceof EfxFont)) {
                throw new TypeError('measureText requires a live Font');
            }
            if (!font.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            var lo = __efxTextLayout(opts);
            var tptr = __efxAllocCStr(text);
            var optr = bridge['_malloc'](12);
            var rc = bridge['_efx_bridge_text_measure'](
                tptr, font.__id, lo.align, lo.valign, lo.hasWidth, lo.width,
                lo.hasLh, lo.lh, lo.scale, lo.rotation, optr);
            bridge['_free'](tptr);
            var b = { width: HEAPF32[optr >> 2],
                      height: HEAPF32[(optr >> 2) + 1],
                      lines: HEAPF32[(optr >> 2) + 2] };
            bridge['_free'](optr);
            __efxRc(rc, 'text operation', { 4: [RangeError, 'text layout failed'] });
            return b;
        },
        drawText: function (text, font, x, y, opts) {
            if (arguments.length < 4 || typeof text !== 'string') {
                throw new TypeError('drawText requires (text, font, x, y, opts?)');
            }
            if (!(font instanceof EfxFont)) {
                throw new TypeError('drawText requires a live Font');
            }
            if (!font.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            x = __efxFinite(x, 'drawText requires finite x and y');
            y = __efxFinite(y, 'drawText requires finite x and y');
            var lo = __efxTextLayout(opts);
            var color = [1, 1, 1, 1], oc = [0, 0, 0, 1], sc = [0, 0, 0, 1];
            if (__efxIsObject(opts)) {
                if (opts.color !== undefined) { color = __efxFloatArray(opts.color, 4); }
                if (opts.outlineColor !== undefined) { oc = __efxFloatArray(opts.outlineColor, 4); }
                if (opts.shadowColor !== undefined) { sc = __efxFloatArray(opts.shadowColor, 4); }
            }
            var tptr = __efxAllocCStr(text);
            var optr = bridge['_malloc'](12);
            var rc = bridge['_efx_bridge_text_draw'](
                tptr, font.__id, x, y, lo.align, lo.valign, lo.hasWidth,
                lo.width, lo.hasLh, lo.lh, lo.scale, lo.rotation,
                color[0], color[1], color[2], color[3],
                oc[0], oc[1], oc[2], oc[3],
                sc[0], sc[1], sc[2], sc[3], optr);
            bridge['_free'](tptr);
            var b = { width: HEAPF32[optr >> 2],
                      height: HEAPF32[(optr >> 2) + 1],
                      lines: HEAPF32[(optr >> 2) + 2] };
            bridge['_free'](optr);
            __efxRc(rc, 'text operation', { 4: [RangeError, 'text layout failed'] });
            return b;
        },
