# Tasks

## 1. Native binding (desktop quickjs)

- [x] 1.1 In `src/api/api_text.c`, change `efx_js_measureText` to resolve the receiver from `this_val` via the `Font` liveness guard, shift the text/opts indices, and update the arity message to `measure requires (text, opts?)`. Verify: a headless script calling `font.measure('x')` succeeds and `font.measure(5)` throws `TypeError: measure requires (text, opts?)`.
- [x] 1.2 In `src/api/api_3d.c`, change `efx_js_poseMesh` to resolve the receiver `Mesh` from `this_val`, take `pose` as `argv[0]`, and update arity/rigless messages to `pose requires a pose` / `pose requires a Mesh with a rig`. Verify: `mesh.pose({ clip: 0, time: 0 })` on a rigged mesh poses it; a rigless mesh throws the rigless `TypeError`.
- [x] 1.3 In `src/api/api_lighting.c`, change `efx_js_setMeshSurfaceMaterial` to resolve the receiver `Mesh` from `this_val`, take `surfaceIndex`/`mat` as `argv[0]`/`argv[1]`, and update the arity message to `setSurfaceMaterial requires (surfaceIndex, mat)`. Verify: `mesh.setSurfaceMaterial(0, M)` binds, `mesh.setSurfaceMaterial(2, M)` on a 2-surface mesh throws `RangeError: surfaceIndex out of range`.
- [x] 1.4 In `src/api/api.c`, add `pose` and `setSurfaceMaterial` to `mesh_proto_funcs` and `measure` to `font_proto_funcs`, and update the `src/api/api.h` declarations to match the new signatures. Verify: `efx_api_init` registers the methods (a headless script enumerates the `Mesh`/`Font` prototype and sees all three).
- [x] 1.5 In `src/runtime/runtime.c`, remove `measureText`, `poseMesh`, and `setMeshSurfaceMaterial` from `GRAPHICS_FUNCS`. Verify: a headless script reads all three as `undefined` on `efx.graphics`.

## 2. Web binding (Emscripten bridge)

- [x] 2.1 In `src/web/js/core.js`, add `measure` to the `EfxFont` `methods` spec and `pose`/`setSurfaceMaterial` to the `EfxMesh` `methods` spec, moving the bodies from `src/web/js/text.js` and `src/web/js/render3d.js` and dropping the redundant receiver resolution (the class wrapper supplies `live(this)`). Verify: the web player runs a script that calls all three methods and produces the same results as desktop.
- [x] 2.2 In `src/web/js/text.js` and `src/web/js/render3d.js`, remove `measureText`, `poseMesh`, and `setMeshSurfaceMaterial` from the `api.graphics` literal and mirror the operation-token message updates from task group 1. Verify: the web error catalog output is byte-identical to the desktop output.

## 3. Type document and generated reference

- [x] 3.1 Update `gallery/src/api/efx.d.ts`: add `measure(text, opts?)` to `EfxFont`, add `pose(pose)` and `setSurfaceMaterial(surfaceIndex, mat)` to `EfxMesh`, and remove the three members from `EfxGraphics`; update TSDoc examples. Verify: `npx tsc --noEmit` over the gallery type-test passes.
- [x] 3.2 Update `gallery/src/api/efx.type-test.ts` with valid method calls and `@ts-expect-error` cases for the removed `efx.graphics.measureText`/`poseMesh`/`setMeshSurfaceMaterial` forms. Verify: the type-test compiles and the expected errors are reported.
- [x] 3.3 Regenerate the committed reference with `npm --prefix gallery run docs:markdown` and commit `docs/api/`. Verify: `npm --prefix gallery run docs:check` is clean.

## 4. Documentation and ADR

- [x] 4.1 Write `docs/decisions/0055-resource-operation-methods.md` (per `docs/decisions/TEMPLATE.md`) recording the method-vs-free-function rule and add its row to `docs/decisions/README.md`. Verify: the ADR is indexed and `openspec validate "resource-class-methods" --type change --strict` still passes.
- [x] 4.2 Update `docs/js-api.md`: state the resource-operation-method rule in Overview/Conventions and the Resource & memory model, re-path the `setMeshSurfaceMaterial`/`poseMesh`/`measureText` mentions, and update the vision-traceability rows (Phong material, skinning, text/font). Verify: a search for the removed `efx.graphics.*` names finds only historical/removal notes.

## 5. Migrate callers

- [x] 5.1 Migrate `tests/unit/api_tests.c` to the method forms and update the API-surface enumeration assertion. Verify: the `efx_api_tests` ctest suites pass.
- [x] 5.2 Migrate the portable script suite (`tests/scripts/s_4a_validation.js`, `s_4b_validation.js`, `s_5a_validation.js`, `s_7_skin_pose.js`) to the method forms. Verify: the portable script suites pass on desktop.
- [x] 5.3 Update `tests/scripts/s_error_catalog.js` triggers to the method forms and regenerate/update `tests/scripts/s_error_catalog.expected.txt` for the receiver/operation-token message changes. Verify: the desktop and web error catalogs are byte-identical.
- [x] 5.4 Migrate the web fixtures (`tests/fixtures/web/skin_probe/main.js`, `tests/fixtures/web/text_root/main.js`), the affected `tests/goldens/*/main.js` scenes, the curated samples (`gallery/samples/curated/*/main.js`, `manifest.json`), and `examples/browser/main.js` to the method forms. Verify: every migrated scene/script runs.

## 6. Verification

- [x] 6.1 Build headless (`cmake -B build -DEFX_HEADLESS=ON`) and run the non-golden ctest suites. Verify: all pass.
- [x] 6.2 Run the golden-image suite and confirm every affected frame is pixel-identical to the pre-change baseline (no re-baseline). Verify: the golden harness reports zero diffs.
- [x] 6.3 Run `python3 tools/verify_remote.py all <branch>` on the SSH verification server and fix until green. Verify: the Linux server pre-filter passes.
- [x] 6.4 Dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) and iterate Linux → Windows → macOS. Verify: all four targets are green.
- [x] 6.5 Run `openspec validate "resource-class-methods" --type change --strict` and `npm --prefix gallery run docs:check`. Verify: both are clean.
