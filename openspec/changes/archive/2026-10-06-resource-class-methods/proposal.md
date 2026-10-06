# Proposal

## Why

The API's argument convention (ADR 0053) already leads every call with its
**subject** — the thing drawn, created, or queried. For three operations the
subject is a native-backed class instance, yet the operation lives as a free
function in `efx.graphics` and re-takes that instance as its first argument:

- `efx.graphics.poseMesh(mesh, pose)` poses `mesh`;
- `efx.graphics.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)` binds a
  material to a surface of `mesh`;
- `efx.graphics.measureText(text, font, opts?)` measures against `font`.

Every other operation on a native-backed class is already a method
(`ParticleSystem.emit`/`start`/`set`, `Audio.stop`/`pause`, `Body.applyImpulse`,
`Character.moveAndSlide`, `destroy()`). Leaving these three as free functions
is inconsistent: the subject is repeated as an argument, and the methods
surface of `Mesh`/`Font` is incomplete. Making them methods completes the
class model, removes the redundant subject argument, and lets the namespace
hold constructors/factories and stateless operations only.

## What Changes

- **`measureText` → `Font.measure(text, opts?)`. BREAKING.** The measurement
  operation moves onto the `Font` it measures with. Signature becomes
  `font.measure(text, opts?)` returning the same `{ width, height, lines }`
  bounds; all layout options and error behavior are unchanged apart from the
  removed `font` argument.
- **`poseMesh` → `Mesh.pose(pose)`. BREAKING.** The posing operation moves
  onto the `Mesh` it poses. Signature becomes `mesh.pose(pose)`, where `pose`
  is a single sample or an array of samples; clip lookup, weight
  normalization, wrapping, and errors are unchanged apart from the removed
  `mesh` argument.
- **`setMeshSurfaceMaterial` → `Mesh.setSurfaceMaterial(surfaceIndex, mat)`.
  BREAKING.** The per-surface binding moves onto the `Mesh`. Signature becomes
  `mesh.setSurfaceMaterial(surfaceIndex, mat)`; material snapshotting, the
  `null`-to-default reset, range/type errors, and retained maps are unchanged
  apart from the removed `mesh` argument.
- **Hard cut, no aliases.** The three `efx.graphics` members are removed, not
  deprecated: `efx.graphics.measureText`, `efx.graphics.poseMesh`, and
  `efx.graphics.setMeshSurfaceMaterial` no longer exist. All consumers are
  in-repo (unit tests, portable script tests, golden scenes, curated samples,
  web fixtures, examples) and are migrated in the same change; there is no
  external script corpus to keep alive.
- **Durable design rule.** The API design guidelines gain the rule this
  change establishes: an operation whose subject is a native-backed class
  instance is a **method on that class**; `efx.graphics` holds constructors,
  factories, and stateless operations, and does not re-take a class instance as
  a free-function argument.
- **Nothing else moves.** `drawText`, `drawMesh`, `createMesh`,
  `createFont`, `loadFontData`, and every other graphics member keep their
  names and paths. The class read-only query properties and `destroy()` are
  unchanged. `Font.measure` keeps native validation (a hot query path,
  ADR 0049); `Mesh.pose` and `Mesh.setSurfaceMaterial` likewise.

## Capabilities

### New Capabilities

- None — this is a surface reorganization of existing behavior, not a new
  capability.

### Modified Capabilities

- `js-api`: **structural.** MODIFY "Graphics namespace API" to drop
  `measureText`, `poseMesh`, and `setMeshSurfaceMaterial` from the
  `efx.graphics` member list; MODIFY "Font and text API" so measurement is
  `Font.measure(text, opts?)`; MODIFY "Skinned mesh data and implicit rig
  payload" so posing is `Mesh.pose`; MODIFY "Gallery type document accuracy"
  for the method call shapes; ADD a "Resource operation methods" requirement
  pinning the method-vs-function rule.
- `font-text`: MODIFY "Typesetting and word wrapping" and "Text measurement"
  so measurement is `Font.measure(text, opts?)`.
- `skinning-animation`: MODIFY "Script-driven posing with poseMesh" (renamed)
  so posing is `Mesh.pose(pose)`.
- `3d-core`: MODIFY "Mesh upload and lifecycle" so rebinding is
  `Mesh.setSurfaceMaterial(surfaceIndex, mat)`.
- `lighting`: MODIFY "Phong material model" and "Per-surface material
  binding" so rebinding is `Mesh.setSurfaceMaterial(surfaceIndex, mat)`.

## Impact

- **Bindings (three lockstep layers):** `src/runtime/runtime.c` (drop the
  three names from `GRAPHICS_FUNCS`), `src/api/api.c` (register the three as
  prototype functions on the `Mesh` and `Font` class specs), the C
  implementations `src/api/api_3d.c` (`efx_js_poseMesh`),
  `src/api/api_lighting.c` (`efx_js_setMeshSurfaceMaterial`), and
  `src/api/api_text.c` (`efx_js_measureText`) — read the subject from
  `this_val` instead of `argv[0]` and shift the argument indices; the web
  bridge `src/web/js/core.js` (add `methods` to the `EfxMesh`/`EfxFont`
  class specs) with the moved logic from `src/web/js/render3d.js` and
  `src/web/js/text.js`; `src/api/api.h` declarations.
- **Error text:** engine error strings embed bare operation names (e.g.
  `poseMesh requires a Mesh with a rig`). The design keeps the operation names
  in messages (or settles new canonical text) so the cross-runtime error
  catalog (`tests/scripts/s_error_catalog.js` + `.expected.txt`) is updated
  deliberately, not accidentally.
- **Types & generated docs:** `gallery/src/api/efx.d.ts` (`EfxMesh` gains
  `pose`/`setSurfaceMaterial`; `EfxFont` gains `measure`; the `EfxGraphics`
  members are removed) and `gallery/src/api/efx.type-test.ts`; regenerate the
  committed `docs/api/` reference (`npm --prefix gallery run docs:markdown`).
- **Guidelines & ADR:** `docs/js-api.md` (the Overview/Conventions and
  Resource & memory model sections gain the method rule; re-path examples and
  the vision-traceability table) and a new ADR
  (`docs/decisions/0055-resource-operation-methods.md`, next free number)
  recording the rule.
- **Tests:** `tests/unit/api_tests.c` (~31 call sites), the portable script
  suite (`tests/scripts/s_4a_validation.js`, `s_4b_validation.js`,
  `s_5a_validation.js`, `s_7_skin_pose.js`, `s_error_catalog.js`), the web
  fixtures (`tests/fixtures/web/skin_probe/main.js`,
  `tests/fixtures/web/text_root/main.js`), and the ~18 golden-scene
  `main.js` files under `tests/goldens/` that call the three operations.
- **Samples/examples:** the affected `gallery/samples/curated/*/main.js`
  (fox-walk, hello-cube, light-show, modules-showcase, physics-showcase,
  texture-showcase), `gallery/samples/curated/manifest.json`, and
  `examples/browser/main.js`.
- **Verification:** no new test kind; the existing gate applies (Linux server
  pre-filter via `tools/verify_remote.py all`, then the four-target gate:
  Linux → Windows → macOS; Emscripten suite included). Golden pixels must be
  bit-identical — a pure surface move must not change any rendered frame.
- **Dependencies:** none added or removed.

## Non-goals

- No behavior change beyond the access path: no layout, posing, material, or
  lifecycle semantics change; no default, error-class, or error-message change
  (except where a message must name the new method form, settled in design).
- No other `efx.graphics` function moves to a class method; `drawText`,
  `drawMesh`, `drawQuad`, and friends stay namespace functions.
- No new resource class, no new read-only query property, no limit changes;
  the native-backed class list and fixed-limits table are unchanged.
- No deprecated aliases, no deprecation warnings, no compatibility shim
  (hard cut, pre-1.0, all consumers in-repo).
- No milestone work: this implements no F1–F14 roadmap item — all are done;
  it is a post-roadmap cross-cutting API reorganization.
- `docs/api/` is regenerated, never hand-edited; archived change folders and
  historical ADR texts are left as written.
