# Design

## Context

Playback walks the display list in record order (ADR 0019). Quads are already
coalesced: `efx_render_runs` (render_records.c) builds a run plan over
consecutive same-texture/same-blend quad records, `emit_quad_runs` writes one
continuous triangle strip per run (degenerate bridge vertices between quads
via `emit_quad(v, q, flip, q > 0)`), and `play_records` draws one strip per
run. Billboard records have no run plan: `play_records` calls `draw_textured`
per record, and `emit_billboards_and_particles` writes each billboard as an
isolated 4-vertex strip. The recorded `billboard.depth_test` flag
(render.h:190) is never read at playback — all billboards draw through
`bill_pip` (depth `LESS_EQUAL`, no write), and no test covers
`depthTest: false`. `quad_pip` is byte-for-byte the same pipeline except
depth compare (`ALWAYS`, no write) — same shader, layout, blend table, strip
primitive, cull mode. The mesh fragment shader (`shaders/mesh.glsl`) samples
all five material maps before the `unlit` early-out.

Constraints that shape the design: sokol pipelines are immutable (state
variation = pipeline objects); the display-list contract allows reordering
only where output is provably identical (2d-layer display-list requirement);
shaders stay single-source uniform-driven (ADR 0026/0027); committed
`shaders/mesh.h` must be regenerated with the pinned sokol-shdc (ADR 0021,
`EFX_SHDC` CMake cache path); golden gates run on all four targets (ADR 0020).

## Goals / Non-Goals

**Goals:**

- One draw + one pipeline/binding apply per consecutive same-state billboard
  group, with output pixel-identical to per-record playback.
- `depthTest: false` actually bypasses depth testing, and gains its first
  test coverage.
- Unlit fragments stop paying the three color-map fetches they ignore.

**Non-Goals:**

- No reordering beyond record-adjacent grouping (no sort-key machinery).
- No instancing, no index-buffer reuse across billboards, no vertex-format
  changes, no particle-path changes.
- No shader permutations of any kind.

## Decisions

**D1 — Separate billboard run plan, not a unified plan.** Add
`efx_billboard_run { start, count, texture, blend, depth_test }` and
`efx_render_billboard_runs()` in render_records.c, structurally mirroring
`efx_render_runs`. `play_records` consumes it at billboard records exactly
the way it consumes quad runs (draw at the run's first record, skip ahead).
The existing `efx_draw_run` and quad path stay untouched.

- *Alternative: one run array with a kind discriminator.* Rejected — every
  consumer pays a branch for a distinction the playback loop already makes
  per record type; two small plans beat one coupled one.

**D2 — Run key is (texture, blend, depth_test), adjacency strict.** A run is
a maximal sequence of record-adjacent `EFX_RECORD_BILLBOARD` records with
equal texture, resolved blend, and depth-test setting. Any intervening record
of any kind — including `BEGIN_TARGET`/`END_TARGET` and particles — breaks
adjacency, which automatically keeps runs inside one target segment and keeps
emitted vertices contiguous (billboard/particle vertices are written in
record order; a particle record between billboards inserts its own vertices).

- *Accepted conservatism:* two billboards separated only by quad records are
  vertex-contiguous in the buffer (quads are emitted to a separate region
  first) but still split into two runs. The single-pass record-order
  emission stays simple; the lost merges are rare.

**D3 — Bridge vertices in the billboard emitter, not per-record strips.**
`emit_billboard` gains a `bridge` parameter (the `q > 0` pattern from
`emit_quad`): when set, it duplicates the previous billboard's last vertex
and the new billboard's first vertex so a run renders as one triangle strip
(4 verts for the first billboard, 6 for each subsequent one — same math as
quad runs and `append_strip_quad`). Scratch reservation switches to per-run
(`4 + (n-1)·6`). Per-record `boff/bcnt` entries remain for particles only.

- *Alternative: draw each record's 4 verts with `sg_draw(0, 4, 4)` instances
  from one binding.* Rejected — adds an instanced-draw pathway to a
  strip-based path whose only consumer benefit is already achieved by the
  bridge pattern; Sokol instancing would also need a pipeline change.

**D4 — `depthTest: false` reuses `quad_pip`, no new pipelines.** The
depth-always billboard pipeline would be identical to `quad_pip[blend]`
(compare `ALWAYS`, no write, same shader/layout/blend), so playback selects
`depth_test ? bill_pip[blend] : quad_pip[blend]`. Zero new GPU objects; the
run key carries `depth_test` so a run never mixes the two states.

- *Alternative: grow `bill_pip` to `[blend][depth_test]` (6 objects).*
  Rejected — duplicate GPU state for no behavioral difference; a comment at
  the selection site records the coupling so a future divergence of the quad
  pipeline's depth state revisits it.

**D5 — Unlit fetch reorder inside the single canned shader.** Reorder
`mesh_fs` to: sample alpha mask → discard → sample diffuse → `unlit`
early-out (existing math) → sample ambient/specular/emissive → lighting.
Output is bit-identical: the unlit path consumes only mask + diffuse today;
the lit path consumes all five. The mask stays ahead of the branch so unlit
surfaces keep binary cutout behavior (per the lighting spec). Regenerate
`shaders/mesh.h` with the pinned sokol-shdc; the CPU lighting reference
(ADR 0027) models results, not fetch order, so it is untouched.

- *Alternative: sokol-shdc `@option` specialization for map presence.*
  Rejected again — ADR 0026/0027 both considered and passed on permutations;
  the fetch reorder captures most of the unlit win at zero selection cost.

## Risks / Trade-offs

- [Bridge-vertex bug would visibly corrupt batched billboards] → the bridge
  pattern is copied from two proven sites (`emit_quad`, `append_strip_quad`);
  existing billboard goldens must stay pixel-identical, which exercises
  exactly this path.
- [`depthTest: false` goldens don't exist] → add a dedicated golden scene;
  first capture needs the server-side llvmpipe recipe
  (docs/verification-server.md: capture without the PNG, then re-capture).
- [shader regen drift on contributor machines without sokol-shdc] → same
  situation as every shader edit since ADR 0021; the Linux gate's
  golden comparison fails loudly on a stale committed header.
- [Uniform-load count is unchanged per surface for meshes] → out of scope by
  design; this change targets billboard draw count and unlit fetches only.

## Migration Plan

Single commit series on one branch, no data or API migration. Order:
run plan + playback batching (goldens prove output-identical) → depth-test
selection + its new golden → shader reorder + header regen (goldens prove
bit-identical). Rollback is a normal revert; nothing persists.

## Open Questions

(none)
