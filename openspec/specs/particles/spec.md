# particles Specification

## Purpose
Defines engine-owned CPU particle systems that emit and simulate particles in
3D world space or 2D screen space and draw each particle as a camera-facing
billboard, a vertical-axis billboard, or a fixed oriented 3D plane, configured
through a single options object and advanced by the engine frame loop.

## Requirements

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

### Requirement: World-space and screen-space simulation

A world-space system SHALL store particle positions, velocities, directions,
gravity, and accelerations as 3-component vectors and SHALL simulate particles
in 3D. A screen-space system SHALL use 2-component vectors in the 2D frame
coordinate space. On each engine step the system SHALL advance every live
particle by the frame time scaled by `speedScale`, apply gravity and the
configured accelerations and damping, integrate position, decrement the
remaining lifetime, retire particles whose lifetime has elapsed, and emit new
particles according to `emissionRate` and `emitterLifetime`. `emit(n)` SHALL
immediately spawn up to `n` particles limited by the free capacity,
independent of the current emission rate. A system SHALL never hold more than
`max` live particles.

The simulation SHALL be deterministic: for a fixed configuration and a fixed
sequence of per-frame time steps, the resulting particle state on a given
target SHALL be reproducible, and randomness SHALL be supplied by an
engine-owned generator seeded deterministically at creation.

#### Scenario: Continuous emission

- **WHEN** a system is created with a positive `emissionRate` and several
  frames elapse
- **THEN** its `count` grows toward the expected number for the elapsed time
  and stops at `max`

#### Scenario: Burst emission

- **WHEN** a script calls `emit(50)` on a system with free capacity
- **THEN** `count` increases by up to 50 immediately, without waiting for the
  emission rate

#### Scenario: Lifetime retires particles

- **WHEN** particles are emitted with a finite `lifetime` and the frame time
  advances past that lifetime
- **THEN** those particles are retired, `count` drops, and their slots become
  available for reuse

#### Scenario: Gravity acts in 3D

- **WHEN** a world-space system is created with `gravity: [0, -9.8, 0]` and
  particles are emitted and stepped
- **THEN** particle positions move in the negative Y direction over time

#### Scenario: Emitter lifetime stops continuous emission

- **WHEN** `emitterLifetime` is a finite number and the elapsed time exceeds
  it
- **THEN** no further particles are emitted by the rate, while existing
  particles continue to age and the script can still call `emit`

#### Scenario: Simulation is deterministic

- **WHEN** two identically configured systems are stepped with the same
  sequence of time steps
- **THEN** they report the same live counts and the same particle motion

### Requirement: Particle quad render modes

A world-space system's `facing` SHALL determine how each particle quad is
oriented: `'view'` SHALL face the recorded 3D camera; `'y'` SHALL keep the
quad's up edge aligned with world `+Y` while yawing toward the camera; and
`'plane'` SHALL place the quad in the fixed world plane perpendicular to
`normal`, never turning it toward the camera. A screen-space system SHALL draw
each particle as a 2D quad in the current 2D camera frame, unchanged by
`facing`. A `'plane'` system SHALL simulate and move particles in world space
while keeping their orientation fixed.

#### Scenario: Plane particles stay on a water plane

- **WHEN** a world-space system is created with `facing: 'plane'`,
  `normal: [0, 1, 0]`, and rising particles, and the camera orbits the scene
- **THEN** each particle quad stays horizontal (in the world XZ plane)
  regardless of the camera angle

#### Scenario: View particles face the camera

- **WHEN** a `'view'` world-space system is drawn and the camera orbits it
- **THEN** each particle turns to stay square to the camera

#### Scenario: Y particles keep world up

- **WHEN** a `'y'` world-space system is drawn and the camera moves vertically
- **THEN** each particle yaws toward the camera but keeps its up edge along
  world `+Y`

#### Scenario: Screen particles are 2D quads

- **WHEN** a `space: 'screen'` system is drawn
- **THEN** its particles appear as 2D quads under the current `setCamera2D`
  frame, unaffected by the 3D camera

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

### Requirement: Particle system runtime reconfiguration

A live `ParticleSystem` SHALL provide `set(opts)` accepting a partial options
object with the same field validation as creation. `set` SHALL apply the
provided fields atomically: on any validation failure it SHALL throw and
leave the previous configuration and all live particles unchanged. The
lifecycle controls `start()`, `stop()`, `pause()`, and `reset()` SHALL be
available: `start` resumes emission and simulation; `pause` suspends
simulation while keeping particles; `stop` suspends emission and resets the
emitter lifetime; `reset` removes all live particles and restores the emitter
lifetime while keeping the configuration. `emit(n)` SHALL remain available
regardless of the emitter state.

#### Scenario: Partial update applies

- **WHEN** a script calls `set({ emissionRate: 40, position: [1, 0, 2] })`
- **THEN** the rate and position change and every other field keeps its
  previous value

#### Scenario: Failed update is atomic

- **WHEN** a script calls `set` with one invalid field among valid ones
- **THEN** the call throws and no field changes and no particle is affected

#### Scenario: Pause and resume

- **WHEN** a system with live particles is paused and time advances, then
  resumed
- **THEN** the particles do not age while paused and continue from the same
  state after resuming

#### Scenario: Reset clears particles

- **WHEN** a script calls `reset()` on a system with live particles
- **THEN** `count` becomes 0 and emission restarts from the configured
  lifetime
