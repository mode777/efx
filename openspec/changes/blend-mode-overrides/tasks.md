# Tasks

## 1. Core blend-state plumbing (C renderer)

- [x] 1.1 Add a `blend` field with an "inherit" sentinel (`-1`) to the C `efx_material` struct and its default, and resolve each mesh surface's blend (material override else `R.blend`) into a new `uint8_t surface_blend[16]` on `efx_mesh_record` at record time in `efx_render_mesh`; verify with a headless unit test asserting the resolved per-surface bytes
- [x] 1.2 Reset `R.blend` to `EFX_BLEND_ALPHA` in `efx_render_begin_frame`; verify a headless test that records a draw in a frame after an init-time `setBlendMode('additive')` and observes the alpha record
- [x] 1.3 Add an optional blend sentinel parameter to `efx_render_quad` and the billboard record path so a caller override wins and `-1` falls back to `R.blend`; verify the existing 2D/billboard unit tests still pass and a new override assertion holds
- [x] 1.4 Give `efx_particle_config.blend` an inherit sentinel and resolve `drawParticles` to `cfg.blend` when set, else `R.blend`, at record time; verify a headless test for both the configured and inherited cases
- [x] 1.5 Change mesh playback in `src/platform/pipeline.c` to apply the mesh pipeline per surface, switching only when the surface blend differs and re-applying uniforms as needed; verify the mesh goldens still render and a mixed-blend mesh produces both modes

## 2. 2D API overrides

- [x] 2.1 Parse and validate `blend` in `read_quad_opts` (`src/api/api_2d.c`) and pass it through `drawQuad`; verify `tests/scripts/s_2d_validation.js` and the error catalog cover a valid override and an invalid mode
- [x] 2.2 Add the trailing `opts?` bag with a batch-level `blend` to `drawSprites` (`src/api/api_particles.c`), rejecting unknown fields and invalid values atomically; verify a test that every sprite in the call shares the batch blend and an invalid bag records nothing

## 3. 3D API overrides

- [x] 3.1 Parse, validate (three mode strings plus `null`), and snapshot `Material.blend` in the material binding path (`src/api/api_3d.c` or equivalent); verify the lighting unit tests cover a bound blend and an invalid value
- [x] 3.2 Parse and validate `blend` in `drawBillboard`'s options bag; verify the billboard test covers the override and an invalid mode

## 4. Particles API

- [x] 4.1 Make `createParticleSystem`'s `blend` optional (omitted or `null` = inherit) and accept the same in `ParticleSystem.set`; verify tests for creation default, explicit override, and inheritance at draw time

## 5. Shared bindings (prelude + web)

- [x] 5.1 Add the new option-bag validators to `src/prelude/prelude.js`, regenerate `src/prelude/prelude.h` (`python3 tools/gen_prelude.py`), and verify `python3 tools/gen_prelude.py --check` passes
- [x] 5.2 Mirror the new fields in the web bridge and `src/web/js/*`; verify the desktop/web comparison smoke test and the cross-runtime error catalog stay byte-identical

## 6. Documentation

- [ ] 6.1 Update `gallery/src/api/efx.d.ts` (`DrawQuadOptions.blend`, the `drawSprites` opts bag, `DrawBillboardOptions.blend`, `Material.blend`, the particle `blend` union) and the type test, then regenerate `docs/api/` (`npm --prefix gallery run docs:markdown`) and verify `npm --prefix gallery run docs:check` passes
- [ ] 6.2 Update the `docs/js-api.md` design guidelines with the blend render-state rule (frame-local default, per-object override precedence, particle inheritance) and verify the guidelines state the rule

## 7. ADR

- [ ] 7.1 Write `docs/decisions/0054-blend-mode-state-and-overrides.md` from `TEMPLATE.md` and add its row to `docs/decisions/README.md`; verify the ADR is indexed and states the frame-local default and per-object override decision

## 8. Samples

- [ ] 8.1 Move blend setup into the render hooks (or per-object options) of `gallery/samples/curated/input-playground` and `particles-showcase`, and verify each sample still renders its intended additive/alpha mix

## 9. Tests and verification

- [ ] 9.1 Extend `tests/goldens/blend` to assert the frame reset and a per-object override; verify the golden compares on a display build
- [ ] 9.2 Add unit and portable-script coverage for per-surface material blend, particle inheritance, sprite batch blend, and the invalid-value error classes; verify `ctest` (excluding goldens on headless) passes
- [ ] 9.3 Run `npx openspec validate "blend-mode-overrides" --type change --strict` and verify the change validates with no errors
- [ ] 9.4 Commit, push the branch, run `python3 tools/verify_remote.py all <branch>` green, then dispatch `gh workflow run ci.yml --ref <branch>` and iterate Linux → Windows → macOS
