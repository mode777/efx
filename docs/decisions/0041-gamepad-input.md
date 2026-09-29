# 0041 — Gamepad input: vendored poll backend, pure-C normalized bank, frame-begin polling

Status: Accepted (2026-09, change `gamepad-input`)

Extends: ADR 0036 (C-owned frame-staged input; the gamepad section joins it)
Supports: ADR 0003 (module walls), ADR 0004 (single `efx` namespace),
ADR 0011/0013 (fixed banks vs. GC-finalized classes), ADR 0022 (desktop/web
binding parity), ADR 0023 (tag-triggered four-target gate).

## Context

F9 gave scripts a C-owned keyboard/mouse surface but no controller support,
and the pinned Sokol snapshot exposes no joystick/gamepad API. Unlike
keyboard/mouse, every supported gamepad API is **poll-based**
(`navigator.getGamepads()`, evdev, `XInputGetState`), and each platform
reports raw, device-specific axis/button indices. A normalized cross-platform
surface therefore needs both a poll backend and the SDL game-controller
mapping database. See `openspec/changes/gamepad-input/` for the full process
record and the defect list found in the candidate backend.

## Decision

- **Vendored poll backend, confined to the platform layer.** A pinned
  minigamepad snapshot (`vendor/minigamepad/`, Zlib) supplies device
  enumeration and polling on Windows/Linux/macOS/Emscripten. It is included
  exactly once, from `src/platform/gamepad_backend.c`, compiled into
  `efx_platform` only — never into the pure-C core or the headless tests. The
  backend is initialized in the platform `init_cb` and registered as the
  core's device source.
- **One pure-C normalized model (`src/input/efx_gamepad.{c,h}`).** A fixed
  bank of `EFX_GAMEPAD_MAX` (4) pad slots owns level state and one-frame
  press/release edges, matching ADR 0036. It receives device-agnostic
  descriptors (name, GUID, raw axes/buttons/hats) through a registered source
  or the test injection seam and is backend-agnostic.
- **SDL mapping is the normalization boundary.** A portable C evaluator
  selects a database entry by SDL GUID (with permissive fallback), resolves
  each semantic button/axis to an axis/button/hat element, and applies the
  mapping's `+`/`-` half-axis range and `~` inversion. A device with no entry
  is reported connected and `mapped: false` with raw `rawAxis`/`rawButton`
  access. The platform backend re-encodes its platform-mapped state into the
  canonical standard descriptor; the evaluator owns the semantic surface, so
  the vendored library's own (defective) evaluator is not on the hot path.
- **Frame-begin polling, not the event callback.** `efx_input_gamepad_poll()`
  runs at the top of `efx_input_begin_frame()`, before edges are finalized,
  and is idempotent within a frame. There is no gamepad event on native, and
  web connect/disconnect events do not carry per-frame state, so a poll is
  required regardless. This keeps the semantic surface deterministic and
  identical on desktop and web.
- **Canonical ranges and a pinned trigger threshold.** Sticks are −1..1,
  triggers 0..1, and each trigger's digital button derives from its axis at
  `EFX_GAMEPAD_TRIGGER_THRESHOLD` (0.5). The evaluator collapses every
  mapping onto this contract, so the same physical position reports the same
  value on every target.
- **No Emscripten `ASYNCIFY`.** The web path uses only synchronous Emscripten
  gamepad calls and is polled from the existing animation-frame driver; the
  README's `-s ASYNCIFY` comes from the optional RGFW example, not the
  library. The CMake link options stay unchanged and the build is verified
  with and without the flag.
- **No resource type.** Pads are a fixed engine-owned bank reported by index;
  scripts get `efx.gamepad.count`/`get(index)`, a pad view, and
  `onConnect`/`onDisconnect` returning unsubscribe functions. No `create`,
  no `destroy`, no new native-backed class (ADR 0011/0013 unchanged).

## Consequences

- The pure-C model and evaluator are tested headlessly with synthetic
  descriptors; the non-visual gate is unit tests plus a portable script case
  through both runtimes, with no golden image (per the roadmap).
- The vendored snapshot is treated as a pinned, patchable source. Its two
  web-path defects that affect our surface (right-trigger sampling, initial
  enumeration) are patched in place and recorded in `vendor/README.md`;
  re-pinning requires re-applying the marked hunks.
- Real-device behavior (macOS IOKit/runloop coexistence, Windows
  XInput/DirectInput, browser focus) is verified on the verification server
  and, where possible, by hand; the semantic surface is guaranteed by the
  pure-C evaluator and its tests.
- Gamepad polling must not change keyboard/mouse semantics or ordering; the
  gamepad section is orthogonal state committed at the same frame boundary.

## Rejected alternatives

- **Sokol event callback** — no native gamepad event exists, and web
  connect/disconnect events carry no per-frame axis/button state.
- **Using minigamepad's own mapping evaluator** — it parses but never applies
  `axisScale`/`axisOffset`, leaves hat elements unresolved, and its web axis
  map skipped the right trigger; owning the evaluator keeps the boundary
  testable and backend-agnostic.
- **libstem_gamepad** — no web backend and no SDL GUID/layout, so a
  GLFW-port-sized shim per desktop OS would be required.
- **GLFW joystick layer / SDL2/3** — a second windowing/input system
  alongside Sokol, plus a large binary cost against the small-player goal.
- **A bespoke four-platform backend** — four device backends to maintain for
  v1; deferred.
- **Asyncify** — would make the web bridge asynchronous and break the
  synchronous resource/module guarantees (ADR 0022/0031) for no benefit.
