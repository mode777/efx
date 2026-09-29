# Proposal

## Why

EmotionFX owns a C keyboard/mouse input layer (F9) but has no controller
support: `vision.md` names input as a product capability, F9 explicitly
deferred gamepads, and the Sokol snapshot the platform layer is built on
provides **no** joystick/gamepad API at all (`grep -i gamepad vendor/sokol/`
is empty; upstream issues #200/#393/#436 are open and the maintainer declines
to add it). Every gamepad backend is poll-based — the exact opposite of F9's
event-driven keyboard/mouse path — and each platform reports raw,
device-specific axis/button indices, so a normalized cross-platform surface
requires an SDL-format mapping layer.

This change adds gamepad support as a new **orthogonal input milestone F13**
(predecessor: F9). It vendors a pinned **minigamepad** snapshot (Zlib,
C89 single-header, all four targets, already carrying the SDL
game-controller database and GLFW-compatible GUID generation) behind the
engine's own poll seam, and exposes a single normalized button/axis surface
on `efx.gamepad`. Rumble is out of scope.

## What Changes

- **New orthogonal milestone F13 (gamepad input)** in the roadmap
  (predecessor gate: F9; independent of F3–F8 and F10–F12), verified by
  headless unit tests over the pure-C normalized model plus a script-level
  simulation harness on all four targets — no golden image.
- **Vendored `minigamepad` pinned snapshot** (`vendor/minigamepad/`) added to
  `vendor/README.md`'s dependency table: Zlib license, C89, single header,
  Windows/Linux/macOS/Emscripten. It supplies device enumeration, polling and
  the SDL-mapping evaluation on all four targets.
- **No Emscripten `-sASYNCIFY`.** Exploration proved the library itself uses
  only synchronous Emscripten gamepad calls; the README's `-s ASYNCIFY`
  requirement is inherited from the optional RGFW example (`RGFW.h` calls
  `emscripten_sleep(0)`, which is what actually needs Asyncify). The web
  player keeps its synchronous rAF bridge; a build with and without the flag
  is compared to confirm. This is not a deal breaker.
- **New pure-C gamepad model in `src/input/`** — fixed pad slots with
  level/press-edge/release-edge semantics matching F9, filled by a per-frame
  **poll at `efx_input_begin_frame`** (not an event callback), so gamepad
  state is deterministic and identical on desktop and web. The vendored
  library is confined to the backend layer; the normalized model and its
  injection seam stay dependency-free so the headless tests never touch a
  device.
- **Normalized script surface `efx.gamepad`** — a `Gamepad` view per connected
  slot exposing semantic button queries (`isDown`/`isPressed`/`isReleased` by
  engine-owned name: `'south'`, `'east'`, `'west'`, `'north'`, `'leftShoulder'`,
  `'rightShoulder'`, `'leftTrigger'`, `'rightTrigger'`, `'back'`, `'start'`,
  `'guide'`, `'leftStick'`, `'rightStick'`, `'dpadUp'`, `'dpadDown'`,
  `'dpadLeft'`, `'dpadRight'`) and normalized analog axes (`axis('leftX' |
  'leftY' | 'rightX' | 'rightY' | 'leftTrigger' | 'rightTrigger')`), plus
  `connected`, `name`, `onConnect`/`onDisconnect`, and a `count`. Raw fallback
  (`rawAxis(i)`, `rawButton(i)`) is exposed for unmapped pads.
- **SDL mapping table is authoritative.** The vendored database drives
  normalization. On web, pads reporting `mapping === 'standard'` are already
  canonical (identity); on desktop the SDL GUID/index layout is reconstructed.
- **Bugs found during evaluation are fixed in this change** (see below).
- **Docs**: `docs/js-api.md` gains the gamepad section; `gallery/src/api/
  efx.d.ts` gains the types; `AGENTS.md` roadmap table/current state gain
  F13; a **new ADR** records the durable decisions (vendored poll backend,
  SDL-mapping normalization, frame-begin polling, no Asyncify).

### Bugs to fix (found during evaluation of the vendored library)

These are known defects/gaps in the pinned snapshot that directly affect the
"normalized buttons and axes from the mapping table" requirement. Each is a
task in `tasks.md`; each becomes a requirement in the `gamepad` capability.

1. **Web right trigger never read.** The web axis map's sixth entry duplicates
   `MG_AXIS_LEFT_TRIGGER`, and the loop advances `j += 2` over the flat W3C
   axis array, so axis 5 (right trigger) is never sampled.
2. **Hats/d-pad mapping is unfinished** (the project's own `TODO` lists
   "hats"). `HATBIT` mapping elements are parsed but never resolved against
   device data; desktop d-pad arrives as hat axes (Linux) or buttons
   (XInput/web), so SDL `dpup:h0.1`-style entries do not reach the semantic
   d-pad buttons.
3. **`axisScale`/`axisOffset` are parsed but never applied.** SDL `+aN`/`-aN`
   half-axis and `~` inversion modifiers therefore produce wrong values.
4. **Web initial enumeration is missing.** The web backend registers
   connect/disconnect callbacks but never enumerates pads already connected
   at load, so a controller present before the page loads is not discovered
   until a button press.
5. **Output ranges are not standardized** (the project's `TODO`: "standardize
   output values"). Trigger and axis ranges must be pinned to one canonical
   contract in our normalized model.
6. **Windows GUID matching is XInput-first.** `updateGamepadGUID` matches the
   raw-input `PIDVID` marker, but the primary Windows path is XInput, so
   non-Xbox pads can miss their SDL entry.

Non-blocking notes recorded for the implementer: the snapshot's header text
says "libpng license" while the repository is Zlib (both permissive); the
mapping database is a fixed `mappings[1300]` array filled per-platform via
`#ifdef`; `MG_IMPLEMENTATION` must live in exactly one translation unit.

## Capabilities

### New Capabilities

- `gamepad`: the engine gamepad layer — the vendored poll backend contract,
  the fixed pad-slot state model with frame-begin polling and F9-style edges,
  SDL-mapping normalization to semantic buttons/axes, hot-plug handling, raw
  fallback for unmapped devices, the fixed limits, the deterministic backend
  seam, and the specific evaluated-library fixes (right trigger, hats/d-pad,
  axis scale/offset, web enumeration, canonical ranges, Windows GUIDs).

### Modified Capabilities

- `input`: the C-owned frame-staged input model gains a gamepad state section
  updated by a per-frame poll at `begin_frame` rather than by an event
  callback, while preserving the "queries and callbacks share one source"
  and injection-seam guarantees.
- `js-api`: adds the `efx.gamepad` namespace and the `Gamepad` view, and
  extends the reference's milestone range and catalog with gamepad input.
- `feature-roadmap`: declares **F13 (gamepad input)** as a new orthogonal
  milestone (predecessor F9) with its scope and non-visual verification gate.

## Impact

- **Core**: new gamepad state in `src/input/` (normalized slots + edges +
  injection seam); `src/platform/platform.c` initializes the backend and polls
  at frame begin; both bindings marshal the same model.
- **Vendored**: `vendor/minigamepad/` (pinned copy of `minigamepad.h`), a new
  row in `vendor/README.md`; the implementation TU is compiled into
  `efx_platform` (never into the pure-C test core).
- **Bindings**: `src/api/api.c` + `src/runtime/runtime.c` (desktop quickjs)
  and `src/web/bridge.c` + `src/web/entry.js` (web) expose `efx.gamepad` with
  identical semantics/errors.
- **Docs**: `docs/js-api.md`, `gallery/src/api/efx.d.ts`, `AGENTS.md`, and a
  **new ADR `docs/decisions/0041-gamepad-input.md`** (next free number — 0039
  and 0040 are claimed by the in-flight F11/F12 changes; re-check at apply
  time).
- **Tests**: headless unit tests over the pure-C gamepad model and the
  mapping evaluator using synthetic device data (fixed GUID + raw index
  layouts), plus a portable script-level harness on all four targets; no
  golden image.
- **Dependencies**: one new third-party dependency (minigamepad, Zlib). The
  dependency evaluation is recorded in `design.md` per the roadmap's
  third-party-dependency requirement; no other dependency is added.

## Roadmap position

This change implements **F13**, a new **orthogonal** milestone whose only
predecessor is **F9 (input)** — the C-owned frame-staged input core it
extends; F9's gate is green. It does not depend on F3–F8 and may land
independently of F10–F12. The `feature-roadmap` delta is written as a
**superset** that preserves the concurrently-proposed F11 (particles) and F12
(collision + character + impulse dynamics) milestones so archiving this
change does not drop them; the F11/F12/F13 deltas all rewrite the same two
requirements, so the archive step must reconcile them into one text (per the
repo's existing archive/sync flow). An ADR is required (new
`docs/decisions/0041-gamepad-input.md`): the vendored poll backend, the
SDL-mapping normalization boundary, frame-begin polling, and the no-Asyncify
web constraint are durable decisions future changes must respect.

## Non-goals

- **Rumble / haptics / LEDs / battery** — no vibration in this change.
- **Touch, pen, motion sensors, steering wheels, flight sticks** as
  first-class devices — a non-standard pad is usable only through its SDL
  entry or the raw fallback.
- **A script-facing input-mapping/rebinding layer** (actions, dead-zone
  curves, button remap UI) — scripts map normalized inputs themselves.
- **A native-backed gamepad resource or slot-creation API** — pads are a
  fixed engine-owned bank reported by index; scripts never create or destroy
  one.
- **Per-device custom mapping registration from scripts** — the bundled SDL
  table plus the raw fallback is the v1 surface.
- **Changing the keyboard/mouse API or semantics** — F9 behavior is
  untouched.
- **Vendoring or shipping the RGFW example** — only the library header is
  vendored.
