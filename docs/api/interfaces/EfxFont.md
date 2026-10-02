[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxFont

# Interface: EfxFont

A baked glyph atlas plus layout metrics (opaque native-backed class).

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

## Methods

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`
