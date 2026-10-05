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
        /* createFont is installed by the shared prelude (ADR 0049) */
        /* measure is a Font prototype method (ADR 0055) */
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
