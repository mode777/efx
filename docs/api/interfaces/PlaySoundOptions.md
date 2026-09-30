[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / PlaySoundOptions

# Interface: PlaySoundOptions

Options for `audio.playSound` and `audio.playAudioEffect`.

## Properties

### loop?

> `optional` **loop?**: `boolean`

Loop until stopped (default `false`).

***

### pan?

> `optional` **pan?**: `number`

Stereo pan in `[-1, 1]` (default `0` = center).

***

### pitch?

> `optional` **pitch?**: `number`

Playback-rate multiplier (default `1`); values `<= 0` are treated as `1`.

***

### volume?

> `optional` **volume?**: `number`

Linear gain (default `1`); negative values are clamped to `0`.
