# Proposal

## Why

Two PS2-era staples are awkward or impossible with the current fixed-function
surface: a **skybox** cannot be shown at all (every mesh is lit by the Phong
model, and every mesh draw writes depth), and a **blob shadow** works only if
the script fights coplanar z-fighting by hand. Both are expected of an
oldschool 3D engine and both can be enabled by three small, general-purpose
additions to the existing material/draw/primitive surface — no new draw verbs
and no cubemaps.

## What Changes

- Add an optional `unlit` flag to the JS-managed Phong **material**. When set,
  the surface bypasses the lighting equation: the final color is the material
  `diffuse` channel (color × map) modulated by the vertex-color/tint albedo,
  with the existing `alphaMask` and `blend` still applied. Ambient/specular/
  emissive and all lights are ignored. This gives an unlit textured surface
  without a shader on the consumer API (ADR 0015).
- Add an optional `depthWrite` boolean to `drawMesh` (default `true`). When
  `false`, the mesh draws depth-tested but does **not** write depth, so later
  geometry is never occluded by it. This is what lets a camera-locked sky mesh
  be recorded first and then covered by the scene.
- Add an optional `inverted` boolean to the `makeCube` and `makeSphere`
  primitives (default `false`). When `true`, normals point inward and winding
  is reversed so the inside of the primitive renders under the engine's
  back-face culling — the inward-facing sky cube/sphere, authored in the
  existing pure-JS primitive layer.
- Ship two curated gallery showcases built only on the public API:
  - `skybox-showcase` — an inverted sphere on a CC0 equirectangular sky image,
    drawn camera-locked with an `unlit` material and `depthWrite: false`,
    with a small lit scene so the sky reads as a background.
  - `blob-shadow-showcase` — a moving character mesh over a ground plane with a
    procedurally generated (`createImageData`) radial shadow drawn as a
    `facing: 'plane'` billboard and lifted a hair above the ground, with an
    orbiting camera to show the shadow stays flat.
- Add a rendering **golden scene** covering `unlit`, `inverted`, and
  `depthWrite: false` behavior (no third-party asset), captured per ADR 0020.

No breaking changes: every new field is optional and defaults preserve current
behavior byte-for-byte.

## Capabilities

### New Capabilities

- None. The behavior is expressed as extensions to existing capabilities.

### Modified Capabilities

- `lighting`: the Phong material model gains the `unlit` field, and the F4a
  lit-shading requirement gains the unlit bypass (color = diffuse × map ×
  albedo; no light contributions; alpha mask and blend unchanged).
- `3d-core`: `drawMesh` gains the `depthWrite` option and the whole-mesh
  depth requirement gains the no-write behavior; the procedural primitives
  requirement gains the `inverted` option on `makeCube` and `makeSphere`.
- `web-gallery`: a new requirement pins that the curated catalog includes a
  skybox showcase and a blob-shadow showcase, their demonstrated behavior,
  and that each is exercised by the gallery smoke run.

## Impact

- **Code**: `src/render/render.h` (`efx_material` + mesh record flag),
  `src/render/render_records.c`/mesh playback, `src/platform/pipeline.c`
  (unlit branch, `depthWrite` pipeline variants), `shaders/mesh.glsl`
  (unlit path), the material wire block and its mirrors
  (`src/api/api_3d.c`, `src/prelude/prelude.js`, `src/web/js/core.js`,
  `src/web/bridge_render3d.c`), the prelude primitives (`makeCube`/
  `makeSphere`), and `src/prelude/prelude.h` regeneration.
- **Script API**: `gallery/src/api/efx.d.ts` (`Material.unlit`,
  `DrawMeshOptions.depthWrite`, `MakeCubeOptions.inverted`/
  `MakeSphereOptions.inverted`) and the regenerated `docs/api/` reference;
  `docs/js-api.md` material/render-state guidance.
- **Decision record**: a new ADR is needed —
  `docs/decisions/0058-unlit-material-and-geometry-skybox.md` — recording the
  unlit/depth-write model extension and the geometry (not pass/cubemap)
  skybox approach, with cubemaps and environment mapping deferred.
- **Assets / gallery**: a CC0 equirectangular sky image (Poly Haven,
  `kloofendal_43d_clear_puresky`) downscaled and committed under
  `gallery/samples/curated/skybox-showcase/`, with a row and recipe in
  `gallery/samples/curated/CREDITS.md`; the curated `manifest.json`.
- **Verification**: unit tests for the new field validation and the unlit
  shading reference, a DOM-free ctest smoke, and a golden scene captured on
  the SSH server before the four-target gate (ADR 0020/0023).
- **Dependencies**: none new (no cubemap/EXR/HDR decoder).

### Non-goals

- No dedicated `drawSkybox` or `drawBlob` verb; the features are composed from
  `drawMesh`/`drawBillboard` and the primitives.
- No cubemaps, cube-map sampling, or environment/reflection mapping
  (postponed); the skybox uses a single 2D equirectangular texture.
- No shadow mapping, stencil shadow volumes, or projected-geometry shadows;
  the blob is a flat plane decal, and it does not conform to sloped or uneven
  ground.
- No per-object polygon-offset/depth-bias API; the blob uses a small
  world-space lift.
- No new script-visible resource class, and no consumer-facing shaders.
- No change to 2D quads, particles, billboards, or existing goldens.
