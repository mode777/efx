# Spec Delta: js-api

## MODIFIED Requirements

### Requirement: Graphics namespace API
The script API SHALL expose the graphics drawing, state, and resource surface
as the sub-namespace `efx.graphics` of the single `efx` object, with no new
free globals. `efx.graphics` SHALL contain exactly these function members,
each with its unchanged name, signature, semantics, defaults, layer tag, and
error behavior: `beginRenderTarget`, `createFont`, `createImageData`,
`createMesh`, `createMeshData`, `createParticleSystem`, `createRenderTarget`,
`createTexture`, `drawBillboard`, `drawMesh`, `drawParticles`, `drawQuad`,
`drawSprites`, `drawText`, `endRenderTarget`, `loadFontData`, `loadImage`,
`loadMeshData`, `makeCapsule`, `makeCube`, `makeHeightfield`, `makePlane`,
`makeSphere`, `screenRay`, `setBlendMode`, `setCamera2D`, `setCamera3D`,
`setClearColor`, `setDirectionalLight`, `setLight`, `setPostEffects`, and
`setRenderScale` —
plus the engine-owned read-only `whiteTexture` Texture property. The
operations on a native-backed class instance SHALL be methods on that class,
not free functions: `poseMesh`, `setMeshSurfaceMaterial`, and `measureText`
SHALL NOT be members of `efx.graphics` (see the Resource operation methods
requirement). None of these names SHALL also exist as members of the `efx`
root: the move is a hard cut with no deprecated root aliases and no
compatibility shims. The `efx` root SHALL keep only the runtime/lifecycle
members (`log`, `quit`, the `args` property, `registerUpdateHook`,
`registerRenderHook`) and the domain sub-namespaces (`math`, `io`, `color`,
`keyboard`, `mouse`, `window`, `physics`, `gamepad`, `audio`, `time`);
`whiteTexture`, `loadText`, and the math helpers are no longer root members.
The sub-namespace
SHALL exist on every runtime binding (quickjs desktop and the Emscripten web
bridge) with identical membership, and every entry SHALL behave identically
across them. The generated reference (`docs/api/` from
`gallery/src/api/efx.d.ts`), the guidelines (`docs/js-api.md`), and this
capability's sibling requirements SHALL use the `efx.graphics.*` paths.

#### Scenario: Graphics functions are reachable through the sub-namespace
- **WHEN** a script calls `efx.graphics.drawQuad(...)` — and likewise every
  other listed member — without imports or setup
- **THEN** the call succeeds on every target platform with the exact
  behavior it had at the root before the move

#### Scenario: White texture lives in the graphics sub-namespace
- **WHEN** a script reads `efx.graphics.whiteTexture` and `efx.whiteTexture`
- **THEN** the first is the engine-owned 1×1 opaque-white `Texture` (same
  object on every read; `destroy()` throws) and the second is absent

#### Scenario: No root aliases remain
- **WHEN** a script reads any moved name at the root (for example
  `efx.drawQuad`, `efx.makeCube`, or `efx.whiteTexture`) and enumerates
  `efx.graphics`
- **THEN** the moved names are absent from the root, `efx.graphics` holds
  exactly the listed members, and the root exposes only the lifecycle
  facilities and the domain sub-namespaces

#### Scenario: Resource operations are not graphics members
- **WHEN** a script reads `efx.graphics.measureText`, `efx.graphics.poseMesh`,
  and `efx.graphics.setMeshSurfaceMaterial` and enumerates `efx.graphics`
- **THEN** all three are absent from `efx.graphics`, and the equivalent
  operations exist only as `Font.measure`, `Mesh.pose`, and
  `Mesh.setSurfaceMaterial`

#### Scenario: The move changes no observable behavior
- **WHEN** the in-repo script corpus (golden scenes, portable script tests,
  curated samples) is re-pathed from `efx.<fn>` to `efx.graphics.<fn>` (and
  `efx.whiteTexture` to `efx.graphics.whiteTexture`) and run through both
  runtimes
- **THEN** every golden frame stays pixel-identical, the cross-runtime
  error catalog stays byte-identical, and no signature, default, or error
  message changes
