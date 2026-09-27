# render-targets Specification

## Purpose

Defines offscreen rendering for the engine: render-target resources and
their lifecycle, redirecting drawing into a target, display-list
segmentation and playback ordering across targets, and sampling a target
through the existing texture-consuming calls.

## Requirements

### Requirement: Render target resources
`efx.createRenderTarget(opts)` SHALL create a GPU RenderTarget from the bag
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
`efx.beginRenderTarget(rt)` SHALL redirect all subsequently recorded draws
into `rt` until `efx.endRenderTarget()`; `endRenderTarget` SHALL return
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

### Requirement: Display-list segmentation
Every recorded draw SHALL carry its rendering target (the default target or
a RenderTarget). Records between a begin/end pair form a **segment** — a
maximal run of consecutive records sharing one target. The renderer's
reordering freedom (the `2d-layer` display-list requirement) SHALL apply
only within a segment; segments SHALL play back in the order their first
record was recorded. A record that samples a RenderTarget SHALL observe
the completed contents of that target's earlier segments, never a
partially rendered target. The begin/end calls are themselves records in
the list and honor record order.

#### Scenario: Reordering stays within a segment
- **WHEN** two overlapping quads are recorded into a target and the renderer reorders records for efficiency
- **THEN** painter's order between them is preserved exactly as on the default target

#### Scenario: Sampling sees completed contents
- **WHEN** a script records begin/draw/end for a target and then records a screen draw sampling that target
- **THEN** playback renders the target's segment first and the sampling draw shows its full contents

### Requirement: Sampling render targets as textures
A live RenderTarget SHALL be accepted wherever a live Texture is accepted:
`drawQuad`'s `texture` argument (with `sourceRect` and size derivation
evaluated against the target's `width`/`height`), per-channel material
`map`s, and the material-level `alphaMask` — with identical validation and
error behavior (a destroyed target throws `TypeError`). Recording a draw
that samples the target that is currently active for recording SHALL throw
`TypeError` and record nothing (a target is never sampled while attached).
The engine SHALL reference render targets by handle in records (ADR 0019)
and SHALL expose **no** alias object — there is no `rt.texture` property
and no API that returns a Texture for a target. Sampled content SHALL
render upright on every target backend; origin conventions are engine-owned
(the ADR 0025 discipline — no script-visible flipping).

#### Scenario: Target used as a material map
- **WHEN** a material's `diffuse.map` is set to a live RenderTarget rendered with known content and the mesh is drawn
- **THEN** the surface shades with the target's sampled texels, and unbinding the map releases the retained target per the lighting retention rule

#### Scenario: Self-sampling throws
- **WHEN** a draw recorded inside an active segment names that segment's own target as its texture
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Same validation as textures
- **WHEN** a destroyed RenderTarget is passed to `drawQuad` or as a material `map`
- **THEN** the call throws `TypeError` exactly as a destroyed Texture would
