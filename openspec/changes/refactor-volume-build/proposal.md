## Why

After `refactor-volume-core`, the remaining volume sits in the build, tooling
and documentation layers:

- **Root `CMakeLists.txt`:**
  - 8 test executables repeat the same boilerplate;
  - the render-core source list is spelled out 4×;
  - the `efx_core` list is duplicated between the web and desktop variants.
- **`tests/CMakeLists.txt`** mirrors every portable script case as a
  `smoke_*` and a `web_*` row, through two near-identical registration
  functions.
- **The web runners** embed near-identical host pages.
- **Comments in `src/`** carry 101 `design D#` and 16 `P#` pointers that only
  resolve inside archived change folders.
- **`AGENTS.md` "Current state"** (lines 8–441) restates milestone history,
  per-milestone API surfaces and CI run ids. That breaks the file's own rule
  that it "points rather than restates". The owner signed off on slimming it
  on 2026-10-01.
- **`AGENTS.md` "Not yet decided"** still says the glTF import profile is open,
  although ADR 0032 settled it in F6b.

This change executes the plan's **Phase G (R17–R21)** plus the `AGENTS.md`
slimming — **Checkpoint 3**. It is post-F14 maintenance, not a roadmap
milestone, and it lands after `refactor-volume-core`.

## What Changes

- **R17 — root CMake:**
  - `efx_add_test_exe(name SOURCES … LIBS … DEFS … [MSVC_WARN …] [DESKTOP_ONLY])`
    for the 8 test executables;
  - shared `EFX_RENDER_CORE_SOURCES`/`EFX_PHYSICS_SOURCES`/`EFX_API_SOURCES`
    lists reused by `efx_core` and the tests;
  - one common `efx_core` list, with the desktop variant appending the
    API/runtime sources.
- **R18 — tests CMake:**
  - one internal `efx_add_run_test()` holds the argument escaping;
  - `efx_portable_case(…)` registers `smoke_<name>` (desktop) and
    `web_<name>` (Emscripten) from a single row;
  - genuinely different cases stay explicit.
- **R19 — web runners:** `hostPage({ title, script, verbose })` in
  `tools/lib/web-host.mjs` replaces the two `PAGE_HTML` templates.
- **R20 — comment hygiene in `src/`:**
  - `design D#`/`P#` pointers become ADR numbers or are deleted;
  - comments that restate code and history narration ("formerly …",
    "entry.js") are removed.
  - Invariants and the non-obvious "why" stay.
- **R21 (optional, move-only):** move `drawQuad`/`read_quad_opts`/`setBlendMode`
  to `api_2d.c` and `poseMesh`/`read_pose_sample`/`setCamera3D` to `api_3d.c`.
- **AGENTS.md slimming (signed off):** replace the "Current state" milestone
  narrative with a short per-milestone status list that links the roadmap
  spec, the archived change and the ADR.
  - The operational rules stay **verbatim**: CI triggers, Pages,
    verification order, the SSH pre-check, the commit/push and merge policy,
    and the OpenSpec CLI notes.
  - The structural facts agents rely on are condensed, not dropped: module
    layout, run modes, `prelude.h` regeneration, headless builds, gallery
    build, and the verification approach.
  - The stale "glTF import profile remains open" statement is corrected to
    cite ADR 0032.
- Update the `docs/refactoring.md` status (R17–R21 done, Checkpoint 3), and
  record that the former P21 is now done.

Estimated effect: about −365 non-blank lines of code and tooling, plus about
−300 lines of `AGENTS.md`.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- None. Build, tooling, comment and documentation changes only. The change
  sets `skip_specs: true`.

## Non-goals

- Changing compiler flags, warning levels, link options or the test
  inventory. `compile_commands.json` and `ctest -N` must be identical.
- Changing the `AGENTS.md` operational rules, the "Roadmap" table, the
  "Non-negotiable design constraints", or the "Documentation" section.
- Editing generated files by hand (`docs/api/`, `shaders/*.h`,
  `src/prelude/prelude.h` — the last one is regenerated if `prelude.js`
  comments change).
- Re-homing the web JS fragments: moving methods between them could change the
  `efx` key order.
- Editing `.github/workflows/*.yml`.

## Impact

- **Build/tests:** `CMakeLists.txt`, `tests/CMakeLists.txt`.
- **Tools:** `tools/lib/web-host.mjs`, `tools/run_web_goldens.mjs`,
  `tools/run_web_harness.mjs`.
- **Code:** comment-only edits across `src/` (R20); move-only edits in
  `src/api/api_2d.c`, `api_3d.c`, `api_target_post.c`, `api_particles.c` (R21).
- **Docs:** `AGENTS.md` ("Current state", "Not yet decided"),
  `docs/refactoring.md`. **No ADR.** No new decision: the `AGENTS.md` change
  applies its existing "points rather than restates" rule, and the glTF
  correction cites the existing ADR 0032. `docs/js-api.md`, `docs/api/` and
  `efx.d.ts` are unchanged.
- **Verification:**
  - E5 (`compile_commands.json` identical), E2 (`ctest -N` identical on
    desktop and Emscripten), E7 (comment-only proof for C/C++), V3 for
    `prelude.h`;
  - V4, then the four-target gate (V5) at Checkpoint 3.
