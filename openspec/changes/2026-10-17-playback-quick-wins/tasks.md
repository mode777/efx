# Tasks

## 1. Billboard run plan (render module)

- [ ] 1.1 Add `efx_billboard_run` (start, count, texture, blend, depth_test) and `efx_render_billboard_runs()` beside `efx_render_runs` in `src/render/render_records.c` (+ declarations in `src/render/render.h`); unit-test the plan in `tests/unit/render_tests.c`: record-adjacency builds one run, differences in texture/blend/depth_test split runs, intervening quad/mesh/particle/target records split runs, empty and single-record frames. Verify: `ctest` unit suite green.

## 2. Playback batching (pipeline sink)

- [ ] 2.1 Add the `bridge` parameter to `emit_billboard` (duplicate last/first vertex when set, the `emit_quad`/`append_strip_quad` pattern); switch billboard emission in `emit_billboards_and_particles` to per-run scratch reservation (`4 + (n-1)·6` verts) and stop filling `boff/bcnt` for billboard records (particles keep them). Verify: unit suite green; scratch-growth path exercised by a many-billboard unit scene.
- [ ] 2.2 Consume the run plan in `play_records`: at a run's first record, one `draw_textured` over the run's vertex span, skip ahead as quad runs do; pipeline chosen per run in the next task. Verify: `ctest -E golden` green on a headless build.
- [ ] 2.3 Verify output-identical batching: build with goldens ON and run the existing billboard/particle golden scenes — they must be pixel-identical (batching changes draw granularity only). Verify: `python3 tools/verify_remote.py native <branch>` golden jobs pass unchanged.

## 3. Honor `depthTest: false`

- [ ] 3.1 In `play_records`, select the billboard pipeline from the run key: `depth_test ? bill_pip[blend] : quad_pip[blend]`, with a comment recording the coupling (quad_pip is today's depth-always twin; revisit if quad depth state ever diverges). Verify: unit test asserting the recorded flag drives playback selection (or plan-level assertion in 1.1's harness); existing goldens unchanged.
- [ ] 3.2 Add a golden scene with a `depthTest: false` billboard placed behind opaque geometry next to a `depthTest: true` control; capture server-side per `docs/verification-server.md` (push without PNG → capture → SFTP → commit), then confirm `run_web_compare.mjs` parity. Verify: `golden_<scene>` passes on native and web.

## 4. Unlit fetch reorder (single canned shader)

- [ ] 4.1 Reorder `shaders/mesh.glsl` `mesh_fs`: alpha-mask sample → discard → diffuse sample → `unlit` early-out → ambient/specular/emissive samples → lighting; regenerate committed `shaders/mesh.h` with the pinned sokol-shdc (`cmake -B build -DEFX_SHDC=...`). Verify: diff review confirms no math change; unit CPU-reference tests (render/skin/lit-mesh tests) stay green.
- [ ] 4.2 Verify pixel-identical rendering: run the lighting/unlit/skybox golden scenes — all bit-identical to pre-change captures. Verify: `python3 tools/verify_remote.py native <branch>`.

## 5. Gate and merge

- [ ] 5.1 Local headless sweep: fresh `cmake -B build -DEFX_HEADLESS=ON` configure + `ctest -E golden` fully green. Verify: clean local ctest log.
- [ ] 5.2 Commit the series, push the branch, run `python3 tools/verify_remote.py all <branch>` and iterate until green (both golden-bearing jobs). Verify: verify_remote exits 0.
- [ ] 5.3 Dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`), iterate Linux → Windows → macOS; on green, merge `--no-ff` per the repo lifecycle and let Pages redeploy. Verify: gate run green on all targets.
