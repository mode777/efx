# Tasks

## 1. Engine bindings — native + prelude

- [x] 1.1 `src/runtime/runtime.c`: replace the `args` `JS_CFUNC_DEF` with `JS_CGETSET_DEF("args", efx_js_args, NULL)`, move the `whiteTexture` cgetset from `EFX_FUNCS` into `GRAPHICS_FUNCS`, and add an `IO_FUNCS` table (`loadText`, `loadData`) attached as `efx.io` in `install_efx_api()` (pattern: the `graphics` object). Verify: `cmake -B build -DEFX_HEADLESS=ON && cmake --build build -j4` compiles clean.
- [x] 1.2 `src/api/api.c`: change `efx_js_args` to the getter signature (builds the same array from host state). `src/api/api_resource.c`: add `efx_js_loadData` (validate a string path, read with `efx_resource_read`, return `JS_NewUint8ArrayCopy`, free the native buffer) with messages mirroring `loadText` (`loadData requires a path string` / the existing resource error texts). Verify: the headless build compiles and the new unit case in 2.1 passes.
- [x] 1.3 `src/web/bridge_resource.c`: add `_efx_bridge_load_data(const char *path, int *out_len)` returning a malloc'd byte buffer (length in the caller cell) or NULL. `src/web/js/render2d.js`: move `whiteTexture` into the `api.graphics` object, remove the literal `args`/`loadText` entries, define `args` via `Object.defineProperty(api, 'args', { get: … })` after the literal is assembled, and assign `api.io = { loadText, loadData }`; `loadData` allocates a length cell, copies `HEAPU8`, and frees both allocations. Verify: the Emscripten build (`source /opt/emsdk/emsdk_env.sh && emcmake …`) compiles; membership is proven by task 2.1 on both runtimes.
- [x] 1.4 `src/prelude/prelude.js`: assign the math objects to `efx.math.mat4`/`efx.math.vec3`/`efx.math.quat`, install `efx.color` with the 17 frozen constants (CSS basic 16 + `transparent`; levels `0.5`/`0.75`), and re-path the three internal `efx.loadText` CommonJS-loader calls to `efx.io.loadText`; regenerate `src/prelude/prelude.h` with `python3 tools/gen_prelude.py`. Verify: `python3 tools/gen_prelude.py --check` passes and the headless build compiles.

## 2. Membership and behavior guard tests

- [x] 2.1 `tests/unit/api_tests.c`: extend the namespace guard to assert `efx.math` is exactly `{mat4,vec3,quat}`, `efx.io` is exactly `{loadText,loadData}`, `efx.color` is exactly the 17 named constants with each value a frozen 4-number tuple, `efx.graphics` is the 33 functions plus `whiteTexture`, `efx.args` is a non-function array, the root exposes only the lifecycle members plus the domain namespaces, and no moved name exists at the root. Verify: the new case passes headless (`ctest -R api_tests`) and in the Emscripten ctest run (task 7.3).
- [x] 2.2 Add a portable `loadData` script case with a small binary fixture (register it in `tests/CMakeLists.txt`): assert the returned `Uint8Array` length and bytes match, that a second load is unaffected by mutating the first result, and that a missing path throws the same error on both runtimes. Verify: the case passes headless and in the Emscripten suite.

## 3. Test corpus migration

- [x] 3.1 `tests/unit/api_tests.c`: re-path every `efx.whiteTexture` → `efx.graphics.whiteTexture`, `efx.loadText` → `efx.io.loadText`, `efx.mat4`/`efx.vec3`/`efx.quat` → `efx.math.*`, and the `efx.args()` call to the `efx.args` property. Verify: the full unit suite passes headless (`ctest --test-dir build -E golden`).
- [x] 3.2 `tests/scripts/*.js`: rewrite `s_args.js` to read the `efx.args` property; re-path `whiteTexture`/`loadText`/math in the `s_*.js` suites; update `s_error_catalog.js` trigger lines to the new paths and append the `loadData` cases to `s_error_catalog.expected.txt`. Verify: script tests pass headless and the catalog output is byte-identical on both runtimes (task 7.3).
- [x] 3.3 `tests/goldens/<scene>/main.js`: re-path every `efx.whiteTexture` (~49 scenes; the `tests/CMakeLists.txt` scene list is unchanged). Verify: every golden ctest matches its committed PNG byte-identically on the verification server (task 7.3) — a diff means an accidental behavior change, never a re-baseline.
- [x] 3.4 `tests/fixtures/web/*/main.js`: re-path `whiteTexture`/`loadText`/math calls. Verify: the web fixtures run in the cross-runtime compare (task 7.3).

## 4. Samples and examples

- [x] 4.1 `gallery/samples/curated/*/main.js`: re-path `efx.whiteTexture` and substitute an `efx.color` constant wherever a color literal exactly matches one (custom palette literals stay as literals). Verify: repo grep finds no root-qualified moved name under `gallery/samples/`, and `npm --prefix gallery run build` succeeds.
- [x] 4.2 `examples/browser/main.js`: re-path moved calls and use color constants where literals match. Verify: repo grep finds no root-qualified moved name under `examples/`.

## 5. Type document and generated reference

- [x] 5.1 `gallery/src/api/efx.d.ts`: add `interface EfxMath` (`mat4`/`vec3`/`quat`), `interface EfxIo` (`loadText`/`loadData`), and `interface EfxColor` (the 17 `Color` constants); delete `args()`/`whiteTexture`/`loadText`/`mat4`/`vec3`/`quat` from `interface Efx` and add `readonly args: string[]`, `readonly math`, `readonly io`, `readonly color`, and `whiteTexture` on `EfxGraphics`; re-path TSDoc examples. Re-path and extend `gallery/src/api/efx.type-test.ts` (new forms compile; removed root forms and `efx.args()` are `@ts-expect-error`). Verify: the gallery typecheck/`tsc` step passes.
- [x] 5.2 Regenerate the committed reference: `npm --prefix gallery run docs:markdown`, then `npm --prefix gallery run docs:check`. Verify: `docs/api/` shows the `efx.math`/`efx.io`/`efx.color` pages, `args` as a property, `whiteTexture` under `efx.graphics`, and `docs:check` passes.

## 6. Guidelines, README, ADR, current-state notes

- [x] 6.1 `docs/js-api.md`: update the Overview "One namespace" rule (root = lifecycle/runtime facilities + domain sub-namespaces; math/io/color live in their namespaces; `args` is a read-only property), the colors convention (named constants), the resource table/creator columns (`efx.io.loadText`/`loadData`), and the vision-traceability table entries. Verify: a read-through matches `efx.d.ts` and no stale `efx.<moved-name>` reference remains.
- [x] 6.2 Re-path `README.md` code mentions (`efx.args()` and any moved root symbols) and check `vision.md`'s name mentions, re-pathing only if they name a moved root symbol. Verify: repo grep for root-qualified moved names over `*.md` (excluding `openspec/`, `docs/api/`, historical ADRs) is clean.
- [x] 6.3 Write the ADR `docs/decisions/0051-*.md` (confirm the next free number in `docs/decisions/`) per `TEMPLATE.md`: the completed namespace-organization rule (root = lifecycle/runtime + domain sub-namespaces; `math`/`io`/`color` join `graphics`/input/physics/gamepad/audio), a constants namespace is frozen plain data with no functions, `args` is a read-only property, and `loadData` returns a `Uint8Array` copy; add it to the `docs/decisions/README.md` index. Verify: the ADR file exists, is indexed, and follows the template.
- [x] 6.4 Update the `AGENTS.md` current-state notes: add the post-roadmap change line (change folder + ADR pointer) alongside the existing follow-up entries. Verify: the note names the archived-folder convention used by `audio-source-model`.

## 7. Verification gate (harness: headless ctest smoke suite, golden-image harness, Emscripten suite)

- [x] 7.1 Final audit: word-bounded repo grep for `efx.whiteTexture`, `efx.loadText`, `efx.mat4`, `efx.vec3`, `efx.quat`, and `efx.args(` — zero hits outside `openspec/changes/archive/`, historical ADR text, and this change folder. Verify: the grep output is empty (or only archived/historical lines).
- [x] 7.2 Local headless suite: fresh `cmake -B build -DEFX_HEADLESS=ON`, build `-j4`, `ctest --test-dir build -E golden` (unit + portable script tests; no display). Verify: zero failures.
- [x] 7.3 Server pre-filter (per AGENTS.md): commit the change to a branch, push, and run `python3 tools/verify_remote.py all <branch>` — native ctest including all golden scenes (Xvfb/llvmpipe) and the Emscripten golden suite in pinned Chrome. Fix and re-verify until green; do not dispatch CI before this passes.
- [x] 7.4 Four-target gate: dispatch `gh workflow run ci.yml --ref <branch>` and confirm Linux → Windows → macOS all green with artifacts published (the web bundle and curated-samples pack build from the migrated sources). Verify: the run summary shows all four targets green.
- [x] 7.5 After the gate is green, merge the branch to `main` and push (Pages deploys on push to `main`). Verify: `main` contains the change and the Pages workflow completes.
- [x] 7.6 Reconcile the pre-existing malformed `openspec/specs/js-api/spec.md` (stray `## ADDED Requirements` / `## MODIFIED Requirements` headers from the not-yet-archived `efx-graphics-namespace` sync) — either archive `efx-graphics-namespace` first or normalize the main spec's headers — then confirm `npx openspec validate efx-namespace-consolidation --strict` reports no archive-refusal. Verify: the js-api delta can be applied and the change is ready for `/opsx-archive`.
