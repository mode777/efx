[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxSound

# Interface: EfxSound

One playing sound-effect voice (opaque native-backed class).

## Properties

### pan

> **pan**: `number`

Stereo pan in `[-1, 1]`; out-of-range values are clamped by the mixer.

***

### pitch

> **pitch**: `number`

Playback-rate multiplier; setting a non-positive value throws `RangeError`.

***

### playing

> `readonly` **playing**: `boolean`

Whether this voice is still playing (false once it ends or is stolen).

***

### volume

> **volume**: `number`

Linear gain. Setting a negative value throws `RangeError`.

## Methods

### destroy()

> **destroy**(): `void`

Stop and release the handle deterministically and idempotently.

#### Returns

`void`

***

### stop()

> **stop**(): `void`

Stop this voice immediately.

#### Returns

`void`
