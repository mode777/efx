# 0028 — Render targets are sampled directly (texture coercion), segmented in the display list, and released on the Texture lifecycle

Status: Accepted (2026-09, change `f5a-render-targets`)

Supports: vision.md — render targets on the consumer API (roadmap F5);
ADR 0011/0012 (resource classification and memory discipline — dynamic-count
resources are GC-finalized classes with explicit `destroy()`, no
ref-counting); ADR 0019 (value snapshot vs handle reference); ADR 0025
(engine-owned attachments declare env-default formats; the clip-depth fold
stays engine-side); ADR 0015 (fixed-function consumer API).

## Context

F5a adds offscreen rendering: `createRenderTarget` /
`beginRenderTarget` / `endRenderTarget`. Two questions had API-shape
consequences beyond the change itself. First, how does a rendered target
get *sampled* — the provisional catalog had a dedicated
`drawRenderTarget` and an `rt.texture`-style alias was considered. Second,
how does a GPU-free display list (ADR 0019) order records that both write
and read offscreen memory. The full process record is
`openspec/changes/f5a-render-targets/`.

## Decision

- **A live RenderTarget is accepted wherever a live Texture is accepted**
  — `drawQuad`'s texture argument, per-channel material `map`s, and
  `alphaMask` — with identical validation and error behavior. The coercion
  lives in the binding layer and the sampling resolution
  (`efx_render_sample_*`), which dispatch on the handle's registry tag;
  render-target handles carry a tag in bits 28–31 of the index half so the
  two handle namespaces stay disjoint while every handle remains small
  enough to survive the web bridge's double wire format unchanged.
- **No alias object exists.** There is no `rt.texture` property and no API
  that returns a Texture for a target: two JS owners for one GPU resource
  would force ref-counting or a half-ownership contract (ADR 0011/0012).
  The provisional `drawRenderTarget` entry is dropped instead of shipped.
- **Display-list segmentation.** Every record carries its target;
  `beginRenderTarget`/`endRenderTarget` are control records (with the
  clear color value-snapshotted into the begin record — every begin starts
  from a cleared target); the renderer's stable-sort reordering freedom
  stops at segment boundaries and segments play back in first-record
  order. A draw sampling a target therefore always observes that target's
  completed earlier segments, with no dependency scheduling.
- **One feedback rule, enforced at record time**: a draw (quad texture or
  mesh map) naming the currently active target throws `TypeError` and
  records nothing.
- **The active target is the rendering surface.** The default 2D camera
  frame and the 3D projection aspect derive from the active target's
  extent exactly as they derive from the window; the MeshData→Mesh
  pipeline, depth testing, and the ADR 0025 MVP fold apply unchanged
  inside targets (each target owns a color+depth attachment pair in the
  environment-default formats).
- **Release follows the Texture lifecycle exactly**: idempotent
  `destroy()` with GC-finalizer backstop, native release deferred to frame
  end while display-list records or bound material maps hold the handle
  (the F4b bind-ref/retain set, extended to targets).

## Consequences

- F5b's post pipeline builds directly on this machinery: engine-owned
  ping-pong targets are these attachment pairs with no script handle; a
  chain attached to a resolve is a segment-policy change, not new GPU
  plumbing.
- Sampling orientation is engine-owned end to end (targets render and
  sample upright on all four backends) — a golden-pinned property, same
  discipline as the ADR 0025 depth fold; no backend flavor leaks into
  shaders or API.
- Pass switching costs a begin/end per segment boundary; the no-target
  path plays back as a single pass and stays byte-identical (all
  committed goldens unchanged).
- Readback, mips, MSAA, and per-target resize remain non-goals; adding
  any of them is a new decision against this record.

## Rejected alternatives

- **`rt.texture` returning a Texture view** — creates two JS owners for
  one GPU resource; needs ref-counting or a `destroy()`-that-throws
  half-object; contradicts the 1:1 ownership model (ADR 0011/0012).
- **Keeping `drawRenderTarget`** — a parallel draw path duplicating
  camera/blend/sourceRect semantics that would drift from `drawQuad`;
  the provisional entry never shipped, so dropping it breaks nothing.
- **Cross-segment reordering by sort key** — a sort key says nothing
  about which target's contents a record samples; read-before-write
  hazards become inexpressible.
- **RT-segments-first scheduling** — breaks the interleaved use case
  (render A, composite A, render A again, composite again) that segment
  order handles trivially.
- **Persistent target contents across begins** — goldens would depend on
  frame history; clear-per-begin with a value-snapshotted clear color is
  deterministic and free.
- **Color-only targets** — `drawMesh` inside a target would silently lose
  depth testing or need a second target flavor; one depth-backed flavor
  keeps the API uniform.
