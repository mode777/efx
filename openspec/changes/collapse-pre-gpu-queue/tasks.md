# Tasks

## 1. Collapse the deferred render queue

- [x] 1.1 `src/render/render_texture.c`: delete `flush_pending_uploads` and the `pending`-buffer logic; in `efx_render_texture_create`'s no-sink branch create a live slot with `native = NULL` and no pixel copy. `src/render/render_internal.h`: remove the `tex_slot.pending` field. `src/render/render_records.c`: make `efx_render_install_sink` a plain sink assignment and stop freeing pending buffers in `efx_render_shutdown`. Verify: `cmake -B build -DEFX_HEADLESS=ON && cmake --build build -j4` compiles clean and `ctest --test-dir build -R render_tests` passes (after task 3.1).
- [x] 1.2 `src/render/render_mesh.c`: remove the no-sink upload-deferral branch but keep building and retaining `mesh_pending` (skinned bind data + F12 colliders). `src/render/render_target.c`: remove the no-sink deferral so a target is simply created with no native object. Verify: headless build compiles and the mesh/target render tests pass.

## 2. Make whiteTexture and error paths uniform

- [x] 2.1 `src/render/render_texture.c` `efx_render_white_texture`: drop the `if (!R.sink || !R.sink->create_texture) return 0;` guard so it uses the normal creation path and marks the slot permanent. `src/api/api_2d.c` (`efx_js_whiteTexture`) and `src/web/js/audio.js` (the `whiteTexture` getter): keep the "white texture unavailable" error only as a genuine-creation-failure path. Verify: the no-sink probe (headless core, no mock sink) reads `efx.graphics.whiteTexture` successfully at 1×1 and `destroy()` throws `TypeError`.

## 3. Tests and samples

- [x] 3.1 `tests/unit/render_tests.c`: rewrite `texture_queued_handles` and `mesh_pending_upload` to assert no-sink creation succeeds as CPU-only (live, correct size, no native) instead of asserting the flush transition; update the `white after shutdown` assertion to expect a valid CPU-only white resource. Verify: `ctest --test-dir build -R render_tests` passes.
- [x] 3.2 `tests/unit/api_tests.c`: adjust any case that asserts `whiteTexture` is unavailable; add a case proving it is available and drawable in the surface-less path. Verify: `ctest --test-dir build -R api_tests` passes.
- [x] 3.3 `tests/scripts/s_2d_validation.js` and `tests/scripts/s_5a_validation.js`: update the "white texture needs a window" comments and use `efx.graphics.whiteTexture` in a `--script` draw. Verify: `ctest --test-dir build -R smoke_2d_validation` and `-R smoke_5a_validation` pass, and the web cross-runtime compare (`node tools/run_web_compare.mjs`) produces identical output.
- [x] 3.4 `gallery/samples/curated/audio-showcase/main.js`: remove the lazy `white()` wrapper and its comment; use `efx.graphics.whiteTexture` directly. Verify: `ctest -R smoke_showcase_audio` passes and the gallery sample still builds.
- [x] 3.5 `tests/catalog_runner.c`: remove its mock sink and confirm `tests/scripts/s_error_catalog.expected.txt` is byte-identical (the catalog now runs the real surface-less path); if the catalog depends on a sink, keep the mock and record why. Verify: `ctest -R smoke_error_catalog` passes with unchanged expected output.

## 4. Documentation and decision record

- [x] 4.1 Write **ADR 0052** (`docs/decisions/0052-surface-less-resources-are-cpu-only.md` per `TEMPLATE.md`): surface-less run modes create CPU-only resources and the engine has a single creation path; note that ADR 0034's "deferred pre-GPU-surface queue path" mention is superseded. Add the row to `docs/decisions/README.md`. Verify: the ADR is indexed and reads coherently with ADR 0016's adopted reorder.
- [x] 4.2 Update the `AGENTS.md` current-state map to note surface-less resources are CPU-only and the pre-GPU queue is gone. Verify: no stale reference to the deferred upload queue remains in `AGENTS.md` or `docs/js-api.md`. (No `gallery/src/api/efx.d.ts` or `docs/api/` change — no API shape change.)

## 5. Verification gate

- [ ] 5.1 On the SSH verification server, run the Linux signal first: `python3 tools/verify_remote.py all <branch>` — native ctest including the full golden-image suite (byte-identical) and the surface-less script suite, plus the Emscripten golden suite and the cross-runtime compare. Fix anything it finds.
- [ ] 5.2 Only after Linux passes, dispatch `gh workflow run ci.yml --ref <branch>` and confirm Linux, then Windows, then macOS green in order (ADR 0020/0023). Verify: the workflow run is green on all four targets with goldens unchanged.
