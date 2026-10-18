# Spec Delta

## ADDED Requirements

### Requirement: Fog

`efx.graphics.setFog(opts)` SHALL configure the engine's **single** fog state
(plain value state — no resource, no handle, no destroy). `opts` SHALL be
`null` (disable fog, the startup default) or a bag:

- `color` — required `[r, g, b, a]` normalized floats (the F2 color
  convention); the alpha component SHALL be ignored by fog;
- `mode` — optional `'linear'` (default), `'exp'`, or `'exp2'`;
- `near`, `far` — required when `mode` is `'linear'` (not permitted otherwise):
  finite numbers with `0 <= near < far`;
- `density` — required when `mode` is `'exp'` or `'exp2'` (not permitted
  otherwise): a finite number `> 0`.

A malformed bag (missing `color`, a non-array or wrong-length `color`,
non-number elements, an unknown mode string, a mode/option mismatch, an
out-of-range `near`/`far`/`density`, or an unknown field) SHALL throw
`TypeError` / `RangeError` following the option-bag conventions and change
nothing. A successful call SHALL replace the entire previous fog state.
`efx.graphics.setFog(null)` SHALL disable fog.

Fog SHALL affect **mesh draws only** (including skinned meshes and
heightfields, which draw through the mesh path). Billboards, particles, and
2D quads SHALL be unaffected. **Unlit materials SHALL be unaffected by fog** —
the unlit shading result is fog-free (skyboxes render unlit and must stay
crisp above a fogged scene).

Fog SHALL be distance-based and applied after lighting in the lit path:
with `d` the distance from the camera position to the fragment world
position, the fog factor SHALL be

- `linear`: `clamp((d - near) / (far - near), 0, 1)`;
- `exp`: `1 - exp(-density * d)`;
- `exp2`: `1 - exp(-(density * d)^2)`;

and the fragment color SHALL be `mix(litColor, fogColor.rgb, factor)` with
alpha unchanged. At `d <= near` the linear factor SHALL be 0 (no fog) and the
factor SHALL increase monotonically with `d` for every mode.

Fog state SHALL be value-snapshotted into each mesh record at record time
(ADR 0019): a `drawMesh` call SHALL be fogged with the fog state active when
it was recorded, and a mid-frame `setFog` SHALL affect only later records.
`efx_render_reset_state()` SHALL restore fog to disabled.

#### Scenario: Linear fog ramps near to far

- **WHEN** linear fog with `near = 10`, `far = 50` is set and a mesh is drawn
  spanning distances 0..60 from the camera
- **THEN** fragments at distance <= 10 are unfogged, fragments at distance
  >= 50 are fully fogColor, and in-between fragments blend monotonically

#### Scenario: Exp and exp2 density fog

- **WHEN** `mode: 'exp2'` with `density: 0.05` is set and a mesh is drawn
- **THEN** the fog factor at distance `d` is `1 - exp(-(0.05*d)^2)`,
  approaching fogColor asymptotically with distance

#### Scenario: Null disables and startup default is off

- **WHEN** a script never calls `setFog`, or calls `setFog(...)` then
  `setFog(null)`
- **THEN** all mesh draws render without fog

#### Scenario: Unlit surfaces ignore fog

- **WHEN** fog is enabled and a mesh surface with `unlit: true` is drawn next
  to a lit surface at the same distance
- **THEN** the lit surface blends toward fogColor and the unlit surface does
  not change

#### Scenario: Record-time snapshot

- **WHEN** a frame calls `drawMesh(A)`, then `setFog(different)`, then
  `drawMesh(B)`
- **THEN** A plays back with the earlier fog state and B with the later one

#### Scenario: Invalid bags throw and change nothing

- **WHEN** `setFog({ near: 0, far: 10 })` (missing color),
  `setFog({ color: [1,1,1,1], near: 10, far: 5 })` (inverted range),
  `setFog({ color: [1,1,1,1], density: -1 })` (mode mismatch / bad density),
  or `setFog({ color: [1,1,1,1], mode: 'linear', density: 0.1 })` (option
  not permitted in mode) is called
- **THEN** the call throws and the previous fog state is unchanged

#### Scenario: Fog stays on the mesh path only

- **WHEN** fog is enabled and a scene draws a mesh, a billboard, a particle
  system, and a 2D quad
- **THEN** only the mesh is attenuated toward fogColor

#### Scenario: CPU reference matches the shader contract

- **WHEN** the fog factor math is applied by the CPU reference for a given
  fog state and view distance
- **THEN** the result equals the shader-side formula above (unit-test parity,
  mirroring the lighting CPU reference)
