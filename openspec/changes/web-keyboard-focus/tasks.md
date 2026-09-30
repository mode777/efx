# Tasks

## 1. Platform fix

- [ ] 1.1 In `src/platform/platform.c`, set `html5.bubble_mouse_events = true` on the `__EMSCRIPTEN__` target in `efx_platform_run` (leaving `bubble_key_events`/`bubble_wheel_events` at their defaults), and verify the web player target still builds (`source /opt/emsdk/emsdk_env.sh && emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web --target player_web -j4` on the verification server).
- [x] 1.2 Confirm game-key default suppression is unaffected (`efx_web_game_key` + `sapp_consume_event` still run in `efx_event_cb` and do not depend on the mouse flag), verified by reading the `sapp_desc` initialization and the event callback.

## 2. Regression coverage

- [ ] 2.1 Extend `tools/run_gallery_smoke.mjs` to load a small inline sample through the existing editor channel that records `efx.keyboard` state into a global (e.g. a key-down callback or a per-frame `isDown` poll), and verify the sample boots in the runner iframe with no console/page error.
- [ ] 2.2 In the smoke, click the center of the application area over the iframe and assert the runner's `document.hasFocus()` becomes true, failing the smoke otherwise (this isolates a focus failure from a key-mapping failure).
- [ ] 2.3 In the smoke, send `page.keyboard.down('KeyA')` and assert the running sample observed the key (the recorded global flips true), failing the smoke otherwise.
- [ ] 2.4 Prove the new check is meaningful by temporarily reverting task 1.1 and confirming the smoke fails on the keyboard assertion, then restoring the fix.

## 3. Decision record and docs

- [x] 3.1 Write `docs/decisions/0043-web-pointer-focus-default.md` per `docs/decisions/TEMPLATE.md` (on the web, pointer input must not suppress the focus default that keyboard delivery depends on; references ADR 0030 and ADR 0036) and add its row to `docs/decisions/README.md`; verify both files contain the entry. No ADR supersedes another.
- [x] 3.2 Update the F9 current-state bullet in `AGENTS.md` to note the web keyboard-focus fix; verify the note is present. (No `docs/js-api.md` or generated `docs/api/` change — the script-facing API is unchanged.)

## 4. Verification

- [ ] 4.1 Run the gallery suite on the verification server (`python3 tools/verify_remote.py gallery <branch>`) and confirm the new keyboard check passes.
- [ ] 4.2 Run the existing suites to confirm no regression — native `ctest` smoke + headless unit tests and the Emscripten `ctest` + cross-runtime compare — then dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) per ADR 0020/0023 and confirm all four targets are green.
