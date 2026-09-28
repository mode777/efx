# Tasks

## 1. Sample

- [x] 1.1 Add `gallery/samples/curated/input-playground.js`: a self-playing 2D particle playground that uses `efx.mouse.onMove/onDown/onWheel`, `efx.mouse.position/delta/wheel`, `efx.keyboard.isPressed/isDown`, and `efx.window.size` (surface→frame mapping), with a fixed 400-particle cap and an idle attract mode; verify it runs cleanly under a windowed build on the verification server (`player gallery/samples/curated/input-playground.js`, screenshot non-empty, exit 0 after a few frames).
- [x] 1.2 Add the manifest entry (id `curated:input-playground`, title, category `Showcase`, description naming the F9 API it demonstrates) to `gallery/samples/curated/manifest.json`; verify `npm --prefix gallery run gen:catalog` lists it and the sorted catalog keeps it within the first eight entries (so the smoke exercises it).

## 2. Docs

- [x] 2.1 Update the `gallery/` bullet in `AGENTS.md` current state to mention the interactive input demo; verify the text matches the shipped sample.

## 3. Verification

- [x] 3.1 Run `npm --prefix gallery run check` and build the gallery; verify the build succeeds and the sample is in `gallery/src/samples/generated.json`.
- [x] 3.2 Run `python3 tools/verify_remote.py gallery <branch>` on the verification server (rebuilds the web player from F9-containing source, builds the site, runs the headless gallery smoke); verify `gallery smoke PASSED` with zero console/page errors.
- [x] 3.3 Run the four-target gate (`gh workflow run ci.yml --ref <branch>`) after 3.2 is green and confirm all targets pass.
