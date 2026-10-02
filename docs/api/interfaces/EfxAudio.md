[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxAudio

# Interface: EfxAudio

Audio playback. The engine owns all mixing: scripts never see channels,
buses, or buffers. Two source kinds are loaded separately from playback:
`AudioData` holds fully-decoded PCM and can back many overlapping playheads;
`AudioStream` decodes a long resource incrementally. WAV and MP3 resources
are supported; only decoded PCM is played (no sequenced/modular formats).
All volume control is per-handle plus the single master `volume`; fades are
plain handle writes.

## Properties

### volume

> **volume**: `number`

Master output gain applied to all playback. Setting a negative value throws `RangeError`.

## Methods

### loadAudioData()

> **loadAudioData**(`path`): [`EfxAudioData`](EfxAudioData.md)

Decode a WAV or MP3 resource into fully-decoded PCM.

#### Parameters

##### path

`string`

Root-relative resource path.

#### Returns

[`EfxAudioData`](EfxAudioData.md)

The decoded data; throws `Error` when it cannot be read or decoded, `TypeError` for a non-string path.

***

### loadAudioStream()

> **loadAudioStream**(`path`): [`EfxAudioStream`](EfxAudioStream.md)

Open a WAV or MP3 resource as a streamed source.

#### Parameters

##### path

`string`

Root-relative resource path.

#### Returns

[`EfxAudioStream`](EfxAudioStream.md)

The streamed source; throws `Error` when it cannot be read or decoded, `TypeError` for a non-string path.

***

### playAudio()

> **playAudio**(`source`, `opts?`): [`EfxAudioHandle`](EfxAudioHandle.md) \| `null`

Start an `AudioData` or `AudioStream` playback.

#### Parameters

##### source

[`EfxAudioData`](EfxAudioData.md) \| [`EfxAudioStream`](EfxAudioStream.md)

Data from `loadAudioData` or a stream from `loadAudioStream`.

##### opts?

[`PlayAudioOptions`](PlayAudioOptions.md)

Initial volume, pan, pitch, and loop values.

#### Returns

[`EfxAudioHandle`](EfxAudioHandle.md) \| `null`

The playing handle, or `null` when no voice is available (the playback bank and streaming cap are full, or no device).

***

### resume()

> **resume**(): `void`

Unlock/resume audio after a user gesture (web autoplay); a no-op on desktop.

#### Returns

`void`
