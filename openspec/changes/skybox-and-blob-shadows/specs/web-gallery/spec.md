# Spec Delta

## ADDED Requirements

### Requirement: Skybox and blob-shadow showcase samples

The curated gallery catalog SHALL include a skybox showcase sample and a
blob-shadow showcase sample that demonstrate these effects to visitors using
only the public `efx` API and standard ES6, with no browser or Node.js
dependency.

The skybox showcase SHALL render an inward-facing primitive (`makeSphere` or
`makeCube` with `inverted: true`) carrying an `unlit` material whose `diffuse`
channel samples a single equirectangular sky image, drawn camera-locked and
with `depthWrite: false` before a small lit scene, so the sky reads as a
background that does not occlude the scene. The sky image SHALL be shipped as
a CC0 asset under the existing curated sample asset-pack contract, with its
provenance and downscale recipe recorded in `gallery/samples/curated/CREDITS.md`.

The blob-shadow showcase SHALL draw a radial shadow as a `facing: 'plane'`
billboard placed a small distance above a ground plane, with a dark color and
alpha blending, following a moving character mesh under an orbiting 3D camera,
and SHALL prefer a procedurally generated shadow texture built with
`createImageData` so it needs no third-party asset.

Each showcase SHALL be runnable by the player under the same resource-root
contract as any other sample, and each SHALL be covered by the gallery smoke
run.

#### Scenario: Catalog contains both showcases

- **WHEN** the curated gallery catalog is read
- **THEN** it contains a skybox showcase and a blob-shadow showcase, and
  selecting either runs the sample in the application area

#### Scenario: Skybox showcase draws an unlit, non-occluding sky

- **WHEN** the skybox showcase runs
- **THEN** it draws an inverted unlit mesh with a sky texture and
  `depthWrite: false`, the sky fills the background, and scene geometry is
  drawn over it

#### Scenario: Blob shadow stays flat under an orbiting camera

- **WHEN** the blob-shadow showcase runs and the camera orbits the scene
- **THEN** the shadow remains a flat decal on the ground plane and follows the
  moving character, independent of the camera's facing

#### Scenario: Showcases are portable to the player

- **WHEN** either showcase's source is run by the `efx` player against the
  same resource-root contract
- **THEN** it runs using only the public API and standard ES6, with the blob
  shadow texture generated via `createImageData` and the sky image loaded from
  its mounted asset pack

#### Scenario: Showcases are exercised by the smoke

- **WHEN** the gallery smoke run executes
- **THEN** both showcases are among the samples it drives, and the smoke fails
  on any console or page error from them
