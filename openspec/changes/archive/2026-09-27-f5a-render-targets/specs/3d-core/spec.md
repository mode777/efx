# Spec Delta

## MODIFIED Requirements

### Requirement: 3D camera
`efx.setCamera3D(opts)` SHALL configure the engine's single 3D camera from an
option object `{ pos, target, fov, near?, far? }`: `pos` and `target` are
`[x, y, z]` world points (the eye position and the looked-at point), `fov` is
the **vertical** field of view in **degrees**, `near` and `far` are the depth
range in world units with defaults 0.1 and 100. The up vector SHALL be
`[0, 1, 0]`. The 3D camera SHALL be set, never created, and there SHALL be
exactly one (vision.md fixed limits). The 3D camera SHALL be a projection
state separate from the F2 2D projection frame: `setCamera3D` SHALL affect
only 3D draws (mesh records), while 2D draws (`drawQuad`, including draws
whose texture argument is a RenderTarget) continue to render under the 2D
camera state established by the most recent `setCamera2D` (or the F2 default
when never called), leaving F2 behavior unchanged. The projection aspect
SHALL derive from the active rendering surface's extent — the window, or the
active RenderTarget while a begin/end pair is recording. Like all recorded
state (ADR 0019), the camera SHALL be
value-snapshotted at record time: a mesh draw records the 3D camera state in
effect when the draw is recorded and MUST NOT observe later camera changes.
Calling `setCamera3D` with a malformed bag (missing or non-array `pos`/
`target`, non-number `fov`/`near`/`far`, unknown fields) SHALL throw
`TypeError` and change nothing.

#### Scenario: Perspective projection is observable
- **WHEN** two equal meshes are drawn at different distances from the camera eye along its view axis with the same `drawMesh` parameters
- **THEN** the nearer mesh appears larger in the frame, and a wider `fov` renders a visibly larger field of the scene

#### Scenario: 2D draws are unaffected by the 3D camera
- **WHEN** `setCamera3D` is active and a `drawQuad` is recorded, including one sampling a RenderTarget
- **THEN** the quad renders under the most recent `setCamera2D` state (or the F2 default camera when `setCamera2D` was never called), and the committed F2 golden output is unchanged

#### Scenario: Aspect follows the rendering surface
- **WHEN** the same 3D scene is recorded to the window and into a RenderTarget whose extent has a different aspect ratio, and both are shown
- **THEN** each render uses its own surface's aspect — neither is stretched relative to its own surface

#### Scenario: Camera is value-snapshotted per record
- **WHEN** a mesh is drawn, then `setCamera3D` moves the camera, all in one render hook
- **THEN** playback renders that mesh with the camera state at its record time

#### Scenario: Defaults for near and far
- **WHEN** `setCamera3D({ pos, target, fov })` is called without `near`/`far`
- **THEN** the depth range is 0.1 to 100 and the draw succeeds

#### Scenario: Malformed camera options throw
- **WHEN** `setCamera3D` is called with a missing `pos`, a non-number `fov`, or an unknown field
- **THEN** the call throws `TypeError` and the previously set camera state remains in effect
