# Spec Delta

## MODIFIED Requirements

### Requirement: Native bridge binding contract
On Emscripten the player SHALL expose every C-implemented engine function
through the native bridge with the same name, signature, semantics and
error behavior as the desktop binding, so that browser code and game
scripts use one API. Resource objects (`createImageData`, `createTexture`,
`efx.graphics.whiteTexture`) SHALL wrap native handles as JS objects with the
same `destroy()` semantics; the frame-end native release sweep SHALL continue
to run in C. The bridge SHALL NOT require the embedded interpreter, and
its presence MUST NOT reintroduce embedded-GC machinery into the web
build.

#### Scenario: Same call from browser code
- **WHEN** browser script code calls the bridge's `setClearColor` with a
  4-element color array and then `drawQuad` with a created texture
- **THEN** the core renders identically to the same sequence on desktop

#### Scenario: Resource lifecycle on web
- **WHEN** browser code creates a texture and calls `destroy()` on it
- **THEN** the native release follows the same deterministic/deferred
  contract as on desktop, with the browser GC managing only the JS side

#### Scenario: No embedded GC on web
- **WHEN** the web player runs its frame loop
- **THEN** no embedded-interpreter garbage collection executes (the
  segfault class is structurally absent)
