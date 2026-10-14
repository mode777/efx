# Tasks

## 1. Shared radial asset

- [x] 1.1 Add `gallery/scripts/gen-radial-assets.py` (stdlib `struct`+`zlib`, no Pillow): emits a 64×64 white PNG with alpha `(1−d)²·255` over a normalized disc, writes it as `radial.png` into each consuming sample directory, and supports `--check` for drift (mirroring `gen-audio-assets.py`); verify it regenerates byte-identical output on a second run
- [x] 1.2 Run the generator to add `radial.png` to `game-bot-arena`, `game-bloom-breakout`, `game-glow-gauntlet`, `game-mini-golf`, `game-sky-steps`, `particles-showcase`, `particle-plane-showcase`, `blob-shadow-showcase`; verify each directory contains the file and `--check` passes
- [x] 1.3 Add one `CREDITS.md` row per shipping sample (file `radial.png`, author "Authored in-repo", source "`gallery/scripts/gen-radial-assets.py`", license CC0 1.0); verify every directory that ships the file has a row

## 2. Samples load the radial image

- [x] 2.1 Replace the inline radial falloff loops with `loadImage('radial.png')` + `createTexture` in `game-bloom-breakout`, `game-bot-arena`, `blob-shadow-showcase` (tint at draw time); verify each sample loads cleanly under the catalog runner (`efx_catalog_runner <dir>/main.js <dir>`) with no console error
- [x] 2.2 Replace the `radial()` / `radialTexture()` helpers with the loaded image in `game-glow-gauntlet`, `game-sky-steps`, `game-mini-golf`; verify each loads cleanly under the catalog runner
- [x] 2.3 Replace the `radial()` helper with the loaded image in `particles-showcase` and `particle-plane-showcase` (particle sprite textures); verify each loads cleanly under the catalog runner
- [x] 2.4 Confirm no curated sample still contains a radial-falloff pixel loop (`grep -rn "1 - d) \* (1 - d)" gallery/samples/curated`); verify zero hits

## 3. Mini Golf uses `efx.noise`

- [x] 3.1 Replace `game-mini-golf`'s LCG `noiseTexture` with `efx.noise.imageData(64, 64, { seed, frequency })` remapped to the existing grass brightness and wrapped `repeat`; verify the sample loads cleanly and the grass renders (gallery smoke) with no third-party asset added

## 4. Adopt the remaining engine conveniences

- [x] 4.1 Replace the local `trs()` helpers with `efx.math.mat4.transform(pos, 0, 0, 0, scale)` in `game-bot-arena`, `game-mini-golf`, `physics-showcase`, and the inline translate∘scale in `game-sky-steps`; verify each sample loads cleanly and the transformed draws are unchanged (same matrix values)
- [x] 4.2 Replace the hand-rolled camera basis + NDC ray with `efx.graphics.screenRay(...)` in `game-bot-arena` and `game-mini-golf` (keeping only the plane intersection; golf feeds the ray into its existing `physics.raycast`); verify aiming still resolves the same course-plane point in the gallery smoke run
- [x] 4.3 Add an off-by-default `efx.physics.debugDraw()` toggle to `physics-showcase` and remove its duplicated collider-size render calls; verify the sample loads cleanly and the toggle records debug draws in a render hook
- [x] 4.4 Replace `t += dt` / `clock += dt` animation accumulators with `efx.time.now` across the remaining samples, keeping per-frame `dt` (and any clamp) for simulation steps; verify each edited sample loads cleanly under the catalog runner
- [x] 4.5 Replace hand-written linear blends with `efx.math.lerp` where one exists (e.g. the audio-showcase volume fade); verify the sample loads cleanly and the blend is behavior-preserving

## 5. Spec, docs, and verification

- [ ] 5.1 Confirm `openspec/changes/curated-sample-modernization/specs/web-gallery/spec.md` matches the implementation (asset names, requirement wording); run `npx openspec validate "curated-sample-modernization" --type change --strict`
- [ ] 5.2 Verify every edited sample with `node --check` and the catalog-runner load smoke, then `npm --prefix gallery run build` and the gallery smoke run (`node tools/run_gallery_smoke.mjs` via the server suite) with no console/page errors from any sample
- [ ] 5.3 Verify on the SSH server (`python3 tools/verify_remote.py gallery curated-sample-modernization`), then dispatch the gate (`gh workflow run ci.yml --ref curated-sample-modernization`) and iterate Linux → Windows → macOS → emscripten to green
