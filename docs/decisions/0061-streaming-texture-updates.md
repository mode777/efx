# 0061 — Streaming texture updates are an explicit opt-in with full-texture replacement

Status: Accepted

## Context

Textures were immutable after creation: `createTexture` uploaded pixels once
through the render sink and nothing could change them, blocking every
streaming-image feature (MPEG-1 video playback, procedurally animated
textures). Sokol supports post-creation uploads (`sg_update_image`) but only
on images created with a writable usage, and only for full mip-level
replacements. The decision is recorded for the `dynamic-textures` change;
video playback (`video-playback` change) is the planned consumer.

## Decision

Texture mutability is a create-time, script-visible flag:
`createTexture(imageData, { dynamic: true })` creates the texture with
Sokol's `dynamic_update` usage and is the only texture kind accepted by the
new `update(imageData)` member and the
`efx_render_texture_update(handle, rgba)` core entry point. Updates replace
the full texture contents with exact-size RGBA8 data — there are no
sub-rectangle or per-mip-level updates. `dynamic` and `mipmaps` are mutually
exclusive (TypeError at the option layer, `EFX_RENDER_ERR_ARG` in the core).
Default textures stay `immutable` and byte-identical in behavior. Updates
follow live-binding semantics: texture handles resolve at playback, so a
recorded draw samples whatever pixels are current when it plays (ADR 0027
retention protects the handle, never the bytes). The platform side is one
sink hook (`update_texture`) implemented once in `src/platform/pipeline.c`
via `sg_update_image`, shared by all four targets.

## Consequences

Video and other producers need exactly one decode→`update` call per frame,
with no texture recreation or handle churn. The upload path is
deterministic and unit-testable headlessly (mock-sink counting; sink-less
runs validate and no-op per ADR 0052). Costs accepted: scripts pay a
slightly higher driver cost for dynamic textures (only the ones they opt
in), mip updates and partial updates need future sink-hook extensions
(shape kept additive), and an `update` on a destroyed texture is a JS-layer
`TypeError` while the core answers `EFX_RENDER_ERR_HANDLE` after the slot
is released.

## Rejected alternatives

- **All textures writable** — hidden cost on every texture, silently
  mutable resources, and weaker lifecycle discipline (ADR 0011).
- **Recreate-under-the-hood on update** — handle instability, sampler/mip
  state churn, and per-frame allocations for video's use case.
- **Sub-rectangle / per-level update API now** — no current consumer; the
  hook is shaped so a rect variant can be added later without breaking this
  surface.
