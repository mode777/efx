# Spec Delta

## MODIFIED Requirements

### Requirement: Render target resources
`efx.graphics.createRenderTarget(width, height)` SHALL create a GPU RenderTarget from two
required positional integers `width` and `height`. Both are required positive
integers; the
documented hard maximum for either is **4096**. A non-integer,
non-positive, or oversized value SHALL throw `RangeError`; a missing value
or a wrongly-typed value SHALL throw `TypeError`; on throw nothing SHALL be
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
- **WHEN** a script calls `createRenderTarget(512, 256)`, reads `width` and `height`, calls `destroy()`, then reads either again
- **THEN** the reads return 512 and 256, and the post-destroy reads throw `TypeError`

#### Scenario: Invalid size throws
- **WHEN** `createRenderTarget` is called with `width: 0`, a negative or non-integer size, a size above 4096, or a missing argument
- **THEN** the call throws `RangeError` / `TypeError` respectively and no target exists afterwards

#### Scenario: Destroy defers while sampled
- **WHEN** a script records a `drawQuad` sampling a target, then calls `destroy()` on that target in the same frame
- **THEN** the frame renders with the target's contents, the native release happens after playback, and further use of the target throws
