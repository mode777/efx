# Tasks

## 1. Prerequisites and vendoring

- [x] 1.1 Confirm F9's verification gate is green (F13's only predecessor) and that the `feature-roadmap` delta declaring F13 orthogonal (and preserving F11/F12 as a superset) is coherent; verify by reading the archived F9 change and the current roadmap spec
- [x] 1.2 Add the pinned `minigamepad` snapshot under `vendor/minigamepad/` (header + upstream license text) and record its row in `vendor/README.md` (project, revision, source, Zlib, chosen-over alternatives); verify a configure/build with the header present and no network succeeds
- [x] 1.3 Decide and document where the defect fixes live (patched vendored header vs. a shim TU) and record the pinned revision plus any local patch in `vendor/README.md`; verify the recorded note matches the shipped file

## 2. Backend integration

- [x] 2.1 Add a backend implementation TU that includes the vendored header exactly once (one `MG_IMPLEMENTATION`, deliberate `MG_MAX_GAMEPADS`) and is compiled into `efx_platform` only; verify the pure-C core and the headless test targets do not link it
- [x] 2.2 Initialize the backend in the platform `init_cb` and poll it via a single `efx_input_gamepad_poll()` call at the top of `efx_input_begin_frame()`; verify a native windowed run on the verification server sees pad state update once per frame
- [x] 2.3 Verify the Emscripten build needs no `-sASYNCIFY`: build the web target with and without the flag, run the browser harness, and confirm identical gamepad behavior and no new link option in CMake; record the result in the design doc
- [x] 2.4 Fix web initial enumeration (defect 1.4): enumerate pads already connected at init via the Emscripten gamepad API in addition to connect/disconnect callbacks; verify a synthetic page-load pad is discovered on the first frame in the browser harness

## 3. Normalized model and mapping evaluator

- [x] 3.1 Implement the pure-C fixed pad bank in `src/input/` (slot connect/disconnect, level state, press/release edges valid one frame, hot-plug) and the per-frame commit; verify headless unit tests for edges, hot-plug, and disconnect clearing
- [x] 3.2 Implement the portable SDL-mapping evaluator over a device-agnostic descriptor (GUID selection with permissive fallback, axis/button/hat element kinds, `+`/`-` half-axis ranges, `~` inversion, index mapping); verify headless unit tests with synthetic descriptors
- [x] 3.3 Pin the canonical axis ranges and the digital-trigger threshold (D6) and collapse evaluator output onto them; verify a unit test asserts identical normalized values for desktop-style and web-standard-style descriptors
- [x] 3.4 Add the raw fallback (`rawButton`/`rawAxis`) and the `mapped` flag for devices with no entry; verify a headless test with an unknown GUID reports connected+unmapped and readable raw values
- [x] 3.5 Extend the deterministic injection seam so tests can supply synthetic pad descriptors and state; verify a headless test drives a full connect → press → release → disconnect through the production path with no device

## 4. Evaluated-library defect fixes

- [x] 4.1 Fix the web right-trigger path (defect 1.1): correct the axis map/loop so axis 5 is sampled; verify a headless/browser test moves only the right trigger and observes `rightTrigger` change with `leftTrigger` unchanged
- [x] 4.2 Fix hat/d-pad resolution (defect 1.2): resolve hat mapping elements to the semantic d-pad for devices reporting the d-pad as hat axes or as buttons; verify synthetic-descriptor tests for both representations
- [x] 4.3 Apply mapping `axisScale`/`axisOffset` (defect 1.3): honor half-axis ranges and inversion in normalized values; verify a headless test with an inverted mapping asserts the mapped direction
- [x] 4.4 Fix Windows database matching (defect 1.6): ensure non-Xbox controllers reached through the primary path match their SDL entry (GUID normalization or DirectInput path); verify on the verification server with the available devices and a synthetic GUID test
- [x] 4.5 Regression-test the six defect fixes together and confirm none changes the semantic surface for already-correct devices; verify the full headless gamepad suite is green

## 5. Script bindings

- [x] 5.1 Register `efx.gamepad` in `src/api/api.c`/`src/runtime/runtime.c`: `count`, `get(index)`, the pad view (`connected`, `name`, `mapped`, `isDown`/`isPressed`/`isReleased`, `axis`, `rawButton`/`rawAxis`), and `onConnect`/`onDisconnect` returning unsubscribe functions (reusing the hook-list machinery); verify headless unit tests for the query/edge/validation/unsubscribe matrix
- [x] 5.2 Mirror `efx.gamepad` in `src/web/bridge.c` and `src/web/entry.js` with identical names, semantics, and errors; verify `tools/run_web_compare.mjs` diffs desktop vs web at zero for a gamepad script

## 6. Tests and integration harness

- [x] 6.1 Add headless unit tests for the `gamepad` capability (slot/edges, mapping selection, half-axis/inversion, hat d-pad, ranges/threshold, raw fallback, hot-plug, unknown-name `TypeError`, unsubscribe idempotence) to the ctest suite; verify they pass in an `EFX_HEADLESS=ON` local build
- [x] 6.2 Add a portable script-level simulation harness that injects a synthetic pad sequence and asserts script-observable results end to end; verify it runs green in ctest on the desktop build and on Emscripten via the bridge
- [x] 6.3 Add a cross-runtime compare case for the gamepad script; verify desktop and web outputs are identical

## 7. Docs and ADR

- [x] 7.1 Write ADR `docs/decisions/0041-gamepad-input.md` (next free number — re-check 0039/0040 at apply time) covering the vendored poll backend, SDL-mapping normalization boundary, frame-begin polling, fixed pad bank, canonical ranges, and the no-Asyncify web constraint, per `TEMPLATE.md`; add it to `docs/decisions/README.md` and verify the index row links the new file
- [x] 7.2 Add the F13 gamepad section to `docs/js-api.md` (provisional): `efx.gamepad`, the pad view, the semantic button/axis name sets, canonical ranges and trigger threshold, the raw fallback, error behavior, and the fixed pad limit; verify every signature matches the implementation
- [x] 7.3 Add the `efx.gamepad` types to `gallery/src/api/efx.d.ts`; verify the gallery build type-checks
- [x] 7.4 Update `AGENTS.md` (roadmap table gains F13 as orthogonal with its non-visual gate; current-state and script-API sections mention gamepad); verify the roadmap spec's "documented in AGENTS.md" requirement is satisfied

## 8. Verification gate

- [x] 8.1 Run the four-target gate in order per AGENTS.md: `python3 tools/verify_remote.py all <branch>` on the SSH verification server, then `gh workflow run ci.yml --ref <branch>`, confirming Linux, then Windows, then macOS green; note F13 has no golden-image gate — the gate is the headless gamepad unit tests plus the portable simulation harness on all four targets, and the with/without-Asyncify web check
