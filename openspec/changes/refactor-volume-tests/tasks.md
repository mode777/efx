## 1. Baseline (R0)

- [x] 1.1 On `main`, record the baseline in `design.md` → Context:
  - the `docs/refactoring.md` §1 volume number;
  - `ctest -N` name lists from the headless (V1), full desktop (V2) and
    Emscripten (V4 server) builds;
  - `nm -g --defined-only` (or `dumpbin /symbols`) of `efx_core`.

  Store the three name lists as sorted text attached to the PR, not inlined.
  Verify: re-running the commands on a clean checkout reproduces them.

## 2. Fix the never-run test cases (R1)

- [x] 2.1 Add `t_thin_floor_large_dt`, `t_fast_body_thin_floor` and
  `t_force_substep` to the physics case list, and `clear_color_js` to the
  `efx_api_tests` list, in `tests/CMakeLists.txt`. Verify: V1 + V2 pass, and
  `ctest -N` shows exactly the four new names compared with 1.1.
- [x] 2.2 Run the four cases on Linux and Emscripten via V4. If any fails,
  fix the underlying bug in its own commit and record the root cause in
  `design.md` → "Findings during apply". Verify: all four pass on every V4
  target.

## 3. Shared test support and `CASE` tables (R2)

- [x] 3.1 Add `tests/test_support.h` with `fail`, `feq` (0.001 tolerance),
  `EFX_CASE(fn)` and `efx_test_main(cases, n, argc, argv)` (D1/D3). Make
  `tests/physics/test_support.h` include it, and switch `tests/physics/main.c`
  to `EFX_CASE`/`efx_test_main`. Verify: `efx_physics_tests` passes every
  case, with and without an argument.
- [x] 3.2 Convert `math_tests.c`, `input_tests.c`, `audio_tests.c` and
  `text_tests.c` to the shared header and a `CASE` table, deleting their local
  `fail`/`feq`/`strcmp` mains. Verify: V1; an unknown case exits 2.
- [x] 3.3 Convert `render_tests.c`, `resource_tests.c` and `api_tests.c` the
  same way. Verify: V1 + V2, and `ctest -N` equals the post-2.1 inventory.

## 4. One source of truth for case names (R3)

- [x] 4.1 Add `efx_register_unit_cases(target source)` to
  `tests/CMakeLists.txt` (D2), with `CMAKE_CONFIGURE_DEPENDS` on the source.
  Replace every hand-written `foreach(CASE …)` list, keeping the
  desktop-only guards for `resource`/`text`/`api`. Verify: the V1 and V2
  `ctest -N` lists equal the post-2.1 inventory.
- [x] 4.2 Verify on the server (V4) that the Emscripten build's ctest
  registers the same unit-case names as before for its suites.

## 5. Shorter assertions (R4)

- [x] 5.1 Add `REQUIRE` and `T_HELPER` to `api_tests.c` (D4). Convert all 89
  clean-up-and-fail blocks and the 6 pasted `t(fn, kind)` helpers. Verify: V1;
  flip one assertion locally once to confirm the message is printed and
  `end_js()` runs; then revert the flip.
- [x] 5.2 Apply the equivalent `REQUIRE` to the 9 blocks in
  `resource_tests.c`. Verify: V2 (resource tests are desktop-only).

## 6. Delete dead code and unused surface (R5–R7)

- [x] 6.1 (R5) Delete `efx_quat`, `efx_mat3` and the 8 unused inline helpers
  from `src/physics/efx_phys_vec.h` (D6). Change ADR 0040's
  "`vec3`/`quat`/`mat3` math" to "its own vector math". Verify: a grep finds
  zero references, and V1 `efx_physics_tests` passes.
- [x] 6.2 (R6) For each module (api, runtime, render, resource, audio,
  physics, web), make the single-file `efx_*` functions `static` and drop
  their declarations, honoring the D7 exclusions. One commit per module.
  Delete any function the compiler reports as unused. Verify per commit: V1,
  plus `nm` (E3) showing removals only.
- [x] 6.3 (R7) Delete the inner `__efxAllocCStr` and merge
  `__physNumber`/`__efxAudioNum` into `__efxFiniteNumber` in `core.js`.
  Switch every `bridge['_efx_bridge_mem_free']` call to `bridge['_free']` and
  delete `efx_bridge_mem_free` from `bridge_render2d.c` (D8). Verify:
  `node tools/check_exports.mjs` reports zero; `web_error_catalog` is
  byte-identical.

## 7. Checkpoint 1 verification

- [x] 7.1 Run the full local suite: V2 (`ctest --test-dir build -C Release`,
  all goldens) and V3 (`python tools/gen_prelude.py --check`). Confirm
  `tests/scripts/s_error_catalog.expected.txt` is unchanged and record the
  volume Δ (E4).
- [x] 7.2 Push the branch and run V4
  (`python3 tools/verify_remote.py all refactor-volume-tests`). Verify: green,
  and the Emscripten `ctest -N` equals the R0 inventory plus the four R1
  names.
- [ ] 7.3 Dispatch V5 (`gh workflow run ci.yml --ref refactor-volume-tests`)
  in the order Linux → Windows → macOS. Verify: the four-target gate is green;
  record the run id.

## 8. Docs and close-out

- [ ] 8.1 Update `docs/refactoring.md`: mark R0–R7 done, note Checkpoint 1
  and the measured Δ. Verify: no R0–R7 pass is still marked pending.
- [ ] 8.2 Merge to `main` and push (per `AGENTS.md`), then archive the change
  (`skip_specs`).
