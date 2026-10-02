[**EFX API**](../README.md)

***

[EFX API](../README.md) / PlayAudioOptions

# Interface: PlayAudioOptions

Options for `audio.playAudio`; these set initial values only.

## Properties

### loop?

> `optional` **loop?**: `boolean`

Loop until stopped (default `false`).

***

### pan?

> `optional` **pan?**: `number`

Initial stereo pan in `[-1, 1]` (default `0` = center).

***

### pitch?

> `optional` **pitch?**: `number`

Initial playback-rate multiplier (default `1`); values `<= 0` are treated as `1`.

***

### volume?

> `optional` **volume?**: `number`

Initial linear gain (default `1`); a negative value throws `RangeError`.
