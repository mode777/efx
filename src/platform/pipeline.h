#ifndef EFX_PIPELINE_H
#define EFX_PIPELINE_H

#include "sokol_gfx.h"

/* sokol-backed GPU sink for the render module (platform side, ADR 0003) */

void efx_pipeline_install(void);   /* after sg_setup */
/* playback owns its passes (F5a): the record list is segmented by target
 * and each segment plays inside a pass on its rendering surface — the
 * swapchain (default target) or the target's attachments */
void efx_pipeline_play(void);
void efx_pipeline_shutdown(void);  /* before sg_shutdown */

/* override for the *default* segment's attachments (Metal golden capture:
 * render the default segment into the capture attachments instead of the
 * swapchain); an invalid id restores the swapchain */
void efx_pipeline_set_default_attachments(sg_attachments atts);

#endif
