[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxVideoNamespace

# Interface: EfxVideoNamespace

The `efx.video` sub-namespace: MPEG-1 video playback.

## Methods

### openVideo()

> **openVideo**(`path`, `opts?`): [`EfxVideo`](EfxVideo.md)

Open an MPEG-1 resource from the resource root.

#### Parameters

##### path

`string`

Resource path to a `.mpg` MPEG-1 program stream.

##### opts?

Optional initial `loop`/`muted` flags.

###### loop?

`boolean`

Loop at the end instead of stopping (default `false`).

###### muted?

`boolean`

Start with audio muted (default `false`).

#### Returns

[`EfxVideo`](EfxVideo.md)

A paused Video at time 0.

#### Throws

`Error` when the resource is missing or is not parseable MPEG-1;
  `TypeError` for a bad path or options.
