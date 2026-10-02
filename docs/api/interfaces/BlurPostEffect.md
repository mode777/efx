[**EFX API**](../README.md)

***

[EFX API](../README.md) / BlurPostEffect

# Interface: BlurPostEffect

A separable gaussian blur post effect.

## Properties

### effect

> **effect**: `"blur"`

Discriminator selecting the blur effect.

***

### mix?

> `optional` **mix?**: `number`

Input/output blend in `0..1` (default 1).

***

### radius?

> `optional` **radius?**: `number`

Blur radius in scene pixels (finite, > 0 and <= 64; default 1).
