## 1. Spike and decision (R22, go/no-go)

- [x] 1.1 On a throwaway branch, move `__efxParticleWire` into
  `src/prelude/prelude.js` behind a temporary `natives` parameter, and make the
  desktop `createParticleSystem` accept the wire. Verify: the particles catalog
  lines and `api_tests` `particles_js` pass on desktop.
  (Branch `spike/r22-particles`; catalog byte-identical, 191/191 V1, particles_js green.)
- [x] 1.2 Measure on the Windows Release desktop build:
  - `createParticleSystem` with a full bag ×1000, prelude path vs C path;
  - `drawQuad` with a full option bag ×10 000 in one frame, prelude path vs
    native.

  Record the median of 5 runs each.
  (Deviation: no Windows machine in the authoring environment — measured on
  Linux, gcc -O2 Release, headless quickjs. Cold +74…+80 ms/1000 (≈75–80 µs per
  call); hot 9–14× native. Recorded in ADR 0049.)
- [x] 1.3 Write `docs/decisions/0049-shared-option-validation.md` (per
  `TEMPLATE.md`) with:
  - the measurements;
  - the cold/hot boundary and budget (design D7);
  - the check order (D3);
  - the canonical-message rule and table (D5);
  - the strict-number rule (D6).

  Add the row to `docs/decisions/README.md`.
  (Status left Proposed pending 1.4.)
- [ ] 1.4 Go/no-go: the owner accepts or rejects ADR 0049.
  - If **rejected**, set the ADR status to Rejected, mark groups 2–10
    won't-do with a pointer to the ADR, update `docs/refactoring.md` §5, and
    stop: no spec sync.
  - If **accepted**, discard the spike branch and continue.

## 2. Shared infrastructure

- [ ] 2.1 Change the prelude wrapper to `function (efx, natives)`. On desktop,
  `eval_prelude` calls the wrapper value with `(efx, natives)` built by
  `efx_api_init`. On the web, use `new Function('efx', 'natives', src)` in
  `src/web/js/audio.js` (D1). Verify:
  - V1 `module_js`, `module_hooks_js`, `smoke_10_*`;
  - V4 `web_10_*`;
  - a new portable case shows no `natives` or validator name on `efx` or
    `globalThis`.
- [ ] 2.2 Move the shared helpers into the prelude: the known-field check,
  strict number/finite readers, array readers, `sourceRect`, and the
  return-code → error table with overrides (D4). Regenerate `prelude.h`.
  Verify: V3 (`gen_prelude.py --check`); catalog byte-identical on both
  runtimes (no domain migrated yet).

## 3. Particles (R23)

- [x] 3.1 Install the prelude validator for `createParticleSystem` and
  `ParticleSystem.set`. Delete `read_particle_config` and the `pcfg_*`
  readers (desktop) and the web reader path, so natives unpack the wire.
- [x] 3.2 Resolve `11.ps-max-range`, `11.ps-bad-facing` and `11.ps-set-max`
  per D5. Update `s_error_catalog.expected.txt` and remove those `DIVERGENT`
  entries.
- [ ] 3.3 Verify the domain: V1 `particles_js`; V2 particle goldens; V4 catalog
  through both runtimes, `web_11_particles`, compare; timing within the
  ADR 0049 budget.

## 4. Post effects (R24)

- [x] 4.1 Migrate `setPostEffects` validation to the prelude (9-float entry
  wire), deleting `read_post_entry` (desktop) and `__efxPostEntry` (web).
- [ ] 4.2 Verify: V1 `f5b_js`; V2 post goldens; V4 `web_5b_validation`,
  catalog, compare; timing within budget.

## 5. Fonts (R25)

- [ ] 5.1 Migrate `createFont` option validation (size, glyphs, padding,
  filter, outline, shadow) to the prelude. Delete the desktop reader and the
  web free-before-throw path. `drawText`/`measureText` layout options stay
  native unless ADR 0049 lists them as movable.
- [ ] 5.2 Verify: V1 `font_js`; V2 text goldens; V4 `web_8a_text`, catalog,
  compare; timing within budget.

## 6. Physics (R26)

- [x] 6.1 Migrate shape, body, character, static-mesh and query option
  validation to the prelude, deleting `parse_shape`, `read_common_body_opts`
  and the remaining physics option readers (desktop) and `__physShape`/
  `__physBodyCommonOpts`/`__physMask` (web; `__physBodyVec3` stays with the
  hot accessors). Apply strict numbers (D6).
- [x] 6.2 Resolve `12.createbody-noshape`, `12.createstaticmesh-noopts`,
  `12.raycast-noopts`, `12.overlap-noopts`, `12.shapecast-noopts`,
  `12.step-nodt` and `coercion.phys-number-string` (now `TypeError` on both).
  Update the expected file, remove the `DIVERGENT` entries, and replace any
  `api_tests` assertion that relied on desktop coercion.
  (No api_tests assertion relied on coercion; physics_js passes unchanged.)
- [ ] 6.3 Verify: V1 `physics_js`, `efx_physics_tests`; V2 `smoke_12_physics`,
  `smoke_showcase_physics`; V4 `web_12_physics`, catalog, compare; timing
  within budget.

## 7. Audio (R27)

- [x] 7.1 Migrate `playAudio` options and the loader argument checks to the
  prelude. Map the not-found, unreadable and undecodable loader failures to
  dedicated codes (D4).
- [x] 7.2 Resolve `14.loadaudiodata-missing` and `14.loadaudiostream-missing`.
  Update the expected file and `DIVERGENT`.
- [ ] 7.3 Verify: V1 `audio_js`; V2 `smoke_14_audio`,
  `smoke_showcase_audio`; V4 `web_14_audio`, catalog, compare.

## 8. Resource construction (R28)

- [ ] 8.1 Migrate `createImageData`, `createTexture`, `createRenderTarget`,
  `createMeshData` (surfaces + materials) and `loadMeshData`/`loadImage`
  argument validation to the prelude. Use the D4 load-failure codes.
- [ ] 8.2 Resolve `6a.loadimage-missing`, `6b.loadmesh-missing` and
  `6b.loadmesh-corrupt`. Update the expected file and `DIVERGENT`.
- [ ] 8.3 Verify: V1 `meshdata_js`, `mesh_js`, `createTexture_js`,
  `resource_js`, `gltf_js`, `f4b_js`, `f5a_js`; V2 goldens; V4
  `web_2d_validation`, `web_3d_validation`, `web_6b_gltf`, catalog, compare;
  timing within budget.

## 9. Lights and cameras (R29)

- [ ] 9.1 Migrate `setLight`, `setDirectionalLight`, `setCamera2D` and
  `setCamera3D` validation to the prelude (or leave any that ADR 0049 marks
  hot).
- [ ] 9.2 Resolve `4a.light-slot`, `4a.light-slot-neg`, `4a.light-pos-short`
  and `11.billboard-pos`. The last one is resolved here only if
  `drawBillboard` moved under ADR 0049; otherwise unify the native messages in
  both bindings. Update the expected file and `DIVERGENT`.
- [ ] 9.3 Verify: V1 `f4a_js`, `camera3d_js`, `camera_snapshot`; V2 lighting
  goldens; V4 `web_4a_validation`, catalog, compare.

## 10. Docs

- [ ] 10.1 Update `docs/js-api.md`:
  - layering: cold-path argument validation lives once in the shared
    prelude;
  - the "adding API" process: write the validator once and keep natives
    marshal-only;
  - the strict-number rule, stated explicitly.
- [ ] 10.2 Update the `AGENTS.md` "JS API layering" bullet to cite ADR 0049.
  Update `docs/refactoring.md` §5 status and the measured Δ.
- [ ] 10.3 Verify `docs/api/` is unchanged: `npm --prefix gallery run
  docs:markdown` then `git diff --exit-code docs/api`. `efx.d.ts` needs no
  edit.

## 11. Verification and close-out

- [ ] 11.1 Confirm `DIVERGENT` in `tests/scripts/s_error_catalog.js` is empty,
  and `smoke_error_catalog`/`web_error_catalog` match the updated expected
  file byte-for-byte.
- [ ] 11.2 Run V2 + V3 locally. Run V4
  (`python3 tools/verify_remote.py all shared-option-validation`) and confirm
  green.
- [ ] 11.3 Dispatch V5 (`gh workflow run ci.yml --ref shared-option-validation`)
  in the order Linux → Windows → macOS. Verify: green; record the run id.
- [ ] 11.4 Merge to `main` and push (per `AGENTS.md`). Confirm ADR 0049 is
  written and indexed. Archive the change with spec sync (`js-api`).
