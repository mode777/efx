# billboards Specification

## Purpose
Defines world-space billboard drawing: textured quads placed at a 3D position
and oriented toward the recorded 3D camera, with a configurable facing axis,
atlas regions, in-plane rotation, tint, and a depth-tested/no-depth-write
render policy.

## Requirements

### Requirement: World-space billboard drawing

`efx.drawBillboard(pos, opts)` SHALL record one textured quad placed at the
world position `pos` (a `[x, y, z]` array of finite numbers) and oriented by
the engine using the **3D camera recorded at call time**. The function SHALL be
C-implemented mid-level and SHALL have identical names, signatures, semantics,
and error behavior across the desktop and web bindings. Scripts SHALL NOT
supply or read the camera to orient a billboard.

`opts` SHALL be a required object. `texture` SHALL be required and MUST be a
live Texture or RenderTarget — passing nothing, a value that is neither, or a
destroyed resource SHALL throw `TypeError` and record nothing. `size` SHALL be
either a finite number `> 0` (a uniform world-unit size) or a `[width, height]`
array of finite numbers `> 0`; it names the quad's world dimensions (default
`1`). `color` SHALL be a tint `[r, g, b, a]` (default opaque white).
`sourceRect` (`{ x, y, w, h }`) SHALL select an atlas region (default the full
texture); a zero extent or a region outside the texture bounds SHALL throw
`RangeError` and record nothing. `rotation` SHALL be a finite number of
degrees of in-plane spin about the quad's center (default `0`). `facing` SHALL
be `'view'` (default) or `'y'`. `depthTest` SHALL be a boolean (default
`true`). Unknown fields and wrongly-typed values SHALL throw `TypeError`;
non-finite or `<= 0` size values SHALL throw `RangeError`; a throw records
nothing.

#### Scenario: View-facing billboard follows the camera

- **WHEN** a `facing: 'view'` billboard is drawn at a fixed world position and
  the 3D camera is moved to a new position around it
- **THEN** the quad turns to stay square to the camera, so it is always seen
  face-on

#### Scenario: Y-axis billboard keeps world up

- **WHEN** a `facing: 'y'` billboard is drawn and the camera moves around it,
  including vertically
- **THEN** the quad's up edge stays aligned with world `+Y` and it yaws toward
  the camera without pitching

#### Scenario: Atlas region is sampled

- **WHEN** `drawBillboard` is called with `sourceRect` naming a region of the
  texture
- **THEN** only that region is sampled and drawn, matching `drawQuad`'s
  `sourceRect` semantics

#### Scenario: Rotation spins the quad in its own plane

- **WHEN** a billboard is drawn with `rotation: 90`
- **THEN** the quad turns 90 degrees about its center in its own plane,
  leaving its position and orientation basis otherwise unchanged

#### Scenario: Camera state is snapshotted per record

- **WHEN** a billboard is recorded, the 3D camera is then changed, and another
  billboard is recorded
- **THEN** each billboard uses the camera state at its own record time

#### Scenario: Invalid input throws and records nothing

- **WHEN** `drawBillboard` is called with no texture, a non-sample value, a
  destroyed Texture/RenderTarget, `size: 0`, `size: [1]`, `facing: 'diagonal'`,
  `depthTest: 1`, or an unknown option field
- **THEN** the call throws (`TypeError` or `RangeError` as appropriate) and
  records no billboard

### Requirement: Billboard depth and blend behavior

A billboard SHALL be drawn depth-tested against earlier 3D records when
`depthTest` is `true` (the default), and SHALL NOT write depth. Because it
does not write depth, overlapping billboards SHALL blend over one another in
record order rather than occluding each other. When `depthTest` is `false`,
the billboard SHALL be drawn without depth testing, so it appears over
previously drawn 3D content. Every billboard SHALL use the blend mode
`('alpha' | 'additive' | 'subtractive')` in effect when it was recorded;
changing the blend mode afterwards SHALL NOT affect it. Billboards SHALL NOT
participate in the 2D painter's-order depth behavior and SHALL NOT write to
any depth buffer.

#### Scenario: Occluded by opaque geometry

- **WHEN** a billboard is placed behind an opaque mesh relative to the camera
- **THEN** the billboard is not visible through the mesh where the mesh covers
  it

#### Scenario: Overlapping billboards do not occlude one another

- **WHEN** two additive billboards at different distances overlap
- **THEN** both contribute to the result regardless of their relative
  distances, because neither writes depth

#### Scenario: No depth write between billboards and later draws

- **WHEN** a billboard is drawn and then a mesh is drawn behind it
- **THEN** the mesh still renders where it is visible, because the billboard
  did not write depth

#### Scenario: depthTest false draws over geometry

- **WHEN** a billboard is drawn with `depthTest: false` at a position behind
  opaque geometry
- **THEN** it is drawn over the geometry

#### Scenario: Blend mode is snapshotted

- **WHEN** an additive billboard is recorded and the blend mode is then
  switched to alpha before another billboard is recorded
- **THEN** the first stays additive and the second is alpha
