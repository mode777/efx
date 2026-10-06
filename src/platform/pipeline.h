#ifndef EFX_PIPELINE_H
#define EFX_PIPELINE_H

/* sokol-backed GPU sink for the render module (platform side, ADR 0003).
 * Deliberately sokol-free: including sokol_gfx.h here would compile its
 * implementation section twice in platform.c (the impl guard is consumed
 * by the first include). */

void efx_pipeline_install(void);   /* after sg_setup */
/* Re-install the render sink and re-derive the engine white view after
 * efx_render_reset (in-place game swap, ADR 0057); no-op shader/pipeline
 * rebuild. */
void efx_pipeline_rebind(void);
/* playback owns its passes (F5a): the record list is segmented by target
 * and each segment plays inside a pass on its rendering surface — the
 * swapchain (default target) or the target's attachments */
void efx_pipeline_play(void);
void efx_pipeline_shutdown(void);  /* before sg_shutdown */

/* override for the *default* segment's attachments (Metal golden capture:
 * render the default segment into the capture attachments instead of the
 * swapchain); NULL restores the swapchain. `atts` points at a sokol
 * sg_attachments value, copied during the call. */
void efx_pipeline_set_default_attachments(void *atts);

#endif
