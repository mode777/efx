# Spec Delta

## MODIFIED Requirements

### Requirement: Script math layer
The engine SHALL bundle pure-JS math helpers on the `efx` object — `efx.math.mat4`
(`identity`, `perspective(fovY, aspect, near, far)`, `ortho(width, height,
near, far)`, `translate(m, v)`, `rotate(m, deg, axis)`, `scale(m, v)`,
`multiply(a, b)`), `efx.math.vec3` (`add`, `sub`, `scale(v, s)`, `normalize`,
`cross`, `dot`), and `efx.math.quat` (`identity`, `fromAxisAngle(deg, axis)`,
`multiply(a, b)`, `toMat4(q)`) — implemented entirely in the `[JS]` layer on
standard ES6 (zero browser/Node dependencies, per the two-layer rule). The
three helpers SHALL live in the `efx.math` sub-namespace and SHALL NOT also
exist as root members of `efx` (hard cut, no aliases). All helpers SHALL be
pure functions that never mutate their arguments and return plain JS data:
matrices are flat 16-number column-major arrays, vectors are 3-number arrays,
angles are **degrees** (ADR 0010: script math is plain JS data; the API never
uses radians). Matrix composition SHALL follow the column-major convention
`multiply(a, b)` computes `a·b` (b applies to the vector first), and
`rotate(m, deg, axis)` computes `m·R(deg, axis)` — a right-handed rotation,
counter-clockwise about `axis` looking down the axis toward the origin,
matching the F2 degree convention's 3D counterpart. The helpers MUST produce
values consistent with the engine's own camera math so script-built transforms
and `setCamera3D` compose predictably.

#### Scenario: Perspective matrix is correct
- **WHEN** a math unit test computes `efx.math.mat4.perspective(60, 4/3, 0.1, 100)`
  and checks selected entries against the expected perspective values
- **THEN** the entries match (degrees-to-tan conversion, aspect on the x
  axis, near/far depth mapping)

#### Scenario: Multiplication order
- **WHEN** `multiply(translate(m, t), rotate(m2, deg, axis))` transforms a
  point
- **THEN** the rotation applies to the point first, then the translation —
  `v' = T · R · v`

#### Scenario: Pure functions
- **WHEN** a helper such as `translate(m, v)` is called
- **THEN** the input matrix `m` is unchanged and the result is a new plain
  array

#### Scenario: Degrees everywhere
- **WHEN** `efx.math.mat4.rotate(identity, 90, [0, 1, 0])` is applied to
  `[1, 0, 0]` (w = 1)
- **THEN** the result is approximately `[0, 0, -1]` — a quarter turn taken
  as 90 degrees, not radians

#### Scenario: No root math aliases
- **WHEN** the math helpers are read after the move
- **THEN** `efx.math.mat4`, `efx.math.vec3`, and `efx.math.quat` expose the
  same functions with the same behavior as the former root helpers, and
  `efx.mat4`, `efx.vec3`, and `efx.quat` no longer exist
