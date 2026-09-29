# gamepad Specification

## Purpose
The engine's gamepad input layer: a vendored, cross-target poll backend that
reports device state, and a pure-C normalization model that turns
device-specific axes and buttons into one stable semantic surface through the
SDL game-controller mapping database.

## Requirements

### Requirement: Vendored poll backend with four-target parity

The engine SHALL obtain gamepad device data from a single vendored backend
(a pinned source snapshot) that covers Windows, Linux, macOS, and Emscripten,
and SHALL confine that backend to the platform layer behind an engine-owned
interface. The backend SHALL be initialized once when the platform starts and
polled once per frame; scripts SHALL NOT depend on the backend's types,
indices, or lifecycle. The backend's license, pinned revision, and source
origin SHALL be recorded in the vendor table, and the build SHALL never fetch
the dependency from the network.

#### Scenario: Device data is backend-agnostic to callers

- **WHEN** the gamepad model is consumed by a script binding or a test
- **THEN** it reads only engine-owned state and never a backend struct, index, or enum

#### Scenario: All four targets report devices

- **WHEN** a supported controller is connected on Windows, Linux, macOS, or in the browser
- **THEN** the engine reports it as a connected pad with a name, axes, and buttons on that target

#### Scenario: Build is offline

- **WHEN** the project is configured and built without network access
- **THEN** the gamepad backend resolves from the vendored snapshot

### Requirement: Fixed pad slots with frame-begin polling

Gamepad state SHALL be a fixed, engine-owned bank of pad slots. Device state
SHALL be sampled by a poll at frame begin — not delivered through the
keyboard/mouse event callback — because every supported gamepad API is
poll-based. Each slot SHALL expose the same three observable conditions as
the keyboard/mouse model: currently down (level), went down this frame (press
edge), and went up this frame (release edge), with edges valid for exactly
one frame and cleared at frame end. Trigger axes SHALL additionally be
reported as digital buttons using a documented threshold. Polling SHALL be
idempotent within a frame so queries and state updates cannot disagree.

#### Scenario: Press and release edges last one frame

- **WHEN** a pad button goes down in frame N and stays down
- **THEN** it reports as pressed in frame N, and as down-but-not-pressed in frame N+1, and reports a release edge only in the frame it goes up

#### Scenario: Poll happens at frame begin

- **WHEN** a frame runs with a pad whose button changed since the previous frame
- **THEN** the new state and its edges are visible to that frame's update hooks, and repeating the poll within the frame does not change the edges

#### Scenario: Unplugging leaves no stuck state

- **WHEN** a pad is disconnected while a button is held
- **THEN** the slot reports disconnected and no button or axis remains logically down

### Requirement: SDL-mapping normalization to a semantic surface

The engine SHALL normalize device-specific raw axes and buttons into a stable
semantic set using the SDL game-controller mapping database format. The
semantic surface SHALL be: the buttons `south`, `east`, `west`, `north`,
`leftShoulder`, `rightShoulder`, `leftTrigger`, `rightTrigger`, `back`,
`start`, `guide`, `leftStick`, `rightStick`, `dpadUp`, `dpadDown`,
`dpadLeft`, `dpadRight`; and the axes `leftX`, `leftY`, `rightX`, `rightY`,
`leftTrigger`, `rightTrigger`. Normalization SHALL honor the mapping
elements' axis/button/hat kinds, half-axis (`+`/`-`) ranges, inversion (`~`),
and index mapping. A mapping SHALL be selected by the device's SDL GUID (with
a permissive fallback), and on web a pad reporting the standard mapping SHALL
normalize directly by its standard indices. A device with no mapping SHALL
still be usable through the raw fallback.

#### Scenario: A mapped pad produces semantic values

- **WHEN** a device with a matching database entry moves its left stick and presses a face button
- **THEN** `axis('leftX')`/`axis('leftY')` change accordingly and `isDown` reports the semantic face button, not a raw index

#### Scenario: Half-axis and inversion are applied

- **WHEN** a mapping entry maps a trigger as a half-axis or declares an inverted axis
- **THEN** the normalized value reflects the mapping's range and direction

#### Scenario: Web standard pad is canonical

- **WHEN** a browser pad reports `mapping === 'standard'`
- **THEN** the normalized surface is produced from the standard layout without requiring a database entry

### Requirement: Digital-trigger threshold and canonical ranges

The engine SHALL report analog axes with a canonical, documented range and
SHALL derive each trigger's digital button state from its analog axis using a
documented threshold, consistently on all four targets. The axis range and
threshold SHALL be pinned in the API reference and SHALL not vary by backend
or device. Analog button values beyond on/off SHALL NOT be required (devices
whose face buttons are analog are reported as digital).

#### Scenario: Trigger produces both an axis and a button

- **WHEN** a trigger is pulled past the documented threshold
- **THEN** its axis reports the analog value and its semantic trigger button reports down with a press edge

#### Scenario: Range is consistent across targets

- **WHEN** the same physical stick position is sampled on desktop and in the browser
- **THEN** the reported normalized value uses the same canonical range

### Requirement: Hot-plug handling

The engine SHALL detect pads connected and disconnected during a session and
report connection state per slot. Connect and disconnect SHALL be observable
both as a query (`connected`/`count`) and as event callbacks with
unsubscribe semantics matching the existing hook convention. A pad connected
before the first frame — including a browser pad present before the page
loads — SHALL be discovered without requiring an additional button press.

#### Scenario: Pad connected before first frame is discovered

- **WHEN** a pad is already attached when the engine starts (or when the page loads, on web)
- **THEN** it is reported connected on the first frame

#### Scenario: Connection events fire once

- **WHEN** a pad connects and a script has registered a connect callback
- **THEN** the callback fires once, and calling its unsubscribe function stops further delivery idempotently

### Requirement: Raw fallback for unmapped devices

When a device has no matching mapping, the engine SHALL still expose its raw
axes and buttons by index so it can be used, and SHALL report it as unmapped
rather than silently dropping it. The raw surface SHALL be clearly
distinguished from the semantic surface.

#### Scenario: Unmapped pad is usable

- **WHEN** a connected device matches no database entry
- **THEN** it reports as connected and unmapped, and its raw axes/buttons are readable by index

### Requirement: No Asyncify in the web build

The web player SHALL integrate the gamepad backend using only synchronous
Emscripten gamepad calls and SHALL NOT require the Emscripten `ASYNCIFY`
transform. The backend SHALL be polled from the existing animation-frame
driver, and the synchronous resource/module guarantees of the web bridge
SHALL be preserved. The build SHALL be verified with and without the
`ASYNCIFY` flag to confirm behavior is identical.

#### Scenario: Web build omits Asyncify

- **WHEN** the Emscripten target is configured and built
- **THEN** no `-sASYNCIFY` link option is required for gamepads, and the wasm behaves identically with and without the flag

#### Scenario: Polling rides the animation frame

- **WHEN** the browser frame callback runs
- **THEN** the gamepad backend is sampled synchronously within it and no blocking or yielding primitive is used

### Requirement: Evaluated-library defect fixes

Because the vendored backend was found to have known defects during
evaluation, the engine SHALL correct the following before the milestone's
gate passes, and SHALL cover each with a test:

1. The right analog trigger SHALL be sampled on the web path (the vendored
   axis map/loop must not duplicate the left trigger and skip the right).
2. D-pad/hat mapping SHALL resolve to the semantic d-pad buttons for devices
   whose mapping uses hat elements, including devices that report the d-pad
   as axes or as buttons.
3. Mapping axis scale and offset modifiers SHALL be applied to normalized
   values, so half-axes and inversion are honored.
4. Pads already connected before initialization SHALL be enumerated on all
   targets, not only discovered via connect events.
5. Normalized axis and trigger ranges SHALL be standardized and documented.
6. On Windows, database matching SHALL work for non-Xbox controllers reached
   through the primary path, not only those whose GUID carries the raw-input
   marker.

#### Scenario: Web right trigger is read

- **WHEN** a browser pad moves its right trigger and no left trigger
- **THEN** `rightTrigger` changes and `leftTrigger` does not

#### Scenario: Hat-mapped d-pad works

- **WHEN** a device whose mapping expresses the d-pad as hat bits is used
- **THEN** the semantic d-pad buttons report correctly

#### Scenario: Inversion is honored

- **WHEN** a mapping declares an inverted axis
- **THEN** moving the physical axis in its positive direction produces the mapped direction

#### Scenario: Pre-existing pad enumerated

- **WHEN** a pad is attached before initialization on any target
- **THEN** it appears as connected without an extra input event

#### Scenario: Windows non-Xbox device matches

- **WHEN** a non-Xbox controller is connected on Windows and has a database entry
- **THEN** its mapping is found and its semantic surface is correct

### Requirement: Deterministic backend seam for tests

The gamepad model SHALL be testable without a physical device, a window, or a
backend: a deterministic seam SHALL let tests supply synthetic device
descriptors (name/GUID/raw axes/raw buttons) and advance the poll, exercising
the same normalization and edge logic as production. The seam SHALL NOT be
reachable from scripts and SHALL NOT be part of the script-facing API.

#### Scenario: Headless normalization test

- **WHEN** the harness injects a synthetic device with a known GUID and raw layout and polls a frame
- **THEN** the expected semantic buttons/axes and edges are observed with no device and no window

#### Scenario: Seam is not script-visible

- **WHEN** the script namespace and the API reference are inspected
- **THEN** no injection or simulation function is exposed to scripts
