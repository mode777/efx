# Spec Delta: input

## MODIFIED Requirements

### Requirement: Mouse semantics and coordinate space

Mouse input SHALL report three buttons (left, right, middle) with the same
level/press-edge/release-edge conditions as keys, plus pointer position,
pointer movement since the previous frame, and wheel movement for the frame.
Pointer coordinates and movement deltas SHALL be reported in **frame pixels**
— the coordinate space of the most recently set 2D camera frame (the same
space quad drawing uses) — mapped from the surface pixel space by a
scale-only transform (`surface px × frame/surface` per axis), so hit-testing
against drawn content requires no coordinate conversion and a virtual-frame
game reads input in its own units. When no 2D frame was ever set, the frame
equals the surface extent and the mapping is the identity. The mapping SHALL
NOT fold in 2D camera pan, zoom, or rotation: it is a resolution transform
only, stable regardless of camera motion. The wheel delta SHALL remain in
scroll ticks and SHALL NOT be scaled. Window size SHALL be reported in the
renderer's surface pixel space with a top-left origin and y pointing down —
the physical surface, independently of the frame mapping — and on high-DPI
displays the engine SHALL additionally expose
the ratio of surface pixels to logical window pixels, so scripts can derive
logical units when they want them.

#### Scenario: Mouse position matches drawing space

- **WHEN** the pointer is over a quad drawn at a known frame-pixel position
  under an active 2D frame
- **THEN** the reported pointer position lies inside the quad's frame-pixel
  rectangle with no script-side conversion

#### Scenario: Frame movement and wheel

- **WHEN** the pointer moves and the wheel turns between two frames
- **THEN** the movement and wheel queries report the accumulated deltas for the frame and reset on the next frame

#### Scenario: High-DPI surface is reported consistently

- **WHEN** the display has a scale factor greater than one
- **THEN** the reported window size is in surface pixels with the reported
  scale factor as the surface-to-logical ratio, while the reported pointer
  coordinates are frame pixels and are unaffected by the display scale
  beyond the frame mapping

#### Scenario: No 2D frame falls back to surface pixels

- **WHEN** `setCamera2D` was never called (or was last called without a
  `frame`) and the pointer moves
- **THEN** the reported pointer position and deltas equal the surface-pixel
  values — the frame mapping is the identity
