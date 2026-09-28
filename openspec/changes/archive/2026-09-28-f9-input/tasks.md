# Tasks

## 1. Prerequisite and scaffolding

- [x] 1.1 Confirm F2's verification gate is green (F9's only predecessor) and that the `feature-roadmap` delta declaring F9 orthogonal is coherent; verify by reading the archived F2 change and the current `AGENTS.md` state
- [x] 1.2 Create the `src/input/` module (pure C, no Sokol/quickjs includes) and wire it into the `efx_core` build; verify a headless configure/build succeeds

## 2. Input core

- [x] 2.1 Implement the fixed key/button state table, the engine-owned name lookup (key names and `left`/`right`/`middle`), and the level/press-edge/release-edge queries; verify a unit test resolves every documented name and that an unknown name fails the lookup
- [x] 2.2 Implement the event queue and frame staging (`begin_frame`/`end_frame`): level state on arrival, edges valid one frame, arrival-ordered drain, pointer/wheel per-frame accumulation, and focus-loss clearing without synthetic up events; verify unit tests cover edge expiry, ordering, per-frame deltas, and focus clearing
- [x] 2.3 Implement the deterministic injection seam (`efx_input_inject_*` synthetic key/char/mouse events) used only by tests; verify a headless unit test injects events with no window and observes the same state as the platform path

## 3. Platform capture

- [x] 3.1 Install `sapp_desc.event_cb` in `src/platform/platform.c` and translate `sapp_event` (KEY_DOWN/UP, CHAR, MOUSE_DOWN/UP/MOVE/SCROLL, FOCUSED/UNFOCUSED) into input-core calls; verify the native player builds and a windowed smoke run on the verification server shows the core receiving events
- [x] 3.2 Suppress browser default actions for game keys at the platform layer on Emscripten (so space/arrows do not scroll the embedding page); verify the web build runs in the browser harness without page scroll on key input

## 4. Script bindings

- [x] 4.1 Register `efx.keyboard`, `efx.mouse`, and `efx.window` in `src/api/api.c`/`src/runtime/runtime.c`: query functions, read-only properties, `on*` registrations returning unsubscribe functions (reusing the hook-list machinery), with `TypeError` on non-function registration and unknown key/button names; verify headless unit tests for the full validation and unsubscribe matrix
- [x] 4.2 Drain staged input events before the update hooks in the desktop player and REPL frame dispatch (`src/player/player.c`, `src/player/repl.c`); verify a unit test asserts callback-before-update ordering within a frame
- [x] 4.3 Mirror the namespace in `src/web/bridge.c` and `src/web/entry.js` (bridge query functions, bridge-retained callback lists, `__efxDispatchInput` before `efx_web_call_hook_js(1, dt)`); verify `tools/run_web_compare.mjs` diffs desktop vs web at zero for an input script and the Emscripten ctest suite runs the portable input scripts green

## 5. Tests and integration harness

- [x] 5.1 Add headless unit tests for the `input` capability (query level/edge semantics, auto-repeat vs press edge, char events, mouse buttons/position/delta/wheel, coordinate space, unknown-name `TypeError`, unsubscribe idempotence) to the ctest suite; verify they pass in an `EFX_HEADLESS=ON` local build
- [x] 5.2 Add a script-level simulation harness that injects a synthetic input sequence and asserts script-observable results end to end; verify it runs green in ctest on the desktop build and on Emscripten via the bridge

## 6. Docs and ADR

- [x] 6.1 Write ADR `docs/decisions/0036` (input state is C-owned and frame-staged; scripts get query functions plus unsubscribe-returning event callbacks; module-wall core fed by the platform event callback) and add it to `docs/decisions/README.md`; verify the index row links the new file
- [x] 6.2 Add the F9 input section to `docs/js-api.md` (provisional): the three namespaces, signatures, the key/button name set, the surface-pixel coordinate and high-DPI rules, event-object shapes, and error behavior; verify every signature matches the implementation
- [x] 6.3 Add the namespaced types and key/button string unions to `gallery/src/api/efx.d.ts`; verify the gallery build type-checks
- [x] 6.4 Update `AGENTS.md` (roadmap table gains F9 as orthogonal with its non-visual gate; current-state and script-API sections mention input); verify the roadmap spec's "documented in AGENTS.md" requirement is satisfied

## 7. Verification gate

- [x] 7.1 Run the four-target gate in order per AGENTS.md: `python3 tools/verify_remote.py all <branch>` on the SSH verification server, then `gh workflow run ci.yml --ref <branch>`, confirming Linux, then Windows, then macOS green; note F9 has no golden-image gate — the gate is the headless unit tests plus the simulation harness on all four targets
