# Spec Delta

## ADDED Requirements

### Requirement: Argument passing convention
Every public script API function SHALL follow one argument convention:
**required inputs SHALL be positional arguments and all optional inputs SHALL
be fields of a single trailing options bag**. A call with no optional inputs
SHALL omit the bag entirely. Every field of an options bag SHALL be optional;
a bag SHALL NOT carry a required input. A lone optional input MAY be passed
positionally instead of in a bag. Required fields are permitted inside a
**record/value** argument — a shape, a light descriptor, a pose sample, a
source rectangle, or a batch element — because such an argument is itself a
required positional value, not the function's options bag. Argument order
SHALL be consistent: the subject (the thing being drawn, created, or queried)
first, then required resources/selectors, then required scalars (2D `x`, `y`)
or vectors (3D position), then the options bag. `efx.log(msg?)` and
`efx.quit(code?)` are the sole lone-optional lifecycle facilities and keep
their positional optional argument. The convention SHALL be stated in
`docs/js-api.md`, and the declaration `gallery/src/api/efx.d.ts` SHALL express
it: a required input SHALL NOT appear as a field of an options bag.

#### Scenario: A required input is a positional argument
- **WHEN** a script creates a render target or a body
- **THEN** the required dimensions/`shape` are passed positionally
  (`createRenderTarget(width, height)`, `createBody(shape, opts?)`) and an
  empty options bag cannot omit them

#### Scenario: Optional inputs are bagged
- **WHEN** a function has two or more optional inputs (for example
  `createParticleSystem(texture, max, lifetime, opts?)`)
- **THEN** those inputs are fields of the trailing `opts` bag and each may be
  omitted to take its documented default

#### Scenario: A lone optional input may stay positional
- **WHEN** a function has exactly one optional input and no bag
  (`efx.quit(code?)`)
- **THEN** the call is valid with or without that positional argument

#### Scenario: Record arguments may carry required fields
- **WHEN** a shape, light descriptor, pose sample, source rectangle, or batch
  element is passed as a required positional argument
- **THEN** its required fields are permitted, because it is a value rather
  than the function's options bag

#### Scenario: The thing drawn leads the argument list
- **WHEN** a 2D or 3D draw function is called
- **THEN** the subject being drawn is the first argument
  (`drawQuad(texture, x, y, opts?)`, `drawBillboard(texture, pos, opts?)`),
  followed by required position and then the options bag

#### Scenario: Guidelines state the convention
- **WHEN** `docs/js-api.md` is read
- **THEN** it states the required-positional / optional-bag rule, the
  lone-optional allowance, the record-versus-bag boundary, and the ordering
  rule, and points to the generated reference for per-symbol detail

## MODIFIED Requirements

### Requirement: Font and text API

The script API SHALL expose font loading, font creation, text drawing, and
text measurement as C-implemented members of the `efx.graphics`
sub-namespace, with identical names, signatures, semantics, and error
behavior across the desktop and web bindings:

- `efx.graphics.loadFontData(path)` → a native-backed `FontData` resource (the parsed
  font, no GPU resource), released by `destroy()`.
- `efx.graphics.createFont(fontData, size, opts?)` → a native-backed `Font` that bakes a
  fixed glyph atlas at the requested size and optional baked outline/shadow
  effects; released by `destroy()`; read-only `size`, `lineHeight`, `ascent`,
  `descent`.
- `efx.graphics.drawText(text, font, x, y, opts?)` → lays out and draws the text as
  display-list quads and returns the laid-out bounds
  `{ width, height, lines }`.
- `efx.graphics.measureText(text, font, opts?)` → returns the same bounds without
  drawing.

Text drawing SHALL be a mid-level C facility (like `drawQuad`/`drawMesh`) —
not a pure-JS high-level convenience — and SHALL be 2D-only. Formatting SHALL
be limited to newlines, greedy word wrapping, horizontal alignment
(`left`/`center`/`right`/`justify`), and vertical alignment
(`top`/`middle`/`bottom`); there SHALL be no rich text (per-span styles or
markup), no 3D/world-space text, and no script-visible glyph metrics, atlas,
or shader. The font/text behavior SHALL be defined by the `font-text`
capability. `docs/js-api.md` and the gallery type document
(`gallery/src/api/efx.d.ts`) SHALL be updated in the same change, and the
provisional `loadFont` entry SHALL be removed.

#### Scenario: Font pipeline is exposed
- **WHEN** the API reference is read after this change
- **THEN** it catalogs `loadFontData`, `createFont`, `drawText`, and `measureText` under `efx.graphics`, each tagged C-implemented and F8, and does not catalog a `loadFont` convenience

#### Scenario: Text drawing is mid-level C
- **WHEN** a reviewer checks the layer of the text entries
- **THEN** `drawText` and `measureText` are tagged C-implemented mid-level functions, distinct from the pure-JS high-level layer

#### Scenario: No rich text or 3D text
- **WHEN** the API reference's font/text section is read
- **THEN** it documents wrapping and the four horizontal and three vertical alignment modes, and states that rich text, 3D text, and script-visible glyph metrics are not provided

#### Scenario: Type document is updated in the same change
- **WHEN** this change updates `docs/js-api.md`
- **THEN** `gallery/src/api/efx.d.ts` (and its type-test) declare `FontData`, `Font`, `drawText`, and `measureText` consistently with the reference

#### Scenario: createFont takes size positionally
- **WHEN** `createFont` is called with a live `FontData`, a positive `size`, and an optional trailing bag
- **THEN** the font bakes at `size`; a missing or non-positive `size` throws, and the bag carries only optional fields (`glyphs`, `padding`, `filter`, `outline`, `shadow`)

### Requirement: Billboard, sprite-batch, and particle API

The script API SHALL expose world-space billboard drawing, batched 2D sprite
drawing, and CPU particle systems as C-implemented members of the
`efx.graphics` sub-namespace, with identical names, signatures, semantics,
and error behavior across the desktop and web bindings:

- `efx.graphics.drawBillboard(texture, pos, opts?)` → records one world-space textured
  quad with its source `texture` leading, at a 3D position. `opts` carries
  `size`, `color`, `sourceRect`, `rotation`, `facing` (`'view'` default or
  `'y'`), `normal`, and `depthTest`, per the `billboards` capability.
- `efx.graphics.drawSprites(texture, sprites)` → records one 2D textured quad per entry
  with `drawQuad` semantics, per the `2d-layer` capability.
- `efx.graphics.createParticleSystem(texture, max, lifetime, opts?)` → a native-backed
  `ParticleSystem`; the source texture, particle capacity, and lifetime are
  positional required inputs and `opts` carries the remaining optional
  configuration.
- `efx.graphics.drawParticles(sys)` → records one particle batch for a live system.

`ParticleSystem` SHALL be a native-backed class exposing a read-only `count`,
a read-write `speedScale`, an `emit(n)` burst, `start`/`stop`/`pause`/`reset`,
a `set(opts)` partial reconfiguration, and `destroy()` with a GC finalizer
backstop. `drawSprites` SHALL be 2D-only; `drawBillboard` and `drawParticles`
SHALL use the 3D camera. The `js-api` reference (`docs/js-api.md`) and the
gallery type document (`gallery/src/api/efx.d.ts`) SHALL be updated in the same
change, and the particle pool limit and the `ParticleSystem` class SHALL be
reflected in the fixed-limits table and the native-backed class list.

#### Scenario: New entries are cataloged

- **WHEN** the API reference is read after this change
- **THEN** it catalogs `drawBillboard`, `drawSprites`, `createParticleSystem`,
  and `drawParticles` under `efx.graphics`, each tagged C-implemented and F11

#### Scenario: Billboard is a 3D primitive

- **WHEN** a script calls `drawBillboard(texture, pos, { facing: 'view' })` under a 3D camera
- **THEN** the quad is placed and oriented in world space from the recorded 3D
  camera, with no camera state supplied by the script

#### Scenario: Sprite batch is 2D-only

- **WHEN** a script calls `drawSprites(tex, [{ x, y }])`
- **THEN** the sprites are recorded as 2D quads in the current 2D frame and are
  not depth-tested or 3D-oriented

#### Scenario: Particle system is exposed

- **WHEN** a script calls `createParticleSystem(texture, max, lifetime, opts?)` and reads the returned
  object
- **THEN** it is a `ParticleSystem` with a read-only `count` and the documented
  methods, and `drawParticles` accepts it

#### Scenario: Reference and type document are updated

- **WHEN** this change updates `docs/js-api.md`
- **THEN** `gallery/src/api/efx.d.ts` declares `drawBillboard`, `drawSprites`,
  `ParticleSystem`, `createParticleSystem`, and `drawParticles` consistently
  with the reference

### Requirement: Physics namespace API

The script API SHALL expose collision, character movement, and linear impulse
dynamics through a single `efx.physics` sub-namespace of the `efx` object,
adding no free globals. Every entry SHALL be C-implemented and SHALL have
identical names, signatures, semantics, and error behavior across the desktop
and web bindings. The sub-namespace SHALL provide:

- world configuration: read/write `gravity` (a `[x, y, z]` vector, default
  `[0, -9.81, 0]`) and `iterations` (a positive integer, default `8`);
- `step(dt)` — advance the dynamic simulation by `dt` seconds (script-owned;
  the engine SHALL NOT step the world itself);
- `clear()` — remove every collider;
- `createBody(shape, opts?)`, `createCharacter(radius, height, opts?)`, and
  `createStaticMesh(mesh, opts?)` — resource factories returning native-backed
  classes; the required collider shape / capsule dimensions are positional and
  the trailing bag carries only optional configuration;
- `raycast`, `overlap`, and `shapeCast` — spatial queries, defined by the
  `physics-queries` capability.

`Body` and `Character` SHALL be classified as native-backed classes under the
resource-classification requirement. `Body` SHALL expose `position`,
`velocity`, `contacts`, and a `transform` (a flat 16-number column-major
translation matrix, directly usable by `drawMesh`), `applyImpulse`, and
`applyForce`; `Character` SHALL expose `position`, `velocity`, `onFloor`, and
`moveAndSlide`. Shapes SHALL be plain JS option objects (sphere, box, capsule,
mesh) accepted positionally by bodies and by queries. Validation SHALL follow
the engine's convention: unknown fields, unknown shape/body kinds, and wrong
types throw `TypeError`; out-of-range numeric values throw `RangeError`. The
physics classes SHALL be documented in `docs/js-api.md` and typed in the
gallery type document (`gallery/src/api/efx.d.ts`), both updated in the same
change, with their entries tagged with milestone F12. The `docs/js-api.md`
resource, lifecycle, and error sections SHALL cover the new sub-namespace, and
the type document SHALL reject invalid call shapes (unknown option fields,
mixing static/dynamic-only options).

#### Scenario: Physics namespace is reachable without setup

- **WHEN** a script calls `efx.physics.step(1/60)` and `efx.physics.raycast`
  without imports
- **THEN** both succeed on every target, and no additional free global exists

#### Scenario: World configuration is validated

- **WHEN** `efx.physics.gravity` is set to a non-3-number value or
  `efx.physics.iterations` to a non-positive integer
- **THEN** the assignment throws (`TypeError` for the shape, `RangeError` for
  the range) and the previous value remains in effect

#### Scenario: Factories return classified resources

- **WHEN** `createBody(shape, opts?)` and `createCharacter(radius, height, opts?)` are called with valid required inputs
- **THEN** they return `Body` and `Character` instances whose class entries in
  the reference state the native-backed classification and the documented
  read-only properties

#### Scenario: Body transform feeds drawMesh

- **WHEN** a dynamic body's `transform` is passed as the `transform` option of
  `drawMesh`
- **THEN** the mesh draws at the body's world position

#### Scenario: Reference and type document are updated

- **WHEN** this change lands
- **THEN** `docs/js-api.md` catalogs every `efx.physics` entry and the
  `Body`/`Character` classes with their signatures, layer tags, and the F12
  milestone, and `gallery/src/api/efx.d.ts` declares the same surface,
  including the new native-backed classes

#### Scenario: Invalid options are rejected

- **WHEN** `createBody` receives an unknown option field or a shape of unknown
  type
- **THEN** the call throws `TypeError` and creates nothing

### Requirement: Gallery type document accuracy

The gallery type document `gallery/src/api/efx.d.ts` SHALL accurately
describe the valid call shapes of the public script API: every documented
call form MUST type-check, and invalid calls MUST be rejected at compile
time. It SHALL express the argument convention — required inputs positional,
optional inputs in a trailing all-optional bag — so that a required input
cannot be omitted from an options bag and an unknown or optional-only bag
field is rejected. `createMeshData` SHALL type one positional surface list
(`MeshSurfaceData[]`) plus an optional positional materials array, with no
single-surface shorthand form. It SHALL be updated in the same change as any
script-facing API change, alongside `docs/js-api.md`, and its declarations
MUST agree with that reference document. Because it is the source of truth
for the generated reference, the committed Markdown reference `docs/api/`
SHALL be regenerated from it in that same change. The document SHALL type the
`efx.math`, `efx.io`, and `efx.color` sub-namespaces and the read-only
`efx.args` property, and SHALL reject the removed root members.

#### Scenario: Batch form does not require shorthand fields

- **WHEN** `efx.graphics.createMeshData([surface, surface])` is type-checked
- **THEN** it compiles with the surface list positional and no single-surface
  shorthand attributes

#### Scenario: Mixing construction forms is rejected

- **WHEN** a call supplies the removed single-surface shorthand bag (a bare
  surface object instead of a surface list) alongside or instead of the
  positional surface list
- **THEN** the type document reports a compile-time error

#### Scenario: Required inputs are typed positionally

- **WHEN** `efx.graphics.createRenderTarget(512, 256)` and
  `efx.physics.createBody(shape, { dynamic: true })` are type-checked
- **THEN** both compile, while an empty bag cannot supply the required
  dimensions/`shape` and such a call is rejected

#### Scenario: drawMesh takes a positional mesh

- **WHEN** `efx.graphics.drawMesh(mesh, { transform, color, skinned })` and
  `efx.graphics.drawMesh(mesh)` are type-checked
- **THEN** both compile, and the former option bag holds only
  `transform`/`color`/`skinned` (a `mesh` field in the bag is rejected)

#### Scenario: Posing API is typed

- **WHEN** `efx.graphics.poseMesh(mesh, { clip: 'Walk', time: 1 })` and
  `efx.graphics.poseMesh(mesh, [{ clip: 0, time: 1, weight: 0.5 }])` are type-checked
- **THEN** both compile, the sample `clip` accepts a name or index, and an
  unknown sample field is rejected

#### Scenario: Primitive material option is typed

- **WHEN** `efx.graphics.makeCube({ size: 1, material })` is type-checked with a
  material object
- **THEN** it compiles and the material argument is accepted as a material
  object or `null`

#### Scenario: New namespaces and removed roots are typed

- **WHEN** `efx.math.mat4.identity()`, `efx.io.loadData(path)`,
  `efx.color.white`, and reading `efx.args` are type-checked, and
  `efx.mat4`, `efx.loadText`, and `efx.args()` are type-checked
- **THEN** the first four compile and the last three are rejected at compile
  time

#### Scenario: Type document agrees with the reference

- **WHEN** a script-facing API change updates `gallery/src/api/efx.d.ts`
- **THEN** the same change regenerates `docs/api/` so the published reference
  matches the declaration
