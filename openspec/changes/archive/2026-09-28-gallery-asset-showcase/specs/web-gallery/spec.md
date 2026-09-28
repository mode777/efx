# Spec Delta

## ADDED Requirements

### Requirement: Sample asset packs

The gallery build SHALL support an asset pack on any catalog sample, golden
or curated. When a sample ships an asset pack, the build SHALL copy that
pack into the built site and the runner SHALL mount it as the sample's
resource root before the sample's script is evaluated, so the sample's
synchronous `load*` calls resolve against it. The same pack SHALL run the
sample under the player's resource-root contract (directory or zip), so a
sample stays portable between the gallery and the player.

#### Scenario: Curated sample asset pack is mounted before the script runs

- **WHEN** a curated sample ships an asset pack and is selected in the gallery
- **THEN** the runner mounts that pack as the resource root before evaluating
  the sample, and the sample's top-level `load*` calls succeed

#### Scenario: Asset pack is portable to the player

- **WHEN** a curated sample's asset pack is used as the player's resource root
- **THEN** the sample runs with the same loaded resources as in the gallery

#### Scenario: Missing or undecodable asset surfaces

- **WHEN** a sample's script loads a resource its pack does not provide, or
  one that cannot be decoded
- **THEN** the failure surfaces through the sample error channel without
  breaking the gallery shell
