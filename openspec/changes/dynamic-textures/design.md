# Design

## Context

The texture path is `efx_render_texture_create` → sink `create_texture` →
slot table (`src/render/render_texture.c`), with one sink implementation in
`src/platform/pipeline.c` (Sokol `sg_make_image`; shared by D3D11/Metal/GL/
GLES). There is no post-creation upload path anywhere; Sokol's
`sg_update_image` exists but is unused. Texture slots already carry
deferred-release machinery for display-list safety (ADR 0027), and the
headless/CPU-only seam (ADR 0052) keeps unit tests sink-free.

## Goals / Non-Goals

- Goals: one full-texture pixel-replacement path, script-facing on dynamic
  Textures, working on all four targets through the single Sokol call.
- Non-Goals: sub-rectangle updates, mip-chain updates, pixel readback,
  async/staged uploads, PBO/timeline-semaphore tricks. Video (the planned
  consumer) needs exactly full-frame RGBA replacement per frame.

## Decisions

- **D1 — Explicit `dynamic` create option; streaming usage only then.**
  Sokol requires `SG_USAGE_STREAM` for `sg_update_image`. Rather than making
  every texture streamable (hidden cost on all textures, silently mutable
  resources), the flag is explicit: `dynamic: true` → `SG_USAGE_STREAM`
  creation, update allowed; default stays `SG_USAGE_IMMUTABLE` and update
  rejects. `tex_slot` gains a `dynamic` bit; the core rejects
  `efx_render_texture_update` on non-dynamic slots (`EFX_RENDER_ERR_ARG`),
  so the invariant holds below the JS layer too.
  *Alternative*: update-on-any-texture (recreate-under-the-hood) — rejected:
  handle stability, sampler/mip state churn, and ADR 0011's explicit
  lifecycle discipline all suffer.
- **D2 — `dynamic` and `mipmaps` are mutually exclusive.** A mip chain needs
  per-level uploads the v1 scope deliberately excludes; the combination
  throws at create time instead of silently degrading.
- **D3 — Full-texture update only.** `update(imageData)` replaces all pixels
  and requires exact size match. Rect updates would complicate the sink
  signature, the validation, and the wire protocol for no current consumer;
  the native core API is shaped so a rect variant can be added later
  without breaking this one.
- **D4 — Live binding semantics (no snapshot).** Recorded draws already
  resolve texture handles at playback (ADR 0027 retention); updates are just
  new bytes behind the same handle. No copy-on-record, no versioning. This
  matches every existing texture-destroy-before-play behavior and is what
  video expects (decode frame N+1 while draws of frame N are in flight).
- **D5 — Validation split per ADR 0049.** `createTexture` option parsing
  (`dynamic` included) stays cold-path in the shared prelude;
  `texture.update(imageData)` is expected hot (per-frame) so its argument
  check (instanceof ImageData, live, dynamic) happens in the native wire
  function on both desktop and web. Error type is `TypeError` everywhere.
- **D6 — Single sink hook, four targets free.**
  `efx_render_sink` gains `update_texture(ud, native, w, h, rgba)` implemented
  once in `pipeline.c` as `sg_update_image`. Sokol's per-backend image
  update paths (D3D11 `UpdateSubresource`, Metal `replaceBytes`, GL
  `glTexSubImage2D`) need no engine code. Headless keeps working because the
  sink-less path (ADR 0052) validates and succeeds without a sink call, and
  render unit tests install a mock sink to count updates deterministically.

## Risks / Trade-offs

- [Streaming images may have higher driver cost than immutable ones on some
  backends] → only scripts that opt into `dynamic` pay it; default path is
  byte-identical to today.
- [Update while a draw of the same texture is mid-display-list] → the slot's
  existing `bind_refs` retention protects the *handle*, not the bytes;
  Sokol's update replaces contents at call time and playbacks sample whatever
  is current — documented, tested, and exactly the video model.
- [WebGL2 `sg_update_image` path variance] → covered by the four-target
  golden gate (ADR 0020); the web compare job diffs desktop vs web output.

## Migration Plan

Purely additive; no existing behavior changes. Rollback = revert; the new
slot flag and sink hook are unused by existing code paths.

## Open Questions

None blocking. If a later consumer needs rect or mip updates, extend the
sink with additional hooks then (D3 keeps that additive).

## Applied amendments (apply phase)

- The sink hook landed as `update_texture(ud, native, rgba)` (w/h live on
  the platform's `pipe_tex`, which sizes the mip-0 upload); the core shape
  stays rect-extendable per D3.
- The core exposes `efx_render_texture_dynamic(h)` alongside the existing
  sampler/size queries — the web bridge needs the flag for its
  error-code mapping, and it keeps the query family symmetric.
- The desktop `update` method is a prototype function in `api.c`
  (prototype-level validation, ADR 0049 hot-path native validation), not a
  prelude `natives.*` wire — the option-bag path (`dynamic` create opt)
  remains cold-path prelude as planned. No ADR follow-up needed: D1–D6 hold
  as written; ADR 0061 records the decision.
