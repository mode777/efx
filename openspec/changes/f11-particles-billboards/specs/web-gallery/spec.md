# Spec Delta

## ADDED Requirements

### Requirement: Particle system showcase samples

The curated gallery SHALL include at least one particle showcase sample that
demonstrates the particle system's defining features to visitors, and those
samples SHALL use only the public `efx` API and standard ES6. Together the
particle showcase(s) SHALL demonstrate:

- world-space emission and simulation (`space: 'world'`), including a burst
  (`emit`) and continuous emission;
- at least two quad render modes — camera-facing billboards (`facing: 'view'`
  or `'y'`) and fixed oriented planes (`facing: 'plane'`, e.g. a water
  surface);
- both additive and alpha (or subtractive) blending;
- lifetime-interpolated size and/or color, and at least one showcase MAY use
  atlas frames (`quads`).

A showcase SHALL prefer procedurally generated textures built with
`createImageData` so it needs no third-party assets, or otherwise ship a CC0
asset pack under the existing sample asset-pack contract. Each particle
showcase SHALL be covered by the gallery smoke run.

#### Scenario: Showcase demonstrates the render modes

- **WHEN** a visitor selects a particle showcase
- **THEN** it renders particles through the public `createParticleSystem` /
  `drawParticles` API and demonstrates camera-facing billboards and fixed
  oriented planes

#### Scenario: Showcase demonstrates blending and emission styles

- **WHEN** the particle showcase runs
- **THEN** it demonstrates an additive effect and an alpha/subtractive effect,
  and both a burst and continuously emitted particles

#### Scenario: Showcase is portable to the player

- **WHEN** a particle showcase's source is run by the `efx` player against the
  same resource-root contract
- **THEN** it runs using only the public API and standard ES6, with textures
  either generated with `createImageData` or loaded from its mounted asset
  pack

#### Scenario: Showcase is exercised by the smoke

- **WHEN** the gallery smoke run executes
- **THEN** at least one particle showcase is among the samples it drives, and
  the smoke fails on any console or page error from it
