/*
 * Resource provider implementation (F6a). Directory backend here; the zip
 * backend is added behind the same functions. Pure C: stdio only, which
 * Emscripten maps onto MEMFS.
 */
#include "resource/resource.h"

/* miniz's header defines static inline zlib-compat shims that this TU does
 * not call; suppress the unused-function diagnostics (as with stb). */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "miniz.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#define EFX_RES_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#else
#define EFX_RES_ISDIR(m) S_ISDIR(m)
#endif

struct efx_resource {
    char *root;   /* normalized, no trailing separators */
    int is_zip;
    int zip_open;
    mz_zip_archive zip;
};

/* Reject absolute paths and any parent-directory component. Only forward
 * slashes are meaningful in resource paths (documented API contract). */
static int path_is_safe(const char *path) {
    if (!path || path[0] == '\0') {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 0;
    }
    const char *p = path;
    while (*p) {
        /* component start */
        const char *start = p;
        while (*p && *p != '/' && *p != '\\') {
            p++;
        }
        size_t n = (size_t)(p - start);
        if (n == 2 && start[0] == '.' && start[1] == '.') {
            return 0;
        }
        if (n == 0) {
            /* empty component (leading/duplicate separator) */
            return 0;
        }
        if (*p == '/' || *p == '\\') {
            p++;
        }
    }
    return 1;
}

static char *join_path(const char *root, const char *path) {
    size_t rl = strlen(root);
    size_t pl = strlen(path);
    char *out = malloc(rl + 1 + pl + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, root, rl);
    out[rl] = '/';
    memcpy(out + rl + 1, path, pl);
    out[rl + 1 + pl] = '\0';
    return out;
}

efx_resource *efx_resource_open(const char *root, int *err) {
    if (err) {
        *err = EFX_RESOURCE_ERR_OPEN;
    }
    if (!root || root[0] == '\0') {
        return NULL;
    }
    struct stat st;
    if (stat(root, &st) != 0) {
        return NULL;
    }
    size_t len = strlen(root);
    while (len > 1 && (root[len - 1] == '/' || root[len - 1] == '\\')) {
        len--;
    }
    efx_resource *r = calloc(1, sizeof(*r));
    if (!r) {
        if (err) {
            *err = EFX_RESOURCE_ERR_NOMEM;
        }
        return NULL;
    }
    r->root = malloc(len + 1);
    if (!r->root) {
        free(r);
        if (err) {
            *err = EFX_RESOURCE_ERR_NOMEM;
        }
        return NULL;
    }
    memcpy(r->root, root, len);
    r->root[len] = '\0';
    if (EFX_RES_ISDIR(st.st_mode)) {
        r->is_zip = 0;
    } else {
        /* a regular file root must be a zip archive */
        if (!mz_zip_reader_init_file(&r->zip, r->root, 0)) {
            free(r->root);
            free(r);
            return NULL;
        }
        r->is_zip = 1;
        r->zip_open = 1;
    }
    if (err) {
        *err = EFX_RESOURCE_OK;
    }
    return r;
}

void efx_resource_close(efx_resource *r) {
    if (!r) {
        return;
    }
    if (r->is_zip && r->zip_open) {
        mz_zip_reader_end(&r->zip);
    }
    free(r->root);
    free(r);
}

const char *efx_resource_root(const efx_resource *r) {
    return r ? r->root : NULL;
}

int efx_resource_is_zip(const efx_resource *r) {
    return r ? r->is_zip : 0;
}

static int dir_read(efx_resource *r, const char *path, uint8_t **out,
                    size_t *out_size, int *err) {
    char *full = join_path(r->root, path);
    if (!full) {
        if (err) {
            *err = EFX_RESOURCE_ERR_NOMEM;
        }
        return EFX_RESOURCE_ERR_NOMEM;
    }
    FILE *f = fopen(full, "rb");
    free(full);
    if (!f) {
        if (err) {
            *err = EFX_RESOURCE_ERR_NOTFOUND;
        }
        return EFX_RESOURCE_ERR_NOTFOUND;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        if (err) {
            *err = EFX_RESOURCE_ERR_IO;
        }
        return EFX_RESOURCE_ERR_IO;
    }
    long n = ftell(f);
    if (n < 0) {
        fclose(f);
        if (err) {
            *err = EFX_RESOURCE_ERR_IO;
        }
        return EFX_RESOURCE_ERR_IO;
    }
    rewind(f);
    uint8_t *buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        if (err) {
            *err = EFX_RESOURCE_ERR_NOMEM;
        }
        return EFX_RESOURCE_ERR_NOMEM;
    }
    size_t got = n > 0 ? fread(buf, 1, (size_t)n, f) : 0;
    int failed = ferror(f) || got != (size_t)n;
    fclose(f);
    if (failed) {
        free(buf);
        if (err) {
            *err = EFX_RESOURCE_ERR_IO;
        }
        return EFX_RESOURCE_ERR_IO;
    }
    buf[n] = '\0';
    *out = buf;
    if (out_size) {
        *out_size = (size_t)n;
    }
    if (err) {
        *err = EFX_RESOURCE_OK;
    }
    return EFX_RESOURCE_OK;
}

/* Zip entries are addressed by their exact archive name (forward slashes),
 * matching the resource path contract. */
static int zip_read(efx_resource *r, const char *path, uint8_t **out,
                    size_t *out_size, int *err) {
    if (!r->zip_open) {
        if (err) {
            *err = EFX_RESOURCE_ERR_OPEN;
        }
        return EFX_RESOURCE_ERR_OPEN;
    }
    int idx = mz_zip_reader_locate_file(&r->zip, path, NULL, 0);
    if (idx < 0) {
        if (err) {
            *err = EFX_RESOURCE_ERR_NOTFOUND;
        }
        return EFX_RESOURCE_ERR_NOTFOUND;
    }
    mz_zip_archive_file_stat st;
    if (!mz_zip_reader_file_stat(&r->zip, (mz_uint)idx, &st)) {
        if (err) {
            *err = EFX_RESOURCE_ERR_IO;
        }
        return EFX_RESOURCE_ERR_IO;
    }
    size_t n = (size_t)st.m_uncomp_size;
    uint8_t *buf = malloc(n + 1);
    if (!buf) {
        if (err) {
            *err = EFX_RESOURCE_ERR_NOMEM;
        }
        return EFX_RESOURCE_ERR_NOMEM;
    }
    if (n > 0 && !mz_zip_reader_extract_to_mem(&r->zip, (mz_uint)idx, buf,
                                               n, 0)) {
        free(buf);
        if (err) {
            *err = EFX_RESOURCE_ERR_IO;
        }
        return EFX_RESOURCE_ERR_IO;
    }
    buf[n] = '\0';
    *out = buf;
    if (out_size) {
        *out_size = n;
    }
    if (err) {
        *err = EFX_RESOURCE_OK;
    }
    return EFX_RESOURCE_OK;
}

uint8_t *efx_resource_read(efx_resource *r, const char *path, size_t *out_size,
                           int *err) {
    if (err) {
        *err = EFX_RESOURCE_ERR_PATH;
    }
    if (!r) {
        if (err) {
            *err = EFX_RESOURCE_ERR_OPEN;
        }
        return NULL;
    }
    if (!path_is_safe(path)) {
        return NULL;
    }
    uint8_t *out = NULL;
    size_t size = 0;
    int rc;
    if (r->is_zip) {
        rc = zip_read(r, path, &out, &size, err);
    } else {
        rc = dir_read(r, path, &out, &size, err);
    }
    if (rc != EFX_RESOURCE_OK) {
        return NULL;
    }
    if (out_size) {
        *out_size = size;
    }
    return out;
}

char *efx_resource_read_text(efx_resource *r, const char *path, int *err) {
    size_t size = 0;
    int e = EFX_RESOURCE_OK;
    uint8_t *buf = efx_resource_read(r, path, &size, &e);
    if (!buf) {
        if (err) {
            *err = e;
        }
        return NULL;
    }
    if (err) {
        *err = e;
    }
    return (char *)buf;
}

int efx_resource_exists(efx_resource *r, const char *path) {
    size_t size = 0;
    return efx_resource_size(r, path, &size) == EFX_RESOURCE_OK;
}

int efx_resource_size(efx_resource *r, const char *path, size_t *out_size) {
    if (!r || !path_is_safe(path)) {
        return EFX_RESOURCE_ERR_PATH;
    }
    uint8_t *buf = efx_resource_read(r, path, out_size, NULL);
    if (!buf) {
        return EFX_RESOURCE_ERR_NOTFOUND;
    }
    efx_resource_free(buf);
    return EFX_RESOURCE_OK;
}

void efx_resource_free(void *bytes) {
    free(bytes);
}
