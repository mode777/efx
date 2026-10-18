# Proposal

## Why

Games need atmospheric depth — distance haze, fog-of-war fades, skybox-blending
horizons. The renderer currently has no way to attenuate geometry toward a
color with distance, so foggy scenes must be faked with per-vertex colors or
post tints, neither of which responds to camera depth. Fog is pure value state
(no resources, no memory-management surface) and slots into the existing
value-snapshot state family (lights, camera, clear color), making it a
low-risk, high-leverage addition.

## What Changes

- New script API `efx.graphics.setFog(opts | null)` configuring a single fog
  state: `color` (required), `mode` (`'linear'` default, `'exp'`, `'exp2'`),
  `near`/`far` (linear mode), `density` (exp modes); `null` disables fog
  (the startup default).
- Fog applies to **mesh draws only** (lit Phong path, including skinned meshes
  and heightfields via the mesh path). Billboards, particles, and 2D quads are
  unaffected in this change.
- Fog is **distance-based**, computed in the canned mesh fragment shader from
  the fragment's view distance; output is `mix(litColor, fogColor, factor)`
  applied after lighting. **Unlit materials are unaffected by fog** — the unlit
  early-out stays fog-free, which keeps unlit-rendered skyboxes crisp above a
  fogged scene.
- Fog state is a plain value snapshot: each mesh record snapshots the fog set
  active at record time (ADR 0019), exactly like the light-set snapshot.
- New CPU reference fog math beside `efx_lighting_shade` for unit-test parity.
- Fog state clears on `efx_render_reset_state()` (game swap, ADR 0057).
- Docs guidance: fog color is distinct from the clear color; foggy scenes
  should set `setClearColor` to match `setFog` color (dumb and documented —
  no auto-tinting magic).

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `lighting`: adds a `### Requirement: Fog` requirement — the `setFog` API
  contract, the fog models and their math, record-time snapshot semantics,
  the meshes-only scope, and the unlit exemption. Fog lives with lighting
  because it modifies the lit shading result and follows the same
  value-snapshot + CPU-reference patterns.

## Impact

- `src/render/render.h` — `efx_fog` struct, `efx_mesh_record` grows the fog
  snapshot, set/query API.
- `src/render/render_records.c` (state + record snapshot), `src/api/api_lighting.c`
  + `src/api/api.c`/`api.h` (wire native), `src/prelude/prelude.js` (+ regenerate
  `prelude.h`).
- `src/platform/pipeline.c` + `shaders/mesh.glsl` (regenerate `shaders/mesh.h`)
  — two new `fs_params` vec4s and the fog factor in the lit path only.
- Web bridge: `src/web/bridge_render3d.c` + a `src/web/js/*.js` natives entry.
- `gallery/src/api/efx.d.ts` → regenerate `docs/api/`; gallery type-test.
- Tests: unit tests (validation, snapshot, CPU-reference parity), one or two
  golden scenes (linear + exp2).
- Docs: `docs/js-api.md` unaffected in guidelines (option-bag conventions
  already covered); fog/clear-color guidance goes in the generated reference
  doc text via the `.d.ts` comments. No ADR — fog is a value-state feature
  following the existing ADR 0019 snapshot and ADR 0015 canned-shader rules;
  no durable architecture decision is being settled.

## Non-goals

- Fog on billboards, particles, or 2D quads (the quad shader has no view
  distance; CPU-baked billboard fog is a possible follow-up, not designed here).
- Fog on unlit materials (deliberate: skyboxes render unlit and must stay
  unfogged).
- Depth-buffer fog, height-based fog, volumetrics, custom fog curves, fog
  noise — fixed-function only.
- Auto-tinting the clear color from fog state.
- A `fog` query API beyond the state getter needed for tests (no per-record
  introspection script-visible).
