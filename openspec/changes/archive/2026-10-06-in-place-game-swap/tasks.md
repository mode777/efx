# Tasks

## 1. Player session

- [x] 1.1 Introduce a player session struct owning the runtime, resource provider, and current root, and make `hooks.ud` the session so `efx_player_frame` reads `session->rt`; verify the existing desktop ctest suite still passes unchanged.
- [x] 1.2 Add a pending-swap root to the session and a frame-start swap step that runs before hook dispatch when a swap is pending (and is a no-op otherwise); verify a run with no drop renders and behaves exactly as before.

## 2. Render and pipeline reset

- [x] 2.1 Add `efx_render_reset()` that destroys registered textures/meshes/render targets/particles, frees the arenas, and re-applies default state while preserving the installed sink, and refactor `efx_render_shutdown` to share the teardown; verify with a unit test that resources are released, defaults re-applied, and the sink still set.
- [x] 2.2 Add `efx_pipeline_rebind()` that re-installs the render sink and re-derives `P.white_view` from a freshly created engine white texture without recreating shaders/pipelines; verify a 3D draw with an absent material map after a rebind samples the white view (smoke or golden).

## 3. Swap path

- [x] 3.1 Implement the swap order in the player: destroy the old runtime, `efx_render_reset` + `efx_pipeline_rebind`, clear input and stop all audio sources, close/open the resource root, create a new runtime, read and run the new `main.js`, and pick its hooks; verify dropping a second fixture zip swaps the running game in place with the window persisting and the new entry's output appearing.
- [x] 3.2 Validate the candidate root before any teardown (reuse the `drop-to-load-game` helper) and abandon the swap with a diagnostic when it lacks `main.js` or is unreadable; verify the current game keeps running.
- [x] 3.3 Coalesce/ignore a drop that arrives while a swap is in progress; verify a rapid double-drop does not corrupt state or crash.

## 4. Verification

- [x] 4.1 Add a native smoke that swaps between fixture roots several times in one run and asserts each new game draws and that no GPU resources accumulate (e.g., post-swap draws still render, context not lost); verify via `ctest -R swap`.
- [x] 4.2 Run `python3 tools/verify_remote.py all <branch>` green, then dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) and confirm Linux/Windows/macOS/Emscripten pass (ADR 0020, ADR 0023).

## 5. Documentation

- [x] 5.1 Write ADR `docs/decisions/0057` recording the desktop in-place session-swap lifecycle and the render-reset/pipeline-rebind split, add it to `docs/decisions/README.md`, and mark it as superseding the desktop restart mechanism noted in ADR 0056.
- [x] 5.2 Update the `AGENTS.md` run-modes/player pointer to describe the session swap and confirm no script-facing API, `docs/js-api.md`, `efx.d.ts`, or `docs/api/` change is needed (state this in the ADR).
- [x] 5.3 Run `npx openspec validate "in-place-game-swap" --type change --strict` and confirm it passes.
