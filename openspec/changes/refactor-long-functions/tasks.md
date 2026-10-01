# Tasks

## 1. Decompose `efx_pipeline_play` (P15)

- [x] 1.1 Extract `ensure_scratch(quad_count)` and `grow_int_pair(...)`;
  verify the native build and all quad/billboard/particle goldens pass
- [x] 1.2 Extract `emit_quad_runs(...)`, `emit_billboards_and_particles(...)`
  (with `sort_particles_back_to_front`) and `draw_textured(pip, handle, first,
  count)`, leaving the clip-depth fold in `play_mesh_record` untouched; verify
  the billboard/particle/target goldens pass
- [x] 1.3 Extract `upload_vertices(total)` and `play_records(...)`; verify
  `efx_pipeline_play` is under ~60 lines of orchestration and the `error_catalog`
  is byte-identical
- [ ] 1.4 Verify the render/particle goldens on Linux (V4) and dispatch V5
  through **macOS** (Metal/D3D11 flip and depth paths); confirm green

## 2. Decompose the remaining long functions (P16)

- [x] 2.1 (text.c) Split `efx_text_font_create` into glyph-set resolution →
  pack → rasterize (+ outline/shadow) → atlas build; verify the text goldens
  and `text_tests` pass and the catalog is byte-identical
- [x] 2.2 (gltf.c) Split `build_surface` / `efx_gltf_load_meshdata` into
  per-attribute accessor import, index import, material conversion, rig attach;
  verify `resource_tests` and the glTF/rig goldens pass
- [x] 2.3 (api) Split `efx_js_drawQuad`, `efx_js_drawBillboard`, `parse_sprite`,
  `efx_js_createFont`, `read_particle_config`, `efx_js_createMeshData`,
  `efx_js_createImageData` into named option-readers separate from recording;
  verify the catalog is byte-identical and `api_tests` pass
- [x] 2.4 (render.c/skin.c/runtime.c) Split `efx_render_mesh_create`,
  `efx_skin_evaluate`, `efx_runtime_new` into named steps; verify
  `render_tests` skin cases, `skin_pose` goldens and the smoke suite pass

## 3. Shared web test runner library (P17)

- [x] 3.1 Add `tools/lib/web-host.mjs` exporting `loadPuppeteer()`,
  `serveStatic(root, routes)` and `launchBrowser(opts)`; verify it loads under
  Node 20
- [ ] 3.2 Migrate `run_web_goldens.mjs` and `run_web_harness.mjs` onto it,
  keeping CLI/env/exit codes; verify both produce unchanged output on the
  verification server
- [ ] 3.3 Migrate `run_gallery_smoke.mjs` and the server half of
  `test_web_assets.mjs`; verify both run locally with unchanged output

## 4. Build and naming hygiene (P18–P19)

- [x] 4.1 (P18, optional) Add `efx_add_vendor_library(...)` and convert the
  miniz/cgltf/dr_libs targets; verify `compile_commands.json` is byte-identical
  before/after, else drop P18
- [x] 4.2 (P19) `git mv` `src/input/efx_input.{c,h}` → `input.{c,h}` and
  `efx_gamepad.{c,h}` → `gamepad.{c,h}`, updating includes and CMake; verify
  V1 and the V5 build on all four targets

## 5. Documentation hygiene (P20–P21)

- [x] 5.1 (P20) Rename `0042-api-reference-generated-from-type-doc.md` to
  `0048-api-reference-generated-from-type-doc.md`, update the
  `docs/decisions/README.md` index row and any citations; verify every index
  link resolves and a grep finds no stale "0042 — The API reference" citation
- [x] 5.2 (P21, owner-gated) **DROPPED — owner sign-off not obtained.** The
  `AGENTS.md` "Current state" section is left unchanged.
- [x] 5.3 Update `docs/refactoring.md` status: mark P15–P21 done (or P21
  dropped) and note Checkpoint 3 reached; verify no pass in Phase G–I is still
  pending

## 6. Verification

- [ ] 6.1 Run the local unit suites
  (`cmake -B build-h -DEFX_HEADLESS=ON && ctest --test-dir build-h`) and the
  generated-file checks (`python tools/gen_prelude.py --check`,
  `npm --prefix gallery run docs:check`); verify all green
- [ ] 6.2 Run `python3 tools/verify_remote.py all <branch>` (native ctest incl.
  all goldens, Emscripten ctest, web goldens, `run_web_compare.mjs`); verify
  green before dispatching CI
- [ ] 6.3 Dispatch `gh workflow run ci.yml --ref <branch>` and confirm the
  Linux → Windows → macOS gate is green; then merge to `main` and push per
  `AGENTS.md`
