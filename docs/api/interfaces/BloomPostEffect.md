[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / BloomPostEffect

# Interface: BloomPostEffect

A bloom post effect.

## Properties

### effect

> **effect**: `"bloom"`

Discriminator selecting the bloom effect.

***

### mix?

> `optional` **mix?**: `number`

Input/output blend in `0..1` (default 1).

***

### strength?

> `optional` **strength?**: `number`

Additive contribution of the bloom (finite, `0..1`; default 0.5).

***

### threshold?

> `optional` **threshold?**: `number`

Luminance below which texels contribute nothing (finite, `0..1`; default 0.8).
