# Tasks

## 1. Sample

- [x] 1.1 Add the authored modules `gallery/samples/curated/modules/lib/palette.js`, `gallery/samples/curated/modules/lib/orbit.js`, and `gallery/samples/curated/modules/data/scene.json`; add `gallery/scripts/pack-curated-modules.py` (deterministic: fixed entry timestamps, stored entries, no directory records) and generate `gallery/samples/curated/modules-showcase.zip`.
- [x] 1.2 Add `gallery/samples/curated/modules-showcase.js`: a lit 3D scene that composes `./lib/palette.js` (relative), `./lib/orbit` (extension fallback), and `./data/scene.json` (JSON module), registering `update`/`render` hooks; verify it loads and runs via `build/player --script gallery/samples/curated/modules-showcase.js --root gallery/samples/curated/modules-showcase.zip` (exit 0, no throw).
- [x] 1.3 Add the manifest entry (id `curated:modules-showcase`, title, category `Showcase`, description naming the F10 module features it demonstrates, `assets` pointing at the pack) to `gallery/samples/curated/manifest.json`; verify `gen-catalog` includes it.

## 2. Tests

- [x] 2.1 Add `smoke_showcase_modules` to `tests/CMakeLists.txt` (`--script` the sample against its pack, expect 0); note the pack in `gallery/samples/curated/CREDITS.md`.

## 3. Docs

- [x] 3.1 Add a short "Script modules" note to `README.md` (CommonJS `require`, synchronous from the dir/zip root, TypeScript `import` authoring → `docs/js-api.md`).
- [x] 3.2 Update the `gallery/` bullet in `AGENTS.md` current state to mention the modules showcase.

## 4. Verification

- [x] 4.1 Run `npm --prefix gallery run check` and `npm --prefix gallery run build`; verify the build succeeds and the sample is in `gallery/src/samples/generated.json`.
- [x] 4.2 Run `python3 tools/verify_remote.py gallery <branch>`; verify `gallery smoke PASSED` and the native module smoke case passes.
- [x] 4.3 Run the four-target gate (`gh workflow run ci.yml --ref <branch>`) after 4.2 is green and confirm all targets pass.
