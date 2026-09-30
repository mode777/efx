# Tasks

## 1. Error-message characterization catalog (P0)

- [x] 1.1 Add `tests/scripts/s_error_catalog.js` covering every option-bag and
  resource-liveness path (2D, 3D, lighting, targets, post, resources, text,
  particles/billboards/sprites, input, physics, gamepad, audio), printing a
  stable `Kind: message` line per case; verify it runs headless on desktop with
  `build-h/efx --script tests/scripts/s_error_catalog.js` and exits 0
- [x] 1.2 Commit the captured output as
  `tests/scripts/s_error_catalog.expected.txt`; verify the file is the exact
  stdout of the script and contains one line per case
- [x] 1.3 Add an `EXPECT_OUT_FILE` mode to `tests/run_test.cmake` /
  `add_player_test` (and the `tests/CMakeLists.txt` case) that runs the player
  and byte-compares stdout to the expected file; verify the new case fails when
  the expected file is edited and passes when restored
- [x] 1.4 Add an `error_catalog` case to `tools/run_web_compare.mjs` so the web
  runtime is held to the same expected file; verify a local compare run is
  green
- [x] 1.5 Add a probe for the suspected `phys_opt_number`/`pcfg_num` vs
  `__physNumber` coercion divergence and mark it `// KNOWN-DIVERGENCE` so the
  compare stays green; verify the divergence is recorded in the catalog, not
  fixed

## 2. Dead-export guard (P0b)

- [x] 2.1 Add `tools/check_exports.mjs` reporting `EMSCRIPTEN_KEEPALIVE`
  functions in `src/web/bridge.c` not referenced from `src/web/**` or
  `tools/**`, and public `efx_*` header declarations referenced nowhere else;
  verify running it reproduces the `docs/refactoring.md` §1.2 list exactly
- [x] 2.2 Confirm the guard is advisory (not wired into ctest/CI) and exits 0;
  verify a normal build/test run is unaffected

## 3. Remove unused bridge liveness exports (P1)

- [x] 3.1 Delete the eight `efx_bridge_*_alive` functions
  (`texture`/`fontdata`/`font`/`target`/`particles`/`mesh`/`physics_body`/
  `physics_character`) from `src/web/bridge.c`; verify `tools/check_exports.mjs`
  reports none of them and `rg '_efx_bridge_.*_alive' src/web/entry.js` is empty
- [x] 3.2 Verify the Emscripten build, web goldens, `run_web_harness.mjs` and
  `run_web_compare.mjs` still pass

## 4. Remove unused C exports (P2)

- [x] 4.1 Grep `openspec/specs/` and `docs/decisions/` for every §1.2 symbol;
  record any spec-named contract to keep and drop it from the deletion list;
  verify the decision list is written into the commit message
- [x] 4.2 Delete the api/audio dead exports (`efx_log`,
  `efx_audio_sample_rate`, `efx_audio_available`, `efx_audio_voice_looping`,
  `efx_decoder_channels`) with their header declarations; verify the
  `efx_core` `nm -g --defined-only` diff shows only those symbols
- [x] 4.3 Delete the input/physics dead exports
  (`efx_physics_body_is_dynamic`/`_is_sensor`/`_is_mesh`,
  `efx_physics_shape_is_mesh`, the dangling `efx_character_move` declaration)
  and either delete or unit-test the gamepad seam symbols
  (`efx_input_gamepad_load_mappings`, `efx_input_gamepad_inject_clear`) per
  design D6; verify the `nm` diff and, if kept, the new test
- [x] 4.4 Delete the resource/text dead exports (`efx_resource_root`,
  `efx_text_font_alive`) with their header declarations; verify the `nm` diff
  shows only those symbols
- [x] 4.5 Verify `tools/check_exports.mjs` reports no remaining dead symbols
  (or only deliberately kept, tested seams)

## 5. Docs and verification

- [x] 5.1 Update `docs/refactoring.md` status: mark P0, P0b, P1, P2 done and
  note Checkpoint 1 reached; verify the file no longer lists the deleted
  symbols as live dead code
- [x] 5.2 Run the local unit suites
  (`cmake -B build-h -DEFX_HEADLESS=ON && ctest --test-dir build-h`) and the
  generated-file checks (`python tools/gen_prelude.py --check`,
  `npm --prefix gallery run docs:check`); verify all green
- [x] 5.3 Run `python3 tools/verify_remote.py all <branch>` on the verification
  server (native ctest incl. goldens, Emscripten ctest, web goldens,
  `run_web_compare.mjs` incl. `error_catalog`); verify green before dispatching
  CI
- [x] 5.4 Dispatch `gh workflow run ci.yml --ref <branch>` and confirm the
  Linux → Windows → macOS gate is green; then merge to `main` and push per
  `AGENTS.md`
