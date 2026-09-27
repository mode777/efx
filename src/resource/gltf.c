/*
 * glTF 2.0 static import (F6b). cgltf parses the container; this module
 * resolves external references through the F6a provider, maps one selected
 * mesh's primitives to engine MeshData surfaces, converts each material to
 * the fixed-function Phong material, decodes images, and creates textures
 * with their glTF samplers. Pure C (no quickjs/sokol).
 */
#include "resource/gltf.h"
#include "resource/image.h"

#include <stdlib.h>
#include <string.h>

#include "cgltf.h"

/* --------------------------------------------------- file path helpers */

static char *dup_cstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) {
        memcpy(p, s, n);
    }
    return p;
}

/* collapse duplicate separators and drop "." components; keeps ".." so the
 * provider's traversal guard still sees (and rejects) escaping references */
static void normalize_path(char *p) {
    if (!p) {
        return;
    }
    for (char *q = p; *q; q++) {
        if (*q == '\\') {
            *q = '/';
        }
    }
    char *out = p;
    char *rd = p;
    while (*rd) {
        while (*rd == '/') {
            rd++;
        }
        char *start = rd;
        while (*rd && *rd != '/') {
            rd++;
        }
        size_t n = (size_t)(rd - start);
        if (n == 0) {
            break;
        }
        if (n == 1 && start[0] == '.') {
            continue;
        }
        if (out != p) {
            *out++ = '/';
        }
        memmove(out, start, n);
        out += n;
    }
    *out = '\0';
}

static int hex_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* in-place percent-decoding of a URI path segment */
static void decode_uri(char *uri) {
    char *out = uri;
    for (const char *p = uri; *p;) {
        int hi = p[1] ? hex_val((unsigned char)p[1]) : -1;
        int lo = p[2] ? hex_val((unsigned char)p[2]) : -1;
        if (p[0] == '%' && hi >= 0 && lo >= 0) {
            *out++ = (char)((hi << 4) | lo);
            p += 3;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
}

static cgltf_result gltf_file_read(const cgltf_memory_options *mo,
                                   const cgltf_file_options *fo,
                                   const char *path, cgltf_size *size,
                                   void **data) {
    (void)mo;
    efx_resource *res = (efx_resource *)fo->user_data;
    if (!res || !path) {
        return cgltf_result_file_not_found;
    }
    char *p = dup_cstr(path);
    if (!p) {
        return cgltf_result_out_of_memory;
    }
    normalize_path(p);
    size_t n = 0;
    int err = EFX_RESOURCE_OK;
    uint8_t *buf = efx_resource_read(res, p, &n, &err);
    free(p);
    if (!buf) {
        if (err == EFX_RESOURCE_ERR_NOMEM) {
            return cgltf_result_out_of_memory;
        }
        if (err == EFX_RESOURCE_ERR_IO) {
            return cgltf_result_io_error;
        }
        return cgltf_result_file_not_found;
    }
    *data = buf;
    *size = (cgltf_size)n;
    return cgltf_result_success;
}

static void gltf_file_release(const cgltf_memory_options *mo,
                              const cgltf_file_options *fo, void *data) {
    (void)mo;
    (void)fo;
    efx_resource_free(data);
}

/* ------------------------------------------------------ base64 / images */

static int b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static uint8_t *base64_decode(const char *in, size_t in_len, size_t *out_len) {
    size_t cap = (in_len / 4 + 1) * 3;
    uint8_t *out = malloc(cap ? cap : 1);
    if (!out) {
        return NULL;
    }
    size_t n = 0;
    unsigned acc = 0;
    int bits = 0;
    for (size_t i = 0; i < in_len; i++) {
        int c = (unsigned char)in[i];
        if (c == '=') {
            break;
        }
        int v = b64_val(c);
        if (v < 0) {
            continue; /* whitespace and line breaks are ignored */
        }
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[n++] = (uint8_t)((acc >> bits) & 0xFFu);
        }
    }
    *out_len = n;
    return out;
}

/* ---------------------------------------------------------------- context */

typedef struct {
    const cgltf_data *data;
    efx_resource *res;
    const char *gltf_path;
    /* decoded images (one per glTF image index, decoded once) */
    efx_image **images;
    char *image_state; /* 0 unknown, 1 ok, 2 failed */
    int image_count;
    /* created textures, deduped per (image, wrap, filter) */
    struct tex_entry {
        int image;
        int wrap;
        int filter;
        uint64_t handle;
    } *tex;
    int tex_count;
    int tex_cap;
    int err;
} gltf_ctx;

static void ctx_free(gltf_ctx *c) {
    if (c->images) {
        for (int i = 0; i < c->image_count; i++) {
            efx_image_free(c->images[i]);
        }
        free(c->images);
    }
    free(c->image_state);
    free(c->tex);
    c->images = NULL;
    c->image_state = NULL;
    c->tex = NULL;
    c->tex_count = 0;
    c->tex_cap = 0;
}

/* destroy every texture this import created (failure cleanup; the render
 * layer defers/retains as needed) */
static void ctx_destroy_textures(gltf_ctx *c) {
    for (int i = 0; i < c->tex_count; i++) {
        if (c->tex[i].handle) {
            efx_render_texture_destroy(c->tex[i].handle);
            c->tex[i].handle = 0;
        }
    }
}

/* decode one glTF image (external file, data-URI, or buffer view) */
static efx_image *ctx_image(gltf_ctx *c, int idx) {
    if (idx < 0 || idx >= c->image_count) {
        return NULL;
    }
    if (c->image_state[idx] == 1) {
        return c->images[idx];
    }
    if (c->image_state[idx] == 2) {
        return NULL;
    }
    c->image_state[idx] = 2;
    const cgltf_image *img = &c->data->images[idx];
    efx_image *result = NULL;

    if (img->buffer_view) {
        const uint8_t *bytes = cgltf_buffer_view_data(img->buffer_view);
        if (bytes) {
            int e = EFX_IMAGE_OK;
            result = efx_image_decode(bytes, (size_t)img->buffer_view->size, &e);
            if (!result && e == EFX_IMAGE_ERR_NOMEM) {
                c->err = EFX_GLTF_ERR_NOMEM;
                return NULL;
            }
        }
    } else if (img->uri && strncmp(img->uri, "data:", 5) == 0) {
        const char *comma = strchr(img->uri, ',');
        if (comma) {
            const char *meta = img->uri + 5;
            int is_base64 = 0;
            for (const char *m = meta; m < comma; m++) {
                if (m[0] == ';' && m + 7 <= comma &&
                    strncmp(m, ";base64", 7) == 0) {
                    is_base64 = 1;
                }
            }
            if (is_base64) {
                size_t n = 0;
                uint8_t *bytes = base64_decode(comma + 1, strlen(comma + 1), &n);
                if (!bytes) {
                    c->err = EFX_GLTF_ERR_NOMEM;
                    return NULL;
                }
                int e = EFX_IMAGE_OK;
                result = efx_image_decode(bytes, n, &e);
                free(bytes);
                if (!result && e == EFX_IMAGE_ERR_NOMEM) {
                    c->err = EFX_GLTF_ERR_NOMEM;
                    return NULL;
                }
            }
        }
    } else if (img->uri) {
        /* external file resolved relative to the glTF path */
        const char *base = c->gltf_path;
        const char *slash = NULL;
        for (const char *p = base; *p; p++) {
            if (*p == '/') {
                slash = p;
            }
        }
        size_t dirlen = slash ? (size_t)(slash - base) + 1 : 0;
        size_t urilen = strlen(img->uri);
        char *path = malloc(dirlen + urilen + 1);
        if (!path) {
            c->err = EFX_GLTF_ERR_NOMEM;
            return NULL;
        }
        if (dirlen) {
            memcpy(path, base, dirlen);
        }
        memcpy(path + dirlen, img->uri, urilen + 1);
        decode_uri(path + dirlen);
        normalize_path(path);
        size_t n = 0;
        int e = EFX_RESOURCE_OK;
        uint8_t *bytes = efx_resource_read(c->res, path, &n, &e);
        free(path);
        if (bytes) {
            int ie = EFX_IMAGE_OK;
            result = efx_image_decode(bytes, n, &ie);
            efx_resource_free(bytes);
            if (!result && ie == EFX_IMAGE_ERR_NOMEM) {
                c->err = EFX_GLTF_ERR_NOMEM;
                return NULL;
            }
        }
    }

    if (!result) {
        c->err = EFX_GLTF_ERR_IMAGE;
        return NULL;
    }
    c->images[idx] = result;
    c->image_state[idx] = 1;
    return result;
}

/* glTF sampler -> engine wrap/filter (absent/unsupported values default) */
static int filter_is_nearest(cgltf_filter_type f) {
    return f == cgltf_filter_type_nearest ||
           f == cgltf_filter_type_nearest_mipmap_nearest ||
           f == cgltf_filter_type_nearest_mipmap_linear;
}

static int filter_is_known(cgltf_filter_type f) {
    return filter_is_nearest(f) || f == cgltf_filter_type_linear ||
           f == cgltf_filter_type_linear_mipmap_nearest ||
           f == cgltf_filter_type_linear_mipmap_linear;
}

static int sampler_wrap(const cgltf_sampler *s) {
    if (!s) {
        return EFX_TEX_WRAP_REPEAT;
    }
    if (s->wrap_s == cgltf_wrap_mode_clamp_to_edge) {
        return EFX_TEX_WRAP_CLAMP;
    }
    if (s->wrap_s == cgltf_wrap_mode_mirrored_repeat) {
        return EFX_TEX_WRAP_MIRROR;
    }
    return EFX_TEX_WRAP_REPEAT;
}

static int sampler_filter(const cgltf_sampler *s) {
    if (!s) {
        return EFX_FILTER_LINEAR;
    }
    if (filter_is_known(s->mag_filter)) {
        return filter_is_nearest(s->mag_filter) ? EFX_FILTER_NEAREST
                                                : EFX_FILTER_LINEAR;
    }
    if (filter_is_known(s->min_filter)) {
        return filter_is_nearest(s->min_filter) ? EFX_FILTER_NEAREST
                                                : EFX_FILTER_LINEAR;
    }
    return EFX_FILTER_LINEAR;
}

/* get-or-create the texture for a glTF texture view's texture */
static uint64_t ctx_texture(gltf_ctx *c, const cgltf_texture *t) {
    if (!t || !t->image) {
        return 0;
    }
    int image_idx = (int)cgltf_image_index(c->data, t->image);
    int wrap = sampler_wrap(t->sampler);
    int filter = sampler_filter(t->sampler);
    for (int i = 0; i < c->tex_count; i++) {
        if (c->tex[i].image == image_idx && c->tex[i].wrap == wrap &&
            c->tex[i].filter == filter) {
            return c->tex[i].handle;
        }
    }
    efx_image *img = ctx_image(c, image_idx);
    if (!img) {
        return 0;
    }
    uint64_t h = efx_render_texture_create(img->width, img->height,
                                           img->pixels, wrap, filter);
    if (!h) {
        c->err = EFX_GLTF_ERR_NOMEM;
        return 0;
    }
    if (c->tex_count >= c->tex_cap) {
        int cap = c->tex_cap ? c->tex_cap * 2 : 8;
        void *grown = realloc(c->tex, (size_t)cap * sizeof(*c->tex));
        if (!grown) {
            efx_render_texture_destroy(h);
            c->err = EFX_GLTF_ERR_NOMEM;
            return 0;
        }
        c->tex = grown;
        c->tex_cap = cap;
    }
    c->tex[c->tex_count].image = image_idx;
    c->tex[c->tex_count].wrap = wrap;
    c->tex[c->tex_count].filter = filter;
    c->tex[c->tex_count].handle = h;
    c->tex_count++;
    return h;
}

/* PBR -> Phong conversion (design D4) */
static int material_from_gltf(gltf_ctx *c, const cgltf_material *gm,
                              efx_material *out) {
    efx_material_default(out);
    float base[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 1.0f;
    float roughness = 1.0f;
    const cgltf_texture_view *base_tex = NULL;
    if (gm->has_pbr_metallic_roughness) {
        for (int i = 0; i < 4; i++) {
            base[i] = gm->pbr_metallic_roughness.base_color_factor[i];
        }
        metallic = gm->pbr_metallic_roughness.metallic_factor;
        roughness = gm->pbr_metallic_roughness.roughness_factor;
        base_tex = &gm->pbr_metallic_roughness.base_color_texture;
    }
    for (int i = 0; i < 4; i++) {
        out->diffuse[i] = base[i];
    }
    out->emissive[0] = gm->emissive_factor[0];
    out->emissive[1] = gm->emissive_factor[1];
    out->emissive[2] = gm->emissive_factor[2];
    out->emissive[3] = 1.0f;
    /* metallic selects the specular color: mix(black, baseColor, metallic) */
    for (int i = 0; i < 3; i++) {
        out->specular[i] = base[i] * metallic;
    }
    out->specular[3] = 1.0f;
    float rough = 1.0f - roughness;
    float shin = rough * rough * 128.0f;
    if (!(shin >= 1.0f)) {
        shin = 1.0f;
    }
    if (shin > 128.0f) {
        shin = 128.0f;
    }
    out->shininess = shin;

    if (base_tex && base_tex->texture) {
        uint64_t h = ctx_texture(c, base_tex->texture);
        if (!h) {
            return c->err ? c->err : EFX_GLTF_ERR_IMAGE;
        }
        out->diffuse_map = h;
        if (gm->alpha_mode == cgltf_alpha_mode_mask) {
            out->alpha_mask = h;
        }
    }
    if (gm->emissive_texture.texture) {
        uint64_t h = ctx_texture(c, gm->emissive_texture.texture);
        if (!h) {
            return c->err ? c->err : EFX_GLTF_ERR_IMAGE;
        }
        out->emissive_map = h;
    }
    return EFX_GLTF_OK;
}

/* --------------------------------------------------------- geometry */

static void surface_src_free(efx_surface_src *s) {
    free((void *)s->positions);
    free((void *)s->normals);
    free((void *)s->uvs);
    free((void *)s->colors);
    free((void *)s->indices);
    s->positions = NULL;
    s->normals = NULL;
    s->uvs = NULL;
    s->colors = NULL;
    s->indices = NULL;
    s->positions_len = 0;
    s->normals_len = 0;
    s->uvs_len = 0;
    s->colors_len = 0;
    s->indices_len = 0;
}

/* unpack any accessor into tightly packed floats (applies sparse data and
 * normalized integer conversion). out_len < 0 signals parse (-1) / nomem (-2) */
static float *accessor_floats(const cgltf_accessor *a, int *out_len) {
    *out_len = 0;
    if (!a || a->count == 0) {
        return NULL;
    }
    size_t comps = cgltf_num_components(a->type);
    size_t total = (size_t)a->count * comps;
    if (comps == 0 || total == 0 || total > 0x7fffffffu) {
        *out_len = -1;
        return NULL;
    }
    float *buf = malloc(total * sizeof(float));
    if (!buf) {
        *out_len = -2;
        return NULL;
    }
    if (cgltf_accessor_unpack_floats(a, buf, total) != (cgltf_size)total) {
        free(buf);
        *out_len = -1;
        return NULL;
    }
    *out_len = (int)total;
    return buf;
}

static int build_surface(gltf_ctx *c, const cgltf_primitive *prim,
                         efx_surface_src *s) {
    (void)c;
    memset(s, 0, sizeof(*s));
    if (prim->type != cgltf_primitive_type_triangles) {
        return EFX_GLTF_ERR_PARSE;
    }
    const cgltf_accessor *pa =
        cgltf_find_accessor(prim, cgltf_attribute_type_position, 0);
    if (!pa) {
        return EFX_GLTF_ERR_PARSE;
    }
    int len = 0;
    float *pos = accessor_floats(pa, &len);
    if (!pos) {
        return len == -2 ? EFX_GLTF_ERR_NOMEM : EFX_GLTF_ERR_PARSE;
    }
    if (len <= 0 || len % 3 != 0) {
        surface_src_free(s);
        free(pos);
        return EFX_GLTF_ERR_PARSE;
    }
    s->positions = pos;
    s->positions_len = len;
    int vcount = len / 3;

    const cgltf_accessor *na =
        cgltf_find_accessor(prim, cgltf_attribute_type_normal, 0);
    if (na) {
        int nlen = 0;
        float *nrm = accessor_floats(na, &nlen);
        if (!nrm) {
            surface_src_free(s);
            return nlen == -2 ? EFX_GLTF_ERR_NOMEM : EFX_GLTF_ERR_PARSE;
        }
        if (nlen != vcount * 3) {
            free(nrm);
            surface_src_free(s);
            return EFX_GLTF_ERR_PARSE;
        }
        s->normals = nrm;
        s->normals_len = nlen;
    }

    const cgltf_accessor *ua =
        cgltf_find_accessor(prim, cgltf_attribute_type_texcoord, 0);
    if (ua) {
        int ulen = 0;
        float *uv = accessor_floats(ua, &ulen);
        if (!uv) {
            surface_src_free(s);
            return ulen == -2 ? EFX_GLTF_ERR_NOMEM : EFX_GLTF_ERR_PARSE;
        }
        if (ulen != vcount * 2) {
            free(uv);
            surface_src_free(s);
            return EFX_GLTF_ERR_PARSE;
        }
        s->uvs = uv;
        s->uvs_len = ulen;
    }

    const cgltf_accessor *ca =
        cgltf_find_accessor(prim, cgltf_attribute_type_color, 0);
    if (ca) {
        size_t comps = cgltf_num_components(ca->type);
        if (comps != 3 && comps != 4) {
            surface_src_free(s);
            return EFX_GLTF_ERR_PARSE;
        }
        int clen = 0;
        float *raw = accessor_floats(ca, &clen);
        if (!raw) {
            surface_src_free(s);
            return clen == -2 ? EFX_GLTF_ERR_NOMEM : EFX_GLTF_ERR_PARSE;
        }
        if (clen / (int)comps != vcount) {
            free(raw);
            surface_src_free(s);
            return EFX_GLTF_ERR_PARSE;
        }
        float *col = malloc((size_t)vcount * 4 * sizeof(float));
        if (!col) {
            free(raw);
            surface_src_free(s);
            return EFX_GLTF_ERR_NOMEM;
        }
        for (int v = 0; v < vcount; v++) {
            col[v * 4] = raw[v * (int)comps];
            col[v * 4 + 1] = raw[v * (int)comps + 1];
            col[v * 4 + 2] = raw[v * (int)comps + 2];
            col[v * 4 + 3] = comps == 4 ? raw[v * 4 + 3] : 1.0f;
        }
        free(raw);
        s->colors = col;
        s->colors_len = vcount * 4;
    }

    if (prim->indices) {
        size_t icount = prim->indices->count;
        if (icount % 3 != 0 || icount > 0x7fffffffu) {
            surface_src_free(s);
            return EFX_GLTF_ERR_PARSE;
        }
        uint32_t *idx = malloc((icount ? icount : 1) * sizeof(uint32_t));
        if (!idx) {
            surface_src_free(s);
            return EFX_GLTF_ERR_NOMEM;
        }
        for (size_t i = 0; i < icount; i++) {
            size_t v = cgltf_accessor_read_index(prim->indices, i);
            if (v >= (size_t)vcount) {
                free(idx);
                surface_src_free(s);
                return EFX_GLTF_ERR_PARSE;
            }
            idx[i] = (uint32_t)v;
        }
        s->indices = idx;
        s->indices_len = (int)icount;
    }
    return EFX_GLTF_OK;
}

/* ------------------------------------------------------------ importer */

efx_meshdata *efx_gltf_load_meshdata(efx_resource *res, const char *path,
                                     const efx_gltf_mesh_opts *opts, int *err) {
    if (err) {
        *err = EFX_GLTF_OK;
    }
    if (!res || !path || path[0] == '\0') {
        if (err) *err = EFX_GLTF_ERR_IO;
        return NULL;
    }
    cgltf_options options;
    memset(&options, 0, sizeof(options));
    options.file.read = gltf_file_read;
    options.file.release = gltf_file_release;
    options.file.user_data = res;

    cgltf_data *data = NULL;
    cgltf_result cr = cgltf_parse_file(&options, path, &data);
    if (cr != cgltf_result_success) {
        if (err) {
            *err = (cr == cgltf_result_file_not_found ||
                    cr == cgltf_result_io_error)
                       ? EFX_GLTF_ERR_IO
                       : EFX_GLTF_ERR_PARSE;
        }
        return NULL;
    }
    cr = cgltf_load_buffers(&options, data, path);
    if (cr != cgltf_result_success) {
        cgltf_free(data);
        if (err) {
            *err = (cr == cgltf_result_file_not_found ||
                    cr == cgltf_result_io_error)
                       ? EFX_GLTF_ERR_IO
                       : EFX_GLTF_ERR_PARSE;
        }
        return NULL;
    }

    if (data->extensions_required_count > 0) {
        cgltf_free(data);
        if (err) *err = EFX_GLTF_ERR_UNSUPPORTED;
        return NULL;
    }

    /* select one mesh */
    const cgltf_mesh *mesh = NULL;
    if (opts && opts->has_mesh && opts->is_name) {
        for (cgltf_size i = 0; i < data->meshes_count; i++) {
            if (data->meshes[i].name && opts->mesh_name &&
                strcmp(data->meshes[i].name, opts->mesh_name) == 0) {
                mesh = &data->meshes[i];
                break;
            }
        }
        if (!mesh) {
            cgltf_free(data);
            if (err) *err = EFX_GLTF_ERR_SELECTION;
            return NULL;
        }
    } else {
        long index = (opts && opts->has_mesh) ? opts->mesh_index : 0;
        if (index < 0 || (cgltf_size)index >= data->meshes_count) {
            cgltf_free(data);
            if (err) *err = EFX_GLTF_ERR_SELECTION;
            return NULL;
        }
        mesh = &data->meshes[index];
    }

    if (mesh->primitives_count < 1 ||
        mesh->primitives_count > EFX_MESH_MAX_SURFACES) {
        cgltf_free(data);
        if (err) *err = EFX_GLTF_ERR_CAP;
        return NULL;
    }

    gltf_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.data = data;
    ctx.res = res;
    ctx.gltf_path = path;
    ctx.image_count = (int)data->images_count;
    if (ctx.image_count > 0) {
        ctx.images = calloc((size_t)ctx.image_count, sizeof(efx_image *));
        ctx.image_state = calloc((size_t)ctx.image_count, 1);
        if (!ctx.images || !ctx.image_state) {
            ctx_free(&ctx);
            cgltf_free(data);
            if (err) *err = EFX_GLTF_ERR_NOMEM;
            return NULL;
        }
    }

    int surface_count = (int)mesh->primitives_count;
    efx_surface_src *srcs = calloc((size_t)surface_count, sizeof(*srcs));
    if (!srcs) {
        ctx_free(&ctx);
        cgltf_free(data);
        if (err) *err = EFX_GLTF_ERR_NOMEM;
        return NULL;
    }
    int rc = EFX_GLTF_OK;
    for (int i = 0; i < surface_count && rc == EFX_GLTF_OK; i++) {
        rc = build_surface(&ctx, &mesh->primitives[i], &srcs[i]);
    }
    if (rc != EFX_GLTF_OK) {
        for (int i = 0; i < surface_count; i++) {
            surface_src_free(&srcs[i]);
        }
        free(srcs);
        ctx_free(&ctx);
        cgltf_free(data);
        if (err) *err = rc;
        return NULL;
    }

    int merr = 0;
    efx_meshdata *md = efx_meshdata_create(srcs, surface_count, &merr);
    for (int i = 0; i < surface_count; i++) {
        surface_src_free(&srcs[i]);
    }
    free(srcs);
    if (!md) {
        ctx_free(&ctx);
        cgltf_free(data);
        if (err) *err = (merr == EFX_MESHERR_NOMEM) ? EFX_GLTF_ERR_NOMEM
                                                    : EFX_GLTF_ERR_PARSE;
        return NULL;
    }

    for (int i = 0; i < surface_count; i++) {
        const cgltf_primitive *prim = &mesh->primitives[i];
        if (!prim->material) {
            continue; /* engine default material */
        }
        efx_material mat;
        rc = material_from_gltf(&ctx, prim->material, &mat);
        if (rc != EFX_GLTF_OK) {
            efx_meshdata_destroy(md);
            ctx_destroy_textures(&ctx);
            ctx_free(&ctx);
            cgltf_free(data);
            if (err) *err = rc;
            return NULL;
        }
        efx_meshdata_set_material(md, i, &mat, 1);
    }

    ctx_free(&ctx);
    cgltf_free(data);
    if (err) *err = EFX_GLTF_OK;
    return md;
}
