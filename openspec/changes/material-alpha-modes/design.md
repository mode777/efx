# Design

## Context

The renderer is fixed-function over a re-orderable display list (ADR 0015,
0019): records play in submission order (`sort_key` = record index), with a
pre-built pipeline matrix per draw type — meshes select `[blend][depth_write]`
(`pipeline.c` `mesh_pip`, 12 variants after ADR 0058). Fragment alpha on the
mesh path is `vertex_color.a × tint.a` (`shaders/mesh.glsl`); the material's
`diffuse.a` is uploaded but never read. `alphaMask` is an unconditional binary
`< 0.5` discard (ADR 0027). Blend equations (`alpha`/`additive`/`subtractive`,
ADR 0054) resolve per surface at record time; only world-space particles sort,
and only within one batch. There is no pass separation and no record-level
sorting, so translucent meshes blend in place while writing depth.

Exploration decisions (conversation of 2026-10-17): stance B (explicit
`alphaMode`, no heuristics), record-origin sort key, fixed 0.5 cutoff, routing
independent of blend equations, golden fallout accepted where it makes the
breaking class explicit.

## Goals / Non-Goals

**Goals:**
- An exact, heuristic-free transparent/opaque split driven by one declared
  material field.
- Correct compositing: transparent surfaces last, back-to-front by origin,
  never writing depth.
- glTF `alphaMode` fidelity (OPAQUE/MASK/BLEND) on import.
- Zero growth in shader uniform layout; zero new pipelines.

**Non-Goals:**
- Per-surface-centroid or per-triangle sorting; configurable `alphaCutoff`;
  premultiplied alpha or new blend equations; routing for quads/billboards/
  particles; order-independent transparency; any shader-visible
  customizability.

## Decisions

### D1 — Explicit `alphaMode` routing, exact classification (stance B)
A surface is transparent **iff** its bound material's `alphaMode == 'blend'`,
read at playback via the existing per-surface material snapshot. No vertex
scanning, no alpha-threshold guessing, no blend-equation involvement.

- *Alternatives*: derived classification (zero API but heuristic; a single
  0.99 vertex silently reorders a draw) and hybrid auto-detect (superset of
  both costs). Rejected: explicit beats implicit, and the classification must
  be cheap and exact at playback. The cost — translucent scripts must declare
  — is the point (proposal's BREAKING note).

### D2 — Output-alpha saturation outside `'blend'`
`'opaque'`/`'mask'` force output alpha to 1 (glTF semantics). Implemented as
one `mat_params` component (free slot in the existing `vec4`; uniform block
layout unchanged): the shader computes
`alpha = mode == blend ? clamp(diffuse.a * vcolor.a * tint.a) : 1.0`, and the
mask discard becomes conditional on mode (`'blend'` never discards; `'opaque'`
and `'mask'` discard when the mask map is bound — the white fallback keeps
absent masks neutral). CPU reference `efx_lighting_shade` mirrors this in
lockstep.

- *Alternatives*: not saturating (today's alpha path preserved) keeps a
  z-writing in-place blend footgun as the default and makes "opaque" a lie;
  rejected. Saturation is also what makes D1's classification exact — no
  hidden alpha source can make an opaque-routed draw translucent.
- *Compat*: the one visible change is tint/vertex-alpha fades on default-mode
  materials; they must declare `'blend'`. Accepted and called out in the
  proposal.

### D3 — `diffuse.a` joins the alpha product only in `'blend'` mode
The natural material-level opacity source, already serialized (4-float
diffuse in the 19-float wire block) and already uploaded — wiring it costs no
struct or wire growth beyond the mode field itself. Honoring it outside
`'blend'` would contradict D2.

### D4 — Pass split at playback, per pass segment
`play_records` gains a per-segment side list: the main sweep plays records in
order but mesh records skip `'blend'` surfaces and push `(record, surface)`
entries; after the sweep, the side list is sorted and replayed. The side list
resets at `BEGIN_TARGET`/`END_TARGET` boundaries (existing segmentation).
Replay reuses `play_mesh_record`'s per-surface logic, pinned to the
`[blend][0]` pipeline variants that already exist (ADR 0058 "test-only" row) —
`depth_write` is forced 0 regardless of the record's option, per spec.

- *Alternatives*: splitting at record time (two records per draw) doubles
  records and breaks the "drawMesh records one draw" contract; sorting whole
  records (not surfaces) misroutes mixed-mode meshes. Rejected both.
- Sort key: record origin = `record.transform` translation (elements 12–14)
  through the recorded camera's view matrix; sort descending view-space z
  (farthest first), stable with record sequence as tiebreak (sort by composite
  key `(depth, seq)` — `qsort` is not stable). The ADR 0025 depth remap is
  monotonic, so the key is computed pre-projection; the remap stays where it
  is.

### D5 — Blend equations never route (easiest correct answer)
Frame `setBlendMode('additive')` and material `blend` overrides select only
the equation inside whichever pass `alphaMode` chose. Additive on the opaque
path behaves exactly as today (no golden fallout from this axis); additive
plus `'blend'` gets the sorted, no-z-write pass, which is order-insensitive
for additive anyway.

- *Alternative*: auto-routing additive/subtractive to the transparent pass —
  heuristics again, plus behavior change on every existing additive scene.
  Rejected.

### D6 — Wire growth 19 → 20 floats, the five-mirror lockstep
`efx_material` gains `int alpha_mode` (`EFX_ALPHA_OPAQUE 0 / MASK 1 / BLEND 2`,
default OPAQUE). The block grows one float after `unlit`, touching exactly the
mirrors listed in `render.h`'s lockstep comment: `prelude.js`
`__efxMaterialWire`, web `core.js` `__efxMaterial`, `wire_mat_from_block`,
`bridge_mat_from_wire`, and the web `createMeshData` stride — plus
`tools/gen_prelude.py` regeneration (the Linux gate fails on drift) and the
web map-vector count check (mode rides the float block, not the 5-double map
vector). Validation: `efx_api_read_material` accepts `'opaque'|'mask'|'blend'`
and `null` (→ opaque), throws `TypeError` otherwise, written in the desktop
validator; the shared prelude layer passes it through marshal-only (ADR 0049:
cold-path option validation stays native).

### D7 — glTF mapping with documented cutoff approximation
`OPAQUE`/absent → `'opaque'`, `MASK` → `'mask'` + alpha mask binding (as
today), `BLEND` → `'blend'` + `baseColorFactor.a` → `diffuse.a` (clamped to
[0,1]; currently the importer drops or clamps channel alpha — the diffuse
slot already carries 4 floats). glTF `alphaCutoff ≠ 0.5` is approximated by
the fixed 0.5; noted in the spec delta and the importer comment.

### D8 — Goldens: two new scenes, targeted fallout check
New scenes: `material_alpha_modes` (opaque + mask + blend surfaces, including
a blend box straddling an opaque one — proves deferral and no-z-write — and a
far/near blend pair recorded out of order — proves origin sorting) and
`vertex_alpha_blend` (vertex-color alpha gradient over a checker — the
vertex-transparency contract). Fallout check on `blend`, `vertcolor3d`,
`map_alpha_mask`, glTF goldens; re-baseline only if alpha < 1 is actually in
play (greps say no, so expected none). Web compare (`run_web_compare.mjs`)
must match — one shared shader source, so no divergence is expected.

## Risks / Trade-offs

- [Origin sort is approximate for one stretched mesh with surfaces at very
  different depths; skinned poses don't move the origin] → documented v1
  limitation (spec text); per-surface centroids are the upgrade path and need
  cached bounds (future change).
- [Side-list sort cost per frame] → entries are few (transparent mesh surfaces
  only), key is 2 floats + seq; composite-key sort over a tiny array is noise
  next to the draws themselves.
- [Behavior change for implicit translucent fades (D2)] → BREAKING note in the
  proposal; golden corpus sweep in tasks catches any committed scene in the
  class.
- [Wire-block drift across five mirrors] → the change lands all mirrors in one
  commit and runs `gen_prelude.py --check`; the gate fails loudly on drift.
- [Transparent sub-pass and RT passes] → side list resets per segment;
  GL-family RT passes reuse the existing `mesh_pip_cw` winding row of the same
  matrix, so no new variants.

## Migration Plan

Single change, no data migration. Scripts using translucency add
`alphaMode: 'blend'`; everything else is source-compatible. Rollback = revert
the change; the mode field defaults to `'opaque'`, so pre-change scenes render
as before except the D2 saturation class.

## Open Questions

None — the exploration settled routing stance, sort key, cutoff, blend-equation
policy, and golden policy.
