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
extending outside the texture bounds SHALL throw `RangeError`), and `origin`
(a `[px, py]` array of finite numbers — the pivot point for rotation and
scale, expressed in quad-local frame pixels relative to the quad's top-left;
default the determined size's center). The origin offset SHALL NOT itself be
rotated or scaled, and an unrotated, unscaled quad SHALL place its top-left
corner at `(x, y)` regardless of `origin`. Unknown or wrongly-typed option
fields SHALL throw `TypeError`.

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

### Requirement: Image and texture resources
`createImageData(width, height, pixels, opts?)` SHALL build CPU-side
pixel data: `width` and `height` are positive integers and `pixels` is a flat
byte array in RGBA8 order of length exactly
`width × height × 4` (wrong length SHALL throw `RangeError`). The trailing
`opts` bag is optional and SHALL accept `format`
(default `'rgba8'`, the only format in F2). `createTexture(imageData, opts?)`
SHALL upload image data to a GPU Texture — an opaque native-backed class
released by `destroy()` with GC finalizer backstop (ADR 0011/0013); the
ImageData remains valid afterwards. The optional `opts` object SHALL accept
`wrap` (`'repeat'` default, `'clamp'`, or `'mirror'`), `filter`
(`'linear'` default or `'nearest'`), and `mipmaps` (boolean, `false`
default); an unknown field or an unknown/wrongly-typed value SHALL throw
`TypeError`. When `mipmaps` is `true`, the texture SHALL be created with a
full mip chain and minification SHALL select mip levels, using the mipmap
variant of `filter` (`'linear'` → linear-mipmap/trilinear, `'nearest'` →
nearest-mipmap); when `mipmaps` is `false` or omitted, the texture SHALL
have a single level and minification SHALL NOT use mipmaps, preserving the
existing behavior exactly. A live Texture SHALL expose read-only `width` and
`height` properties naming its pixel size; reading either on a destroyed
texture SHALL throw `TypeError`. The engine SHALL expose
`efx.graphics.whiteTexture`, an engine-owned 1×1 opaque-white Texture usable
in any draw: scripts SHALL NOT destroy it — `destroy()` on it SHALL throw
`TypeError` — and it SHALL remain valid for the whole run. The former root
member `efx.whiteTexture` SHALL be removed (hard cut, no alias).

#### Scenario: Image to texture round trip
- **WHEN** a script builds an ImageData of known colors, creates a texture,
  and draws it full-frame
- **THEN** the rendered frame shows exactly those pixel colors

#### Scenario: Texture reports its pixel size
- **WHEN** a script reads `width` and `height` on a Texture created from a
  64×32 ImageData, and on `efx.graphics.whiteTexture`
- **THEN** the values are 64 and 32, and 1 and 1 respectively

#### Scenario: Destroyed texture getters throw
- **WHEN** a script reads `width` or `height` on a Texture after calling
  `destroy()` on it
- **THEN** reading throws `TypeError`

#### Scenario: White texture draws solid rects
- **WHEN** a script draws `efx.graphics.whiteTexture` with `color: [1, 0, 0, 1]`
- **THEN** a solid red rectangle appears, and calling
  `efx.graphics.whiteTexture.destroy()` throws `TypeError`

#### Scenario: Pixel buffer length is validated
- **WHEN** `createImageData` receives a `pixels` array whose length does not
  match `width × height × 4`
- **THEN** the call throws `RangeError`

#### Scenario: Sampler options apply
- **WHEN** a script creates a texture with `{ wrap: 'clamp', filter: 'nearest' }`
- **THEN** the texture samples with clamp wrapping and nearest filtering, and
  omitting `opts` preserves the default repeat/linear sampling

#### Scenario: Mipmaps build a full chain and apply mip filtering
- **WHEN** a script creates a texture with a mip chain (`mipmaps: true`) from
  an ImageData larger than one pixel and draws it minified
- **THEN** the texture has mip levels down to 1×1 and minification blends
  across them, differing from the same draw with `mipmaps` omitted

#### Scenario: Mipmaps default off
- **WHEN** `mipmaps` is omitted or `false`
- **THEN** the texture has a single level and minification never selects a
  minified mip level

#### Scenario: Invalid sampler options throw
- **WHEN** `createTexture` receives an unknown option field, an unsupported
  wrap or filter value, or a non-boolean `mipmaps`
- **THEN** the call throws `TypeError` and creates no texture

#### Scenario: Invalid mipmap option throws
- **WHEN** `createTexture` receives a `mipmaps` value that is present but not a
  boolean
- **THEN** the call throws `TypeError` and creates no texture

### Requirement: Batched 2D sprite drawing

`efx.graphics.drawSprites(texture, sprites)` SHALL record one textured 2D quad per
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
Each recorded sprite SHALL snapshot the 2D camera and blend mode in effect at
the time of the call, and the sprites SHALL play back in entry order,
consistent with the display-list painter's-order contract. Consecutive sprites
sharing a texture and blend mode SHALL render identically to the equivalent
sequence of individual `drawQuad` calls.

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

#### Scenario: Entry order is painter's order

- **WHEN** two overlapping sprites are supplied with the same texture, in a
  known order
- **THEN** the later entry appears on top

#### Scenario: Invalid argument records nothing

- **WHEN** `drawSprites` is called with a non-array `sprites`, with a missing
  or destroyed texture, or with an entry missing `x`/`y` or holding an
  invalid option
- **THEN** the call throws (`TypeError` or `RangeError` as appropriate) and
  records no sprites from the call

#### Scenario: Empty array records nothing

- **WHEN** `drawSprites` is called with an empty array
- **THEN** the call succeeds and records no draws
