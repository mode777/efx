#include "bridge_internal.h"

/* ------------------------------------------------- F3 (3D core) */

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_js_prelude(void) {
    return EFX_JS_PRELUDE; /* NUL-terminated; EFX_JS_PRELUDE_LEN bytes */
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_camera3d(float px, float py, float pz,
                                                  float tx, float ty, float tz,
                                                  float fov, float near_z,
                                                  float far_z) {
    efx_camera3d cam;
    memset(&cam, 0, sizeof(cam));
    cam.pos[0] = px;
    cam.pos[1] = py;
    cam.pos[2] = pz;
    cam.target[0] = tx;
    cam.target[1] = ty;
    cam.target[2] = tz;
    cam.fov = fov;
    cam.near_z = near_z;
    cam.far_z = far_z;
    efx_render_set_camera3d(&cam);
}

/* MeshData slot table (mirrors the desktop api.c wrapper ownership).
   Surfaces arrive one at a time from JS: they are validated as a probe
   MeshData, then their storage moves into the staging array; commit
   assembles the final efx_meshdata. */
typedef struct {
    efx_surface *staged; /* surface_count entries; storage NULL until filled */
    int filled;
    efx_meshdata *md;    /* set by commit */
    int alive;
    int surface_count;
} wmd_slot;

static struct {
    wmd_slot *slots;
    int count, cap;
} WMD;

static wmd_slot *wmd_get(int id) {
    if (id <= 0 || id > WMD.count) {
        return NULL;
    }
    return &WMD.slots[id - 1];
}

static void wmd_release(wmd_slot *s) {
    if (s->staged) {
        for (int i = 0; i < s->surface_count; i++) {
            efx_surface *sf = &s->staged[i];
            free(sf->positions);
            free(sf->normals);
            free(sf->uvs);
            free(sf->colors);
            free(sf->joints);
            free(sf->weights);
            free(sf->indices);
        }
        free(s->staged);
        s->staged = NULL;
    }
    efx_meshdata_destroy(s->md);
    s->md = NULL;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_create(int surface_count) {
    if (surface_count < 1 || surface_count > EFX_MESH_MAX_SURFACES) {
        return 0;
    }
    if (WMD.count >= WMD.cap) {
        int cap = WMD.cap ? WMD.cap * 2 : 16;
        wmd_slot *grown = realloc(WMD.slots, (size_t)cap * sizeof(wmd_slot));
        if (!grown) {
            return 0;
        }
        WMD.slots = grown;
        WMD.cap = cap;
    }
    wmd_slot *s = &WMD.slots[WMD.count];
    memset(s, 0, sizeof(*s));
    s->staged = calloc((size_t)surface_count, sizeof(efx_surface));
    if (!s->staged) {
        return 0;
    }
    s->surface_count = surface_count;
    s->alive = 1;
    WMD.count++;
    return WMD.count;
}

/* fill one staged surface; returns 0 ok, EFX_MESHERR_* on validation
   failure (the staged MeshData is released; the JS layer throws) */
EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_surface(int id, int index,
                                                     const float *positions,
                                                     int positions_len,
                                                     const float *normals,
                                                     int normals_len,
                                                     const float *uvs,
                                                     int uvs_len,
                                                     const float *colors,
                                                     int colors_len,
                                                     const uint32_t *joints,
                                                     int joints_len,
                                                     const float *weights,
                                                     int weights_len,
                                                     const uint32_t *indices,
                                                     int indices_len) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || index < 0 || index >= s->surface_count) {
        return EFX_MESHERR_COUNT;
    }
    efx_surface_src src;
    memset(&src, 0, sizeof(src));
    src.positions = positions;
    src.positions_len = positions_len;
    src.normals = normals;
    src.normals_len = normals_len;
    src.uvs = uvs;
    src.uvs_len = uvs_len;
    src.colors = colors;
    src.colors_len = colors_len;
    src.joints = joints;
    src.joints_len = joints_len;
    src.weights = weights;
    src.weights_len = weights_len;
    src.indices = indices;
    src.indices_len = indices_len;
    int err = 0;
    efx_meshdata *probe = efx_meshdata_create(&src, 1, &err);
    if (!probe) {
        wmd_release(s);
        s->alive = 0;
        return err;
    }
    /* transfer the validated surface storage into the staging slot */
    s->staged[index] = probe->surfaces[0];
    free(probe->surfaces);
    free(probe);
    s->filled++;
    return EFX_MESHERR_OK;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_surface_count(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->md) {
        return -1;
    }
    return s->md->surface_count;
}

/* assemble the final MeshData; returns 0 ok, EFX_MESHERR_* on failure */
EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_commit(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->staged) {
        return EFX_MESHERR_COUNT;
    }
    if (s->filled != s->surface_count) {
        wmd_release(s);
        s->alive = 0;
        return EFX_MESHERR_LEN;
    }
    s->md = malloc(sizeof(efx_meshdata));
    if (!s->md) {
        wmd_release(s);
        s->alive = 0;
        return EFX_MESHERR_NOMEM;
    }
    s->md->surface_count = s->surface_count;
    s->md->surfaces = s->staged;
    s->md->rig = NULL; /* script-built meshes carry no rig payload */
    s->staged = NULL; /* ownership moved into the meshdata */
    return EFX_MESHERR_OK;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_meshdata_destroy(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive) {
        return;
    }
    s->alive = 0;
    wmd_release(s);
}

/* F6b: import a glTF mesh straight into a MeshData slot (same slot table as
 * createMeshData, so createMesh/surfaceCount/destroy are unchanged). Returns
 * the 1-based slot id, or 0 on failure. */
EMSCRIPTEN_KEEPALIVE int efx_bridge_load_meshdata(const char *path, int has_mesh,
                                                  int is_name, int index,
                                                  const char *name) {
    if (!W.resource || !path) {
        return 0;
    }
    efx_gltf_mesh_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.has_mesh = has_mesh;
    opts.is_name = is_name;
    opts.mesh_index = index;
    opts.mesh_name = name;
    int e = EFX_GLTF_OK;
    efx_meshdata *md = efx_gltf_load_meshdata(W.resource, path, &opts, &e);
    if (!md) {
        return 0;
    }
    if (WMD.count >= WMD.cap) {
        int cap = WMD.cap ? WMD.cap * 2 : 16;
        wmd_slot *grown = realloc(WMD.slots, (size_t)cap * sizeof(wmd_slot));
        if (!grown) {
            efx_meshdata_destroy(md);
            return 0;
        }
        WMD.slots = grown;
        WMD.cap = cap;
    }
    wmd_slot *s = &WMD.slots[WMD.count];
    memset(s, 0, sizeof(*s));
    s->md = md;
    s->alive = 1;
    s->surface_count = md->surface_count;
    WMD.count++;
    return WMD.count;
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_mesh_create(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->md) {
        return 0;
    }
    return (double)efx_render_mesh_create(s->md);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_mesh_destroy(double handle) {
    efx_render_mesh_destroy((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_mesh_surface_count(double handle) {
    return efx_render_mesh_surface_count((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_draw_mesh(double handle,
                                              const float *transform,
                                              const float *color,
                                              int skinned) {
    return efx_render_mesh((uint64_t)handle, transform, color, skinned);
}

/* F7: one wire pose sample: [clip_index, time, weight] per entry */
EMSCRIPTEN_KEEPALIVE int efx_bridge_pose_mesh(double handle,
                                              const float *wire, int count) {
    if (count < 0) {
        return EFX_RENDER_ERR_INDEX;
    }
    efx_pose_sample *samples = NULL;
    if (count > 0) {
        samples = malloc((size_t)count * sizeof(*samples));
        if (!samples) {
            return EFX_RENDER_ERR_NOMEM;
        }
        for (int i = 0; i < count; i++) {
            samples[i].clip = (int)wire[i * 3];
            samples[i].time = wire[i * 3 + 1];
            samples[i].weight = wire[i * 3 + 2];
        }
    }
    int rc = efx_render_mesh_pose((uint64_t)handle, samples, count);
    free(samples);
    return rc;
}

/* resolve a clip name to its index for the web binding's name lookup */
EMSCRIPTEN_KEEPALIVE int efx_bridge_find_clip(double handle, const char *name) {
    return efx_render_mesh_find_clip((uint64_t)handle, name);
}

/* ------------------------------------------------- F4a (lighting) */

/* mat layout: ambient[4], diffuse[4], specular[4], emissive[4], shininess;
 * maps (F4b): [ambient, diffuse, specular, emissive, alphaMask] as doubles
 * (a float would truncate a 64-bit texture handle) */
static void bridge_mat_from_wire(efx_material *m, const float *f,
                                 const double *maps) {
    if (!f) {
        efx_material_default(m);
        return;
    }
    for (int i = 0; i < 4; i++) {
        m->ambient[i] = f[i];
        m->diffuse[i] = f[4 + i];
        m->specular[i] = f[8 + i];
        m->emissive[i] = f[12 + i];
    }
    m->shininess = f[16];
    if (maps) {
        m->ambient_map = (uint64_t)maps[0];
        m->diffuse_map = (uint64_t)maps[1];
        m->specular_map = (uint64_t)maps[2];
        m->emissive_map = (uint64_t)maps[3];
        m->alpha_mask = (uint64_t)maps[4];
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_point_light(int slot, int enabled,
                                                     float px, float py,
                                                     float pz, float r, float g,
                                                     float b, float a,
                                                     float range) {
    (void)a; /* alpha ignored by lighting */
    if (!enabled) {
        efx_render_set_point_light(slot, NULL);
        return;
    }
    efx_point_light l;
    memset(&l, 0, sizeof(l));
    l.enabled = 1;
    l.pos[0] = px; l.pos[1] = py; l.pos[2] = pz;
    l.color[0] = r; l.color[1] = g; l.color[2] = b; l.color[3] = 1;
    l.range = range;
    efx_render_set_point_light(slot, &l);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_directional_light(int enabled,
                                                           float dx, float dy,
                                                           float dz, float r,
                                                           float g, float b,
                                                           float a) {
    (void)a;
    if (!enabled) {
        efx_render_set_directional_light(NULL);
        return;
    }
    efx_dir_light l;
    memset(&l, 0, sizeof(l));
    l.enabled = 1;
    l.dir[0] = dx; l.dir[1] = dy; l.dir[2] = dz;
    l.color[0] = r; l.color[1] = g; l.color[2] = b; l.color[3] = 1;
    efx_render_set_directional_light(&l);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_mesh_set_material(double handle, int index,
                                                      const float *mat,
                                                      const double *maps,
                                                      int has) {
    efx_material m;
    if (has) {
        bridge_mat_from_wire(&m, mat, maps);
    } else {
        efx_material_default(&m);
    }
    return efx_render_mesh_set_material((uint64_t)handle, index,
                                        has ? &m : NULL, has);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_set_material(int id, int index,
                                                          const float *mat,
                                                          const double *maps,
                                                          int has) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->md) {
        return -1;
    }
    efx_material m;
    if (has) {
        bridge_mat_from_wire(&m, mat, maps);
    } else {
        efx_material_default(&m);
    }
    efx_meshdata_set_material(s->md, index, has ? &m : NULL, has);
    return 0;
}

