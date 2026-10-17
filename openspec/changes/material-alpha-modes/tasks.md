# Tasks

## 1. Material model and wire format

- [x] 1.1 Add `EFX_ALPHA_*` constants and `alpha_mode` to `efx_material` in `src/render/render.h`; update the DEFAULT_MATERIAL default (opaque) and the wire-lockstep mirror list comment; verify the header builds (`cmake --build build --target efx` compiles clean)
- [x] 1.2 Grow the material wire block 19 → 20 floats in all five mirrors — `src/prelude/prelude.js` `__efxMaterialWire`, `src/web/js/core.js` `__efxMaterial`, `src/api/api_3d.c` `wire_mat_from_block`, `src/web/bridge_render3d.c` `bridge_mat_from_wire`, and the web `createMeshData` stride — and regenerate `src/prelude/prelude.h` via `tools/gen_prelude.py`; verify `python3 tools/gen_prelude.py --check` passes and the headless unit suite builds
- [x] 1.3 Extend `efx_api_read_material` in `src/api/api.c` to parse `alphaMode` (`'opaque'`/`'mask'`/`'blend'`/`null`, default opaque, `TypeError` otherwise) with the same error-shape conventions as `blend`; verify with a quickjs headless script test that valid modes bind and `'multiply'`/`1` throw `TypeError` with nothing recorded

## 2. Shader and CPU reference alpha rule

- [x] 2.1 Update `shaders/mesh.glsl`: pack `alpha_mode` into the free `mat_params` component, make the alpha-mask discard conditional on mode (`'blend'` never discards), and compute output alpha = `1.0` outside `'blend'`, `clamp(diffuse.a * efx_color.a * tint.a)` inside — in both lit and unlit paths; regenerate the sokol-shdc headers and verify all four targets compile
- [x] 2.2 Mirror the rule in the CPU reference `efx_lighting_shade` (mode-gated alpha, mode-conditional discard) and extend its unit tests: opaque saturates to 1, blend yields the clamped product, discard boundary unchanged in mask path; verify `ctest` unit tests pass headless (`cmake -B build -DEFX_HEADLESS=ON`)

## 3. Transparent pass at playback

- [x] 3.1 Add the per-segment transparent side list to `play_records` in `src/platform/pipeline.c`: main sweep skips `'blend'` surfaces in mesh records and pushes `(record, surface_index)` entries; list resets at `BEGIN_TARGET`/`END_TARGET`; verify a scene with one blend mesh still renders (deferred draw present)
- [ ] 3.2 Implement the stable origin sort — view-space z of `record.transform` translation through the recorded camera, composite key `(depth, sequence)` — and the one-surface replay pinned to the `[blend][0]` (or `_cw`) pipeline row with `depth_write` forced off; verify with a debug scene that a far blend mesh recorded before a near one draws far-first
- [ ] 3.3 Confirm quads/sprites/text/billboards/particles are untouched by the split and that GL-family RT passes select the `_cw` row correctly inside the transparent replay; verify existing goldens (`blend`, `map_alpha_mask`, `vertcolor3d`, `2d` scenes) capture unchanged under `tests/run_golden` on a golden-enabled build

## 4. glTF import

- [ ] 4.1 Map glTF `alphaMode` OPAQUE/absent→opaque, MASK→mask (as today), BLEND→blend in `src/resource/gltf.c`, carrying `baseColorFactor.a` into diffuse alpha for BLEND (clamped) and noting the 0.5 cutoff approximation; verify an imported BLEND-material glTF asset composites in the transparent pass with a small fixture asset

## 5. API surface and docs

- [ ] 5.1 Add `alphaMode?: 'opaque' | 'mask' | 'blend' | null` to `Material` in `gallery/src/api/efx.d.ts` with doc comments matching the spec semantics; verify `npm --prefix gallery run check` passes (type-test included)
- [ ] 5.2 Regenerate `docs/api/` via `npm --prefix gallery run docs:markdown` and confirm `docs:check` passes; verify `docs/js-api.md` needs no guideline change (no new design rule)

## 6. Goldens and verification

- [ ] 6.1 Add `tests/goldens/material_alpha_modes/` — opaque + mask + blend surfaces with a blend box straddling an opaque one and a far/near blend pair recorded out of order — and `tests/goldens/vertex_alpha_blend/` (vertex-color alpha gradient over a checker); capture server-side per `docs/verification-server.md` and verify the desktop-vs-web compare matches
- [ ] 6.2 Sweep the existing golden corpus for alpha < 1 on default-mode materials (tint fades, vertex alpha) and re-baseline only scenes in the breaking class; verify the full golden suite is green after any re-baseline
- [ ] 6.3 Write `docs/decisions/0064-material-alpha-modes-and-transparent-pass.md` (routing policy, origin sort, saturation rule, mode-over-mask, blend-equation orthogonality) and index it in `docs/decisions/README.md`; verify the index renders and statuses are consistent
- [ ] 6.4 Commit, push the branch, and run `python3 tools/verify_remote.py all <branch>` until green; then dispatch `gh workflow run ci.yml --ref <branch>` and iterate Linux → Windows → macOS to green (four-target gate, ADR 0020/0023)
