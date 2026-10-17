# noise

## Purpose

Lets scripts generate deterministic procedural noise — point queries, bulk
fills, and grayscale `ImageData` textures — with identical output on every
target platform, so terrain, textures, and motion variation need no
per-game noise code and never gamble on script-side float behavior.

## Requirements

### Requirement: Noise is stateless and deterministic

The engine SHALL expose procedural noise as pure functions of a seed,
coordinates, and options: the noise surface SHALL contain only plain
functions (no generator objects, no resource lifecycle, no script-visible
engine state), and identical arguments SHALL produce identical values on
every target platform (Windows, Linux, macOS, web). A golden scene
exercising the fill path with multiple noise types, fractal octaves,
domain warp, and distinct seeds SHALL gate this contract on all four
targets under the golden-image harness (ADR 0020).

#### Scenario: No resource lifecycle
- **WHEN** a script inspects `efx.noise`
- **THEN** it finds only plain functions and constants — no constructible
  class, no `destroy()`, and nothing to release

#### Scenario: Cross-target determinism
- **WHEN** the noise golden scene runs on all four targets
- **THEN** the rendered output matches the committed goldens, proving the
  underlying noise values agree bit-for-bit across desktop and web runtimes

#### Scenario: Seeds select independent channels
- **WHEN** two queries differ only in their `seed` option
- **THEN** the returned value streams are independent (neither is an offset
  or repeat of the other)

#### Scenario: Pure function of inputs
- **WHEN** the same query runs twice in one session, or across sessions
- **THEN** both calls return the same value — no generator advance, no
  per-frame drift, no ambient engine state

### Requirement: Point queries return raw generator values

`efx.noise.noise2(x, y, opts?)` and
`efx.noise.noise3(x, y, z, opts?)` SHALL return the raw, unnormalized
value of the requested noise type at the given coordinates. Value ranges
SHALL pass through per type and be documented in the API reference:
`'opensimplex2'`, `'opensimplex2s'`, `'perlin'`, `'value'`, and
`'valuecubic'` return values in approximately `[-1, 1]`; `'cellular'`
returns the distance to the nearest feature point in the engine's
centered convention — distance minus one cell unit — so it lands in
approximately `[-1, 0.5]` (zero at the cell edge, most negative near
feature points). The optional bag SHALL accept `seed` (integer, default
`0`), `type` (default `'opensimplex2'`), `frequency` (default `1`),
`octaves` (default `1`), `lacunarity` (default `2`), `gain` (default
`0.5`), and `warp` (`'none'`, `'opensimplex2'`, or `'gradient'`;
default `'none'`).

#### Scenario: Raw passthrough without normalization
- **WHEN** a script queries a single octave of `'perlin'` or `'cellular'`
- **THEN** the value lands in that type's documented range and is returned
  exactly as the generator produced it — no remap to a common range

#### Scenario: Fractal stacking in one call
- **WHEN** a query sets `octaves` above `1`
- **THEN** the call returns the stacked fractal result without the script
  issuing per-octave queries

#### Scenario: Defaults apply when the bag is omitted
- **WHEN** a script calls `efx.noise.noise2(0.5, 0.5)` with no options
- **THEN** the call succeeds using the documented defaults and returns the
  same value as an explicit bag carrying exactly those defaults

#### Scenario: Invalid options are rejected
- **WHEN** a script passes an unknown `type`, a non-integer `seed`, a
  negative or non-integer `octaves`, or a non-positive `frequency`
- **THEN** the call throws the canonical validation error for that field

#### Scenario: 3D query mirrors 2D
- **WHEN** a script calls `efx.noise.noise3(x, y, z, opts)`
- **THEN** it behaves exactly like `noise2` with one more coordinate

### Requirement: Bulk fill generates into a Float32Array

`efx.noise.fill2(buf, w, h, opts?)` SHALL write exactly `w*h` values,
row-major, into the caller-provided `Float32Array` in one native call —
per-sample work and octave stacking SHALL happen inside the engine, not
across the script boundary. The values SHALL be the same raw values the
point queries produce for the same options. A buffer smaller than `w*h`
SHALL be rejected with the canonical validation error and left untouched.

#### Scenario: Fills the full extent row-major
- **WHEN** a script calls `efx.noise.fill2(buf, 64, 64, opts)` with a
  4096-element buffer
- **THEN** all 4096 elements are overwritten, value `(col, row)` at index
  `row * 64 + col`, each matching `noise2(col, row, opts)`

#### Scenario: Undersized buffer is rejected
- **WHEN** a script calls `efx.noise.fill2` with a buffer shorter than
  `w*h`, or with a non-`Float32Array`
- **THEN** the call throws the canonical validation error and the buffer
  contents are unchanged

#### Scenario: Fill matches point queries
- **WHEN** a script fills a buffer and compares it element-wise against
  per-point `noise2` calls with the same options
- **THEN** every element is identical

### Requirement: ImageData helper is a pure-JS convenience

`efx.noise.imageData(w, h, opts?)` SHALL build an `EfxImageData` of `w*h`
grayscale pixels by composing the bulk fill, a value→byte remap, and the
existing `createImageData` — adding no new binding entry point for the
helper itself. The remap SHALL be driven by a `domain` option (two
numbers, default `[-1, 1]`): raw values are mapped linearly from `domain`
onto the full `[0, 255]` byte range and clamped at both ends.

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
