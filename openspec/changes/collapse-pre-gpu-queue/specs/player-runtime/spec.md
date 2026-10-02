# Spec Delta

## ADDED Requirements

### Requirement: Resources without a rendering surface
In run modes that have no rendering surface — the desktop `--script` mode and
the web Node harness — the player SHALL still allow scripts to create and query
engine resources. Texture, mesh, render-target, font, and particle-system
creation SHALL succeed; a created resource SHALL report its size and SHALL be
accepted by recorded draws, while nothing is uploaded to a GPU or rendered.
Engine-owned resources, including `efx.graphics.whiteTexture`, SHALL be
available in these modes exactly as in surface-bearing modes. No rendering
surface SHALL be initialized in these modes.

#### Scenario: Create and query a texture without a surface
- **WHEN** a `--script` run creates a texture from an ImageData and reads its
  `width` and `height`
- **THEN** the call succeeds and the values match the ImageData

#### Scenario: Engine-owned resource is available without a surface
- **WHEN** a `--script` run reads `efx.graphics.whiteTexture`
- **THEN** it receives a valid 1×1 Texture usable in a recorded draw

#### Scenario: Draws are recorded but not rendered
- **WHEN** a `--script` run draws a quad or mesh
- **THEN** the draw is accepted and recorded with no rendering surface present

#### Scenario: No surface is created
- **WHEN** the `--script` mode or the web Node harness runs
- **THEN** no window, GPU context, or engine pipelines are initialized
