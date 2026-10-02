# Spec Delta

## MODIFIED Requirements

### Requirement: Render target resources
`efx.graphics.createRenderTarget(opts)` SHALL create a GPU RenderTarget from the bag
`{ width, height }`. Both fields are required positive integers; the
documented hard maximum for either is **4096**. A non-integer,
non-positive, or oversized value SHALL throw `RangeError`; a missing value
or an unknown field SHALL throw `TypeError`; on throw nothing SHALL be
created. A RenderTarget is a native-backed class (ADR 0011/0013): released
deterministically by `destroy()` (idempotent), reclaimed by the GC
finalizer backstop if never destroyed, finalized at runtime teardown. A
live RenderTarget SHALL expose the read-only query properties `width` and
`height` naming its pixel size; reading either on a destroyed target SHALL
throw `TypeError`, as SHALL any API call that requires a live target.
`destroy()` on a target referenced by pending display-list records or by a
bound material map SHALL defer the native release until playback completes
or the binding is released — the Texture retention rule, extended — and
further script use of the destroyed target SHALL throw.

#### Scenario: Create, query, destroy
- **WHEN** a script calls `createRenderTarget({ width: 512, height: 256 })`, reads `width` and `height`, calls `destroy()`, then reads either again
- **THEN** the reads return 512 and 256, and the post-destroy reads throw `TypeError`

#### Scenario: Invalid size throws
- **WHEN** `createRenderTarget` is called with `width: 0`, a negative or non-integer size, a size above 4096, or a missing field
- **THEN** the call throws `RangeError` / `TypeError` respectively and no target exists afterwards

#### Scenario: Destroy defers while sampled
- **WHEN** a script records a `drawQuad` sampling a target, then calls `destroy()` on that target in the same frame
- **THEN** the frame renders with the target's contents, the native release happens after playback, and further use of the target throws

### Requirement: Render redirection
`efx.graphics.beginRenderTarget(rt)` SHALL redirect all subsequently recorded draws
into `rt` until `efx.graphics.endRenderTarget()`; `endRenderTarget` SHALL return
recording to the default target (the window). `rt` MUST be a live
RenderTarget (`TypeError` otherwise). Calling `beginRenderTarget` while a
begin is already active SHALL throw `TypeError` and change nothing (no
nesting); calling `endRenderTarget` with no active begin SHALL throw
`TypeError`. Entering a target SHALL clear it to the clear color in effect
when the begin was recorded (value-snapshot, ADR 0019) — every begin starts
from a cleared target, so a target rendered in multiple segments shows the
last segment's contents. While a target is active it is the rendering
surface: 2D drawing follows the `2d-layer` camera rules with the target as
the surface, 3D drawing derives its projection aspect from the target's
extent, and a depth attachment SHALL be active so `drawMesh` depth testing
applies unchanged inside targets.

#### Scenario: Draw into a target and sample it
- **WHEN** a script begins a 256×256 target, draws a quad of known color, ends, then draws the target full-frame via `drawQuad`
- **THEN** the sampled content shows exactly that quad, upright, over the target's clear color

#### Scenario: Each begin clears
- **WHEN** a target is begun and a red quad drawn, then begun again in the same frame and a blue quad drawn at the same place
- **THEN** sampling afterwards shows only the blue quad over the clear color — segments do not accumulate

#### Scenario: Nesting and unbalanced calls throw
- **WHEN** `beginRenderTarget(a)` is called while a begin is active, or `endRenderTarget()` is called with none active
- **THEN** the call throws `TypeError` and the currently active target is unchanged

#### Scenario: 3D renders into a target with depth
- **WHEN** a lit mesh scene with two overlapping meshes is drawn inside a target whose extent differs from the window's, and the target is sampled to the screen
- **THEN** depth occlusion works as on the default target and the perspective aspect matches the target's extent
