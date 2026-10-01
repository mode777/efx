# Proposal

## Why

After `refactor-split-modules` (Checkpoint 2) the files are domain-sized, but
the plan's §1.1 still lists long functions that are hard to review and easy to
break: `efx_pipeline_play` (299 lines), `efx_text_font_create` (281),
`build_surface` / `efx_gltf_load_meshdata` (197/168), and a set of ~130–190
line binding functions (`efx_js_drawQuad`, `efx_js_createFont`,
`efx_js_drawBillboard`, `read_particle_config`, `parse_sprite`,
`efx_js_createMeshData`, `efx_js_createImageData`, `efx_render_mesh_create`,
`efx_skin_evaluate`, `efx_runtime_new`). The plan's Phase H–I also collects
smaller hygiene items that have accumulated: three puppeteer web runners
duplicate their host/launch boilerplate, the CMake vendor targets repeat
include/warning-relaxation blocks, the input module files are the only ones
with an `efx_` prefix, two ADRs share number 0042, and `AGENTS.md`'s "Current
state" section restates the roadmap against its own "points rather than
restates" rule. This change executes the plan's **Phase G–I** — decompose the
long functions, share the tooling/build boilerplate, and finish the naming and
documentation hygiene — the plan's **Checkpoint 3**.

This is post-F14 maintenance, not a roadmap milestone: it implements no F1–F14
milestone and changes no spec-level behavior. It assumes `refactor-safety-net`
(Checkpoint 1) has landed so the error catalog and dead-export guard are
available.

## What Changes

- **P15**: decompose `efx_pipeline_play` into named steps — `ensure_scratch`,
  `emit_quad_runs`, `emit_billboards_and_particles` (with
  `sort_particles_back_to_front`), `upload_vertices`, `play_records`, plus
  `draw_textured` for the three bind/draw copies and `grow_int_pair` for the
  two parallel-array growths. The clip-depth remap fold in `play_mesh_record`
  (ADR 0025) is left untouched.
- **P16**: decompose the remaining long functions (one PR per module):
  `efx_text_font_create` (text.c), `build_surface` /
  `efx_gltf_load_meshdata` (gltf.c), the binding option-parsers and creators
  (api), `efx_render_mesh_create` (render.c), `efx_skin_evaluate` (skin.c),
  `efx_runtime_new` (runtime.c). Each becomes an orchestration of named steps
  under ~60 lines; error-return order is unchanged.
- **P17**: add `tools/lib/web-host.mjs` (`loadPuppeteer()`, `serveStatic()`,
  `launchBrowser()`) and migrate `run_web_goldens.mjs`, `run_web_harness.mjs`,
  `run_gallery_smoke.mjs` and the server half of `test_web_assets.mjs`,
  preserving each runner's CLI, env vars and exit codes.
- **P18** (optional): add an `efx_add_vendor_library(...)` CMake helper for the
  miniz/cgltf/dr_libs targets; flags in `compile_commands.json` must be
  identical before/after.
- **P19**: rename `src/input/efx_input.{c,h}` → `input.{c,h}` and
  `efx_gamepad.{c,h}` → `gamepad.{c,h}` with `git mv`, updating includes and
  CMake; symbol names unchanged.
- **P20**: renumber `0042-api-reference-generated-from-type-doc.md` to
  **0048** (0043–0047 are taken), updating `docs/decisions/README.md` and any
  citations; the audio ADR keeps 0042.
- **P21** (needs owner sign-off): slim `AGENTS.md`'s "Current state" (now
  ~430 lines) to a short per-milestone status list linking the roadmap spec,
  the archived change and the ADR, keeping the operational rules verbatim.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- None. Every pass is behavior-preserving, so no spec-level behavior changes.
  The change sets `skip_specs: true`.

## Impact

- **Code**: `src/platform/pipeline.c` (P15); `src/render/text.c`,
  `src/resource/gltf.c`, `src/api/api*.c`, `src/render/render*.c`,
  `src/render/skin.c`, `src/runtime/runtime.c` (P16); `src/input/*` and
  `CMakeLists.txt` includes (P19).
- **Tools/build**: `tools/lib/web-host.mjs` + the four runner scripts (P17);
  `CMakeLists.txt` vendor targets (P18).
- **Docs**: `docs/decisions/0048-api-reference-generated-from-type-doc.md` +
  `docs/decisions/README.md` (P20); `AGENTS.md` (P21);
  `docs/refactoring.md` status (all passes done, Checkpoint 3 reached). No
  `js-api` delta, no `efx.d.ts` change, no `docs/api/` regeneration, no golden
  re-baseline.
- **Verification**: P15 requires V5 through macOS (Metal/D3D11 flip and depth
  paths); P16 requires the matching goldens and `text_tests` /
  `resource_tests` / `render_tests` skin cases; P17 runs the web runners and
  gallery smoke locally; P19 requires V1 + V5; the whole change ends with V4
  and the four-target V5 gate.

## Non-goals

- **Any observable behavior change.** In particular P15/P16 preserve each
  function's error-return order and first failing check (the catalog stays
  byte-identical).
- **Changing the clip-depth remap** or any renderer invariant (ADR 0025).
- **The deferred behavior changes** in `docs/refactoring.md` §4 (coercion
  divergence, single-source validation, texture-slot reuse, free lists).
- **Forcing P21** without owner sign-off; if not signed off it is dropped from
  the change rather than guessed.
