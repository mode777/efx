#include "render/render_heightfield.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Rec. 709 luminance on 0..1 channels, alpha ignored; left-associated float
 * ops with FP contraction disabled keep this bit-identical on every target
 * (MSVC's default /fp:precise never contracts). */
static float hf_luma(const uint8_t *px) {
    float r = px[0] / 255.0f;
    float g = px[1] / 255.0f;
    float b = px[2] / 255.0f;
    return (0.2126f * r + 0.7152f * g + 0.0722f * b);
}

efx_meshdata *efx_heightfield_build(const efx_image *img,
                                    const efx_heightfield_opts *opts,
                                    int *err) {
    if (err) *err = EFX_HF_OK;
    if (!img || !img->pixels || img->width < 1 || img->height < 1 || !opts ||
        opts->step < 1) {
        if (err) *err = EFX_HF_ERR_RANGE;
        return NULL;
    }

    /* clamp the region to the image (64-bit sums: the components are
     * arbitrary validated ints) */
    long long rx = opts->region_x < 0 ? 0 : opts->region_x;
    long long ry = opts->region_y < 0 ? 0 : opts->region_y;
    long long x2 = (long long)opts->region_x + (long long)opts->region_w;
    long long y2 = (long long)opts->region_y + (long long)opts->region_h;
    if (x2 > img->width) x2 = img->width;
    if (y2 > img->height) y2 = img->height;
    long long rw = x2 - rx;
    long long rh = y2 - ry;
    if (rw < 1 || rh < 1) {
        if (err) *err = EFX_HF_ERR_RANGE;
        return NULL;
    }
    int cols = (int)((rw - 1) / opts->step) + 1;
    int rows = (int)((rh - 1) / opts->step) + 1;
    if (cols < 2 || rows < 2) {
        if (err) *err = EFX_HF_ERR_RANGE;
        return NULL;
    }
    if ((long long)cols * (long long)rows > (long long)0x7fffffff / 12) {
        if (err) *err = EFX_HF_ERR_NOMEM;
        return NULL;
    }

    /* world extents: x from opts.size (<= 0: 1 unit per pixel), z follows
     * the clamped region aspect so texels stay square */
    float xspan = opts->size > 0.0f ? opts->size : (float)rw;
    float zspan = xspan * (float)rh / (float)rw;
    float dx = xspan / (float)(cols - 1);
    float dz = zspan / (float)(rows - 1);
    int iw1 = img->width - 1;
    int ih1 = img->height - 1;

    int vcount = cols * rows;
    size_t vbytes = (size_t)vcount * sizeof(float);
    float *heights = malloc(vbytes);
    size_t pbytes = (size_t)vcount * 3 * sizeof(float);
    float *positions = malloc(pbytes);
    float *normals = malloc(pbytes);
    float *uvs = malloc((size_t)vcount * 2 * sizeof(float));
    size_t icount = (size_t)(cols - 1) * (size_t)(rows - 1) * 6u;
    uint32_t *indices = malloc(icount * sizeof(uint32_t));
    if (!heights || !positions || !normals || !uvs || !indices) {
        free(heights);
        free(positions);
        free(normals);
        free(uvs);
        free(indices);
        if (err) *err = EFX_HF_ERR_NOMEM;
        return NULL;
    }

    /* sampled heights first: normals need the whole grid */
    for (int j = 0; j < rows; j++) {
        long long py = ry + (long long)j * opts->step;
        const uint8_t *row = img->pixels + ((size_t)py * (size_t)img->width) * 4u;
        for (int i = 0; i < cols; i++) {
            long long px = rx + (long long)i * opts->step;
            heights[j * cols + i] = hf_luma(row + (size_t)px * 4u) *
                                    opts->height;
        }
    }

    for (int j = 0; j < rows; j++) {
        long long py = ry + (long long)j * opts->step;
        for (int i = 0; i < cols; i++) {
            int v = j * cols + i;
            long long px = rx + (long long)i * opts->step;
            positions[v * 3] = -0.5f * xspan + (float)i * dx;
            positions[v * 3 + 1] = heights[v];
            positions[v * 3 + 2] = -0.5f * zspan + (float)j * dz;
            uvs[v * 2] = (float)px / (float)iw1;
            uvs[v * 2 + 1] = (float)py / (float)ih1;

            /* surface normal from central differences (one-sided at the
             * edges), scaled by the world grid spacing */
            float sx;
            if (i > 0 && i < cols - 1) {
                sx = (heights[v + 1] - heights[v - 1]) / (2.0f * dx);
            } else if (i == 0) {
                sx = (heights[v + 1] - heights[v]) / dx;
            } else {
                sx = (heights[v] - heights[v - 1]) / dx;
            }
            float sz;
            if (j > 0 && j < rows - 1) {
                sz = (heights[v + cols] - heights[v - cols]) / (2.0f * dz);
            } else if (j == 0) {
                sz = (heights[v + cols] - heights[v]) / dz;
            } else {
                sz = (heights[v] - heights[v - cols]) / dz;
            }
            float nx = 0.0f - sx, ny = 1.0f, nz = 0.0f - sz;
            float len = sqrtf(nx * nx + ny * ny + nz * nz);
            normals[v * 3] = nx / len;
            normals[v * 3 + 1] = ny / len;
            normals[v * 3 + 2] = nz / len;
        }
    }

    /* quad indices, CCW seen from +Y (the makePlane emission pattern) */
    size_t o = 0;
    for (int j = 0; j < rows - 1; j++) {
        for (int i = 0; i < cols - 1; i++) {
            uint32_t a = (uint32_t)(j * cols + i);
            uint32_t b = a + 1;
            uint32_t d = a + (uint32_t)cols;
            uint32_t e = d + 1;
            indices[o++] = a;
            indices[o++] = d;
            indices[o++] = e;
            indices[o++] = a;
            indices[o++] = e;
            indices[o++] = b;
        }
    }

    efx_surface_src src;
    memset(&src, 0, sizeof(src));
    src.positions = positions;
    src.positions_len = vcount * 3;
    src.normals = normals;
    src.normals_len = vcount * 3;
    src.uvs = uvs;
    src.uvs_len = vcount * 2;
    src.indices = indices;
    src.indices_len = (int)icount;
    int merr = 0;
    efx_meshdata *md = efx_meshdata_create(&src, 1, &merr);
    free(heights);
    free(positions);
    free(normals);
    free(uvs);
    free(indices);
    if (!md) {
        if (err) *err = merr == EFX_MESHERR_NOMEM ? EFX_HF_ERR_NOMEM
                                                  : EFX_HF_ERR_RANGE;
        return NULL;
    }
    return md;
}
