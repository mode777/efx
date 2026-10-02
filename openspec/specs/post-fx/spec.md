# post-fx Specification

## Purpose

Defines the engine's full-screen post-processing layer: the declarative
effect chain and its validation, the effect set and per-effect mix, the
resolve pipeline with its no-chain fast path, and render-resolution
decoupling from the output surface.

## Requirements

### Requirement: Post-effect chain declaration
`efx.graphics.setPostEffects(list)` SHALL set the frame's post-effect chain; `list`
SHALL be `null`, an empty array (both mean no chain), or an array of at
most **8** entry objects. Each entry SHALL be an object
`{ effect: <name>, ...options, mix? }` where `effect` names a registered
effect and `mix` is the entry's input/output blend factor. The call SHALL
validate eagerly and atomically: a non-array `list`, a non-object entry, a
missing or unregistered `effect` name, an unknown option field, or a
wrongly-typed option value SHALL throw `TypeError`; an out-of-range numeric
option, a `mix` outside 0..1, or more than 8 entries SHALL throw
`RangeError`; on throw the previously set chain SHALL remain in effect.
Entries are plain JS objects (JS-managed) snapshotted at call time — later
mutation of a script-held entry object MUST NOT change the applied chain.
The chain is plain engine state: the most recent value at frame resolve
applies, and it persists across frames until changed.

#### Scenario: Valid chain is accepted
- **WHEN** `setPostEffects([{ effect: 'blur', radius: 4 }])` is called and a frame renders
- **THEN** the frame resolves with that blur applied, and subsequent frames keep applying it until the chain changes

#### Scenario: Unknown effect throws atomically
- **WHEN** `setPostEffects([{ effect: 'colorFilter', saturation: 0.5 }, { effect: 'vortex' }])` is called while a previous chain is set
- **THEN** the call throws `TypeError` and the previous chain remains in effect

#### Scenario: Chain length is capped
- **WHEN** a chain of 9 entries is passed
- **THEN** the call throws `RangeError` and the previous chain remains

#### Scenario: Entries are snapshotted
- **WHEN** a script passes an entry object, mutates it (`mix` or an option), and a frame renders
- **THEN** the applied chain uses the values at call time

#### Scenario: Null clears
- **WHEN** `setPostEffects(null)` or `setPostEffects([])` is called after a chain was set
- **THEN** subsequent frames render with no post effects

### Requirement: Effect set and mix
The engine SHALL register exactly these effects in F5b, each with pinned
defaults and documented bounds; unknown fields in an entry SHALL throw
`TypeError` and out-of-bound numbers `RangeError`:

- `colorFilter { brightness? = 1, contrast? = 1, saturation? = 1, tint? = [1, 1, 1, 1] }` — `brightness` multiplies the color, `contrast` pivots at 0.5 grey, `saturation` 0 yields fully desaturated (grey) output and 1 is unchanged, `tint` multiplies rgb (alpha ignored); all finite numbers ≥ 0, tint a 4-component normalized color. One pass.
- `blur { radius? = 1 }` — blur radius in scene pixels: a finite number > 0 and ≤ 64. The engine MAY use several internal passes and downsampling; the script-visible result is only "blurred by radius".
- `bloom { threshold? = 0.8, strength? = 0.5 }` — `threshold` a luminance cut in 0..1 (texels below it contribute nothing to the bloom), `strength` the additive contribution in 0..1. A composite multi-pass effect.

Every entry MAY set `mix?` (a number in 0..1, default 1). The written
result of an entry SHALL be `lerp(input, output, mix)` — `mix: 0` leaves
the input unchanged. Entries apply in array order; the order is
script-authored and observable.

#### Scenario: Defaults are neutral
- **WHEN** `setPostEffects([{ effect: 'colorFilter' }])` is applied to a known scene
- **THEN** the output matches the unchained scene within the golden tolerance (all defaults are identity)

#### Scenario: Mix zero is identity
- **WHEN** the same scene is rendered with `{ effect: 'blur', radius: 8, mix: 0 }` and with no chain
- **THEN** the two outputs match within the golden tolerance

#### Scenario: Chain order is observable
- **WHEN** a scene is rendered with `[colorFilter(saturation 0), colorFilter(tint red)]` and with the two entries reversed
- **THEN** the outputs differ deterministically (grey-then-tint versus tint-then-desaturate)

#### Scenario: Option bounds throw
- **WHEN** an entry has `radius: 0`, `radius: 65`, `strength: 1.5`, or `tint: [1, 1]`
- **THEN** the call throws `RangeError` and the previous chain remains

### Requirement: Resolve pipeline and fast path
When no chain is set and the render scale is 1, the frame SHALL render
direct to the default target, byte-identical to the pre-F5b path — every
committed golden SHALL remain valid without re-baselining. Otherwise the
scene SHALL render into an engine-owned implicit scene target (sized by
the render scale), the chain entries SHALL run in order through
engine-owned temporaries (ping-pong), and the final pass SHALL blit to the
default target. All post passes SHALL be engine-owned canned shaders
(ADR 0015/0021); the chain applies only to the default target's resolve —
draws recorded into user RenderTargets render raw, and their sampled
contents are unfiltered. The implicit scene target and temporaries are
engine-owned (not script-visible resources) and their formats follow the
env-default attachment rule (ADR 0025).

#### Scenario: Fast path is byte-identical
- **WHEN** the full golden suite runs with no chain set and render scale 1
- **THEN** every committed golden matches byte-identically — no re-baselining

#### Scenario: Chain engages the scene target
- **WHEN** a known scene is rendered with `[{ effect: 'blur', radius: 6 }]` and without
- **THEN** the blurred render differs from the direct render by a blur of radius 6 within the golden tolerance

#### Scenario: User render targets render raw
- **WHEN** a chain is active and a user RenderTarget's contents are sampled to the screen through the chain
- **THEN** the target's scene-side contents are unfiltered; only the final screen resolve passes through the chain

### Requirement: Render scale
`efx.graphics.setRenderScale(scale, opts?)` SHALL set the ratio between the scene
render resolution and the default target's size: `scale` a finite number in
(0, 2] (`RangeError` otherwise; default 1), `opts.filter` one of
`'nearest'` or `'linear'` (default `'linear'`; unknown fields or values
throw `TypeError`). The scene target's size SHALL be the surface size
multiplied by `scale`, rounded up; the final blit SHALL scale the scene to
the surface with the chosen filter. Render scale SHALL be orthogonal to
the 2D camera frame: the frame maps onto the scene target (the stretch
policy's active rendering surface), which then maps onto the output
surface. Render scale is plain engine state, applies to the default
target's resolve, and with scale 1 and no chain the fast path renders
direct.

#### Scenario: Half-resolution crisp pixels
- **WHEN** a known pixel scene is rendered with `setRenderScale(0.5, { filter: 'nearest' })`
- **THEN** each scene pixel becomes a 2×2 sharp block on the surface — no interpolated gradients

#### Scenario: Linear filter interpolates
- **WHEN** the same scene is rendered with `filter: 'linear'`
- **THEN** the upscaled output is smoothly interpolated, differing from the nearest render deterministically

#### Scenario: Scale persists and validates
- **WHEN** `setRenderScale(0.5)` is called and several frames render, then `setRenderScale(0)` is called
- **THEN** the half-resolution resolve persists across frames until changed, and the zero call throws `RangeError` leaving 0.5 in effect
