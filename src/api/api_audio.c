#include "api/api_internal.h"


/* ================================================= F14 audio bindings */

static int audio_opt_number(JSContext *ctx, JSValueConst opts, const char *key,
                            double *out) {
    JSValue v = JS_GetPropertyStr(ctx, opts, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    double d = 0.0;
    int bad = !JS_IsNumber(v) || JS_ToFloat64(ctx, &d, v) < 0 || !isfinite(d);
    JS_FreeValue(ctx, v);
    if (bad) {
        JS_ThrowTypeError(ctx, "%s must be a finite number", key);
        return -1;
    }
    *out = d;
    return 1;
}


static int audio_opt_bool(JSContext *ctx, JSValueConst opts, const char *key,
                          int *out) {
    JSValue v = JS_GetPropertyStr(ctx, opts, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (!JS_IsBool(v)) {
        JS_FreeValue(ctx, v);
        JS_ThrowTypeError(ctx, "%s must be a boolean", key);
        return -1;
    }
    *out = JS_ToBool(ctx, v) ? 1 : 0;
    JS_FreeValue(ctx, v);
    return 1;
}


/* reads a resource for a loader; on failure throws and returns NULL */
static uint8_t *audio_read_resource(JSContext *ctx, JSValueConst pathv,
                                    const char *fn, size_t *out_size) {
    if (!JS_IsString(pathv)) {
        JS_ThrowTypeError(ctx, "%s requires a path string", fn);
        return NULL;
    }
    const char *path = JS_ToCString(ctx, pathv);
    if (!path) {
        return NULL;
    }
    struct efx_host_state *h = efx_api_host_state(ctx);
    if (!h->resource) {
        JS_FreeCString(ctx, path);
        JS_ThrowInternalError(ctx, "%s requires a resource root", fn);
        return NULL;
    }
    size_t size = 0;
    int rerr = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(h->resource, path, &size, &rerr);
    if (!bytes) {
        JS_ThrowInternalError(ctx, "cannot read audio: %s", path);
    }
    JS_FreeCString(ctx, path);
    *out_size = size;
    return bytes;
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

static JSValue efx_js_audio_loadAudioData(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "loadAudioData requires a path string");
    }
    size_t size = 0;
    uint8_t *bytes = audio_read_resource(ctx, argv[0], "loadAudioData", &size);
    if (!bytes) {
        return JS_EXCEPTION;
    }
    int derr = 0;
    efx_audio_data *data = efx_audio_data_load(bytes, size, &derr);
    efx_resource_free(bytes);
    if (!data) {
        return efx_api_generic_error(ctx, "cannot decode audio");
    }
    efxjs_audiodata *o = calloc(1, sizeof(*o));
    if (!o) {
        efx_audio_data_release(data);
        return efx_api_generic_error(ctx, "out of memory");
    }
    o->data = data;
    o->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, audiodata_class_id);
    JS_SetOpaque(obj, o);
    return obj;
}


static JSValue efx_js_audio_loadAudioStream(JSContext *ctx, JSValueConst this_val,
                                            int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "loadAudioStream requires a path string");
    }
    size_t size = 0;
    uint8_t *bytes =
        audio_read_resource(ctx, argv[0], "loadAudioStream", &size);
    if (!bytes) {
        return JS_EXCEPTION;
    }
    int derr = 0;
    efx_audio_stream *stream = efx_audio_stream_load(bytes, size, &derr);
    efx_resource_free(bytes);
    if (!stream) {
        return efx_api_generic_error(ctx, "cannot decode audio");
    }
    efxjs_audiostream *o = calloc(1, sizeof(*o));
    if (!o) {
        efx_audio_stream_release(stream);
        return efx_api_generic_error(ctx, "out of memory");
    }
    o->stream = stream;
    o->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, audiostream_class_id);
    JS_SetOpaque(obj, o);
    return obj;
}


static JSValue efx_js_audio_playAudio(JSContext *ctx, JSValueConst this_val, int argc,
                                      JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "playAudio requires an AudioData or AudioStream");
    }
    efxjs_audiodata *ad = JS_GetOpaque(argv[0], audiodata_class_id);
    efxjs_audiostream *as = JS_GetOpaque(argv[0], audiostream_class_id);
    if (!ad && !as) {
        return efx_api_type_error(ctx, "playAudio requires an AudioData or AudioStream");
    }
    float volume = 1.0f;
    float pan = 0.0f;
    float pitch = 1.0f;
    int loop = 0;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return efx_api_type_error(ctx, "playAudio options must be an object");
        }
        static const char *known[] = {"volume", "pan", "pitch", "loop"};
        if (efx_api_check_known_fields(ctx, argv[1], known, 4, "playAudio") != 0) {
            return JS_EXCEPTION;
        }
        double n = 0.0;
        int r;
        if ((r = audio_opt_number(ctx, argv[1], "volume", &n)) < 0) {
            return JS_EXCEPTION;
        }
        if (r) {
            if (n < 0.0) {
                return efx_api_range_error(ctx, "volume must be a non-negative number");
            }
            volume = (float)n;
        }
        if ((r = audio_opt_number(ctx, argv[1], "pan", &n)) < 0) {
            return JS_EXCEPTION;
        }
        if (r) {
            pan = (float)n;
        }
        if ((r = audio_opt_number(ctx, argv[1], "pitch", &n)) < 0) {
            return JS_EXCEPTION;
        }
        if (r) {
            pitch = (n > 0.0) ? (float)n : 1.0f;
        }
        if ((r = audio_opt_bool(ctx, argv[1], "loop", &loop)) < 0) {
            return JS_EXCEPTION;
        }
    }
    int voice;
    if (ad) {
        if (!ad->alive || !ad->data) {
            return efx_api_generic_error(ctx, "AudioData was destroyed");
        }
        voice = efx_audio_play_data(ad->data, volume, pan, pitch, loop);
    } else {
        if (!as->alive || !as->stream) {
            return efx_api_generic_error(ctx, "AudioStream was destroyed");
        }
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
        JS_CFUNC_DEF("loadAudioData", 1, efx_js_audio_loadAudioData),
        JS_CFUNC_DEF("loadAudioStream", 1, efx_js_audio_loadAudioStream),
        JS_CFUNC_DEF("playAudio", 2, efx_js_audio_playAudio),
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
