# Tasks

## 1. Engine bindings — three lockstep layers

- [x] 1.1 `src/runtime/runtime.c`: move the 16 native entries (`setClearColor`, `drawQuad`, `setBlendMode`, `createMesh`, `drawMesh`, `poseMesh`, `setMeshSurfaceMaterial`, `beginRenderTarget`, `endRenderTarget`, `setRenderScale`, `loadFontData`, `drawText`, `measureText`, `drawBillboard`, `drawSprites`, `drawParticles`) from `EFX_FUNCS` into a new `GRAPHICS_FUNCS` table; create the `graphics` object in `install_efx_api()` and attach it with `JS_SetPropertyStr(ctx, efx, "graphics", graphics)` (pattern: `efx_api_register_input`). Verify: `cmake -B build -DEFX_HEADLESS=ON && cmake --build build -j4` compiles clean.
- [x] 1.2 `src/prelude/prelude.js`: re-point the 17 prelude-installed members (`setLight`, `setDirectionalLight`, `setCamera2D`, `setCamera3D`, `createImageData`, `createTexture`, `createRenderTarget`, `loadImage`, `loadMeshData`, `createMeshData`, `createFont`, `setPostEffects`, `createParticleSystem`, `makeCube`, `makePlane`, `makeSphere`, `makeCapsule`) to `efx.graphics.<name>` (guard `efx.graphics = efx.graphics || {}`), and re-path the four internal `efx.createMeshData` calls in the make* primitives to `efx.graphics.createMeshData`; regenerate `src/prelude/prelude.h` with `python3 tools/gen_prelude.py`. Verify: `python3 tools/gen_prelude.py --check` passes and the headless build still compiles.
- [ ] 1.3 `src/web/js/*.js`: add the nested `graphics: { … }` object to the concatenated `api` literal (opened in `render2d.js`) and move the web entries of all 33 functions into it (`render2d`, `resource`, `text`, `target_post`, `particles`, `render3d`); leave the web `natives` twin untouched. Verify: the Emscripten build (`source /opt/emsdk/emsdk_env.sh && emcmake …`) compiles; membership is proven by task 2.1 on both runtimes.

## 2. Membership guard test

- [x] 2.1 `tests/unit/api_tests.c`: add an enumeration test asserting `efx.graphics` exposes exactly the 33 names, none of the 33 exists at the `efx` root, and the root still exposes `log`, `quit`, `args`, `registerUpdateHook`, `registerRenderHook`, `whiteTexture`, `loadText`, `mat4`, `vec3`, `quat`, and the six domain namespaces. Verify: the new test passes on the headless build (`ctest -R api_tests` via the unit suite) and in the Emscripten ctest run (task 7.3).

## 3. Test corpus migration

- [x] 3.1 `tests/unit/api_tests.c`: re-path every root-qualified call of the 33 functions (~265 sites) to `efx.graphics.<fn>`. Verify: the full unit suite passes headless (`cmake --build build -j4 && ctest --test-dir build -E golden`).
- [x] 3.2 `tests/scripts/*.js` (all `s_*.js` suites + `physics_smoke.js`) and `tests/fixtures/web/{gltf_probe,skin_probe,text_root}/main.js`: re-path all root-qualified calls. Verify: script tests pass headless and the web fixtures run in the cross-runtime compare (task 7.3).
- [x] 3.3 `tests/goldens/<scene>/main.js`: re-path all ~49 scenes (the `tests/CMakeLists.txt` scene list is unchanged). Verify: every golden ctest matches its committed PNG byte-identically on the verification server (task 7.3) — a diff means an accidental behavior change, never a re-baseline.
- [x] 3.4 Error catalog: run `s_error_catalog.js` on the desktop runtime and confirm the output is byte-identical to `s_error_catalog.expected.txt` (per design D3 messages stay bare-function-named; if any message turns out to embed a root-qualified `efx.<fn>` name, re-path that message and update the fixture in the same task). Verify: byte-identical catalog output, and the cross-runtime compare reports no divergence.

## 4. Samples and examples

- [x] 4.1 Re-path all 15 curated samples `gallery/samples/curated/*/main.js` (including `modules-showcase`'s entry flow). Verify: repo grep finds no root-qualified moved name under `gallery/samples/`.
- [x] 4.2 Re-path `examples/browser/main.js`. Verify: repo grep finds no root-qualified moved name under `examples/`.

## 5. Type document and generated reference

- [x] 5.1 `gallery/src/api/efx.d.ts`: extract `interface EfxGraphics` with the 33 declarations (TSDoc summaries and `@example` blocks re-pathed), delete the members from `interface Efx`, add `readonly graphics: EfxGraphics`; re-path `gallery/src/api/efx.type-test.ts`. Verify: the gallery typecheck passes (`npm --prefix gallery run build` or its `tsc` step).
- [x] 5.2 Regenerate the committed reference: `npm --prefix gallery run docs:markdown`, then `npm --prefix gallery run docs:check`. Verify: `docs/api/` shows the `efx.graphics` pages and `docs:check` passes.

## 6. Guidelines, README, ADR, current-state notes

- [x] 6.1 `docs/js-api.md`: amend the "One namespace" overview bullet with the sub-namespace organization rule (root = runtime/lifecycle facilities + domain sub-namespaces; graphics functions live in `efx.graphics`), re-path the layering examples, the resource-table creator columns, and the vision-traceability table entries. Verify: a read-through matches `efx.d.ts` and no stale `efx.<moved-name>` reference remains.
- [x] 6.2 Re-path `README.md` (L11/L13 code mentions) and check `vision.md`'s name mention (L40), re-pathing only if it names a moved root symbol. Verify: repo grep for root-qualified moved names over `*.md` (excluding `openspec/`, `docs/api/`, `docs/decisions/` historical ADRs) is clean.
- [x] 6.3 Write the ADR `docs/decisions/0050-graphics-namespace.md` (confirm the next free number in `docs/decisions/`) per `TEMPLATE.md`: the namespace-organization rule (root holds lifecycle/runtime facilities and domain sub-namespaces; `efx.graphics` joins `efx.audio`/`efx.physics`/`efx.input`/`efx.gamepad`), the hard-cut/no-alias decision, and the byte-identical error-text rule; add it to the `docs/decisions/README.md` index. Verify: the ADR file exists, is indexed, and follows the template.
- [x] 6.4 Update the `AGENTS.md` current-state notes: add the post-roadmap change line (change folder + ADR pointer) alongside the existing follow-up entries. Verify: the note names the archived-folder convention used by `audio-source-model`.

## 7. Verification gate (harness: headless ctest smoke suite, golden-image harness, Emscripten suite)

- [ ] 7.1 Final audit: word-bounded repo grep for all 33 names as `efx.<name>` — zero hits outside `openspec/changes/archive/`, historical ADR text, and this change folder. Verify: the grep output is empty (or only archived/historical lines).
- [x] 7.2 Local headless suite: fresh `cmake -B build -DEFX_HEADLESS=ON`, build `-j4`, `ctest --test-dir build -E golden` (unit + portable script tests; no display). Verify: zero failures.
- [x] 7.3 Server pre-filter (per AGENTS.md): commit the change to a branch, push, and run `python3 tools/verify_remote.py all <branch>` — native ctest including all golden scenes (Xvfb/llvmpipe) and the Emscripten golden suite in pinned Chrome. Fix and re-verify until green; do not dispatch CI before this passes.
- [x] 7.4 Four-target gate: dispatch `gh workflow run ci.yml --ref <branch>` and confirm Linux → Windows → macOS all green with artifacts published (the web bundle and curated-samples pack build from the migrated sources). Verify: the run summary shows all four targets green.
- [ ] 7.5 After the gate is green, merge the branch to `main` and push (Pages deploys on push to `main`); the change is then ready for `/opsx-archive`. Verify: `main` contains the change and the Pages workflow completes.
