#include "runtime/clock.h"

/* Max seconds a single frame may advance the script clock. A stall (window
 * drag, background tab, debugger pause) must not teleport time-driven
 * animation; 0.05 matches the per-frame clamp every curated game sample
 * hand-rolls. */
#define EFX_CLOCK_MAX_STEP 0.05

static double g_clock_now;

void efx_clock_begin_frame(double raw_dt) {
    if (!(raw_dt > 0.0)) {
        return; /* first frame / paused: the snapshot stays put */
    }
    if (raw_dt > EFX_CLOCK_MAX_STEP) {
        raw_dt = EFX_CLOCK_MAX_STEP;
    }
    g_clock_now += raw_dt;
}

double efx_clock_now(void) {
    return g_clock_now;
}
