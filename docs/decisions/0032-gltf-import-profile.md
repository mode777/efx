# 0032 — glTF 2.0 import profile (static assets)

Status: Accepted (2026-09, change `f6b-gltf-import`)

Supports: vision.md (asset format is glTF 2.0); ADR 0004 (one `efx`
namespace, no new resource types); ADR 0024 (multi-surface meshes,
materials bind per surface); ADR 0026/0027 (fixed-function Phong: four
channels + maps + binary alpha mask, no permutations); ADR 0031 (the
dir/zip provider and synchronous `load*` API).

## Context

F6 must bring authored meshes into the engine, and the roadmap defers the
glTF profile to F6. Left open, each later importer would re-decide what
part of glTF 2.0 it accepts. The engine has no PBR material model and no
scene graph (`drawMesh` consumes one mesh with a script-supplied
transform), so the importer has to pick a fixed subset and a deterministic
PBR→Phong mapping. The F6a provider already resolves root-relative paths
synchronously on desktop and web, so references must flow through it.

## Decision

- **One parser, one header.** `vendor/cgltf/cgltf.h` (MIT, C99) parses both
  containers; `src/resource/gltf.c` maps the result to `MeshData` +
  `Texture`. No hand-rolled parser and no second language in the loader.
- **Containers/references.** `.glb` and `.gltf` load through the provider.
  Buffers and images may be external files (resolved relative to the
  asset's path *inside the root*), data-URIs, or buffer views. Missing or
  undecodable references fail the import.
- **One selected mesh, no scene graph.** `loadMeshData(path, { mesh })`
  selects one mesh by index or name (default first) and turns each of its
  primitives into one `MeshData` surface, in order, under the fixed 1..16
  cap. Node/scene transforms are never applied; geometry imports exactly as
  authored and the script positions it with `drawMesh`.
- **Attributes.** Positions/normals/uvs (TEXCOORD_0)/colors (COLOR_0) and
  indices; any component type (u8/u16/u32/float), normalized or not,
  strided or sparse, is unpacked to the engine's float layout. Triangulated
  triangle-list primitives only.
- **PBR→Phong (pinned heuristic).** Per primitive material:
  `diffuse = baseColorFactor` (+ base color texture); `emissive =
  emissiveFactor` (+ emissive texture); `specular = mix(black,
  baseColorFactor, metallicFactor)`; `shininess = clamp((1 −
  roughnessFactor)² · 128, 1, 128)`; `alphaMode: MASK` sets the alpha mask
  from the base color texture (threshold 0.5); occlusion/normal textures
  are ignored; no glTF material means the engine default. Scripts may
  override any surface with `setMeshSurfaceMaterial`.
- **Samplers are per-texture creation state.** `createTexture(imageData,
  { wrap, filter })` gains `repeat|clamp|mirror` (default repeat) and
  `linear|nearest` (default linear); glTF samplers map onto it and the
  platform caches one sampler per (wrap, filter) pair. A texture is
  created once per (image, sampler) pair and shared.
- **Feature policy.** Morph targets are ignored; unknown *optional*
  extensions are ignored; any `extensionsRequired` entry fails the import
  (the supported-extension allowlist starts empty). sRGB/color management,
  mipmaps, skins, animation, and unsupported `KHR_*` material features are
  out of scope here.

## Consequences

- A later glTF change (skins/animation for F6c/F7, scene/`loadModel` for
  F8) extends this profile rather than re-litigating container, reference,
  sampler, or material-mapping choices.
- Imported textures are retained by the returned `MeshData` until it is
  destroyed or `createMesh` consumes it, matching the F4b bound-map
  retention rule; scripts need not hold each texture.
- Assets authored for a linear/PBR renderer may look different under the
  engine's Phong model; the mapping is documented so authors can predict
  or override it.
- No new script-visible resource type: importer output is the existing
  `MeshData`.

## Rejected alternatives

- **tinygltf / a hand-rolled parser**: C++ plus a heavier JSON dependency,
  or a large test surface (accessors, sparse, GLB chunking, data-URIs) for
  a solved problem.
- **Materializing trees with cgltf's default file loader**: breaks the zip
  root and duplicates the provider's path rules; a provider-fed `file.read`
  callback keeps one resolution scheme.
- **A `loadModel` scene graph now**: introduces a resource type and a scene
  the single-mesh `drawMesh` model cannot consume, and overlaps F8.
- **Baking the owning node transform**: surprises scripts and bakes a scene
  concept into geometry.
- **Exposing raw PBR fields**: the consumer API has no PBR model; leaking
  it would force a material-system redesign.
- **Ignoring samplers / a mutable `setTextureSampler`**: ignoring them
  renders tiled or clamped assets wrong; a setter implies mutation the
  backend cannot honor cheaply (sokol binds a sampler to the image).
- **A separate `loadMesh` convenience**: `loadMeshData` is the whole
  import; `loadMesh` would only wrap `createMesh(loadMeshData(...))`.
