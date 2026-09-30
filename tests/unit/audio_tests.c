/*
 * Headless unit tests for the F14 audio core (no device, no script runtime):
 * WAV/MP3 decoding, mixing determinism, resampling/pitch, error handling, the
 * fixed voice bank + steal policy, static and streamed source kinds, the
 * concurrent-stream cap, the master gain, source lifetime, and the no-device
 * soft-fail path.
 *
 * Usage: efx_audio_tests <case> ; exit 0 = pass.
 * The WAV/MP3 fixtures are embedded at configure time (efx_audio_fixtures.h)
 * so the suite is portable and needs no filesystem.
 */
#include "audio/audio.h"
#include "efx_audio_fixtures.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float g_buf[8192 * 2];

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

/* expected decoded value of the 800-frame 440 Hz / 0.5 amp sine fixture */
static double expected_wav(double i) {
    return round(0.5 * 32767.0 * sin(2.0 * M_PI * 440.0 * i / 8000.0)) /
           32768.0;
}

/* the mixer linearly interpolates between decoded source frames */
static double expected_lerp(double p) {
    int i0 = (int)floor(p);
    double f = p - (double)i0;
    return expected_wav((double)i0) * (1.0 - f) + expected_wav((double)i0 + 1.0) * f;
}

static int any_nonzero(const float *b, int frames) {
    for (int i = 0; i < frames * 2; i++) {
        if (fabsf(b[i]) > 1e-6f) {
            return 1;
        }
    }
    return 0;
}

static int all_zero(const float *b, int frames) {
    for (int i = 0; i < frames * 2; i++) {
        if (b[i] != 0.0f) {
            return 0;
        }
    }
    return 1;
}

static efx_audio_data *load_wav(int *err) {
    return efx_audio_data_load(EFX_TEST_WAV, EFX_TEST_WAV_SIZE, err);
}

/* --------------------------------------------------------------- decoding */

static int decode_wav(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_data *d = load_wav(&err);
    if (!d || err != EFX_AUDIO_OK) {
        return fail("wav load");
    }
    if (efx_audio_data_frames(d) != 800) {
        return fail("wav frame count");
    }
    if (efx_audio_data_rate(d) != 8000) {
        return fail("wav sample rate");
    }
    efx_audio_data_release(d);
    efx_audio_shutdown();
    return 0;
}

static int decode_mp3(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_data *d =
        efx_audio_data_load(EFX_TEST_MP3, EFX_TEST_MP3_SIZE, &err);
    if (!d || err != EFX_AUDIO_OK) {
        return fail("mp3 load");
    }
    if (efx_audio_data_frames(d) == 0) {
        return fail("mp3 frame count");
    }
    if (efx_audio_data_rate(d) != 8000) {
        return fail("mp3 sample rate");
    }
    int v = efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 0);
    if (v < 0) {
        return fail("mp3 play");
    }
    efx_audio_mix(g_buf, 256);
    if (!any_nonzero(g_buf, 256)) {
        return fail("mp3 produced silence");
    }
    for (int i = 0; i < 512; i++) {
        if (!isfinite(g_buf[i])) {
            return fail("mp3 non-finite sample");
        }
    }
    efx_audio_data_release(d);
    efx_audio_shutdown();
    return 0;
}

/* ----------------------------------------------------------- mix fidelity */

static int mix_matches(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_data *d = load_wav(&err);
    if (!d) {
        return fail("load");
    }
    int v = efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 0);
    if (v < 0) {
        return fail("play");
    }
    efx_audio_mix(g_buf, 800);
    /* device rate == source rate, pitch 1 => step 1, no interpolation: the
     * first 799 output frames reproduce the source exactly (the final frame
     * needs a lookahead sample, so the voice ends there) */
    for (int i = 0; i < 799; i++) {
        double exp = expected_wav((double)i);
        if (fabs((double)g_buf[i * 2] - exp) > 1e-3 ||
            fabs((double)g_buf[i * 2 + 1] - exp) > 1e-3) {
            fprintf(stderr, "FAIL: mix frame %d got %f/%f exp %f\n", i,
                    (double)g_buf[i * 2], (double)g_buf[i * 2 + 1], exp);
            return 1;
        }
    }
    if (efx_audio_voice_playing(v)) {
        return fail("voice should have ended");
    }
    efx_audio_data_release(d);
    efx_audio_shutdown();
    return 0;
}

static int resample_pitch(void) {
    /* half rate: 8 kHz source on a 16 kHz device => 0.5 source frames/out */
    efx_audio_init(16000);
    int err = -1;
    efx_audio_data *d = load_wav(&err);
    if (!d) {
        return fail("load");
    }
    int v = efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 0);
    if (v < 0) {
        return fail("play");
    }
    efx_audio_mix(g_buf, 1600);
    for (int j = 0; j < 1500; j++) {
        double exp = expected_lerp((double)j * 0.5);
        if (fabs((double)g_buf[j * 2] - exp) > 1e-4) {
            fprintf(stderr, "FAIL: resample frame %d got %f exp %f\n", j,
                    (double)g_buf[j * 2], exp);
            return 1;
        }
    }
    /* pitch 2: two source frames per output frame */
    efx_audio_init(8000);
    efx_audio_data *d2 = load_wav(&err);
    if (!d2) {
        return fail("load2");
    }
    int v2 = efx_audio_play_data(d2, 1.0f, 0.0f, 2.0f, 0);
    if (v2 < 0) {
        return fail("play2");
    }
    efx_audio_mix(g_buf, 400);
    for (int j = 0; j < 398; j++) {
        double exp = expected_lerp((double)j * 2.0);
        if (fabs((double)g_buf[j * 2] - exp) > 1e-4) {
            fprintf(stderr, "FAIL: pitch frame %d got %f exp %f\n", j,
                    (double)g_buf[j * 2], exp);
            return 1;
        }
    }
    efx_audio_data_release(d);
    efx_audio_data_release(d2);
    efx_audio_shutdown();
    return 0;
}

static int mix_determinism(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_data *d = load_wav(&err);
    if (!d) {
        return fail("load");
    }
    if (efx_audio_play_data(d, 0.8f, 0.25f, 1.3f, 0) < 0) {
        return fail("play");
    }
    efx_audio_mix(g_buf, 300);
    float first[300 * 2];
    memcpy(first, g_buf, sizeof(first));
    efx_audio_data_release(d);

    efx_audio_init(8000);
    efx_audio_data *d2 = load_wav(&err);
    if (!d2) {
        return fail("load2");
    }
    if (efx_audio_play_data(d2, 0.8f, 0.25f, 1.3f, 0) < 0) {
        return fail("play2");
    }
    efx_audio_mix(g_buf, 300);
    if (memcmp(first, g_buf, sizeof(first)) != 0) {
        return fail("mix not deterministic");
    }
    efx_audio_data_release(d2);
    efx_audio_shutdown();
    return 0;
}

/* ----------------------------------------------------------------- errors */

static int errors(void) {
    efx_audio_init(8000);
    int err = 0;
    if (efx_audio_data_load(NULL, 0, &err) != NULL ||
        err != EFX_AUDIO_ERR_ARG) {
        return fail("null load");
    }
    if (efx_audio_data_load(EFX_TEST_WAV, 16, &err) != NULL ||
        err != EFX_AUDIO_ERR_FORMAT) {
        return fail("truncated wav");
    }
    static const unsigned char garbage[16] = {
        0xff, 0xff, 0xff, 0xff, 0x00, 0x11, 0x22, 0x33,
        0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb};
    if (efx_audio_data_load(garbage, sizeof(garbage), &err) != NULL ||
        err != EFX_AUDIO_ERR_FORMAT) {
        return fail("garbage data load");
    }
    if (efx_audio_stream_load(garbage, sizeof(garbage), &err) != NULL ||
        err != EFX_AUDIO_ERR_FORMAT) {
        return fail("garbage stream load");
    }
    if (efx_audio_stream_load(NULL, 0, &err) != NULL ||
        err != EFX_AUDIO_ERR_ARG) {
        return fail("null stream load");
    }
    efx_audio_shutdown();
    return 0;
}

/* ----------------------------------------------------------- voice bank */

static int voices_steal(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_data *d = load_wav(&err);
    if (!d) {
        return fail("load");
    }
    int ids[EFX_AUDIO_MAX_VOICES];
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        ids[i] = efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 0);
        if (ids[i] != i) {
            return fail("voice allocation order");
        }
    }
    if (efx_audio_active_voice_count() != EFX_AUDIO_MAX_VOICES) {
        return fail("active count full");
    }
    /* quietest non-looping voice is stolen */
    efx_audio_set_voice_volume(5, 0.01f);
    int stolen = efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 0);
    if (stolen != 5) {
        fprintf(stderr, "FAIL: expected steal of voice 5, got %d\n", stolen);
        return 1;
    }
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        efx_audio_stop_voice(ids[i]);
    }
    /* all looping => reject */
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        if (efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 1) < 0) {
            return fail("looping allocation");
        }
    }
    if (efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 1) != -1) {
        return fail("all-looping must reject");
    }
    efx_audio_data_release(d);
    efx_audio_shutdown();
    return 0;
}

/* --------------------------------------------------------------- streams */

static int stream_plays(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_stream *s =
        efx_audio_stream_load(EFX_TEST_WAV, EFX_TEST_WAV_SIZE, &err);
    if (!s) {
        return fail("stream load");
    }
    int v = efx_audio_play_stream(s, 1.0f, 0.0f, 1.0f, 0);
    if (v < 0) {
        return fail("stream play");
    }
    if (!efx_audio_voice_playing(v)) {
        return fail("stream should be playing");
    }
    efx_audio_pump();
    efx_audio_mix(g_buf, 200);
    if (!any_nonzero(g_buf, 200)) {
        return fail("stream silence");
    }
    /* drain past the end without looping */
    for (int k = 0; k < 20; k++) {
        efx_audio_pump();
        efx_audio_mix(g_buf, 200);
    }
    if (efx_audio_voice_playing(v)) {
        return fail("stream should have ended");
    }
    /* looped stream keeps playing across the wrap */
    efx_audio_stop_voice(v);
    int v2 = efx_audio_play_stream(s, 1.0f, 0.0f, 1.0f, 1);
    if (v2 < 0) {
        return fail("stream loop play");
    }
    for (int k = 0; k < 12; k++) {
        efx_audio_pump();
        efx_audio_mix(g_buf, 200);
        if (!efx_audio_voice_playing(v2)) {
            return fail("looped stream stopped");
        }
        if (!any_nonzero(g_buf, 200)) {
            return fail("looped stream silence");
        }
    }
    efx_audio_stream_release(s);
    efx_audio_shutdown();
    return 0;
}

static int stream_controls(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_stream *s =
        efx_audio_stream_load(EFX_TEST_WAV, EFX_TEST_WAV_SIZE, &err);
    if (!s) {
        return fail("stream load");
    }
    int v = efx_audio_play_stream(s, 1.0f, 0.0f, 1.0f, 1);
    if (v < 0) {
        return fail("stream play");
    }
    efx_audio_pump();
    efx_audio_mix(g_buf, 100);
    if (!any_nonzero(g_buf, 100)) {
        return fail("stream should be audible");
    }
    efx_audio_set_voice_paused(v, 1);
    if (efx_audio_voice_playing(v) || !efx_audio_voice_paused(v)) {
        return fail("pause state");
    }
    efx_audio_mix(g_buf, 100);
    if (!all_zero(g_buf, 100)) {
        return fail("paused stream must be silent");
    }
    efx_audio_set_voice_paused(v, 0);
    if (!efx_audio_voice_playing(v)) {
        return fail("resume state");
    }
    efx_audio_set_voice_volume(v, 0.0f);
    efx_audio_pump();
    efx_audio_mix(g_buf, 100);
    if (!all_zero(g_buf, 100)) {
        return fail("zero volume must be silent");
    }
    efx_audio_stop_voice(v);
    if (efx_audio_voice_playing(v)) {
        return fail("stop state");
    }
    efx_audio_stream_release(s);
    efx_audio_shutdown();
    return 0;
}

static int stream_cap(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_stream *s =
        efx_audio_stream_load(EFX_TEST_WAV, EFX_TEST_WAV_SIZE, &err);
    if (!s) {
        return fail("stream load");
    }
    int ids[EFX_AUDIO_MAX_STREAMS];
    for (int i = 0; i < EFX_AUDIO_MAX_STREAMS; i++) {
        ids[i] = efx_audio_play_stream(s, 1.0f, 0.0f, 1.0f, 0);
        if (ids[i] < 0) {
            return fail("stream allocation");
        }
    }
    if (efx_audio_stream_voice_count() != EFX_AUDIO_MAX_STREAMS) {
        return fail("stream count");
    }
    /* a further (non-looping) stream steals a stream voice, staying at cap */
    int extra = efx_audio_play_stream(s, 1.0f, 0.0f, 1.0f, 0);
    if (extra < 0) {
        return fail("stream steal");
    }
    if (efx_audio_stream_voice_count() != EFX_AUDIO_MAX_STREAMS) {
        return fail("stream cap exceeded");
    }
    /* with every stream looping, a new stream is rejected deterministically */
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        efx_audio_stop_voice(i);
    }
    for (int i = 0; i < EFX_AUDIO_MAX_STREAMS; i++) {
        if (efx_audio_play_stream(s, 1.0f, 0.0f, 1.0f, 1) < 0) {
            return fail("looping stream allocation");
        }
    }
    if (efx_audio_play_stream(s, 1.0f, 0.0f, 1.0f, 1) != -1) {
        return fail("all-looping streams must reject");
    }
    efx_audio_stream_release(s);
    efx_audio_shutdown();
    return 0;
}

/* --------------------------------------------------------------- lifetime */

static int source_lifetime(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_data *d = load_wav(&err);
    if (!d) {
        return fail("load");
    }
    int v = efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 0);
    if (v < 0) {
        return fail("play");
    }
    efx_audio_data_release(d); /* the voice retains the source */
    efx_audio_mix(g_buf, 200);
    if (!any_nonzero(g_buf, 200)) {
        return fail("released source went silent");
    }
    efx_audio_stop_voice(v);
    efx_audio_shutdown();

    efx_audio_init(8000);
    efx_audio_stream *s =
        efx_audio_stream_load(EFX_TEST_WAV, EFX_TEST_WAV_SIZE, &err);
    if (!s) {
        return fail("stream load");
    }
    int sv = efx_audio_play_stream(s, 1.0f, 0.0f, 1.0f, 0);
    if (sv < 0) {
        return fail("stream play");
    }
    efx_audio_stream_release(s); /* the playhead retains the source */
    efx_audio_pump();
    efx_audio_mix(g_buf, 200);
    if (!any_nonzero(g_buf, 200)) {
        return fail("released stream went silent");
    }
    efx_audio_stop_voice(sv);
    efx_audio_shutdown();
    return 0;
}

/* --------------------------------------------------------------- master */

static int master_gain(void) {
    efx_audio_init(8000);
    int err = -1;
    efx_audio_data *d = load_wav(&err);
    if (!d) {
        return fail("load");
    }
    efx_audio_set_master_volume(0.0f);
    if (efx_audio_master_volume() != 0.0f) {
        return fail("master getter");
    }
    if (efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 0) < 0) {
        return fail("play");
    }
    efx_audio_mix(g_buf, 100);
    if (!all_zero(g_buf, 100)) {
        return fail("zero master must be silent");
    }
    efx_audio_set_master_volume(1.0f);
    efx_audio_mix(g_buf, 100);
    if (!any_nonzero(g_buf, 100)) {
        return fail("master restore");
    }
    efx_audio_data_release(d);
    efx_audio_shutdown();
    return 0;
}

static int unavailable(void) {
    efx_audio_init(8000);
    efx_audio_set_available(0);
    int err = -1;
    efx_audio_data *d = load_wav(&err);
    if (!d) {
        return fail("load");
    }
    int v = efx_audio_play_data(d, 1.0f, 0.0f, 1.0f, 0);
    if (v < 0) {
        return fail("play must still allocate with no device");
    }
    if (efx_audio_voice_playing(v)) {
        return fail("must report not playing with no device");
    }
    efx_audio_stream *s =
        efx_audio_stream_load(EFX_TEST_WAV, EFX_TEST_WAV_SIZE, &err);
    if (!s) {
        return fail("stream load");
    }
    int sv = efx_audio_play_stream(s, 1.0f, 0.0f, 1.0f, 0);
    if (sv < 0) {
        return fail("stream play pre-unlock");
    }
    if (efx_audio_voice_playing(sv)) {
        return fail("stream must report not playing pre-unlock");
    }
    efx_audio_pump();
    efx_audio_mix(g_buf, 100);
    if (!all_zero(g_buf, 100)) {
        return fail("no-device mix must be silent");
    }
    efx_audio_set_available(1);
    efx_audio_pump();
    if (!efx_audio_voice_playing(v)) {
        return fail("static must play after unlock");
    }
    efx_audio_mix(g_buf, 100);
    if (!any_nonzero(g_buf, 100)) {
        return fail("audio must be audible after unlock");
    }
    efx_audio_data_release(d);
    efx_audio_stream_release(s);
    efx_audio_shutdown();
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: efx_audio_tests <case>\n");
        return 2;
    }
    const char *c = argv[1];
    if (!strcmp(c, "decode_wav")) return decode_wav();
    if (!strcmp(c, "decode_mp3")) return decode_mp3();
    if (!strcmp(c, "mix_matches")) return mix_matches();
    if (!strcmp(c, "resample_pitch")) return resample_pitch();
    if (!strcmp(c, "mix_determinism")) return mix_determinism();
    if (!strcmp(c, "errors")) return errors();
    if (!strcmp(c, "voices_steal")) return voices_steal();
    if (!strcmp(c, "stream_plays")) return stream_plays();
    if (!strcmp(c, "stream_controls")) return stream_controls();
    if (!strcmp(c, "stream_cap")) return stream_cap();
    if (!strcmp(c, "source_lifetime")) return source_lifetime();
    if (!strcmp(c, "master_gain")) return master_gain();
    if (!strcmp(c, "unavailable")) return unavailable();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
