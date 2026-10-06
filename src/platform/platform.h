#ifndef EFX_PLATFORM_H
#define EFX_PLATFORM_H

typedef struct efx_frame_hooks {
    void *ud;
    /* called once after the rendering surface and engine subsystems are ready,
     * before the first frame; a non-zero return stops the run (the callback
     * has recorded the exit code via efx_platform_set_exit_code) */
    int (*on_init)(void *ud);
    int (*on_frame)(void *ud, double dt); /* dt: seconds since previous frame */
    /* native drag-and-drop: called with the first dropped path (UTF-8,
     * absolute) when files are dropped on the window; optional (NULL ignores
     * drops). Not fired on Emscripten, where the boot JS owns drops. */
    void (*on_files_dropped)(void *ud, const char *path);
} efx_frame_hooks;

typedef struct efx_platform_capture {
    int frame;         /* capture after this 1-based frame; 0 = off */
    const char *output; /* PNG output path */
} efx_platform_capture;

typedef struct efx_platform_desc {
    int width, height;      /* window size; 0 = default */
    efx_platform_capture capture;
} efx_platform_desc;

int efx_platform_run(const efx_platform_desc *desc, efx_frame_hooks hooks);
/* Record the code a deliberate windowed quit should exit with. On
 * Linux/Windows `efx_platform_run` returns and the caller propagates it;
 * macOS's Cocoa run loop never returns (sokol), so the platform layer exits
 * with the recorded code when the frame callback requests a stop. */
void efx_platform_set_exit_code(int code);
void efx_platform_shutdown(void); /* after callers released GPU resources */

#endif
