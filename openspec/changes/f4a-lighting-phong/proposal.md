# Proposal

**Roadmap position:** Implements the first half of milestone **F4 (lighting +
Phong)** from `openspec/specs/feature-roadmap` — the roadmap's **F4a** slice:
4 point + 1 directional light and a 4-channel Phong material on solids and
vertex colors, no maps yet. The predecessor gate (F3) has passed on all four
targets (run 36122872839: native suites incl. all twelve goldens on
Linux/Windows/macOS, Emscripten ctest + web goldens), so this proposal is in
roadmap order. F4b (per-channel maps + alpha masks) is a later change built on
this one.

## Why

F3 delivered the 3D core but its canned mesh fill is **unlit**: fragment color
is vertex color × tint. The roadmap names lighting and the Phong material
system as the next milestone, and every visual feature after this one assumes
it — F4b adds maps to these channels, F6 imports glTF materials into them, F7
animates meshes that are lit by them, and the F8 high-level layer shades
models. The provisional F4 contract is small and already pinned in
`docs/js-api.md`; the material **binding mechanism** (per-surface, never
global) was settled in F3 (ADR 0024) and only needs to be made real here. The
one deferred architecture question the roadmap assigns to F4 — the canned
shader strategy for lighting — is settled by this change (ADR 0026).

Splitting F4 into F4a/F4b keeps the gate honest: F4a is lighting math over
normals and solid colors, testable against a CPU reference; F4b adds texture
sampling and alpha masking, the part that actually needs shader permutations
and uv plumbing.

## What Changes

- **Point light bank (`setLight(slot, opts)`)** — four fixed slots (0..3),
  each `{ pos, color, range? }`: world-space position, `[r, g, b, a]` color
  (alpha ignored), and an optional attenuation `range` (finite, > 0;
  default `0` = no falloff). `null` disables the slot. Lights are a
  pre-allocated bank (ADR 0011 — the only slot-based resource), configured by
  value-snapshot into each mesh record (ADR 0019).
- **Directional light (`setDirectionalLight(opts)`)** — the single
  directional light, `{ dir, color }`: `dir` is the direction the light
  **travels** (so the direction to the light is `-dir`); `null` disables it.
- **Per-surface Phong materials** — `efx.setMeshSurfaceMaterial(mesh,
  surfaceIndex, mat)` binds a **snapshot** of a JS-managed object to one
  surface (Godot `surface_set_material` analog; the mechanism reserved by
  F3/ADR 0024). Channels and defaults: `ambient` (black), `diffuse` (white),
  `specular` (`{ color: black, shininess: 32 }`), `emissive` (black).
  `null` resets the surface to the engine default material. `createMeshData`
  now accepts the parallel `materials` array (F3 rejected it as an unknown
  field), carried over at `createMesh`. Materials are **JS-managed** — no
  native class, no `destroy()`.
- **Lit mesh rendering** — the F3 unlit mesh shader becomes a **Phong
  shader**: world-space lighting evaluated per fragment from interpolated
  (normalized) normals; ambient + diffuse + specular + emissive; Blinn-Phong
  specular; the surface albedo is `vertex color × tint`, so F3's vertex
  colors and `drawMesh` tint keep working. `uvs` remain validated/stored but
  unused (F4b). Surfaces without normals keep F3's deterministic default
  normal `(0, 0, 1)`. The default material is white diffuse Phong (unbound
  surface), so a scene with no lights renders meshes black — an intended,
  documented change.
- **Canned shader strategy (ADR 0026)** — one lit mesh shader driven by
  uniforms; **no per-material shader permutations** in F4a (maps in F4b may
  introduce the first permutation/branch). Reuses the pinned sokol-shdc
  pipeline (ADR 0021), single GLSL source.
- **CPU lighting reference** — a pure-C reference implementation of the F4a
  Phong equation, exercised by unit tests on analytic cases (head-on diffuse,
  specular peak, attenuation at range, directional light, emissive), so the
  lighting math is verified independently of the GPU (roadmap: F4 verified
  against a CPU reference).
- **Goldens** — the six committed F3 mesh goldens are re-baselined (their
  scenes gain explicit lights/materials so the images stay meaningful); new
  F4a lighting scenes are added. The seven F2 2D goldens MUST stay
  byte-identical.
- **Docs** — `docs/js-api.md` F4a entries move from provisional to current in
  the same change (js-api requirement), and the F4b entries stay provisional;
  AGENTS.md roadmap/status updated.

**Decisions doc:** new ADR `docs/decisions/0026-lighting-and-canned-shader-strategy.md`
— settles the F4 shader strategy deferred by ADR 0015/0021 (single lit shader,
uniform-driven, no F4a permutations) and records the F4a Phong equation,
light semantics (direction convention, attenuation), and the default-material
choice. It is a durable decision future changes (F4b permutations, F5 passes)
must respect.

**Non-goals:** per-channel maps, texture sampling on meshes, and alpha masks
(F4b — `uvs` stay unused by lighting here); render targets/post FX (F5); zip
root, glTF import, REPL (F6 — the glTF material mapping arrives there);
skinning/animation (F7); high-level `drawModel`/`drawText`/demo pack (F8);
shadows, multiple cameras, spot lights, per-draw material overrides
(`drawMesh({ material })`, rejected in ADR 0024), global material state (never
— ADR 0024), gamma correction/tonemapping (not in vision.md), and any change
to F2 2D behavior or goldens.

## Capabilities

### New Capabilities

- `lighting`: the F4a fixed-function lighting model — the 4-slot point light
  bank and single directional light (shape, defaults, validation,
  value-snapshot), the 4-channel Phong material object and its defaults, the
  per-surface binding API and the `materials` creation array, the exact lit
  shading equation (world space, Blinn-Phong, attenuation, albedo = vertex
  color × tint, default normal/material), and the CPU reference contract.

### Modified Capabilities

- `3d-core`: the material binding slot becomes active — `createMeshData`
  accepts a parallel `materials` array (no longer rejected as unknown),
  `setMeshSurfaceMaterial` rebinds a surface, bindings carry over at
  `createMesh`, and `drawMesh`'s per-surface color becomes the F4a lit result
  (the "unlit in F3" clause is superseded).
- `js-api`: the resource-classification requirement gains the Materials
  (Phong parameter objects) as a JS-managed resource type and states the
  lights bank classification explicitly; the API reference's F4a entries move
  from provisional to current.

## Impact

- **Code:** `shaders/mesh.glsl` becomes the lit Phong shader (regenerated
  `shaders/mesh.h`, ADR 0021); `src/platform/pipeline.c` gains the model/
  normal + light/material uniform wiring and per-fragment lighting playback;
  `src/render/` gains the light bank, per-surface material snapshot storage, a
  bigger mesh record (light snapshot), and a pure-C lighting reference module;
  `src/api/api.c` + `src/web/bridge.c` gain `setLight`,
  `setDirectionalLight`, `setMeshSurfaceMaterial`, and the `materials`
  creation array with identical semantics on both bindings (ADR 0022).
- **APIs:** F4a entries in `docs/js-api.md` become current; the F4b entries
  remain provisional and now read as an extension of the F4a material object.
- **Verification:** new F4a golden scenes plus re-baselined F3 mesh goldens
  (new scenes need the manual server-side llvmpipe capture before remote
  verification, `docs/verification-server.md`); new CPU lighting-reference
  unit tests and headless display-list assertions for the enlarged mesh
  record; the F2 2D goldens MUST remain byte-identical. Gate order per
  AGENTS.md: Linux first, then Windows, then macOS; remote pre-verification
  via `tools/verify_remote.py` before dispatching the tag/manual gate
  (ADR 0023).
