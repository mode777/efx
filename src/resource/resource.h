#ifndef EFX_RESOURCE_H
#define EFX_RESOURCE_H

/*
 * Resource provider (F6a): reads entries from a resource root that is either
 * a directory or a zip archive, by path relative to the root.
 *
 * Pure C (ADR 0003 module walls): no quickjs, no sokol. stdio maps onto the
 * Emscripten MEMFS, so the same implementation serves desktop and web. The
 * zip backend is miniz (vendor/miniz).
 */

#include <stddef.h>
#include <stdint.h>

/* error codes */
#define EFX_RESOURCE_OK 0
#define EFX_RESOURCE_ERR_OPEN 1      /* root missing / unreadable / bad archive */
#define EFX_RESOURCE_ERR_NOTFOUND 2  /* entry does not exist in the root */
#define EFX_RESOURCE_ERR_PATH 3      /* invalid or root-escaping path */
#define EFX_RESOURCE_ERR_IO 4        /* read/decode failure */
#define EFX_RESOURCE_ERR_NOMEM 5

typedef struct efx_resource efx_resource;

/* Opens a root: a directory, or a .zip archive file. Returns NULL on failure
 * and, when err is non-NULL, sets *err to an EFX_RESOURCE_ERR_*. */
efx_resource *efx_resource_open(const char *root, int *err);

/* Releases the provider. NULL safe. */
void efx_resource_close(efx_resource *r);

/* 1 when path is a readable entry under the root, else 0. */
int efx_resource_exists(efx_resource *r, const char *path);

/* Reads an entry into a freshly malloc'd buffer (caller frees with
 * efx_resource_free). On success returns the buffer and sets *out_size (the
 * buffer is NUL-terminated one byte past *out_size for text convenience);
 * on failure returns NULL and sets *err when non-NULL. */
uint8_t *efx_resource_read(efx_resource *r, const char *path, size_t *out_size,
                           int *err);

/* Reads an entry as NUL-terminated UTF-8 text. Returns a malloc'd string
 * (free with efx_resource_free) or NULL with *err set. */
char *efx_resource_read_text(efx_resource *r, const char *path, int *err);

/* Frees a buffer returned by efx_resource_read / read_text. NULL safe. */
void efx_resource_free(void *bytes);

/* 1 when the root is a zip archive, 0 for a directory. */
int efx_resource_is_zip(const efx_resource *r);

#endif
