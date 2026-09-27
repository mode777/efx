# Proposal

## Why

F5's first half. Scripts need to render offscreen and sample the result —
the substrate for the F5b post pipeline and for composited layers (mirrors,
minimap-style views, effects rendered to texture). F4b's gate is green on
all four targets, so F5 is unblocked. The exploration behind this change
settled two API-shape decisions that reshape the provisional F5 catalog:
a render target is sampled **directly** wherever a texture is accepted (no
`drawRenderTarget` function, no `rt.texture` alias), which keeps the 1:1
ownership model intact (ADR 0011/0012 — no ref-counting, no alias objects).

## What Changes

- Milestone **F5a** (first half of roadmap F5, mirroring the F4a/F4b
  split; `f5b-post-fx` is the follow-up). New script API:
  - `efx.createRenderTarget({ width, height })` → a `RenderTarget`
    native-backed class: deterministic idempotent `destroy()` with release
    deferred while the display list or a bound material map holds it,
    GC-finalizer backstop, read-only `width`/`height` query properties.
  - `efx.beginRenderTarget(rt)` / `efx.endRenderTarget()` — redirect
    subsequently recorded draws into `rt` / return to the default target.
    Entering a target clears it to the clear color in effect at that
    record (value-snapshot, ADR 0019). No nesting: a begin while a begin
    is active throws, an end without one throws.
- A live RenderTarget is accepted wherever a live Texture is accepted
  today: `drawQuad`'s `texture` argument, per-channel material `map`s, and
  `alphaMask` — same validation, same error behavior. The provisional
  `drawRenderTarget` catalog entry is dropped before ever shipping
  (provisional-only; no shipped behavior breaks).
- Display-list **segmentation**: every record carries its target; the
  renderer's reordering freedom is constrained to within a segment;
  segments play back in first-record order, so a draw sampling a render
  target always sees that target's completed earlier segments. A draw
  sampling the currently-active target is rejected at record time
  (feedback-loop guard).
- The default 2D camera frame becomes the **active rendering surface's**
  size (the window, or the active render target's extent) when
  `setCamera2D` was never called; 3D aspect likewise derives from the
  active surface.
- Engine-created render-target attachments (color + depth) declare
  env-default pixel formats; the ADR 0025 clip-depth remap discipline
  applies to offscreen passes unchanged.

## Capabilities

### New Capabilities
- `render-targets`: render-target resource lifecycle, render redirection,
  display-list segmentation and playback ordering, and sampling render
  targets through the existing texture-consuming calls.

### Modified Capabilities
- `2d-layer`: the quad `texture` argument accepts a live RenderTarget as
  well as a live Texture; the default camera frame is the active
  rendering surface's size, not just the window's.
- `3d-core`: the 3D-camera requirement's 2D-draws clause is reworded —
  the render-target quad draw is `drawQuad` sampling a RenderTarget, not
  a separate call.
- `lighting`: material `map` channels and `alphaMask` accept a live
  RenderTarget; bound-map retention extends to RenderTargets.
- `js-api`: RenderTarget's `width`/`height` query properties join the
  documented classification; the texture-coercion rule (RenderTarget
  accepted wherever Texture is, no alias object) is recorded in the
  resource-classification requirement.

## Impact

- Code: `src/render` (render-target resource, pass/attachment management,
  display-list segmentation and the feedback-loop guard), `src/api`
  (desktop bindings), `src/web` (bridge parity — same records, engine-side
  handles), display-list record format (target tag, begin/end control
  records).
- API/docs: `docs/js-api.md` F5 section rewritten from provisional to the
  delivered F5a half; **new ADR `docs/decisions/0028`** (render targets
  sampled directly — coercion, segmentation, deferred release).
- Verification: new golden scenes (2D content sampled from a target, 3D +
  lighting rendered into a target, default camera frame into a target)
  and headless unit tests (validation, segmentation ordering, destroy
  deferral) on all four targets; the committed F2/F3/F4 goldens MUST
  remain byte-identical (the no-RT path is untouched).

## Non-goals

- Post FX, the effect chain, and render scale — F5b (separate change
  `f5b-post-fx`, sequenced after this change's gate passes).
- CPU readback of render-target contents, mipmap generation, multisample
  targets, resizing a target after creation.
- Applying post chains to user render targets (the F5b mechanism stays
  target-agnostic, but exposing chains on user targets is not in F5).
- Any change to committed goldens — the no-RT path renders
  byte-identically.
