# Spec Delta

## ADDED Requirements

### Requirement: Input namespace API

The script API SHALL expose keyboard and mouse input as sub-namespaces of the
single `efx` object — `efx.keyboard`, `efx.mouse`, and `efx.window` — with no
new free globals. Every entry SHALL be C-implemented and SHALL have identical
names, signatures, semantics, and error behavior across the desktop and web
bindings. Input exposes **no resource types**: it adds no native-backed class,
no `destroy()`, and no slot bank, so the native-backed class list and the
fixed-limits table are unchanged.

`efx.keyboard` SHALL provide:
- `isDown(key)`, `isPressed(key)`, `isReleased(key)` — current level, press
  edge, and release edge, each returning a boolean.
- `onDown(fn)`, `onUp(fn)`, `onChar(fn)` — register a callback and return an
  unsubscribe function.

`efx.mouse` SHALL provide:
- `isDown(button)`, `isPressed(button)`, `isReleased(button)` — level, press
  edge, and release edge, each returning a boolean.
- `onDown(fn)`, `onUp(fn)`, `onMove(fn)`, `onWheel(fn)` — register a callback
  and return an unsubscribe function.
- Read-only properties `position` → `[x, y]`, `x`, `y`, `delta` → `[dx, dy]`,
  and `wheel` → `[dx, dy]`, reported in surface pixels.

`efx.window` SHALL provide read-only properties `size` → `[width, height]`,
`width`, `height`, and `dpiScale`.

Event callbacks SHALL receive a single plain JS object — never a host or DOM
event — holding only engine-provided primitives: `{ key, repeat, mods }` for
keyboard down, `{ key, mods }` for keyboard up, `{ char }` for character
input, `{ button, x, y, mods }` for mouse down/up, `{ x, y, dx, dy }` for
mouse move, and `{ dx, dy }` for wheel. Key names and mouse button names SHALL
be engine-owned string constants from a documented set. Registration SHALL
require a function and SHALL throw `TypeError` otherwise; each registration
SHALL return an unsubscribe function with the same idempotent semantics as the
lifecycle hooks. A query with an unknown key or button name SHALL throw
`TypeError`. The namespaces SHALL be documented in `docs/js-api.md` and typed
in the gallery type document, both updated in the same change.

#### Scenario: Query and event are both available
- **WHEN** a script calls `efx.keyboard.isDown('space')` and registers `efx.keyboard.onDown`
- **THEN** the query reflects current state and the callback fires on key-down before the next update hook

#### Scenario: Unknown key or button rejected
- **WHEN** a script calls `efx.keyboard.isDown('notakey')` or `efx.mouse.isDown('side')`
- **THEN** the call throws `TypeError`

#### Scenario: Unsubscribe removes an input callback
- **WHEN** a script registers an input callback and calls the returned unsubscribe function
- **THEN** the callback no longer fires, and calling unsubscribe again is a no-op

#### Scenario: Event objects are plain data
- **WHEN** an input callback receives its argument
- **THEN** it is a plain JS object holding only engine-provided primitives, with no host/DOM object and no methods

#### Scenario: Mouse position is a read-only property
- **WHEN** a script reads `efx.mouse.position` or `efx.mouse.x`
- **THEN** it receives the current pointer position in surface pixels, and assigning to the property has no effect

#### Scenario: No resources added
- **WHEN** the API reference's resource classes and fixed limits are read after this change
- **THEN** they are unchanged: input adds no native-backed class, no `destroy()`, and no slot bank

## MODIFIED Requirements

### Requirement: Single global API namespace
All engine-provided script functions SHALL be exposed as members of one
well-known global namespace object (the `efx` object established by F1),
available to every script without imports or setup. Scripts SHALL access
engine functionality only through this namespace and standard ES6 built-ins;
the reference document SHALL state this rule. This covers both C-implemented
functions and engine-provided high-level JS functions.

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

### Requirement: Normative API reference document
The project SHALL maintain `docs/js-api.md` as the normative, developer-facing
reference of the entire script API. It SHALL contain an entry for every public
API function with a signature sketch, a description, its layer tag, and the
roadmap milestone (F1–F9) that delivers it. Entries for functions whose
milestone has not passed its verification gate SHALL be explicitly marked
provisional. The document SHALL also document the lifecycle model — loading `main.js`
as the implicit init, with the `efx` namespace and engine API ready before it
executes (the rendering surface is initialized when the frame loop starts and
is not script-visible at load time), plus explicit, stacking hook registration
(`registerUpdateHook` / `registerRenderHook`, update hooks receiving `dt`,
unsubscribe returned, F1 globals as load-time sugar) — and how API errors
surface (exceptions, exit codes). Any change that adds, modifies, or removes a public API function
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

#### Scenario: Catalog derived from vision
- **WHEN** the document's function catalog is checked against vision.md
- **THEN** every capability vision.md names for the consumer API (2D quads,
  meshes, vertex colors, cameras, lights, Phong materials with maps, alpha
  masks, blending modes, render targets, post FX, resource loading,
  keyboard/mouse input query and events, skinning/animation, high-level model
  and text drawing) has a corresponding catalog entry or an explicitly noted
  open question
