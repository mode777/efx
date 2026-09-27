# Design

## Context

The display list (ADR 0019) currently plays back every record against the
default framebuffer; the renderer (ADR 0025) already folds the clip-depth
remap into the MVP engine-side and requires engine-created attachments to
declare env-default pixel formats. The resource model (ADR 0011/0012) gives
dynamic-count resources a native-backed class with explicit `destroy()` and
deferred release while the display list holds them — Texture already
implements this, and the F4b map-retention mechanism extends the same idea
to bound material maps. Sokol exposes offscreen passes as
`sg_attachments` + `sg_image`; sampling an attachment as a texture is the
normal sokol pattern. The exploration session settled the API shape; this
design records how it maps onto the existing walls (`src/render`, `src/api`,
`src/web`, prelude untouched).

## Goals / Non-Goals

**Goals:**
- RenderTarget as a native-backed class with the exact Texture lifecycle
  semantics (deterministic destroy, deferred release, GC backstop).
- Sampling via coercion at the binding layer — one code path for
  Texture-or-RenderTarget, zero new script-visible concepts.
- Display-list segmentation that preserves the existing reordering freedom
  inside a segment and gives deterministic cross-target ordering.
- Byte-identical rendering when no target is used (all committed goldens
  unchanged).

**Non-Goals:** post FX and render scale (F5b); readback, mipmaps, MSAA,
resize; chains on user targets; any change to the no-RT path.

## Decisions

**D1 — Render targets are sampled directly; no `rt.texture` alias.**
The C binding layer accepts a live RenderTarget wherever it accepts a live
Texture and extracts the underlying `sg_image` when recording the handle.
One object, one owner; the display list and map retention already work on
handles, so nothing new is introduced.
*Rejected: exposing `rt.texture` returning a Texture view* — two JS owners
for one GPU resource breaks the 1:1 ownership model (ADR 0011/0012) and
forces ref-counting or a half-object whose `destroy()` throws.
*Rejected: keeping the provisional `drawRenderTarget`* — a parallel draw
path duplicates camera/blend/sourceRect semantics and diverges over time;
the provisional entry never shipped, so dropping it breaks nothing.

**D2 — Segments, not a dependency graph.** Every record carries a target
id (0 = default). `beginRenderTarget`/`endRenderTarget` are control records
that open/close a segment; playback walks segments in first-record order
and the existing stable-sort reordering applies only within a segment.
This gives deterministic semantics ("a draw sampling a target sees that
target's earlier segments complete") with zero scheduling machinery.
*Rejected: reordering across segments by sort key* — a record's sort key
says nothing about which target's contents it samples; cross-segment
reordering creates read-before-write hazards the API cannot express.
*Rejected: RT-segments-first scheduling* — breaks interleaved multi-segment
use (render A, composite A, render A again, composite again), which
segment order handles trivially.

**D3 — Clear on every begin, value-snapshotted clear color.** Entering a
target clears it to the clear color in effect at that record, so goldens
are frame-deterministic and "what you see is the last segment" is a one-line
rule.
*Rejected: persistent contents across begins/frames* — goldens would depend
on frame history; the deterministic option costs nothing.
*Rejected: a `clear` option on begin* — add when a use case appears; not
needed for F5 scope.

**D4 — One feedback-loop rule, checked at record time.** A draw whose
sampled target equals the active target throws `TypeError` at the API call.
Sokol/backends forbid read-while-attached anyway; making it a record-time
error keeps the failure script-visible and immediate instead of a
backend-dependent artifact.

**D5 — Attachments: env-default color + depth, no MSAA.** Each RenderTarget
creates one color `sg_image` and one depth `sg_image` in the backend's
default pixel formats (the ADR 0025 rule for engine-created attachments);
the existing MVP clip-depth fold applies to RT passes unchanged. Depth is
mandatory so `drawMesh` semantics are target-independent. MSAA and mips are
non-goals.
*Rejected: depth-less color-only targets* — `drawMesh` inside a target would
silently lose depth testing or need a second target flavor; one flavor
keeps the API uniform.

**D6 — Aspect and default camera follow the active surface.** The 2D
default frame and the 3D aspect read the active rendering surface's extent
(default target or RT), mirroring the F2 "default camera matches the
window" rule. An explicit `setCamera2D` frame stretches onto whatever
surface is active, unchanged.

**D7 — Resource exposure (per the design rules).** RenderTarget is a
dynamic-count resource → native-backed opaque class with explicit
`destroy()` (idempotent), GC-finalizer backstop, finalized at teardown; GPU
size feeds the existing GC-pressure accounting. Deferred release covers two
holders: pending display-list records and bound material maps (the F4b
retention set, extended). No slot bank — targets are user-authored
resources like Textures.

**D8 — Web bridge parity.** RT handles are engine-side ids; the `src/web`
bridge records the same begin/end/segmented list, so the cross-runtime
compare and web goldens exercise identical semantics with no
quickjs-specific state.

## Risks / Trade-offs

- [Backend orientation flip when sampling RTs (GL default-framebuffer vs.
  offscreen origin conventions)] → engine-owned fix in the quad pipeline,
  verified by per-target goldens; the ADR 0025 discipline (never in
  shaders, all columns engine-side) is the template. Caught earliest on
  the Linux llvmpipe + Emscripten pair.
- [Pass-switch cost per segment] → one begin/end pair per segment switch;
  acceptable for the F5 use set, and the no-RT fast path pays nothing.
- [GPU memory growth from unbounded targets] → 4096 cap per side, native
  size in GC pressure, teardown finalization; same envelope as Texture.
- [Deferred-release holders growing (list + many bound maps)] → the
  retention set is the same bookkeeping F4b shipped for Textures, extended
  by one handle type; unit tests pin release points.

## Migration Plan

Additive: new API entries, new record fields, no shipped behavior changes.
The provisional `drawRenderTarget` catalog entry is replaced in
`docs/js-api.md` in the same change. Rollback is reverting the change; no
data or resource format persists across versions.

## Open Questions

- Exact 2D-camera stretch behavior when an explicit `setCamera2D` frame is
  active inside a target with a different aspect (stretch policy already
  says non-uniform stretch — confirm a golden covers the off-rectangle
  aspect case). Answerable during test-scene authoring without changing the
  specs.
- Whether the record budget counts begin/end control records (they are
  records; decide the constant during implementation — spec impact is nil).
