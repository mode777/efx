# Spec Delta

## ADDED Requirements

### Requirement: Resource-loading API

The script API SHALL provide a resource-loading layer that reads files from
the resource root by relative path and returns engine resources. Each function
SHALL be tagged with its layer in the reference: `loadText` and `loadImage`
are C-implemented; `loadTexture` is a pure-JS convenience composed only from
public API and standard ES6. Loading SHALL be synchronous from the script's
point of view on every target. A missing, unreadable, or undecodable resource
SHALL throw a standard ES6 `Error`; a malformed path argument SHALL throw
`TypeError`. The reference document (`docs/js-api.md`) and the gallery type
document (`gallery/src/api/efx.d.ts`) SHALL be updated in the same change that
delivers these functions.

#### Scenario: loadText returns decoded text
- **WHEN** a script calls `efx.loadText(path)` for a text resource in the root
- **THEN** it receives the file's contents as a string

#### Scenario: loadImage returns ImageData
- **WHEN** a script calls `efx.loadImage(path)` for a PNG or JPEG in the root
- **THEN** it receives an `ImageData` with read-only pixel dimensions and
  decoded RGBA pixels, releasable with `destroy()`

#### Scenario: loadTexture composes the public API
- **WHEN** a script calls `efx.loadTexture(path)`
- **THEN** it receives a live `Texture` equivalent to creating one from the
  image data returned by `loadImage`, with the same `destroy()` lifecycle

#### Scenario: Undecodable or missing resource throws
- **WHEN** a script loads a path that does not exist or is not a decodable
  image (for `loadImage`)
- **THEN** the call throws an `Error` and no resource object is returned

#### Scenario: Malformed argument throws TypeError
- **WHEN** a script passes a non-string path to a load function
- **THEN** the call throws `TypeError`
