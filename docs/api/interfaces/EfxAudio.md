[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxAudio

# Interface: EfxAudio

Audio playback. The engine owns all mixing: scripts never see channels,
buses, or buffers. WAV and MP3 resources are supported; only decoded PCM is
played (no sequenced/modular formats). One background-music stream is active
at a time; a fixed bank of 32 sound-effect voices is mixed, with a
deterministic steal policy when all are busy.

## Methods

### loadSoundData()

> **loadSoundData**(`path`): [`EfxSoundData`](EfxSoundData.md)

Decode a WAV or MP3 resource into sound data.

#### Parameters

##### path

`string`

Root-relative resource path.

#### Returns

[`EfxSoundData`](EfxSoundData.md)

The decoded sound data; throws `Error` when it cannot be read or decoded, `TypeError` for a non-string path.

***

### playAudioEffect()

> **playAudioEffect**(`path`, `opts?`): [`EfxSound`](EfxSound.md) \| `null`

Load (with path caching) and start a sound effect.

#### Parameters

##### path

`string`

Root-relative resource path.

##### opts?

[`PlaySoundOptions`](PlaySoundOptions.md)

Volume, pan, pitch, and loop options.

#### Returns

[`EfxSound`](EfxSound.md) \| `null`

The playing handle, or `null` when no voice is available.

***

### playBackgroundMusic()

> **playBackgroundMusic**(`path`, `opts?`): [`EfxMusic`](EfxMusic.md)

Start streamed background music, replacing any current track.

#### Parameters

##### path

`string`

Root-relative resource path.

##### opts?

[`PlayMusicOptions`](PlayMusicOptions.md)

Volume and loop options.

#### Returns

[`EfxMusic`](EfxMusic.md)

The music handle; throws `Error` when it cannot be read or decoded.

***

### playSound()

> **playSound**(`sound`, `opts?`): [`EfxSound`](EfxSound.md) \| `null`

Start a decoded sound as a sound-effect voice.

#### Parameters

##### sound

[`EfxSoundData`](EfxSoundData.md)

Sound data from `loadSoundData`.

##### opts?

[`PlaySoundOptions`](PlaySoundOptions.md)

Volume, pan, pitch, and loop options.

#### Returns

[`EfxSound`](EfxSound.md) \| `null`

The playing handle, or `null` when no voice is available (all busy and looping, or no audio device).

***

### resume()

> **resume**(): `void`

Unlock/resume audio after a user gesture (web autoplay); a no-op on desktop.

#### Returns

`void`

***

### stopBackgroundMusic()

> **stopBackgroundMusic**(): `void`

Stop the active background music.

#### Returns

`void`
