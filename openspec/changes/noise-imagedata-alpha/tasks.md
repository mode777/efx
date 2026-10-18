# Tasks

## 1. Prelude implementation

- [x] 1.1 In `src/prelude/prelude.js` `noise.imageData`: add `'alpha'` to the extra allowed-key list passed to `__efxNoiseParse`, validate `alpha` as a strict boolean after the existing `domain` block (`TypeError` otherwise), resolve the flag before the pixel loop, and in alpha mode write `[255, 255, 255, b]` instead of `[b, b, b, 255]` (byte `b` computed exactly as today). Verify with a headless `--script` run exercising `alpha: true`, `alpha: false`, and a `TypeError` case
- [x] 1.2 Regenerate the committed header with `python3 tools/gen_prelude.py` and confirm `python3 tools/gen_prelude.py --check` passes (the Linux gate fails on drift); rebuild `cmake -B build -DEFX_HEADLESS=ON` and run the unit ctest suite green

## 2. Unit tests

- [x] 2.1 Extend `noise_imagedata_js` in `tests/unit/api_tests.c`: acceptance for `{alpha: true}` and `{alpha: false}` (extent + `destroy`), `TypeError` rejections for `alpha: 'yes'` and `alpha: 1`, and an unknown-key rejection alongside a valid `alpha`; run the unit tests via `ctest` in the headless build (all four targets gate this suite)

## 3. Type document and generated reference

- [x] 3.1 Add `alpha?: boolean` to `NoiseImageDataOptions` in `gallery/src/api/efx.d.ts` (document the white-RGB/noise-alpha layout and the default), refresh the `imageData` method doc, and extend `gallery/src/api/efx.type-test.ts` with a valid `{ alpha: true }` call and an invalid `{ alpha: 'yes' }` rejection; verify with `npm --prefix gallery run check`
- [x] 3.2 Regenerate `docs/api/` with `npm --prefix gallery run docs:markdown` and confirm `npm --prefix gallery run docs:check` passes (never hand-edit `docs/api/`)

## 4. Golden receipt (byte-level)

- [x] 4.1 Extend `tests/goldens/noise/main.js` with a fourth texture built via `alpha: true` (same fBm options as one existing quad) and re-layout to four quads over the existing clear color so white-RGB × noise-alpha is visible; verify the scene loads in a local `--script`/windowed run
- [ ] 4.2 Commit and push the branch; on the SSH verification server capture the updated noise scene per `docs/verification-server.md` (native llvmpipe + pinned headless Chrome), commit the re-baselined goldens, and run `python3 tools/verify_remote.py all <branch>` until green (golden-image harness, ADR 0020)

## 5. Gate, merge, archive

- [ ] 5.1 Dispatch `gh workflow run ci.yml --ref <branch>` and iterate Linux → Windows → macOS until the four-target matrix is green
- [ ] 5.2 Merge to `main` with `git merge --no-ff` using the repo's `Merge <change>: <summary> (ADR 0059)` message shape and push (Pages gallery redeploy follows)
- [ ] 5.3 Run `npx openspec validate "noise-imagedata-alpha" --type change --strict`, then archive the change (`/opsx-archive`) and push; no new ADR per design D5 — ADR 0059 already indexes the noise surface
