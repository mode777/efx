# resource-loading

## Purpose

Defines how the player locates and reads game resources from a resource root
(a directory or a zip archive), decodes text and images into engine resources,
and — on the web — mounts a host-provided resource archive before the entry
script runs, so the script-facing API stays synchronous on every target.

## Requirements

### Requirement: Resource root may be a directory or a zip archive

The player SHALL accept, as its resource root, either a directory or a zip
archive file. The same relative paths SHALL resolve identically against both
kinds of root, and `main.js` SHALL be read from the root through the same
mechanism as any other resource. A root that does not exist, or a zip archive
that cannot be opened, SHALL produce a diagnostic and a non-zero exit.

#### Scenario: Directory root
- **WHEN** the player is launched with a directory containing `main.js` and
  other resources
- **THEN** scripts load those resources by their paths relative to the root

#### Scenario: Zip root
- **WHEN** the player is launched with a zip archive whose entries include
  `main.js` and other resources
- **THEN** scripts load those resources by their paths relative to the archive
  root, with the same behavior as a directory root

#### Scenario: Unreadable root
- **WHEN** the player is launched with a missing path or a corrupt zip
- **THEN** it prints a diagnostic and exits non-zero

### Requirement: Relative resource paths

Every resource path SHALL be interpreted relative to the resource root, using
forward-slash separators. A path that attempts to escape the root via parent
segments SHALL be rejected with an error. Resources are read synchronously;
there is no asynchronous resource API visible to scripts.

#### Scenario: Path resolves relative to root
- **WHEN** a script loads `data/welcome.txt` and the root contains
  `data/welcome.txt`
- **THEN** the file's contents are returned

#### Scenario: Escape attempt rejected
- **WHEN** a script supplies a path that resolves outside the resource root
- **THEN** the load fails with an error and no data outside the root is read

#### Scenario: Missing resource errors
- **WHEN** a script loads a path that does not exist in the root
- **THEN** the load fails with an error and the run reports it through the
  standard error/exit-code contract

### Requirement: Text loading

The engine SHALL load a resource as UTF-8 text through `efx.io.loadText(path)`
and return it as a string, rejecting a file that is not valid for the
platform's script string encoding with an error.

#### Scenario: Load text
- **WHEN** a script calls `efx.io.loadText(path)` for a text resource
- **THEN** the returned string is the file's decoded contents

### Requirement: Image loading

The engine SHALL decode PNG and JPEG images into CPU image data in the
engine's `rgba8` pixel format, exposing width, height, and RGBA bytes through
the existing `ImageData` resource. Decoding a corrupt or unsupported image
SHALL fail with an error and SHALL NOT return a partially decoded resource.
JPEG images, which carry no alpha channel, SHALL decode with fully opaque
alpha.

#### Scenario: PNG with alpha
- **WHEN** a script loads a PNG image containing transparency
- **THEN** the resulting `ImageData` preserves the image's width, height, and
  per-pixel alpha

#### Scenario: JPEG
- **WHEN** a script loads a JPEG image
- **THEN** the resulting `ImageData` has the image's width and height and
  fully opaque alpha

#### Scenario: Unsupported or corrupt image
- **WHEN** a script loads a file that is not a decodable image
- **THEN** the load fails with an error and no `ImageData` is returned

### Requirement: Web host-provided resource root

On Emscripten, when the embedding page supplies an asset-root URL before the
player boots, the player SHALL fetch that archive, mount it as the resource
root, and only then read and evaluate `main.js`, so that all `load*` calls in
the entry script — at top level and in hooks — are synchronous. When no
asset-root URL is supplied, the existing resource-root behavior (including
build-time preloaded roots) SHALL be unchanged. A fetch or mount failure SHALL
surface through the standard non-zero exit-code contract. The host channel
SHALL be consumed before the entry script is evaluated and MUST NOT be
observable by the script, which remains bound by the no-browser/host-
dependency restriction.

#### Scenario: Asset root mounted before the entry script runs
- **WHEN** the embedding page supplies an asset-root URL before boot and the
  fetched archive contains the resources the entry script loads
- **THEN** the entry script's top-level `load*` calls succeed synchronously

#### Scenario: Boot blocks until the archive is mounted
- **WHEN** an asset-root URL is supplied
- **THEN** the player completes the fetch and mount before evaluating the
  entry script and before starting the frame loop, and exposes no
  script-visible loading hook or asynchronous resource API

#### Scenario: No asset root falls back
- **WHEN** no asset-root URL is supplied
- **THEN** the player reads resources from the resource root as before

#### Scenario: Fetch failure surfaces
- **WHEN** the asset archive cannot be fetched or mounted
- **THEN** the run ends with a diagnostic on the error channel and the
  failure exit code

#### Scenario: Host channel invisible to the script
- **WHEN** an entry script runs with a host-provided asset root
- **THEN** the script observes no host channel and no browser fetch facility

### Requirement: Binary loading

The engine SHALL load a resource as raw bytes and return them as a `Uint8Array`
copy through `efx.io.loadData(path)`, applying no decoding or interpretation
(text, image, mesh, and audio loaders remain separate). The returned array SHALL
have exactly the file's length and bytes, and mutating it SHALL NOT affect the
engine. A missing, unreadable, or escaping path SHALL fail with an error and no
data outside the resource root SHALL be read. Loading SHALL be synchronous, with
no asynchronous resource API visible to scripts.

#### Scenario: Load raw bytes
- **WHEN** a script calls `efx.io.loadData(path)` for a file in the root
- **THEN** it receives a `Uint8Array` whose length and bytes equal the file's,
  independent of the file's content

#### Scenario: Returned bytes are a copy
- **WHEN** a script mutates the returned `Uint8Array` and loads the same path
  again
- **THEN** the second load returns the file's original bytes

#### Scenario: Missing binary resource errors
- **WHEN** a script calls `efx.io.loadData(path)` for a path that does not exist
- **THEN** the load fails with an error and no data is returned

#### Scenario: Escape attempt rejected
- **WHEN** a script supplies a path to `efx.io.loadData` that resolves outside
  the resource root
- **THEN** the load fails with an error and no data outside the root is read
