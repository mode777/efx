# Design

## Context

`f5a-render-targets` (must gate green first) delivers the RT machinery this
change builds on: segmented playback, engine-owned attachments, and
env-default formats. The renderer's shader strategy is pinned — single-source
GLSL compiled by pinned sokol-shdc (ADR 0021), uniform-driven single
shaders with no permutations (ADR 0026/0027), fixed-function consumer API
(ADR 0015). Golden tolerance and determinism are settled (ADR 0020). The
exploration session settled the product shape: one declarative chain entry
point, `mix` per effect, implicit scene target only when needed, and render
scale riding the same blit. This design records how that maps onto the
engine.

## Goals / Non-Goals

**Goals:**
- One script-facing concept (the chain) covering color ops, blur, and
  composite multi-pass effects, extensible engine-side without API churn.
- Zero cost and byte-identical output when nothing is set (fast path —
  all committed goldens stay valid).
- Deterministic, per-backend-stable results inside the ADR 0020 tolerance.
- A native registry clean enough that adding an effect is a descriptor +
  canned shader(s), no binding or pipeline-permutation work.

**Non-Goals:** chains on user RTs; script-defined shaders/passes;
depth-based/motion/HDR effects; per-object filters; inter-entry blend
modes; changing any no-FX behavior.

## Decisions

**D1 — Declarative chain via `setPostEffects`, not imperative
`applyFullscreenEffect` and not per-effect globals.** The array is the
order, the snapshot, and the whole contract; it validates atomically and
records as plain engine state applied at frame resolve (the `setClearColor`
model — resolve-phase state, not per-draw state).
*Rejected: `applyFullscreenEffect(name, options)` per frame* — imperative
singular hides stacking, invites per-call confusion about when effects
apply, and doesn't map onto value-snapshot semantics.
*Rejected: the provisional `setColorFilter`/`setBlur` globals* — N global
setters for N effects, no stacking or ordering, API churn per effect.

**D2 — `mix` per entry, not blend modes between entries.** Each entry
writes `lerp(input, output, mix)`. This covers "subtle blur / faint bloom"
— the actual motivation for blend control — without a blend-mode subsystem
on opaque fullscreen passes.
*Rejected: inter-entry blend modes* — the passes are opaque fullscreen
quads; blending against what (the previous pass's output is the input)
reduces to exactly `mix`; a general blend plumbing would grow surface area
no scenario needs.

**D3 — Implicit scene target + fast path (hybrid, not always-on).** Chain
empty and scale 1 → render direct to the default target exactly as today.
Chain set or scale ≠ 1 → scene renders into an engine-owned scene target,
entries ping-pong through two engine-owned temporaries, final pass blits.
Godot's model (extra buffers only when the environment needs them) rather
than three.js's always-copying composer.
*Rejected: always-on normalized resolve* — pays a full-screen copy +
sample on every frame (web/low-end), and worse, makes every committed
golden depend on the new path for no feature gain; "no FX = old pixels
byte-identical" is also a large verification simplification.

**D4 — Engine-owned effect registry; pass structure is not script-visible.**
Each effect is a descriptor: name → option validation, a pass plan built
from options (shader, uniform block, temp resolutions), executed by the
resolve pipeline. `blur` switches between separable gaussian and a
downsample-chain at an internal radius threshold; `bloom` is
threshold → downsample chain → additive upsample; none of that appears in
the API (an entry is name + numbers). Adding an effect = GLSL + descriptor;
no binding changes.
*Rejected: script-assembled pass chains (three.js EffectComposer model)* —
requires script-visible passes/shaders, violating ADR 0015.
*Rejected: per-object filter arrays (Pixi model)* — needs per-object RTs
and padding semantics; one frame-level chain is the right granularity for
this engine.

**D5 — New canned shaders follow the existing discipline.** One
single-source GLSL per pass type (color-ops, blur-tap, bright-pass,
upsample-composite), compiled with the pinned sokol-shdc, uniform-driven
with no pipeline permutations (ADR 0026/0027): effect options are uniforms;
`mix` is a uniform lerp; textures bind as whole samplers. Post pipelines are
separate from the mesh pipeline but live in the same generated blob.

**D6 — Render scale rides the scene target.** The scene target is sized
`ceil(surface × scale)`; the final blit samples it with nearest or linear
filter. The 2D camera frame maps onto the scene target (it *is* the active
rendering surface during the scene), so camera semantics are unchanged —
scale composes after the frame, unlike the camera's frame which composes
before. The implicit scene target alone (scale ≠ 1, no chain) is allowed —
nearest-upscale retro rendering is a first-class use.
*Rejected: a separate supersampling milestone* — the mechanism is the same
scene target + blit; splitting it would duplicate the pipeline.

**D7 — Sequencing.** This change's `js-api` delta is written against the
post-f5a spec text; f5a's gate must pass on all four targets before f5b
starts (roadmap ladder rule). Ping-pong temporaries are engine-owned RTs
created via the f5a internal machinery but never exposed as script
resources (no handles, no class, not in the native-backed class list).

## Risks / Trade-offs

- [Precision drift across backends (RGBA8 clamp/round differences in blur
  and bloom accumulation)] → env-default formats per ADR 0025, downsample-
  first chains to bound tap counts, per-target goldens pin results inside
  the ADR 0020 tolerance; re-baseline is a documented knob if a backend
  drifts (fast-path goldens are unaffected either way).
- [Chain cost on web/low-end at scale 2] → 8-entry cap, radius ≤ 64,
  downsampled intermediates; the fast path is free, so cost is opt-in.
- [Scene-target reallocation on window resize while chained] → reallocate
  lazily at resolve; one-frame hitch on resize is acceptable and matches
  default-framebuffer behavior.
- [Registry creep toward shader-shaped features] → the fixed-function
  mandate is the hard wall (ADR 0015): effects are engine-reviewed canned
  passes; the registry makes adding them cheap, not script-extensible.

## Migration Plan

Additive; the only removals are provisional catalog entries that never
shipped (`setColorFilter`/`setBlur` → replaced by `setPostEffects`).
Chain-unset behavior is byte-identical, so rollback is trivial.

## Open Questions

- Gaussian kernel weights and the blur's separable→downsample radius
  threshold — implementation tuning, pinned by goldens once captured; no
  spec or API impact.
- Whether `bloom`'s downsample depth (chain length) adapts to scene size or
  is fixed at 3–4 levels — tune during golden authoring.
