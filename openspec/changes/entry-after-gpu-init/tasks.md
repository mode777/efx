# Tasks

## 1. Platform init hook

- [x] 1.1 `src/platform/platform.h`: add `int (*on_init)(void *ud)` to `efx_frame_hooks` (documented as "called after the rendering surface and engine subsystems are ready, before the first frame"). `src/platform/platform.c`: at the end of `efx_init_cb`, after `sg_setup`/`efx_pipeline_install`/`efx_gamepad_backend_init`/`efx_audio_backend_init`, call `efx_input_set_window(sapp_width(), sapp_height(), sapp_dpi_scale())` and then `if (g_hooks.on_init) g_hooks.on_init(g_hooks.ud)`. Initialize `on_init = NULL` in every hooks construction site (`player.c`, `repl.c`, `bridge_core.c`). Verify: `cmake -B build -DEFX_HEADLESS=ON && cmake --build build -j4` compiles clean.

## 2. Desktop evaluation move

- [x] 2.1 `src/player/player.c` `run_root_mode`: keep the early read of `main.js` (missing entry still fails before any window), keep `efx_runtime_new`/resource setup, but move `efx_player_run_entry` + `efx_runtime_pick_hooks` into a new `on_init` callback; keep the entry source alive until evaluation and free it inside the callback. A failed evaluation records the exit code and stops the loop through the existing stop path (`sapp_quit` from `init_cb`; macOS uses its non-returning exit path). Verify: `cmake --build build-disp --target player` on the verification server, then a windowed run of a resource root whose `main.js` reads `efx.graphics.whiteTexture` at top level exits 0.
- [x] 2.2 `src/player/repl.c` `efx_repl_run`: move the optional root-entry evaluation and `pick_hooks` into the same `on_init` pattern; the console loop is otherwise unchanged. Verify: `--repl <root>` with a root whose `main.js` reads a top-level GPU resource starts and accepts input on the verification server.
- [x] 2.3 Add a display-required player test (registered in `tests/CMakeLists.txt` alongside the other resource-root smoke tests) that runs a small root whose top-level code reads `efx.graphics.whiteTexture` and calls `efx.quit(0)`, asserting exit 0. Verify: `ctest -R <name>` passes on the verification server (it requires a display, so it is not part of the headless build).

## 3. Web boot inversion

- [x] 3.1 `src/web/bridge_core.c`: add an `EM_JS` trampoline that calls a JS entry-evaluation function (e.g. `globalThis.__efxEvaluateEntry()`), and a `web_on_init` callback that invokes it; set `hooks.on_init = web_on_init` in `efx_web_start_loop`. Verify: the Emscripten build (`source /opt/emsdk/emsdk_env.sh && emcmake cmake -B build-em … && cmake --build build-em`) compiles and links.
- [x] 3.2 `src/web/js/boot.js`: split `__efxEvaluateEntry` so it evaluates and registers hooks but no longer calls `_efx_web_start_loop`; in `__efxResolveAssets`, call `_efx_web_start_loop()` on DOM (evaluation now happens from the C init callback) and keep the direct evaluate + frame-guard path for the Node harness. Guard against double evaluation (`st.started`/`st.api`). Verify: the web Node harness (`node build-em/player.js <root>`) still evaluates its entry and exits with the same code as before.

## 4. Documentation and decision record

- [x] 4.1 Amend `docs/decisions/0016-explicit-hook-registration-implicit-init.md`: record that the readiness-before-eval reordering previously deferred is now adopted for surface-bearing modes, and that surface-less modes remain (queue removal tracked by the follow-up change); keep its `docs/decisions/README.md` index row accurate. Verify: the ADR reads coherently against this change and the index links it.
- [x] 4.2 Update the `AGENTS.md` current-state map / run-mode description if the entry-evaluation timing wording changes. Verify: no stale "evaluated before the window/GPU" claim remains in `AGENTS.md` or `docs/js-api.md` lifecycle text. (No `gallery/src/api/efx.d.ts` or `docs/api/` change — no script-facing API delta.)

## 5. Verification gate

- [ ] 5.1 On the SSH verification server, run the Linux signal first: `python3 tools/verify_remote.py all <branch>` — native ctest including the full golden-image suite (every committed PNG MUST match byte-identically) and the new display-required top-level-GPU test, plus the Emscripten golden suite and the cross-runtime compare. Fix anything it finds.
- [ ] 5.2 Only after Linux passes, dispatch the four-target gate `gh workflow run ci.yml --ref <branch>` and confirm Linux, then Windows, then macOS green in order (ADR 0020/0023). Verify: the workflow run is green on all four targets with goldens unchanged.
