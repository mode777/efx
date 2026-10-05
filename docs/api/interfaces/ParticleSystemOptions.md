[**EFX API**](../README.md)

***

[EFX API](../README.md) / ParticleSystemOptions

# Interface: ParticleSystemOptions

Full particle configuration; the creation inputs (`texture`, `max`, `lifetime`) are positional on `createParticleSystem`.

## Extends

- [`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md)

## Properties

### blend?

> `optional` **blend?**: [`EfxBlendMode`](../type-aliases/EfxBlendMode.md)

Blend mode (default `'alpha'`).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`blend`](ParticleSystemCreateOptions.md#blend)

***

### colors?

> `optional` **colors?**: [`Color`](../type-aliases/Color.md) \| [`Color`](../type-aliases/Color.md)[]

Color over the lifetime: one color or up to 8 interpolated keyframes.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`colors`](ParticleSystemCreateOptions.md#colors)

***

### direction?

> `optional` **direction?**: [`Vec3`](../type-aliases/Vec3.md) \| [`Vec2`](../type-aliases/Vec2.md)

Emission direction (2- or 3-component vector).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`direction`](ParticleSystemCreateOptions.md#direction)

***

### emissionRate?

> `optional` **emissionRate?**: `number`

Particles emitted per second (default 0).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`emissionRate`](ParticleSystemCreateOptions.md#emissionrate)

***

### emissionShape?

> `optional` **emissionShape?**: [`EmissionShapeOptions`](EmissionShapeOptions.md)

Emission volume; defaults to a point.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`emissionShape`](ParticleSystemCreateOptions.md#emissionshape)

***

### emitterLifetime?

> `optional` **emitterLifetime?**: `number`

Emitter lifetime in seconds; `-1` is infinite.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`emitterLifetime`](ParticleSystemCreateOptions.md#emitterlifetime)

***

### facing?

> `optional` **facing?**: [`EfxFacing`](../type-aliases/EfxFacing.md)

Quad render mode in world space (default `'view'`; screen space must be `'view'`).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`facing`](ParticleSystemCreateOptions.md#facing)

***

### gravity?

> `optional` **gravity?**: [`Vec3`](../type-aliases/Vec3.md) \| [`Vec2`](../type-aliases/Vec2.md)

Constant acceleration (2- or 3-component vector).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`gravity`](ParticleSystemCreateOptions.md#gravity)

***

### insertMode?

> `optional` **insertMode?**: `"top"` \| `"bottom"` \| `"random"`

Draw order within the batch (default `'top'`).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`insertMode`](ParticleSystemCreateOptions.md#insertmode)

***

### lifetime

> **lifetime**: `number` \| \[`number`, `number`\]

Particle lifetime in seconds: a number or `[min, max]`.

***

### linearAcceleration?

> `optional` **linearAcceleration?**: [`Vec3`](../type-aliases/Vec3.md) \| [`Vec2`](../type-aliases/Vec2.md)

Additional constant acceleration (2- or 3-component vector).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`linearAcceleration`](ParticleSystemCreateOptions.md#linearacceleration)

***

### linearDamping?

> `optional` **linearDamping?**: `number` \| \[`number`, `number`\]

Linear damping: a number or `[min, max]`.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`linearDamping`](ParticleSystemCreateOptions.md#lineardamping)

***

### max

> **max**: `number`

Maximum live particles (integer, 1..65536).

***

### normal?

> `optional` **normal?**: [`Vec3`](../type-aliases/Vec3.md)

Plane orientation normal for `facing: 'plane'` (default `[0, 1, 0]`).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`normal`](ParticleSystemCreateOptions.md#normal)

***

### position?

> `optional` **position?**: [`Vec3`](../type-aliases/Vec3.md) \| [`Vec2`](../type-aliases/Vec2.md)

Spawn position (2- or 3-component vector).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`position`](ParticleSystemCreateOptions.md#position)

***

### quads?

> `optional` **quads?**: [`SourceRect`](SourceRect.md)[]

Atlas frames cycled over the lifetime.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`quads`](ParticleSystemCreateOptions.md#quads)

***

### radialAcceleration?

> `optional` **radialAcceleration?**: `number` \| \[`number`, `number`\]

Radial acceleration: a number or `[min, max]`.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`radialAcceleration`](ParticleSystemCreateOptions.md#radialacceleration)

***

### relativeRotation?

> `optional` **relativeRotation?**: `boolean`

When true, particle angle follows its velocity.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`relativeRotation`](ParticleSystemCreateOptions.md#relativerotation)

***

### rotation?

> `optional` **rotation?**: `number` \| \[`number`, `number`\]

Rotation in degrees: a number or `[min, max]`.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`rotation`](ParticleSystemCreateOptions.md#rotation)

***

### sizes?

> `optional` **sizes?**: `number` \| `number`[]

Size over the lifetime: one number or up to 8 interpolated keyframes.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`sizes`](ParticleSystemCreateOptions.md#sizes)

***

### sizeVariation?

> `optional` **sizeVariation?**: `number`

Per-particle size variation (`0..1`).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`sizeVariation`](ParticleSystemCreateOptions.md#sizevariation)

***

### space?

> `optional` **space?**: `"world"` \| `"screen"`

Simulation space: `'world'` (default, 3D) or `'screen'` (2D).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`space`](ParticleSystemCreateOptions.md#space)

***

### speed?

> `optional` **speed?**: `number` \| \[`number`, `number`\]

Initial speed: a number or `[min, max]`.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`speed`](ParticleSystemCreateOptions.md#speed)

***

### speedScale?

> `optional` **speedScale?**: `number`

Simulated-time factor (default 1).

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`speedScale`](ParticleSystemCreateOptions.md#speedscale)

***

### spin?

> `optional` **spin?**: `number` \| \[`number`, `number`\]

Angular velocity in degrees/second: a number or `[min, max]`.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`spin`](ParticleSystemCreateOptions.md#spin)

***

### spinVariation?

> `optional` **spinVariation?**: `number`

Per-particle spin variation.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`spinVariation`](ParticleSystemCreateOptions.md#spinvariation)

***

### spread?

> `optional` **spread?**: `number`

Emission cone half-angle in degrees.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`spread`](ParticleSystemCreateOptions.md#spread)

***

### tangentialAcceleration?

> `optional` **tangentialAcceleration?**: `number` \| \[`number`, `number`\]

Tangential acceleration: a number or `[min, max]`.

#### Inherited from

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md).[`tangentialAcceleration`](ParticleSystemCreateOptions.md#tangentialacceleration)

***

### texture

> **texture**: [`EfxSample`](../type-aliases/EfxSample.md)

Texture (or render target) for particle quads.
