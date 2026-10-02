# input

## Purpose

The engine's platform input layer: a C-owned keyboard and mouse state model
with frame-staged event delivery, shared by every script binding, plus the
deterministic injection seam the test harness uses to exercise it headlessly.

## Requirements

### Requirement: C-owned frame-staged input state

The engine SHALL own keyboard and mouse input state in its C core,
independent of the script binding and of the rendering backend. The platform
layer SHALL translate the backend's event callback into core state updates;
scripts SHALL NOT read backend or host input state directly. Level state
(which keys and mouse buttons are currently down, and the current pointer
position) SHALL update when an event arrives. Edge state — keys and buttons
that went down or up — SHALL be valid for exactly one frame and cleared at
frame end. Buffered events SHALL be dispatched to registered script callbacks
once per frame, in arrival order, before that frame's update hooks, so a
callback never runs outside a frame. The two script-facing views of input —
queries and event callbacks — SHALL be derived from this one state source and
MUST NOT disagree within a frame.

#### Scenario: Queries and callbacks share one source

- **WHEN** a key-down event arrives and the frame's callbacks and update hooks run
- **THEN** the `onDown` callback fires before the update hooks, and during those hooks the key queries as down and pressed

#### Scenario: Edge state lasts one frame

- **WHEN** a key-down event is delivered in frame N and no further event arrives
- **THEN** the key queries as pressed in frame N and as down-but-not-pressed in frame N+1

#### Scenario: Event order is arrival order

- **WHEN** several input events are delivered between two frames
- **THEN** their callbacks run in the order the events arrived

### Requirement: Keyboard semantics

Keyboard input SHALL be reported by an engine-owned, layout-named key
identifier set that maps onto the platform's virtual keycodes and is
documented in the API reference. A key-down event SHALL report whether it is
an auto-repeat. The engine SHALL distinguish three observable conditions per
key: currently down (level), went down this frame (press edge), and went up
this frame (release edge). A press edge SHALL be reported only for the
initial up-to-down transition, not for auto-repeats. Character/text input
SHALL be delivered as a separate character event carrying the platform's
decoded text, distinct from physical key events.

#### Scenario: Auto-repeat is distinguishable

- **WHEN** a key is held long enough to auto-repeat
- **THEN** the down callback fires again with its repeat flag set, while the press-edge query stays true only for the initial transition

#### Scenario: Character input is separate from keys

- **WHEN** the user types a shifted letter
- **THEN** key events report the physical keys and a character event reports the resulting text

#### Scenario: Release edge is observable

- **WHEN** a held key is released
- **THEN** the release-edge query is true for that frame and the up callback fires

### Requirement: Mouse semantics and coordinate space

Mouse input SHALL report three buttons (left, right, middle) with the same
level/press-edge/release-edge conditions as keys, plus pointer position,
pointer movement since the previous frame, and wheel movement for the frame.
Pointer coordinates and window size SHALL be reported in the renderer's
surface pixel space with a top-left origin and y pointing down — the same
space the 2D camera frame and quad drawing use — so hit-testing against drawn
content requires no coordinate conversion. On high-DPI displays the surface
size SHALL be the framebuffer size and the engine SHALL additionally expose
the ratio of surface pixels to logical window pixels, so scripts can derive
logical units when they want them.

#### Scenario: Mouse position matches drawing space

- **WHEN** the pointer is over a quad drawn at a known surface-pixel position
- **THEN** the reported pointer position lies inside the quad's surface-pixel rectangle

#### Scenario: Frame movement and wheel

- **WHEN** the pointer moves and the wheel turns between two frames
- **THEN** the movement and wheel queries report the accumulated deltas for the frame and reset on the next frame

#### Scenario: High-DPI surface is reported consistently

- **WHEN** the display has a scale factor greater than one
- **THEN** the reported window size and pointer coordinates are in surface pixels, and the reported scale factor is the surface-to-logical ratio

### Requirement: Focus handling

When the window loses focus, the engine SHALL clear all held key and mouse
button state so that no key or button remains logically down, and SHALL NOT
emit synthetic up events for the cleared state. When the window regains
focus, input resumes from the cleared state.

#### Scenario: Keys do not stick across focus loss

- **WHEN** a key is held, the window loses focus, and a frame runs
- **THEN** the key queries as not down

#### Scenario: No synthetic up events on focus loss

- **WHEN** the window loses focus while a key is held
- **THEN** no key-up callback is dispatched for that key

### Requirement: Deterministic input simulation

The C input core SHALL expose a deterministic injection entry point through
which the test harness can synthesize keyboard and mouse events without a
window or a display. The seam SHALL NOT be reachable from scripts and SHALL
not be part of the script-facing API. Synthetic events SHALL flow through the
same state and frame-staging path as real platform events, so tests exercise
production semantics.

#### Scenario: Headless integration test

- **WHEN** the harness injects a key-down before a frame and then runs the frame
- **THEN** the registered down callback fires and the key queries as down and pressed, with no window created

#### Scenario: Seam is not script-visible

- **WHEN** the API reference and the script namespace are inspected
- **THEN** no simulation or injection function is exposed to scripts

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

### Requirement: Web keyboard focus in an embed

On the web target, keyboard input SHALL reach scripts once the player's
document has focus, including when the player is embedded in an iframe on a
host page that currently holds focus. A pointer press on the player's canvas
SHALL move focus into the player's document so that subsequent key events are
delivered, and the engine SHALL NOT suppress the browser default that
performs this focus transfer. The engine SHALL continue to suppress the
browser default for game keys (so a game key does not scroll or navigate the
host page), independently of pointer-focus handling. Native (non-web) input
behavior SHALL be unchanged.

#### Scenario: Clicking the embed focuses it and keys arrive

- **WHEN** the player runs in an iframe and the host page holds focus, and the visitor presses the pointer on the player's canvas
- **THEN** the player's document gains focus and a subsequent key-down is reported by `efx.keyboard` (both the event callback and the held-state query)

#### Scenario: Game keys stay suppressed

- **WHEN** a game key such as space or an arrow is pressed while the player's canvas has focus
- **THEN** the engine suppresses the browser's default action for that key while still reporting it to scripts

#### Scenario: Desktop behavior is unchanged

- **WHEN** the player runs on a native target
- **THEN** keyboard and mouse input behave as before, with no dependency on browser focus
