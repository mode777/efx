# Spec Delta: js-api

## MODIFIED Requirements

### Requirement: Two-layer API with strict layering

The script API SHALL consist of exactly two layers: low/mid-level functions
implemented in C/C++ and registered through the engine binding, and
high-level convenience functions implemented in pure ES6. High-level
functions MUST be implemented using only the public low/mid-level API and
standard ES6 built-ins — they MUST NOT use private bindings or host
facilities that are not part of the public API. The API design guidelines
SHALL state this two-layer rule. Because the layer is an implementation
concern and the reference is end-user facing, the generated per-symbol
reference SHALL NOT be required to tag entries by layer.

#### Scenario: High-level function built on public API
- **WHEN** a high-level convenience function (e.g. a model or text drawer) is
  implemented
- **THEN** it calls only documented public API functions and standard ES6

#### Scenario: Layer tag present
- **WHEN** the API design guidelines are read
- **THEN** they state the two-layer structure (C-implemented versus pure-JS)
  and that high-level functions build only on the public low/mid-level API,
  and no generated per-symbol entry carries a layer tag

### Requirement: Normative API reference document

The project SHALL maintain the script API's per-symbol reference as generated
output, not hand-written prose. The TypeScript declaration
`gallery/src/api/efx.d.ts` SHALL be the single source of truth for the
reference: its TSDoc comments define each symbol's summary, parameters,
return value, defaults, constraints, and examples. The reference SHALL be
produced from that declaration by a pinned generator and published in two
renderings: a Markdown rendering committed under `docs/api/`, and an HTML
rendering published on the gallery site under `/api`.

`docs/js-api.md` SHALL be maintained as the API **design guidelines**. It
SHALL state the design rules future API additions must follow — the single
namespace, the two-layer structure, naming and option-bag conventions, units
and colors, the error model, the resource and memory model, the fixed limits,
the lifecycle model (loading `main.js` as the implicit init, explicit
stacking hook registration with `dt` and unsubscribe, and the load-time
`update`/`render` sugar), the CommonJS module model, and the gamepad
namespace model — and it SHALL direct readers to the generated reference for
per-symbol detail rather than cataloging every function itself.

Any change that adds, modifies, or removes a public API function SHALL update
the declaration in the same change and SHALL regenerate the committed
Markdown reference from it. The generated Markdown SHALL NOT be hand-edited.
The reference SHALL NOT tag entries by internal roadmap milestone, and
entries SHALL NOT be marked provisional.

#### Scenario: Callable-today vs planned is distinguishable
- **WHEN** a reader opens the reference
- **THEN** every documented symbol describes current shipped behavior, and no
  entry is marked provisional

#### Scenario: Milestone change updates the reference
- **WHEN** a feature change adds or changes an API function
- **THEN** the same change updates the declaration and regenerates
  `docs/api/`, with no internal milestone tag added to the reference

#### Scenario: Input namespaces are documented
- **WHEN** the reference is read
- **THEN** it documents `efx.keyboard`, `efx.mouse`, and `efx.window` with
  their query functions, event registrations, read-only properties, the
  key/button name set, and the surface-pixel coordinate rule

#### Scenario: Module model is documented
- **WHEN** the API design guidelines are read
- **THEN** they document the CommonJS module format, the synchronous resolver
  and its supported/unsupported specifier forms, module caching and cycles,
  JSON modules, the module-shaped entry hooks, and that Node/npm
  compatibility is not provided

#### Scenario: Particle, billboard, and sprite API is documented
- **WHEN** the reference is read
- **THEN** it documents `drawBillboard`, `drawSprites`, `createParticleSystem`,
  and `drawParticles` with their options, error behavior, the `ParticleSystem`
  class and its lifecycle, and the `facing` render modes

#### Scenario: Gamepad namespace is documented
- **WHEN** the reference is read
- **THEN** it documents `efx.gamepad` with its `count`/`get`, the pad view's
  query methods and read-only properties, the semantic button/axis name sets,
  the canonical range and trigger threshold, and the raw fallback

#### Scenario: Audio namespace is documented
- **WHEN** the reference is read
- **THEN** it documents `efx.audio` with its entry points, the
  `SoundData`/`Sound`/`Music` classes and their properties, the
  decoded-PCM-only rule, the no-device and web-unlock behavior, and the fixed
  limits

#### Scenario: Catalog derived from vision
- **WHEN** the guidelines' vision traceability is checked against vision.md
- **THEN** every capability vision.md names for the consumer API (2D quads,
  meshes, vertex colors, cameras, lights, Phong materials with maps, alpha
  masks, blending modes, render targets, post FX, resource loading,
  keyboard/mouse input query and events, gamepad input, audio playback,
  script modules, skinning/animation, high-level text drawing) has a
  corresponding documented symbol or an explicitly noted open question

#### Scenario: Reference is generated, not hand-written
- **WHEN** a reader consults the per-symbol API reference
- **THEN** it was generated from `gallery/src/api/efx.d.ts`, the committed
  Markdown lives under `docs/api/`, and the published HTML is served under
  `/api`

#### Scenario: API change updates declaration and reference
- **WHEN** a change adds, modifies, or removes a public API function
- **THEN** the same change updates `gallery/src/api/efx.d.ts` and regenerates
  `docs/api/` from it

#### Scenario: Committed reference cannot go stale
- **WHEN** the committed `docs/api/` is compared with a fresh generation from
  the declaration
- **THEN** any difference fails verification

#### Scenario: Guidelines carry the design rules
- **WHEN** `docs/js-api.md` is read
- **THEN** it states the namespace, layering, conventions, units/colors,
  error model, resource and memory model, fixed limits, lifecycle, module,
  and gamepad design rules, and points to the generated reference rather than
  cataloging every function

#### Scenario: No internal milestone tags
- **WHEN** a symbol entry in the generated reference is read
- **THEN** it carries no roadmap milestone tag and no provisional marker

#### Scenario: Hand-written per-function catalog is retired
- **WHEN** `docs/js-api.md` is searched for per-function signature catalogs
- **THEN** none are present; per-symbol detail lives only in the generated
  reference

### Requirement: Gallery type document accuracy

The gallery type document `gallery/src/api/efx.d.ts` SHALL accurately
describe the valid call shapes of the public script API: every documented
call form MUST type-check, and invalid calls MUST be rejected at compile
time. It MUST type a function's alternative call forms so that each form's
required fields are required only for that form (for example, the
`createMeshData` batch bag versus its single-surface shorthand) and so that
mixing forms is rejected. It SHALL be updated in the same change as any
script-facing API change, and because it is the source of truth for the
generated reference, the committed Markdown reference `docs/api/` SHALL be
regenerated from it in that same change.

#### Scenario: Batch form does not require shorthand fields

- **WHEN** `efx.createMeshData({ surfaces: [surface, surface] })` is
  type-checked
- **THEN** it compiles without supplying top-level `positions` or other
  shorthand attributes

#### Scenario: Mixing construction forms is rejected

- **WHEN** `efx.createMeshData({ surfaces: [surface], positions })` combines
  the batch bag and the shorthand fields in one call
- **THEN** the type document reports a compile-time error

#### Scenario: drawMesh takes a positional mesh

- **WHEN** `efx.drawMesh(mesh, { transform, color, skinned })` and
  `efx.drawMesh(mesh)` are type-checked
- **THEN** both compile, and the former option bag holds only
  `transform`/`color`/`skinned` (a `mesh` field in the bag is rejected)

#### Scenario: Posing API is typed

- **WHEN** `efx.poseMesh(mesh, { clip: 'Walk', time: 1 })` and
  `efx.poseMesh(mesh, [{ clip: 0, time: 1, weight: 0.5 }])` are type-checked
- **THEN** both compile, the sample `clip` accepts a name or index, and an
  unknown sample field is rejected

#### Scenario: Primitive material option is typed

- **WHEN** `efx.makeCube({ size: 1, material })` is type-checked with a
  material object
- **THEN** it compiles and the material argument is accepted as a material
  object or `null`

#### Scenario: Type document agrees with the reference

- **WHEN** a script-facing API change updates `gallery/src/api/efx.d.ts`
- **THEN** the same change regenerates `docs/api/` so the published reference
  matches the declaration
