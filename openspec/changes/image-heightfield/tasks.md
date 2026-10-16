# Tasks

## 1. Native heightfield builder

- [x] 1.1 Add `src/render/render_heightfield.h/.c`: pure-C builder over `const efx_image*` + a plain options struct (region, step, height, size) returning `efx_meshdata*` via `efx_surface_src` → `efx_meshdata_create` + `efx_meshdata_set_material`; implement the pinned geometry contract (Rec. 709 luma heights, grid cols/rows from region+step, `makePlane`-pattern CCW indices, full-image uv, cross-product normals, design D3) with `-ffp-contract=off` on the TU; verify the headless build compiles it (`cmake -B build -DEFX_HEADLESS=ON && cmake --build build -j3`)
- [x] 1.2 Write headless unit tests (`tests/unit/`) pinning exact values: 4×4 ramp default grid (16 vertices/18 triangles, uv 0..1, centered extents), region `[8,4,10,6]` + `step 2` sampling (5×3 grid, sub-rect uv), `height 5`/`size 20` scaling on a 64×32 image, flat-image normals exactly `(0,1,0)`, byte-identical streams across two calls, clamp-then-degenerate region errors, and the full error taxonomy (`TypeError`/`RangeError`); verify with `ctest` in the headless build dir

## 2. Script API — desktop binding

- [x] 2.1 Add the marshal-only wire native `efx_js_makeHeightfield_wire` in `src/api/api_3d.c` (image object → `efx_image` extraction, scalar region/step/height/size, material object binding; defensive marshalling with the canonical error codes) and register it on the natives surface; verify a `--script` headless run can build and draw a heightfield from a loaded PNG

## 3. Prelude — validation wrapper

- [ ] 3.1 Add `__efxMakeHeightfield` to `src/prelude/prelude.js`: required-positional `EfxImageData` check, option-bag validation per the family rules (region tuple, positive-integer step, finite height, positive size, unknown fields, material via the shared material-binding path), then the `natives.makeHeightfieldWire` call; install as `efx.graphics.makeHeightfield`; regenerate `src/prelude/prelude.h` via `tools/gen_prelude.py` and verify `gen_prelude.py --check` passes
- [x] 3.2 Verify canonical error text (non-image/destroyed image, `region: [0.5,0,4,4]`, `step: 0`, `size: 0`, `height: Infinity`, unknown key, non-material) in a headless `--script` test on the desktop runtime

## 4. Web bridge

- [x] 4.1 Export the builder wire from the Emscripten module and add the marshalling entry to `src/web/js/render3d.js` (natives surface), keeping the `CMakeLists.txt` post-js concat order; verify the same option errors and a drawn heightfield in the Emscripten build (`tools/run_web_compare.mjs` on the new golden scene)

## 5. Types + generated reference

- [x] 5.1 Extend `gallery/src/api/efx.d.ts` with `makeHeightfield` + `MakeHeightfieldOptions` (luminance/region/step/height/size docs, defaults, error behavior) beside the other prim options, add `efx.type-test.ts` coverage, and verify `npm --prefix gallery run check` passes
- [x] 5.2 Regenerate `docs/api/` (`npm --prefix gallery run docs:markdown`) and verify `npm --prefix gallery run docs:check` reports no drift

## 6. Golden scene

- [x] 6.1 Add a committed tiny grayscale heightmap PNG and a golden scene (load → `makeHeightfield` → lit directional draw, full-image default plus one `region`+`step` variant) to the ADR 0020 harness; verify the Linux golden test passes on an ON build dir (`ctest -R golden`)
- [ ] 6.2 Capture/regenerate goldens on the verification server per `docs/verification-server.md` and confirm the Emscripten golden job agrees pixel-wise

## 7. Docs + ADR

- [x] 7.1 Write `docs/decisions/0062-image-heightfield-primitive.md` (native builder behind prelude validation, luminance/normal contract, `-ffp-contract=off` determinism policy, rejected pure-JS builder) per `TEMPLATE.md` and index it in `docs/decisions/README.md`; update `docs/js-api.md` only if a guideline-level rule changed
- [x] 7.2 Verify `npx openspec validate "image-heightfield" --type change --strict` passes and the change's spec scenarios all map to implemented behavior

## 8. Full verification gate

- [ ] 8.1 Commit and push the branch, run `python3 tools/verify_remote.py all <branch>` and iterate until green (Linux golden-bearing jobs first, per AGENTS.md)
- [ ] 8.2 Dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) and iterate Linux → Windows → macOS until green
