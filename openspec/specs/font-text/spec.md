# font-text

## Purpose

TrueType/OpenType font loading and deterministic C-side typesetting: baking a
fixed glyph atlas from a font resource and drawing formatted (wrapped and
aligned) 2D text as display-list quads, with measurement.

## Requirements

### Requirement: Font data loading

The script API SHALL provide `efx.loadFontData(path)`, a C-implemented,
synchronous loader that reads a TrueType/OpenType font (`.ttf`/`.otf`) from
the resource root by relative path and returns an opaque, native-backed
`FontData` resource carrying the parsed font (no GPU resource). Loading SHALL
be synchronous from the script's point of view on every target. A missing,
unreadable, or unparsable font SHALL throw a standard ES6 `Error`; a
non-string path SHALL throw `TypeError`. `FontData` SHALL expose `destroy()`
(idempotent; using it after `destroy()` throws) and no read-only query
properties; it is released by `destroy()` with the GC-finalizer backstop
(ADR 0011/0012). One `FontData` SHALL support creating multiple `Font`
resources.

#### Scenario: Font file loads into FontData
- **WHEN** a script calls `efx.loadFontData('fonts/perfect.ttf')` for a font in the resource root
- **THEN** it receives a live `FontData` that can be passed to `efx.createFont`, and `destroy()` on it succeeds and is idempotent

#### Scenario: Missing or malformed font throws
- **WHEN** a script loads a path that does not exist or is not a parsable font
- **THEN** the call throws an `Error` and no `FontData` is returned

#### Scenario: Malformed argument throws TypeError
- **WHEN** a script passes a non-string path to `loadFontData`
- **THEN** the call throws `TypeError`

### Requirement: Fixed glyph atlas baking

The script API SHALL provide `efx.createFont(fontData, opts)`, a C-implemented
function that bakes a **fixed** glyph atlas from a live `FontData` and returns
an opaque, native-backed `Font`. `opts` SHALL require `size` (a positive pixel
size) and SHALL accept `glyphs` (the set of codepoints to bake), `padding`
(atlas gutter in pixels, default 1), `filter` (`'linear'` default or
`'nearest'`), and the optional baked **effects** `outline`
(`{ width }`) and `shadow` (`{ blur, offset }`). The baked glyph set SHALL
default to the printable **Latin-1** range (U+0020–U+007E and U+00A0–U+00FF)
when `glyphs` is omitted. The atlas SHALL be baked once at creation and SHALL
NOT grow afterwards; a request whose glyphs do not fit SHALL fail without
returning a `Font`. A missing required field, an unknown field, or a
wrongly-typed value SHALL throw `TypeError`; an out-of-range numeric value
(non-positive `size`, negative `padding`, non-positive outline `width` or
shadow `blur`) SHALL throw `RangeError`. The atlas is an RGBA8 texture in
which RGB is white and the alpha channel carries per-variant coverage, so the
glyphs are tinted by the draw color. `Font` SHALL expose `destroy()`
(idempotent; using it after `destroy()` throws) and the read-only query
properties `size`, `lineHeight`, `ascent`, and `descent`; reading a query
property on a destroyed `Font` SHALL throw `TypeError`.

#### Scenario: A font bakes from font data
- **WHEN** a script calls `efx.createFont(fontData, { size: 32 })` with a valid `FontData`
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
- **WHEN** `createFont` receives an unknown option, a missing `size`, a `size` of `0`, or a negative `padding`
- **THEN** the call throws (`TypeError` for unknown/missing/wrong type, `RangeError` for the out-of-range number) and returns no `Font`

### Requirement: Typesetting and word wrapping

`efx.drawText` and `efx.measureText` SHALL lay out the input string
deterministically using the font's baked metrics and kerning. A newline
character (`\n`) SHALL start a new line. When a wrap `width` is supplied, the
layout SHALL greedily break lines at whitespace so that a line's width does
not exceed `width`; a single word longer than `width` SHALL be broken at a
character boundary rather than overflowing. When no `width` is supplied, the
text SHALL lay out on the lines given by its newlines without wrapping. The
line advance SHALL be `lineHeight` (the font's `lineHeight` by default,
overridable) multiplied by any uniform `scale`.

#### Scenario: Newlines start new lines
- **WHEN** a script draws `"a\nb"`
- **THEN** `a` and `b` are placed on two lines and the reported `lines` is 2

#### Scenario: Greedy wrapping respects the width
- **WHEN** a script draws a multi-word string with a wrap `width` narrower than the whole string
- **THEN** the text wraps at whitespace and no line's width exceeds the wrap width

#### Scenario: An over-long word is broken
- **WHEN** a script draws a single word wider than the wrap `width`
- **THEN** the word is broken across lines rather than overflowing the wrap width

### Requirement: Horizontal and vertical alignment

`efx.drawText` SHALL accept `align` (`'left'` default, `'center'`, `'right'`,
or `'justify'`) and `valign` (`'top'` default, `'middle'`, `'bottom'`).
Horizontal alignment SHALL position each line within the block's width
relative to the anchor `x`; `justify` SHALL distribute the extra space between
words on every line except the last, which SHALL be left-aligned, and SHALL
require a wrap `width` (omitting it SHALL throw `TypeError`). Vertical
alignment SHALL position the whole laid-out block relative to the anchor `y`.
The anchor `x`/`y` SHALL denote the block's left/top for `left`/`top`, its
center for `center`/`middle`, and its right/bottom for `right`/`bottom`.

#### Scenario: Center and right alignment
- **WHEN** a script draws the same string with `align: 'left'`, `'center'`, and `'right'` at the same anchor
- **THEN** the laid-out lines are left-anchored, center-anchored, and right-anchored respectively

#### Scenario: Justify spreads inter-word space, last line left
- **WHEN** a script draws wrapped text with `align: 'justify'` and a `width`
- **THEN** non-last lines fill the wrap width by widening inter-word spacing and the last line is left-aligned

#### Scenario: Justify without a width is rejected
- **WHEN** a script draws with `align: 'justify'` but no `width`
- **THEN** the call throws `TypeError` and draws nothing

#### Scenario: Vertical alignment positions the block
- **WHEN** a script draws a multi-line string with `valign: 'top'`, `'middle'`, and `'bottom'` at the same anchor
- **THEN** the block's top, vertical center, and bottom are placed at the anchor respectively

### Requirement: Opaque 2D text drawing

`efx.drawText(text, font, x, y, opts?)` SHALL be a C-implemented, 2D-only
operation that records the laid-out glyphs as quads into the engine display
list (the same re-orderable list as `drawQuad`), with no script-visible
geometry, glyph table, or shader. It SHALL return the laid-out bounds
`{ width, height, lines }`, where `width` is the widest line, `height` is
`lines × lineHeight × scale`, and `lines` is the line count. `opts` SHALL
accept `align`, `valign`, `width`, `lineHeight`, `color` (fill, default
opaque white), `outlineColor`, `shadowColor`, `rotation` (degrees about the
anchor), and `scale` (uniform, default 1). When outline/shadow were baked,
the glyphs SHALL be recorded in layer order (shadow, then outline, then fill);
otherwise only fill quads are recorded. The font atlas SHALL remain alive
while any recorded text is pending playback. A non-string `text`, a
non-`Font` `font`, or a destroyed `Font` SHALL throw `TypeError`; an unknown
option field SHALL throw `TypeError`.

#### Scenario: Text records quads and returns bounds
- **WHEN** a script draws text and then a frame plays back
- **THEN** the glyphs appear in the current 2D frame and the call returned `{ width, height, lines }` matching the laid-out text

#### Scenario: Effects draw as ordered layers
- **WHEN** text is drawn with a font baked with outline and shadow
- **THEN** the shadow layer is behind the outline layer, which is behind the fill

#### Scenario: Destroying the font after recording is safe
- **WHEN** a script draws text and then calls `destroy()` on the font in the same frame
- **THEN** the recorded text still plays back correctly and later use of the font throws `TypeError`

#### Scenario: Invalid arguments are rejected
- **WHEN** `drawText` receives a non-string `text`, a non-`Font` second argument, or an unknown option
- **THEN** the call throws `TypeError`

### Requirement: Text measurement

`efx.measureText(text, font, opts?)` SHALL compute and return the same
`{ width, height, lines }` bounds as `efx.drawText` for the same `text`,
`font`, and layout options (`align`, `valign`, `width`, `lineHeight`,
`scale`) without recording any draw or otherwise mutating render state. The
measurement SHALL agree with the drawn layout.

#### Scenario: Measurement agrees with drawing
- **WHEN** the same text and options are passed to `measureText` and `drawText`
- **THEN** both return identical bounds

#### Scenario: Measurement does not draw
- **WHEN** a script calls `measureText` and then renders a frame
- **THEN** no glyphs are drawn and the frame is unchanged

### Requirement: Deterministic atlas and layout

For a given font file, `createFont` options, text, and layout options, the
baked atlas pixels, glyph placement, and laid-out bounds SHALL be identical
across all four target platforms, so text golden images are stable under the
golden-image tolerance policy (ADR 0020).

#### Scenario: Cross-target reproducibility
- **WHEN** the same text scene is rendered on Linux, Windows, macOS, and Emscripten
- **THEN** the glyph layout and rendered text pixels match within the golden-image tolerance
