/*
 * Headless unit tests for the F6a resource provider (directory backend and
 * path safety). Usage: efx_resource_tests <case-name> ; exit 0 = pass.
 */
#include "resource/image.h"
#include "resource/resource.h"

#include <stdio.h>
#include <string.h>

#ifndef EFX_RES_FIXTURES
#define EFX_RES_FIXTURES "tests/fixtures/resource"
#endif

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

static int dir_read(void) {
    int err = -1;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r || err != EFX_RESOURCE_OK) return fail("open fixtures dir");
    if (!efx_resource_exists(r, "hello.txt")) {
        efx_resource_close(r);
        return fail("hello.txt should exist");
    }
    size_t size = 0;
    uint8_t *buf = efx_resource_read(r, "hello.txt", &size, &err);
    if (!buf || err != EFX_RESOURCE_OK) {
        efx_resource_close(r);
        return fail("read hello.txt");
    }
    if (size != 10 || memcmp(buf, "hello efx\n", 10) != 0) {
        efx_resource_free(buf);
        efx_resource_close(r);
        return fail("hello.txt contents/size");
    }
    efx_resource_free(buf);
    efx_resource_close(r);
    return 0;
}

static int dir_nested(void) {
    int err = -1;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    size_t size = 0;
    uint8_t *buf = efx_resource_read(r, "data/nested.txt", &size, &err);
    efx_resource_close(r);
    if (!buf || err != EFX_RESOURCE_OK) return fail("read nested");
    int ok = (size == 7 && memcmp(buf, "nested\n", 7) == 0);
    efx_resource_free(buf);
    return ok ? 0 : fail("nested contents/size");
}

static int dir_missing(void) {
    int err = -1;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    uint8_t *buf = efx_resource_read(r, "nope.txt", NULL, &err);
    efx_resource_close(r);
    if (buf) return fail("missing file returned data");
    return err == EFX_RESOURCE_ERR_NOTFOUND ? 0 : fail("missing err code");
}

static int dir_traversal(void) {
    int err = -1;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    const char *bad[] = {"../hello.txt", "/etc/passwd", "a/../../b",
                         "data/../../hello.txt", "..", ""};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        err = EFX_RESOURCE_OK;
        uint8_t *buf = efx_resource_read(r, bad[i], NULL, &err);
        if (buf) {
            efx_resource_free(buf);
            efx_resource_close(r);
            return fail("escaping path was read");
        }
        if (err != EFX_RESOURCE_ERR_PATH) {
            efx_resource_close(r);
            return fail("escaping path err code");
        }
    }
    efx_resource_close(r);
    return 0;
}

static int dir_bad_root(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES "/no_such_dir", &err);
    if (r) {
        efx_resource_close(r);
        return fail("missing root opened");
    }
    return err == EFX_RESOURCE_ERR_OPEN ? 0 : fail("bad root err code");
}

/* read a fixture and decode it as an image */
static efx_image *load_image(efx_resource *r, const char *path, int *out_err) {
    size_t size = 0;
    int err = EFX_RESOURCE_OK;
    uint8_t *buf = efx_resource_read(r, path, &size, &err);
    if (!buf) {
        *out_err = err;
        return NULL;
    }
    efx_image *img = efx_image_decode(buf, size, out_err);
    efx_resource_free(buf);
    return img;
}

static int image_png(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    efx_image *img = load_image(r, "test_rgba.png", &err);
    efx_resource_close(r);
    if (!img || err != EFX_IMAGE_OK) return fail("decode png");
    static const uint8_t expect[24] = {
        255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0,
        255, 255, 0, 255, 0, 255, 255, 255, 255, 0, 255, 64};
    int ok = (img->width == 3 && img->height == 2 &&
              memcmp(img->pixels, expect, sizeof(expect)) == 0);
    efx_image_free(img);
    return ok ? 0 : fail("png pixels/size");
}

static int image_jpeg(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    efx_image *img = load_image(r, "test_opaque.jpg", &err);
    efx_resource_close(r);
    if (!img || err != EFX_IMAGE_OK) return fail("decode jpeg");
    int ok = (img->width == 4 && img->height == 2);
    if (ok) {
        for (int i = 0; i < img->width * img->height; i++) {
            if (img->pixels[i * 4 + 3] != 255) {
                ok = 0;
                break;
            }
        }
    }
    efx_image_free(img);
    return ok ? 0 : fail("jpeg size/opaque alpha");
}

static int image_corrupt(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    efx_image *img = load_image(r, "corrupt.png", &err);
    efx_resource_close(r);
    if (img) {
        efx_image_free(img);
        return fail("corrupt image decoded");
    }
    return err == EFX_IMAGE_ERR_DECODE ? 0 : fail("corrupt err code");
}

static efx_resource *open_pack(int *err) {
    return efx_resource_open(EFX_RES_FIXTURES "/pack.zip", err);
}

static int zip_read(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = open_pack(&err);
    if (!r) return fail("open pack.zip");
    if (!efx_resource_is_zip(r)) {
        efx_resource_close(r);
        return fail("pack.zip not detected as zip");
    }
    size_t size = 0;
    uint8_t *buf = efx_resource_read(r, "hello.txt", &size, &err);
    if (!buf || err != EFX_RESOURCE_OK) {
        efx_resource_close(r);
        return fail("zip read hello.txt");
    }
    int ok = (size == 10 && memcmp(buf, "hello efx\n", 10) == 0);
    efx_resource_free(buf);
    efx_resource_close(r);
    return ok ? 0 : fail("zip hello contents/size");
}

static int zip_nested(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = open_pack(&err);
    if (!r) return fail("open pack.zip");
    size_t size = 0;
    uint8_t *buf = efx_resource_read(r, "data/nested.txt", &size, &err);
    efx_resource_close(r);
    if (!buf) return fail("zip read nested");
    int ok = (size == 7 && memcmp(buf, "nested\n", 7) == 0);
    efx_resource_free(buf);
    return ok ? 0 : fail("zip nested contents/size");
}

static int zip_image(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = open_pack(&err);
    if (!r) return fail("open pack.zip");
    efx_image *img = load_image(r, "img/test_rgba.png", &err);
    efx_resource_close(r);
    if (!img || err != EFX_IMAGE_OK) return fail("zip decode image");
    int ok = (img->width == 3 && img->height == 2);
    efx_image_free(img);
    return ok ? 0 : fail("zip image dims");
}

static int zip_missing(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = open_pack(&err);
    if (!r) return fail("open pack.zip");
    uint8_t *buf = efx_resource_read(r, "nope.txt", NULL, &err);
    efx_resource_close(r);
    if (buf) return fail("zip missing returned data");
    return err == EFX_RESOURCE_ERR_NOTFOUND ? 0 : fail("zip missing err code");
}

static int zip_bad_root(void) {
    int err = EFX_RESOURCE_OK;
    /* corrupt.png is not a zip archive */
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES "/corrupt.png", &err);
    if (r) {
        efx_resource_close(r);
        return fail("non-zip file opened as root");
    }
    return err == EFX_RESOURCE_ERR_OPEN ? 0 : fail("bad zip err code");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: efx_resource_tests <case>\n");
        return 2;
    }
    const char *c = argv[1];
    if (!strcmp(c, "dir_read")) return dir_read();
    if (!strcmp(c, "dir_nested")) return dir_nested();
    if (!strcmp(c, "dir_missing")) return dir_missing();
    if (!strcmp(c, "dir_traversal")) return dir_traversal();
    if (!strcmp(c, "dir_bad_root")) return dir_bad_root();
    if (!strcmp(c, "image_png")) return image_png();
    if (!strcmp(c, "image_jpeg")) return image_jpeg();
    if (!strcmp(c, "image_corrupt")) return image_corrupt();
    if (!strcmp(c, "zip_read")) return zip_read();
    if (!strcmp(c, "zip_nested")) return zip_nested();
    if (!strcmp(c, "zip_image")) return zip_image();
    if (!strcmp(c, "zip_missing")) return zip_missing();
    if (!strcmp(c, "zip_bad_root")) return zip_bad_root();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
