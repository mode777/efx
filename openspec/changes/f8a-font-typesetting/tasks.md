# Tasks

## 1. Vendoring and build

- [x] 1.1 Add pinned `stb_truetype.h` and `stb_rect_pack.h` snapshots to `vendor/stb/` and record both rows (project, version/commit, source, license) in `vendor/README.md`; verify the files are present and the README rows match the snapshots
- [x] 1.2 Add `src/render/stb_truetype_impl.c` and `src/render/stb_rect_pack_impl.c` (single implementation TUs with warnings relaxed, mirroring `src/resource/stb_image_impl.c`) and wire them into the `efx_core` target in `CMakeLists.txt`; verify a clean native build compiles the TUs
- [x] 1.3 Add `src/render/text.[ch]` to `efx_core` and verify the library builds on the native and Emscripten configurations

## 2. Core C font module

- [x] 2.1 Define the `FontData` struct (owned font bytes + parsed font info) and implement `efx_text_fontdata_load(path)` via the resource provider; verify with a unit test that a valid TTF loads and a missing/garbage file returns an error code
- [x] 2.2 Implement glyph rasterization + fixed-atlas packing in `text.c`: bake the requested codepoint set at the requested size, with `padding`, using `stb_truetype` bitmaps and `stb_rect_pack`, producing an RGBA8 atlas (white RGB + coverage alpha); verify a unit test that the atlas is non-empty, within bounds, and identical across repeated bakes of the same input
- [x] 2.3 Implement baked outline and shadow variants (dilated ring and blurred silhouette) as separate atlas regions with per-variant atlas rects and bearing offsets; verify a unit test that enabling effects adds the expected variant regions and leaves advances unchanged
- [x] 2.4 Implement the `Font` struct (atlas texture handle via `efx_render_texture_create`, per-glyph metrics/kerning/atlas rects, `size`/`lineHeight`/`ascent`/`descent`) and `efx_text_font_create(fontdata, opts)` including option validation (TypeError/RangeError/Error) and atlas-overflow failure; verify unit tests for defaults (Latin-1), custom `glyphs`, invalid options, and overflow
- [x] 2.5 Implement `efx_text_font_destroy` and `efx_text_fontdata_destroy` with idempotent semantics and deferred atlas release; verify a unit test that a destroyed font reports dead and repeated destroy is a no-op

## 3. Layout, drawing, and measurement

- [x] 3.1 Implement layout: `\n` hard breaks, greedy word wrap at `width`, character-level breaking of over-long words, kerning, and line advance from `lineHeight × scale`; verify unit tests against a CPU reference for wrapping and line counts
- [x] 3.2 Implement horizontal alignment (`left`/`center`/`right`/`justify`, last line left, `justify` requires `width`) and vertical alignment (`top`/`middle`/`bottom`) with the documented anchor semantics; verify unit tests for each mode's line positions and the `justify`-without-width `TypeError`
- [x] 3.3 Implement `efx_text_measure` returning `{ width, height, lines }`; verify a unit test that measurement matches the layout used by drawing for several inputs
- [x] 3.4 Implement `efx_text_draw` recording fill (and baked shadow/outline layers, in that order) as `efx_render_quad` records with `color`/`outlineColor`/`shadowColor`, `rotation`, and `scale`, returning the same bounds; verify a unit test over the recorded display-list records (count/order/source rects) and a headless draw smoke

## 4. Script bindings

- [x] 4.1 Register `FontData` and `Font` native classes plus `loadFontData`, `createFont`, `drawText`, and `measureText` in `src/api/api.c` with the documented query properties, errors, and destroy semantics; verify the desktop `--script` smoke test exercises the full pipeline and exits 0
- [x] 4.2 Add the matching C entry points to `src/web/bridge.c` and the `EfxFontData`/`EfxFont` wrappers + `loadFontData`/`createFont`/`drawText`/`measureText` in `src/web/entry.js` with identical names, semantics, and errors; verify the Emscripten smoke/compare suite produces the same output as desktop
- [x] 4.3 Add a portable script case (e.g. `tests/scripts/s_f8_text.js`) covering loading, baking, measurement, drawing, alignment, effects, and the error cases; verify it passes in ctest on all four targets and in the cross-runtime compare

## 5. Tests and goldens

- [x] 5.1 Commit a CC0-licensed TTF under the text golden scene/fixture directories and verify it is referenced by the scenes and recorded in `gallery/samples/curated/CREDITS.md`
- [x] 5.2 Author golden scenes for basic text, alignment/vertical alignment, wrapping + justify, and baked outline/shadow, then capture their goldens server-side under llvmpipe per `docs/verification-server.md`; verify `ctest` golden tests pass on the host
- [x] 5.3 Add headless layout/measure unit tests to the `tests/unit` suite (all four targets); verify they pass under `-DEFX_HEADLESS=ON` and on Emscripten ctest

## 6. Documentation and decision record

- [x] 6.1 Rewrite the `docs/js-api.md` F8 section from provisional to current (`loadFontData`, `createFont`, `drawText`, `measureText`, option fields, formatting modes, errors), add the `FontData`/`Font` rows to the resource table and the query-property note, and remove the provisional `loadFont`; verify every entry has signature, layer, and milestone tags
- [x] 6.2 Update `gallery/src/api/efx.d.ts` and `gallery/src/api/efx.type-test.ts` to declare `FontData`, `Font`, and the text functions consistently with the reference; verify the gallery type-check/build passes
- [x] 6.3 Write `docs/decisions/0038-native-font-typesetting.md` (per `TEMPLATE.md`) recording native C typesetting, the fixed-atlas decision, the vendored rasterizer, and the supersession of ADR 0013's font clause, and add its row to `docs/decisions/README.md`; verify the ADR and index row exist
- [x] 6.4 Update `vision.md` (drop `drawText` from the pure-JS example list) and `AGENTS.md` (current-state F8 entry, roadmap F8 row, script-API catalog, and the gallery sample bullet); verify the docs describe the shipped behavior

## 7. Gallery showcase

- [x] 7.1 Author `gallery/samples/curated/text-showcase.js` demonstrating wrapping, alignment, and baked outline/shadow (e.g. a typing demo driven by `efx.keyboard`/`efx.mouse`), plus its font asset pack `text-showcase.zip` and the `manifest.json` entry; verify the sample runs against its pack in the desktop smoke test
- [x] 7.2 Add the pack provenance/recipe to `gallery/samples/curated/CREDITS.md` and regenerate the gallery catalog; verify the gallery `check`/build succeeds and the sample appears in the catalog

## 8. Verification and gate

- [x] 8.1 Run the Linux pipeline first (headless unit tests + native golden suite) and fix any failures
- [x] 8.2 Run `python3 tools/verify_remote.py all <branch>` and fix any failures before dispatching CI
- [ ] 8.3 Dispatch `gh workflow run ci.yml --ref <branch>` and confirm the four-target gate is green (native suites incl. text goldens on Linux/Windows/macOS, Emscripten ctest + web goldens + browser harness + cross-runtime compare)
