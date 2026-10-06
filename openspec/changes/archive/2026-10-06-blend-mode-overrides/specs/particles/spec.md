# Spec Delta

## MODIFIED Requirements

### Requirement: Particle system creation and configuration

`efx.graphics.createParticleSystem(texture, max, lifetime, opts?)` SHALL create and return a `ParticleSystem`.
The three required inputs are positional: `texture` (a live Texture or
RenderTarget used for every particle quad), `max` (the particle capacity, a
positive integer from `1` to the documented hard cap of `65536`), and
`lifetime` (a particle lifetime in seconds: a finite number `> 0` or a
`[min, max]` pair of finite numbers `> 0`). The trailing `opts` bag is
optional and SHALL contain only optional configuration. A missing required
input or a wrongly-typed
value SHALL throw `TypeError`; an out-of-range value SHALL throw `RangeError`;
on failure no system SHALL be created. Unknown fields SHALL throw `TypeError`.
Every numeric option SHALL be finite unless stated otherwise, and vector
options SHALL supply all required components.

The options bag SHALL accept at least:

- `space` — `'world'` (default) or `'screen'`.
- `facing` — the quad render mode for world-space systems: `'view'` (default),
  `'y'`, or `'plane'`; a screen-space system SHALL accept only `'view'` (or
  omit `facing`).
- `normal` — a finite `[x, y, z]` orientation normal, meaningful only when
  `facing` is `'plane'` (default `[0, 1, 0]`); supplying it for another facing
  SHALL throw `TypeError`.
- `blend` — `'alpha'`, `'additive'`, `'subtractive'`, or `null`. When set to a
  mode string, the system uses that mode for every particle batch it draws.
  When omitted or `null`, the system has no configured blend and inherits the
  frame's blend render state (see the `2d-layer` blending requirement) in
  effect when `drawParticles` records it. An invalid value SHALL throw
  `TypeError`.
- `emissionRate` — particles per second, finite `>= 0` (default `0`).
- `emitterLifetime` — seconds the emitter runs, finite `> 0`, or `-1` for
  infinite (default `-1`).
- `position` — the emitter position: `[x, y, z]` for world space or `[x, y]`
  for screen space (default the origin).
- `direction` — the base emission direction as a finite vector; `spread` — the
  random cone half-angle in degrees, finite `>= 0` (default `0`); `speed` — a
  finite number `>= 0` or `[min, max]` of finite numbers `>= 0` (default `0`).
- `gravity` — a finite acceleration vector (default zero).
- `linearAcceleration`, `radialAcceleration`, `tangentialAcceleration` — each
  a finite scalar or vector, or a min/max pair, describing per-particle
  accelerations.
- `linearDamping` — a finite number `>= 0` or `[min, max]` pair (`>= 0`).
- `emissionShape` — an emission volume: `'point'` (default), `'box'`,
  `'sphere'`, `'sphereSurface'`, or `'disc'`, with the extents/radius its shape
  requires.
- `sizes` — a finite size `> 0` or an array of `1..8` finite sizes `> 0`,
  interpolated across the particle lifetime (default `1`); `sizeVariation` —
  a finite number in `0..1`.
- `colors` — one `[r, g, b, a]` color or an array of `1..8` colors,
  interpolated across the lifetime (default opaque white).
- `rotation` — a finite number of degrees or a `[min, max]` pair (initial
  in-plane angle); `spin` — a finite number or `[start, end]` pair of degrees
  per second; `spinVariation` — a finite number in `0..1`.
- `relativeRotation` — a boolean (default `false`); when true, each particle's
  screen angle follows its velocity.
- `quads` — an optional array of `sourceRect` regions selecting atlas frames
  over the lifetime.
- `insertMode` — `'top'` (default), `'bottom'`, or `'random'`, controlling
  which slot a new particle reuses when the pool is full.
- `speedScale` — a finite factor `> 0` scaling simulated time (default `1`).

The returned `ParticleSystem` SHALL be a native-backed class with a
`destroy()` release method and an idempotent release contract; it SHALL expose
a read-only `count` property naming the number of live particles, and a
read-write `speedScale` property. Reading `count` or setting `speedScale` on a
destroyed system SHALL throw `TypeError`.

#### Scenario: System is created with defaults

- **WHEN** a script calls `createParticleSystem(texture, 512, [1, 2])` with
  only the required inputs
- **THEN** it receives a world-space `'view'`-facing system with no configured
  blend (it inherits the frame's blend render state at draw time), `count` 0,
  and no particles emitted until a rate or `emit` call

#### Scenario: Configured blend overrides the frame state

- **WHEN** a system is created with `blend: 'additive'` while the frame state
  is `'alpha'`
- **THEN** every batch the system draws is additive regardless of the frame
  state

#### Scenario: Invalid configuration is rejected

- **WHEN** a script omits `texture`, `max`, or `lifetime`, passes `max: 0` or a
  non-integer, passes `facing: 'plane'` with a
  `normal` of the wrong length, passes `facing: 'y'` with `space: 'screen'`,
  passes `blend: 'multiply'`, or supplies an unknown bag field
- **THEN** the call throws the appropriate `TypeError` or `RangeError` and no
  system is created

#### Scenario: Destroyed system is safe

- **WHEN** a script calls `destroy()` on a system and then reads `count`, sets
  `speedScale`, calls `emit`, or passes it to `drawParticles`
- **THEN** each use throws `TypeError`, and calling `destroy()` again is a
  no-op

### Requirement: Particle rendering and batching

`efx.graphics.drawParticles(sys)` SHALL record a single display-list batch for the live
particles of a system. On playback the engine SHALL draw all live particles,
each as a textured quad sized by the lifetime-interpolated `sizes`, tinted by
the lifetime-interpolated `colors`, rotated by its simulated angle, and
sampling its current atlas region when `quads` is set. Particle quads SHALL be
drawn depth-tested against earlier 3D records and SHALL NOT write depth, so
opaque geometry occludes them and they do not occlude one another. Particles
within a batch SHALL be ordered back-to-front by camera distance for
`'alpha'` blending; `'additive'` and `'subtractive'` batches need no ordering.
Drawing SHALL use the system's configured blend mode when one is set, and
otherwise the frame's blend render state in effect when the record was
created, resolved and value-snapshotted at record time (see the `2d-layer`
blending requirement). Passing a value that is not a live `ParticleSystem`, or
a destroyed one, to `drawParticles` SHALL throw `TypeError`. A live system
SHALL retain its texture until it is destroyed.

#### Scenario: All live particles are drawn

- **WHEN** a system has live particles and `drawParticles(sys)` is recorded in a
  render hook
- **THEN** every live particle appears in the frame using its interpolated
  size, color, angle, and atlas frame

#### Scenario: Particles are occluded by geometry

- **WHEN** a world-space particle is behind an opaque mesh relative to the
  camera
- **THEN** it is not visible through the mesh, and the particle does not write
  depth

#### Scenario: Alpha particles are sorted back-to-front

- **WHEN** an alpha-blended batch has overlapping particles at different
  distances
- **THEN** nearer particles blend over farther ones in the correct order

#### Scenario: Additive particles are order-independent

- **WHEN** an additive batch has overlapping particles
- **THEN** the result does not depend on the order in which the particles are
  stored

#### Scenario: Unconfigured system inherits the frame state

- **WHEN** a system created without `blend` is recorded while the frame state
  is `'subtractive'`
- **THEN** its batch is subtractive, and a later `setBlendMode` does not
  change that recorded batch

#### Scenario: Destroyed system is rejected

- **WHEN** `drawParticles` is passed a non-system value or a destroyed system
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Texture is retained while the system lives

- **WHEN** a script destroys the Texture used by a live particle system and
  then draws the system
- **THEN** the draw still resolves the texture, because the system retained it
  until `destroy()`
