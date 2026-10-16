#ifndef EFX_RENDER_HEIGHTFIELD_H
#define EFX_RENDER_HEIGHTFIELD_H

/*
 * Image heightfield builder (image-heightfield): turns the luminance of a
 * grayscale image (or a pixel sub-rect of it, decimated by a stride) into a
 * single-surface CPU MeshData. Pure C, no sokol/quickjs — the same headless
 * testability as the noise core (ADR 0059). The script-facing option bag is
 * validated once in the shared prelude (ADR 0049); this builder pins the
 * geometry contract (3d-core "Image heightfield primitive"):
 *
 * - height = Rec. 709 luminance * opts.height (0..1 channels, alpha ignored)
 * - grid samples every step-th pixel from the clamped region origin,
 *   cols = (region_w - 1) / step + 1, rows analogously
 * - XZ plane facing +Y, centered on the origin, CCW seen from +Y
 * - x extent = opts.size (<= 0: 1 world unit per pixel), z follows aspect
 * - uv maps the full image (px/(w-1), py/(h-1)) so a region keeps the
 *   un-cropped world-texture alignment
 * - normals from central differences (one-sided at the edges), normalized;
 *   a locally flat area yields exactly (0, 1, 0)
 *
 * This TU must never be compiled with FP contraction enabled (see
 * CMakeLists.txt): byte-identical vertex streams across targets are a pinned
 * contract, proven by the unit receipts and the golden scene.
 */

#include "render.h"
#include "resource/image.h"

#define EFX_HF_OK 0
#define EFX_HF_ERR_RANGE 1 /* degenerate region after clamping (< 2 samples) */
#define EFX_HF_ERR_NOMEM 2

typedef struct efx_heightfield_opts {
    int region_x, region_y, region_w, region_h; /* pixels; clamped to image */
    int step;    /* >= 1 decimation stride */
    float height; /* luminance scale (may be negative to invert) */
    float size;   /* world x extent; <= 0 means the clamped region width */
} efx_heightfield_opts;

/* Builds the heightfield MeshData. Returns NULL and sets *err (when
 * non-NULL) on a degenerate region or allocation failure. */
efx_meshdata *efx_heightfield_build(const efx_image *img,
                                    const efx_heightfield_opts *opts,
                                    int *err);

#endif
