[**EFX API**](../README.md)

***

[EFX API](../README.md) / ColorFilterPostEffect

# Interface: ColorFilterPostEffect

A color-filter post effect (identity with all defaults).

## Properties

### brightness?

> `optional` **brightness?**: `number`

Multiplies color (finite, >= 0; default 1).

***

### contrast?

> `optional` **contrast?**: `number`

Contrast pivoting at 0.5 grey (finite, >= 0; default 1).

***

### effect

> **effect**: `"colorFilter"`

Discriminator selecting the color-filter effect.

***

### mix?

> `optional` **mix?**: `number`

Input/output blend in `0..1` (default 1).

***

### saturation?

> `optional` **saturation?**: `number`

Saturation; 0 is fully grey, 1 is unchanged (finite, >= 0; default 1).

***

### tint?

> `optional` **tint?**: [`Color`](../type-aliases/Color.md)

RGB multiplier (alpha ignored); four finite components in `0..1`.
