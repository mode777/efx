# 0058 — An unlit material flag and a depth-write-off mesh draw enable geometry skyboxes

Status: Accepted (2026-10, change `skybox-and-blob-shadows`)

Supports: ADR 0015 (fixed-function consumer API; engine-owned canned shaders),
ADR 0026 (one uniform-driven mesh shader, no per-material permutations),
ADR 0019 (value-snapshot display-list records), ADR 0021 (one shader source
per program). Related capability specs: `lighting` (material model), `3d-core`
(mesh drawing, primitives), `web-gallery` (showcase samples).

## Context

An oldschool 3D engine is expected to show a sky and a blob shadow, but the
renderer offered no way to do either correctly. Every mesh draw ran the full
Phong equation (`shaders/mesh.glsl`) and wrote depth, and every primitive was
outward-facing with back-face culling. A skybox therefore could not be built
from the public API: an inward-facing dome would be culled, and even if it
were not, drawing it first with depth writes would hide every scene object
farther than the dome. Blob shadows already composed from `drawBillboard`
(`facing: 'plane'`), which is camera-independent and depth-tests without
writing depth; that path needed no engine change, only an example and a small
ground lift. The remaining gap was exactly two mesh knobs the fixed-function
surface lacked, plus inward-facing primitives.

## Decision

- **`Material.unlit` (default `false`) bypasses the lighting equation.** When
  set, the shaded fragment is `diffuse.color × M_diffuse × albedo`, clamped,
  with `alphaMask`, the albedo alpha, and `blend` unchanged and `ambient`,
  `specular`, `emissive`, and every light ignored. The flag rides the existing
  material wire block (grown 18 → 19 floats) and one `mat_params.y` shader
  branch — the single uniform-driven mesh shader (ADR 0026) is preserved; no
  second shader or permutation is added.
- **`DrawMeshOptions.depthWrite` (default `true`) is per-draw render state.**
  The flag is stored on `efx_mesh_record` (value-snapshotted, ADR 0019) and
  selects a mesh pipeline variant. The mesh pipeline matrix grows from
  3 blends × 2 windings to 3 blends × 2 depth-write values × 2 windings = 12;
  the no-write variants keep `LESS_EQUAL`, so the mesh is still occluded by
  nearer geometry but never occludes later draws.
- **`makeCube`/`makeSphere` gain `inverted` (default `false`).** Inverted
  generation negates the per-vertex normals and reverses each triangle's
  winding, so the inside is front-facing under the engine's BACK/CCW culling.
  This is pure JS geometry in `src/prelude/prelude.js`; no native code and no
  new primitive type.
- **The skybox is geometry, not a pass or a cubemap.** The supported recipe is
  an `inverted` primitive carrying an `unlit` diffuse-mapped material,
  transformed to the camera and recorded before the scene with
  `depthWrite: false`. Because the sky writes no depth, scene geometry tests
  against the cleared far buffer and draws over it regardless of distance, so
  the sky radius need not exceed the scene extent.
- **Blob shadows compose existing billboards.** A `facing: 'plane'` billboard
  with a dark color, `blend: 'alpha'`, and a small lift above the ground; no
  new draw verb, no depth-bias/polygon-offset API.
- **No new unmanaged resource.** All three additions are plain value state
  (material field, record flag, CPU geometry); ADR 0011/0012 are untouched.

## Consequences

- The fixed-function API gains two generally useful knobs beyond skies: unlit
  texturing (signage, UI meshes, effects) and non-occluding mesh draws
  (decals, gizmos, backdrops). Consumer scripts still never see a shader.
- The mesh pipeline set doubles to 12 variants. This is a mechanical product of
  the existing blend × winding matrix, created once at install/rebind and
  selected by an integer index; it is the accepted cost of exposing depth
  writing as per-draw state without dynamic pipeline state.
- The material wire block is 19 floats; every mirror enumerated in
  `src/render/render.h` must move together (prelude `__efxMaterialWire`, web
  `__efxMaterial`, `wire_mat_from_block`, `bridge_mat_from_wire`, the web
  `createMeshData` stride, and the desktop `efx_api_read_material`), and the
  embedded prelude must be regenerated (`tools/gen_prelude.py`).
- Cubemaps, cube-map sampling, and environment/reflection mapping are
  explicitly deferred. A geometry skybox needs only a 2D equirectangular image
  (mapped through the sphere's existing equirect UVs) or a face atlas, so a
  cubemap resource is not on the critical path; adding it later is a separate
  change with its own decision.
- A sky must be recorded first (or writes no depth) to read as a background;
  the showcase and guidance order it first.
- Blob shadows remain flat decals: they do not conform to sloped or uneven
  ground, and a script that does not lift the quad can z-fight. Both are
  documented limitations, not bugs.

## Rejected alternatives

- **A dedicated `drawSkybox`/`drawBlob` verb or a fullscreen sky pass:** a new
  shader + pass + API for what the mesh and billboard paths already express,
  and it would not generalize to decals or unlit meshes.
- **A cubemap skybox now:** postponed; it drags in a cubemap resource,
  cube sampling, and environment-mapping scope for no benefit to a 2D-image
  sky.
- **Building the sky in JS from existing primitives (negative scale to flip
  winding, or the ambient-map-as-unlit trick):** obscure and fragile, and it
  cannot disable depth writing, so it cannot be made correct.
- **`unlit` as a `drawMesh` option or a second shader:** it is surface
  appearance, belongs to the material snapshot, and a second shader breaks the
  one-canned-shader rule (ADR 0026).
- **`depthWrite` as a material field:** it is render state at record time,
  orthogonal to appearance, and the same mesh may need it toggled per draw.
- **`unlit` reusing the `emissive` channel:** emissive is added on top of the
  lit result and is not modulated by the albedo, so it cannot show a texture
  exactly as authored.
- **New `makeInvertedCube`/`makeInvertedSphere` functions:** two near-duplicate
  members for one boolean.
- **Polygon-offset/depth-bias control for the blob decal:** Sokol takes bias
  as pipeline state with no dynamic apply, so it would open an unbounded set of
  pipeline variants; a small lift makes the flat case correct.
- **`depthTest: false` for the blob:** it would draw over any geometry between
  the camera and the ground, not just over the ground.
