# Design

## Context

F9 delivered a C-owned, frame-staged keyboard/mouse input core
(`src/input/efx_input.{c,h}`) fed by the single Sokol `event_cb` in
`src/platform/platform.c`, with a deterministic injection seam and a
non-visual gate (ADR 0036). Both bindings (`src/runtime/runtime.c`,
`src/web/bridge.c`) marshal the same core. See `proposal.md` — Why for the
motivation.

Three constraints shape the gamepad approach:

- **Sokol has no gamepad API.** `vendor/sokol/` is pinned at `2e75443` and
  exposes no joystick/gamepad functions; upstream issues #200/#393/#436 are
  open and the maintainer declines to add one to `sokol_app.h`. The platform
  layer must obtain device data from somewhere else.
- **Gamepads are poll-based, not event-based.** Keyboard/mouse arrive as
  events; a gamepad's state is read each tick (`navigator.getGamepads()`,
  evdev read, `XInputGetState`). The F9 model has no poll path yet.
- **Raw indices are not portable.** The SDL game-controller database
  (`gamecontrollerdb.txt`, Zlib, ~2290 lines) is authored against SDL's
  canonical per-platform joystick index layout and GUID format. GLFW can
  consume it only because it deliberately reproduces that layout and GUID
  generation per OS (`src/mappings.h`, `linux_joystick.c:174`). A backend
  that reports its own raw indices cannot feed the table directly.

## Goals / Non-Goals

**Goals:**

- One normalized, device-independent gamepad surface on all four targets,
  driven by the SDL mapping table, with the same frame-staged edge semantics
  as F9.
- The vendored backend fully behind the engine interface; the normalized
  model and its tests depend on neither the backend nor a device.
- Keep the web build synchronous — no Emscripten Asyncify.
- Correct the known defects in the vendored snapshot that affect the
  normalized surface.

**Non-Goals:**

- Rumble/haptics, motion/gyro, steering wheels, custom per-device mapping
  registration, script-side rebinding, a gamepad resource class.
- Rewriting F9's keyboard/mouse path or changing its semantics.

## Decisions

**D1 — Vendor `minigamepad` (pinned snapshot).** The third-party dependency
evaluation required by `feature-roadmap`:

| Candidate | License | Vendoring fit | 4-target (incl. Emscripten) | C11 fit | Verdict |
|---|---|---|---|---|---|
| **minigamepad** | Zlib | single header, pinned snapshot | **yes** (Win/Linux/macOS/Emscripten) | C89 header, compiles under C11 | **chosen** |
| libstem_gamepad | Zlib | multi-file, per-OS | **no web**; raw only | C | rejected |
| GLFW joystick layer | Zlib | large, couples to GLFW platform struct | no web | C | rejected |
| SDL2/SDL3 | Zlib | very large, conflicts with Sokol windowing | yes | C | rejected |
| Own per-OS backend | n/a | none | would need 4 backends | C | rejected for v1 |

minigamepad is `libstem_gamepad` lineage (same `Gamepad_device` shape, same
Zlib license) extended with an SDL-mapping evaluator, GLFW-compatible GUID
generation, and an Emscripten backend — i.e. the architecture this change
wanted, already assembled. It is alpha, so upstream is treated as a pinned
snapshot we may patch; no upstream support is assumed.
*Rejected: libstem + own SDL shim* — libstem exposes neither the SDL GUID
(bus/version are read but discarded) nor SDL's index layout, so the shim
would be GLFW-port-sized per desktop OS.
*Rejected: SDL/GLFW* — a second windowing/input system alongside Sokol, plus
a large binary cost against the project's small-player goal.

**D2 — Poll at frame begin; backend confined to the platform layer.** New
gamepad state lives in `src/input/` next to the keyboard/mouse model. The
platform layer initializes the backend once (in `sapp_desc.init_cb`) and calls
a single `efx_input_gamepad_poll()` at the top of `efx_input_begin_frame()`
before edges are finalized. This mirrors F9's "commit at frame begin" for mouse
accumulators. The vendored header is included only from the platform
implementation TU, never from the pure-C core or the tests. The core receives
normalized device snapshots (or synthetic ones) through one ingestion
function, so it is backend-agnostic.

*Rejected: feed gamepads through `sapp_desc.event_cb`* — there is no gamepad
event on native, and on web the connect/disconnect events do not carry the
per-frame axis/button state, so a poll is required regardless.

**D3 — SDL mapping is the normalization boundary; a raw fallback always
exists.** On desktop, the backend yields a device descriptor (name, SDL GUID,
raw axes/buttons); the mapping evaluator selects a database entry by GUID
(permissive fallback) and resolves each semantic button/axis to a raw element
(axis, button, or hat bit) applying element kind, `+`/`-` half-axis range, and
`~` inversion. On web, a pad with `mapping === 'standard'` normalizes directly
by the standard indices (identity); a non-standard pad goes through the raw
fallback. A device with no entry is still reported connected and `mapped:
false` with raw access. The evaluator is portable C over a device-agnostic
descriptor, so it is unit-tested with synthetic GUIDs/layouts.

**D4 — No Asyncify.** The vendored library's web path uses only synchronous
Emscripten calls (`emscripten_sample_gamepad_data`, `emscripten_get_gamepad_status`,
`emscripten_set_gamepad{connected,disconnected}_callback`), all marked
`__proxy: 'sync'` in Emscripten's `libhtml5.js`. The README's `-s ASYNCIFY`
comes from the optional RGFW example (`RGFW.h:13239` calls
`emscripten_sleep(0)`, the actual Asyncify trigger). The engine polls from
its existing rAF frame callback, so `ASYNCIFY` stays out of the CMake link
options. The build is verified with and without the flag; the wasm behavior
must match.

**D5 — Fixed pad bank, no resource class.** Like lights, gamepads are a fixed
engine-owned bank reported by index; scripts do not create, destroy, or own a
pad. The bank size (proposed 4, matching `XUSER_MAX_COUNT` and the browser's
typical cap) is a documented fixed limit. This keeps the native-backed class
list, the resource taxonomy, and the `destroy()` contract unchanged
(ADR 0011/0013).

**D6 — Canonical ranges and trigger threshold.** Axes are normalized to a
documented range (sticks −1..1; triggers 0..1), and each trigger's digital
button derives from its axis via a documented threshold (a value comparable to
the GLFW/SDL convention, e.g. ≥ 0.5). The mapping evaluator's ranges are
collapsed onto this contract, so desktop and web report the same values for
the same physical position.

**D7 — Correct the evaluated-library defects (see proposal.md).** Concretely:

| # | Defect | Fix location |
|---|---|---|
| 1 | web axis map duplicates left trigger / `j += 2` skips the right trigger | vendored web backend or our shim |
| 2 | hat/d-pad elements parsed but not resolved; d-pad arrives as axes/buttons | our mapping evaluator / vendored snapshot |
| 3 | `axisScale`/`axisOffset` parsed but never applied | our evaluator (apply them) |
| 4 | web does not enumerate pads present before init | our backend init (enumerate, then rely on events) |
| 5 | axis/trigger ranges unstandardized | D6 |
| 6 | Windows GUID matching is XInput/raw-input specific | vendored Windows GUID step |

Fixes live in the vendored snapshot (patched in place, documented as local
divergence) or in our thin shim, whichever is smaller; either way the vendor
table records the pinned revision and the local patch. Each fix carries a
headless test using synthetic descriptors.

**D8 — Testing and gate.** The pure-C model + evaluator are tested headlessly
with synthetic device descriptors (fixed GUID → known mapping, raw axis/button
fixtures) across mapping, edges, hot-plug, range/threshold, and the defect
cases. A portable script harness injects synthetic pads and asserts the
script-visible result through both runtimes (ctest + cross-runtime compare).
No golden image. Real-device behavior (macOS IOKit runloop coexistence with
Sokol's Cocoa runloop, Windows XInput/DirectInput) is verified on the
verification server and, where possible, by hand.

**D9 — Roadmap/ADR collision handling.** F11, F12, and F13 are in flight and
all rewrite the same `feature-roadmap` and `js-api` reference requirements.
This change's `feature-roadmap` delta is written as a **superset** (F9–F13) so
archiving it does not drop F11/F12; the archive/sync step must still reconcile
the three deltas into one text. ADR number 0041 is proposed (0039/0040 claimed
by F11/F12); re-check the next free number at apply time.

## Risks / Trade-offs

- [Vendored library is alpha and has known gaps] → treat it as a pinned
  snapshot we may patch; the defect list is explicit and each fix is a task
  with a test; the SDL evaluator and normalized model are ours to control.
- [macOS backend defers to the IOKit runloop] → sokol_app already runs the
  Cocoa runloop; verify coexistence on the server and document if a manual
  pump is needed.
- [Windows primary path is XInput (Xbox-only)] → verify non-Xbox pads match
  their SDL entry; fall back to the DirectInput path if needed.
- [Browser pads require focus / a user gesture in some browsers] → document
  the focus requirement (as F9 did) and cover it in the browser harness; a
  click-to-focus affordance is a gallery concern.
- [Vendored header defines a global mapping table and `MG_IMPLEMENTATION`
  must be one TU] → isolate the implementation in the platform TU and define
  `MG_MAX_GAMEPADS`/`MG_C89` deliberately.
- [Roadmap/reference deltas collide with F11/F12] → superset text here, and
  reconcile at archive.

## Migration Plan

Additive: no existing API or behavior changes. When no gamepad backend
initializes or no pad is present, the model reports `count === 0` and the
poll is a no-op. Rollback removes the vendored snapshot, the gamepad section,
and the namespace entries; no data or format migration exists.

## Open Questions

- The exact pad-bank cap (proposed 4) and the trigger threshold constant —
  fixed during implementation and pinned by the unit tests; both are
  documented, not spec-changing.
- Whether the defect fixes land as local patches to the vendored header or in
  a separate shim TU — an internal choice.
- Whether a curated gallery gamepad sample ships with this change or a
  follow-up (the gate needs only the script harness, not gallery content).
