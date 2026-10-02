#include "bridge_internal.h"


/* ------------------------------------------------ F6a resource loading */

void web_open_root(void) {
    if (W.resource) {
        efx_resource_close(W.resource);
        W.resource = NULL;
    }
    if (W.root[0]) {
        int e = EFX_RESOURCE_OK;
        W.resource = efx_resource_open(W.root, &e);
    }
}

/* Point the provider at a new root (directory or mounted zip). Returns 1 on
 * success, 0 when the root cannot be opened. Used by the boot glue after it
 * has written a fetched asset archive into the filesystem. */
EMSCRIPTEN_KEEPALIVE int efx_bridge_set_root(const char *path) {
    snprintf(W.root, sizeof(W.root), "%s", path ? path : "");
    web_open_root();
    return W.resource ? 1 : 0;
}

/* Returns a malloc'd NUL-terminated string the JS side frees with _free, or
 * NULL on failure. */
EMSCRIPTEN_KEEPALIVE const char *efx_bridge_load_text(const char *path) {
    if (!W.resource) {
        return NULL;
    }
    int e = EFX_RESOURCE_OK;
    return efx_resource_read_text(W.resource, path, &e);
}

/* Decodes an image into an ImageData slot; returns the 1-based id or 0. */
EMSCRIPTEN_KEEPALIVE int efx_bridge_load_image(const char *path) {
    if (!W.resource) {
        return 0;
    }
    size_t n = 0;
    int e = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(W.resource, path, &n, &e);
    /* negative codes are loader failures the prelude maps to messages
     * (ADR 0049 D4): -err for resource errors, -100 decode, -5 out of
     * memory */
    if (!bytes) {
        return -e;
    }
    int ie = EFX_IMAGE_OK;
    efx_image *img = efx_image_decode(bytes, n, &ie);
    efx_resource_free(bytes);
    if (!img) {
        return ie == EFX_IMAGE_ERR_NOMEM ? -5 : -100;
    }
    size_t sz = (size_t)img->width * (size_t)img->height * 4u;
    uint8_t *px = malloc(sz ? sz : 1);
    if (!px) {
        efx_image_free(img);
        return 0;
    }
    memcpy(px, img->pixels, sz);
    int w = img->width;
    int h = img->height;
    efx_image_free(img);
    return efx_bridge_imagedata_commit(w, h, px);
}

