# Proposal

## Why

All milestones F1–F14 are implemented and the four-target gate is green, but
`src/` has accumulated structural debt documented in `docs/refactoring.md`
(oversized modules, dead exports, duplicated helpers, long functions). The
plan's Phase A calls for a safety net *before* any restructuring: today the
parity scripts assert only error **classes** (`instanceof TypeError` /
`RangeError`) and `run_web_compare.mjs` diffs stdout that those scripts leave
mostly empty, so a drift in an error *message* between the desktop and web
bindings goes unnoticed — and would go unnoticed if introduced during
refactoring. Separately, nothing detects unused exports: the plan's §1.2
dead-code inventory was built by hand and cannot be re-checked mechanically.
This change lands that safety net (Phase A) and removes the dead code it
confirms (Phase B) — the plan's **Checkpoint 1**.

This is post-F14 maintenance, not a roadmap milestone: it implements no
F1–F14 milestone and changes no spec-level behavior.

## What Changes

- **Error-message characterization catalog** (`P0`): add
  `tests/scripts/s_error_catalog.js`, which walks every option bag and
  resource-liveness path and prints `Kind: message` per case; commit its
  expected output as `tests/scripts/s_error_catalog.expected.txt`; add an
  `EXPECT_OUT_FILE` mode to `tests/run_test.cmake` / `add_player_test`; add an
  `error_catalog` case to `tools/run_web_compare.mjs`. The catalog pins exact
  messages and records the one suspected coercion divergence as a
  `// KNOWN-DIVERGENCE` line (recorded, not fixed).
- **Dead-export guard** (`P0b`): add `tools/check_exports.mjs`, an advisory
  script that reports `EMSCRIPTEN_KEEPALIVE` functions in `bridge.c` not
  referenced from `src/web/**`/`tools/**`, and public `efx_*` definitions
  referenced nowhere else. Not part of the gate.
- **Remove unused bridge liveness exports** (`P1`): delete the eight
  `efx_bridge_*_alive` functions from `src/web/bridge.c` that `entry.js` never
  calls (entry.js tracks liveness itself).
- **Remove unused C exports** (`P2`): delete the dead C functions listed in
  `docs/refactoring.md` §1.2 together with their header declarations, one
  commit per module, after confirming no spec/ADR names them as a C-level
  contract. Newly found on re-verification: `efx_audio_voice_looping`; also
  resolve the dangling `efx_character_move` declaration (no definition, no
  caller). The two gamepad injection/mapping helpers are candidates to keep as
  test seams — if kept, they gain a unit test instead of deletion.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- None. This is a pure refactor plus test/tooling work: no spec-level behavior
  changes, so no spec should change. The change sets `skip_specs: true`.

## Impact

- **Code**: `src/web/bridge.c` (P1); `src/api/api.c`+`api.h`,
  `src/audio/audio.c`+`audio.h`, `src/audio/decode.c`+`decode.h`,
  `src/input/efx_gamepad.c`+`efx_gamepad.h`, `src/physics/world.c`+`world.h`
  (+`physics.h`), `src/resource/resource.c`+`resource.h`,
  `src/render/text.c`+`text.h` (P2).
- **Tests/tools**: `tests/scripts/s_error_catalog.js` +
  `.expected.txt`, `tests/run_test.cmake`, `tests/CMakeLists.txt`,
  `tools/run_web_compare.mjs`, `tools/check_exports.mjs` (P0/P0b).
- **Docs**: update `docs/refactoring.md` status (P0–P2 done). No ADR — the
  change settles no durable architecture decision and is behavior-preserving.
  No `js-api` delta, no `gallery/src/api/efx.d.ts` change, no `docs/api/`
  regeneration, no golden re-baseline.
- **Verification**: the existing suites (ctest smoke + unit) plus the new
  `error_catalog` case; the plan's V4 server pre-check and the four-target gate
  (V5) as the integration check. No golden images are added or changed.

## Non-goals

- **Any observable behavior change, including error messages.** The suspected
  numeric-coercion divergence is *recorded* by P0, not fixed; fixing it is a
  separate behavior change with its own `js-api` delta (see
  `docs/refactoring.md` §4).
- **The structural passes P3–P21** (file splits, helper consolidation, long
  functions, tools, naming/ADR hygiene) — those land as separate changes.
- **Removing symbols a spec names as a C-level contract**, or symbols kept as
  test seams.
- Adding the guard script to the CI gate (it is advisory).
