# Tasks

## 1. Sample scaffold

- [x] 1.1 Create `gallery/samples/curated/startup-flight/`, copy `assets/efx_logo.png` into it, and add the catalog entry (title "Startup Flight", category "Showcase") to `gallery/samples/curated/manifest.json`; verify with `npm --prefix gallery run build` (gen-catalog picks the sample up) and that only `main.js` + `efx_logo.png` exist in the dir.
- [x] 1.2 Author the `main.js` skeleton: frame-derived layout constants (`cx`, `cy`, `horizonY`, focal `f` from the live frame size per design D4), clear color, logo texture via `loadImage`+`createTexture`, empty update/render hook registration; verify it boots under the desktop player: `build/efx gallery/samples/curated/startup-flight` (windowed build dir) shows a solid frame with no console errors.

## 2. Procedural cloud textures (design D2)

- [x] 2.1 Implement the puff sculptor: `noise.fill2` fBm (fixed seeds) + per-pixel radial falloff, vertical density bias, soft knee, and baked top-light RGB shading; emit 4–6 puff variant textures (256², `wrap: 'clamp'`, `mipmaps: true`) with all shape values as named module constants; verify in the running scene that puffs read as puffy cumulus silhouettes (not fog) and no `Math.random` is used.
- [x] 2.2 Add 1–2 wisp strip textures (raw `noise.imageData(..., { alpha: true })` with a domain mapped so most of the quad is transparent, or the same sculptor with elongated falloff); verify they read as faint high haze when tinted at low alpha.

## 3. Scene assembly

- [x] 3.1 Sky: build the vertical gradient texture (zenith blue → pale horizon haze, e.g. `createImageData` rows) and record the fullscreen `drawQuad` first in the render hook; verify the gradient fills the frame and later draws composite over it.
- [x] 3.2 Cloud field: recycler state (`x`, `y`, `z` per cloud, `z += speed·dt`, respawn at `zFar` below `zNear` with fresh jitter/variant), per-frame z-descending in-place sort, JS projection (`sx = cx + f·x/z`, `sy = horizonY − f·y/z`, `w = f·worldW/z`), haze-color lerp + alpha smoothstep at both z ends, drawn far→near; verify clouds stream toward and past the camera, converge toward the horizon line in the distance, fade in/out without pops, and no per-frame allocation beyond option bags.
- [x] 3.3 Logo: centered 2D `drawQuad` recorded after all clouds, swaying with `y = cy + A·sin(t·ω)` (period ~3.5 s, amplitude ~1.5% frame height, design D6); verify it stays centered, on top, and sways smoothly.
- [x] 3.4 Motion tuning pass: slow bank roll via pre-projection `(x, y)` rotation (design D1), forward speed, recycle spread, and palette constants tuned by eye; verify the scene reads as flight along a cloud-deck edge under a clear sky, deterministic across two consecutive launches (fixed seeds, no time-of-day or random variation).

## 4. Registration & verification

- [x] 4.1 Add `smoke_showcase_startup_flight` to `tests/CMakeLists.txt` mirroring the other `smoke_showcase_*` entries (`--script;…/main.js;--root;…`); verified via `ctest -R smoke_showcase_startup_flight` on the verification server's full build (headless builds define no smoke targets by design — they live in the `NOT EFX_HEADLESS` block of tests/CMakeLists.txt).
- [x] 4.2 Run the full local gate: headless `ctest` suite green and `npm --prefix gallery run check` + `npm --prefix gallery run build` green (catalog embeds the new sample); verify the sample appears in the generated gallery catalog.
- [x] 4.3 Commit to a branch, push, and run `python3 tools/verify_remote.py all <branch>` until green (both golden-bearing jobs per AGENTS.md lifecycle step 3) before dispatching the CI gate.

## 5. Realism pass (review feedback)

- [x] 5.1 Rewrite the cloud field as an additive density-field sea: per-pixel envelope × |fBm| billow textures (6 variants, 256², clamp+mipmaps), `blend: 'additive'`, no sort; verify the deck reads as one continuous sea with no sprite boundaries in llvmpipe captures.
- [x] 5.2 Three depth tiers + horizon haze band with `setTop`-pinned silhouette; verify the sea's upper edge hugs the flight path at frames 2/150/300/480 (server captures), with no clouds above the rim or below the deck.
- [x] 5.3 Sega-blue palette: saturated zenith, pale horizon, cloud-shadow under-gradient (gaps read as holes); verify in captures and the local exact-noise preview harness.
- [x] 5.4 Try and evaluate a crevice-shadow source-over pass; reject and revert after server captures showed gray fog in floor gaps (recorded in design.md D2).
- [x] 5.5 Refresh the catalog description and re-run the full pre-CI verification (`tools/verify_remote.py all`).
