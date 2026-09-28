# Spec Delta

## REMOVED Requirements

### Requirement: Resource-loading API

**Reason**: The `loadTexture` convenience is removed so that loaded textures
can take creation options (sampler and mipmap) through the one creation
function; the flow becomes the explicit composition
`createTexture(loadImage(path), opts?)`, symmetric with
`createMesh(loadMeshData(path))`.

**Migration**: Replace `efx.loadTexture(path)` with
`efx.createTexture(efx.loadImage(path), opts?)`. `loadText` and `loadImage`
are unchanged.

## ADDED Requirements

### Requirement: Resource loading and texture composition

The script API SHALL provide a resource-loading layer that reads files from
the resource root by relative path and returns engine resources. Each function
SHALL be tagged with its layer in the reference: `loadText` and `loadImage`
are C-implemented loaders. There SHALL be no separate texture loader: a
texture is created by composing the public API,
`createTexture(loadImage(path), opts?)`, matching the mesh flow where
`createMesh` consumes `loadMeshData`. Loading SHALL be synchronous from the
script's point of view on every target. A missing, unreadable, or undecodable
resource SHALL throw a standard ES6 `Error`; a malformed path argument SHALL
throw `TypeError`. The reference document (`docs/js-api.md`) and the gallery
type document (`gallery/src/api/efx.d.ts`) SHALL be updated in the same change
that delivers these functions.

#### Scenario: loadText returns decoded text
- **WHEN** a script calls `efx.loadText(path)` for a text resource in the root
- **THEN** it receives the file's contents as a string

#### Scenario: loadImage returns ImageData
- **WHEN** a script calls `efx.loadImage(path)` for a PNG or JPEG in the root
- **THEN** it receives an `ImageData` with read-only pixel dimensions and
  decoded RGBA pixels, releasable with `destroy()`

#### Scenario: Texture creation composes loadImage and createTexture
- **WHEN** a script calls `efx.createTexture(efx.loadImage(path), opts?)`
- **THEN** it receives a live `Texture` carrying the image's pixels plus any
  requested sampler and mipmap options, with the same `destroy()` lifecycle

#### Scenario: No texture-loading convenience
- **WHEN** the API reference and gallery type document are read after this
  change
- **THEN** they catalog `loadImage` and `createTexture` and do not catalog a
  `loadTexture` function

#### Scenario: Undecodable or missing resource throws
- **WHEN** a script loads a path that does not exist or is not a decodable
  image (for `loadImage`)
- **THEN** the call throws an `Error` and no resource object is returned

#### Scenario: Malformed argument throws TypeError
- **WHEN** a script passes a non-string path to a load function
- **THEN** the call throws `TypeError`
