# Design

## Context

See `proposal.md` — Why. Current implementation facts that shape the approach:

- `setBlendMode` writes a single engine field, `R.blend`
  (`src/render/render_internal.h`), which is copied into each display-list
  record at record time (`render_records.c`): `quad.blend`, `mesh.blend`,
  `billboard.blend`. `drawParticles` is the exception — it uses the system's
  own `cfg.blend` (`render_particles.c`).
- Playback selects one of three pre-built Sokol pipelines per draw type
  (`P.quad_pip[blend]`, `P.bill_pip[blend]`, `P.mesh_pip[blend]`,
  `P.mesh_pip_cw[blend]`), indexed by the record's blend byte
  (`src/platform/pipeline.c`). The mesh pipeline is applied once per mesh
  record, before the surface loop.
- Quad batching groups consecutive quad records by `texture + blend`
  (`efx_render_draw_runs`); a blend change breaks a run.
- `efx_render_begin_frame` rewinds records but does not touch `R.blend`;
  `efx_render_reset_state` does reset it to alpha. It is called per frame from
  `platform.c` (windowed) and `web/bridge_core.c`; headless `--script` mode has
  no frame loop.
- ADR 0019 permits exactly one declared late-binding (the posed vertex buffer)
  and requires small state to be value-snapshotted at record time.

## Goals / Non-Goals

**Goals:**

- One coherent rule: blend is render state with a frame-local default and
  explicit per-object overrides.
- Per-surface 3D blend control through the JS-managed material object.
- Preserve the fixed-function pipeline: no new shaders, no consumer-visible
  shader state, no new blend equations.
- Honor ADR 0019: everything resolved and snapshotted at record time.

**Non-Goals:**

- Removing `setBlendMode` or changing clear-color/camera/light persistence.
- Per-sprite blend (blend stays batch-level for `drawSprites`).
- New blend modes or consumer shaders.

## Decisions

### D1 — Keep `setBlendMode` as the render-state default; add per-object overrides

The setter stays the single default for every draw record type, with overrides
layered on top. **Alternative rejected:** delete the setter and require `blend`
on every draw bag. It is broad churn across five capability specs, two
runtimes, and the prelude; it complicates sprite batch semantics (blend is a
batch property, not per-sprite); and it buys no performance — the record
already carries one blend byte either way.

### D2 — The state is frame-local; it resets to `alpha` at frame start

`efx_render_begin_frame` sets `R.blend = EFX_BLEND_ALPHA` alongside the record
rewind, so a mode set at load time cannot leak into later frames.
**Alternative rejected:** keep cross-frame persistence and only document it.
That preserves the current footgun the change exists to remove. Consequence:
this is a **breaking** behavior change for scripts that set a mode once at
init — see the migration plan.

### D3 — Override placement: 2D in draw bags, 3D in `Material`, particles inherit

- 2D quads: `DrawQuadOptions.blend?`.
- 2D sprite batch: a new trailing `drawSprites(texture, sprites, opts?)` bag
  with a batch-level `blend?`. **Alternative rejected:** per-entry `blend`.
  A batch is one texture + blend; mixed per-sprite blends shatter batching and
  add validation cost on the hot path.
- 3D meshes: `Material.blend?` per surface. **Alternative rejected:**
  `DrawMeshOptions.blend?`. A mesh record already draws each surface under its
  own material, so the material is the natural per-object location and lets
  one mesh mix an additive emissive surface with an alpha surface. A
  whole-mesh draw-level `blend` would have to override every surface and
  cannot express that mix.
- Billboards: `DrawBillboardOptions.blend?` (no material concept).
- Particles: keep the existing system `blend`, but make it optional; omitted
  or `null` means inherit the frame state at draw time. **Alternative
  rejected:** leave particles fully independent and only document it. That
  keeps two silent mechanisms and contradicts the unified rule.

### D4 — Resolve per-surface mesh blend at record time, not playback

`efx_material` gains a `blend` field with a sentinel (`-1`) meaning "no
override". `efx_render_mesh` resolves each surface's blend — the bound
material's value when set, else `R.blend` — and stores the resolved bytes in
the mesh record (`uint8_t surface_blend[16]`; 16 is the fixed surface cap).
Playback applies the pipeline per surface, switching only when the blend
changes. **Alternative rejected:** read `Material.blend` from the mesh at
playback. That is a second declared late-binding, which ADR 0019 forbids; a
`setMeshSurfaceMaterial` between record and playback would change a recorded
draw.

### D5 — `blend` sentinel and validation live in the shared prelude

Option-bag validation follows ADR 0049: the prelude (`src/prelude/prelude.js`)
validates `blend` as one of the three strings (`null` accepted only where
documented, i.e. particle config) and passes a normalized integer to the
marshal-only native, which uses `-1` for "inherit / frame state". Regenerate
`prelude.h` after the edit.

### D6 — Resource model unchanged

No new resource type is exposed. `Material` remains a JS-managed plain object
(no native handle, no `destroy()`); its `blend` is part of the value snapshot
taken at binding. `ParticleSystem` remains a native-backed GC-finalized class
with explicit `destroy()`, unchanged by this design. There is no new fixed
bank.

## Risks / Trade-offs

- **Breaking sample/golden behavior from the frame reset** → update the two
  curated samples that set blend at init (`input-playground`,
  `particles-showcase`) and extend the `blend` golden to cover frame reset and
  an override; re-verify on the SSH server before the gate (ADR 0020/0023).
- **Per-surface pipeline switching can cost uniform re-application** → apply
  the mesh pipeline only when the surface blend differs from the previous
  surface; meshes with uniform or no overrides pay nothing (the common case).
- **Record growth** → +16 bytes on the mesh record (one byte per fixed
  surface). Negligible against the 16 MiB record budget.
- **`--script` mode has no frame begin** → the frame reset does not apply
  there; the state persists for the whole script run, matching "per frame"
  semantics vacuously. Recorded output is unaffected.
- **Two spellings of "no blend" (`null` in particle config, absent elsewhere)**
  → document the one case that accepts `null` (particle `blend`) and keep every
  other bag to the three strings only.

## Migration Plan

1. Land the code + prelude + type doc + generated reference together; the
   `js-api` drift guards (`gen_prelude.py --check`, `docs:check`) fail on a
   partial change.
2. Update the two curated samples: call `setBlendMode('additive')` inside the
   render hook, or pass `blend: 'additive'` on the affected draws.
3. Update the `blend` golden to assert the frame reset and one per-object
   override; add unit assertions for per-surface material blend and particle
   inheritance.
4. Rollback: the change is contained to the blend state path; reverting the
   commit restores cross-frame persistence and the alpha particle default.
