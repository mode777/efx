# Proposal

## Why

The API grew one domain at a time and every later domain got its own
sub-namespace — `efx.keyboard`/`efx.mouse`/`efx.window` (F9),
`efx.physics` (F12), `efx.gamepad` (F13), `efx.audio` (F14) — but the
graphics surface (33 drawing, state, and resource functions, F2–F8 +
F11) is still pinned flat at the root of `efx`, alongside the runtime
facilities (`log`, `quit`, `args`, hook registration). The root is now an
unstructured mix of "everything the engine did first". Moving graphics
into `efx.graphics` makes the namespace organization uniform: root =
runtime/lifecycle facilities + domain sub-namespaces, and gives the
largest surface a named home before the API freezes further.

## What Changes

- **New `efx.graphics` sub-namespace.** These 33 functions move from the
  `efx` root to `efx.graphics`, with unchanged names, signatures,
  semantics, defaults, and error behavior:
  cameras/state: `setCamera2D`, `setCamera3D`, `setClearColor`,
  `setBlendMode`, `setLight`, `setDirectionalLight`,
  `setMeshSurfaceMaterial`, `setRenderScale`;
  2D drawing: `drawQuad`, `drawSprites`, `drawText`;
  3D drawing: `drawMesh`, `drawBillboard`, `drawParticles`, `poseMesh`;
  resources: `createImageData`, `loadImage`, `createTexture`,
  `createMeshData`, `loadMeshData`, `createMesh`,
  `setMeshSurfaceMaterial`, `createFont`, `loadFontData`,
  `createRenderTarget`, `beginRenderTarget`, `endRenderTarget`,
  `createParticleSystem`;
  pure-JS primitives: `makeCube`, `makePlane`, `makeSphere`,
  `makeCapsule`;
  text queries: `measureText`; post FX: `setPostEffects`.
- **BREAKING — hard cut, no aliases.** The root members are removed, not
  deprecated: `efx.drawQuad` and friends no longer exist. All consumers
  are in-repo (tests, golden scenes, curated samples, examples) and are
  migrated in the same change; there is no external script corpus to
  keep alive. Recorded assumption per the request: "instead of the root"
  means removal, and every demo/test is updated to the new path.
- **Nothing else moves.** Root keeps `log`, `quit`, `args`,
  `registerUpdateHook`, `registerRenderHook`, `whiteTexture` (not in the
  moved set), `loadText` (generic resource facility, used by the
  CommonJS loader), and the `mat4`/`vec3`/`quat` math helpers; the
  `keyboard`/`mouse`/`window`/`physics`/`gamepad`/`audio` namespaces are
  unchanged. Not in scope: any signature, default, validation, or error
  message change — the functions keep their exact observable behavior;
  only the access path changes.
- **Registration stays three-layer.** The move is applied in lockstep in
  the native registration (`src/runtime/runtime.c`), the web bridge
  (`src/web/js/*.js` `api` literal), and the prelude install targets
  (`src/prelude/prelude.js`, `prelude.h` regenerated) — mirroring how
  `efx.audio`/`efx.physics` are already assembled.

## Capabilities

### New Capabilities

- None — this is a reorganization of the existing script surface, not a
  new capability.

### Modified Capabilities

- `js-api`: **structural.** MODIFY "Single global API namespace" to
  define the graphics sub-namespace; ADD a "Graphics namespace API"
  requirement enumerating the moved members, the no-alias rule, and
  unchanged behavior; MODIFY "Font and text API", "Billboard,
  sprite-batch, and particle API", "Resource loading and texture
  composition", "Gallery type document accuracy", "Resource
  classification and fixed limits", "Skinned mesh data and implicit rig
  payload", and "Identical argument errors on every runtime" to the
  `efx.graphics.*` paths.
- `2d-layer`: `efx.drawSprites` → `efx.graphics.drawSprites` in the
  sprite-batch requirement.
- `3d-core`: camera/mesh/material/draw/primitive requirements move to
  `efx.graphics.setCamera3D`/`createMeshData`/`createMesh`/
  `setMeshSurfaceMaterial`/`drawMesh`/`makeCube`/`makePlane`/
  `makeSphere` paths.
- `lighting`: light and material-binding requirements move to
  `efx.graphics.setLight`/`setDirectionalLight`/
  `setMeshSurfaceMaterial`/`createMeshData` paths.
- `skinning-animation`: posing requirements move to
  `efx.graphics.poseMesh`/`drawMesh` paths.
- `render-targets`: target requirements move to
  `efx.graphics.createRenderTarget`/`beginRenderTarget`/
  `endRenderTarget` paths.
- `post-fx`: chain and render-scale requirements move to
  `efx.graphics.setPostEffects`/`setRenderScale` paths.
- `particles`: system requirements move to
  `efx.graphics.createParticleSystem`/`drawParticles` paths.
- `billboards`: billboard requirement moves to
  `efx.graphics.drawBillboard`.
- `font-text`: font/text requirements move to
  `efx.graphics.loadFontData`/`createFont`/`drawText`/`measureText`
  paths.

## Impact

- **Bindings (three lockstep layers):** `src/runtime/runtime.c`
  (`EFX_FUNCS` split; a `graphics` object attached like
  `efx_api_register_input` does), `src/web/js/*.js` (the concatenated
  `api` literal gains an `api.graphics` object; per-domain files assign
  into it), `src/prelude/prelude.js` (install targets — `efx.graphics.*`
  — plus the four internal `efx.createMeshData` call sites in the make*
  primitives; regenerate `src/prelude/prelude.h` via
  `tools/gen_prelude.py`).
- **Error text:** engine error strings embed bare function names (e.g.
  `drawQuad: …`); the design keeps them byte-identical so the
  cross-runtime error catalog (`tests/scripts/s_error_catalog.js` +
  `.expected.txt`) stays valid apart from call paths in the trigger
  script.
- **Types & generated docs:** `gallery/src/api/efx.d.ts` (new
  `EfxGraphics` interface + `graphics` member; TSDoc examples re-pathed)
  and `gallery/src/api/efx.type-test.ts`; regenerate the committed
  `docs/api/` reference (`npm --prefix gallery run docs:markdown`).
- **Guidelines & ADR:** `docs/js-api.md` (the "One namespace" overview
  rule gains the sub-namespace organization rule; re-path examples and
  the vision-traceability table) and a short new ADR
  (`docs/decisions/0050-graphics-namespace.md`, number = next free)
  recording the namespace organization rule. `README.md` code mentions
  and the `vision.md` name mentions are re-pathed where they name moved
  root symbols.
- **Tests:** `tests/unit/api_tests.c` (~265 call sites), the portable
  script suite (`tests/scripts/s_*.js` — ~17 files) and
  `physics_smoke.js`, all ~49 golden-scene `main.js` files under
  `tests/goldens/`, the 3 web fixtures (`tests/fixtures/web/*/main.js`),
  and the `tests/CMakeLists.txt` scene list is unchanged (paths only).
- **Samples/examples:** all 15 `gallery/samples/curated/*/main.js` and
  `examples/browser/main.js`.
- **Verification:** no new test kind; the existing gate applies (Linux
  server pre-filter via `tools/verify_remote.py all`, then the
  four-target gate: Linux → Windows → macOS; Emscripten suite included).
  Golden pixels must be bit-identical — a pure re-path must not change
  any rendered frame.
- **Dependencies:** none added or removed.

## Non-goals

- No behavior change: no signature, default, validation, error-class, or
  error-message changes; no renaming of any function.
- No deprecated root aliases, no deprecation warnings, no
  compatibility shim (hard cut, pre-1.0, all consumers in-repo).
- `efx.whiteTexture`, `efx.loadText`, `efx.log`/`quit`/`args`, the hook
  registrations, and `mat4`/`vec3`/`quat` stay at the root; the
  `keyboard`/`mouse`/`window`/`physics`/`gamepad`/`audio` namespaces are
  untouched.
- No new graphics functionality, no new resource classes, no limit
  changes; the fixed-limits table and native-backed class list are
  unchanged.
- No milestone work: this implements no F1–F14 roadmap item — all are
  done; it is a post-roadmap cross-cutting API reorganization in the
  spirit of `audio-source-model` (ADR 0047).
- `docs/api/` is regenerated, never hand-edited; archived change folders
  and historical ADR texts are left as written.
