## Why

`docs/refactoring.md` (the volume-reduction plan) found that the test net has
**already drifted**. Unit-test case names are kept twice: once in each suite's
C dispatch and once in a CMake `foreach(CASE …)` list. Four compiled cases
were never registered with ctest:

- `t_thin_floor_large_dt`, `t_fast_body_thin_floor` and `t_force_substep` —
  the ADR 0045 tunneling tests.
- `clear_color_js`.

The plan also found test boilerplate repeated in every suite, unused physics
math, about 35 over-exported C functions, and duplicate web helpers. This
change makes the safety net trustworthy and removes the dead surface before any
production code is restructured. It is the plan's **Phase A–B (R0–R7)** —
**Checkpoint 1**.

This is post-F14 maintenance, not a roadmap milestone. It implements no
F1–F14 milestone and changes no spec-level behavior.

## What Changes

- **R0 — baseline.** Record the volume baseline, the three `ctest -N`
  inventories (headless, full desktop, Emscripten) and the `efx_core` symbol
  list. All later passes are compared against these.
- **R1 — fix the never-run tests.** Register the four orphaned cases with
  ctest.
  - On 2026-10-01 all four were run directly against `main` (`cdf1c67`,
    Windows/MSVC Release) and all four passed (exit 0).
  - If one fails on another target (Linux, macOS, Emscripten), the bug is fixed
    in this change. ADR 0045 and the existing specs already require the
    behavior these cases test.
- **R2 — shared test support.** Add `tests/test_support.h` with `fail`, `feq`,
  `EFX_CASE` and `efx_test_main`. The seven hand-written `strcmp` dispatch
  `main`s and the seven `fail()`/`feq()` copies collapse into `CASE` tables.
- **R3 — one source of truth for case names.** CMake reads each suite's
  `EFX_CASE(name)` tokens, so the hand-written case lists in
  `tests/CMakeLists.txt` are deleted and the drift behind R1 cannot happen
  again.
- **R4 — shorter assertions.** A `REQUIRE` macro replaces 89 four-line
  clean-up-and-fail blocks in `api_tests.c` (and 9 in `resource_tests.c`). A
  compile-time `T_HELPER` literal replaces the 6 pasted copies of the JS
  `t(fn, kind)` helper.
- **R5 — remove unused physics math.** Delete `efx_quat`, `efx_mat3` and 8
  unused inline helpers from `src/physics/efx_phys_vec.h`.
- **R6 — internal linkage.** Header-declared `efx_*` functions that only their
  defining file uses become `static`, and their declarations are dropped.
- **R7 — duplicate web helpers.** Remove:
  - the second `__efxAllocCStr`;
  - the identical `__physNumber`/`__efxAudioNum` pair (one shared helper
    remains);
  - the `efx_bridge_mem_free` export, in favor of the already-exported `_free`.
- Update the `docs/refactoring.md` status for R0–R7.

Estimated effect: about −650 non-blank lines, with **+4** registered test cases.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- None. No pass changes spec-level behavior. R1 only registers cases that
  verify behavior the specs already require. The change sets
  `skip_specs: true`.

## Non-goals

- Any script-facing API change, error-message change, or golden re-baseline.
  `tests/scripts/s_error_catalog.expected.txt` stays byte-identical.
- Removing or weakening any test. The test inventory may only grow (R1).
- Restructuring production code beyond deletions and `static` linkage. That is
  done by `refactor-volume-core` (R8–R16).
- Removing ADR-named test seams (`efx_input_inject_*`, gamepad
  inject/`load_mappings`) or committed generated files.

## Impact

- **Tests/build:** `tests/test_support.h` (new); `tests/physics/test_support.h`
  (now includes it); `tests/unit/*.c` and `tests/physics/main.c` (`CASE`
  tables, `REQUIRE`); `tests/CMakeLists.txt` (case registration).
- **Code:**
  - `src/physics/efx_phys_vec.h` (R5).
  - Headers and `.c` files across `src/` that gain `static` linkage (R6).
  - `src/web/js/core.js`, `physics.js`, `audio.js` and the `mem_free` call
    sites; `src/web/bridge_render2d.c` (R7).
- **Docs:** `docs/decisions/0040-physics-core.md` — a one-line wording fix
  ("own vec3/quat/mat3" → "own vector math"). **No ADR**: no new decision is
  made; dead code that an existing ADR described is removed.
  `docs/refactoring.md` status. `docs/js-api.md`, `docs/api/`, `efx.d.ts` and
  `AGENTS.md` are unchanged.
- **Verification:**
  - V1/V2 locally, then V4 (`tools/verify_remote.py all`).
  - Checkpoint 1 runs the four-target gate (V5, Linux → Windows → macOS).
  - The ctest inventory must equal the R0 baseline plus exactly the four R1
    names on every target.
