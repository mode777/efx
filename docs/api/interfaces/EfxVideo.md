[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxVideo

# Interface: EfxVideo

A playing MPEG-1 video (opaque native-backed class).

Decoding and audio are engine-owned: the engine advances the video once
per frame while it plays and uploads each decoded frame to `texture`. The
video's audio plays through the engine mixer unless `muted`; scripts never
manage a voice or channel.

## Example

```js
const vid = efx.video.openVideo('intro.mpg', { loop: true });
vid.play();
function render() { efx.graphics.drawQuad(vid.texture, 0, 0, { size: [640, 480] }); }
```

## Methods

### destroy()

> **destroy**(): `void`

Release the decoder, texture, and audio deterministically and idempotently.

#### Returns

`void`

***

### pause()

> **pause**(): `void`

Hold the current time until `play()` is called again.

#### Returns

`void`

***

### play()

> **play**(): `void`

Start (or resume) playback; after `ended`, restarts from the beginning.

#### Returns

`void`

## Properties

### currentTime

> `readonly` **currentTime**: `number`

Current playback time in seconds. Throws `TypeError` when destroyed.

***

### duration

> `readonly` **duration**: `number`

Total duration in seconds. Throws `TypeError` when destroyed.

***

### ended

> `readonly` **ended**: `boolean`

`true` once a non-looping video has reached its end.

***

### height

> `readonly` **height**: `number`

Frame height in pixels. Throws `TypeError` when destroyed.

***

### loop

> **loop**: `boolean`

Whether the video loops at the end (read-write).

***

### muted

> **muted**: `boolean`

Whether audio is muted (read-write); does not affect decoding.

***

### texture

> `readonly` **texture**: [`EfxTexture`](EfxTexture.md)

The video's current frame as an engine-owned dynamic Texture, usable in
any draw or map slot. Scripts cannot `destroy()` it — it is released
with the video.

***

### width

> `readonly` **width**: `number`

Frame width in pixels. Throws `TypeError` when destroyed.
