# Spec Delta

## ADDED Requirements

### Requirement: Batched 2D sprite drawing

`efx.drawSprites(texture, sprites)` SHALL record one textured 2D quad per
entry in `sprites`, each exactly equivalent to a `drawQuad(sprite.x, sprite.y,
texture, sprite)` call with the entry's fields. It SHALL be C-implemented
mid-level and 2D-only: sprites SHALL be placed and transformed in the current
2D camera frame and SHALL NOT be oriented in 3D or depth-tested.

`texture` SHALL be required and MUST be a live Texture or RenderTarget —
otherwise the call SHALL throw `TypeError` and record nothing. `sprites` SHALL
be a required array; a non-array value SHALL throw `TypeError`. Each entry
SHALL be an object carrying `x` and `y` (required finite numbers, frame
pixels) plus the `drawQuad` options `size`, `sourceRect`, `color`, `rotation`,
`scale`, and `origin`, with the same types, defaults, and error behavior as
`drawQuad`. If any entry is invalid, the call SHALL throw (`TypeError` or
`RangeError` as appropriate) and SHALL record none of the call's sprites.
Each recorded sprite SHALL snapshot the 2D camera and blend mode in effect at
the time of the call, and the sprites SHALL play back in entry order,
consistent with the display-list painter's-order contract. Consecutive sprites
sharing a texture and blend mode SHALL render identically to the equivalent
sequence of individual `drawQuad` calls.

#### Scenario: Equivalent to individual quad draws

- **WHEN** a script calls `drawSprites(tex, entries)` in one frame and, in
  another frame, calls `drawQuad` once per entry with the same `x`, `y`, and
  options
- **THEN** the two frames are pixel-identical

#### Scenario: Per-sprite options apply

- **WHEN** entries use `sourceRect` atlas regions, `rotation`, `scale`,
  `color`, and `origin`
- **THEN** each sprite is drawn exactly as the corresponding `drawQuad` call
  would draw it

#### Scenario: Entry order is painter's order

- **WHEN** two overlapping sprites are supplied with the same texture, in a
  known order
- **THEN** the later entry appears on top

#### Scenario: Invalid argument records nothing

- **WHEN** `drawSprites` is called with a non-array `sprites`, with a missing
  or destroyed texture, or with an entry missing `x`/`y` or holding an
  invalid option
- **THEN** the call throws (`TypeError` or `RangeError` as appropriate) and
  records no sprites from the call

#### Scenario: Empty array records nothing

- **WHEN** `drawSprites` is called with an empty array
- **THEN** the call succeeds and records no draws
