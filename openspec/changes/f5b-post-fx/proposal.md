# Proposal

## Why

F5's second half: full-screen post-processing — the roadmap's color filter
and blur, plus the composite multi-pass proof. The exploration session
settled the architecture: a declarative chain behind one entry point, per-
effect `mix` instead of inter-pass blending, an engine-owned implicit scene
target engaged only when needed (fast path preserved, committed goldens
byte-identical), and render-resolution/surface-resolution decoupling riding
the same mechanism.

## What Changes

- Milestone **F5b** (second half of roadmap F5; sequenced after
  `f5a-render-targets`' gate passes — it builds on the RT machinery for its
  ping-pong temporaries). New script API:
  - `efx.setPostEffects(list | null)` — the frame's ordered post-effect
    chain; each entry `{ effect: <name>, ...options, mix? }`; at most 8
    entries; validated eagerly and atomically; entries are plain JS objects
    snapshotted at call time; the chain is plain engine state (most recent
    value at frame resolve wins); `null`/`[]` clears.
  - Effect set v1: `colorFilter` (`brightness`/`contrast`/`saturation`/
    `tint`, single pass), `blur` (`radius`, multi-pass internally — the
    engine picks separable vs. downsample-chain, invisible to scripts),
    `bloom` (`threshold`/`strength`, composite multi-pass — the registry's
    N-pass proof). Every entry accepts `mix` (default 1) lerping input to
    output.
  - `efx.setRenderScale(scale, { filter })` — decouples scene render
    resolution from the surface (scale in (0, 2], `nearest`/`linear`
    blit filter); orthogonal to the 2D camera frame.
- Retired provisional catalog entries `setColorFilter`/`setBlur` (never
  shipped — no break).
- Resolve pipeline: chain empty and scale 1 → the frame renders direct,
  byte-identical to today (fast path); otherwise the scene renders into an
  engine-owned implicit scene target (sized by render scale), the chain
  runs through engine-owned temporaries, and the final pass blits to the
  default target. The chain applies to the default target's resolve only —
  user render targets render raw.
- Native effect registry: each effect is an engine-owned descriptor
  (name → pass plan from options, canned sokol-shdc shader, uniform block);
  scripts pass names and numbers only (fixed-function mandate, ADR 0015;
  single-source GLSL per ADR 0021, uniform-driven no-permutation shaders
  per ADR 0026/0027).

## Capabilities

### New Capabilities
- `post-fx`: post-effect chain declaration and validation, the v1 effect
  set with `mix`, the resolve pipeline and fast path, and render scale.

### Modified Capabilities
- `js-api`: the fixed limits gain the post-effect chain length (8); the
  effect option bags are classified JS-managed (plain objects snapshotted
  at call time). This delta is written against the `js-api` spec as it
  exists after `f5a-render-targets` lands (sequential changes; f5b gates
  after f5a).

## Impact

- Code: `src/render` (implicit scene-target pipeline, effect registry,
  ping-pong temporaries — built on f5a's RT machinery), `shaders/*.glsl`
  (new canned passes via pinned sokol-shdc), `src/api` + `src/web`
  (bindings, bridge parity).
- API/docs: `docs/js-api.md` F5 section completed — `setPostEffects`/
  `setRenderScale` delivered, provisional `setColorFilter`/`setBlur`
  retired; **new ADR `docs/decisions/0029`** (post-chain architecture:
  declarative chain + implicit scene target + fast path + registry + mix
  over blend modes).
- Verification: golden scenes (colorFilter, blur, bloom, `mix`-blended,
  render-scale nearest/linear, fast-path identity) and headless unit tests
  (validation matrix, atomicity, snapshot semantics) on all four targets;
  committed goldens MUST remain byte-identical with the chain unset.

## Non-goals

- Post chains on user render targets (the mechanism stays target-agnostic;
  exposing chains on user targets is a future change if asked for).
- Script-defined shaders, passes, or pass ordering beyond array order
  (fixed-function mandate, ADR 0015).
- Depth-based effects (SSAO, depth of field), motion blur, HDR/
  tonemapping, displacement/uv-warp effects.
- Per-object or per-camera filters (Pixi-style granularity) — one chain
  for the frame resolve.
- Inter-entry blend modes — `mix` covers the requested control; the final
  blit is opaque.
