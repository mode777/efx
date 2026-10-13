# Spec Delta: 2d-layer

## MODIFIED Requirements

### Requirement: Quad drawing
`drawQuad(texture, x, y, opts?)` SHALL record one textured quad, with the
sampled `texture` leading the argument list. `x` and `y` SHALL place the
quad's top-left corner in frame pixels. The `texture`
argument SHALL be required and MUST be a live Texture or a live
RenderTarget (F5a) — passing nothing, a value that is neither, or a
destroyed Texture or RenderTarget SHALL throw `TypeError`.

The quad's size SHALL be determined in frame pixels by the first match of:
`opts.size` — a `[width, height]` array of finite numbers, both required;
else the drawn texture region's extent when `opts.sourceRect` is given;
else the texture's pixel size (for a RenderTarget, its `width`/`height`).
A `size` entry that is non-numeric or infinite SHALL throw (`TypeError` /
`RangeError` respectively) and record nothing. A `size` entry that is
finite but ≤ 0 SHALL be a **degenerate draw**: the call SHALL record
nothing, SHALL NOT throw, and SHALL leave all render state (blend mode,
camera, clear color) untouched — callers can draw health bars and meters
that shrink to zero without guarding. A `sourceRect` of zero extent
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
- **WHEN** `drawQuad` is called with `size: [10]`, `size: 'big'`, or `origin: [NaN, 0]`
- **THEN** the call throws (`RangeError` for out-of-range numbers, `TypeError` for wrong types) and records nothing

#### Scenario: Degenerate size records nothing
- **WHEN** `drawQuad` is called with `size: [0, 10]`, `size: [12, 0]`, or `size: [-4, 10]`, between two valid draws
- **THEN** no error is thrown, no quad is recorded for those calls, and the frame is identical to one where the calls were absent

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
