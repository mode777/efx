# Spec Delta

## MODIFIED Requirements

### Requirement: Quad drawing
`drawQuad(texture, x, y, opts?)` SHALL record one textured quad, with the
sampled `texture` leading the argument list. `x` and `y` SHALL place the
quad's top-left corner in frame pixels. The `texture`
argument SHALL be required and MUST be a live Texture or a live
RenderTarget (F5a) — passing nothing, a value that is neither, or a
destroyed Texture or RenderTarget SHALL throw `TypeError`.

The quad's size SHALL be determined in frame pixels by the first match of:
`opts.size` — a `[width, height]` array of finite numbers, both required and
> 0; else the drawn texture region's extent when `opts.sourceRect` is given;
else the texture's pixel size (for a RenderTarget, its `width`/`height`). A
`size` entry that is non-numeric, infinite, or ≤ 0 SHALL throw (`TypeError` /
`RangeError` respectively) and record nothing. A `sourceRect` of zero extent
in either dimension SHALL throw `RangeError` and record nothing. `opts.scale`
SHALL apply after the size is determined: size is pre-scale frame pixels and
scale multiplies the drawn extents.

Options: `color` (tint `[r, g, b, a]`, default opaque white), `rotation`
(degrees clockwise, default 0), `scale` (uniform positive factor, default 1;
non-finite SHALL throw `TypeError`, ≤ 0 SHALL throw `RangeError`),
`sourceRect` (the texture region drawn, `{ x, y, w, h }` in texture pixels —
for a RenderTarget, target pixels — default the full texture; a region
extending outside the texture bounds SHALL throw `RangeError`), `origin`
(a `[px, py]` array of finite numbers — the pivot point for rotation and
scale, expressed in quad-local frame pixels relative to the quad's top-left;
default the determined size's center), and `blend` (a blend mode string
`'alpha'` | `'additive'` | `'subtractive'`; when present it overrides the
frame's blend render state for this draw only, and when absent the frame
state at record time applies). The origin offset SHALL NOT itself be
rotated or scaled, and an unrotated, unscaled quad SHALL place its top-left
corner at `(x, y)` regardless of `origin`. Unknown or wrongly-typed option
fields SHALL throw `TypeError`; a `blend` that is present but not one of the
three mode strings SHALL throw `TypeError` and record nothing.

#### Scenario: Textured quad with defaults
- **WHEN** a script calls `drawQuad(tex, 0, 0)` for a 128×128 texture with known pixel colors
- **THEN** the full texture is drawn 1:1, tinted white, into the 128×128 frame area at the requested position

#### Scenario: Size derives from the source rect
- **WHEN** `drawQuad(tex, 40, 40, { sourceRect: { x: 0, y: 0, w: 64, h: 32 } })` is called on a 128×128 texture
- **THEN** the named region is drawn 1:1 into a 64×32 frame area at the requested position

#### Scenario: Explicit size overrides derivation
- **WHEN** `opts.size` names a different area than the source region or texture size, e.g. a 64×32 source region drawn with `size: [128, 128]`
- **THEN** the region is stretched to fill the named size, overriding the source-rect and texture-size derivation

#### Scenario: Scale applies after size determination
- **WHEN** a 64×64 texture is drawn with `size: [32, 16]` and `scale: 2`
- **THEN** the drawn quad covers a 64×32 frame area, pivoting on its center

#### Scenario: Source rect selects a texture region
- **WHEN** `sourceRect` names the top-left quarter of the texture
- **THEN** only that region is sampled and drawn, stretched to the destination size

#### Scenario: Out-of-bounds source rect throws
- **WHEN** `sourceRect` extends beyond the texture's pixel dimensions
- **THEN** the call throws `RangeError` and records nothing

#### Scenario: Zero-extent source rect throws
- **WHEN** `sourceRect` has `w` or `h` equal to 0
- **THEN** the call throws `RangeError` and records nothing

#### Scenario: Rotation and scale pivot on the quad center
- **WHEN** a quad is drawn with rotation 90 and scale 2 and no `origin`
- **THEN** the quad's center stays at the center of the determined size's rectangle at `x, y`, and the quad rotates clockwise and doubles in size around it

#### Scenario: Origin moves the pivot
- **WHEN** a quad is drawn with `origin: [0, 0]` and rotation 90
- **THEN** the quad rotates clockwise around its top-left corner, which stays fixed at `(x, y)`

#### Scenario: Origin does not move an untransformed quad
- **WHEN** two identical quads are drawn without rotation or scale, one with and one without `origin`
- **THEN** both render pixel-identically with their top-left corner at `(x, y)`

#### Scenario: Invalid size or origin values throw
- **WHEN** `drawQuad` is called with `size: [0, 10]`, `size: [10]`, `size: 'big'`, or `origin: [NaN, 0]`
- **THEN** the call throws (`RangeError` for out-of-range numbers, `TypeError` for wrong types) and records nothing

#### Scenario: Missing or destroyed texture throws
- **WHEN** `drawQuad` is called with no texture, a value that is neither a Texture nor a RenderTarget, a destroyed Texture, or a destroyed RenderTarget
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Render target drawn like a texture
- **WHEN** a 512×512 RenderTarget rendered with known content is passed to `drawQuad(rt, 0, 0, { sourceRect: { x: 0, y: 0, w: 256, h: 256 } })`
- **THEN** the top-left quarter of the target's content is drawn into a 256×256 frame area, exactly as the same call with a Texture would

#### Scenario: Per-draw blend overrides the frame state
- **WHEN** the frame's blend state is `'alpha'` and a script records
  `drawQuad(tex, x, y, { blend: 'additive' })` followed by
  `drawQuad(tex, x, y)`
- **THEN** the first quad is additive and the second uses the frame state
  (`'alpha'`), and a later `setBlendMode` does not change either

#### Scenario: Invalid blend throws
- **WHEN** `drawQuad` is called with `blend: 'multiply'` or `blend: 1`
- **THEN** the call throws `TypeError` and records nothing

### Requirement: Blending modes
`setBlendMode(mode)` SHALL select how recorded draws that do not carry their
own blend combine with the existing frame content: `'alpha'` (source-over
weighted by source alpha), `'additive'` (destination plus source weighted by
source alpha), and `'subtractive'` (destination minus source weighted by
source alpha, clamped at zero). The selected mode SHALL be the engine's blend
**render state**. The engine SHALL reset the blend render state to `'alpha'`
at the start of every frame, so a mode set at load time does not carry into
later frames; the default at frame start SHALL be `'alpha'`.

The blend render state SHALL apply to every draw record type that does not
specify a per-object blend: 2D quads and sprite batches, 3D mesh surfaces
whose bound material does not specify a blend, and billboards. The state
SHALL be value-snapshotted into each record at record time (ADR 0019), so a
recorded draw MUST NOT observe a later `setBlendMode` change. A draw that
supplies a per-object `blend` override SHALL use that value instead of the
frame state. Particle systems SHALL use their own configured blend when they
have one and otherwise the frame state in effect when `drawParticles` is
recorded. Draws recorded before a `setBlendMode` call MUST NOT be affected.

#### Scenario: Alpha is the default
- **WHEN** a partially transparent quad is drawn without ever calling
  `setBlendMode`
- **THEN** it blends over the background with source-over alpha

#### Scenario: Additive and subtractive modes
- **WHEN** the same quad is drawn with `'additive'` and with
  `'subtractive'` in separate frames
- **THEN** the additive frame is brighter than the alpha frame and the
  subtractive frame is darker, per the equations above

#### Scenario: Mode is value-snapshotted per record
- **WHEN** a script records a quad, switches the blend mode, records another
  quad
- **THEN** playback blends each quad with the mode active at its record time

#### Scenario: The state resets every frame
- **WHEN** a script calls `setBlendMode('additive')` at load time and records
  a quad in a later frame without calling `setBlendMode` again
- **THEN** the quad uses `'alpha'`, because the frame began by resetting the
  blend render state

#### Scenario: Per-object override wins over the frame state
- **WHEN** the frame's blend state is `'additive'` and a draw supplies its own
  `blend` of `'subtractive'`
- **THEN** only that draw is subtractive and other draws in the frame keep the
  frame state

#### Scenario: Invalid mode throws
- **WHEN** `setBlendMode` is called with an unknown mode string, a non-string,
  or no argument
- **THEN** the call throws `TypeError` and the blend render state is unchanged

### Requirement: Batched 2D sprite drawing

`efx.graphics.drawSprites(texture, sprites, opts?)` SHALL record one textured 2D quad per
entry in `sprites`, each exactly equivalent to a `drawQuad(texture, sprite.x,
sprite.y, sprite)` call with the entry's fields. It SHALL be C-implemented
mid-level and 2D-only: sprites SHALL be placed and transformed in the current
2D camera frame and SHALL NOT be oriented in 3D or depth-tested.

`texture` SHALL be required and MUST be a live Texture or RenderTarget —
otherwise the call SHALL throw `TypeError` and record nothing. `sprites` SHALL
be a required array; a non-array value SHALL throw `TypeError`. Each entry
SHALL be an object carrying `x` and `y` (required finite numbers, frame
pixels) plus the `drawQuad` options `size`, `sourceRect`, `color`, `rotation`,
`scale`, and `origin`, with the same types, defaults, and error behavior as
`drawQuad`. If any entry is invalid, the call SHALL throw (`TypeError` or
`RangeError` as appropriate) and SHALL record none of the call's sprites.

The optional trailing `opts` bag SHALL accept only `blend`, a batch-level
blend mode string (`'alpha'` | `'additive'` | `'subtractive'`) applied to
every sprite recorded by the call; an unknown field or an invalid `blend`
value SHALL throw `TypeError` and record none of the call's sprites. Each
recorded sprite SHALL snapshot the 2D camera in effect at the time of the
call and the batch blend: `opts.blend` when supplied, otherwise the frame's
blend render state at call time. There SHALL be no per-entry `blend` field.
The sprites SHALL play back in entry order, consistent with the display-list
painter's-order contract. Consecutive sprites sharing a texture and blend mode
SHALL render identically to the equivalent sequence of individual `drawQuad`
calls.

#### Scenario: Equivalent to individual quad draws

- **WHEN** a script calls `drawSprites(tex, entries)` in one frame and, in
  another frame, calls `drawQuad` once per entry with the same `x`, `y`, and
  options
- **THEN** the two frames are pixel-identical

#### Scenario: Per-sprite options apply

- **WHEN** entries use `sourceRect` atlas regions, `rotation`, `scale`,
  `color`, and `origin`
- **THEN** each sprite is drawn exactly as the corresponding `drawQuad` call
  would draw it

#### Scenario: Batch blend applies to every sprite

- **WHEN** `drawSprites(tex, entries, { blend: 'additive' })` is recorded while
  the frame state is `'alpha'`
- **THEN** every sprite in the call is additive and a subsequent `drawQuad`
  without a `blend` uses the frame state

#### Scenario: Entry order is painter's order

- **WHEN** two overlapping sprites are supplied with the same texture, in a
  known order
- **THEN** the later entry appears on top

#### Scenario: Invalid argument records nothing

- **WHEN** `drawSprites` is called with a non-array `sprites`, with a missing
  or destroyed texture, with an entry missing `x`/`y` or holding an invalid
  option, or with an unknown `opts` field or invalid `blend`
- **THEN** the call throws (`TypeError` or `RangeError` as appropriate) and
  records no sprites from the call

#### Scenario: Empty array records nothing

- **WHEN** `drawSprites` is called with an empty array
- **THEN** the call succeeds and records no draws
