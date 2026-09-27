#ifndef EFX_GLTF_H
#define EFX_GLTF_H

/*
 * glTF 2.0 static import (F6b): parses a .glb/.gltf asset through the F6a
 * resource provider and builds an engine MeshData — one surface per glTF
 * primitive of one selected mesh, with PBR->Phong materials and imported
 * textures bound per surface. Pure C (no quickjs/sokol); the texture
 * handles it creates live in the render registry.
 *
 * Profile (ADR 0032): .glb and .gltf; external/data-URI/buffer-view buffers
 * and images; node/scene transforms not applied; morph targets ignored;
 * any extensionsRequired entry is unsupported and fails the load.
 */

#include "render/render.h"
#include "resource/resource.h"

/* error codes (distinct, so the binding can pick an exception kind) */
#define EFX_GLTF_OK 0
#define EFX_GLTF_ERR_PARSE 1        /* malformed / unreadable / bad primitive */
#define EFX_GLTF_ERR_UNSUPPORTED 2  /* extensionsRequired names an extension */
#define EFX_GLTF_ERR_SELECTION 3    /* opts.mesh names no mesh */
#define EFX_GLTF_ERR_CAP 4          /* primitive count outside 1..16 */
#define EFX_GLTF_ERR_IMAGE 5        /* a referenced image cannot be decoded */
#define EFX_GLTF_ERR_NOMEM 6
#define EFX_GLTF_ERR_IO 7           /* provider read failure */

/* mesh selection: default = the first mesh; otherwise an index or a name */
typedef struct efx_gltf_mesh_opts {
    int has_mesh;         /* 0 = default (first mesh) */
    int is_name;          /* 1 = mesh_index is unused, mesh_name is used */
    int mesh_index;
    const char *mesh_name; /* borrowed */
} efx_gltf_mesh_opts;

/* Imports one mesh from `path` (relative to the provider root) into a
 * freshly allocated MeshData. Returns NULL and sets *err (when non-NULL)
 * to an EFX_GLTF_ERR_* on failure. */
efx_meshdata *efx_gltf_load_meshdata(efx_resource *res, const char *path,
                                     const efx_gltf_mesh_opts *opts, int *err);

#endif
