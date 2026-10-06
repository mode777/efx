[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxFont

# Interface: EfxFont

A baked glyph atlas plus layout metrics (opaque native-backed class).

## Methods

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`

***

### measure()

> **measure**(`text`, `opts?`): [`TextBounds`](TextBounds.md)

Lay out text against this font without drawing it.

#### Parameters

##### text

`string`

Text to measure.

##### opts?

Optional alignment, wrap, and scale (matching a later draw).

###### align?

`"left"` \| `"center"` \| `"right"` \| `"justify"`

Horizontal alignment (default `'left'`); `'justify'` requires `width`.

###### color?

[`Color`](../type-aliases/Color.md)

Fill color (default opaque white).

###### lineHeight?

`number`

Line advance in pixels; defaults to the font's `lineHeight`.

###### outlineColor?

[`Color`](../type-aliases/Color.md)

Baked-outline color (default black).

###### rotation?

`number`

Rotation in degrees about the anchor (default 0).

###### scale?

`number`

Uniform scale (default 1).

###### shadowColor?

[`Color`](../type-aliases/Color.md)

Baked-shadow color (default black).

###### valign?

`"top"` \| `"middle"` \| `"bottom"`

Vertical alignment relative to `y` (default `'top'`).

###### width?

`number`

Wrap width in pixels; required for `'justify'`.

#### Returns

[`TextBounds`](TextBounds.md)

The laid-out bounds.

## Properties

### ascent

> `readonly` **ascent**: `number`

Distance from the baseline to the top of the em box, in pixels.

***

### descent

> `readonly` **descent**: `number`

Distance from the baseline to the bottom of the em box, in pixels.

***

### lineHeight

> `readonly` **lineHeight**: `number`

Line advance in pixels for the baked size.

***

### size

> `readonly` **size**: `number`

Pixel size the atlas was baked at.
