# Spec Delta

## MODIFIED Requirements

### Requirement: Phong material model
A material is a **JS-managed** plain object (ADR 0011; no native class, no
`destroy()`) with four optional Phong channels. Channel values are
snapshotted by the engine when the material is bound — later mutation of
the script object MUST NOT change the bound material. Channel maps are held
by **handle** (ADR 0019) and the referenced Textures and RenderTargets are
retained by the engine while bound (see below). Omitted channels or fields
take documented defaults:

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

Each channel `color` SHALL be a `[r, g, b, a]` array of normalized floats;
the alpha component SHALL be ignored by shading. `shininess` SHALL be a finite
number `> 0`. A `map` field and the material-level `alphaMask`, when present,
MUST be a live `Texture` or a live `RenderTarget` (F5a). A non-object
material, an unknown field, a wrong-typed or wrong-length `color`, a
non-number `shininess`, a wrongly-typed `map`/`alphaMask`, or a destroyed
`Texture` or destroyed `RenderTarget` passed as a map SHALL throw
`TypeError`; a `blend` that is present and is neither one of the three mode
strings nor `null` SHALL throw `TypeError`; a non-positive or non-finite
`shininess` SHALL throw `RangeError`; the call that was passed the material
SHALL record nothing. `map`/`alphaMask` were rejected as unknown in F4a and
are now accepted.

The engine SHALL **retain** each bound map's `Texture` or `RenderTarget`:
calling `destroy()` on the bound resource releases the script's handle, and
the native storage is released only once no material binding references it —
i.e. after the surface is rebound without that map, bound to `null`, or the
owning mesh is destroyed. A bound map therefore never becomes a dangling
handle. An omitted or `null` map means no modulation.

#### Scenario: Omitted channels take defaults
- **WHEN** `mesh.setSurfaceMaterial(0, {})` is called
- **THEN** the surface uses the default white-diffuse Phong material with no maps, no alpha mask, and no blend override

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
