# Spec Delta

## MODIFIED Requirements

### Requirement: Typesetting and word wrapping

`efx.graphics.drawText` and `Font.measure` SHALL lay out the input string
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

### Requirement: Text measurement

`Font.measure(text, opts?)` SHALL compute and return the same
`{ width, height, lines }` bounds as `efx.graphics.drawText` for the same
`text`, receiver `font`, and layout options (`align`, `valign`, `width`,
`lineHeight`, `scale`) without recording any draw or otherwise mutating render
state. The measurement SHALL agree with the drawn layout.

#### Scenario: Measurement agrees with drawing
- **WHEN** the same text and options are passed to `Font.measure` and
  `efx.graphics.drawText`
- **THEN** both return identical bounds

#### Scenario: Measurement does not draw
- **WHEN** a script calls `Font.measure` and then renders a frame
- **THEN** no glyphs are drawn and the frame is unchanged

#### Scenario: No free-function measurement remains
- **WHEN** a script reads `efx.graphics.measureText` after this change
- **THEN** it is `undefined`, and measurement is reachable only as the
  `Font.measure` method on a live `Font`
