# Tasks

## 1. Render module (pure C)

- [x] 1.1 Add `efx_fog` struct, `EFX_FOG_*` mode constants, `R.fog` state, `efx_render_set_fog`/`efx_render_fog`, reset in `efx_render_reset_state`, and the `fog` snapshot member on `efx_mesh_record` (render.h + render_records.c); verify with a new unit test: set/query round-trip, `null`/disabled default, reset clears, record snapshot captured at record time (tests/unit)
- [x] 1.2 Add the fog factor helper per design D5 (linear/exp/exp2 formulas) beside the lighting CPU reference and apply it to lit results only; verify with unit tests asserting the three factor formulas at boundary distances (near/far endpoints, d=0) and that unlit results are never fogged
- [x] 1.3 Verify the record-budget unit assertions still pass after the `efx_record` size growth (`ctest` unit suite green)

## 2. Canned shader (platform sink)

- [x] 2.1 Extend `shaders/mesh.glsl` `fs_params` with `fog_color`/`fog_params` per design D2 and add the fog factor + `mix` in the lit path only (unlit early-out untouched); regenerate `shaders/mesh.h` with the pinned sokol-shdc; verify: disabled-fog output is byte-identical (existing goldens unchanged)
- [x] 2.2 Fill the fog uniforms from the record snapshot in `mesh_record_setup` (pipeline.c); verify via the new golden scenes below

## 3. Bindings

- [x] 3.1 Add the desktop wire native `efx_js_set_fog_wire` in api_lighting.c and register it (api.c/api.h); verify it appears in the natives table and marshals all nine positionals
- [x] 3.2 Add prelude `g.setFog` with `__efxCheckKnown` validation per design D6 and regenerate `src/prelude/prelude.h` (`tools/gen_prelude.py`); verify: prelude drift check passes and prelude unit tests cover the TypeError/RangeError cases from the spec
- [x] 3.3 Add `efx_bridge_set_fog` (bridge_render3d.c) and the web natives fragment entry mirroring `setPointLight`; verify the web unit harness (headless Emscripten smoke) still passes

## 4. Script-facing surface

- [x] 4.1 Add `FogOptions` + `setFog` to `gallery/src/api/efx.d.ts` with the fog/clear-color guidance in the doc comment, and a gallery type-test entry; verify `npm --prefix gallery run check`
- [x] 4.2 Regenerate `docs/api/` (`npm --prefix gallery run docs:markdown`); verify `docs:check` passes

## 5. Golden scenes

- [ ] 5.1 Add golden scene `fog_linear` (ground plane receding from camera, near=2 far=20) and `fog_exp2` (same plane, density=0.09) with captures per docs/verification-server.md; wire into the golden harness; verify `tools/run_web_compare.mjs` diffs desktop vs web clean and the goldens pass locally on an ON build dir
- [ ] 5.2 Confirm all pre-existing golden scenes are unchanged (fog-off byte-identity); verify the golden ctest suite green except the new scenes pending server capture

## 6. Verification gate

- [ ] 6.1 Full unit + golden verification on the SSH server (`python3 tools/verify_remote.py all <branch>`) until green before dispatching the gate
- [ ] 6.2 Dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`); iterate Linux → Windows → macOS until green; merge on green per AGENTS.md
