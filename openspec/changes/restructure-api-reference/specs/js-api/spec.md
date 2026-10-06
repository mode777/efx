# Spec Delta

## MODIFIED Requirements

### Requirement: Normative API reference document

The project SHALL maintain the script API's per-symbol reference as generated
output, not hand-written prose. The TypeScript declaration
`gallery/src/api/efx.d.ts` SHALL be the single source of truth for the
reference: its TSDoc comments define each symbol's summary, parameters,
return value, defaults, constraints, and examples. The reference SHALL be
produced from that declaration by a pinned generator and published in two
renderings: a Markdown rendering committed under `docs/api/`, and an HTML
rendering published on the gallery site under `/api`. The generated reference
SHALL be organized domain-first: it SHALL lead with the `efx` global and the
sub-namespaces it offers (`graphics`, `math`, `io`, `physics`, `keyboard`,
`mouse`, `gamepad`, `window`, `audio`, `color`) and SHALL group the remaining
symbols by domain rather than by reflection kind. Per-operation configuration
option types MAY be rendered inline in the operations that accept them
instead of as standalone index entries. Module-scoped authoring facilities
(`require`, `module`, `exports`, `__dirname`, `__filename`) SHALL NOT appear
in the reference. A short, curated hand-authored landing page MAY be merged
as the root of both renderings; per-symbol detail SHALL remain generated from
the declaration.

`docs/js-api.md` SHALL be maintained as the API **design guidelines**. It
SHALL state the design rules future API additions must follow — the single
namespace and its sub-namespace organization, the two-layer structure,
naming and option-bag conventions, units and colors, the error model, the
resource and memory model, the fixed limits, the lifecycle model (loading
`main.js` as the implicit init, explicit stacking hook registration with `dt`
and unsubscribe, and the load-time `update`/`render` sugar), the CommonJS
module model, and the gamepad, audio, math, io, and color namespace models —
and it SHALL direct readers to the generated reference for per-symbol detail
rather than cataloging every function itself.

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
  `AudioData`/`AudioStream`/`Audio` classes and their properties, the
  decoded-PCM-only rule, the no-device and web-unlock behavior, and the fixed
  limits

#### Scenario: Math, io, and color namespaces are documented
- **WHEN** the reference is read
- **THEN** it documents `efx.math` with `mat4`/`vec3`/`quat`, `efx.io` with
  `loadText`/`loadData`, and `efx.color` with its 17 named `Color` constants,
  and the guidelines state that `efx.args` is a read-only property

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
- **THEN** it states the design rules (namespace organization, two layers,
  conventions, units and colors, errors, resource model, limits, lifecycle,
  modules, and the gamepad/audio/math/io/color namespace models) and points to
  the generated reference for per-symbol detail

#### Scenario: No internal milestone tags
- **WHEN** a symbol entry in the generated reference is read
- **THEN** it carries no roadmap milestone tag and no provisional marker

#### Scenario: Hand-written per-function catalog is retired
- **WHEN** `docs/js-api.md` is searched for per-function signature catalogs
- **THEN** none are present; per-symbol detail lives only in the generated
  reference

#### Scenario: Reference leads with the efx global
- **WHEN** a reader opens the reference index
- **THEN** the `efx` global and the sub-namespaces it offers are presented
  before the supporting types, and the index is grouped by domain rather than
  by reflection kind

#### Scenario: Configuration types do not crowd the index
- **WHEN** a reader browses the reference index
- **THEN** single-use per-operation option types do not appear as standalone
  index entries, shared configuration bags are collected under a dedicated
  `Configuration` heading, and every option field remains reachable from the
  operations that accept it

#### Scenario: Authoring facilities are absent from the reference
- **WHEN** the reference is searched for `require`, `module`, `exports`,
  `__dirname`, or `__filename`
- **THEN** they are not documented as part of the public API

#### Scenario: Curated landing page introduces the API
- **WHEN** a reader opens the reference root
- **THEN** a curated introduction presents EFX and points into the generated
  reference, while per-symbol pages remain generated from the declaration
