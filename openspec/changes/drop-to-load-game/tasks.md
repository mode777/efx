# Tasks

## 1. Desktop drop detection

- [ ] 1.1 Set `sapp_desc.enable_dragndrop` (with a bounded `max_dropped_files`) in `efx_platform_run` and handle `SAPP_EVENTTYPE_FILES_DROPPED` in `efx_event_cb`, exposing the dropped path(s) to the player; verify a native build receives the event and logs the path when a fixture is dropped (manual, display-capable build).
- [x] 1.2 Add a desktop validation helper that accepts a dropped path only when `efx_resource_open` succeeds and `main.js` reads from it, covering both a zip and a directory; verify with unit-level checks over `tests/fixtures/root_zip.zip` and a directory fixture.
- [ ] 1.3 Choose and enforce the accepted-archive byte-size cap from the largest curated pack, and verify an oversized drop is rejected with a diagnostic (design Open Question 1).

## 2. Desktop relaunch

- [ ] 2.1 Resolve the current executable path per OS (`/proc/self/exe`, `_NSGetExecutablePath`, `GetModuleFileNameW`) and spawn a detached new player bound to the dropped root (POSIX `fork`+`execv`, Windows `CreateProcess`), then stop the current run; verify dropping `tests/fixtures/root_zip.zip` starts a new run whose entry loads (`zip-entry-ok`) and the old run exits 0 (design Open Question 2).
- [ ] 2.2 Ensure an invalid desktop drop prints a diagnostic and leaves the current run running; verify by dropping a non-archive file and a `main.js`-less directory and observing the run continues.

## 3. Web drop and reload

- [ ] 3.1 Add a bridge helper that opens an archive at a scratch MEMFS path, checks for `main.js`, and closes it without disturbing the active root; verify it returns success for a staged fixture zip and failure for a corrupt file.
- [ ] 3.2 Install `dragover`/`drop` listeners on the canvas in the boot JS: prevent the browser default, validate the dropped archive via 3.1, stash the bytes in IndexedDB under a one-shot token, and reload with the token; on boot, read/consume the token, mount via `efx_bridge_set_root`, and continue entry evaluation; verify the existing Node asset-root harness pattern with a browser test that drops a fixture zip and observes the new sample's output.
- [ ] 3.3 Surface an invalid or un-storable web drop through the standard error channel without reloading, and verify the currently running sample keeps running.
- [ ] 3.4 Wire the gallery runner (`gallery/public/runner.html`) so a drop on the application area is forwarded to the embed and the browser default is suppressed; verify in the browser harness that a dropped archive replaces the running sample and the shell stays up.

## 4. Verification

- [ ] 4.1 Add native smoke coverage for the drop path in the display-capable ctest build (a player test that triggers the drop helper/relaunch with a fixture zip and a fixture directory, expecting the dropped entry's output) and confirm it passes via `ctest -R drop`.
- [ ] 4.2 Extend the browser/gallery harness to cover a dropped archive and an unusable drop, and confirm it fails when the drop path regresses.
- [ ] 4.3 Run `python3 tools/verify_remote.py all <branch>` green, then dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) and confirm Linux/Windows/macOS/Emscripten pass (ADR 0020, ADR 0023).

## 5. Documentation

- [x] 5.1 Write ADR `docs/decisions/0056` recording that a dropped resource root is a host-level feature (no script API; desktop zip/folder, web zip) loaded by restarting the run, and add it to `docs/decisions/README.md`.
- [x] 5.2 Update the `AGENTS.md` run-modes/player pointer to note dropped-root loading and that `in-place-game-swap` will replace the restart mechanism; verify no script-facing API, `docs/js-api.md`, `efx.d.ts`, or `docs/api/` change is needed and state that in the ADR.
- [x] 5.3 Run `npx openspec validate "drop-to-load-game" --type change --strict` and confirm it passes.
