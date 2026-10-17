# 0064 — Material alpha modes route a dedicated sorted transparent pass

Status: Accepted

## Context

Meshes had no coherent transparency story. A translucent fragment (vertex or
tint alpha below 1) blended **in place, in submission order, while writing
depth** — opaque geometry recorded after a translucent mesh was z-killed
behind it, and translucent-vs-translucent order was submission luck. The
material could not express transparency at all: `diffuse.a` was uploaded but
never read, and the only cutout tool was the unconditional binary `alphaMask`
discard (ADR 0027). Recorded for the `material-alpha-modes` change.

## Decision

`Material.alphaMode` — `'opaque'` (default) | `'mask'` | `'blend'` — is the
single, explicit router for mesh surfaces; classification is exact (a surface
is transparent iff its material says `'blend'`), snapshotted per surface into
the mesh record at record time alongside the blend resolve (ADR 0019/0054
pattern, `efx_mesh_record.surface_alpha`).

- `'opaque'`/`'mask'` take the full rendering path: depth test + depth write
  (`depthWrite` option honored), record order, no sorting. `'mask'` discards
  on the alpha-mask sample below the fixed 0.5 cutoff (ADR 0027 stands).
- `'blend'` defers to a per-pass-segment **transparent sub-pass**: drawn after
  every opaque/mask surface of that segment, stable-sorted back-to-front by
  **record origin** (transform translation through the snapshot camera's view
  space; ties by record order, then surface), depth-tested but **never
  depth-writing** (`depthWrite` ignored). Fragment alpha is the product
  `diffuse.a × vertex-color.a × tint.a`; a bound `alphaMask` does **not**
  discard in this mode (mode wins over mask).
- Outside `'blend'`, output alpha is saturated to 1 (glTF OPAQUE/MASK
  semantics) — declaring the mode is the only door to translucency, which is
  what keeps the classification exact and kills the z-writing-translucency
  footgun by default.
- Blend **equations** (`'alpha'`/`'additive'`/`'subtractive'`, ADR 0054) never
  route; they only select how fragments combine inside whichever pass the mode
  chose.
- No new pipelines and no uniform-block growth: the transparent replay pins
  the existing `[blend][0]` test-only rows of the mesh pipeline matrix
  (ADR 0058), and the mode rides the free `mat_params` component; the material
  wire block grows 19 → 20 floats across the documented five-mirror lockstep.

glTF import maps `alphaMode` OPAQUE/MASK/BLEND onto the engine modes and
carries base-color alpha into the diffuse channel for BLEND (the fixed 0.5
threshold approximates any glTF `alphaCutoff`).

## Consequences

Playback owns a second ordering policy: `play_records` defers blend surfaces
to a per-segment side list, sorts it, and replays it through a one-surface
mesh path pinned to no-z-write pipelines. Future draw types that need
transparency routing must opt in explicitly — quads, sprite batches, text,
billboards, and particles deliberately keep their existing painter's-order /
per-batch behavior. The origin sort is approximate for a single stretched mesh
whose parts span very different depths, and skinned poses do not move the
origin — per-surface centroids are the sanctioned upgrade path if that bites.
Scripts that relied on implicit in-place blending of sub-1 vertex/tint alpha
must declare `alphaMode: 'blend'` (the change's one breaking class, golden-
swept to zero fallout in the committed corpus). glTF `alphaCutoff ≠ 0.5` is
approximated, documented in the importer.

## Rejected alternatives

- **Derived classification** (no API; auto-detect translucent draws from
  material/vertex/tint alpha and blend overrides): zero wire growth, but
  heuristic — a single 0.99 vertex silently reorders a draw, and the
  mask-vs-blend conflict has no resolvable answer. Explicit beats implicit.
- **Hybrid auto-detect with explicit override**: superset of both costs
  (heuristics *and* a new field) for no additional capability.
- **Not saturating alpha in opaque/mask modes**: preserves today's in-place
  z-writing blend footgun as the default and makes "opaque" a lie; also
  breaks the exact-classification property the routing relies on.
- **Record-time split into two records per draw** (transparent surfaces in a
  separate record): doubles records and breaks the "drawMesh records one
  draw" contract; the side list achieves the same with no record growth.
- **Sorting whole records instead of surfaces**: misroutes mixed-mode meshes
  (one `'blend'` surface would force the mesh's opaque surfaces out of the
  depth-writing sweep).
- **Auto-routing additive/subtractive draws to the transparent pass**: heuristics
  plus a behavior change on every existing additive scene; additive is
  order-insensitive anyway, so a declared `'blend'` covers the use case.
- **Per-surface centroid or per-triangle sorting**: better precision, needs
  cached bounds / CPU triangle transforms; deferred as the documented upgrade
  path, not v1.
