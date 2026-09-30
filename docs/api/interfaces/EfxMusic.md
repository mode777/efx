[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxMusic

# Interface: EfxMusic

The streamed background-music source (opaque native-backed class).

## Properties

### playing

> `readonly` **playing**: `boolean`

Whether the background music is currently playing.

## Methods

### destroy()

> **destroy**(): `void`

Stop and release the handle deterministically and idempotently.

#### Returns

`void`

***

### pause()

> **pause**(): `void`

Pause the background music.

#### Returns

`void`

***

### resume()

> **resume**(): `void`

Resume paused background music.

#### Returns

`void`

***

### setVolume()

> **setVolume**(`volume`): `void`

Set the linear gain; a negative value throws `RangeError`.

#### Parameters

##### volume

`number`

#### Returns

`void`

***

### stop()

> **stop**(): `void`

Stop the background music.

#### Returns

`void`
