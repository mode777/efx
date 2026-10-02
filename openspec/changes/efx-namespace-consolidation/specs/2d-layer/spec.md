# Spec Delta

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
