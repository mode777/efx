/* physics.debugDraw recorder shared by the desktop binding and the web
 * bridge (script-api-ergonomics). Option-bag validation lives in the shared
 * prelude (ADR 0049); this TU is binding-free so both link it directly. */
#include "api/api_physics_debug.h"
#include "physics/broadphase.h"
#include "render/render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- physics.debugDraw ---- */

/* Unlit translucent hull meshes for collider visualization, generated at the
 * collider's real dimensions so the draw transform is a pure translation.
 * A small LRU cache holds the distinct shapes a world actually uses (a debug
 * tool: games have a handful); slots invalidated by render resets rebuild
 * lazily via the draw error path. */
#define DEBUG_HULL_CACHE 8

typedef struct {
    int used;
    int type;
    float a, b, c; /* sphere radius / box half extents */
    float h;       /* capsule total height */
    uint64_t mesh;
} debug_hull_slot;

static debug_hull_slot g_hulls[DEBUG_HULL_CACHE];
static int g_hull_next; /* LRU replacement index */

/* uploads one hull built from the given float streams; frees them either way */
static uint64_t hull_upload(float *pos, float *nrm, float *uv, uint32_t *idx,
                            int nv, int nt, const efx_material *mat) {
    efx_surface_src src;
    memset(&src, 0, sizeof(src));
    src.positions = pos;
    src.positions_len = (int)(nv * 3);
    src.normals = nrm;
    src.normals_len = (int)(nv * 3);
    src.uvs = uv;
    src.uvs_len = (int)(nv * 2);
    src.indices = idx;
    src.indices_len = (int)(nt * 3);
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(&src, 1, &err);
    free(pos);
    free(nrm);
    free(uv);
    free(idx);
    if (!md) {
        return 0;
    }
    efx_meshdata_set_material(md, 0, mat, 1);
    uint64_t handle = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    return handle;
}

static float *hull_floats(size_t n) {
    return calloc(n, sizeof(float));
}

static uint32_t *hull_u32(size_t n) {
    return calloc(n, sizeof(uint32_t));
}

/* axis-aligned box, full extents (2hx, 2hy, 2hz); per-face normals,
 * outward CCW winding (24 verts / 36 indices, prelude makeCube shape) */
static uint64_t hull_box(float hx, float hy, float hz,
                         const efx_material *mat) {
    static const float faces[6][3][3] = {
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},
        {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
        {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
        {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
    };
    static const int su[4] = {-1, 1, 1, -1};
    static const int sv[4] = {-1, -1, 1, 1};
    float *pos = hull_floats(24 * 3), *nrm = hull_floats(24 * 3);
    float *uv = hull_floats(24 * 2);
    uint32_t *idx = hull_u32(36);
    if (!pos || !nrm || !uv || !idx) {
        free(pos); free(nrm); free(uv); free(idx);
        return 0;
    }
    int p = 0, t = 0;
    for (int f = 0; f < 6; f++) {
        const float *n = faces[f][0], *eu = faces[f][1], *ev = faces[f][2];
        for (int c = 0; c < 4; c++) {
            /* half extent along each direction = its axis' half size */
            float hu = eu[0] ? hx : (eu[1] ? hy : hz);
            float hv = ev[0] ? hx : (ev[1] ? hy : hz);
            float hn = n[0] ? hx : (n[1] ? hy : hz);
            pos[p] = n[0] * hn + eu[0] * hu * su[c] + ev[0] * hv * sv[c];
            pos[p + 1] = n[1] * hn + eu[1] * hu * su[c] + ev[1] * hv * sv[c];
            pos[p + 2] = n[2] * hn + eu[2] * hu * su[c] + ev[2] * hv * sv[c];
            nrm[p] = n[0];
            nrm[p + 1] = n[1];
            nrm[p + 2] = n[2];
            uv[(p / 3) * 2] = (c == 1 || c == 2) ? 1 : 0;
            uv[(p / 3) * 2 + 1] = (c == 2 || c == 3) ? 1 : 0;
            p += 3;
        }
        idx[t++] = (uint32_t)(f * 4 + 0);
        idx[t++] = (uint32_t)(f * 4 + 1);
        idx[t++] = (uint32_t)(f * 4 + 2);
        idx[t++] = (uint32_t)(f * 4 + 0);
        idx[t++] = (uint32_t)(f * 4 + 2);
        idx[t++] = (uint32_t)(f * 4 + 3);
    }
    return hull_upload(pos, nrm, uv, idx, 24, 12, mat);
}

/* UV sphere of the given radius (12 slices x 8 bands) */
static uint64_t hull_sphere(float radius, const efx_material *mat) {
    const int S = 12, B = 8;
    const int nv = S * (B + 1), nt = S * B * 2;
    float *pos = hull_floats((size_t)nv * 3), *nrm = hull_floats((size_t)nv * 3);
    float *uv = hull_floats((size_t)nv * 2);
    uint32_t *idx = hull_u32((size_t)nt * 3);
    if (!pos || !nrm || !uv || !idx) {
        free(pos); free(nrm); free(uv); free(idx);
        return 0;
    }
    int p = 0, vi = 0;
    for (int i = 0; i <= B; i++) {
        float v = (float)i / B;
        float theta = v * 3.14159265f;
        float st = sinf(theta), ct = cosf(theta);
        for (int j = 0; j < S; j++) {
            float u = (float)j / S;
            float phi = u * 2.0f * 3.14159265f;
            float x = st * cosf(phi), y = ct, z = st * sinf(phi);
            pos[p] = x * radius; pos[p + 1] = y * radius; pos[p + 2] = z * radius;
            nrm[p] = x; nrm[p + 1] = y; nrm[p + 2] = z;
            uv[vi * 2] = u; uv[vi * 2 + 1] = v;
            p += 3;
            vi++;
        }
    }
    int t = 0;
    for (int i = 0; i < B; i++) {
        for (int j = 0; j < S; j++) {
            uint32_t a = (uint32_t)(i * S + j);
            uint32_t b = (uint32_t)(i * S + (j + 1) % S);
            uint32_t c = (uint32_t)((i + 1) * S + j);
            uint32_t d = (uint32_t)((i + 1) * S + (j + 1) % S);
            idx[t++] = a; idx[t++] = d; idx[t++] = c;
            idx[t++] = a; idx[t++] = b; idx[t++] = d;
        }
    }
    return hull_upload(pos, nrm, uv, idx, nv, nt, mat);
}

/* vertical capsule: total tip-to-tip `height`, cylinder + hemispherical caps
 * (12 slices; explicit ring table so the vertex/index counts are exact) */
static uint64_t hull_capsule(float radius, float height, const efx_material *mat) {
    const int S = 12;
    float hh = height * 0.5f - radius; /* segment half-length */
    /* interior rings top-to-bottom: (polar angle from +Y, y offset) */
    const float rings[7][2] = {
        {67.5f, hh}, {22.5f, hh}, {90.0f, hh}, {90.0f, 0.0f},
        {90.0f, -hh}, {112.5f, -hh}, {157.5f, -hh},
    };
    const int ring_count = 7;
    const int nv = S * ring_count + 2;
    const int nt = S * (ring_count + 1) * 2; /* ring bands + two pole fans */
    float *pos = hull_floats((size_t)nv * 3), *nrm = hull_floats((size_t)nv * 3);
    float *uv = hull_floats((size_t)nv * 2);
    uint32_t *idx = hull_u32((size_t)nt * 3);
    if (!pos || !nrm || !uv || !idx) {
        free(pos); free(nrm); free(uv); free(idx);
        return 0;
    }
    int vi = 0, p = 0;
    for (int r = 0; r < ring_count; r++) {
        float th = rings[r][0] * 3.14159265f / 180.0f;
        float st = sinf(th), ct = cosf(th);
        for (int j = 0; j < S; j++) {
            float u = (float)j / S;
            float phi = u * 2.0f * 3.14159265f;
            float x = st * cosf(phi), z = st * sinf(phi);
            pos[p] = x * radius;
            pos[p + 1] = ct * radius + rings[r][1];
            pos[p + 2] = z * radius;
            nrm[p] = x;
            nrm[p + 1] = ct;
            nrm[p + 2] = z;
            uv[vi * 2] = u;
            uv[vi * 2 + 1] = (float)r / (float)(ring_count - 1);
            p += 3;
            vi++;
        }
    }
    int pole_top = vi++;
    pos[p] = 0; pos[p + 1] = hh + radius; pos[p + 2] = 0;
    nrm[p] = 0; nrm[p + 1] = 1; nrm[p + 2] = 0;
    uv[pole_top * 2] = 0.5f; uv[pole_top * 2 + 1] = 0.0f;
    p += 3;
    int pole_bot = vi++;
    pos[p] = 0; pos[p + 1] = -hh - radius; pos[p + 2] = 0;
    nrm[p] = 0; nrm[p + 1] = -1; nrm[p + 2] = 0;
    uv[pole_bot * 2] = 0.5f; uv[pole_bot * 2 + 1] = 1.0f;
    p += 3;
    int t = 0;
    for (int r = 0; r + 1 < ring_count; r++) {
        for (int j = 0; j < S; j++) {
            uint32_t a = (uint32_t)(r * S + j);
            uint32_t b = (uint32_t)(r * S + (j + 1) % S);
            uint32_t c = (uint32_t)((r + 1) * S + j);
            uint32_t d = (uint32_t)((r + 1) * S + (j + 1) % S);
            idx[t++] = a; idx[t++] = b; idx[t++] = d;
            idx[t++] = a; idx[t++] = d; idx[t++] = c;
        }
    }
    for (int j = 0; j < S; j++) {
        idx[t++] = (uint32_t)pole_top;
        idx[t++] = (uint32_t)((j + 1) % S);
        idx[t++] = (uint32_t)j;
        idx[t++] = (uint32_t)pole_bot;
        idx[t++] = (uint32_t)((ring_count - 1) * S + j);
        idx[t++] = (uint32_t)((ring_count - 1) * S + (j + 1) % S);
    }
    return hull_upload(pos, nrm, uv, idx, nv, nt, mat);
}

static debug_hull_slot *hull_for(const efx_debug_collider *d,
                                 const efx_material *mat) {
    float a = d->shape.type == EFX_PHYS_SHAPE_BOX
                  ? d->shape.half.x
                  : (d->shape.type == EFX_PHYS_SHAPE_CAPSULE ? d->shape.height
                                                             : d->shape.radius);
    float b = d->shape.type == EFX_PHYS_SHAPE_BOX ? d->shape.half.y : 0.0f;
    float c = d->shape.type == EFX_PHYS_SHAPE_BOX ? d->shape.half.z : 0.0f;
    debug_hull_slot *free_slot = NULL;
    for (int i = 0; i < DEBUG_HULL_CACHE; i++) {
        debug_hull_slot *s = &g_hulls[i];
        if (s->used && s->type == d->shape.type && s->a == a && s->b == b &&
            s->c == c && s->h == (d->shape.type == EFX_PHYS_SHAPE_CAPSULE
                                      ? d->shape.height
                                      : 0.0f)) {
            return s;
        }
        if (!s->used && !free_slot) {
            free_slot = s;
        }
    }
    debug_hull_slot *s = free_slot ? free_slot : &g_hulls[g_hull_next];
    if (!free_slot) {
        g_hull_next = (g_hull_next + 1) % DEBUG_HULL_CACHE;
    }
    uint64_t mesh = 0;
    if (d->shape.type == EFX_PHYS_SHAPE_BOX) {
        mesh = hull_box(d->shape.half.x, d->shape.half.y, d->shape.half.z, mat);
    } else if (d->shape.type == EFX_PHYS_SHAPE_SPHERE) {
        mesh = hull_sphere(d->shape.radius, mat);
    } else if (d->shape.type == EFX_PHYS_SHAPE_CAPSULE) {
        mesh = hull_capsule(d->shape.radius, d->shape.height, mat);
    }
    if (!mesh) {
        return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->used = 1;
    s->type = d->shape.type;
    s->a = a;
    s->b = b;
    s->c = c;
    s->h = d->shape.type == EFX_PHYS_SHAPE_CAPSULE ? d->shape.height : 0.0f;
    s->mesh = mesh;
    return s;
}


/* Recorder shared by both bindings (script-api-ergonomics): the caller's
 * binding validated the option bag; this enumerates the world's live
 * colliders and records an unlit translucent hull per one into the display
 * list. Returns 0, or -1 on allocation failure (binding throws). */
int efx_physics_debug_draw_recorder(efx_physics_world *w, const float color[4]) {
    efx_material mat;
    efx_material_default(&mat);
    mat.unlit = 1;
    mat.blend = EFX_BLEND_ALPHA;
    mat.diffuse[0] = mat.diffuse[1] = mat.diffuse[2] = mat.diffuse[3] = 1.0f;

    int n = efx_physics_debug_colliders(w, NULL, 0);
    if (n <= 0) {
        return 0;
    }
    efx_debug_collider *cols = calloc((size_t)n, sizeof(*cols));
    if (!cols) {
        return -1;
    }
    efx_physics_debug_colliders(w, cols, n);
    for (int i = 0; i < n; i++) {
        debug_hull_slot *s = hull_for(&cols[i], &mat);
        if (!s) {
            continue;
        }
        /* hulls are generated at real dimensions: pure translation */
        const efx_vec3 *p = &cols[i].position;
        const float transform[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                     0, 0, 1, 0, p->x, p->y, p->z, 1};
        efx_render_mesh(s->mesh, transform, color, 0, 0);
    }
    free(cols);
    return 0;
}

