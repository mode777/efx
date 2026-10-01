## Context

See `proposal.md` (Why) and `docs/refactoring.md` §2.6–§2.7 and R17–R21.
The current facts that shape the approach:

- **Root `CMakeLists.txt`** builds 8 test executables (L362–L631). Each
  repeats:
  - `target_include_directories(... src)`;
  - `-lm` on UNIX;
  - `-sALLOW_MEMORY_GROWTH=1` on Emscripten;
  - MSVC `/W4 /WX` vs `-Wall -Wextra -Werror`.

  The real per-target differences:
  - `/W3` for `efx_math_tests` (GLM is not `/W4`-clean);
  - `_CRT_SECURE_NO_WARNINGS` for `audio`/`text`/`api`;
  - `_POSIX_C_SOURCE` for `text`/`api`;
  - fixture `EFX_*` definitions;
  - vendor include dirs;
  - desktop-only guards for `resource`/`text`/`api`.

  `efx_core` lists its sources twice (L90 web, L117 desktop = web + API +
  runtime).
- **`tests/CMakeLists.txt`.** `add_player_test` and `add_web_test` both
  build a `cmake -P run_test.cmake` command with the same escaping. About 20
  portable cases are registered twice with identical arguments for both
  runtimes. Others differ on purpose:
  - `smoke_log` vs `web_log` (`EXPECT_OUT`);
  - `smoke_quit3` vs `web_quit3`;
  - `smoke_14_audio` vs `web_14_audio` (root form);
  - `web_error_catalog` (`ASSETS`);
  - `web_pose`/`web_6b_gltf` (probe fixtures);
  - the root-mode and desktop-only cases.
- **Web runners.** `tools/run_web_goldens.mjs` and `tools/run_web_harness.mjs`
  each embed a `PAGE_HTML` template. They share the canvas, the BeginFrame
  keep-alive animation and the rAF wrapper. They differ in title and logging:
  the goldens runner logs the first three rAF ticks and unhandled rejections.
- **Comments.** `src/` has 1 737 comment-only lines; 101 cite `design D#` and
  16 cite `P#`.
- **`AGENTS.md`** is 569 lines. "Current state" spans L8–L441, of which
  L382–L441 are operational rules (CI triggers, Pages, verification order,
  SSH pre-check, commit/push, merge-after-apply). "Not yet decided" (L560)
  calls the glTF import profile open, but ADR 0032 settled it.
- **Resource exposure to scripts is unaffected:** no class, finalizer or bank
  changes (ADR 0011/0012).

## Goals / Non-Goals

**Goals:**

- Make the build and test registration say each thing once, with
  byte-identical compile commands and test inventory.
- Make `AGENTS.md` point rather than restate, without losing any operational
  rule or any fact an agent needs to work in the repo.

**Non-Goals:**

- Reorganizing CMake beyond the test executables and the `efx_core` list
  (vendor helpers already exist; platform and player targets stay as they
  are).
- Rewriting comments for style. R20 deletes or re-points; it does not
  rephrase.

## Decisions

### D1 — `efx_add_test_exe` with explicit per-target differences

```cmake
efx_add_test_exe(name
  SOURCES … [LIBS …] [DEFS …] [INCLUDES …]
  [MSVC_WARN /W3] [DESKTOP_ONLY])
```

The function applies the shared include, `m`, memory-growth and warning
flags. Every real difference stays an explicit argument at the call site.
`EFX_RENDER_CORE_SOURCES`, `EFX_PHYSICS_SOURCES` and `EFX_API_SOURCES` are set
once and reused by `efx_core` and the tests.

Applied refinement (2026-10-01): `efx_math_tests` is the one executable that
today has no `-sALLOW_MEMORY_GROWTH=1` on Emscripten, and the proposal's
non-goals forbid changing link options — so the helper takes an explicit
`NO_EM_GROWTH` opt-out (math only) and `DEFS` splits into `DEFS`/
`MSVC_DEFS`/`POSIX_DEFS` so each definition keeps its original compiler
scoping. Per-target flags stay byte-identical (E5, plus the link line by
construction). No ADR follows; this is flag preservation, not a new
decision.

- **Proof:** `compile_commands.json` from a Ninja configure
  (`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, on the Linux server, where the
  goldens build runs) is identical after sorting entries by file. The Visual
  Studio generator does not emit it, so the Windows side relies on V2 plus V5.
- **Rejected — CMake presets:** they configure builds, not targets, so the
  repetition remains.
- **Rejected — one test executable for all suites:** it changes link sets
  (physics must stay self-contained, ADR 0040) and the test binaries' names.

### D2 — `efx_portable_case` only where both runtimes take identical arguments

A row qualifies only when the script or root, the args, `EXPECT`,
`EXPECT_OUT` and `EXPECT_ERR` are identical for desktop and web. It then
registers `smoke_<name>` on desktop and `web_<name>` on Emscripten through
the shared `efx_add_run_test()`. Every other case stays an explicit
`add_player_test`/`add_web_test` call; both become thin wrappers over
`efx_add_run_test()`.

- **Proof:** the `ctest -N` names and full commands (`ctest -N -V`) are
  identical on the desktop and Emscripten builds.
- **Rejected — unify the differing cases by changing their expectations:**
  that changes what is tested.

### D3 — `hostPage()` reproduces each runner's current page

`hostPage({ title, script, verbose })` emits the shared canvas, keep-alive and
rAF wrapper. `verbose: true` adds the goldens runner's tick logging and
`unhandledrejection` hook.

- **Proof:** each runner's served HTML is semantically identical; runner
  stdout and exit codes are unchanged on V4.
- **Rejected — one runner for both jobs:** the CLIs and env contracts differ,
  and the gate invokes both (refactor-long-functions D3).

### D4 — Comment hygiene rules, proven comment-only

Rules:

- Replace a `design D#`/`P#` pointer with the ADR number when an ADR records
  the decision; otherwise delete the pointer.
- Delete comments that restate the next statement, and history narration.
- Keep invariants, units, ownership and lifetime notes, and every ADR
  reference.

Proof:

- For every touched C/C++ file, `cc -fpreprocessed -dD -E` output is identical
  before and after (E7).
- For JS, review plus V4.
- `prelude.js` edits regenerate `prelude.h` (V3).
- **Rejected — automated stripping:** it cannot tell invariants from noise.
- **Rejected — reflow or reword comments:** churn without volume gain.

### D5 — `AGENTS.md`: point to the record, keep the rules verbatim

The new "Current state" has three parts:

1. **Summary.** One short paragraph: F1–F14 implemented, four-target gate
   green, and where each kind of fact lives — roadmap status in
   `openspec/specs/feature-roadmap` and the Roadmap table, decisions in
   `docs/decisions/README.md`, change records and gate run ids in
   `openspec/changes/archive/`, the API in `docs/api/`.
2. **Milestones.** One line per milestone or follow-up change: the archived
   change folder and its ADR(s). No API surface, no run ids.
3. **Codebase map.** Condensed bullets:
   - `src/` modules;
   - vendored deps (ADR 0006);
   - player run modes (ADR 0007);
   - `prelude.h` regeneration and drift check;
   - headless builds (`-DEFX_HEADLESS=ON`);
   - goldens need a display;
   - gallery build and sample-directory contract (ADR 0030/0044);
   - verification approach (ADR 0020).

The operational rules (current L382–L441) move into the new section
byte-for-byte. "Not yet decided" is corrected: glTF import profile settled in
F6 (ADR 0032).

- **Proof:**
  - the operational-rule block diffs empty;
  - every relative link resolves;
  - a reviewer checklist samples ≥10 removed facts and confirms each is
    reachable from the roadmap spec, an ADR, an archived change, or
    `docs/api/`.
- **Rejected — delete "Current state" entirely:** agents need the codebase map
  and the operational rules in the always-loaded file.
- **Rejected — also slim the Roadmap table:** not covered by the sign-off.

### D6 — R21 is optional and move-only

Move `drawQuad`/`read_quad_opts`/`setBlendMode` to `api_2d.c` and
`poseMesh`/`read_pose_sample`/`setCamera3D` to `api_3d.c`, updating the
registration tables.

- **Proof:** `git diff -M --color-moved=dimmed-zebra` shows only moves and
  declaration lines.
- If any non-move edit is needed beyond includes and declarations, drop R21.
- **Rejected — also re-home the web fragments:** object key order on `efx`
  could change.

## Risks / Trade-offs

- **[Risk] A CMake helper silently changes a flag.** → D1 proof via sorted
  `compile_commands.json`, plus V5 configure and build on all four targets.
- **[Risk] A portable row drops a desktop- or web-only expectation.** → D2
  qualification rule, plus a `ctest -N -V` comparison.
- **[Risk] Agents lose context they relied on in `AGENTS.md`.** → D5 keeps the
  codebase map and the rules verbatim; the reviewer checklist samples removed
  facts.
- **[Risk] Comment edits accidentally change code.** → The E7 preprocessed
  diff is empty for every touched C/C++ file.
- **[Trade-off] Comment pointers to `design D#` lose their deep link into
  archived changes.** → Those links only resolved for readers who knew which
  archive folder to open. ADR numbers are the durable reference.

## Migration Plan

1. Branch `refactor-volume-build` from `main` after `refactor-volume-core` is
   merged. One commit per pass (R17, R18, R19, R20 per module, R21 optional,
   `AGENTS.md`).
2. Per pass: the proof named in its decision, plus V1 or V2.
3. Checkpoint 3:
   - V4 (`python3 tools/verify_remote.py all refactor-volume-build`);
   - then V5 in the order Linux → Windows → macOS.
4. Merge to `main`, push, and archive (`skip_specs`).
5. Rollback: revert per pass. The `AGENTS.md` commit is independent of the
   code passes.
