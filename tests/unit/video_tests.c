/*
 * Headless unit tests for the video-playback core (ADR 0063): MPEG-1 open +
 * metadata, decode determinism, the playback state machine (play/pause/
 * loop/ended/restart), bounded catch-up on stalls, audio-ring pulls, the
 * silent-video path, and non-MPEG-1 rejection.
 *
 * Usage: efx_video_tests [<case>] ; exit 0 = pass.
 * The MPEG-1 fixtures are embedded at configure time (efx_video_fixtures.h)
 * so the suite is portable and needs no filesystem.
 */
#include "video/efx_video.h"
#include "efx_video_fixtures.h"

#include "../test_support.h"

#include <stdlib.h>
#include <string.h>

/* the procedural fixture: testsrc2 64x64 @ 25 fps, 1 s, 440 Hz MP2 44.1 kHz */
#define FIX_W 64
#define FIX_H 64
#define FIX_FPS 25.0
#define FIX_RATE 44100

static efx_video *open_procedural(void) {
    efx_video *v = NULL;
    int rc = efx_video_open(EFX_TEST_MPG, EFX_TEST_MPG_SIZE, &v);
    if (rc != EFX_VIDEO_OK) {
        fail("procedural open");
    }
    return v;
}

static int any_nonzero(const uint8_t *px, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (px[i]) {
            return 1;
        }
    }
    return 0;
}

static int open_metadata(void) {
    efx_video *v = open_procedural();
    if (efx_video_width(v) != FIX_W || efx_video_height(v) != FIX_H) {
        return fail("dimensions");
    }
    if (!feq((float)efx_video_fps(v), (float)FIX_FPS)) {
        return fail("fps");
    }
    if (!(feq((float)efx_video_duration(v), 0.96f) && efx_video_duration(v) <= 1.0) ||
        efx_video_duration(v) > 1.0) {
        return fail("duration");
    }
    if (!efx_video_has_audio(v) || efx_video_audio_rate(v) != FIX_RATE) {
        return fail("audio stream");
    }
    if (efx_video_playing(v) || efx_video_ended(v)) {
        return fail("starts playing/ended");
    }
    if (efx_video_time(v) != 0.0) {
        return fail("starts at 0");
    }
    if (!any_nonzero(efx_video_frame_rgba(v), FIX_W * FIX_H * 4)) {
        return fail("first frame decoded");
    }
    /* every frame is opaque (pl_mpeg's RGBA conversion leaves alpha unwritten;
     * the wrapper forces 255 so video blends like any textured quad) */
    {
        const uint8_t *px = efx_video_frame_rgba(v);
        for (int i = 0; i < FIX_W * FIX_H; i++) {
            if (px[i * 4 + 3] != 255) {
                return fail("frame alpha not opaque");
            }
        }
    }
    efx_video_destroy(v);

    /* silent fixture */
    efx_video *s = NULL;
    if (efx_video_open(EFX_TEST_MPG_SILENT, EFX_TEST_MPG_SILENT_SIZE, &s) !=
        EFX_VIDEO_OK) {
        return fail("silent open");
    }
    if (efx_video_has_audio(s) || efx_video_audio_rate(s) != 0) {
        return fail("silent has audio");
    }
    float buf[64 * 2];
    if (efx_video_pull_audio(s, buf, 64) != 0) {
        return fail("silent pull");
    }
    efx_video_destroy(s);
    return 0;
}

/* identical inputs decode to identical bytes: RGBA sequence + audio ring */
static int decode_determinism(void) {
    efx_video *a = open_procedural();
    efx_video *b = open_procedural();
    static uint8_t ra[FIX_W * FIX_H * 4];
    static uint8_t rb[FIX_W * FIX_H * 4];
    static float fa[4096 * 2];
    static float fb[4096 * 2];
    for (int step = 0; step < 12; step++) {
        int na = 0;
        int nb = 0;
        if (efx_video_advance(a, 0.04, &na) != EFX_VIDEO_OK ||
            efx_video_advance(b, 0.04, &nb) != EFX_VIDEO_OK) {
            return fail("advance");
        }
        if (na != nb) {
            return fail("frame production diverged");
        }
        if (na) {
            memcpy(ra, efx_video_frame_rgba(a), sizeof(ra));
            memcpy(rb, efx_video_frame_rgba(b), sizeof(rb));
            if (memcmp(ra, rb, sizeof(ra)) != 0) {
                return fail("rgba diverged");
            }
        }
        if (efx_video_time(a) != efx_video_time(b)) {
            return fail("clock diverged");
        }
        uint64_t pa = efx_video_pull_audio(a, fa, 4096);
        uint64_t pb = efx_video_pull_audio(b, fb, 4096);
        if (pa != pb || memcmp(fa, fb, pa * 2 * sizeof(float)) != 0) {
            return fail("audio diverged");
        }
    }
    efx_video_destroy(a);
    efx_video_destroy(b);
    return 0;
}

static int state_machine(void) {
    efx_video *v = open_procedural();
    /* paused: advance is a no-op */
    if (efx_video_advance(v, 0.5, NULL) != EFX_VIDEO_OK) {
        return fail("paused advance");
    }
    if (efx_video_time(v) != 0.0) {
        return fail("paused time moved");
    }
    /* play: frames due in the window decode */
    efx_video_set_playing(v, 1);
    int frames = 0;
    if (efx_video_advance(v, 0.04, &frames) != EFX_VIDEO_OK || !frames) {
        return fail("first advance produced no frame");
    }
    if (!feq((float)efx_video_time(v), 0.04f)) {
        return fail("time advance");
    }
    /* pause holds the playhead exactly */
    efx_video_set_playing(v, 0);
    if (efx_video_advance(v, 0.4, NULL) != EFX_VIDEO_OK ||
        !feq((float)efx_video_time(v), 0.04f)) {
        return fail("pause did not hold");
    }
    /* a video-only stall lands on the end state */
    efx_video_set_playing(v, 1);
    if (efx_video_advance(v, 10.0, NULL) != EFX_VIDEO_OK) {
        return fail("stall advance");
    }
    if (!efx_video_ended(v) || efx_video_playing(v)) {
        return fail("end state");
    }
    if (efx_video_time(v) != efx_video_duration(v)) {
        return fail("end clamps to duration");
    }
    /* play() after ending restarts from the beginning */
    efx_video_set_playing(v, 1);
    if (efx_video_ended(v) || efx_video_time(v) != 0.0) {
        return fail("restart");
    }
    efx_video_destroy(v);
    return 0;
}

static int loop_wrap(void) {
    efx_video *v = open_procedural();
    efx_video_set_loop(v, 1);
    efx_video_set_playing(v, 1);
    if (efx_video_advance(v, efx_video_duration(v) + 0.1, NULL) !=
        EFX_VIDEO_OK) {
        return fail("loop advance");
    }
    if (efx_video_ended(v)) {
        return fail("loop ended");
    }
    double t = efx_video_time(v);
    if (!(t >= 0.0 && t < efx_video_duration(v))) {
        return fail("loop wrap range");
    }
    efx_video_destroy(v);
    return 0;
}

static int bounded_catchup(void) {
    /* decode work per advance is capped: a huge stall must not run long —
       observable behavior is the jump + end state, returned promptly */
    efx_video *v = open_procedural();
    efx_video_set_playing(v, 1);
    int total = 0;
    for (int i = 0; i < 8; i++) {
        int n = 0;
        if (efx_video_advance(v, 0.04, &n) != EFX_VIDEO_OK) {
            return fail("catchup advance");
        }
        total += n;
    }
    if (total != 8) {
        return fail("one frame per 25fps tick");
    }
    efx_video_destroy(v);
    return 0;
}

static int audio_pull(void) {
    efx_video *v = open_procedural();
    efx_video_set_playing(v, 1);
    /* ~0.2 s of 44.1 kHz audio decodes into the ring across the window */
    for (int i = 0; i < 5; i++) {
        efx_video_advance(v, 0.04, NULL);
    }
    if (efx_video_audio_buffered(v) == 0) {
        return fail("ring empty after decode window");
    }
    static float buf[2048 * 2];
    uint64_t before = efx_video_audio_buffered(v);
    uint64_t got = efx_video_pull_audio(v, buf, 2048);
    if (got == 0 || got > 2048) {
        return fail("pull size");
    }
    if (efx_video_audio_buffered(v) != before - got) {
        /* exact bookkeeping: buffered dropped by exactly `got` */
        return fail("ring accounting");
    }
    int nonzero = 0;
    for (uint64_t i = 0; i < got * 2; i++) {
        if (buf[i] > 1e-4f || buf[i] < -1e-4f) {
            nonzero = 1;
            break;
        }
    }
    if (!nonzero) {
        return fail("sine silent");
    }
    efx_video_destroy(v);
    return 0;
}

static int reject_garbage(void) {
    efx_video *v = NULL;
    static const uint8_t png[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13
    };
    if (efx_video_open(png, sizeof(png), &v) != EFX_VIDEO_ERR_FORMAT) {
        return fail("png accepted");
    }
    if (efx_video_open(EFX_TEST_MPG, 8, &v) != EFX_VIDEO_ERR_FORMAT) {
        return fail("truncated accepted");
    }
    if (efx_video_open(NULL, 0, &v) != EFX_VIDEO_ERR_ARG) {
        return fail("null accepted");
    }
    if (efx_video_advance(NULL, 0.016, NULL) != EFX_VIDEO_ERR_ARG) {
        return fail("null advance");
    }
    efx_video_destroy(NULL); /* NULL safe */
    return 0;
}

static const efx_test_case cases[] = {
    EFX_CASE(open_metadata),
    EFX_CASE(decode_determinism),
    EFX_CASE(state_machine),
    EFX_CASE(loop_wrap),
    EFX_CASE(bounded_catchup),
    EFX_CASE(audio_pull),
    EFX_CASE(reject_garbage),
};

int main(int argc, char **argv) {
    return efx_test_main(cases, sizeof(cases) / sizeof(cases[0]), argc, argv);
}
