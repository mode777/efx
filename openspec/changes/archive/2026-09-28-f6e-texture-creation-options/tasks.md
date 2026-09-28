# Tasks

## 1. Mipmap creation state through the C layers

- [x] 1.1 Add a `mipmaps` argument to `efx_render_texture_create` and the `create_texture` sink callback in `src/render/render.[ch]`; store it on `tex_slot` (including the deferred no-GPU-surface queue path) and expose it via an optional `out_mipmaps` on `efx_render_texture_sampler`. Verify: existing texture unit tests still build/pass and a new unit test reads back the mipmaps flag for both true and false.
- [x] 1.2 Implement CPU mip-chain generation in `src/platform/pipeline.c`: when `mipmaps` is set, 2×2 box-filter each level (each dimension `max(1, n/2)`), fill `sg_image_desc.num_mipmaps` and every `mip_levels[i]`, and key the sampler cache by `(wrap, filter, mipmaps)` so a mipmapped texture gets the mipmap variant of `filter` (linear→trilinear, nearest→nearest-mipmap) while magnification stays plain. Verify: a display-bearing build (local headless excludes goldens) renders a mipmapped vs non-mipmapped minified quad distinctly; no mip levels are uploaded when `mipmaps` is false.
- [x] 1.3 Extend the Emscripten bridge `efx_bridge_texture_create` in `src/web/bridge.c` to carry `mipmaps` into `efx_render_texture_create`. Verify: the web smoke/compare suite still passes and a web case creating a mipmapped texture matches the desktop result.

## 2. Script bindings

- [x] 2.1 In `src/api/api.c`, add `mipmaps` to the `createTexture` known-field allowlist and parse it as a strict boolean (non-boolean → `TypeError`, no texture created); pass it to the render call. Verify: ctest unit test covers `mipmaps: true`, `false`, omitted, and a non-boolean rejection.
- [x] 2.2 Mirror the parsing in `src/web/entry.js` (`known` map, boolean check, bridge call) with identical error semantics. Verify: desktop-vs-web comparison smoke test (`tools/run_web_compare.mjs`) exercises the option and reports no divergence.

## 3. Remove the `loadTexture` convenience

- [x] 3.1 Delete the `efx.loadTexture` assignment from `src/prelude/prelude.js` and regenerate `src/prelude/prelude.h` with `python3 tools/gen_prelude.py`; confirm `python3 tools/gen_prelude.py --check` is clean. Verify: a headless script calling `efx.loadTexture` throws `TypeError` and `createTexture(loadImage(path), opts)` works.
- [x] 3.2 Rewrite the in-tree callers to the composed flow: `tests/goldens/load_png/main.js`, `tests/scripts/s_6a_resource.js`, and `gallery/samples/curated/texture-showcase.js`. Verify: `load_png`'s rendered output is unchanged (golden comparison) and the resource smoke test exits 0.

## 4. Tests and goldens

- [x] 4.1 Add a mipmap golden scene under `tests/goldens/` that draws a minified, mipmapped texture next to a non-mipmapped one so aliasing differences are visible; capture its reference on the verification server (llvmpipe only, per `docs/verification-server.md`) and commit scene + reference. Verify: native ctest golden suite passes on the server and the Emscripten golden suite reproduces it.
- [x] 4.2 Add/extend unit coverage for the new option validation and mipmap state (render-layer unit test in `tests/unit/`, plus the existing desktop/web smoke cases). Verify: `ctest` passes with the display-free targets locally (`-DEFX_HEADLESS=ON`) and all targets on the gate.

## 5. Docs and decisions

- [x] 5.1 Write `docs/decisions/0034-texture-mipmaps.md` (per `docs/decisions/TEMPLATE.md`: CPU box-filter chain for cross-target determinism, boolean option + filter-variant mapping, samplers per `(wrap, filter, mipmaps)`) and add its row to `docs/decisions/README.md`; note it amends ADR 0032's "mipmaps out of scope" line. Verify: index lists 0034 and the ADR is internally consistent.
- [x] 5.2 Update `docs/js-api.md`: drop `loadTexture` from the F6a section and resource table, document the `createTexture(loadImage(path), opts?)` flow, and add `mipmaps` to the texture option catalog and samples. Verify: every `loadTexture` mention is gone except the removal note in the change record.
- [x] 5.3 Update `gallery/src/api/efx.d.ts` (remove `loadTexture`, add `mipmaps?: boolean` to `TextureOptions`) and `gallery/src/api/efx.type-test.ts` (mipmaps accepted; non-boolean rejected). Verify: the gallery type-check passes.
- [x] 5.4 Update the AGENTS.md current-state paragraph to record the removed convenience and the new texture option. Verify: no statement contradicts `docs/js-api.md`.

## 6. Verification and archive

- [x] 6.1 Run `python3 tools/verify_remote.py all <branch>` and fix anything it reports; the native ctest (incl. all goldens) and Emscripten golden/compare suites must be green. Verify: the pre-CI server report is clean.
- [x] 6.2 Dispatch `gh workflow run ci.yml --ref <branch>` and confirm the four-target gate (Linux → Windows → macOS plus Emscripten) is green. Verify: the run's native suites, all goldens, web goldens, and cross-runtime compare pass.
- [x] 6.3 Merge to `main` and push (triggers the Pages deploy), then archive the change (`openspec archive`) and push the archive. Verify: `main` contains the merged change and the archive folder, and the gate run that justified it is recorded in the change notes.
