# Spec Delta

## MODIFIED Requirements

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
