[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxNoise

# Interface: EfxNoise

Deterministic procedural noise. Pure functions of a seed, coordinates,
and options: no generator objects, no state, and bit-identical values on
every target platform. Values pass through raw per type — approximately
`[-1, 1]` for `'opensimplex2'`, `'opensimplex2s'`, `'perlin'`,
`'valuecubic'`, and `'value'`; `'cellular'` is the nearest-feature
distance in the engine's centered convention (approximately
`[-1, 0.5]`).

## Example

```js
// terrain heights from fBm simplex, plus a matching grayscale minimap
const heights = new Float32Array(129 * 129);
const opts = { seed: 7, frequency: 0.01, octaves: 5 };
efx.noise.fill2(heights, 129, 129, opts);
const minimap = efx.graphics.createTexture(
  efx.noise.imageData(129, 129, opts));
```

## Methods

### fill2()

> **fill2**(`buf`, `w`, `h`, `opts?`): `void`

Fill a buffer with a 2D noise grid in one call: value `(col, row)`
lands at `buf[row * w + col]` and equals `noise2(col, row, opts)`.
Octave stacking and warping happen inside the engine.

#### Parameters

##### buf

`Float32Array`

Destination `Float32Array` with at least `w*h` elements.

##### w

`number`

Grid width (positive integer).

##### h

`number`

Grid height (positive integer).

##### opts?

Optional seed, algorithm, fractal, and warp options.

###### frequency?

`number`

Coordinate scale in cycles per world unit (positive; default `1`).

###### gain?

`number`

Amplitude falloff between octaves in `(0, 1]` (default `0.5`).

###### lacunarity?

`number`

Frequency multiplier between octaves (positive; default `2`).

###### octaves?

`number`

Stacked fBm octaves for values `> 1` (integer `1..16`; default `1`).

###### seed?

`number`

Seed selecting an independent value channel (integer `0..4294967295`; default `0`).

###### type?

[`NoiseType`](../type-aliases/NoiseType.md)

Noise algorithm (default `'opensimplex2'`).

###### warp?

[`NoiseWarp`](../type-aliases/NoiseWarp.md)

Domain warp applied before sampling (default `'none'`).

#### Returns

`void`

***

### imageData()

> **imageData**(`w`, `h`, `opts?`): [`EfxImageData`](EfxImageData.md)

Build a grayscale `ImageData` of `w*h` noise pixels in one call:
the raw values are mapped linearly from `domain` onto `[0, 255]`
(clamped). Pure JS over `fill2` and `createImageData`.

#### Parameters

##### w

`number`

Image width in pixels (positive integer).

##### h

`number`

Image height in pixels (positive integer).

##### opts?

Noise options plus the `domain` mapping.

###### domain?

\[`number`, `number`\]

Raw value range `[lo, hi]` mapped linearly onto `[0, 255]` grayscale
and clamped at both ends (default `[-1, 1]`). Cellular noise uses the
engine's centered distance convention (approximately `[-1, 0.5]`), so
pass a matching domain for a full-contrast image.

###### frequency?

`number`

Coordinate scale in cycles per world unit (positive; default `1`).

###### gain?

`number`

Amplitude falloff between octaves in `(0, 1]` (default `0.5`).

###### lacunarity?

`number`

Frequency multiplier between octaves (positive; default `2`).

###### octaves?

`number`

Stacked fBm octaves for values `> 1` (integer `1..16`; default `1`).

###### seed?

`number`

Seed selecting an independent value channel (integer `0..4294967295`; default `0`).

###### type?

[`NoiseType`](../type-aliases/NoiseType.md)

Noise algorithm (default `'opensimplex2'`).

###### warp?

[`NoiseWarp`](../type-aliases/NoiseWarp.md)

Domain warp applied before sampling (default `'none'`).

#### Returns

[`EfxImageData`](EfxImageData.md)

The new ImageData; upload it with `graphics.createTexture`.

***

### noise2()

> **noise2**(`x`, `y`, `opts?`): `number`

Sample 2D noise at a point.

#### Parameters

##### x

`number`

X coordinate in world units.

##### y

`number`

Y coordinate in world units.

##### opts?

Optional seed, algorithm, fractal, and warp options.

###### frequency?

`number`

Coordinate scale in cycles per world unit (positive; default `1`).

###### gain?

`number`

Amplitude falloff between octaves in `(0, 1]` (default `0.5`).

###### lacunarity?

`number`

Frequency multiplier between octaves (positive; default `2`).

###### octaves?

`number`

Stacked fBm octaves for values `> 1` (integer `1..16`; default `1`).

###### seed?

`number`

Seed selecting an independent value channel (integer `0..4294967295`; default `0`).

###### type?

[`NoiseType`](../type-aliases/NoiseType.md)

Noise algorithm (default `'opensimplex2'`).

###### warp?

[`NoiseWarp`](../type-aliases/NoiseWarp.md)

Domain warp applied before sampling (default `'none'`).

#### Returns

`number`

The raw generator value in the type's documented range.

***

### noise3()

> **noise3**(`x`, `y`, `z`, `opts?`): `number`

Sample 3D noise at a point (same options and ranges as `noise2`).

#### Parameters

##### x

`number`

X coordinate in world units.

##### y

`number`

Y coordinate in world units.

##### z

`number`

Z coordinate in world units.

##### opts?

Optional seed, algorithm, fractal, and warp options.

###### frequency?

`number`

Coordinate scale in cycles per world unit (positive; default `1`).

###### gain?

`number`

Amplitude falloff between octaves in `(0, 1]` (default `0.5`).

###### lacunarity?

`number`

Frequency multiplier between octaves (positive; default `2`).

###### octaves?

`number`

Stacked fBm octaves for values `> 1` (integer `1..16`; default `1`).

###### seed?

`number`

Seed selecting an independent value channel (integer `0..4294967295`; default `0`).

###### type?

[`NoiseType`](../type-aliases/NoiseType.md)

Noise algorithm (default `'opensimplex2'`).

###### warp?

[`NoiseWarp`](../type-aliases/NoiseWarp.md)

Domain warp applied before sampling (default `'none'`).

#### Returns

`number`

The raw generator value in the type's documented range.
