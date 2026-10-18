# Spec Delta

## MODIFIED Requirements

### Requirement: ImageData helper is a pure-JS convenience

`efx.noise.imageData(w, h, opts?)` SHALL build an `EfxImageData` of `w*h`
noise pixels by composing the bulk fill, a value→byte remap, and the
existing `createImageData` — adding no new binding entry point for the
helper itself. The remap SHALL be driven by a `domain` option (two
numbers, default `[-1, 1]`): raw values are mapped linearly from `domain`
onto the full `[0, 255]` byte range and clamped at both ends. The helper
SHALL accept an `alpha` flag (boolean, default `false`): when set, the
same normalized bytes SHALL be written to the alpha channel with RGB left
white (`255, 255, 255`), instead of grayscale RGB with opaque alpha; the
normalized bytes SHALL be identical in both modes for the same options. A
non-boolean `alpha` SHALL be rejected with the canonical validation error,
and the default mode SHALL remain byte-for-byte what it produced before
this option existed.

#### Scenario: Default domain maps symmetric range
- **WHEN** a script calls `efx.noise.imageData(w, h)` without `domain`
- **THEN** values in `[-1, 1]` map linearly onto `[0, 255]` grayscale with
  out-of-range values clamped

#### Scenario: Explicit domain fits the noise type
- **WHEN** a script requests `'cellular'` noise with the cellular-centered
  domain (e.g. `domain: [-1, 0.15]`)
- **THEN** the raw range in actual use maps onto the full byte range instead
  of being rendered half-dark by the `[-1, 1]` default

#### Scenario: No new native for the helper
- **WHEN** a reviewer checks the binding surface
- **THEN** the helper composes only the bulk fill and the existing image
  creation entry points

#### Scenario: Alpha mode writes noise into the alpha channel
- **WHEN** a script calls `efx.noise.imageData(w, h, { alpha: true })` with
  the same options as a grayscale call
- **THEN** every pixel is white (`R = G = B = 255`) and its alpha byte equals
  the grayscale byte that call would have produced, so the image renders as
  a white texture whose opacity follows the noise

#### Scenario: Alpha flag is strictly validated
- **WHEN** a script passes `alpha` that is not a boolean (e.g. `alpha: 'yes'`
  or `alpha: 1`), or an unknown option key alongside it
- **THEN** the call throws the canonical validation error and no image is
  created

#### Scenario: Default behavior is unchanged
- **WHEN** a script omits `alpha` or passes `alpha: false`
- **THEN** the returned image is byte-for-byte identical to the grayscale
  image the helper produced before the option existed (RGB = normalized
  value, alpha = 255)
