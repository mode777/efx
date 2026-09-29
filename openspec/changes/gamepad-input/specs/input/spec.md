# Spec Delta

## ADDED Requirements

### Requirement: C-owned frame-staged gamepad state

The C-owned input core SHALL additionally own gamepad state, independent of
the script binding and of the rendering backend. Unlike keyboard and mouse,
gamepad state SHALL be updated by a per-frame poll at frame begin rather than
by the platform event callback, because gamepad APIs are poll-based. The
gamepad section SHALL preserve the same guarantees as the keyboard/mouse
section: one shared state source for queries and callbacks, press/release
edges valid for exactly one frame, and event callbacks dispatched once per
frame in a deterministic order before the update hooks. The platform layer
SHALL be responsible for translating the vendored backend's device state into
core poll input; scripts SHALL NOT read backend or host gamepad state
directly. Gamepad polling SHALL NOT change the existing keyboard/mouse
semantics or their ordering.

#### Scenario: Gamepad and keyboard share one dispatch point

- **WHEN** a frame runs with both a keyboard event and a changed gamepad state
- **THEN** both are committed before the update hooks and a gamepad callback cannot observe a different frame state than its query

#### Scenario: Edges expire for gamepads too

- **WHEN** a pad button goes down in frame N and no further change happens
- **THEN** its press edge is true in frame N and false in frame N+1

### Requirement: Deterministic gamepad simulation

The C input core SHALL expose a deterministic injection entry point through
which the test harness can supply synthetic gamepad device descriptors and
state without a window, a display, or a backend. Synthetic input SHALL flow
through the same state and frame-staging path as real backend input so tests
exercise production normalization and edge semantics. The seam SHALL NOT be
reachable from scripts.

#### Scenario: Headless gamepad integration test

- **WHEN** the harness injects a synthetic pad descriptor and button/axis state before a frame and runs the frame
- **THEN** the registered gamepad callbacks fire and the semantic queries report the expected state, with no window created

#### Scenario: Gamepad seam is not script-visible

- **WHEN** the API reference and the script namespace are inspected
- **THEN** no gamepad injection or simulation function is exposed to scripts
