# 0029 — Post-processing is one declarative chain over an implicit scene target, with an engine-owned effect registry

Status: Accepted (2026-09, change `f5b-post-fx`)

Supports: vision.md — simple post processing (roadmap F5); ADR 0015
(fixed-function consumer API; no script-visible shaders or passes); ADR 0021
(canned single-source shaders via sokol-shdc); ADR 0026/0027 (uniform-driven
shaders, no permutations); ADR 0028 (render targets sampled directly,
segmented display list); ADR 0019 (value-snapshot state vs handle
references); ADR 0020 (golden images pin rendering); ADR 0025 (engine-owned
attachment formats).

## Context

F5b adds the roadmap's post-processing: color filter and blur, plus the
composite multi-pass proof. The API could take several shapes — imperative
fullscreen-effect calls, a global setter per effect (the provisional
`setColorFilter` / `setBlur` catalog entries), a three.js-style composer of
script-visible passes, or a Pixi-style per-object filter array. The renderer
also had to choose when the extra offscreen work happens, because every
committed golden is verified byte-for-byte and "no FX = old pixels" is a
large verification simplification. The full process record is
`openspec/changes/f5b-post-fx/`.

## Decision

- **One declarative chain entry point.** `setPostEffects(list | null)` sets
  the frame's ordered effect chain; `null`/`[]` clears. The array is the
  order, the snapshot, and the whole contract; it validates eagerly and
  atomically and records as plain engine state applied at frame resolve
  (the `setClearColor` model, not per-draw state). Per-effect globals and an
  imperative per-frame `applyFullscreenEffect` are rejected: they hide
  stacking, invite confusion about when effects apply, and do not map onto
  value-snapshot semantics.
- **`mix` per entry, not blend modes between entries.** Each entry writes
  `lerp(input, output, mix)`. The fullscreen passes are opaque, so a blend
  against the previous pass's output reduces to exactly `mix`; a general
  blend subsystem would grow surface area no scenario needs. `mix` covers
  the actual motivation ("subtle blur / faint bloom").
- **Implicit scene target + fast path (hybrid, not always-on).** Chain empty
  and scale 1 renders direct to the default target exactly as before
  (committed goldens stay byte-identical). Chain set or scale ≠ 1 renders
  the default segment into an engine-owned implicit scene target, runs the
  chain through engine-owned ping-pong temporaries, and blits to the default
  target. An always-on normalized resolve would pay a full-screen copy +
  sample on every frame and make every golden depend on the new path for no
  feature gain. The scene target and temporaries are engine-owned: no
  handle, no class, never script-visible.
- **Engine-owned effect registry; pass structure is invisible.** Each effect
  is a descriptor — name → option validation, a pass plan built from the
  options (shader, uniform block, temporary resolutions). `blur` switches
  between a separable gaussian and a downsample chain at an internal radius
  threshold; `bloom` is threshold → downsample blur → additive up. None of
  that appears in the API (an entry is a name plus numbers). Adding an
  effect is GLSL plus a descriptor, with no binding or pipeline-permutation
  work. Script-assembled pass chains (three.js EffectComposer model) and
  per-object filter arrays (Pixi model) are rejected: the former needs
  script-visible passes/shaders (ADR 0015), the latter needs per-object
  render targets and padding semantics — one frame-level chain is the right
  granularity for this engine.
- **New canned shaders follow the existing discipline.** One single-source
  GLSL per pass type (color-ops, blur tap, bright-pass, mix, additive
  composite, copy), compiled with the pinned sokol-shdc, uniform-driven with
  no pipeline permutations (ADR 0021/0026/0027): effect options are
  uniforms; `mix` is a uniform lerp; textures bind as whole samplers.
- **Render scale rides the scene target.** The scene target is
  `ceil(surface × scale)`; the final blit samples it with a nearest or
  linear sampler. The 2D camera frame maps onto the scene target (it is the
  active rendering surface during the scene), so camera semantics are
  unchanged — scale composes after the frame. The implicit scene target
  alone (scale ≠ 1, no chain) is allowed: nearest-upscale retro rendering is
  a first-class use.
- **The chain applies to the default target's resolve only.** Draws recorded
  into user RenderTargets render raw and sample unfiltered; only the final
  screen resolve passes through the chain.

## Consequences

- `blur`'s kernel weights, the separable→downsample threshold, and bloom's
  downsample depth are implementation tuning pinned by goldens — no spec or
  API impact; the registry can change them without an API change.
- Precision drift across backends (RGBA8 clamp/round in blur and bloom
  accumulation) is bounded by env-default formats (ADR 0025), downsampled
  intermediates, and the ADR 0020 tolerance; the fast-path goldens are
  unaffected either way.
- The scene target reallocates lazily at resolve on window resize; a
  one-frame hitch on resize matches default-framebuffer behavior.
- The registry makes effects cheap to add but not script-extensible — the
  fixed-function mandate (ADR 0015) is the hard wall; every effect is an
  engine-reviewed canned pass.
- Rollback is trivial: chain-unset behavior is byte-identical.

## Rejected alternatives

- **Imperative `applyFullscreenEffect(name, options)` per frame** — a
  singular call hides stacking and ordering and does not fit value-snapshot
  semantics.
- **Per-effect globals (`setColorFilter` / `setBlur`)** — N setters for N
  effects, no stacking or ordering, API churn per effect; the provisional
  entries never shipped.
- **Inter-entry blend modes** — opaque fullscreen passes make the previous
  output the input, so a blend reduces to `mix`; general plumbing for no
  scenario.
- **Always-on normalized resolve (three.js composer)** — costs a
  full-screen copy/sample every frame and couples every golden to the new
  path.
- **Script-assembled pass chains** — requires script-visible passes and
  shaders, violating ADR 0015.
- **Per-object filter arrays (Pixi model)** — needs per-object render
  targets and padding semantics; one frame-level chain is the right
  granularity.
- **A separate supersampling milestone** — render scale uses the same scene
  target + blit mechanism; splitting it would duplicate the pipeline.
