#ifndef EFX_CLOCK_H
#define EFX_CLOCK_H

/* Script-facing engine clock (efx.time.now). Pure C, no quickjs/Emscripten
 * dependencies, so both bindings (desktop quickjs and the web bridge) share
 * one implementation. The frame driver advances the clock once per frame;
 * reads within a frame observe the snapshot taken there. */

/* advance the clock for one frame: adds min(raw_dt, EFX_CLOCK_MAX_STEP);
 * raw_dt <= 0 leaves the clock unchanged (first frame, paused window) */
void efx_clock_begin_frame(double raw_dt);

/* seconds since the script started (frame-start snapshot) */
double efx_clock_now(void);

#endif /* EFX_CLOCK_H */
