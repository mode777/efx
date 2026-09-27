# Tasks

## 1. Engine host entry-source hook

- [x] 1.1 In `src/web/entry.js`, before the resource-root `main.js` read, prefer a host-supplied `globalThis.__efx_main_js` source, delete it before evaluation, and skip root resolution when it is present; verify by loading the Node web player (`build-web/player.js`) from a small wrapper that sets the global, and asserting the override script's `efx.log` output appears and the process exits 0
- [x] 1.2 Verify the no-override path is unchanged: run the existing Emscripten ctest and cross-runtime web compare against a build with no `__efx_main_js` set and confirm they pass

## 2. Sample catalog generation

- [x] 2.1 Add a build-time generator that scans `tests/goldens/*/main.js` and emits the gallery catalog (name, source, category) sorted by name; verify the output contains exactly one entry per golden scene and is stable across two runs
- [x] 2.2 Add the curated showcase samples with metadata (title, category, description) and verify each runs to completion through the player without browser/Node references

## 3. Gallery host application

- [x] 3.1 Scaffold the gallery project (Vite + TypeScript + Svelte) and verify `npm ci && npm run build` emits a static `dist/`
- [x] 3.2 Implement the runner page and the `postMessage` handshake, copy `player_web.{js,wasm,data}` into the bundle, and verify a selected sample renders in headless Chrome with no console errors
- [x] 3.3 Build the three panels — catalog list, 4:3 application area (iframe-per-run), and Monaco-from-CDN editor with Run/Reset and inline error surfacing — and verify selecting, editing+Run, Reset, and a thrown sample error all behave without breaking the shell
- [x] 3.4 Apply the PlayStation-2-era styling tokens (dark palette, glowing accents, beveled panels) across the shell and verify a screenshot shows a consistent retro presentation with the app as the visual focus

## 4. API type document

- [x] 4.1 Author `efx.d.ts` describing the current public API (F1–F5) and register it with Monaco via `addExtraLib`; verify the editor offers completion for `efx.` members and reports a type error for a mistyped member
- [x] 4.2 Add `efx.d.ts` to the `AGENTS.md` doc map beside `docs/js-api.md` with the same-change update rule; verify AGENTS.md names it and the rule

## 5. Build and Pages deployment

- [x] 5.1 Update `.github/workflows/pages.yml` to build `player_web`, copy its output into the gallery bundle, run `npm ci && npm run build`, check every expected output exists, and deploy the gallery `dist/`; verify the steps reproduce locally and `dist/` contains the player module+data, catalog, and editor assets
- [x] 5.2 Retire `examples/browser/main.js` as the gallery and update its references (`CMakeLists.txt` `LINK_DEPENDS`/preload, `AGENTS.md`); verify the Emscripten build still succeeds and the web target still runs

## 6. Documentation and ADR

- [x] 6.1 Write `docs/decisions/0030-<slug>.md` for the host↔engine embedding contract (iframe-per-run, host entry-source hook, delete-before-eval, rejected live-eval/reset) and add its row to `docs/decisions/README.md`; verify the file and index row exist
- [x] 6.2 Update `AGENTS.md` current-state and verification sections for the gallery, its build, and the new ADR; verify the sections describe the new layout
- [x] 6.3 Confirm this change requires no `docs/js-api.md` delta (no script-visible API) and record that confirmation in the change notes; verify `git diff` shows no script-facing API change in `src/api/`, `src/web/bridge.c`, or `src/prelude/`

## 7. Verification

- [x] 7.1 Add the headless gallery smoke tool that serves the built bundle in pinned headless Chrome, runs several samples in sequence, and asserts the page boots, renders a non-blank canvas, and logs no console errors (including no context-exhaustion); verify it passes locally
- [x] 7.2 Run the ctest smoke suite and the golden-image harness on the verification server (`tools/verify_remote.py all`) to confirm the entry hook does not alter engine behavior; verify green on the native and Emscripten golden jobs
- [x] 7.3 Verify the deployed site after merge to `main`: the gallery boots under the project Pages subpath, every bundle file (including the preloaded data) returns 200, and selecting/editing/running a sample works on the live URL
