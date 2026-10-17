# Proposal

## Why

Textures are immutable after creation: `createTexture` uploads pixels once and
no engine surface can change them afterwards. Every streaming-image feature —
video playback (planned follow-up change), procedurally animated textures,
generated effect atlases — needs per-frame pixel uploads into a texture the
existing fixed-function draw paths already consume. Building the seam now,
standalone, keeps the video change a pure media-decoding change and gives
scripts a generally useful primitive in its own right.

## What Changes

- `createTexture(imageData, opts)` gains a `dynamic` option (`false` default):
  a dynamic texture is created with streaming GPU usage and supports pixel
  updates; default textures are unchanged and remain update-free.
- Live dynamic Textures gain `update(imageData)`: replaces the full texture's
  pixels with same-size ImageData in one call.
- Pure-C core: `efx_render_texture_update(handle, rgba)` on the texture slot
  table; the render sink gains an `update_texture` hook implemented once in
  `src/platform/pipeline.c` via `sg_update_image` (Sokol carries it to
  D3D11/Metal/GL/GLES — no per-backend code).
- Display-list semantics stay live-binding: draws recorded before an update
  and played after it sample the updated pixels (texture bindings resolve at
  playback, not record time).
- Error surface (all `TypeError`): non-ImageData argument, ImageData whose
  size differs from the texture, `update` on a non-dynamic texture, `update`
  on a destroyed texture; cases added to the script error catalog.
- CPU-only run modes (`--script`, no surface): dynamic creation and update are
  accepted and validated but upload nothing, matching existing texture
  semantics (ADR 0052).
- Determinism golden scene `texture_update` (animated pattern driven by frame
  index) gated on all four targets per ADR 0020.
- New ADR (`docs/decisions/0061`) covering the streaming-usage choice and
  full-texture (no sub-rectangle) update scope; `gallery/src/api/efx.d.ts`
  surface, regenerated `docs/api/`, prelude `createTexture` option-validation
  update + `tools/gen_prelude.py` regen.

## Capabilities

### New Capabilities

<!-- none: this change extends the existing texture resource contract, it
     does not introduce a new domain. -->

### Modified Capabilities

- `2d-layer`: the **Image and texture resources** requirement is extended —
  the `createTexture` opts enumeration gains `dynamic` (boolean, `false`
  default), and live dynamic Textures expose `update(imageData)` with defined
  error behavior, CPU-only-mode behavior, and playback-time binding semantics.
  All existing scenarios are preserved; new scenarios cover the dynamic
  surface.

## Impact

- **Render core**: `src/render/render.h` (sink vtable + error codes),
  `src/render/render_texture.c` / `render_internal.h` (`dynamic` flag on
  `tex_slot`, update entry point, deferred-release interplay unchanged).
- **Platform**: `src/platform/pipeline.c` — `pipe_update_texture` sink
  implementation (single Sokol call path shared by all four targets).
- **Script API**: `src/api/api_2d.c` (desktop wire native + method
  registration), `src/web/bridge_render2d.c`-family bridge native,
  `src/web/js/core.js` (`EfxTexture` class method), `src/web/js/audio.js`
  graphics-fragment `createTexture` signature (threading `dynamic` through),
  `src/prelude/prelude.js` option validation (cold path, ADR 0049) +
  prelude regen.
- **Types/docs**: `gallery/src/api/efx.d.ts`, regenerated `docs/api/`
  (`npm --prefix gallery run docs:markdown`), type test coverage in
  `gallery/src/api/efx.type-test.ts`.
- **Tests**: render-core unit tests (mock sink counting `update_texture`
  calls, error codes), golden scene `tests/goldens/texture_update/`,
  error-catalog cases (`tests/scripts/s_error_catalog.js`), lifecycle
  validation cases.
- **No new dependencies**; no vendor changes; wasm size impact negligible.
