# Design

## Context

See `proposal.md` for motivation. F6a delivered the directory/zip provider,
image decoding, and the `load*` surface; this change is the first consumer of
it. Constraints that shape the approach:

- **Multi-surface model (ADR 0024):** one `MeshData` = 1..16 surfaces, each a
  glTF primitive; materials bind per surface; no global material state.
- **Fixed-function Phong (ADR 0015/0026/0027):** four channels + optional maps
  + binary alpha mask; no normal mapping, no per-material alpha beyond the
  mask, no shader permutations.
- **Texture behavior (F2):** `createTexture` has no sampler control today; the
  platform uses one linear/repeat sampler for all sampled textures.
- **Vendoring (ADR 0006):** pinned in-repo snapshots, build never on network.

## Goals / Non-Goals

**Goals:**

- One full glTF static importer producing engine `MeshData` + `Texture`s with
  no new script-visible resource type.
- A pinned, documented profile (container, references, material mapping,
  samplers, extension policy) so later glTF work does not re-litigate it.
- Keep the dependency delta to one vendored header (`cgltf`).

**Non-Goals:**

- Skins/animations (F6c), REPL (F6d), scene graph / `loadModel` (F8).
- Color management, mipmaps, normal/occlusion maps.

## Decisions

### D1 — Vendor cgltf for parsing; no hand-rolled glTF parser

`cgltf` (MIT, single-header C99, zero external deps — it carries its own jsmn
JSON parser) provides `.gltf`/`.glb`, accessors including sparse, buffers
(file/data-URI/GLB), images (file/data-URI/buffer view), materials, and the
extension bookkeeping this change needs.

- **Rejected — tinygltf:** C++11 and pulls nlohmann `json.hpp`; heavier and
  mixes a second language into the loader for no gain.
- **Rejected — hand-rolling:** accessors, sparse storage, GLB chunking, and
  data-URI/stride handling are a large, test-heavy surface for a solved
  problem.

### D2 — Provider-fed `cgltf_options.file.read`; no `FILE*` default loader

cgltf's default file loader cannot resolve URIs inside a zip. The importer
supplies a custom read callback that joins the glTF path's directory with the
URI and reads through the F6a provider, so external buffers/images work
identically for directory and zip roots. `.glb` buffer views and data-URIs are
handled by cgltf itself.

- **Rejected — materializing whole trees with the default loader:** breaks the
  zip root and duplicates the provider's path rules.

### D3 — Import one selected mesh; no scene graph

`loadMeshData(path, { mesh })` selects one glTF mesh (index or name, default
0) and maps its primitives to surfaces. Node/scene transforms are not applied;
the script positions the mesh at draw time. This matches ADR 0024's
primitive→surface mapping and the single-mesh `drawMesh` model.

- **Rejected — a `loadModel` node tree now:** introduces a resource type or a
  JS-managed scene the draw model cannot consume, and overlaps the F8
  high-level layer.
- **Rejected — baking the owning node transform:** surprises scripts and
  bakes a scene concept into geometry.

### D4 — Pinned PBR → Phong mapping

For each primitive material:

```
diffuse.color  = baseColorFactor (default white)
diffuse.map    = baseColorTexture
emissive.color = emissiveFactor (default black)
emissive.map   = emissiveTexture
specular.color = mix(black, baseColorFactor, metallicFactor)   // metallicFactor default 1
shininess      = clamp((1 - roughnessFactor)^2 * 128, 1, 128) // roughnessFactor default 1
alphaMask      = baseColorTexture when alphaMode == MASK (threshold 0.5)
ambient.color  = black (glTF has no ambient term)
occlusion / normal textures: ignored
```

The conversion is deliberately heuristic and documented in the reference so
authors can predict it; scripts may re-bind any surface with
`setMeshSurfaceMaterial`.

- **Rejected — minimal base-color-only mapping:** throws away metallic/rough
  distinction the engine's Phong specular can approximate.
- **Rejected — exposing raw PBR fields:** the consumer API has no PBR model;
  leaking it would force a material-system redesign.

### D5 — Texture samplers become per-texture options

`createTexture(imageData, opts?)` gains `{ wrap, filter }`; the render sink's
`create_texture` grows the sampler parameters and the platform caches samplers
by (wrap, filter) instead of using the single default. glTF samplers map to
these; absent/unsupported sampler values fall back to repeat/linear.

- **Rejected — ignoring samplers (F6a status quo):** tiled/clamped assets
  render visibly wrong; the change is small and additive.
- **Rejected — a separate `setTextureSampler`:** sampler is immutable creation
  state (sokol images bind a sampler); a setter would imply mutation the
  backend cannot honor cheaply.

### D6 — `loadMeshData` is the whole import

It returns a `MeshData` with geometry, converted/bound materials, and created
textures; `createMesh` uploads. No `loadMesh` convenience ships.

- **Rejected — a `loadMesh` convenience:** it would only wrap
  `createMesh(loadMeshData(...))`, and the CPU/GPU split already lets scripts
  inspect or delay the upload.

### D7 — Texture dedup and lifetime

Images are decoded once per glTF image and textures created once per (image,
sampler) pair, shared across materials/channels. The returned `MeshData`
retains the created textures until destroyed or consumed by `createMesh`
(which retains them via material bindings, ADR 0027); this is the same
retention discipline applied one stage earlier.

### D8 — Extension/morph policy

Optional unknown extensions are ignored; `extensionsRequired` naming an
unsupported extension is a load error; morph targets are ignored. A supported
extension allowlist starts empty in F6b.

## Risks / Trade-offs

- **[cgltf accessor variety]** → components may be u8/u16/u32/float, strided,
  sparse, or normalized; normalize all attributes to the engine's float layout
  at import and unit-test each component type and a sparse accessor.
- **[Sampler plumbing ripples through both bindings and the platform]** →
  thread the two enums through `efx_render_texture_create`/sink; keep defaults
  byte-identical so existing goldens and `createTexture(imageData)` calls are
  unchanged.
- **[`.gltf` external reference path resolution]** → covered by the D2
  callback and tested with a `.gltf` + `.bin` + `.png` fixture in a zip.
- **[Large assets / memory]** → images decode once (D7); no mipmaps in v1.
- **[/Werror over cgltf]** → isolate the cgltf TU or vendor it under the
  existing vendored-include treatment; verify on MSVC and clang.
- **[Golden churn]** → no existing rendering path changes; only a new golden
  baseline is added.
- **[sRGB appears wrong]** → documented non-goal; assets authored with linear
  workflows may need re-authoring.

## Migration Plan

1. Vendor cgltf; wire the provider read callback and a parse-only path; add a
   headless unit test that loads a `.glb` and asserts mesh/primitive counts.
2. Implement primitive→surface attribute import and material conversion;
   unit-test against a fixture with multiple materials and alpha MASK.
3. Add sampler options to `createTexture` through render and platform; verify
   existing goldens are unchanged.
4. Implement image import (external/data/view) with dedup and MeshData
   retention; add a golden rendering a committed `.glb` and a `.gltf` package.
5. Update docs, `efx.d.ts`, and the glTF-profile ADR; run the Linux → Windows →
   macOS gate order.

Rollback: the importer is additive; reverting removes `loadMeshData` and the
sampler options and leaves F6a and all existing behavior intact.

## Open Questions

None that affect the specs, approach, or task breakdown. Future additive
layers (scene/`loadModel`, mipmaps, color management) are recorded as
non-goals and can arrive as their own changes.
