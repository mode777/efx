[**EFX API**](../README.md)

***

[EFX API](../README.md) / ParticleSystemOptions

# Interface: ParticleSystemOptions

Options for `createParticleSystem` (`texture`, `max`, and `lifetime` are
required).

## Example

```js
// an additive fire (see the "Particle Showcase" sample)
const fire = efx.graphics.createParticleSystem({
  texture: spark,
  max: 600,
  lifetime: [0.4, 0.9],
  emissionRate: 140,
  position: [0, 0.1, 0],
  direction: [0, 1, 0],
  spread: 22,
  speed: [0.8, 1.8],
  gravity: [0, 0.6, 0],
  sizes: [0.55, 0.05],
  colors: [[1, 0.9, 0.45, 0.95], [1, 0.25, 0.05, 0]],
  blend: 'additive',
  facing: 'view',
});
```

## Properties

### blend?

> `optional` **blend?**: [`EfxBlendMode`](../type-aliases/EfxBlendMode.md)

Blend mode (default `'alpha'`).

***

### colors?

> `optional` **colors?**: [`Color`](../type-aliases/Color.md) \| [`Color`](../type-aliases/Color.md)[]

Color over the lifetime: one color or up to 8 interpolated keyframes.

***

### direction?

> `optional` **direction?**: [`Vec3`](../type-aliases/Vec3.md) \| [`Vec2`](../type-aliases/Vec2.md)

Emission direction (2- or 3-component vector).

***

### emissionRate?

> `optional` **emissionRate?**: `number`

Particles emitted per second (default 0).

***

### emissionShape?

> `optional` **emissionShape?**: [`EmissionShapeOptions`](EmissionShapeOptions.md)

Emission volume; defaults to a point.

***

### emitterLifetime?

> `optional` **emitterLifetime?**: `number`

Emitter lifetime in seconds; `-1` is infinite.

***

### facing?

> `optional` **facing?**: [`EfxFacing`](../type-aliases/EfxFacing.md)

Quad render mode in world space (default `'view'`; screen space must be `'view'`).

***

### gravity?

> `optional` **gravity?**: [`Vec3`](../type-aliases/Vec3.md) \| [`Vec2`](../type-aliases/Vec2.md)

Constant acceleration (2- or 3-component vector).

***

### insertMode?

> `optional` **insertMode?**: `"top"` \| `"bottom"` \| `"random"`

Draw order within the batch (default `'top'`).

***

### lifetime

> **lifetime**: `number` \| \[`number`, `number`\]

Particle lifetime in seconds: a number or `[min, max]`; required.

***

### linearAcceleration?

> `optional` **linearAcceleration?**: [`Vec3`](../type-aliases/Vec3.md) \| [`Vec2`](../type-aliases/Vec2.md)

Additional constant acceleration (2- or 3-component vector).

***

### linearDamping?

> `optional` **linearDamping?**: `number` \| \[`number`, `number`\]

Linear damping: a number or `[min, max]`.

***

### max

> **max**: `number`

Maximum live particles (integer, 1..65536); required.

***

### normal?

> `optional` **normal?**: [`Vec3`](../type-aliases/Vec3.md)

Plane orientation normal for `facing: 'plane'` (default `[0, 1, 0]`).

***

### position?

> `optional` **position?**: [`Vec3`](../type-aliases/Vec3.md) \| [`Vec2`](../type-aliases/Vec2.md)

Spawn position (2- or 3-component vector).

***

### quads?

> `optional` **quads?**: [`SourceRect`](SourceRect.md)[]

Atlas frames cycled over the lifetime.

***

### radialAcceleration?

> `optional` **radialAcceleration?**: `number` \| \[`number`, `number`\]

Radial acceleration: a number or `[min, max]`.

***

### relativeRotation?

> `optional` **relativeRotation?**: `boolean`

When true, particle angle follows its velocity.

***

### rotation?

> `optional` **rotation?**: `number` \| \[`number`, `number`\]

Rotation in degrees: a number or `[min, max]`.

***

### sizes?

> `optional` **sizes?**: `number` \| `number`[]

Size over the lifetime: one number or up to 8 interpolated keyframes.

***

### sizeVariation?

> `optional` **sizeVariation?**: `number`

Per-particle size variation (`0..1`).

***

### space?

> `optional` **space?**: `"world"` \| `"screen"`

Simulation space: `'world'` (default, 3D) or `'screen'` (2D).

***

### speed?

> `optional` **speed?**: `number` \| \[`number`, `number`\]

Initial speed: a number or `[min, max]`.

***

### speedScale?

> `optional` **speedScale?**: `number`

Simulated-time factor (default 1).

***

### spin?

> `optional` **spin?**: `number` \| \[`number`, `number`\]

Angular velocity in degrees/second: a number or `[min, max]`.

***

### spinVariation?

> `optional` **spinVariation?**: `number`

Per-particle spin variation.

***

### spread?

> `optional` **spread?**: `number`

Emission cone half-angle in degrees.

***

### tangentialAcceleration?

> `optional` **tangentialAcceleration?**: `number` \| \[`number`, `number`\]

Tangential acceleration: a number or `[min, max]`.

***

### texture

> **texture**: [`EfxSample`](../type-aliases/EfxSample.md)

Texture (or render target) for particle quads; required.
