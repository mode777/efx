# Spec Delta

## MODIFIED Requirements

### Requirement: Phong material model
A material is a **JS-managed** plain object (ADR 0011; no native class, no
`destroy()`) with four optional Phong channels plus an optional `unlit` flag.
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
  requirement).
- `unlit?` — an optional boolean (default `false`). When `true`, the surface
  is shaded without the lighting equation and without any light contribution:
  its color is the `diffuse` channel color multiplied by the `diffuse` map
  sample and the albedo (see the F4a lit shading requirement). `alphaMask`
  and `blend` still apply; `ambient`, `specular`, `emissive`, and every light
  are ignored.

Each channel `color` SHALL be a `[r, g, b, a]` array of normalized floats;
the alpha component SHALL be ignored by shading. `shininess` SHALL be a finite
number `> 0`. A `map` field and the material-level `alphaMask`, when present,
MUST be a live `Texture` or a live `RenderTarget` (F5a). A non-object
material, an unknown field, a wrong-typed or wrong-length `color`, a
non-number `shininess`, a wrongly-typed `map`/`alphaMask`, a non-boolean
`unlit`, or a destroyed `Texture` or destroyed `RenderTarget` passed as a map
SHALL throw `TypeError`; a `blend` that is present and is neither one of the
three mode strings nor `null` SHALL throw `TypeError`; a non-positive or
non-finite `shininess` SHALL throw `RangeError`; the call that was passed the
material SHALL record nothing. `map`/`alphaMask` were rejected as unknown in
F4a and are now accepted.

The engine SHALL **retain** each bound map's `Texture` or `RenderTarget`:
calling `destroy()` on the bound resource releases the script's handle, and
the native storage is released only once no material binding references it —
i.e. after the surface is rebound without that map, bound to `null`, or the
owning mesh is destroyed. A bound map therefore never becomes a dangling
handle. An omitted or `null` map means no modulation.

#### Scenario: Omitted channels take defaults
- **WHEN** `mesh.setSurfaceMaterial(0, {})` is called
- **THEN** the surface uses the default white-diffuse Phong material with no maps, no alpha mask, no unlit flag, and no blend override

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
- **THEN** fragments sampling mask alpha below `0.5` are not written, the rest show the lit surface with the albedo alpha, and the mask RGB is ignored

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

#### Scenario: Bound map texture is retained
- **WHEN** a live `Texture` is bound as a map and the script then calls `Texture.destroy()` while the binding is in effect, and later re-binds the surface without that map
- **THEN** drawing before the rebind still shades with the map and does not throw, and the native storage is released only after the rebind

#### Scenario: Bound map render target is retained
- **WHEN** a live `RenderTarget` is bound as a map, `destroy()` is called on it while the binding is in effect, and the surface is later rebound without that map
- **THEN** drawing before the rebind still shades with the target's contents and does not throw, and the native storage is released only after the rebind

#### Scenario: Invalid material throws
- **WHEN** a material has a `map` that is not a live `Texture` or live `RenderTarget` (a number, a destroyed texture, a destroyed render target), a channel color with three elements, or `specular: { shininess: 0 }`
- **THEN** the binding call throws `TypeError` / `RangeError` and the surface's previous binding is unchanged

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

When the material has `unlit: true`, the surface SHALL bypass the entire
lighting equation and the light bank: the final fragment color SHALL be the
**unlit color** `material.diffuse.color × M_diffuse × albedo`, clamped to
`[0, 1]`. The `alphaMask`, the albedo alpha, and the blend mode SHALL behave
exactly as in the lit path; `ambient`, `specular`, `emissive`, and every
point/directional light SHALL have no effect. A material with `unlit: true`
and no `diffuse.map` therefore renders its `diffuse` color scaled by the
albedo, and a material with no lights enabled but `unlit: false` continues to
render its lit result (which is black except for its emissive term).

A surface without `normals` SHALL use the F3 default normal `(0, 0, 1)` in
object space. A surface without `uvs` SHALL sample every map at the F3 default
`(0, 0)`. Absent maps contribute the neutral factor `1`, so a material that
binds no maps shades exactly as in F4a.

When the material has an `alphaMask`, the fragment SHALL sample
`texture(alphaMask, uv).a` and be **discarded** when that value is `< 0.5`;
otherwise the fragment is written and its alpha SHALL be the albedo alpha
(vertex-color alpha × tint alpha) — the mask does not modulate the output
alpha and no alpha blending or dithering is applied by the mask. Material
channel alphas remain ignored by shading.

2D quad records SHALL be unaffected and the committed F2 goldens SHALL remain
valid. Because absent maps are neutral and `unlit` defaults to `false`, the
committed F3 and F4a goldens SHALL remain byte-identical. With every light
disabled, a default-material surface renders black except for its emissive
term.

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

- **WHEN** a masked surface is drawn with a tint alpha below `1`
- **THEN** fragments that survive the mask keep the tint-modulated albedo
  alpha, and the mask's own alpha does not alter it

#### Scenario: Unlit ignores lights and the lit channels

- **WHEN** a surface with `unlit: true`, a `diffuse` color and map, and a
  non-black ambient is drawn with lights enabled and again with every light
  disabled
- **THEN** both renders are identical and equal the diffuse color × map ×
  albedo, with no ambient/specular/emissive contribution

#### Scenario: Unlit still honors alpha mask and blend

- **WHEN** an unlit surface with an `alphaMask` and `blend: 'additive'` is
  drawn
- **THEN** fragments below the mask threshold are discarded and the survivors
  blend additively, exactly as in the lit path
