# Tasks

## 1. Curated asset-pack plumbing

- [x] 1.1 Extend `gallery/samples/curated/manifest.json` entries to accept an optional `assets` zip path and update `curatedSamples()` in `gallery/scripts/gen-catalog.mjs` to copy that zip to `gallery/public/samples/<id>.zip` and set `sample.assets`, mirroring the golden path. Verify by running `npm --prefix gallery run gen:catalog` and confirming a curated entry with `assets` appears in `gallery/src/samples/generated.json` and its zip in `gallery/public/samples/`.
- [x] 1.2 Confirm existing curated entries (no `assets`) are unchanged by the extension and the generated catalog still builds. Verify `npm --prefix gallery run check` passes.

## 2. Showcase assets (CC0)

- [x] 2.1 Fetch a CC0 texture (Kenney "Prototype Textures", fallback ambientCG), keep only the file(s) the sample needs, downscale if larger than 512px, and commit the runtime archive at `gallery/samples/curated/texture-showcase.zip`. Verify the zip lists the expected root-relative files and is tens of KiB.
- [x] 2.2 Fetch the CC0 Avocado model (Khronos glTF-Sample-Assets, Microsoft), keep `Avocado.gltf` + `Avocado.bin`, drop the normal/occlusion/metallic-roughness texture references the importer ignores, downscale the base-color PNG, and commit `gallery/samples/curated/gltf-showcase.zip`. Verify the zip lists `Avocado.gltf`, `Avocado.bin`, and the base-color image, and is under ~1 MiB.
- [x] 2.3 Write `gallery/samples/curated/CREDITS.md` recording, per committed file, author, source URL, license (CC0 1.0), and the optimization recipe so each zip can be regenerated. Verify every file in both zips has a matching entry.

## 3. Showcase samples

- [x] 3.1 Add `gallery/samples/curated/texture-showcase.js`: `loadTexture` a real image, draw it full and via `sourceRect`, and tile it with a repeat sampler on a 3D surface; category "Textures". Verify it loads and draws against its zip with `player --script gallery/samples/curated/texture-showcase.js --root gallery/samples/curated/texture-showcase.zip` exiting 0.
- [x] 3.2 Add `gallery/samples/curated/gltf-showcase.js`: `loadMeshData` the model, set lights, and spin it; category "Assets"; use the imported material as-is unless the visual check requires a `setMeshSurfaceMaterial` override. Verify it loads and draws against its zip with the same `player --script … --root …` form exiting 0.
- [x] 3.3 Add both entries to `gallery/samples/curated/manifest.json` with id, title, category, description (naming the CC0 source), and `assets`. Verify the descriptions render in the catalog and the samples appear in their categories.
- [x] 3.4 Regenerate the catalog (`npm --prefix gallery run gen:catalog`) and commit the refreshed `gallery/src/samples/generated.json`. Verify both new samples are present with their `assets` URLs.

## 4. Tests

- [ ] 4.1 Add committed native player smoke tests in `tests/CMakeLists.txt` (mirroring `smoke_6a_root_zip`) that run each gallery sample with `--script <sample>.js --root <sample>.zip` and `EXPECT 0`. These are player tests, so they register only in a display-capable (non-headless) build; verify `ctest -R showcase` passes on the verification server.
- [ ] 4.2 Run the gallery smoke against the built site (`node tools/run_gallery_smoke.mjs` with `CHROME_SHELL_PATH` set) and confirm no console errors, including for the two new samples. Verify it prints `gallery smoke PASSED`.

## 5. Documentation

- [x] 5.1 Update the `gallery/` bullet in `AGENTS.md` current state to note that curated showcase samples can ship committed CC0 asset packs. Verify the text matches the implemented manifest/gen-catalog behavior.

## 6. Verification

- [x] 6.1 Build the gallery (`npm --prefix gallery ci && npm --prefix gallery run build`) and confirm the new samples and their zips are in `gallery/dist/`. Verify the build succeeds and `gallery/dist/samples/` contains both zips.
- [ ] 6.2 Run the native ctest smoke suite (including the new showcase player tests, which need a display) on the verification server, plus the headless unit suite locally. Verify all pass.
- [ ] 6.3 Run `python3 tools/verify_remote.py all <branch>` on the verification server and confirm the native suites, web goldens, gallery smoke, and cross-runtime compare are green.
- [ ] 6.4 Dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) only after 6.3 is green, and confirm all four targets pass.
