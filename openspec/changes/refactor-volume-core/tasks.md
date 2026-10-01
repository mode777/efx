## 1. Render core (R8)

- [x] 1.1 Add `DEFAULT_CAMERA2D`/`DEFAULT_CAMERA3D`/`DEFAULT_MATERIAL`
  constants and `apply_default_state()` in `render_records.c`. Route
  `ensure_state`, `efx_render_reset_state` and `efx_material_default` through
  them (D1). Verify: V1 `render_tests` (`default_camera_viewport`,
  `lights_state`, `material_binding`, `lighting_reference`) and `api_tests`
  `default_camera`.
- [x] 1.2 Make `record_push` stamp `target`/`sort_key`, remove the stamps from
  the 6 producers, and add `color_or_white` for the 3 color loops (D2).
  Verify: V1 `record_fields`, `mesh_record_fields`, `billboard_record_fields`,
  `segmentation`, `batching`, `record_budget`.
- [x] 1.3 Inline `map_bind_retain`/`map_bind_release` into
  `texture_bind_retain`/`texture_bind_release`. Verify: V1 `map_retention`,
  `material_maps`, `target_deferred_release`, plus V2 goldens.

## 2. Input, audio and gamepad (R9)

- [ ] 2.1 Rewrite the input feeders in `input.c` with compound literals and
  `set_level` (D3). Verify: V1 `efx_input_tests` (`level_edge`, `repeat`,
  `ordering`, `deltas`, `mouse`, `focus`, `chars`, `mods`) and `api_tests`
  `input_js`.
- [ ] 2.2 Add `live_voice()` and collapse the five voice setters plus the
  voice queries in `audio.c`. Verify: V1 `efx_audio_tests` (all cases) and
  `api_tests` `audio_js`.
- [ ] 2.3 Add `live_slot()` and one `name_lookup()` in `gamepad.c`/`input.c`
  for the accessors and the four name↔id pairs. Verify: V1 `gp_*` cases and
  `api_tests` `gamepad_js`; V4 `web_9_input`, `web_13_gamepad`.

## 3. Runtime and player (R10)

- [ ] 3.1 Free the hook lists in `efx_runtime_destroy` by looping over
  `efx_host_hook_list`. Verify: V1 `api_tests` `hooks_registration`,
  `module_hooks_js`.
- [ ] 3.2 Add `efx_player_run_entry` and `efx_player_exit_code` (D4) and use
  them in `run_root_mode` and `efx_repl_run`, keeping the teardown order.
  Verify: V2 `smoke_root_*`, `smoke_quit3`, `smoke_10_*`, `smoke_repl_*`,
  `api_tests` `repl_eval`, and the exit-code contract (ADR 0007).

## 4. Desktop `destroy()`/finalize and the pending-exception fix (R11)

- [ ] 4.1 Add `efx_runtime_context()` to `runtime_internal.h`, and the
  `destroy_no_pending_exception` case to `api_tests.c`. It destroys one
  ImageData, MeshData, Mesh, RenderTarget, FontData, Font, ParticleSystem,
  AudioData, AudioStream and Audio, and asserts `!JS_HasException(ctx)` after
  each. Run it on the old code and record the result per class in
  `design.md` → "Findings during apply".
- [ ] 4.2 Add `destroy`/`release` hooks to `CLASS_SPECS`. Replace
  `js_destroy_resource` and the per-class finalizers with
  `JS_GetAnyOpaque`-based dispatch (D5), keeping every message. Verify: the
  new case passes; V1 `texture_lifecycle`, `mesh_js`, `f5a_js`, `font_js`,
  `audio_js`, `particles_js`; V2 `smoke_resource_lifecycle`;
  `smoke_error_catalog` byte-identical.

## 5. Desktop physics wrapper and option readers (R12)

- [ ] 5.1 Introduce `efxjs_collider` with per-kind list heads
  (`physics_colliders[2]` in `efx_host_state`). Share wrap, pin/unpin,
  finalize, live-resolve, `find_wrapper` and teardown, and keep distinct
  `Body`/`Character` classes (D6). Verify: V1 `physics_js`; V2
  `smoke_12_physics`, `smoke_showcase_physics`; `smoke_error_catalog`
  byte-identical.
- [ ] 5.2 Extend `efx_api_opt_*` with the D7 policy flags. Delete
  `audio_opt_number`/`audio_opt_bool`, `phys_opt_*` and `get_opt_number`,
  passing each site's current policy and message. Verify: V1 `audio_js`,
  `physics_js`, `font_js`; the catalog stays byte-identical, including
  `coercion.phys-number-string`.

## 6. Web binding (R13–R15)

- [ ] 6.1 Add `__efxRc` to `core.js` and replace the 27 return-code ladders
  (D8). Verify: V4 `web_*` ctest and `web_error_catalog` byte-identical.
- [ ] 6.2 Reorder allocations after validation, or wrap them in
  `try/finally`, in `text.js`, `render3d.js`, `resource.js`, `physics.js`,
  `particles.js`, `render2d.js`, `audio.js`, `input.js` and `boot.js`.
  Remove every hand-written free-before-throw. Verify: V4 compare,
  `run_web_harness.mjs`, `web_error_catalog` byte-identical.
- [ ] 6.3 Add `__efxSourceRect(tex, v)` and use it in `drawQuad`,
  `drawBillboard` and the sprite parser. Verify: V4 `web_2d_validation`,
  `web_11_particles`, catalog.
- [ ] 6.4 (R14) Add `EFX_WEB_CORE_EXPORTS` to `CMakeLists.txt`, delete the 31
  passthrough `efx_bridge_*` functions, rename the JS call sites, and teach
  `tools/check_exports.mjs` to read the CMake list (D9). Verify:
  `check_exports.mjs` reports zero; V4 (web ctest, web goldens,
  `run_web_harness.mjs`, gallery smoke); E6 export diff shows renames only.
- [ ] 6.5 (R15) Add `efx_bridge_input_event`/`efx_bridge_input_state`
  (D10), delete the 20 per-field getters, and read through `HEAPF64` in
  `core.js`/`input.js`. Verify: V4 `web_9_input`, the
  `run_web_harness.mjs` input scenarios, and gallery smoke click-to-focus key
  delivery (ADR 0043).

## 7. Pipeline (R16)

- [ ] 7.1 Derive the billboard pipeline from the quad descriptor. Change
  `post_draw` to one sampler and `fs == NULL`, add `blur_two_pass`, and merge
  `emit_quad`/`emit_quad_bridged` (D11). Leave `play_mesh_record` untouched.
  Verify: V2, all goldens (quads, billboards/particles, post, render
  targets); V4 web goldens.

## 8. Checkpoint 2 verification

- [ ] 8.1 Run the full local suite: V2 and V3. Confirm
  `s_error_catalog.expected.txt` is unchanged and the ctest inventory equals
  the post-`refactor-volume-tests` inventory plus `destroy_no_pending_exception`.
  Record the volume Δ (E4).
- [ ] 8.2 Push and run V4
  (`python3 tools/verify_remote.py all refactor-volume-core`). Verify: green.
- [ ] 8.3 Dispatch V5 (`gh workflow run ci.yml --ref refactor-volume-core`)
  in the order Linux → Windows → **macOS** (required for R16). Verify: green;
  record the run id.

## 9. Docs and close-out

- [ ] 9.1 Update `docs/refactoring.md`: mark R8–R16 done, record the R11
  finding (confirmed or not) under §6, note Checkpoint 2 and the measured Δ.
- [ ] 9.2 Merge to `main` and push (per `AGENTS.md`), then archive the change
  (`skip_specs`).
