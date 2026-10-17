# Tasks

## 1. Vendor FastNoise Lite

- [x] 1.1 Vendor the official FastNoise Lite C99 header into `vendor/fastnoise-lite/` (pin commit + LICENSE), add the pin row to `vendor/README.md`, and verify the build stays offline (`cmake -B build -DEFX_HEADLESS=ON && cmake --build build -j4` succeeds with no network)
- [x] 1.2 Add the warnings-relaxed implementation TU `src/noise/fastnoise_impl.c` to `efx_core` in `CMakeLists.txt` (same treatment as the miniz/cgltf TUs) and verify all four targets compile locally where possible (Linux native + Emscripten build dirs)

## 2. Pure-C noise core

- [x] 2.1 Write `src/noise/efx_noise.h/.c`: plain-C seed-in-args surface over FNL (`noise2`/`noise3` point query, `fill2` row-major bulk write, one shared evaluator so fill ≡ point queries), floating `float` evaluation, no fast-math; verify with a headless unit test asserting fill-vs-point-query equality, write count, and range sampling for each type
- [x] 2.2 Add a determinism unit test (fixed seeds/coords → pinned expected values) and verify it passes on the Linux build (`ctest` in the headless build dir)

## 3. Script API — desktop binding

- [x] 3.1 Add `src/api/api_noise.c` wire natives (`noise2`, `noise3`, `fill2` — marshal-only, buffer/size checks returning canonical error codes) and register them; verify `efx.noise.noise2(0.5, 0.5)` returns a value in a `--script` headless run
- [x] 3.2 Create the `efx.noise` namespace in the desktop binding (audio/physics registration pattern) and verify it is a member of `efx` with no new free globals

## 4. Prelude — validation + helper

- [x] 4.1 Add prelude-side `efx.noise` augmentation: cold-path option-bag validation (seed/type/frequency/octaves/lacunarity/gain/warp; fill2 buffer type + length; imageData domain) behind the private `natives` object, plus the pure-JS `imageData` helper (fill2 → byte remap → `createImageData`); regenerate `src/prelude/prelude.h` via `tools/gen_prelude.py` and verify `gen_prelude.py --check` passes
- [x] 4.2 Verify canonical error text for invalid options (unknown type, non-integer seed/negative octaves, undersized/non-Float32Array buffer) in a headless `--script` test, matching the ADR 0049 message table

## 5. Web bridge

- [x] 5.1 Add `src/web/js/noise.js` (namespace creation + wire calls to the compiled C), insert it in the `CMakeLists.txt` post-js concat order, and verify the Emscripten build exposes the same `efx.noise` surface

## 6. Types + generated reference

- [x] 6.1 Extend `gallery/src/api/efx.d.ts` with the `efx.noise` surface (per-type range docs, defaults, `domain`) including `efx.type-test.ts` coverage, and verify `npm --prefix gallery run check` passes
- [x] 6.2 Regenerate `docs/api/` (`npm --prefix gallery run docs:markdown`) and verify `npm --prefix gallery run docs:check` reports no drift

## 7. Golden scene

- [x] 7.1 Add the noise golden scene (three blitted textures: simplex fBm seed 7 `[-1,1]`, cellular seed 9 `[0,1]`, warped value fBm seed 13) to the golden harness with committed expected images, and verify the Linux golden test passes on an ON build dir (`ctest -R golden`)
- [x] 7.2 Capture/regenerate goldens on the verification server per `docs/verification-server.md` and confirm the Emscripten golden job agrees pixel-wise

## 8. Docs + ADR

- [x] 8.1 Write `docs/decisions/0059-procedural-noise-fastnoise-lite.md` (stateless surface, FNL choice, raw ranges, determinism policy) per `TEMPLATE.md` and index it in `docs/decisions/README.md`; update `docs/js-api.md` only if a guideline-level rule changed
- [x] 8.2 Verify `npx openspec validate "noise-api" --type change --strict` passes and the change's spec scenarios all map to implemented behavior

## 9. Full verification gate

- [x] 9.1 Commit and push the branch, run `python3 tools/verify_remote.py all <branch>` and iterate until green (Linux golden-bearing jobs first, per AGENTS.md)
- [x] 9.2 Dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) and iterate Linux → Windows → macOS until green
