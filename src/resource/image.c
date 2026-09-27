/*
 * Image decoding implementation (F6a). stb_image is compiled once in
 * stb_image_impl.c; this TU only calls its memory-decode entry point.
 */
#include "resource/image.h"

#include "resource/stb_image_config.h"

#include <limits.h>
#include <stdlib.h>

#include "stb_image.h"

efx_image *efx_image_decode(const uint8_t *bytes, size_t size, int *err) {
    if (err) {
        *err = EFX_IMAGE_ERR_DECODE;
    }
    if (!bytes || size == 0 || size > (size_t)INT_MAX) {
        return NULL;
    }
    int w = 0, h = 0, comp = 0;
    stbi_uc *px = stbi_load_from_memory((const stbi_uc *)bytes, (int)size, &w,
                                        &h, &comp, 4);
    if (!px || w <= 0 || h <= 0) {
        if (px) {
            stbi_image_free(px);
        }
        return NULL;
    }
    efx_image *img = malloc(sizeof(*img));
    if (!img) {
        stbi_image_free(px);
        if (err) {
            *err = EFX_IMAGE_ERR_NOMEM;
        }
        return NULL;
    }
    img->width = w;
    img->height = h;
    img->pixels = px;
    if (err) {
        *err = EFX_IMAGE_OK;
    }
    return img;
}

void efx_image_free(efx_image *img) {
    if (!img) {
        return;
    }
    stbi_image_free(img->pixels);
    free(img);
}
