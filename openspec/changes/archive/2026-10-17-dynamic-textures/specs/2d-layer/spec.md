# dynamic-textures Delta

## MODIFIED Requirements

### Requirement: Image and texture resources
`createImageData({ width, height, pixels, format? })` SHALL build CPU-side
pixel data: `pixels` is a flat byte array in RGBA8 order of length exactly
`width × height × 4` (wrong length SHALL throw `RangeError`), and `format`
defaults to `'rgba8'` (the only format in F2). `createTexture(imageData, opts?)`
SHALL upload image data to a GPU Texture — an opaque native-backed class
released by `destroy()` with GC finalizer backstop (ADR 0011/0013); the
ImageData remains valid afterwards. The optional `opts` object SHALL accept
`wrap` (`'repeat'` default, `'clamp'`, or `'mirror'`), `filter`
(`'linear'` default or `'nearest'`), `mipmaps` (boolean, `false` default),
and `dynamic` (boolean, `false` default); an unknown field or an
unknown/wrongly-typed value SHALL throw `TypeError`. When `mipmaps` is
`true`, the texture SHALL be created with a full mip chain and minification
SHALL select mip levels, using the mipmap variant of `filter` (`'linear'` →
linear-mipmap/trilinear, `'nearest'` → nearest-mipmap); when `mipmaps` is
`false` or omitted, the texture SHALL have a single level and minification
SHALL NOT use mipmaps, preserving the existing behavior exactly. When
`dynamic` is `true`, the texture SHALL be created for streaming updates: its
pixels MAY be replaced after creation via `update(imageData)`, which SHALL
accept only an ImageData of exactly the texture's `width` × `height` size and
SHALL replace the full texture contents in one operation. `update` SHALL
throw `TypeError` when the argument is not an ImageData, when its size does
not match the texture, when the texture was not created with `dynamic: true`,
or when the texture has been destroyed. An `update` SHALL be visible to every
draw played after the call: texture bindings resolve at playback time, so a
draw recorded before an `update` SHALL sample the updated pixels when it is
played after it. A dynamic texture SHALL have a single mip level and SHALL
NOT combine `mipmaps: true` with `dynamic: true` (passing both SHALL throw
`TypeError`). In a run mode without a rendering surface, creating a dynamic
texture and calling `update` SHALL be validated and accepted but upload
nothing, like any created resource (ADR 0052). A live Texture SHALL expose
read-only `width` and `height` properties naming its pixel size; reading
either on a destroyed texture SHALL throw `TypeError`. The engine SHALL
expose `efx.graphics.whiteTexture`, an engine-owned 1×1 opaque-white Texture
usable in any draw in every run mode, including run modes without a rendering
surface: scripts SHALL NOT destroy it — `destroy()` on it SHALL throw
`TypeError` — and it SHALL remain valid for the whole run. In a run mode
without a rendering surface the white texture (like any created resource)
SHALL be CPU-only: it reports its size and is accepted by recorded draws, but
nothing is uploaded or rendered. The former root member `efx.whiteTexture`
SHALL be removed (hard cut, no alias).

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

#### Scenario: White texture is available without a rendering surface
- **WHEN** a `--script` run reads `efx.graphics.whiteTexture` at the top level
- **THEN** it receives a 1×1 Texture that can be passed to a recorded draw, and
  `destroy()` on it throws `TypeError`

#### Scenario: Created texture is CPU-only without a surface
- **WHEN** a `--script` run creates a texture from an ImageData
- **THEN** the texture reports its `width` and `height` and is accepted by a
  recorded draw, though nothing is rendered

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

#### Scenario: Dynamic texture shows updated pixels
- **WHEN** a script creates a texture with `dynamic: true` from an ImageData,
  draws it full-frame, then calls `update` with a differently colored
  same-size ImageData and draws it again in the same frame
- **THEN** the first draw samples the original colors and the second draw
  samples the updated colors

#### Scenario: Update requires a matching size
- **WHEN** a script calls `update` on a dynamic texture with an ImageData
  whose width or height differs from the texture
- **THEN** the call throws `TypeError` and the texture keeps its previous
  pixels

#### Scenario: Update on a default texture throws
- **WHEN** a script calls `update` on a texture created without
  `dynamic: true`
- **THEN** the call throws `TypeError` and the texture is unchanged

#### Scenario: Update on a destroyed texture throws
- **WHEN** a script calls `update` on a dynamic texture after `destroy()`
- **THEN** the call throws `TypeError`

#### Scenario: Recorded draws sample the latest pixels at playback
- **WHEN** a script records a draw of a dynamic texture, then updates the
  texture, then the display list plays
- **THEN** the played draw shows the updated pixels, not the pixels current
  when the draw was recorded

#### Scenario: Dynamic and mipmap options are exclusive
- **WHEN** a script creates a texture with both `dynamic: true` and
  `mipmaps: true`
- **THEN** the call throws `TypeError` and creates no texture

#### Scenario: Dynamic texture is CPU-only without a surface
- **WHEN** a `--script` run creates a texture with `dynamic: true` and calls
  `update` on it with a valid same-size ImageData
- **THEN** the call succeeds, size queries still work, and nothing is
  uploaded or rendered
