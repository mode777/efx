# Spec Delta

## MODIFIED Requirements

### Requirement: Fixed glyph atlas baking

The script API SHALL provide `efx.graphics.createFont(fontData, size, opts?)`, a C-implemented
function that bakes a **fixed** glyph atlas from a live `FontData` at a
required positional pixel `size` (a positive number) and returns
an opaque, native-backed `Font`. The trailing `opts` bag is optional and
SHALL accept `glyphs` (the set of codepoints to bake), `padding`
(atlas gutter in pixels, default 1), `filter` (`'linear'` default or
`'nearest'`), and the optional baked **effects** `outline`
(`{ width }`) and `shadow` (`{ blur, offset }`). The baked glyph set SHALL
default to the printable **Latin-1** range (U+0020–U+007E and U+00A0–U+00FF)
when `glyphs` is omitted. The atlas SHALL be baked once at creation and SHALL
NOT grow afterwards; a request whose glyphs do not fit SHALL fail without
returning a `Font`. A missing required `size`, an unknown field, or a
wrongly-typed value SHALL throw `TypeError`; an out-of-range numeric value
(non-positive `size`, negative `padding`, non-positive outline `width` or
shadow `blur`) SHALL throw `RangeError`. The atlas is an RGBA8 texture in
which RGB is white and the alpha channel carries per-variant coverage, so the
glyphs are tinted by the draw color. `Font` SHALL expose `destroy()`
(idempotent; using it after `destroy()` throws) and the read-only query
properties `size`, `lineHeight`, `ascent`, and `descent`; reading a query
property on a destroyed `Font` SHALL throw `TypeError`.

#### Scenario: A font bakes from font data
- **WHEN** a script calls `efx.graphics.createFont(fontData, 32)` with a valid `FontData`
- **THEN** it receives a live `Font` whose `size` is 32 and whose `lineHeight` is positive

#### Scenario: Default charset is Latin-1
- **WHEN** a script creates a font without `glyphs` and draws text containing ASCII and Latin-1 letters (e.g. `é`)
- **THEN** both render using baked glyphs

#### Scenario: Custom charset is honored
- **WHEN** a script creates a font with `glyphs: 'ABC'` and draws text containing `D`
- **THEN** `D` has no baked glyph and renders the documented fallback (the baked `?` glyph when present, otherwise nothing with zero advance)

#### Scenario: Baked outline and shadow render as layers
- **WHEN** a script creates a font with `outline: { width: 2 }` and `shadow: { blur: 3, offset: [2, 2] }` and draws text
- **THEN** each glyph is drawn as a shadow layer, then an outline layer, then the fill, each with its own color

#### Scenario: The atlas is fixed at creation
- **WHEN** a script draws a codepoint that was not in the baked set
- **THEN** the atlas is not grown or re-baked; the codepoint uses the fallback behavior

#### Scenario: Invalid creation options are rejected
- **WHEN** `createFont` receives an unknown bag option, a missing or non-number `size`, a `size` of `0`, or a negative `padding`
- **THEN** the call throws (`TypeError` for unknown/missing/wrong type, `RangeError` for the out-of-range number) and returns no `Font`
