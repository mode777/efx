#include "api/api_internal.h"


/* ================================================= F14 audio bindings */

/* validated numeric play argument (the prelude checked them already) */
static double audio_arg(JSContext *ctx, JSValueConst v) {
    double d = 0.0;
    if (JS_ToFloat64(ctx, &d, v) < 0) {
        return 0.0;
    }
    return d;
}


/* ---- playback handle ---- */

static efxjs_audio *audio_handle(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a || !a->alive) {
        return NULL;
    }
    return a;
}


static int audio_handle_live(const efxjs_audio *a) {
    return a->voice >= 0 && efx_audio_voice_serial(a->voice) == a->serial;
}


static JSValue audio_handle_stop(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return efx_api_type_error(ctx, "not an Audio handle");
    }
    if (audio_handle_live(a)) {
        efx_audio_stop_voice(a->voice);
    }
    a->voice = -1;
    return JS_UNDEFINED;
}


static JSValue audio_handle_pause(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return efx_api_type_error(ctx, "not an Audio handle");
    }
    if (audio_handle_live(a)) {
        efx_audio_set_voice_paused(a->voice, 1);
    }
    return JS_UNDEFINED;
}


static JSValue audio_handle_resume(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return efx_api_type_error(ctx, "not an Audio handle");
    }
    if (audio_handle_live(a)) {
        efx_audio_set_voice_paused(a->voice, 0);
    }
    return JS_UNDEFINED;
}


static JSValue audio_handle_get_playing(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return JS_FALSE;
    }
    return JS_NewBool(ctx, audio_handle_live(a) &&
                               efx_audio_voice_playing(a->voice));
}


static JSValue audio_handle_get_paused(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return JS_FALSE;
    }
    return JS_NewBool(ctx, audio_handle_live(a) &&
                               efx_audio_voice_paused(a->voice));
}


static JSValue audio_handle_get_volume(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    return JS_NewFloat64(ctx, a ? (double)a->volume : 0.0);
}


static JSValue audio_handle_set_volume(JSContext *ctx, JSValueConst this_val,
                                       JSValueConst val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a) {
        return efx_api_type_error(ctx, "not an Audio handle");
    }
    double d = 0.0;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || d < 0.0) {
        return efx_api_range_error(ctx, "volume must be a non-negative number");
    }
    a->volume = (float)d;
    if (audio_handle_live(a)) {
        efx_audio_set_voice_volume(a->voice, (float)d);
    }
    return JS_UNDEFINED;
}


static JSValue audio_handle_get_pan(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    return JS_NewFloat64(ctx, a ? (double)a->pan : 0.0);
}


static JSValue audio_handle_set_pan(JSContext *ctx, JSValueConst this_val,
                                    JSValueConst val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a) {
        return efx_api_type_error(ctx, "not an Audio handle");
    }
    double d = 0.0;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d)) {
        return efx_api_range_error(ctx, "pan must be a finite number");
    }
    a->pan = (float)d;
    if (audio_handle_live(a)) {
        efx_audio_set_voice_pan(a->voice, (float)d);
    }
    return JS_UNDEFINED;
}


static JSValue audio_handle_get_pitch(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    return JS_NewFloat64(ctx, a ? (double)a->pitch : 1.0);
}


static JSValue audio_handle_set_pitch(JSContext *ctx, JSValueConst this_val,
                                      JSValueConst val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a) {
        return efx_api_type_error(ctx, "not an Audio handle");
    }
    double d = 0.0;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || d <= 0.0) {
        return efx_api_range_error(ctx, "pitch must be a positive number");
    }
    a->pitch = (float)d;
    if (audio_handle_live(a)) {
        efx_audio_set_voice_pitch(a->voice, (float)d);
    }
    return JS_UNDEFINED;
}


static JSValue audio_handle_get_loop(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    return JS_NewBool(ctx, a ? a->loop : 0);
}


static JSValue audio_handle_set_loop(JSContext *ctx, JSValueConst this_val,
                                     JSValueConst val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a) {
        return efx_api_type_error(ctx, "not an Audio handle");
    }
    int loop = JS_ToBool(ctx, val) ? 1 : 0;
    a->loop = loop;
    if (audio_handle_live(a)) {
        efx_audio_set_voice_loop(a->voice, loop);
    }
    return JS_UNDEFINED;
}


_Static_assert(sizeof(audio_proto_funcs) / sizeof((audio_proto_funcs)[0]) == 9,
                "audio_proto_funcs must match the api_internal.h declaration");
const JSCFunctionListEntry audio_proto_funcs[] = {
    JS_CFUNC_DEF("stop", 0, audio_handle_stop),
    JS_CFUNC_DEF("pause", 0, audio_handle_pause),
    JS_CFUNC_DEF("resume", 0, audio_handle_resume),
    JS_CGETSET_DEF("playing", audio_handle_get_playing, NULL),
    JS_CGETSET_DEF("paused", audio_handle_get_paused, NULL),
    JS_CGETSET_DEF("volume", audio_handle_get_volume, audio_handle_set_volume),
    JS_CGETSET_DEF("pan", audio_handle_get_pan, audio_handle_set_pan),
    JS_CGETSET_DEF("pitch", audio_handle_get_pitch, audio_handle_set_pitch),
    JS_CGETSET_DEF("loop", audio_handle_get_loop, audio_handle_set_loop),
};



/* ---- namespace entry points ---- */

/* natives for the shared prelude validators (ADR 0049). Loaders return the
 * wrapper object on success, or a negative code the prelude maps to the
 * canonical message: -1 unreadable, -2 undecodable. */

static int audio_load_bytes(JSContext *ctx, JSValueConst pathv,
                            const char *fn, size_t *out_size,
                            uint8_t **out_bytes) {
    if (!JS_IsString(pathv)) {
        JS_ThrowTypeError(ctx, "%s requires a path string", fn);
        return -3;
    }
    const char *path = JS_ToCString(ctx, pathv);
    if (!path) {
        return -3;
    }
    struct efx_host_state *h = efx_api_host_state(ctx);
    if (!h->resource) {
        JS_FreeCString(ctx, path);
        JS_ThrowInternalError(ctx, "%s requires a resource root", fn);
        return -3;
    }
    size_t size = 0;
    int rerr = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(h->resource, path, &size, &rerr);
    JS_FreeCString(ctx, path);
    if (!bytes) {
        return -1;
    }
    *out_size = size;
    *out_bytes = bytes;
    return 0;
}


JSValue efx_js_audio_load_data_wire(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "loadAudioData requires a path string");
    }
    size_t size = 0;
    uint8_t *bytes = NULL;
    int rc = audio_load_bytes(ctx, argv[0], "loadAudioData", &size, &bytes);
    if (rc != 0) {
        return JS_NewInt32(ctx, rc);
    }
    int derr = 0;
    efx_audio_data *data = efx_audio_data_load(bytes, size, &derr);
    efx_resource_free(bytes);
    if (!data) {
        return JS_NewInt32(ctx, -2);
    }
    efxjs_audiodata *o = calloc(1, sizeof(*o));
    if (!o) {
        efx_audio_data_release(data);
        return JS_NewInt32(ctx, -2);
    }
    o->data = data;
    o->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, audiodata_class_id);
    JS_SetOpaque(obj, o);
    return obj;
}


JSValue efx_js_audio_load_stream_wire(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "loadAudioStream requires a path string");
    }
    size_t size = 0;
    uint8_t *bytes = NULL;
    int rc = audio_load_bytes(ctx, argv[0], "loadAudioStream", &size, &bytes);
    if (rc != 0) {
        return JS_NewInt32(ctx, rc);
    }
    int derr = 0;
    efx_audio_stream *stream = efx_audio_stream_load(bytes, size, &derr);
    efx_resource_free(bytes);
    if (!stream) {
        return JS_NewInt32(ctx, -2);
    }
    efxjs_audiostream *o = calloc(1, sizeof(*o));
    if (!o) {
        efx_audio_stream_release(stream);
        return JS_NewInt32(ctx, -2);
    }
    o->stream = stream;
    o->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, audiostream_class_id);
    JS_SetOpaque(obj, o);
    return obj;
}


/* type + liveness for the playAudio source (throws with the shared
 * messages; the prelude validates the options bag first, D3) */
JSValue efx_js_audio_check_source(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "playAudio requires an AudioData or AudioStream");
    }
    efxjs_audiodata *ad = JS_GetOpaque(argv[0], audiodata_class_id);
    efxjs_audiostream *as = JS_GetOpaque(argv[0], audiostream_class_id);
    if (!ad && !as) {
        return efx_api_type_error(ctx, "playAudio requires an AudioData or AudioStream");
    }
    if (ad) {
        if (!ad->alive || !ad->data) {
            return efx_api_generic_error(ctx, "AudioData was destroyed");
        }
    } else if (!as->alive || !as->stream) {
        return efx_api_generic_error(ctx, "AudioStream was destroyed");
    }
    return JS_UNDEFINED;
}


/* (source, volume, pan, pitch, loop) — all validated by the prelude */
JSValue efx_js_audio_play_wire(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 5) {
        return efx_api_type_error(ctx, "playAudio wire native requires 5 arguments");
    }
    efxjs_audiodata *ad = JS_GetOpaque(argv[0], audiodata_class_id);
    efxjs_audiostream *as = JS_GetOpaque(argv[0], audiostream_class_id);
    if (!ad && !as) {
        return efx_api_type_error(ctx, "playAudio requires an AudioData or AudioStream");
    }
    float volume = (float)audio_arg(ctx, argv[1]);
    float pan = (float)audio_arg(ctx, argv[2]);
    float pitch = (float)audio_arg(ctx, argv[3]);
    int loop = JS_ToBool(ctx, argv[4]) ? 1 : 0;
    int voice;
    if (ad) {
        voice = efx_audio_play_data(ad->data, volume, pan, pitch, loop);
    } else {
        voice = efx_audio_play_stream(as->stream, volume, pan, pitch, loop);
    }
    if (voice < 0) {
        return JS_NULL;
    }
    efxjs_audio *a = calloc(1, sizeof(*a));
    if (!a) {
        efx_audio_stop_voice(voice);
        return efx_api_generic_error(ctx, "out of memory");
    }
    a->voice = voice;
    a->serial = efx_audio_voice_serial(voice);
    a->alive = 1;
    a->volume = volume;
    a->pan = pan;
    a->pitch = pitch;
    a->loop = loop;
    JSValue obj = JS_NewObjectClass(ctx, audio_class_id);
    JS_SetOpaque(obj, a);
    return obj;
}


static JSValue efx_js_audio_get_master(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    return JS_NewFloat64(ctx, (double)efx_audio_master_volume());
}


static JSValue efx_js_audio_set_master(JSContext *ctx, JSValueConst this_val,
                                       JSValueConst val) {
    (void)this_val;
    double d = 0.0;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || d < 0.0) {
        return efx_api_range_error(ctx, "volume must be a non-negative number");
    }
    efx_audio_set_master_volume((float)d);
    return JS_UNDEFINED;
}


static JSValue efx_js_audio_resume(JSContext *ctx, JSValueConst this_val, int argc,
                                   JSValueConst *argv) {
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    efx_audio_request_resume();
    return JS_UNDEFINED;
}


int efx_api_register_audio(JSContext *ctx, JSValueConst efx) {
    static const JSCFunctionListEntry audio_funcs[] = {
        /* loadAudioData/loadAudioStream/playAudio are installed by the
         * shared prelude (ADR 0049) */
        JS_CGETSET_DEF("volume", efx_js_audio_get_master,
                       efx_js_audio_set_master),
        JS_CFUNC_DEF("resume", 0, efx_js_audio_resume),
    };
    JSValue audio = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, audio, audio_funcs,
                               (int)(sizeof(audio_funcs) /
                                     sizeof(audio_funcs[0])));
    /* JS_SetPropertyStr consumes the value reference */
    JS_SetPropertyStr(ctx, efx, "audio", audio);
    return 0;
}
