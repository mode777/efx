# Spec Delta

## MODIFIED Requirements

### Requirement: Single global API namespace
All engine-provided script functions SHALL be exposed as members of one
well-known global namespace object (the `efx` object established by F1),
available to every script without imports or setup. Scripts SHALL access
engine functionality only through this namespace and standard ES6 built-ins;
the reference document SHALL state this rule. This covers both C-implemented
functions and engine-provided high-level JS functions. The CommonJS module
facilities `require`, `module`, and `exports` (F10) are module-scoped
authoring facilities and SHALL NOT be members of the `efx` namespace or free
globals; `require` returns a module's exports and is not an engine API entry.

#### Scenario: Namespace available without setup
- **WHEN** a script calls `efx.log` without any import or setup code
- **THEN** the call succeeds on every target platform

#### Scenario: No scattered engine globals
- **WHEN** a reviewer checks how a script reaches an engine function
- **THEN** every engine-provided function is reachable as a member of the
  single namespace, not as an additional free global

#### Scenario: Sub-namespaces are members of the single namespace
- **WHEN** a script reaches input through `efx.keyboard`, `efx.mouse`, or `efx.window`
- **THEN** those objects are members of the single `efx` namespace object and
  add no free global

#### Scenario: Module facilities are not engine globals
- **WHEN** a script inspects the `efx` namespace and the global scope outside a
  module's scope
- **THEN** `require`, `module`, and `exports` are absent from both; they exist
  only inside a module's own scope

### Requirement: Normative API reference document
The project SHALL maintain `docs/js-api.md` as the normative, developer-facing
reference of the entire script API. It SHALL contain an entry for every public
API function with a signature sketch, a description, its layer tag, and the
roadmap milestone (F1–F10) that delivers it. Entries for functions whose
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
exports, and the unchanged no-browser/Node-dependency rule. Any change that
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

#### Scenario: Catalog derived from vision
- **WHEN** the document's function catalog is checked against vision.md
- **THEN** every capability vision.md names for the consumer API (2D quads,
  meshes, vertex colors, cameras, lights, Phong materials with maps, alpha
  masks, blending modes, render targets, post FX, resource loading,
  keyboard/mouse input query and events, script modules, skinning/animation,
  high-level model and text drawing) has a corresponding catalog entry or an
  explicitly noted open question
