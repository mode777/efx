# Spec Delta

## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Text loading

The engine SHALL load a resource as UTF-8 text through `efx.io.loadText(path)`
and return it as a string, rejecting a file that is not valid for the
platform's script string encoding with an error.

#### Scenario: Load text
- **WHEN** a script calls `efx.io.loadText(path)` for a text resource
- **THEN** the returned string is the file's decoded contents
