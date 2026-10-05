# 0054 — Blend is frame-local render state with per-object overrides

Status: Accepted (2026-10, change `blend-mode-overrides`)

Supports: vision.md — additive/subtractive blending; ADR 0019 (value-snapshot
small state at record time); ADR 0015 (fixed-function pipeline, engine-owned
canned shaders).

## Context

`setBlendMode` was the only way to select blending, and it covered 2D quads and
sprites, 3D meshes, and billboards, while `createParticleSystem` already carried
its own `blend` — two mechanisms coexisting silently. The state also persisted
across frames, so a mode set once at load time leaked into every later draw, and
there was no way to give a single material or sprite batch a different mode.
Blending is rasterizer state (one of three pre-built Sokol pipelines per draw
type), not a Phong surface parameter or a texture property.

## Decision

Blend is render state with a **frame-local default** and **per-object
overrides**, all value-snapshotted at record time (ADR 0019):

- `efx.graphics.setBlendMode(mode)` sets the blend render state, which the
  engine resets to `'alpha'` in `efx_render_begin_frame`. It applies to draws
  that carry no override.
- Overrides: `DrawQuadOptions.blend`, `DrawSpritesOptions.blend` (batch-level,
  no per-sprite blend), `Material.blend` (per mesh surface), and
  `DrawBillboardOptions.blend`.
- A particle system's `blend` is optional; omitted or `null` means it inherits
  the frame state at `drawParticles` record time.
- `efx_render_mesh` resolves each surface's blend at record time into
  `efx_mesh_record.surface_blend[]` (material override else frame state); mesh
  playback applies the pipeline per surface, switching only when it changes.
  The material snapshot's `blend` field uses `EFX_BLEND_INHERIT` (`-1`) for
  "no override". The material wire block grows from 17 to 18 floats to carry it.

## Consequences

- A script must set a non-alpha mode inside the render hook or pass an override;
  setting it once at load time no longer affects later frames (a deliberate
  breaking change).
- Per-surface 3D blending is possible without splitting a mesh draw; mixed-blend
  meshes pay only when the pipeline actually changes (uniforms re-applied on the
  switch).
- The override path is uniform across 2D and 3D and lives where the object is
  described (draw bag or material), while the setter remains the one default.
- Adding a fourth blend equation would touch `pipeline.c`'s three pipeline sets
  and the wire encodings, not just this decision.

## Rejected alternatives

- **Remove `setBlendMode`; require `blend` on every draw bag.** Broad churn
  across five capability specs, both runtimes, and the prelude; it complicates
  sprite batch semantics and buys no performance — the record carries one blend
  byte either way.
- **Keep cross-frame persistence and only document it.** Preserves the leak the
  change exists to remove.
- **Per-entry `blend` on sprites.** A batch is one texture + blend; mixed
  per-sprite blends shatter batching and add hot-path validation cost.
- **`DrawMeshOptions.blend` instead of `Material.blend`.** A whole-mesh override
  cannot express one additive emissive surface beside an alpha surface on the
  same mesh.
- **Read `Material.blend` at playback.** A second declared late-binding, which
  ADR 0019 forbids; a rebind between record and playback would change a recorded
  draw.
