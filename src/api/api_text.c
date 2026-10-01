#include "api/api_internal.h"


/* ------------------------------------------------------ F8a font/text */

static JSValue text_error(JSContext *ctx, int code, const char *msg) {
    if (code == EFX_TEXT_ERR_RANGE) {
        return efx_api_range_error(ctx, msg);
    }
    return efx_api_generic_error(ctx, msg);
}


static int get_opt_number(JSContext *ctx, JSValueConst obj, const char *key,
                          int *present, double *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    *present = 0;
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_ToFloat64(ctx, out, v) < 0 || !isfinite(*out)) {
        JS_FreeValue(ctx, v);
        return -1;
    }
    JS_FreeValue(ctx, v);
    *present = 1;
    return 0;
}


static int parse_align(JSContext *ctx, JSValueConst obj, const char *key,
                       int *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    const char *s = JS_ToCString(ctx, v);
    JS_FreeValue(ctx, v);
    if (!s) return -1;
    int rc = 0;
    if (strcmp(s, "left") == 0) *out = EFX_TEXT_ALIGN_LEFT;
    else if (strcmp(s, "center") == 0) *out = EFX_TEXT_ALIGN_CENTER;
    else if (strcmp(s, "right") == 0) *out = EFX_TEXT_ALIGN_RIGHT;
    else if (strcmp(s, "justify") == 0) *out = EFX_TEXT_ALIGN_JUSTIFY;
    else rc = -1;
    JS_FreeCString(ctx, s);
    if (rc != 0) {
        efx_api_type_error(ctx, "align must be 'left', 'center', 'right' or 'justify'");
        return -1;
    }
    return 0;
}


static int parse_valign(JSContext *ctx, JSValueConst obj, const char *key,
                        int *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    const char *s = JS_ToCString(ctx, v);
    JS_FreeValue(ctx, v);
    if (!s) return -1;
    int rc = 0;
    if (strcmp(s, "top") == 0) *out = EFX_TEXT_VALIGN_TOP;
    else if (strcmp(s, "middle") == 0) *out = EFX_TEXT_VALIGN_MIDDLE;
    else if (strcmp(s, "bottom") == 0) *out = EFX_TEXT_VALIGN_BOTTOM;
    else rc = -1;
    JS_FreeCString(ctx, s);
    if (rc != 0) {
        efx_api_type_error(ctx, "valign must be 'top', 'middle' or 'bottom'");
        return -1;
    }
    return 0;
}


/* Parses the shared layout options; returns 0 or -1 (exception set). Colors
 * are accepted here (draw reads them separately) so measure and draw share
 * one known-field set. */
static int parse_layout_opts(JSContext *ctx, JSValueConst opts,
                             efx_text_layout_opts *lo) {
    memset(lo, 0, sizeof(*lo));
    lo->align = EFX_TEXT_ALIGN_LEFT;
    lo->valign = EFX_TEXT_VALIGN_TOP;
    lo->scale = 1.0f;
    if (JS_IsUndefined(opts) || JS_IsNull(opts)) return 0;
    if (!JS_IsObject(opts)) {
        efx_api_type_error(ctx, "text options must be an object");
        return -1;
    }
    static const char *known[] = {"align", "valign", "width", "lineHeight",
                                  "color", "outlineColor", "shadowColor",
                                  "rotation", "scale"};
    if (efx_api_check_known_fields(ctx, opts, known, 9, "drawText") != 0) return -1;
    if (parse_align(ctx, opts, "align", &lo->align) != 0) return -1;
    if (parse_valign(ctx, opts, "valign", &lo->valign) != 0) return -1;
    double d;
    int present = 0;
    if (get_opt_number(ctx, opts, "width", &present, &d) != 0) {
        efx_api_type_error(ctx, "width must be a finite number");
        return -1;
    }
    if (present) {
        if (!(d > 0)) {
            efx_api_range_error(ctx, "width must be > 0");
            return -1;
        }
        lo->has_width = 1;
        lo->width = (float)d;
    }
    if (get_opt_number(ctx, opts, "lineHeight", &present, &d) != 0) {
        efx_api_type_error(ctx, "lineHeight must be a finite number");
        return -1;
    }
    if (present) {
        if (!(d > 0)) {
            efx_api_range_error(ctx, "lineHeight must be > 0");
            return -1;
        }
        lo->has_line_height = 1;
        lo->line_height = (float)d;
    }
    if (get_opt_number(ctx, opts, "rotation", &present, &d) != 0) {
        efx_api_type_error(ctx, "rotation must be a finite number");
        return -1;
    }
    if (present) lo->rotation = (float)d;
    if (get_opt_number(ctx, opts, "scale", &present, &d) != 0) {
        efx_api_type_error(ctx, "scale must be a finite number");
        return -1;
    }
    if (present) {
        if (!(d > 0)) {
            efx_api_range_error(ctx, "scale must be > 0");
            return -1;
        }
        lo->scale = (float)d;
    }
    if (lo->align == EFX_TEXT_ALIGN_JUSTIFY && !lo->has_width) {
        efx_api_type_error(ctx, "justify alignment requires a width");
        return -1;
    }
    return 0;
}


static JSValue bounds_object(JSContext *ctx, const efx_text_bounds *b) {
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "width", JS_NewFloat64(ctx, (double)b->width));
    JS_SetPropertyStr(ctx, o, "height", JS_NewFloat64(ctx, (double)b->height));
    JS_SetPropertyStr(ctx, o, "lines", JS_NewInt32(ctx, b->lines));
    return o;
}


JSValue efx_js_loadFontData(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return efx_api_type_error(ctx, "loadFontData requires a path string");
    }
    struct efx_host_state *h = efx_api_host_state(ctx);
    if (!h->resource) {
        return efx_api_plain_error(ctx, "no resource root");
    }
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;
    int err = EFX_TEXT_OK;
    efx_text_fontdata *fd = efx_text_fontdata_load(h->resource, path, &err);
    JS_FreeCString(ctx, path);
    if (!fd) {
        return efx_api_generic_error(ctx, err == EFX_TEXT_ERR_NOMEM
                                      ? "out of memory"
                                      : "font could not be loaded");
    }
    efxjs_fontdata *wrap = calloc(1, sizeof(*wrap));
    if (!wrap) {
        efx_text_fontdata_destroy(fd);
        return efx_api_generic_error(ctx, "out of memory");
    }
    wrap->fd = fd;
    wrap->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, fontdata_class_id);
    JS_SetOpaque(obj, wrap);
    return obj;
}


JSValue efx_js_createFont(JSContext *ctx, JSValueConst this_val, int argc,
                          JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "createFont requires a FontData");
    }
    efxjs_fontdata *fdw = JS_GetOpaque2(ctx, argv[0], fontdata_class_id);
    if (!fdw) {
        return efx_api_type_error(ctx, "createFont requires a FontData");
    }
    if (!fdw->alive) {
        return efx_api_type_error(ctx, "using a destroyed resource");
    }
    if (argc < 2 || !JS_IsObject(argv[1])) {
        return efx_api_type_error(ctx, "createFont requires an options object");
    }
    JSValueConst opts = argv[1];
    static const char *known[] = {"size",   "glyphs", "padding",
                                  "filter", "outline", "shadow"};
    if (efx_api_check_known_fields(ctx, opts, known, 6, "createFont") != 0) {
        return JS_EXCEPTION;
    }
    efx_font_opts fo;
    memset(&fo, 0, sizeof(fo));
    fo.padding = 1;
    fo.filter = EFX_FILTER_LINEAR;
    double d = 0;
    int present = 0;
    if (get_opt_number(ctx, opts, "size", &present, &d) != 0) {
        return efx_api_type_error(ctx, "size must be a finite number");
    }
    if (!present) {
        return efx_api_type_error(ctx, "createFont requires size");
    }
    if (!(d > 0)) {
        return efx_api_range_error(ctx, "size must be > 0");
    }
    fo.size = (float)d;

    uint32_t *cps = NULL;
    int ncp = 0;
    JSValue glyphs = JS_GetPropertyStr(ctx, opts, "glyphs");
    if (!JS_IsUndefined(glyphs)) {
        if (!JS_IsString(glyphs)) {
            JS_FreeValue(ctx, glyphs);
            return efx_api_type_error(ctx, "glyphs must be a string");
        }
        const char *gs = JS_ToCString(ctx, glyphs);
        JS_FreeValue(ctx, glyphs);
        if (!gs) return JS_EXCEPTION;
        ncp = efx_text_codepoints(gs, &cps);
        JS_FreeCString(ctx, gs);
        if (ncp < 0) return efx_api_generic_error(ctx, "out of memory");
        if (ncp == 0) {
            free(cps);
            return efx_api_range_error(ctx, "glyphs must not be empty");
        }
        fo.codepoints = cps;
        fo.codepoint_count = ncp;
    } else {
        JS_FreeValue(ctx, glyphs);
    }

    if (get_opt_number(ctx, opts, "padding", &present, &d) != 0) {
        free(cps);
        return efx_api_type_error(ctx, "padding must be a finite number");
    }
    if (present) {
        if (d < 0 || d != floor(d)) {
            free(cps);
            return efx_api_range_error(ctx, "padding must be a non-negative integer");
        }
        fo.padding = (int)d;
    }

    JSValue filter = JS_GetPropertyStr(ctx, opts, "filter");
    if (!JS_IsUndefined(filter)) {
        const char *fs = JS_ToCString(ctx, filter);
        JS_FreeValue(ctx, filter);
        if (!fs) {
            free(cps);
            return JS_EXCEPTION;
        }
        if (strcmp(fs, "linear") == 0) fo.filter = EFX_FILTER_LINEAR;
        else if (strcmp(fs, "nearest") == 0) fo.filter = EFX_FILTER_NEAREST;
        else {
            JS_FreeCString(ctx, fs);
            free(cps);
            return efx_api_type_error(ctx, "filter must be 'linear' or 'nearest'");
        }
        JS_FreeCString(ctx, fs);
    } else {
        JS_FreeValue(ctx, filter);
    }

    JSValue outline = JS_GetPropertyStr(ctx, opts, "outline");
    if (!JS_IsUndefined(outline) && !JS_IsNull(outline)) {
        if (!JS_IsObject(outline)) {
            JS_FreeValue(ctx, outline);
            free(cps);
            return efx_api_type_error(ctx, "outline must be an object or null");
        }
        static const char *ok[] = {"width"};
        if (efx_api_check_known_fields(ctx, outline, ok, 1, "outline") != 0) {
            JS_FreeValue(ctx, outline);
            free(cps);
            return JS_EXCEPTION;
        }
        if (get_opt_number(ctx, outline, "width", &present, &d) != 0 ||
            !present) {
            JS_FreeValue(ctx, outline);
            free(cps);
            return efx_api_type_error(ctx, "outline requires a numeric width");
        }
        if (!(d > 0)) {
            JS_FreeValue(ctx, outline);
            free(cps);
            return efx_api_range_error(ctx, "outline width must be > 0");
        }
        fo.effects.has_outline = 1;
        fo.effects.outline_width = (float)d;
    }
    JS_FreeValue(ctx, outline);

    JSValue shadow = JS_GetPropertyStr(ctx, opts, "shadow");
    if (!JS_IsUndefined(shadow) && !JS_IsNull(shadow)) {
        if (!JS_IsObject(shadow)) {
            JS_FreeValue(ctx, shadow);
            free(cps);
            return efx_api_type_error(ctx, "shadow must be an object or null");
        }
        static const char *sk[] = {"blur", "offset"};
        if (efx_api_check_known_fields(ctx, shadow, sk, 2, "shadow") != 0) {
            JS_FreeValue(ctx, shadow);
            free(cps);
            return JS_EXCEPTION;
        }
        if (get_opt_number(ctx, shadow, "blur", &present, &d) != 0 ||
            !present) {
            JS_FreeValue(ctx, shadow);
            free(cps);
            return efx_api_type_error(ctx, "shadow requires a numeric blur");
        }
        if (!(d > 0)) {
            JS_FreeValue(ctx, shadow);
            free(cps);
            return efx_api_range_error(ctx, "shadow blur must be > 0");
        }
        fo.effects.has_shadow = 1;
        fo.effects.shadow_blur = (float)d;
        JSValue off = JS_GetPropertyStr(ctx, shadow, "offset");
        if (!JS_IsUndefined(off)) {
            float o[2];
            if (efx_api_get_float_array(ctx, off, o, 2) != 0) {
                JS_FreeValue(ctx, off);
                JS_FreeValue(ctx, shadow);
                free(cps);
                return JS_EXCEPTION;
            }
            fo.effects.shadow_offset[0] = o[0];
            fo.effects.shadow_offset[1] = o[1];
        }
        JS_FreeValue(ctx, off);
    }
    JS_FreeValue(ctx, shadow);

    int err = EFX_TEXT_OK;
    efx_text_font *font = efx_text_font_create(fdw->fd, &fo, &err);
    free(cps);
    if (!font) {
        return text_error(ctx, err, "font could not be baked");
    }
    efxjs_font *wrap = calloc(1, sizeof(*wrap));
    if (!wrap) {
        efx_text_font_destroy(font);
        return efx_api_generic_error(ctx, "out of memory");
    }
    wrap->font = font;
    wrap->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, font_class_id);
    JS_SetOpaque(obj, wrap);
    return obj;
}


static efxjs_font *live_font(JSContext *ctx, JSValueConst v) {
    efxjs_font *f = JS_GetOpaque2(ctx, v, font_class_id);
    if (!f || !f->alive) return NULL;
    return f;
}


JSValue efx_js_measureText(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
    (void)this_val;
    if (argc < 2 || !JS_IsString(argv[0])) {
        return efx_api_type_error(ctx, "measureText requires (text, font, opts?)");
    }
    efxjs_font *f = live_font(ctx, argv[1]);
    if (!f) {
        return efx_api_type_error(ctx, "measureText requires a live Font");
    }
    efx_text_layout_opts lo;
    if (parse_layout_opts(ctx, argc >= 3 ? argv[2] : JS_UNDEFINED, &lo) != 0) {
        return JS_EXCEPTION;
    }
    const char *text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_EXCEPTION;
    efx_text_bounds b;
    int rc = efx_text_measure(f->font, text, &lo, &b);
    JS_FreeCString(ctx, text);
    if (rc != EFX_TEXT_OK) {
        return text_error(ctx, rc, "text measurement failed");
    }
    return bounds_object(ctx, &b);
}


JSValue efx_js_drawText(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
    (void)this_val;
    if (argc < 4 || !JS_IsString(argv[0])) {
        return efx_api_type_error(ctx, "drawText requires (text, font, x, y, opts?)");
    }
    efxjs_font *f = live_font(ctx, argv[1]);
    if (!f) {
        return efx_api_type_error(ctx, "drawText requires a live Font");
    }
    double x = 0, y = 0;
    if (JS_ToFloat64(ctx, &x, argv[2]) < 0 || !isfinite(x) ||
        JS_ToFloat64(ctx, &y, argv[3]) < 0 || !isfinite(y)) {
        return efx_api_type_error(ctx, "drawText requires finite x and y");
    }
    JSValueConst opts = argc >= 5 ? argv[4] : JS_UNDEFINED;
    efx_text_layout_opts lo;
    if (parse_layout_opts(ctx, opts, &lo) != 0) return JS_EXCEPTION;

    float color[4] = {1, 1, 1, 1};
    float outline_color[4] = {0, 0, 0, 1};
    float shadow_color[4] = {0, 0, 0, 1};
    if (JS_IsObject(opts)) {
        JSValue cv = JS_GetPropertyStr(ctx, opts, "color");
        if (!JS_IsUndefined(cv)) {
            if (efx_api_get_float_array(ctx, cv, color, 4) != 0) {
                JS_FreeValue(ctx, cv);
                return JS_EXCEPTION;
            }
        }
        JS_FreeValue(ctx, cv);
        JSValue ov = JS_GetPropertyStr(ctx, opts, "outlineColor");
        if (!JS_IsUndefined(ov)) {
            if (efx_api_get_float_array(ctx, ov, outline_color, 4) != 0) {
                JS_FreeValue(ctx, ov);
                return JS_EXCEPTION;
            }
        }
        JS_FreeValue(ctx, ov);
        JSValue sv = JS_GetPropertyStr(ctx, opts, "shadowColor");
        if (!JS_IsUndefined(sv)) {
            if (efx_api_get_float_array(ctx, sv, shadow_color, 4) != 0) {
                JS_FreeValue(ctx, sv);
                return JS_EXCEPTION;
            }
        }
        JS_FreeValue(ctx, sv);
    }

    const char *text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_EXCEPTION;
    efx_text_bounds b;
    int rc = efx_text_draw(f->font, text, (float)x, (float)y, &lo, color,
                           outline_color, shadow_color, &b);
    JS_FreeCString(ctx, text);
    if (rc != EFX_TEXT_OK) {
        return text_error(ctx, rc, "text drawing failed");
    }
    return bounds_object(ctx, &b);
}
