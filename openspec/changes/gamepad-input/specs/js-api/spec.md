# Spec Delta

## ADDED Requirements

### Requirement: Gamepad namespace API

The script API SHALL expose gamepad input as the sub-namespace `efx.gamepad`
of the single `efx` object, with no new free globals. Every entry SHALL be
C-implemented and SHALL have identical names, signatures, semantics, and error
behavior across the desktop and web bindings. Gamepad exposes **no resource
types**: pads are a fixed, engine-owned bank reported by index; scripts SHALL
NOT create, destroy, or own a pad, and the native-backed class list is
unchanged.

`efx.gamepad` SHALL provide:
- `count` — the number of connected pads.
- `get(index)` — the pad view for slot `index`, or `null` when none is
  connected.
- `onConnect(fn)` and `onDisconnect(fn)` — register a callback receiving the
  pad view and return an unsubscribe function.

A pad view SHALL provide:
- `connected` — whether the slot currently has a pad.
- `name` — the device name.
- `mapped` — whether a semantic mapping was found.
- `isDown(button)`, `isPressed(button)`, `isReleased(button)` — level, press
  edge, and release edge for a semantic button name, returning a boolean.
- `axis(axis)` — the normalized value of a semantic axis name.
- `rawButton(i)` and `rawAxis(i)` — the raw device values by index, for
  unmapped pads.

Semantic button names SHALL be the engine-owned set `south`, `east`, `west`,
`north`, `leftShoulder`, `rightShoulder`, `leftTrigger`, `rightTrigger`,
`back`, `start`, `guide`, `leftStick`, `rightStick`, `dpadUp`, `dpadDown`,
`dpadLeft`, `dpadRight`. Semantic axis names SHALL be `leftX`, `leftY`,
`rightX`, `rightY`, `leftTrigger`, `rightTrigger`. Axis values SHALL use one
canonical documented range on every target, and each trigger's digital button
SHALL derive from its axis using a documented threshold. A query with an
unknown button or axis name SHALL throw `TypeError`; registration SHALL
require a function and SHALL throw `TypeError` otherwise, and each
registration SHALL return an unsubscribe function with the same idempotent
semantics as the lifecycle hooks. Event callbacks SHALL receive a single
plain JS object — never a host or DOM object. The namespace SHALL be
documented in `docs/js-api.md` and typed in the gallery type document, both
updated in the same change.

#### Scenario: Pad queries and events are available
- **WHEN** a script reads `efx.gamepad.count`, calls `efx.gamepad.get(0).axis('leftX')`, and registers `efx.gamepad.onConnect`
- **THEN** the queries reflect current state and the callback fires when a pad connects

#### Scenario: Unknown semantic name is rejected
- **WHEN** a script calls `pad.isDown('notabutton')` or `pad.axis('leftZ')`
- **THEN** the call throws `TypeError`

#### Scenario: Unsubscribe removes a gamepad callback
- **WHEN** a script registers a connect or disconnect callback and calls the returned unsubscribe function
- **THEN** the callback no longer fires, and calling unsubscribe again is a no-op

#### Scenario: Event objects are plain data
- **WHEN** a gamepad callback receives its argument
- **THEN** it is a plain JS object holding only engine-provided primitives, with no host/DOM object and no methods

#### Scenario: No resources added
- **WHEN** the API reference's resource classes and fixed limits are read after this change
- **THEN** pads are a fixed engine-owned bank reported by index with no `create`, no `destroy`, and no new native-backed class

## MODIFIED Requirements

### Requirement: Normative API reference document

The project SHALL maintain `docs/js-api.md` as the normative, developer-facing
reference of the entire script API. It SHALL contain an entry for every public
API function with a signature sketch, a description, its layer tag, and the
roadmap milestone (F1–F13) that delivers it. Entries for functions whose
milestone has not passed its verification gate SHALL be explicitly marked
provisional. The document SHALL also document the lifecycle model — loading `main.js`
as the implicit init, with the `efx` namespace and engine API ready before it
executes (the rendering surface is initialized when the frame loop starts and
is not script-visible at load time), plus explicit, stacking hook registration
(`registerUpdateHook` / `registerRenderHook`, update hooks receiving `dt`,
unsubscribe returned, F1 globals as load-time sugar) — and how API errors
surface (exceptions, exit codes). It SHALL document the CommonJS module model
(F10): the module format and synchronous `require` resolution, module caching
and cycles, `module.exports`/`exports`, the JSON-module form, the restricted
resolver and unsupported specifiers, the module-shaped `update`/`render`
exports, and the unchanged no-browser/Node-dependency rule. It SHALL document
the gamepad namespace (F13): the pad bank and `count`/`get`, the pad view and
its query methods and read-only properties, the semantic button/axis name
sets, the canonical axis range and trigger threshold, the raw fallback, and
the error behavior. Any change that
adds, modifies, or removes a public API function
MUST update the document in the same change.

#### Scenario: Callable-today vs planned is distinguishable
- **WHEN** a reader opens the reference
- **THEN** the F1 functions (`efx.log`, `efx.quit`, `efx.args`,
  `efx.registerUpdateHook`, `efx.registerRenderHook`) are presented as current
  behavior, and later-milestone entries are marked provisional

#### Scenario: Milestone change updates the reference
- **WHEN** a feature change adds or changes an API function
- **THEN** the same change contains the matching `docs/js-api.md` update with
  the function's signature, layer, and milestone tags

#### Scenario: Input namespaces are documented
- **WHEN** the reference is read after this change
- **THEN** it catalogs `efx.keyboard`, `efx.mouse`, and `efx.window` with
  their query functions, event registrations, read-only properties, the
  key/button name set, the surface-pixel coordinate rule, and the F9 milestone
  tag

#### Scenario: Module model is documented
- **WHEN** the reference is read after this change
- **THEN** it documents the CommonJS module format, the synchronous resolver
  and its supported/unsupported specifier forms, module caching and cycles,
  JSON modules, the module-shaped entry hooks, and the F10 milestone tag, and
  states that Node/npm compatibility is not provided

#### Scenario: Particle, billboard, and sprite API is documented
- **WHEN** the reference is read after this change
- **THEN** it catalogs `drawBillboard`, `drawSprites`, `createParticleSystem`,
  and `drawParticles` with their options, error behavior, the `ParticleSystem`
  class and its lifecycle, the `facing` render modes, and the F11 milestone
  tag

#### Scenario: Gamepad namespace is documented
- **WHEN** the reference is read after this change
- **THEN** it catalogs `efx.gamepad` with its `count`/`get`, the pad view's
  query methods and read-only properties, the semantic button/axis name sets,
  the canonical range and trigger threshold, the raw fallback, and the F13
  milestone tag

#### Scenario: Catalog derived from vision
- **WHEN** the document's function catalog is checked against vision.md
- **THEN** every capability vision.md names for the consumer API (2D quads,
  meshes, vertex colors, cameras, lights, Phong materials with maps, alpha
  masks, blending modes, render targets, post FX, resource loading,
  keyboard/mouse input query and events, gamepad input, script modules,
  skinning/animation, high-level text drawing) has a corresponding
  catalog entry or an explicitly noted open question
