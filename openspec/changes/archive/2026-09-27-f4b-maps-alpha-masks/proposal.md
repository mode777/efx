# Proposal

**Roadmap position:** Implements the second half of milestone **F4 (lighting +
Phong)** from `openspec/specs/feature-roadmap` — the roadmap's **F4b** slice:
per-channel maps (Ambient/Diffuse/Specular/Emissive) and alpha masks extending
the F4a Phong material, consuming the mesh `uvs` that have been validated and
stored since F3. The predecessor gate (F4a) has passed on all four targets
(ci run 36271736775: native suites incl. all nineteen goldens on
Linux/Windows/macOS, Emscripten ctest + cross-runtime compare + web goldens),
so this proposal is in roadmap order and is the last change of F4; F5 (render
targets + post FX) follows.

## Why

F4a delivered the fixed light bank and a four-channel Phong material over solid
colors and vertex colors, but `vision.md` asks for **maps for each of those
channels** and **support for alpha masks**. F4a deliberately left `uvs` unused
and rejected `map`/`alphaMask` as unknown fields, and ADR 0026 left the
map/shader question open for exactly this change: F4b is where per-channel
sampling and cutout arrive. Every later visual feature depends on it — F6 maps
glTF materials (including their texture references) into these channels, and
the F8 layer shades imported models. F4b is also the first consumer of the F3
`uvs` attribute, closing the geometry-to-material loop.

## What Changes

- **Per-channel maps** — each Phong channel gains an optional `map` (a live
  `Texture`): `ambient.map`, `diffuse.map`, `specular.map`, `emissive.map`.
  A present map modulates that channel's color by the sampled
  `texture(map, uv).rgb`, per fragment, using the surface's interpolated `uv`.
  The channel equation becomes `channel.color × channel.map.rgb` (map absent
  ⇒ factor `1`). Specular keeps its uniform `shininess`. Channel alphas stay
  ignored by shading; maps affect RGB only.
- **Alpha mask** — the material gains an optional material-level
  `alphaMask` (a live `Texture`). When present, the fragment samples the mask
  alpha and is **discarded when `alpha < 0.5`** (binary cutout; mask RGB
  ignored). Surviving fragments keep the albedo alpha (vertex-color alpha ×
  tint alpha) — the mask does not modulate or add transparency. This is the
  classic PS2-era cutout (foliage, grates).
- **Single-shader realization (upholds ADR 0026)** — the F4a uniform-driven
  mesh shader (`shaders/mesh.glsl`) is extended in place: four channel-map
  samplers plus one alpha-mask sampler, always declared and bound; an absent
  color map binds the engine `whiteTexture` so sampling yields `(1,1,1,1)` and
  the multiply is a no-op; a uniform flag gates the mask discard. **No
  per-material shader permutations** — the alternative the roadmap left open
  is settled against permutations (see the ADR).
- **Map texture lifetime** — a bound map keeps its `Texture` alive: the engine
  retains the native storage while the texture stays bound to any surface
  (`Texture.destroy()` releases the script's handle; the native release is
  deferred until no material binding references it — rebind, `null`, or mesh
  destroy). This generalizes the display-list keep-alive rule (ADR 0019) to
  persistent material bindings and prevents a dangling map handle.
- **Material validation and snapshot** — `map`/`alphaMask` were rejected as
  unknown fields in F4a and are now accepted. Each must be a live `Texture`
  (`TypeError` otherwise); the engine snapshots the channel colors as before
  and holds the texture **handles** (ADR 0019 handle-reference), so later
  mutation of the script object cannot change the binding but the retained
  textures stay valid.
- **UV consumption** — `uvs` now affect mesh shading (only when a map is
  bound). A surface without `uvs` samples at the F3 default `(0, 0)`, so
  behavior stays defined for every F3-legal mesh.
- **CPU lighting reference** — the pure-C reference is extended to take
  per-channel map samples (RGB) and the mask alpha as inputs, so map
  modulation and the cutout threshold are unit-tested headlessly against the
  same equation the shader transcribes.
- **Goldens** — new F4b scenes (diffuse map, ambient map, specular map,
  emissive map, alpha-mask cutout, multi-map) at 640×480. Because unbound maps
  default to white, **every committed F3/F4a golden and every F2 2D golden
  MUST stay byte-identical** — no re-baseline.
- **Docs** — `docs/js-api.md` F4b entries move from provisional to current in
  the same change; AGENTS.md status updated; the browser gallery cycles the
  new scenes.

**Decisions doc:** new ADR
`docs/decisions/0027-mesh-material-maps-and-alpha-mask.md` — settles how
per-channel maps are realized (single uniform-driven shader, four channel
samplers + mask sampler, white-texture fallback, no permutations; upholds
ADR 0026), the binary `alpha < 0.5` cutout, and the retained-texture lifetime
rule for bound maps. It is a durable decision F5/F6 must respect (F6 binds
imported glTF textures into these channels).

**Non-goals:** render targets and post FX (F5); zip root, glTF import, and the
REPL (F6 — the glTF texture→channel mapping arrives there); skinning and
animation (F7); high-level `drawModel`/`drawText` and the demo pack (F8);
`uv` transforms/scaling/offset, texture wrap/filter controls, mipmaps,
anisotropy, and texture arrays (none are in `vision.md`); soft/graded alpha or
alpha blending driven by maps (only the binary mask ships); normal maps, light
maps, cube maps, reflections, shadows, and any consumer-visible shader surface
(ADR 0015) — the shader stays engine-owned and fixed-function in spirit.

## Capabilities

### New Capabilities

- None — F4b extends the F4a `lighting` capability rather than introducing a
  new one.

### Modified Capabilities

- `lighting`: the Phong material object gains a per-channel `map` and a
  material-level `alphaMask` (live `Texture` references, retained while
  bound); the lit shading equation is modulated by each channel's map sampled
  at the interpolated `uv`; a present `alphaMask` discards fragments whose
  alpha is below `0.5`; the CPU reference is extended to accept map samples
  and mask alpha.
- `3d-core`: `drawMesh`'s per-surface shading is now the `lighting`
  capability's F4 lit result **including per-channel maps and alpha masks**,
  and surface `uvs` (validated/stored since F3) are consumed by that shading.
- `js-api`: the resource-classification requirement is clarified — a
  JS-managed material may reference native-backed `Texture` objects through
  its maps; the engine snapshots the handles and retains the referenced
  textures while bound, so materials remain JS-managed with no `destroy()`
  and the classification is unchanged.

## Impact

- **Code:** `shaders/mesh.glsl` gains the map/mask samplers and modulation
  (regenerated `shaders/mesh.h`, ADR 0021); `src/platform/pipeline.c` binds
  the five textures per surface (white fallback), sets the mask flag, and
  enables the discard; `src/render/` extends `efx_material` with map handles
  and an alpha-mask handle, adds texture retention for bound maps, and extends
  the CPU reference; `src/api/api.c` + `src/web/bridge.c` parse and validate
  `map`/`alphaMask` with identical semantics on both bindings (ADR 0022).
- **APIs:** `docs/js-api.md` F4b entries become current (the F4a material
  object is extended additively); no new functions, so the F4b block mostly
  moves from provisional to current.
- **Dependencies:** none new — maps reuse the F2 `Texture`/`ImageData` path
  and the pinned sokol-shdc pipeline; no vendored library is added, so the
  roadmap's dependency-evaluation requirement does not trigger.
- **Verification:** new F4b golden scenes (need the manual server-side
  llvmpipe capture before remote verification, `docs/verification-server.md`);
  extended CPU lighting-reference unit tests over map modulation and the cutout
  threshold; headless assertions for material validation and texture
  retention. F2 and F3/F4a goldens MUST stay byte-identical. Gate order per
  AGENTS.md: Linux first, then Windows, then macOS, with
  `tools/verify_remote.py` pre-verification before dispatching the gate
  (ADR 0023).
