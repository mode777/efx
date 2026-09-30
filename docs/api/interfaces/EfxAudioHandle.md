[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxAudioHandle

# Interface: EfxAudioHandle

One playing audio handle (opaque native-backed class).

## Properties

### loop

> **loop**: `boolean`

Loop the source until stopped.

***

### pan

> **pan**: `number`

Stereo pan in `[-1, 1]`; out-of-range values are clamped by the mixer.

***

### paused

> `readonly` **paused**: `boolean`

Whether this handle has been explicitly paused.

***

### pitch

> **pitch**: `number`

Playback-rate multiplier; setting a non-positive value throws `RangeError`.

***

### playing

> `readonly` **playing**: `boolean`

Whether this handle is currently audible (false when paused, ended, stolen, or before web unlock).

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

### pause()

> **pause**(): `void`

Pause this playback; `playing` becomes `false` and it can be resumed.

#### Returns

`void`

***

### resume()

> **resume**(): `void`

Resume a paused playback.

#### Returns

`void`

***

### stop()

> **stop**(): `void`

Stop this playback immediately (it cannot be resumed afterwards).

#### Returns

`void`
