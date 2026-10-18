# Proposal

## Why

`efx.noise.imageData` always bakes the normalized noise into opaque grayscale
(RGB = value, alpha = 255). The classic procedural texture for soft particles,
glow/fog sprites, and dissolving masks is the other shape — white RGB with the
noise in the alpha channel so the draw-time tint colors it — and today scripts
must hand-roll that remap loop over a raw `Uint8Array` + `graphics.createImageData`,
duplicating the domain/clamp/rounding logic the helper already owns.

## What Changes

- Add an optional `alpha` flag to the `efx.noise.imageData` options bag
  (`alpha?: boolean`, default `false`):
  - `alpha: true` — RGB is white (`255, 255, 255`) and the alpha channel
    carries the same normalized bytes (same `domain` mapping, clamping, and
    deterministic rounding) the grayscale path produces today.
  - omitted / `false` — current grayscale output, bit-for-bit unchanged.
- Validation follows the existing strict bag rules: a non-boolean `alpha`
  throws `TypeError`; `alpha` is added to the helper's allowed-key list so it
  is not rejected as unknown.
- Pure JS in the shared prelude — no new native, no web-bridge change
  (ADR 0059 constraint preserved).
- Receipts: script-level unit tests (extent/accept/reject — `EfxImageData`
  is script-opaque), the gallery type test, and a new alpha-mode quad in the
  noise golden scene so the byte placement is pinned cross-target like the
  grayscale mapping already is (goldens re-baselined via the verification
  server).

Assumption (recorded): `alpha` is a strict boolean flag with one shared
normalized value stream — no separate alpha `domain`, no per-channel remaps.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `noise`: the "ImageData helper is a pure-JS convenience" requirement gains
  the `alpha` option — alpha-mode pixel layout (white RGB, normalized noise in
  alpha), strict boolean validation, and unchanged default behavior.

## Impact

- **Script API**: `efx.noise.imageData` options bag (`NoiseImageDataOptions`);
  no existing call changes behavior (`alpha` defaults to off). Requires the
  matching update to `gallery/src/api/efx.d.ts` and regeneration of the
  committed `docs/api/` reference.
- **Code**: `src/prelude/prelude.js` (option parse + pixel loop) and the
  committed `src/prelude/prelude.h` (regenerate via `tools/gen_prelude.py`;
  the Linux gate fails on drift). No C changes; the web runtime gets the
  behavior through the shared prelude.
- **Tests**: `tests/unit/api_tests.c` (`noise_imagedata_js`),
  `gallery/src/api/efx.type-test.ts`, `tests/goldens/noise/main.js` + golden
  re-baseline.
- **Docs**: `docs/api/` regenerated (never hand-edited). `docs/js-api.md` —
  no update needed: no API design rule changes (the option follows the
  existing trailing-bag convention).
- **ADR**: No ADR — this extends ADR 0059's established pure-JS helper design
  without altering any of its decisions (no new native, no new determinism
  mechanism).
- **Gate**: behavior change → full four-target verification (ADR 0020);
  verify on the SSH server before dispatching CI.

## Non-goals

- No separate alpha domain or remap — one normalized value stream drives
  whichever channel it lands in.
- No colored/gradient output modes (value → color ramps remain script-side).
- No premultiplied-alpha conversion; bytes are stored as today's path stores
  them and `createTexture` semantics are unchanged.
- No changes to `fill2`, `noise2`/`noise3`, `graphics.createImageData`, or
  texture upload; no new natives; no web-bridge code.
- No gallery sample changes (Mini Golf's grayscale use is unaffected).
