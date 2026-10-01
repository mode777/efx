#include "render_internal.h"

efx_meshdata *efx_meshdata_create(const efx_surface_src *src, int count,
                                  int *err) {
    if (err) *err = EFX_MESHERR_OK;
    if (count < 1 || count > EFX_MESH_MAX_SURFACES) {
        if (err) *err = EFX_MESHERR_COUNT;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        const efx_surface_src *s = &src[i];
        if (s->positions_len <= 0 || s->positions_len % 3 != 0) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->normals_len % 3 != 0 || s->uvs_len % 2 != 0 ||
            s->colors_len % 4 != 0) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        int vcount = s->positions_len / 3;
        if ((s->normals_len && s->normals_len / 3 != vcount) ||
            (s->uvs_len && s->uvs_len / 2 != vcount) ||
            (s->colors_len && s->colors_len / 4 != vcount)) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        /* F6c: joints and weights are an all-or-nothing pair, four
         * influences per vertex (glTF JOINTS_0 / WEIGHTS_0) */
        if ((s->joints_len == 0) != (s->weights_len == 0)) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->joints_len &&
            (s->joints_len != vcount * EFX_JOINTS_PER_VERTEX ||
             s->weights_len != vcount * EFX_WEIGHTS_PER_VERTEX)) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->indices_len % 3 != 0) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->indices_len == 0 && vcount % 3 != 0) {
            /* non-indexed surfaces draw as a triangle list */
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        for (int j = 0; j < s->indices_len; j++) {
            if ((size_t)s->indices[j] >= (size_t)vcount) {
                if (err) *err = EFX_MESHERR_INDEX;
                return NULL;
            }
        }
    }

    efx_meshdata *md = calloc(1, sizeof(efx_meshdata));
    if (!md) {
        if (err) *err = EFX_MESHERR_NOMEM;
        return NULL;
    }
    md->surface_count = count;
    md->surfaces = calloc((size_t)count, sizeof(efx_surface));
    if (!md->surfaces) {
        free(md);
        if (err) *err = EFX_MESHERR_NOMEM;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        const efx_surface_src *s = &src[i];
        efx_surface *d = &md->surfaces[i];
        d->vertex_count = s->positions_len / 3;
        d->index_count = s->indices_len;
        size_t fb = sizeof(float);
        d->positions = malloc((size_t)s->positions_len * fb);
        if (d->positions) memcpy(d->positions, s->positions,
                                 (size_t)s->positions_len * fb);
        if (s->normals_len) {
            d->normals = malloc((size_t)s->normals_len * fb);
            if (d->normals) memcpy(d->normals, s->normals,
                                   (size_t)s->normals_len * fb);
        }
        if (s->uvs_len) {
            d->uvs = malloc((size_t)s->uvs_len * fb);
            if (d->uvs) memcpy(d->uvs, s->uvs, (size_t)s->uvs_len * fb);
        }
        if (s->colors_len) {
            d->colors = malloc((size_t)s->colors_len * fb);
            if (d->colors) memcpy(d->colors, s->colors,
                                  (size_t)s->colors_len * fb);
        }
        if (s->joints_len) {
            d->joints = malloc((size_t)s->joints_len * sizeof(uint32_t));
            if (d->joints) memcpy(d->joints, s->joints,
                                  (size_t)s->joints_len * sizeof(uint32_t));
            d->weights = malloc((size_t)s->weights_len * fb);
            if (d->weights) memcpy(d->weights, s->weights,
                                   (size_t)s->weights_len * fb);
        }
        if (s->indices_len) {
            d->indices = malloc((size_t)s->indices_len * sizeof(uint32_t));
            if (d->indices) memcpy(d->indices, s->indices,
                                   (size_t)s->indices_len * sizeof(uint32_t));
        }
        int bad = (s->positions_len && !d->positions) ||
                  (s->normals_len && !d->normals) ||
                  (s->uvs_len && !d->uvs) ||
                  (s->colors_len && !d->colors) ||
                  (s->joints_len && (!d->joints || !d->weights)) ||
                  (s->indices_len && !d->indices);
        if (bad) {
            efx_meshdata_destroy(md);
            if (err) *err = EFX_MESHERR_NOMEM;
            return NULL;
        }
    }
    return md;
}

void efx_meshdata_destroy(efx_meshdata *md) {
    if (!md) {
        return;
    }
    if (md->surfaces) {
        for (int i = 0; i < md->surface_count; i++) {
            efx_surface *s = &md->surfaces[i];
            material_release_maps(&s->material);
            free(s->positions);
            free(s->normals);
            free(s->uvs);
            free(s->colors);
            free(s->joints);
            free(s->weights);
            free(s->indices);
        }
        free(md->surfaces);
    }
    efx_rig_free(md->rig);
    free(md);
}

/* ------------------------------------------------------ F6c rig payload */

void efx_rig_free(efx_rig *rig) {
    if (!rig) {
        return;
    }
    if (rig->clips) {
        for (int i = 0; i < rig->clip_count; i++) {
            efx_animation_clip *c = &rig->clips[i];
            if (c->channels) {
                for (int j = 0; j < c->channel_count; j++) {
                    free(c->channels[j].times);
                    free(c->channels[j].values);
                }
                free(c->channels);
            }
            free(c->name);
        }
        free(rig->clips);
    }
    free(rig->joint_nodes);
    free(rig->joint_parents);
    free(rig->inverse_bind);
    free(rig);
}

static float *clone_floats(const float *src, int n) {
    if (!src || n <= 0) {
        return NULL;
    }
    float *buf = malloc((size_t)n * sizeof(float));
    if (buf) {
        memcpy(buf, src, (size_t)n * sizeof(float));
    }
    return buf;
}

static int *clone_ints(const int *src, int n) {
    if (!src || n <= 0) {
        return NULL;
    }
    int *buf = malloc((size_t)n * sizeof(int));
    if (buf) {
        memcpy(buf, src, (size_t)n * sizeof(int));
    }
    return buf;
}

efx_rig *efx_rig_clone(const efx_rig *src) {
    if (!src) {
        return NULL;
    }
    efx_rig *r = calloc(1, sizeof(efx_rig));
    if (!r) {
        return NULL;
    }
    r->joint_count = src->joint_count;
    r->joint_nodes = clone_ints(src->joint_nodes, src->joint_count);
    r->joint_parents = clone_ints(src->joint_parents, src->joint_count);
    r->inverse_bind = clone_floats(src->inverse_bind, src->joint_count * 16);
    if (src->joint_count &&
        (!r->joint_nodes || !r->joint_parents || !r->inverse_bind)) {
        efx_rig_free(r);
        return NULL;
    }
    r->clip_count = src->clip_count;
    if (src->clip_count > 0) {
        r->clips = calloc((size_t)src->clip_count, sizeof(efx_animation_clip));
        if (!r->clips) {
            efx_rig_free(r);
            return NULL;
        }
        for (int i = 0; i < src->clip_count; i++) {
            const efx_animation_clip *sc = &src->clips[i];
            efx_animation_clip *dc = &r->clips[i];
            if (sc->name) {
                size_t n = strlen(sc->name) + 1;
                dc->name = malloc(n);
                if (!dc->name) {
                    efx_rig_free(r);
                    return NULL;
                }
                memcpy(dc->name, sc->name, n);
            }
            dc->channel_count = sc->channel_count;
            if (sc->channel_count > 0) {
                dc->channels = calloc((size_t)sc->channel_count,
                                      sizeof(efx_anim_channel));
                if (!dc->channels) {
                    efx_rig_free(r);
                    return NULL;
                }
                for (int j = 0; j < sc->channel_count; j++) {
                    const efx_anim_channel *scn = &sc->channels[j];
                    efx_anim_channel *dcn = &dc->channels[j];
                    *dcn = *scn;
                    dcn->times = clone_floats(scn->times, scn->times_len);
                    dcn->values = clone_floats(scn->values, scn->values_len);
                    if ((scn->times_len && !dcn->times) ||
                        (scn->values_len && !dcn->values)) {
                        efx_rig_free(r);
                        return NULL;
                    }
                }
            }
        }
    }
    return r;
}

void efx_meshdata_set_rig(efx_meshdata *md, efx_rig *rig) {
    if (!md) {
        efx_rig_free(rig);
        return;
    }
    efx_rig_free(md->rig);
    md->rig = rig;
}

void efx_meshdata_set_material(efx_meshdata *md, int index,
                               const efx_material *mat, int has) {
    if (!md || index < 0 || index >= md->surface_count) {
        return;
    }
    efx_surface *s = &md->surfaces[index];
    material_release_maps(&s->material);
    if (has && mat) {
        s->material = *mat;
        s->has_material = 1;
        material_retain_maps(&s->material);
    } else {
        s->has_material = 0;
        efx_material_default(&s->material);
    }
}

/* ------------------------------------------------------------- meshes */

/* build the interleaved GPU layout (design D1) into a pending block: one
 * backing allocation holding every surface's interleaved vertices followed
 * by its indices; p->surfs points into it */
static int pending_build(mesh_pending *p, const efx_meshdata *md) {
    if (!md || md->surface_count < 1 ||
        md->surface_count > EFX_MESH_MAX_SURFACES) {
        return EFX_RENDER_ERR_NOMEM;
    }
    size_t floats = 0;
    for (int i = 0; i < md->surface_count; i++) {
        floats += (size_t)md->surfaces[i].vertex_count * 12;
    }
    /* non-indexed surfaces draw through a synthesized identity index
       buffer (the pipeline is always-indexed; see pipeline.c) */
    size_t index_words = 0;
    for (int i = 0; i < md->surface_count; i++) {
        const efx_surface *s = &md->surfaces[i];
        index_words += s->index_count ? (size_t)s->index_count
                                      : (size_t)s->vertex_count;
    }
    size_t total_words = floats + index_words;
    p->data = malloc(total_words * sizeof(uint32_t));
    p->surfs = calloc((size_t)md->surface_count, sizeof(efx_mesh_gpu_surface));
    if (!p->data || !p->surfs) {
        free(p->data);
        free(p->surfs);
        p->data = NULL;
        p->surfs = NULL;
        return EFX_RENDER_ERR_NOMEM;
    }
    uint32_t *w = (uint32_t *)p->data;
    for (int i = 0; i < md->surface_count; i++) {
        const efx_surface *s = &md->surfaces[i];
        efx_mesh_gpu_surface *g = &p->surfs[i];
        g->vertex_count = s->vertex_count;
        g->index_count = s->index_count ? s->index_count : s->vertex_count;
        g->interleaved = (const float *)w;
        g->skinned = (md->rig != NULL && s->joints != NULL) ? 1 : 0;
        /* defaults: normal +z, uv 0, color white (design D1) */
        for (int v = 0; v < s->vertex_count; v++) {
            float *dst = (float *)w + v * 12;
            dst[0] = s->positions[v * 3];
            dst[1] = s->positions[v * 3 + 1];
            dst[2] = s->positions[v * 3 + 2];
            if (s->normals) {
                dst[3] = s->normals[v * 3];
                dst[4] = s->normals[v * 3 + 1];
                dst[5] = s->normals[v * 3 + 2];
            } else {
                dst[3] = 0.0f;
                dst[4] = 0.0f;
                dst[5] = 1.0f;
            }
            if (s->uvs) {
                dst[6] = s->uvs[v * 2];
                dst[7] = s->uvs[v * 2 + 1];
            } else {
                dst[6] = 0.0f;
                dst[7] = 0.0f;
            }
            if (s->colors) {
                dst[8] = s->colors[v * 4];
                dst[9] = s->colors[v * 4 + 1];
                dst[10] = s->colors[v * 4 + 2];
                dst[11] = s->colors[v * 4 + 3];
            } else {
                dst[8] = 1.0f;
                dst[9] = 1.0f;
                dst[10] = 1.0f;
                dst[11] = 1.0f;
            }
        }
        w += (size_t)s->vertex_count * 12;
        if (s->indices) {
            g->indices = (const uint32_t *)w;
            memcpy(w, s->indices, (size_t)s->index_count * sizeof(uint32_t));
            w += s->index_count;
        } else {
            g->indices = (const uint32_t *)w;
            for (int j = 0; j < s->vertex_count; j++) {
                *w++ = (uint32_t)j;
            }
        }
    }
    p->count = md->surface_count;
    return EFX_RENDER_OK;
}

void mesh_materials_free(mesh_slot *m);

void pending_free(mesh_pending *p) {
    free(p->data);
    free(p->surfs);
    p->data = NULL;
    p->surfs = NULL;
    p->count = 0;
}

uint64_t efx_render_mesh_create(const efx_meshdata *md) {
    if (!md || md->surface_count < 1 ||
        md->surface_count > EFX_MESH_MAX_SURFACES) {
        return 0;
    }
    /* reuse a released slot (generation bump invalidates stale handles),
       else grow */
    mesh_slot *m = NULL;
    for (int i = 0; i < R.mesh_count; i++) {
        if (!R.meshes[i].used) {
            m = &R.meshes[i];
            break;
        }
    }
    if (!m) {
        if (R.mesh_count >= R.mesh_cap) {
            if (!pool_grow((void **)&R.meshes, &R.mesh_cap, R.mesh_count + 1,
                           sizeof(mesh_slot), 16)) {
                return 0;
            }
        }
        m = &R.meshes[R.mesh_count];
        memset(m, 0, sizeof(*m));
    }
    uint32_t gen = m->gen + 1;
    mesh_pending pending = {0, NULL, NULL};
    void *native = NULL;
    if (pending_build(&pending, md) != EFX_RENDER_OK) {
        return 0;
    }
    int skinned = md->rig != NULL;
    /* F7: each skinned surface owns a posed CPU array seeded from the bind
       pose; the interleaved bind copy stays owned by the mesh, and the
       joints/weights attributes are retained for posing */
    float **posed = NULL;
    int *vert_count = NULL;
    uint32_t **joints = NULL;
    float **weights = NULL;
    if (skinned) {
        posed = calloc((size_t)md->surface_count, sizeof(float *));
        vert_count = calloc((size_t)md->surface_count, sizeof(int));
        joints = calloc((size_t)md->surface_count, sizeof(uint32_t *));
        weights = calloc((size_t)md->surface_count, sizeof(float *));
        if (!posed || !vert_count || !joints || !weights) {
            goto skin_alloc_fail;
        }
        for (int i = 0; i < md->surface_count; i++) {
            const efx_surface *s = &md->surfaces[i];
            vert_count[i] = s->vertex_count;
            if (!pending.surfs[i].skinned) {
                continue;
            }
            size_t bytes = (size_t)s->vertex_count * 12 * sizeof(float);
            posed[i] = malloc(bytes);
            size_t infl = (size_t)s->vertex_count * 4;
            joints[i] = malloc(infl * sizeof(uint32_t));
            weights[i] = malloc(infl * sizeof(float));
            if (!posed[i] || !joints[i] || !weights[i]) {
                goto skin_alloc_fail;
            }
            memcpy(posed[i], pending.surfs[i].interleaved, bytes);
            memcpy(joints[i], s->joints, infl * sizeof(uint32_t));
            memcpy(weights[i], s->weights, infl * sizeof(float));
        }
    }
    if (R.sink && R.sink->create_mesh) {
        native = R.sink->create_mesh(R.sink->ud, pending.surfs, pending.count);
        if (!native) {
            goto skin_alloc_fail;
        }
        /* F12: the interleaved CPU bind copy is retained for every mesh (not
         * just skinned ones) so createStaticMesh can read its triangles; it is
         * freed with the mesh. */
    }
    m->used = 1;
    m->alive = 1;
    m->gen = gen;
    m->surface_count = md->surface_count;
    m->native = native;
    m->pending = pending;
    m->skinned = skinned;
    m->posed = posed;
    m->vert_count = vert_count;
    m->joints = joints;
    m->weights = weights;
    m->pose_revision = 0;
    /* per-surface material bindings (F4a): copy the MeshData snapshot */
    m->materials = calloc((size_t)md->surface_count, sizeof(efx_material));
    m->has_material = calloc((size_t)md->surface_count, sizeof(uint8_t));
    if (!m->materials || !m->has_material) {
        mesh_materials_free(m);
        pending_free(&m->pending);
        if (native && R.sink && R.sink->destroy_mesh) {
            R.sink->destroy_mesh(R.sink->ud, native);
        }
        m->native = NULL;
        m->used = 0;
        return 0;
    }
    for (int i = 0; i < md->surface_count; i++) {
        efx_material_default(&m->materials[i]);
        if (md->surfaces[i].has_material) {
            m->materials[i] = md->surfaces[i].material;
            m->has_material[i] = 1;
        }
        material_retain_maps(&m->materials[i]);
    }
    /* F6c: deep-copy the rig payload so the Mesh outlives its MeshData */
    m->rig = efx_rig_clone(md->rig);
    if (md->rig && !m->rig) {
        mesh_materials_free(m);
        pending_free(&m->pending);
        if (native && R.sink && R.sink->destroy_mesh) {
            R.sink->destroy_mesh(R.sink->ud, native);
        }
        m->native = NULL;
        m->used = 0;
        return 0;
    }
    uint32_t idx = (uint32_t)(m - R.meshes) + 1;
    if ((int)idx > R.mesh_count) {
        R.mesh_count = (int)idx;
    }
    return ((uint64_t)gen << 32) | (uint64_t)idx;

skin_alloc_fail:
    for (int i = 0; i < md->surface_count; i++) {
        if (posed) free(posed[i]);
        if (joints) free(joints[i]);
        if (weights) free(weights[i]);
    }
    free(posed);
    free(joints);
    free(weights);
    free(vert_count);
    pending_free(&pending);
    return 0;
}

void mesh_materials_free(mesh_slot *m) {
    if (m->materials) {
        for (int i = 0; i < m->surface_count; i++) {
            material_release_maps(&m->materials[i]);
        }
    }
    free(m->materials);
    free(m->has_material);
    m->materials = NULL;
    m->has_material = NULL;
    if (m->posed) {
        for (int i = 0; i < m->surface_count; i++) {
            free(m->posed[i]);
        }
        free(m->posed);
        m->posed = NULL;
    }
    if (m->joints) {
        for (int i = 0; i < m->surface_count; i++) {
            free(m->joints[i]);
        }
        free(m->joints);
        m->joints = NULL;
    }
    if (m->weights) {
        for (int i = 0; i < m->surface_count; i++) {
            free(m->weights[i]);
        }
        free(m->weights);
        m->weights = NULL;
    }
    free(m->vert_count);
    m->vert_count = NULL;
    efx_rig_free(m->rig);
    m->rig = NULL;
    pending_free(&m->pending);
}

static int mesh_release(uint64_t h, mesh_slot **out) {
    mesh_slot *m = mesh_get(h);
    if (!m) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (!m->alive) {
        return EFX_RENDER_OK; /* idempotent */
    }
    m->alive = 0;
    if (!m->native && m->pending.data) {
        /* upload never happened; release the slot right away */
        pending_free(&m->pending);
        mesh_materials_free(m);
        m->used = 0;
        return EFX_RENDER_OK;
    }
    /* deferred native release at frame end (resource lifecycle rules) */
    if (R.sink && R.sink->destroy_mesh) {
        if (R.deferred_mesh_count >= R.deferred_mesh_cap) {
            if (!pool_grow((void **)&R.deferred_mesh, &R.deferred_mesh_cap,
                           R.deferred_mesh_count + 1, sizeof(int), 16)) {
                return EFX_RENDER_ERR_NOMEM;
            }
        }
        R.deferred_mesh[R.deferred_mesh_count++] = (int)(m - R.meshes);
    }
    if (out) {
        *out = m;
    }
    return EFX_RENDER_OK;
}

int efx_render_mesh_destroy(uint64_t h) {
    return mesh_release(h, NULL);
}

int efx_render_mesh_alive(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m && m->alive;
}

int efx_render_mesh_surface_count(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m && m->alive ? m->surface_count : -1;
}

void *efx_render_mesh_native(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m ? m->native : NULL;
}

int efx_render_mesh_geometry_count(uint64_t h, int *out_verts,
                                   int *out_indices) {
    mesh_slot *m = mesh_get(h);
    if (!m || !m->alive) return 0;
    int verts = 0, indices = 0;
    for (int i = 0; i < m->pending.count; i++) {
        const efx_mesh_gpu_surface *s = &m->pending.surfs[i];
        verts += s->vertex_count;
        indices += s->indices ? s->index_count : s->vertex_count;
    }
    if (out_verts) *out_verts = verts;
    if (out_indices) *out_indices = indices;
    return 1;
}

int efx_render_mesh_geometry(uint64_t h, float *positions,
                             uint32_t *indices) {
    mesh_slot *m = mesh_get(h);
    if (!m || !m->alive || !positions || !indices) return 0;
    int vbase = 0;
    int ibase = 0;
    for (int i = 0; i < m->pending.count; i++) {
        const efx_mesh_gpu_surface *s = &m->pending.surfs[i];
        for (int v = 0; v < s->vertex_count; v++) {
            const float *src = &s->interleaved[(size_t)v * 12];
            positions[(size_t)(vbase + v) * 3 + 0] = src[0];
            positions[(size_t)(vbase + v) * 3 + 1] = src[1];
            positions[(size_t)(vbase + v) * 3 + 2] = src[2];
        }
        if (s->indices) {
            for (int k = 0; k < s->index_count; k++) {
                indices[ibase + k] = (uint32_t)(vbase + s->indices[k]);
            }
            ibase += s->index_count;
        } else {
            for (int v = 0; v < s->vertex_count; v++) {
                indices[ibase + v] = (uint32_t)(vbase + v);
            }
            ibase += s->vertex_count;
        }
        vbase += s->vertex_count;
    }
    return 1;
}

const efx_rig *efx_render_mesh_rig(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return (m && m->alive) ? m->rig : NULL;
}

int efx_render_mesh_skinned(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m && m->alive && m->skinned;
}

int efx_render_mesh_find_clip(uint64_t h, const char *name) {
    mesh_slot *m = mesh_get(h);
    if (!m || !m->alive || !m->rig || !name) {
        return -1;
    }
    for (int i = 0; i < m->rig->clip_count; i++) {
        if (m->rig->clips[i].name && strcmp(m->rig->clips[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int efx_render_mesh_pose(uint64_t h, const efx_pose_sample *samples,
                         int count) {
    mesh_slot *m = mesh_get(h);
    if (!m || !m->alive || !m->skinned || !m->rig) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (count < 0 || (count > 0 && !samples)) {
        return EFX_RENDER_ERR_INDEX;
    }
    for (int i = 0; i < count; i++) {
        if (samples[i].clip < 0 || samples[i].clip >= m->rig->clip_count) {
            return EFX_RENDER_ERR_INDEX;
        }
    }
    int joints = m->rig->joint_count;
    if (joints <= 0) {
        return EFX_RENDER_OK; /* no usable joint transform: bind pose */
    }
    float *palette = malloc((size_t)joints * 16 * sizeof(float));
    if (!palette) {
        return EFX_RENDER_ERR_NOMEM;
    }
    if (efx_skin_evaluate(m->rig, samples, count, palette) != 0) {
        free(palette);
        return EFX_RENDER_ERR_INDEX;
    }
    for (int i = 0; i < m->surface_count; i++) {
        if (!m->posed || !m->posed[i] || !m->joints || !m->joints[i] ||
            !m->weights || !m->weights[i] || !m->pending.surfs) {
            continue;
        }
        const float *bind = m->pending.surfs[i].interleaved;
        efx_skin_surface(bind, m->vert_count[i], m->joints[i], m->weights[i],
                         palette, joints, m->posed[i]);
    }
    free(palette);
    m->pose_revision++;
    return EFX_RENDER_OK;
}

const float *efx_render_mesh_posed(uint64_t h, int surface) {
    mesh_slot *m = mesh_get(h);
    if (!m || !m->alive || !m->posed || surface < 0 ||
        surface >= m->surface_count) {
        return NULL;
    }
    return m->posed[surface];
}

uint32_t efx_render_mesh_pose_revision(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return (m && m->alive) ? m->pose_revision : 0;
}

int efx_render_mesh_set_material(uint64_t h, int surface,
                                 const efx_material *mat, int has) {
    mesh_slot *m = mesh_get(h);
    if (!m || !m->alive) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (surface < 0 || surface >= m->surface_count) {
        return EFX_RENDER_ERR_INDEX;
    }
    material_release_maps(&m->materials[surface]);
    if (has && mat) {
        m->materials[surface] = *mat;
        m->has_material[surface] = 1;
        material_retain_maps(&m->materials[surface]);
    } else {
        efx_material_default(&m->materials[surface]);
        m->has_material[surface] = 0;
    }
    return EFX_RENDER_OK;
}

int efx_render_mesh_surface_material(uint64_t h, int surface,
                                     efx_material *out) {
    /* no alive check: playback may still reference a mesh destroyed earlier
     * in the same frame (native and materials release at frame end) */
    mesh_slot *m = mesh_get(h);
    if (!m || surface < 0 || surface >= m->surface_count) {
        if (out) {
            efx_material_default(out);
        }
        return -1;
    }
    if (out) {
        *out = m->materials[surface];
    }
    return m->has_material[surface] ? 1 : 0;
}

/* -------------------------------------------------------------- affine */
static float lclampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void lnormalize3(float out[3], const float v[3]) {
    float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 0.0f) {
        out[0] = v[0] / len;
        out[1] = v[1] / len;
        out[2] = v[2] / len;
    } else {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

/* one light's contribution (shares the exact formula with the shader;
 * channel maps scale the diffuse/specular colors, design D1/D8) */
static void lighting_term(const efx_material *mat, const efx_map_samples *maps,
                          const float contrib[3], float atten, const float N[3],
                          const float V[3], const float L[3],
                          const float albedo[3], float out[3]) {
    float ndl = N[0] * L[0] + N[1] * L[1] + N[2] * L[2];
    if (ndl < 0.0f) {
        ndl = 0.0f;
    }
    float H[3] = {L[0] + V[0], L[1] + V[1], L[2] + V[2]};
    float Hn[3];
    lnormalize3(Hn, H);
    float ndh = N[0] * Hn[0] + N[1] * Hn[1] + N[2] * Hn[2];
    if (ndh < 0.0f) {
        ndh = 0.0f;
    }
    float sp = powf(ndh, mat->shininess);
    float scale = atten;
    for (int c = 0; c < 3; c++) {
        float dc = mat->diffuse[c] * maps->diffuse[c];
        float sc = mat->specular[c] * maps->specular[c];
        float d = dc * albedo[c] * ndl;
        float s = sc * sp;
        out[c] += (d + s) * contrib[c] * scale;
    }
}

int efx_lighting_shade(const efx_material *mat, const efx_light_set *lights,
                       const float world_pos[3], const float normal[3],
                       const float camera_pos[3], const float albedo[4],
                       const efx_map_samples *maps, float out[4]) {
    efx_material def;
    efx_light_set empty;
    efx_map_samples neutral;
    if (!mat) {
        efx_material_default(&def);
        mat = &def;
    }
    if (!lights) {
        memset(&empty, 0, sizeof(empty));
        lights = &empty;
    }
    if (!maps) {
        for (int c = 0; c < 3; c++) {
            neutral.ambient[c] = 1.0f;
            neutral.diffuse[c] = 1.0f;
            neutral.specular[c] = 1.0f;
            neutral.emissive[c] = 1.0f;
        }
        neutral.mask_alpha = 1.0f;
        neutral.has_mask = 0;
        maps = &neutral;
    }
    if (maps->has_mask && maps->mask_alpha < 0.5f) {
        return 1; /* alpha-mask cutout (design D2) */
    }
    float N[3];
    lnormalize3(N, normal);
    float V[3] = {camera_pos[0] - world_pos[0],
                  camera_pos[1] - world_pos[1],
                  camera_pos[2] - world_pos[2]};
    lnormalize3(V, V);

    float col[3];
    for (int c = 0; c < 3; c++) {
        col[c] = mat->ambient[c] * maps->ambient[c] * albedo[c] +
                 mat->emissive[c] * maps->emissive[c];
    }
    for (int i = 0; i < EFX_MAX_POINT_LIGHTS; i++) {
        const efx_point_light *p = &lights->points[i];
        if (!p->enabled) {
            continue;
        }
        float toL[3] = {p->pos[0] - world_pos[0],
                        p->pos[1] - world_pos[1],
                        p->pos[2] - world_pos[2]};
        float d = sqrtf(toL[0] * toL[0] + toL[1] * toL[1] + toL[2] * toL[2]);
        float L[3];
        if (d > 0.0f) {
            L[0] = toL[0] / d;
            L[1] = toL[1] / d;
            L[2] = toL[2] / d;
        } else {
            L[0] = 0.0f; L[1] = 1.0f; L[2] = 0.0f;
        }
        float atten = p->range > 0.0f ? lclampf(1.0f - d / p->range, 0.0f, 1.0f)
                                      : 1.0f;
        lighting_term(mat, maps, p->color, atten, N, V, L, albedo, col);
    }
    if (lights->directional.enabled) {
        const efx_dir_light *dl = &lights->directional;
        float L[3] = {-dl->dir[0], -dl->dir[1], -dl->dir[2]};
        lnormalize3(L, L);
        lighting_term(mat, maps, dl->color, 1.0f, N, V, L, albedo, col);
    }
    out[0] = lclampf(col[0], 0.0f, 1.0f);
    out[1] = lclampf(col[1], 0.0f, 1.0f);
    out[2] = lclampf(col[2], 0.0f, 1.0f);
    out[3] = albedo[3];
    return 0;
}
