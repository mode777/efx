# lighting Specification

## Purpose

Defines the F4a fixed-function lighting model: the four point-light slots and
the single directional light, the four-channel Phong material object bound per
mesh surface, the exact lit shading result for meshes, and the CPU reference
that verifies the lighting math.

## Requirements

### Requirement: Point light bank

`efx.graphics.setLight(slot, opts)` SHALL configure one of exactly **four** fixed
point-light slots (vision.md fixed limits). `slot` SHALL be an integer `0..3`;
any other value SHALL throw `RangeError` and change nothing. `opts` SHALL be
`null` (disable the slot) or a bag `{ pos, color, range? }`:

- `pos` — required `[x, y, z]` world-space position;
- `color` — required `[r, g, b, a]` normalized floats (the F2 color
  convention); the alpha component SHALL be ignored by lighting;
- `range` — optional attenuation radius: a finite number `>= 0`, default `0`
  meaning **no attenuation** (the light reaches everywhere at full strength).

A malformed bag (missing or non-array `pos`/`color`, wrong element count,
non-number elements, a negative or non-finite `range`, or an unknown field)
SHALL throw `TypeError` and change nothing. Setting a slot SHALL replace that
slot's previous value. All four slots SHALL start disabled, and lights are a
pre-allocated bank — the only slot-based resource (ADR 0011).

#### Scenario: Set and replace a slot

- **WHEN** `setLight(0, A)` is called and later `setLight(0, B)`
- **THEN** slot 0 holds B's values, slots 1–3 are unchanged, and the color
  alpha has no effect on the rendered light

#### Scenario: Lights are disabled by default and `null` disables

- **WHEN** a script sets a light and later calls `setLight(slot, null)`, or
  never sets any light
- **THEN** the slot contributes nothing to shading

#### Scenario: Invalid slot or malformed bag throws

- **WHEN** `setLight(4, opts)` or `setLight(0, { pos: [0, 0], color: [1, 1, 1, 1] })`
  is called
- **THEN** the call throws `RangeError` / `TypeError` respectively and the
  slot's previous state is unchanged

### Requirement: Directional light

`efx.graphics.setDirectionalLight(opts)` SHALL configure the engine's **single**
directional light (vision.md fixed limits). `opts` SHALL be `null` (disable
the light, the startup default) or a bag `{ dir, color }`:

- `dir` — required `[x, y, z]` **direction the light travels**, so the
  direction toward the light used in shading is `normalize(-dir)`;
- `color` — required `[r, g, b, a]` normalized floats; alpha ignored.

A malformed bag (missing or non-array/non-3-element `dir`, zero-length `dir`,
missing/wrong `color`, unknown field) SHALL throw `TypeError` and change
nothing. The directional light contributes no attenuation.

#### Scenario: Directional light illuminates one side

- **WHEN** a lit surface is drawn with `setDirectionalLight({ dir, color })`
  and the same surface is drawn with the light disabled
- **THEN** the lit frame is brighter, with the faces perpendicular to
  `-dir` brightest and faces facing away unlit

#### Scenario: Malformed directional light throws

- **WHEN** `setDirectionalLight({ color: [1, 1, 1, 1] })` or
  `setDirectionalLight({ dir: [0, 0, 0], color: [1, 1, 1, 1] })` is called
- **THEN** the call throws `TypeError` and the previous light state remains

### Requirement: Phong material model
A material is a **JS-managed** plain object (ADR 0011; no native class, no
`destroy()`) with four optional Phong channels plus optional `unlit` and
`alphaMode` flags.
Channel values are snapshotted by the engine when the material is bound —
later mutation of the script object MUST NOT change the bound material.
Channel maps are held by **handle** (ADR 0019) and the referenced Textures and
RenderTargets are retained by the engine while bound (see below). Omitted
channels or fields take documented defaults:

- `ambient: { color, map? }` — default color black `[0, 0, 0, 1]`, no map;
- `diffuse: { color, map? }` — default color white `[1, 1, 1, 1]`, no map;
- `specular: { color, shininess?, map? }` — default color black
  `[0, 0, 0, 1]`, `shininess` default `32`, no map;
- `emissive: { color, map? }` — default color black `[0, 0, 0, 1]`, no map;
- `alphaMask?` — an optional material-level live `Texture` or live
  `RenderTarget` (F5a); default none;
- `blend?` — an optional blend mode string (`'alpha'` | `'additive'` |
  `'subtractive'`) selecting how the surfaces bound to this material combine
  with the existing frame content; absent or `null` means the surface uses the
  frame's blend render state at draw record time (see the `2d-layer` blending
  requirement). The blend equation is orthogonal to `alphaMode`: it selects
  how fragments combine inside whichever pass `alphaMode` routes the surface
  to, and never selects the pass itself.
- `unlit?` — an optional boolean (default `false`). When `true`, the surface
  is shaded without the lighting equation and without any light contribution:
  its color is the `diffuse` channel color multiplied by the `diffuse` map
  sample and the albedo (see the F4a lit shading requirement). `alphaMask`,
  `alphaMode`, and `blend` still apply; `ambient`, `specular`, `emissive`, and
  every light are ignored.
- `alphaMode?` — an optional transparency-mode string selecting the render
  path the surface takes (see the `3d-core` whole-mesh drawing requirement for
  the playback routing): `'opaque'` (the default), `'mask'` (the alpha-scissor
  path — full rendering path with depth writing, fragments sampling
  `alphaMask` alpha below `0.5` discarded), or `'blend'` (the transparent
  path — drawn last in the pass, depth-sorted, no depth writing, fragment
  alpha honored). Absent or `null` means `'opaque'`.

Each channel `color` SHALL be a `[r, g, b, a]` array of normalized floats.
The channel alphas are ignored by shading **except** the `diffuse` alpha in
`'blend'` mode, where it multiplies the fragment alpha (see the F4a lit
shading requirement). `shininess` SHALL be a finite
number `> 0`. A `map` field and the material-level `alphaMask`, when present,
MUST be a live `Texture` or a live `RenderTarget` (F5a). A non-object
material, an unknown field, a wrong-typed or wrong-length `color`, a
non-number `shininess`, a wrongly-typed `map`/`alphaMask`, a non-boolean
`unlit`, or a destroyed `Texture` or destroyed `RenderTarget` passed as a map
SHALL throw `TypeError`; a `blend` that is present and is neither one of the
three mode strings nor `null` SHALL throw `TypeError`; an `alphaMode` that is
present and is neither `'opaque'`, `'mask'`, `'blend'`, nor `null` SHALL throw
`TypeError`; a non-positive or non-finite `shininess` SHALL throw
`RangeError`; the call that was passed the material SHALL record nothing.
`map`/`alphaMask` were rejected as unknown in F4a and are now accepted.

The engine SHALL **retain** each bound map's `Texture` or `RenderTarget`:
calling `destroy()` on the bound resource releases the script's handle, and
the native storage is released only once no material binding references it —
i.e. after the surface is rebound without that map, bound to `null`, or the
owning mesh is destroyed. A bound map therefore never becomes a dangling
handle. An omitted or `null` map means no modulation.

#### Scenario: Omitted channels take defaults
- **WHEN** `mesh.setSurfaceMaterial(0, {})` is called
- **THEN** the surface uses the default white-diffuse Phong material with no maps, no alpha mask, no unlit flag, no blend override, and `alphaMode` `'opaque'`

#### Scenario: Specular channel accepts color and shininess
- **WHEN** a material sets `specular: { color: [1, 1, 1, 1], shininess: 64 }`
- **THEN** the surface's specular highlight is sharper than with the default shininess of 32

#### Scenario: Material object is snapshotted
- **WHEN** a material is bound, its script object is then mutated (including swapping its `map`), and the frame renders
- **THEN** the surface renders with the values and map handles at binding time

#### Scenario: Channel map modulates the channel
- **WHEN** a surface is drawn first with no map and then with a per-channel map whose sampled texels are darker than white
- **THEN** that channel's contribution is multiplied by the sampled texel colors, and the no-map case is the brightest

#### Scenario: Alpha mask cuts out below the threshold
- **WHEN** a material with an `alphaMask` whose alpha is `0` over part of the surface and `1` over the rest is drawn
- **THEN** fragments sampling mask alpha below `0.5` are not written, the rest show the lit surface, and the mask RGB is ignored

#### Scenario: Material blend is snapshotted at binding
- **WHEN** a material with `blend: 'additive'` is bound to a surface and the
  script object's `blend` is later changed before the surface is drawn
- **THEN** the surface's recorded draw uses `'additive'`, the value at binding
  time, and a surface whose material omits `blend` uses the frame's blend
  render state at record time

#### Scenario: Invalid blend throws
- **WHEN** a material sets `blend: 'multiply'` or `blend: 1`
- **THEN** the binding call throws `TypeError` and the surface's previous
  binding is unchanged

#### Scenario: Unlit accepts only a boolean
- **WHEN** a material sets `unlit: 1` or `unlit: 'yes'`
- **THEN** the binding call throws `TypeError` and the surface's previous
  binding is unchanged

#### Scenario: Alpha mode validates and defaults to opaque
- **WHEN** a material sets `alphaMode: 'blend'`, `alphaMode: 'mask'`, or omits
  it, and another material sets `alphaMode: 'multiply'` or `alphaMode: 1`
- **THEN** the valid materials bind with the named mode (`'opaque'` when
  omitted or `null`), and the invalid ones throw `TypeError` leaving the
  surface's previous binding unchanged

#### Scenario: Alpha mode is snapshotted at binding
- **WHEN** a material with `alphaMode: 'blend'` is bound to a surface and the
  script object's `alphaMode` is later changed before the surface is drawn
- **THEN** the surface's recorded draw routes by `'blend'`, the value at
  binding time

#### Scenario: Bound map texture is retained
- **WHEN** a live `Texture` is bound as a map and the script then calls `Texture.destroy()` while the binding is in effect, and later re-binds the surface without that map
- **THEN** drawing before the rebind still shades with the map and does not throw, and the native storage is released only after the rebind

#### Scenario: Bound map render target is retained
- **WHEN** a live `RenderTarget` is bound as a map, `destroy()` is called on it while the binding is in effect, and the surface is later rebound without that map
- **THEN** drawing before the rebind still shades with the target's contents and does not throw, and the native storage is released only after the rebind

#### Scenario: Invalid material throws
- **WHEN** a material has a `map` that is not a live `Texture` or live `RenderTarget` (a number, a destroyed texture, a destroyed render target), a channel color with three elements, or `specular: { shininess: 0 }`
- **THEN** the binding call throws `TypeError` / `RangeError` and the surface's previous binding is unchanged

### Requirement: Per-surface material binding

Materials SHALL bind to mesh **surfaces**, never to global engine state
(ADR 0024). `Mesh.setSurfaceMaterial(surfaceIndex, mat)` SHALL bind a
snapshot of `mat` to surface `surfaceIndex` of the receiver `mesh`; `mat`
SHALL be a material object or `null` (bind the engine default material). The
receiver MUST be a live Mesh (`TypeError` otherwise), `surfaceIndex` an
integer in `0..surfaceCount-1` (`RangeError` otherwise), and `mat` an object
or `null` (`TypeError` otherwise). The call changes only that surface's
binding. The former free function `efx.graphics.setMeshSurfaceMaterial` SHALL
NOT exist (hard cut, no alias).

`efx.graphics.createMeshData(surfaces, materials?)` SHALL accept an optional positional
`materials` array parallel to `surfaces`: `materials[i]` (a material object
or `null` for the default) becomes surface `i`'s initial binding. When present,
`materials` MUST have exactly one entry per surface (wrong length SHALL throw
`RangeError`; invalid entries SHALL throw `TypeError`). A surface with no
binding, or bound to `null`, SHALL render with the engine **default material**
— white diffuse Phong with no maps and no emissive — and surface bindings
SHALL carry over unchanged at `createMesh`.

#### Scenario: Bind and rebind a surface

- **WHEN** `mesh.setSurfaceMaterial(1, A)` is followed by
  `mesh.setSurfaceMaterial(1, B)`
- **THEN** surface 1 renders with B and the mesh's other surfaces are
  unchanged

#### Scenario: Materials bind at creation

- **WHEN** `createMeshData([s0, s1], [A, null])` is
  used to create a mesh
- **THEN** surface 0 renders with material A and surface 1 renders with the
  default material

#### Scenario: Out-of-range index or wrong length throws

- **WHEN** `mesh.setSurfaceMaterial(2, mat)` is called on a 2-surface
  mesh, or `materials` has fewer/more entries than `surfaces`
- **THEN** the call throws `RangeError` and changes nothing

#### Scenario: No free-function binding remains

- **WHEN** a script reads `efx.graphics.setMeshSurfaceMaterial` after this
  change
- **THEN** it is `undefined`, and binding is reachable only as the
  `Mesh.setSurfaceMaterial` method on a live `Mesh`

### Requirement: F4a lit shading

`drawMesh` playback SHALL shade each fragment with the F4 Phong equation — the
F4a base extended by F4b per-channel maps and alpha masks — evaluated per
fragment from the surface normal interpolated across the triangle and
normalized. The surface **albedo** SHALL be the surface's vertex color
(default opaque white) multiplied component-wise by the `drawMesh` tint
(default opaque white). With `N` the unit world normal, `V` the unit direction
to the camera, `uv` the surface's interpolated texture coordinate, and
`M_c = channel.map ? texture(channel.map, uv).rgb : (1, 1, 1)` the channel's
map sample, each enabled light contributes:

- **ambient** = `material.ambient.color × M_ambient × albedo` (flat,
  independent of lights);
- **diffuse** = `material.diffuse.color × M_diffuse × albedo × Σ (lightColor.rgb × max(N·L, 0) × atten)`;
- **specular** = `material.specular.color × M_specular × Σ (lightColor.rgb × max(N·H, 0)^shininess × atten)`
  where `H = normalize(L + V)` (Blinn-Phong);
- **emissive** = `material.emissive.color × M_emissive` (added directly,
  **not** modulated by the albedo).

The final fragment color SHALL be `ambient + diffuse + specular + emissive`,
clamped to `[0, 1]` per channel; there is no HDR or tonemapping. For a point
light, `L` is the unit direction from the fragment to the light's world
position, `d` is the distance, and attenuation is `atten = 1` when `range` is
`0`, else `atten = clamp(1 - d / range, 0, 1)`. For the directional light, `L`
is the unit direction toward the light (`normalize(-dir)`) and `atten = 1`.

The fragment's **output alpha** SHALL be selected by the material's
`alphaMode`. In `'opaque'` and `'mask'` mode the output alpha SHALL be `1`:
the fragment is fully opaque regardless of vertex, tint, or channel alpha
(glTF OPAQUE/MASK semantics — translucency requires declaring `'blend'`).
In `'blend'` mode the output alpha SHALL be the product
`material.diffuse.color.a × vertex-color alpha × tint alpha`, clamped to
`[0, 1]`, and the fragment combines with the framebuffer by the resolved blend
equation. In `'blend'` mode the `alphaMask` map, if bound, SHALL NOT discard —
the mode wins over the mask, and a material that wants cutout transparency
declares `'mask'`.

The mask-discard rule otherwise stands: when the mode is `'opaque'` or
`'mask'` and the material has an `alphaMask`, the fragment SHALL sample
`texture(alphaMask, uv).a` and be **discarded** when that value is `< 0.5`;
otherwise the fragment is written with output alpha `1` — the mask never
modulates the output alpha and no alpha blending or dithering is applied by
the mask.

When the material has `unlit: true`, the surface SHALL bypass the entire
lighting equation and the light bank: the final fragment color SHALL be the
**unlit color** `material.diffuse.color × M_diffuse × albedo`, clamped to
`[0, 1]`. The `alphaMask`, the `alphaMode` output-alpha rule, and the blend
mode SHALL behave exactly as in the lit path; `ambient`, `specular`,
`emissive`, and every point/directional light SHALL have no effect. A material
with `unlit: true` and no `diffuse.map` therefore renders its `diffuse` color
scaled by the albedo, and a material with no lights enabled but `unlit: false`
continues to render its lit result (which is black except for its emissive
term).

A surface without `normals` SHALL use the F3 default normal `(0, 0, 1)` in
object space. A surface without `uvs` SHALL sample every map at the F3 default
`(0, 0)`. Absent maps contribute the neutral factor `1`, so a material that
binds no maps shades exactly as in F4a.

2D quad records SHALL be unaffected and the committed F2 goldens SHALL remain
valid. Because absent maps are neutral and `unlit` defaults to `false`, the
committed F3 and F4a goldens SHALL remain byte-identical for fully opaque
materials. With every light disabled, a default-material surface renders black
except for its emissive term.

#### Scenario: Ambient alone is flat

- **WHEN** a surface with `ambient: { color: [0.5, 0.5, 0.5, 1] }` is drawn
  with every light disabled
- **THEN** every fragment is a flat half-grey scaled by the albedo

#### Scenario: Diffuse depends on the normal

- **WHEN** a cube is lit by one point light and drawn
- **THEN** the face most facing the light is brightest and faces facing away
  are unlit

#### Scenario: Specular highlight follows the camera

- **WHEN** a sphere with a specular material is lit by a point light and the
  camera moves so the reflection direction aligns with a surface point
- **THEN** a highlight appears on that surface point

#### Scenario: Point light attenuation falls to zero at range

- **WHEN** equal-distance fragments are lit by a point light with a finite
  `range`
- **THEN** a fragment at distance `>= range` receives no light from it and a
  fragment at distance `0` receives full strength

#### Scenario: Emissive is not modulated

- **WHEN** a surface with an emissive color and a black albedo is drawn with a
  tint
- **THEN** the emissive contribution is unchanged by the albedo and tint

#### Scenario: Default material with no lights is black

- **WHEN** an unbound surface is drawn with all lights disabled
- **THEN** it renders black (no ambient, diffuse, specular, or emissive term)

#### Scenario: Diffuse map is sampled by uv

- **WHEN** a surface carrying known `uvs` and a `diffuse.map` with distinct
  texels is lit normally and drawn
- **THEN** each fragment's diffuse contribution is the channel color scaled by
  its sampled texel color

#### Scenario: Absent map is neutral

- **WHEN** the same lit scene is rendered once with no maps and once with all
  channels explicitly mapped to the engine white texture
- **THEN** the two renders match within the golden tolerance

#### Scenario: A surface without uvs samples at zero

- **WHEN** a surface with no `uvs` attribute and a `diffuse.map` is drawn
- **THEN** every fragment samples the map at `(0, 0)` and shading is defined

#### Scenario: Alpha mask does not change surviving alpha

- **WHEN** a masked surface is drawn in `'opaque'` or `'mask'` mode with a
  tint alpha below `1`
- **THEN** fragments that survive the mask render fully opaque (output alpha
  `1`), and the mask's own alpha and the tint alpha do not alter that

#### Scenario: Blend mode honors material, vertex, and tint alpha

- **WHEN** a surface is drawn with `alphaMode: 'blend'`, a `diffuse` color
  alpha of `0.5`, vertex-color alpha `0.8`, and tint alpha `1`
- **THEN** the output alpha is `0.4` and the fragment alpha-blends over the
  existing frame content

#### Scenario: Opaque mode saturates fragment alpha

- **WHEN** the same surface with a `diffuse` alpha of `0.5` and vertex alpha
  `0.8` is drawn with the default mode
- **THEN** every surviving fragment renders fully opaque, with no in-place
  blending

#### Scenario: Blend mode ignores the alpha-mask discard

- **WHEN** a material with an `alphaMask` sampling alpha `0` over part of the
  surface is drawn with `alphaMode: 'blend'`
- **THEN** no fragment is discarded by the mask; all fragments alpha-blend per
  the blend equation

#### Scenario: Unlit ignores lights and the lit channels

- **WHEN** a surface with `unlit: true`, a `diffuse` color and map, and a
  non-black ambient is drawn with lights enabled and again with every light
  disabled
- **THEN** both renders are identical and equal the diffuse color × map ×
  albedo, with no ambient/specular/emissive contribution

#### Scenario: Unlit still honors alpha mask and blend

- **WHEN** an unlit surface with an `alphaMask`, `blend: 'additive'`, and the
  default `alphaMode` is drawn
- **THEN** fragments below the mask threshold are discarded and the survivors
  blend additively, exactly as in the lit path

### Requirement: Lighting CPU reference

The repository SHALL contain a CPU implementation of the F4 lighting equation
independent of the GPU path. It SHALL accept, per shaded fragment, the
material, the material's `alphaMode`, the light set, the world position and
normal, the camera position, the albedo, the per-channel map samples `(rgb)`
and the alpha-mask sample, so that map modulation, the cutout threshold, and
the alpha-mode output-alpha rule are testable without a GPU. It SHALL apply
the same output-alpha rule as the GPU path: alpha `1` in `'opaque'` and
`'mask'` mode, and the clamped product of `diffuse` alpha × albedo alpha ×
tint alpha in `'blend'` mode. Unit
tests SHALL assert its values for analytic configurations — ambient-only, a
surface perpendicular to a light (diffuse maximum), the specular peak,
attenuation at exactly `range`, directional-only lighting, emissive-only, a
channel map scaling exactly one channel, the `0.5` alpha-mask cutout
boundary, and both alpha-mode outputs. These tests SHALL run headless on all
four targets as part of the standard suite, and at least one committed golden
scene SHALL confirm the rendered GPU result agrees with the reference.

#### Scenario: CPU reference is analytically correct

- **WHEN** the CPU reference shades a surface with a normal directly facing a
  unit-color light and a white diffuse material
- **THEN** the diffuse term equals the light contribution and a surface
  facing away yields zero diffuse

#### Scenario: CPU reference applies channel maps

- **WHEN** the CPU reference is given a diffuse map sample of `(0.25, 0.5,
  0.75)` and neutral samples elsewhere
- **THEN** only the diffuse contribution is scaled by that sample and the
  other channels are unchanged

#### Scenario: CPU reference cutout boundary

- **WHEN** the CPU reference is given an alpha-mask sample below `0.5`
- **THEN** it reports the fragment discarded, and at `0.5` or above it reports
  the shaded color with the mode's output alpha

#### Scenario: CPU reference applies the alpha mode

- **WHEN** the CPU reference shades the same fragment once in `'opaque'` mode
  and once in `'blend'` mode with a `diffuse` alpha of `0.5` and an albedo
  alpha of `0.8`
- **THEN** the opaque result carries alpha `1` and the blend result carries
  alpha `0.4`

#### Scenario: GPU agrees with the reference

- **WHEN** a golden scene renders a surface under a known light, material,
  map, alpha-mask, and alpha-mode configuration
- **THEN** the captured pixels match the value computed by the CPU reference
  within the golden tolerance

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
