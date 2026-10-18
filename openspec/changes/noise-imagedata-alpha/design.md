# Design

## Context

`efx.noise.imageData` is a pure-JS prelude helper (ADR 0059): it parses the
bag (`__efxNoiseParse` with `['domain']` as extra allowed keys), fills a
`Float32Array` through the `noiseFill2` native, remaps raw values onto bytes
via the `domain` option (linear, clamped, `(t*255 + 0.5) | 0`), and hands a
`Uint8Array` to `graphics.createImageData`. The pixel loop writes
`[b, b, b, 255]` per sample. `EfxImageData` is script-opaque (only
`width`/`height`/`destroy`), so byte placement is not assertable from script —
the committed golden scene is the byte-level receipt, as its unit-test comment
already notes. See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**

- One new optional boolean `alpha` flag; `true` yields white RGB with the
  normalized noise in the alpha channel.
- Grayscale path byte-for-byte unchanged when the flag is off.
- Cross-target receipts consistent with how ADR 0059 already pins the helper
  (exact-bit unit checks + golden scene).
- Zero C changes: desktop and web both get the behavior from the shared
  prelude.

**Non-Goals:**

- Separate alpha domain/remap, color ramps, premultiplied-alpha conversion
  (see proposal Non-goals).
- Any change to natives, the web bridge, or texture upload semantics.

## Decisions

### D1 — Strict boolean flag, default `false`

`alpha` accepts only `true`/`false`; anything else throws `TypeError`, and
`'alpha'` is added to the helper's extra allowed-key list
(`['domain', 'alpha']`) so the known-key check admits it.
*Alternative considered:* truthy coercion — rejected; the prelude's cold-path
validation is uniformly strict (unknown keys and wrong types already throw),
and coercing `'yes'`/`1` to true would let a typo silently pass.

### D2 — One normalized byte stream, channel placement only

The byte `b` is computed exactly as today (`(t*255 + 0.5) | 0` after the
`domain` map and clamp); alpha mode only changes where it lands:
`[255, 255, 255, b]` vs `[b, b, b, 255]`. The flag is resolved before the
loop so the per-pixel work stays identical in cost and float behavior —
no new float ops, so the determinism receipts are untouched.
*Alternative considered:* a second `alphaDomain` — rejected (proposal
Non-goal; no use case that the shared `domain` misses).

### D3 — Prelude-only, no native

The helper's spec scenario ("No new native for the helper") and ADR 0059's
design require composing existing entry points; `createImageData` already
accepts arbitrary RGBA bytes, so alpha placement is a JS concern. The web
runtime inherits the option through the shared prelude unchanged.

### D4 — Byte-level receipt via the noise golden scene

Since script cannot read pixels back, pin the new layout the same way the
grayscale mapping is pinned: extend `tests/goldens/noise/main.js` with a
fourth `alpha: true` texture drawn over the non-white clear color, re-laying
the scene out to four quads (any scene change re-baselines the goldens
anyway). Re-baseline once on the verification server — native under llvmpipe,
web under pinned chrome — per `docs/verification-server.md`, before
dispatching the gate. Unit tests (`tests/unit/api_tests.c`) pin extent,
option acceptance, and rejection (they run on all four targets via ctest);
`efx.type-test.ts` pins the declaration.

### D5 — No ADR

Extends ADR 0059's helper without altering any of its decisions (no new
native, same determinism mechanism). `docs/js-api.md` is untouched: the flag
follows the existing trailing-option-bag rule, so no API design guideline
changes.

## Risks / Trade-offs

- [Golden re-baseline churn touches committed images] → single self-contained
  scene edit; capture on the SSH server with the documented recipe so the
  goldens match the gate's renderers exactly.
- [Re-baseline may collide with the dated llvmpipe-bump risk (2026-10-19)] →
  capture immediately before dispatching the gate; if `ubuntu-latest` bumps
  first, that re-baseline is a separate, already-documented exercise (ADR 0020).
- [Scripts may expect premultiplied alpha in `createTexture`] → bytes are
  stored straight, as `graphics.createImageData` semantics already define;
  with white RGB premultiplication is visually a no-op, and the d.ts doc text
  states the layout plainly.
- [Alpha-mode quad partially transparent pixels vary with rasterizer] → the
  scene draws an opaque noise alpha quad; per ADR 0020 the tolerance absorbs
  rasterizer jitter, and noise values themselves are bit-identical.

## Migration Plan

Additive option, default off — no migration. Rollback is reverting the
prelude (+ regenerated `prelude.h`), the golden scene, and the goldens.

## Open Questions

None.
