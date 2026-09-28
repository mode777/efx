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
