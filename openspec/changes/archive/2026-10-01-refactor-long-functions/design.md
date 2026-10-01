# Design

## Context

See `proposal.md — Why` and `docs/refactoring.md` (§1.1 long-function table,
§1.3 Tools, §1.4, §2, Phases G–I). Facts that shape the approach:

- `efx_pipeline_play` (`src/platform/pipeline.c` L1181, 299 lines) covers
  scratch sizing, quad-run emission, billboard/particle emission with the alpha
  depth sort, the VBO upload, render-target pass switching, per-record draws and
  the post chain. The clip-depth remap lives in `play_mesh_record` and must not
  move (ADR 0025).
- The long binding functions parse option bags and then record; decomposing them
  must keep the exact validation order so the first failing check throws the
  same message.
- `run_web_goldens.mjs`, `run_web_harness.mjs`, `run_gallery_smoke.mjs` and the
  server half of `test_web_assets.mjs` each duplicate the `puppeteer-core`
  dynamic import, the static `http.createServer`, and the browser launch/page
  wiring.
- `src/input/efx_input.{c,h}` and `efx_gamepad.{c,h}` are the only prefixed
  module files. Two ADRs share number 0042. `AGENTS.md` "Current state" is now
  lines 8–441 (~430 lines).
- `refactor-safety-net` provides the byte-pinned `error_catalog`; P15/P16
  rely on it to prove no message changed.

## Goals / Non-Goals

**Goals:**

- Make the long functions read as orchestration of named steps under ~60 lines
  each, without changing error order, output, or the `efx_core` symbol surface.
- Share the web-runner and CMake-vendor boilerplate without changing runner
  CLIs/env/exit codes or compiler flags.
- Finish the naming and documentation hygiene items, including the ADR
  renumber to the correct free slot.

**Non-Goals:**

- Behavior changes and renderer-invariant changes.
- Forcing P21 without owner sign-off.

## Decisions

### D1 — Extract in call order, preserve the first failing check

For each long function, extract helpers in the order the original executes, and
keep the early-return/error sequence identical (same checks, same order, same
messages). `efx_pipeline_play` becomes `ensure_scratch` → `emit_quad_runs` →
`emit_billboards_and_particles` (+ `sort_particles_back_to_front`) →
`upload_vertices` → `play_records`, with `draw_textured` and `grow_int_pair`
replacing the repeated bind/draw and array-growth blocks.

- **Why**: the catalog pins messages and the goldens pin output; preserving
  execution order means both stay byte-identical and the refactor is provably
  behavior-preserving.
- **Alternatives**: reorder for "cleanliness" (would change which error a bad
  input produces — a behavior change); leave the functions long (the plan's
  whole point is reviewability).

### D2 — P15's `draw_textured` does not touch the depth remap

The three "apply pipeline → bind vbuf + view + sampler → draw" copies collapse
into `draw_textured(pip, handle, first, count)`. The clip-depth fold stays in
`play_mesh_record`, untouched.

- **Why**: ADR 0025 makes that fold a cross-platform correctness invariant;
  moving it into a shared helper risks applying it to non-mesh draws.
- **Alternatives**: fold it into `draw_textured` (rejected: wrong scope,
  platform regressions on D3D11/Metal).

### D3 — Shared web host library keeps each runner's contract

`tools/lib/web-host.mjs` exports `loadPuppeteer()`, `serveStatic(root, routes)`
and `launchBrowser(opts)`. Each runner keeps its own CLI parsing, env vars
(`CHROME_SHELL_PATH`, ports), and exit codes; only the duplicated mechanics
move.

- **Why**: the plan requires unchanged output and exit codes, and the runners
  are invoked by the gate and the SSH pre-check with specific env vars.
- **Alternatives**: a single configurable runner (would merge distinct CLIs and
  risk gate behavior changes).

### D4 — Vendor helper must produce identical flags

`efx_add_vendor_library(name SOURCES … INCLUDES …)` centralizes the
include/warning-relaxation boilerplate; the acceptance check is that
`compile_commands.json` is byte-identical before/after.

- **Why**: the vendor targets deliberately relax warnings per toolchain; a
  helper that changes flags would silently alter diagnostics.
- **Alternatives**: leave the duplication (the plan marks P18 optional; it may
  be dropped if the flag diff cannot be kept clean).

### D5 — Renames are `git mv` + include/CMake updates only

P19 renames the input files and updates `#include`s and CMake source lists;
symbol names and behavior are unchanged.

- **Why**: removes the naming inconsistency with a reviewable move-only diff.
- **Alternatives**: keep the prefix (the inconsistency the plan flags).

### D6 — ADR renumber to 0048

P20 renames the API-reference ADR file to `0048-…` (0043–0047 are occupied by
`web-pointer-focus-default`, `curated-sample-dirs`, `physics-substepping`,
`physics-world-holds-live-bodies`, `audio-source-model`), updates the
`docs/decisions/README.md` index row and any citations. The audio ADR keeps
0042.

- **Why**: resolves the duplicate number against the actual next free slot;
  the earlier plan target (0043) is now taken.
- **Alternatives**: renumber the audio ADR instead (breaks the `AGENTS.md`
  citation and the audio change's history).

### D7 — P21 is owner-gated

Slim `AGENTS.md` "Current state" to a per-milestone status list linking the
roadmap spec, the archived change and the ADR, keeping operational rules
(verification order, server pre-check, merge/push policy) verbatim. Do not
guess at what the owner wants to keep; require sign-off, else drop the task.

- **Why**: the section is a maintained human summary; silently deleting facts
  the owner relies on is worse than leaving it.
- **Alternatives**: auto-slim without sign-off (rejected by the plan itself).

## Risks / Trade-offs

- **A decomposition reorders an early return** → D1 preserves order; the
  `error_catalog` byte-compare and the matching unit/golden suites catch any
  drift.
- **P15 platform regressions** → V5 through macOS (Metal) and Windows (D3D11)
  exercises the clip-depth/attachment paths.
- **Runner refactor changes gate behavior** → Keep CLIs/env/exit codes; run
  `run_web_goldens`, `run_web_harness`, `run_gallery_smoke` and
  `test_web_assets` locally and compare output.
- **P18 flag drift** → Byte-compare `compile_commands.json`; drop P18 if it
  cannot stay identical.
- **P19 breaks an include path** → V1 + V5 compile all four targets.
- **P20 breaks a link** → Verify every link in `docs/decisions/README.md`
  resolves and grep for stale 0042 references.

## Migration Plan

Land in the plan's order: P15 (with the V5 macOS pass) → P16 (per module) →
P17 → P18 (optional) → P19 → P20 → P21 (owner-gated). Checkpoint 3 is reached
when P16 is in and the V5 gate is green once; P20/P21 are documentation-only
and may land at any time. Rollback is reverting the pass's commit(s); no data
or API migration. Verification uses the plan's V1–V5 ladder.

## Open Questions

- **P21 owner sign-off**: whether to slim `AGENTS.md` "Current state" at all,
  and what facts must survive. Deferrable — the task is dropped if not signed
  off, and it does not affect any other pass.
