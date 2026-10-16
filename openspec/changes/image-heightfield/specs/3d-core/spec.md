# Spec Delta: 3d-core

## ADDED Requirements

### Requirement: Image heightfield primitive

The engine SHALL provide `efx.graphics.makeHeightfield(image, opts?)` as a
member of the `make*` primitive family. `image` SHALL be a required
positional `EfxImageData`; the result SHALL be a single-surface MeshData
(directly usable by `createMesh`) built from the image's decoded RGBA8
pixels, with pinned defaults so scenes are reproducible:

- Each grid vertex's height SHALL be the source pixel's **luminance**
  (Rec. 709: `0.2126·R + 0.7152·G + 0.0722·B` on 0..1-normalized channels,
  alpha ignored) multiplied by `opts.height` (default `1`): luminance 0 maps
  to y = 0, luminance 1 maps to y = `height`.
- The source area SHALL be `opts.region` — a `[x, y, w, h]` pixel sub-rect
  (default: the whole image). The region SHALL be clamped to the image
  bounds before sampling; a region that is empty or degenerate after
  clamping SHALL throw `RangeError`.
- `opts.step` (default `1`) SHALL be a positive integer decimation stride:
  the grid samples every `step`-th pixel in x and y starting at the region
  origin, giving `cols = floor((regionW − 1) / step) + 1` columns and rows
  analogously. Fewer than 2 columns or 2 rows SHALL throw `RangeError`.
- The field SHALL lie in the XZ plane facing `+Y` (like `makePlane`),
  centered on the origin, with outward-facing (CCW seen from `+Y`) winding.
  The x extent SHALL be `opts.size` (default: the region's pixel width, i.e.
  1 world unit per pixel); the z extent SHALL follow the region's aspect
  ratio (`size · regionH / regionW`) so texels stay square.
- uv coordinates SHALL map each vertex's source pixel position normalized
  over the **full image** (`px / (imgW − 1)`, `py / (imgH − 1)`), so a
  full-image default call spans exactly 0..1 and a region call keeps the
  world-texture alignment of the un-cropped image.
- Vertex normals SHALL follow the sampled surface: interior vertices use
  central differences over the neighboring sampled heights (scaled by the
  world grid spacing), boundary vertices use one-sided differences, all
  normalized. A locally flat area SHALL produce an exactly upward
  `(0, 1, 0)` normal.

Identical image bytes, region, step, `height`, and `size` SHALL produce
byte-identical MeshData vertex streams on every target platform.

Option validation SHALL follow the primitive family rules: a non-`EfxImageData`
or destroyed `image` SHALL throw `TypeError`; non-object `opts` SHALL throw
`TypeError`; a non-integer or negative `region` component (with the possible
exception that `w`/`h` must be positive), a non-positive or non-integer
`step`, a non-positive or non-finite `size`, or a non-finite `height` SHALL
throw `RangeError`; a non-boolean-free unknown field SHALL throw `TypeError`;
a non-material `material` SHALL throw `TypeError`. When `material` is
present it SHALL be bound to the primitive's single surface at MeshData
creation exactly as in the procedural-primitives family (snapshot at call
time; omitted/`null` selects the engine default).

#### Scenario: Whole-image heightfield defaults

- **WHEN** `efx.graphics.makeHeightfield(img)` is called on a 4×4 grayscale
  ramp image with no options
- **THEN** the result is a single-surface MeshData with a 4×4 vertex grid
  (16 vertices, 18 triangles), vertex heights equal to each pixel's
  luminance, the field centered on the origin spanning
  `(w − 1) × (h − 1)` world units, uv spanning 0..1, and the upload/draw
  path identical to any other MeshData

#### Scenario: Region and step sample a subarea

- **WHEN** `makeHeightfield(img, { region: [4, 2, 10, 6], step: 2 })` is
  called on a 16×12 image
- **THEN** the grid samples pixels (4,2), (6,2), … in x and (4,2),
  (4,4), … in y — 5 columns × 3 rows — with heights taken from those
  pixels, and uv coordinates mapping that sub-rect of the full image, not
  a 0..1 wrap of the region

#### Scenario: Height and size scale the field

- **WHEN** `makeHeightfield(img, { height: 5, size: 20 })` is called on a
  64×32 image
- **THEN** vertex y spans exactly 0..5 following luminance, the x extent is
  20 world units, and the z extent is 10 (20 · 32/64)

#### Scenario: Normals follow the surface

- **WHEN** the field is drawn lit, and separately a flat-luminance image is
  used
- **THEN** slopes tilt normals consistently with their sampled height
  differences and every vertex of the flat image carries the exact normal
  `(0, 1, 0)`

#### Scenario: Invalid arguments throw

- **WHEN** `makeHeightfield` is called with a non-image first argument, a
  destroyed image, `region: [0, 0, 4]`, `region: [0.5, 0, 4, 4]`,
  `step: 0`, `step: 1.5`, `size: 0`, `height: Infinity`, an unknown option
  field, or a non-material `material`
- **THEN** the call throws (`TypeError` for the image/type/unknown-field
  cases, `RangeError` for the numeric bounds and the post-clamp degenerate
  region)

#### Scenario: Heightfield binds its material at creation

- **WHEN** `makeHeightfield(img, { material })` is called and the result is
  passed to `createMesh`
- **THEN** the single surface carries `material` (snapshotted at call time)
  and the Mesh renders it, while an omitted or `null` `material` renders
  with the engine default material

#### Scenario: Heightfield generation is deterministic

- **WHEN** the same image bytes are converted twice with the same options
  (including across target platforms)
- **THEN** the resulting vertex position, normal, and uv streams are
  byte-identical
