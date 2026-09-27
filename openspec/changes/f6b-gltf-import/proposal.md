# Proposal

## Why

F6a gives the engine a resource root and a way to read bytes and decode images,
but no way to bring in a real model. F6b delivers glTF 2.0 static-asset import
— geometry, materials, and textures — on top of the F6a provider, so scripts
can load authored meshes and the public gallery can show real assets. It also
settles the glTF profile the roadmap defers to F6 (container, reference forms,
image embedding, allowed material features).

## What Changes

- **glTF/GLB import via `loadMeshData(path, opts?)`.** Imports a glTF 2.0 asset
  into a `MeshData` — one surface per `mesh.primitives[]` of the selected mesh
  (1..16, the fixed surface cap). `opts.mesh` selects by index or name
  (default the first mesh). The import fills geometry (positions/normals/uvs/
  colors/indices), converts each primitive's material to the engine's Phong
  material, decodes its images, creates `Texture`s with the glTF samplers, and
  binds the maps per surface. `createMesh(meshData)` then uploads. There is no
  separate `loadMesh` convenience — `loadMeshData` is the whole import.
- **Container and reference profile (pinned).** `.glb` (self-contained) and
  `.gltf` are both supported; `.gltf` buffers and images may be external files,
  data-URIs, or (for `.glb`) buffer views. The asset is loaded through the F6a
  provider, so external references resolve inside a directory or zip root.
- **Node transforms are not applied.** Imported geometry is as authored; the
  script supplies the `drawMesh` transform. Scenes/nodes/instancing are not
  modeled (a scene/`loadModel` layer is deferred).
- **PBR → Phong material mapping (pinned).** `baseColor` → diffuse,
  `emissive` → emissive, `metallic` → specular color (lerp black→baseColor),
  `roughness` → shininess; `alphaMode: MASK` → `alphaMask`; occlusion and
  normal textures are ignored.
- **Texture sampler support.** `createTexture(imageData, opts?)` gains optional
  `{ wrap, filter }` (defaults preserve today's repeat/linear behavior), and
  glTF samplers are honored per texture.
- **glTF policy.** Morph targets are ignored; an asset whose
  `extensionsRequired` names an unsupported extension fails to load; unknown
  non-required extensions are ignored. sRGB/color management is not performed.
- **Dependency.** Vendor **cgltf** (MIT, single-header, C99) for glTF/GLB
  parsing, per the roadmap's proposal-time dependency evaluation.
- **Docs/contract.** `docs/js-api.md` and `gallery/src/api/efx.d.ts` gain the
  F6b entries; the removed provisional `loadMesh` is dropped. ADR 0031 (F6a)
  is extended or a follow-up ADR records the glTF profile.

## Capabilities

### New Capabilities

- `gltf-import`: glTF 2.0 static import — container/reference support, mesh
  selection, primitive→surface mapping, PBR→Phong material conversion, image
  decoding and texture binding with samplers, and the pinned extension/morph/
  transform policy.

### Modified Capabilities

- `js-api`: catalog `loadMeshData(path, opts?)` and the optional sampler
  options on `createTexture`, and drop the provisional `loadMesh`.
- `2d-layer`: `createTexture` accepts optional sampler options (`wrap`,
  `filter`) with unchanged defaults.

## Impact

- **Vendored:** `vendor/cgltf/` (new, MIT); `vendor/README.md` pin table.
- **New code:** glTF import in `src/resource/` (parse, map, decode images);
  sampler plumbing through the render sink (`src/render`, `src/platform`).
- **Modified code:** `src/api/api.c` and `src/web/bridge.c` (loadMeshData
  binding, createTexture sampler args), `CMakeLists.txt` (vendor include).
- **Docs:** `docs/js-api.md`, `gallery/src/api/efx.d.ts`, ADR for the glTF
  profile; roadmap/AGENTS current-state row for the F6b slice.
- **Milestone:** F6 (F6b slice). Predecessor F6a must pass its gate first.
- **Verification:** a committed `.glb` (and a `.gltf` with external `.bin`/
  `.png`) rendered in a golden; unit tests for primitive→surface mapping,
  material conversion, sampler mapping, and profile errors.

**Non-goals (out of scope for F6b):**

- Skins, skeletons, and animation clips (F6c).
- REPL mode (F6d).
- A scene graph / `loadModel` / `drawModel` (F8 high-level layer).
- Morph targets and unsupported `KHR_*` material features.
- sRGB/color management, mipmaps, and anisotropic filtering.
- Deciding skinning attributes (`JOINTS_0`/`WEIGHTS_0`) — those arrive in F6c.
