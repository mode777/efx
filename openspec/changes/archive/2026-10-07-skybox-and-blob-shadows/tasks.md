# Tasks

## 1. Native material: `unlit`

- [x] 1.1 Add an `unlit` field to `efx_material` in `src/render/render.h`, default it to `0` in `efx_material_default` (`src/render/render_records.c`), and verify `ctest`'s render unit tests still pass
- [x] 1.2 Grow the material wire block from 18 to 19 floats in every mirror enumerated in `render.h`: `wire_mat_from_block`/material-flags in `src/api/api_3d.c`, `__efxMaterialWire` in `src/prelude/prelude.js`, `__efxMaterial` in `src/web/js/core.js`, and `bridge_mat_from_wire` in `src/web/bridge_render3d.c`; verify the desktop and web builds compile and the material bind unit tests pass
- [x] 1.3 Add the unlit branch to `shaders/mesh.glsl` (carry the flag in `mat_params.y`; final color = `diffuse.color × M_diffuse × albedo`, clamp, skip all lights) and regenerate `shaders/mesh.h` with the pinned `sokol-shdc` (`cmake -B build -DEFX_SHDC=...`); verify the regenerated header is committed and the build links
- [x] 1.4 Fill the new uniform in `play_mesh_record` (`src/platform/pipeline.c`) from the snapshotted material, and verify a minimum-viable scene renders an unlit textured surface unmodified by the light bank

## 2. Native render state: `depthWrite`

- [x] 2.1 Add a `depth_write` flag to `efx_mesh_record` (`src/render/render.h`) and set it in `efx_render_mesh` recording (`src/render/render_records.c`), value-snapshotted with the rest of the record; verify render unit tests cover the flag
- [x] 2.2 Extend the mesh pipeline matrix in `src/platform/pipeline.c` with depth-write-disabled variants (`compare = LESS_EQUAL`, `write_enabled = false`) across the existing 3 blends × 2 windings, and select the variant from the record flag in `play_mesh_record`; verify the build creates all variants and a no-write draw does not occlude a later farther mesh
- [x] 2.3 Thread the `depthWrite` option through the native draw entry point (`src/api/api_3d.c`/`api.h`) and both bindings (`src/web/bridge_render3d.c`), defaulting to `true`; verify the desktop and web builds compile

## 3. JS validation and primitives

- [x] 3.1 Accept and validate `unlit` (boolean, else `TypeError`) in the shared material validator (`src/prelude/prelude.js` and `src/web/js/core.js`), and reject it where other unknown fields are rejected; verify `tests/unit/api_tests.c` gains a case for `unlit: 1` throwing `TypeError`
- [x] 3.2 Accept and validate `depthWrite` (boolean, default `true`, else `TypeError`) in the `drawMesh` option validator on both runtimes; verify the option-bag unit test rejects `depthWrite: 1`
- [x] 3.3 Add `inverted?` (boolean, default `false`, else `TypeError`) to `makeCube` and `makeSphere` in `src/prelude/prelude.js`, reversing triangle winding and negating normals when set; verify a unit or script test draws the inside of an inverted cube/sphere and a non-inverted primitive is culled from the same camera
- [x] 3.4 Regenerate the embedded prelude (`tools/gen_prelude.py`) and confirm `gen_prelude.py --check` passes with the committed `src/prelude/prelude.h`

## 4. Tests and golden

- [x] 4.1 Extend the CPU lighting reference (`efx_lighting_shade` in `src/render/render_mesh.c`) with an unlit mode, and add a unit test asserting the unlit result equals `diffuse.color × map × albedo` with and without lights; verify `ctest` passes
- [x] 4.2 Author a new pure-geometry golden scene under `tests/goldens/unlit_inverted/` (`main.js`) covering `unlit`, `inverted`, and `depthWrite: false` with no third-party asset; verify the scene runs headless under `--script`/`--capture-frame` without error
- [x] 4.3 Capture the golden on the SSH build box with the llvmpipe recipe from `docs/verification-server.md` and commit `tests/goldens/unlit_inverted/golden.png`; verify the desktop golden comparison passes locally before dispatch

## 5. Docs, type document, and guidance

- [x] 5.1 Update `gallery/src/api/efx.d.ts`: `Material.unlit`, `DrawMeshOptions.depthWrite`, `MakeCubeOptions.inverted`, `MakeSphereOptions.inverted`, with doc comments and defaults; verify `npm --prefix gallery run check` (svelte-check) and the type test `gallery/src/api/efx.type-test.ts` pass
- [x] 5.2 Update `docs/js-api.md` with the material `unlit` rule and the `depthWrite` render-state rule; verify the guidance names both fields and their defaults
- [x] 5.3 Regenerate the committed reference with `npm --prefix gallery run docs:markdown` and confirm `docs:check` reports no drift
- [x] 5.4 Write `docs/decisions/0058-unlit-material-and-geometry-skybox.md` from `docs/decisions/TEMPLATE.md` (unlit/depth-write model extension; geometry skybox with cubemaps deferred) and add its row to `docs/decisions/README.md`; verify the ADR is indexed

## 6. Gallery showcases

- [x] 6.1 Produce the sky asset: download Poly Haven `kloofendal_43d_clear_puresky` (CC0) tonemapped JPG, downscale to 2048×1024 and re-encode, commit as `gallery/samples/curated/skybox-showcase/sky.jpg`, and add its provenance + recipe row to `gallery/samples/curated/CREDITS.md`; verify the file loads through `efx.io.loadImage` in the sample
- [x] 6.2 Author `gallery/samples/curated/skybox-showcase/main.js`: an inverted sphere with an `unlit` diffuse-mapped material, drawn camera-locked with `depthWrite: false` before a small lit scene; verify it runs under the player against its directory
- [x] 6.3 Author `gallery/samples/curated/blob-shadow-showcase/main.js`: a moving character mesh over a ground plane with a `createImageData` radial shadow drawn as a `facing: 'plane'` billboard (dark color, alpha blend, small lift) under an orbiting camera; verify the shadow stays flat and follows the character
- [x] 6.4 Add both entries to `gallery/samples/curated/manifest.json` (or the generated catalog) and regenerate with `gallery/scripts/gen-catalog.mjs`; verify both samples appear in the built catalog
- [x] 6.5 Build the gallery (`npm --prefix gallery ci && npm --prefix gallery run build`) and run the smoke against both new samples; verify no console or page errors

## 7. Verification

- [x] 7.1 Run `npx openspec validate "skybox-and-blob-shadows" --type change --strict` and confirm it passes
- [x] 7.2 Build and run the non-golden suite locally (`cmake -B build -DEFX_HEADLESS=ON` + `ctest -E golden`) and confirm all unit tests pass
- [x] 7.3 Commit, push the branch, run `python3 tools/verify_remote.py all <branch>`, fix until green, then dispatch the four-target gate with `gh workflow run ci.yml --ref <branch>` (ADR 0020/0023)
