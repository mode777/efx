# Design

## Context

See `proposal.md` — Why. Three facts from the current renderer shape the
approach: the mesh shader always runs the full Phong equation
(`shaders/mesh.glsl`), the mesh pipeline always writes depth and always
back-face culls CCW (`src/platform/pipeline.c`), and the pure-JS primitives
already generate all geometry (`src/prelude/prelude.js`). The material object
is marshalled as a fixed 18-float wire block whose mirrors are enumerated in
`src/render/render.h`; the display list snapshots the material and camera per
record (ADR 0019). Blob shadows use the existing world-space billboard path
(`drawBillboard`), whose `facing: 'plane'` basis is derived from the plane
normal alone and is therefore independent of the camera
(`src/render/render_particles.c`).

## Goals / Non-Goals

**Goals:**

- Enable a period-accurate geometry skybox and a flat blob shadow through
  minimal, general-purpose extensions to the existing material/draw/primitive
  surface, with no new draw verb and no consumer-facing shader.
- Keep every new field optional so all existing goldens stay byte-identical.
- Reuse the existing display-list, snapshot, batching, and pipeline machinery.

**Non-Goals:**

- Cubemaps, cube-map sampling, equirect sampling in a shader, or environment
  mapping.
- A dedicated sky/background pass, a dedicated skybox/blob draw verb, or
  polygon-offset/depth-bias control.
- Shadow mapping, stencil shadows, or projected-geometry shadows that conform
  to arbitrary ground.
- New script-visible resource classes.

## Decisions

### D1 — `unlit` is a material flag, not a draw verb or a shader-visible concept

Add `unlit?: boolean` (default `false`) to the JS-managed material. When set,
the shaded color is the **`diffuse` channel color × `diffuse` map × albedo**,
with `alphaMask` and `blend` unchanged and no light contribution. `unlit` is
snapshotted with the rest of the material. The shader gets one uniform branch
(`mat_params.y`), preserving the single-shader, uniform-driven model
(ADR 0026/0015) rather than a second shader or a permutation.

- **Rejected — a second "unlit" shader/pipeline**: doubles the mesh shader
  surface and the pipeline matrix, and breaks the one-canned-shader rule.
- **Rejected — reuse `emissive` as the unlit term**: emissive is added on top
  of the lit result and is not modulated by the albedo, so it cannot express
  "texture exactly as authored" and would silently interact with lights.
- **Rejected — `unlit` as a `drawMesh` option**: it is a property of the
  surface's appearance and belongs to the material snapshot that already
  governs the surface, not to a single draw.

### D2 — `depthWrite` is a `drawMesh` option implemented as record state + pipeline variants

Add `depthWrite?: boolean` (default `true`) to `DrawMeshOptions`. The flag is
stored on the mesh record (value-snapshotted) and selects a mesh pipeline
variant. The mesh pipeline set grows from 3 blends × 2 windings to
3 blends × 2 depth-write values × 2 windings = 12; the existing `flip` (GL
render-target) `_cw` variants already exist, so this is a mechanical doubling.
The no-write variant keeps `compare = LESS_EQUAL` so the mesh is still
occluded by nearer geometry and still occludes nothing after it.

- **Rejected — a material field**: depth writing is render state at record
  time, orthogonal to appearance, and the same mesh may need it toggled.
- **Rejected — a separate `drawSky`/no-write verb**: duplicates the mesh path
  and violates "no dedicated draw verbs".
- **Rejected — dynamic depth-bias/polygon-offset control**: Sokol takes bias
  as pipeline state with no dynamic apply, so it would create an open set of
  pipeline variants; it is unnecessary for the skybox and not required to make
  the blob usable (see D5).

### D3 — The skybox is geometry, drawn first with `depthWrite: false`

The recommended usage, exercised by the showcase, is an `inverted` sphere
carrying an `unlit` diffuse-mapped material, transformed to the camera
position and recorded before the scene with `depthWrite: false`:

```
render():
  drawMesh(sky,  { transform: translate(cam.pos), depthWrite: false })  // fills background
  drawMesh(scene geometry)                                              // over the sky
  setCamera3D(...) already set for both; sky follows the camera
```

Because the sky writes no depth, scene geometry still tests against the
cleared far buffer and draws over it regardless of distance, so the sky
radius does not have to exceed the scene extent. The inverted sphere is
camera-locked and rotated with the scene camera, giving rotation parallax
without translation parallax — the classic sky dome.

- **Rejected — a dedicated fullscreen sky pass**: more native surface (new
  shader + pass + API) and does not help decals; the geometry route reuses the
  mesh path and yields two generally useful knobs instead.
- **Rejected — cubemap sky**: postponed per the proposal; a cubemap resource
  and cube sampling would also drag in environment-mapping scope.
- **Rejected — a JS dome built by hacking existing primitives** (negative
  scale to flip winding, or the ambient-map-as-unlit trick): obscure, fragile,
  and it cannot disable depth writing, so it cannot be made correct.

### D4 — `inverted` is a creation option on the existing primitives

Add `inverted?: boolean` (default `false`) to `makeCube` and `makeSphere` in
the pure-JS primitives. Inverted generation reverses each triangle's winding
and negates the per-vertex normals, so the inside surface is front-facing
under the engine's BACK/CCW culling. This is a pure geometry change in
`prelude.js`; no native code or new primitive type is involved.

- **Rejected — new `makeInvertedCube`/`makeInvertedSphere` functions**: two
  near-duplicate members for one boolean, against the naming/minimal-surface
  guidance.
- **Rejected — negative scale transform**: it flips winding through the model
  matrix, which also negates the normal matrix handedness and is easy to get
  subtly wrong; an explicit generation flag is unambiguous.
- **Rejected — a double-sided/cull override on the material**: broader than
  needed, changes a core pipeline invariant, and does not address normals.

### D5 — Blob shadows compose existing billboards with a small lift

The blob is a `facing: 'plane'` billboard (default `normal [0,1,0]`) with a
dark color and `blend: 'alpha'` (`src·sa + dst·(1-sa)`), placed a small
distance above the ground plane so it is not exactly coplanar with it. The
billboard path already depth-tests without writing depth, already accepts a
per-draw blend, and its plane basis is camera-independent, so an orbiting
camera never tilts it. The showcase generates the radial alpha with
`createImageData`, so it ships no third-party asset.

- **Rejected — polygon offset/depth bias for the decal**: the clean GPU fix,
  but it needs pipeline variants (D2) and is not necessary to make the flat
  case correct; recorded as a possible future extension.
- **Rejected — `depthTest: false`**: the shadow would draw over any geometry
  between the camera and the ground in that screen region, not just over the
  ground.
- **Rejected — projected-geometry shadows**: out of scope; the blob is a decal
  and does not conform to sloped or uneven ground (stated as a non-goal).

### D6 — Sky asset is a downscaled CC0 equirectangular JPG

Use Poly Haven's `kloofendal_43d_clear_puresky` (CC0 1.0), tonemapped JPG
rendition, downscaled to 2048×1024 and re-encoded to a committed
`skybox-showcase/sky.jpg`; the source URL, license, and recipe are recorded in
`gallery/samples/curated/CREDITS.md` (which already requires every curated
asset to be CC0). The sphere's existing equirectangular UVs map the image
directly.

- **Rejected — the `.hdr`/`.exr` rendition**: the engine has no HDR/EXR
  decoder and no tonemapper (`loadImage` decodes PNG/JPG only).
- **Rejected — a cubemap cross/atlas**: cubemaps are postponed, and a sphere
  with equirect UVs needs no atlas.
- **Rejected — a CC-BY/attribution-only asset**: the gallery's CREDITS
  contract is CC0; a CC0 source keeps that invariant and needs no attribution
  burden on the site.

### D7 — No new unmanaged resource; all additions are value state

`unlit`, `depthWrite`, and `inverted` introduce no native-backed class, no
handle, and no `destroy()`: `unlit` is snapshotted inside the existing
JS-managed material, `depthWrite` is a boolean snapshotted on the existing
mesh record, and `inverted` only changes the CPU vertex/index arrays a
primitive already builds. The ADR 0011/0012 resource discipline is therefore
untouched — dynamic resources remain GC-finalized opaque classes with explicit
`destroy()`, and the light bank remains the only slot bank.

### D8 — Verification uses a pure-geometry golden plus unit/CPU tests

A new golden scene exercises `unlit`, `inverted`, and `depthWrite: false`
using only engine primitives (no third-party asset), so desktop/web goldens
need no extra packing. Unit tests cover the new field validation, and the
existing CPU lighting reference (`efx_lighting_shade`) gains an unlit mode so
the shader result can be asserted headlessly. The golden is captured on the
SSH server (llvmpipe) before dispatching the four-target gate (ADR 0020/0023).

## Apply notes

- The web `drawBillboard` binding accepted `facing: 'view' | 'y'` only, while
  the desktop binding and the `billboards` spec accept `'plane'` too. The
  blob-shadow showcase (`facing: 'plane'`) surfaced the divergence; the web
  validator now maps `plane` and uses the desktop's canonical message. No ADR —
  this restores documented parity rather than changing behavior.

## Risks / Trade-offs

- **Mesh pipeline variant growth (12)** → contained: pipelines are created
  once at install/rebind and selected by an integer index; the combination is
  the same mechanical product the existing blend × flip matrix uses.
- **Material wire block grows 18 → 19** → the mirrors are explicitly
  enumerated in `src/render/render.h`; the task list updates all of them
  (prelude, web core, both bridge directions) in one commit and the Linux gate
  drift-check (`gen_prelude.py --check`) catches a missed prelude regen.
- **Equirect seam / orientation on the sphere** → the sample orients the sky
  with the mesh transform and accepts the existing equirect UV convention; a
  visible seam is a sample-tuning concern, not an engine behavior, and the
  golden uses a procedural pattern so it is independent of the sky image.
- **`depthWrite: false` on a camera-locked sky hides nothing but relies on
  draw order** → documented as "record the sky first"; recording it after
  opaque geometry still works because it writes no depth, but the sample and
  guidance order it first.
- **Blob z-fighting on user scenes that do not lift the quad** → the showcase
  and any guidance use a small lift; the trade-off (no bias API) is recorded
  in D5 and the proposal's non-goals.
- **Sloped/uneven ground** → the blob is a flat decal by design; not addressed
  here.

## Migration Plan

Additive and default-preserving; no migration. Merging is a normal change:
update `efx.d.ts`, regenerate `docs/api/`, write ADR 0058, land the goldens,
examples, and CREDITS row, then archive. Rollback is reverting the branch; no
data or API compatibility window is needed because no default changes.

## Open Questions

None that affect the specs, approach, or task breakdown. The only deferred
item — polygon-offset/depth-bias for decals — is an explicit non-goal and a
candidate for a future change, not a blocker here.
