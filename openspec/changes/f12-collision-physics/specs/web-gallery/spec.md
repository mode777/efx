# Spec Delta

## ADDED Requirements

### Requirement: Physics showcase sample

The curated gallery catalog SHALL include at least one physics showcase sample
that demonstrates the `efx.physics` API to visitors: a character moved with
`moveAndSlide` over solid static geometry (walls, a slope, and a step),
`sensor` trigger volumes detected through `overlap` or `body.contacts`, simple
falling and pushable dynamic props, and at least one query (`raycast` or
`overlap`). The sample SHALL use only the public API and standard ES6, SHALL
use no browser or Node.js dependency, and SHALL prefer procedurally generated
geometry so it needs no third-party asset pack. The sample SHALL be runnable
by the player under the same resource-root contract as any other sample.

#### Scenario: Catalog contains a physics sample

- **WHEN** the curated gallery catalog is read
- **THEN** it contains a physics showcase sample, and selecting it runs the
  sample in the application area

#### Scenario: Sample exercises the physics surface

- **WHEN** the physics showcase sample runs
- **THEN** it moves a character with `moveAndSlide` over geometry, reports a
  floor and a wall, detects a sensor, and simulates at least one dynamic body

#### Scenario: Sample is portable

- **WHEN** the sample's source is run by the player against the same
  resource-root contract
- **THEN** it runs without referencing any browser or Node.js API
