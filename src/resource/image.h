#ifndef EFX_IMAGE_H
#define EFX_IMAGE_H

/*
 * Image decoding for the resource loader (F6a). Decodes PNG/JPEG bytes to
 * tightly packed RGBA8 (width*height*4), the ImageData layout used across
 * the engine. Pure C; no quickjs/sokol.
 */

#include <stddef.h>
#include <stdint.h>

#define EFX_IMAGE_OK 0
#define EFX_IMAGE_ERR_DECODE 1
#define EFX_IMAGE_ERR_NOMEM 2

typedef struct efx_image {
    int width;
    int height;
    uint8_t *pixels; /* RGBA8, width*height*4 bytes, engine-owned */
} efx_image;

/* Decodes an in-memory PNG or JPEG. Returns NULL and sets *err (when
 * non-NULL) on a decode or allocation failure. JPEG (no alpha) decodes with
 * fully opaque alpha. */
efx_image *efx_image_decode(const uint8_t *bytes, size_t size, int *err);

/* Releases an image and its pixels. NULL safe. */
void efx_image_free(efx_image *img);

#endif
