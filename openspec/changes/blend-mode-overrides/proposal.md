# Proposal

## Why

Blending is the one draw attribute a script can only set globally: `setBlendMode`
governs 2D quads and sprites, 3D meshes, and billboards alike, while particle
systems already carry their own `blend` option — two mechanisms coexist
silently. The global also persists across frames, so a mode set once at init
leaks into every later draw, and there is no way to give a single material or
sprite batch a different blend. This change makes blend a first-class render
state with a **frame-local default** plus explicit **per-object overrides**, and
reconciles particles onto the same rule.

## What Changes

- **`setBlendMode(mode)` becomes the frame-local render-state default.** It still
  selects `'alpha'` (startup default), `'additive'`, or `'subtractive'`, but the
  engine resets it to `'alpha'` at the start of every frame. **BREAKING** —
  scripts that set a mode once at load time must set it inside the render hook or
  pass a per-object override.
- **Per-object overrides** (all value-snapshotted at record time, ADR 0019):
  - `DrawQuadOptions.blend?` — 2D quads.
  - `drawSprites(texture, sprites, opts?)` gains a trailing options bag with a
    batch-level `blend?`; the whole batch shares it (no per-sprite blend).
  - `Material.blend?` — per mesh surface in 3D; surfaces whose material does not
    specify one use the recorded frame default.
  - `DrawBillboardOptions.blend?` — 3D billboards.
- **Particles reconciled.** `createParticleSystem`'s `blend` and the system's
  `set({ blend })` become optional: omitting it (or passing `null`) means the
  system inherits the frame default at `drawParticles` record time instead of
  hardcoding `'alpha'`.
- **Docs and samples:** update `gallery/src/api/efx.d.ts`, regenerate
  `docs/api/`, update `docs/js-api.md`, and move the two curated samples that
  set blend at init into their render hooks / per-object options.
- **New ADR** `docs/decisions/0054-blend-mode-state-and-overrides.md` recording
  blend-as-render-state with frame-local default and per-object overrides, plus
  its index row.

## Capabilities

### New Capabilities

None — this refines existing drawing capabilities.

### Modified Capabilities

- `2d-layer`: the **Blending modes** requirement is reworked (frame-local
  default, applicability across draw record types, per-object override
  precedence); **Quad drawing** gains `blend`; **Batched 2D sprite drawing**
  gains a trailing options bag with a batch-level `blend`.
- `lighting`: the **Phong material model** gains an optional `blend` field.
- `3d-core`: **Whole-mesh drawing with depth** resolves each surface's blend
  (its material's override, else the recorded frame default) at record time.
- `billboards`: **World-space billboard drawing** gains `blend`; **Billboard
  depth and blend behavior** is updated for the override.
- `particles`: **Particle system creation and configuration** and **Particle
  rendering and batching** treat `blend` as optional and inherit the frame
  default when unset.

## Impact

- **Code:** `src/api/api_2d.c`, `src/api/api_particles.c`,
  `src/render/render_records.c`, `src/render/render_internal.h`,
  `src/render/render_particles.c`, `src/platform/pipeline.c`,
  `src/prelude/prelude.js` (+ regenerated `prelude.h`), the web bridge and
  `src/web/js/*`, and runtime registration.
- **Docs:** `gallery/src/api/efx.d.ts` (source of truth), generated `docs/api/`,
  `docs/js-api.md` (blend design rule), and `docs/decisions/` (new ADR 0054 +
  index row).
- **Samples/tests:** `gallery/samples/curated/input-playground` and
  `particles-showcase`; the `tests/goldens/blend` scene extended; unit tests
  (`tests/unit/api_tests.c`), the portable script tests and error catalog
  (`tests/scripts/s_2d_validation.js`, `s_error_catalog.js`).
- **ADR:** needed — `docs/decisions/0054-blend-mode-state-and-overrides.md`
  (references and does not supersede ADR 0019; blend overrides stay
  value-snapshotted).
- **Non-goals:** no new blend equations or modes (no multiply/screen); no
  consumer-visible shaders; no per-sprite blend (batch-level only); no change to
  clear-color/camera/light persistence or to the fixed 3-blend pipeline set.
