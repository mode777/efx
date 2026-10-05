# Proposal

## Why

The script API's real convention is **required inputs are positional
arguments; optional inputs live in a trailing options bag** — but the codebase
does not follow it uniformly, and `docs/js-api.md` states a different,
ambiguous rule ("hot calls take scalar arguments first; configuration beyond
~3 values goes in a trailing option object"). The result: ten functions bury
required inputs as fields of their option bag (`createBody({ shape })`,
`createRenderTarget({ width, height })`, `setCamera3D({ pos, target, fov })`,
…), and the draw family orders its arguments inconsistently (`drawQuad` puts
the texture last while `drawSprites` puts it first; `drawBillboard` hides the
texture in its bag). Because the bag rule is the one API convention every
future addition must follow, the contradiction should be removed now: adopt
the required/optional rule as mandatory, normalize argument order, and rewrite
the guideline so there is one unambiguous rule.

## What Changes

- **BREAKING — required inputs move out of the options bag** for ten
  functions, each leaving only optional configuration in a trailing bag:
  - `createImageData(opts)` → `createImageData(width, height, pixels, opts?)`
  - `createRenderTarget(opts)` → `createRenderTarget(width, height)`
  - `setCamera3D(opts)` → `setCamera3D(pos, target, fov, opts?)`
  - `createFont(fontData, opts)` → `createFont(fontData, size, opts?)`
  - `drawBillboard(pos, opts)` → `drawBillboard(texture, pos, opts?)`
  - `createParticleSystem(opts)` →
    `createParticleSystem(texture, max, lifetime, opts?)`
  - `createBody(opts)` → `createBody(shape, opts?)`
  - `createCharacter(opts)` → `createCharacter(radius, height, opts?)`
  - `raycast(origin, direction, opts)` →
    `raycast(origin, direction, maxDistance, opts?)`
  - `createMeshData(data)` → `createMeshData(surfaces, materials?)`
- **BREAKING — `createMeshData` drops the single-surface shorthand.** The
  batch bag and the shorthand union collapse to one positional surface list
  plus an optional parallel materials array; a single-surface mesh wraps its
  surface in a one-element array.
- **BREAKING — draw-family argument order is normalized: the thing drawn
  leads.** `drawQuad(x, y, texture, opts?)` → `drawQuad(texture, x, y, opts?)`;
  `drawBillboard(pos, opts)` → `drawBillboard(texture, pos, opts?)`. `drawText`
  keeps `(text, font, x, y, opts?)` (subject → required resource → position)
  and `drawSprites`/`drawMesh`/`drawParticles` are already correct.
- **The convention becomes a written rule.** `docs/js-api.md` replaces the
  hot-path heuristic with: required inputs positional; all optional
  configuration in one trailing bag; a lone optional input MAY stay
  positional; required fields are permitted inside a **record/value** argument
  (a shape, a light, a pose sample, a batch element) but not inside the
  function's own options bag; and the ordering rule (subject/thing drawn →
  required resources/selectors → required scalars (2D `x, y`) / vectors (3D
  position) → options bag).
- **Docs/types/bindings updated in lockstep.** `gallery/src/api/efx.d.ts`
  (signatures + split bags + `DrawMeshCallOptions` → `DrawMeshOptions`),
  `gallery/src/api/efx.type-test.ts`, regenerated `docs/api/`, and the shared
  prelude wrappers (`src/prelude/prelude.js`) for the cold functions. The hot
  `drawQuad`/`drawBillboard` reorder also updates both native validators
  (desktop `src/api/*.c`, web `src/web/js/*.js`) and the native arg counts in
  `src/runtime/runtime.c`.

## Capabilities

### New Capabilities

- None — this changes the call shape of existing functions and the design
  guideline; it introduces no new capability.

### Modified Capabilities

- `js-api`: ADD "Argument passing convention" (the required/optional rule,
  the lone-optional carve-out, the record-vs-bag boundary, and the ordering
  rule); MODIFY "Font and text API" (`createFont` signature), "Billboard,
  sprite-batch, and particle API" (`drawBillboard`, `createParticleSystem`),
  "Physics namespace API" (`createBody`, `createCharacter`), and "Gallery type
  document accuracy" (`createMeshData` is one positional surface list plus
  optional materials; shorthand removed).
- `2d-layer`: MODIFY "Quad drawing" (`drawQuad(texture, x, y, opts?)`),
  "Image and texture resources" (`createImageData(width, height, pixels,
  opts?)`), and "Batched 2D sprite drawing" (its `drawQuad` equivalence uses
  the new order).
- `3d-core`: MODIFY "3D camera" (`setCamera3D(pos, target, fov, opts?)`) and
  "Multi-surface mesh data" (`createMeshData(surfaces, materials?)`; shorthand
  removed).
- `billboards`: MODIFY "World-space billboard drawing"
  (`drawBillboard(texture, pos, opts?)`).
- `particles`: MODIFY "Particle system creation and configuration"
  (`createParticleSystem(texture, max, lifetime, opts?)`).
- `font-text`: MODIFY "Fixed glyph atlas baking"
  (`createFont(fontData, size, opts?)`).
- `character-controller`: MODIFY "Capsule character creation"
  (`createCharacter(radius, height, opts?)`).
- `physics-queries`: MODIFY "Raycast"
  (`raycast(origin, direction, maxDistance, opts?)`).
- `render-targets`: MODIFY "Render target resources"
  (`createRenderTarget(width, height)`).
- `lighting`: MODIFY "Per-surface material binding" (`createMeshData`
  materials are a positional optional array).

## Impact

- **Declaration & reference:** `gallery/src/api/efx.d.ts`,
  `gallery/src/api/efx.type-test.ts`, regenerated `docs/api/` (never
  hand-edited).
- **Guidelines & ADR:** `docs/js-api.md` §Conventions "Parameters" is
  rewritten and the ordering/record rules are added; a new ADR
  `docs/decisions/0053` (next free number, confirmed at apply time) records the
  convention, indexed in `docs/decisions/README.md`. **ADR required** — this
  amends a durable API design rule.
- **Bindings:** `src/prelude/prelude.js` (nine cold wrappers pull required
  values from positional arguments; the native marshalling is unchanged) plus
  regenerated `src/prelude/prelude.h`; `src/api/api_2d.c`, `src/api/
  api_particles.c`, `src/runtime/runtime.c` (`drawQuad`/`drawBillboard` arg
  order and counts), and `src/web/js/particles.js` for the hot reorder.
- **Callers:** the in-repo script corpus — portable script tests, golden-scene
  `main.js` files, web fixtures, curated gallery samples, and examples —
  updated to the new call shapes. Roughly 56 files reference the changed
  functions.
- **Behavior:** unchanged. No default, validation outcome, or error message
  changes; only the call shape. Golden frames stay pixel-identical; the
  cross-runtime error catalog stays byte-identical apart from re-pathed
  trigger lines.
- **Verification:** the four-target gate (ADR 0020) applies via the ADR 0023
  dispatch policy; Linux server pre-filter with `tools/verify_remote.py all`
  before dispatching. No new test kind.

## Non-goals

- No behavior, default, limit, resource-classification, or error-message
  changes — this is a call-shape and convention change only.
- `log(msg?)` and `quit(code?)` keep their lone optional positional argument,
  and existing single-field bags (`setRenderScale(scale, opts?)`,
  `loadMeshData(path, opts?)`) are left as bags: a lone optional input MAY
  stay positional, it is not forced either way.
- `drawText`/`measureText` keep `(text, font, x, y, opts?)` /
  `(text, font, opts?)`; `drawMesh`'s transform stays optional in its bag
  (2D and 3D draw APIs remain intentionally different).
- `createImageData` keeps its one-field options bag and `Camera3DOptions`
  keeps its name and bag (near/far) — both reserved for future options.
- No deprecated aliases, compatibility shims, or overloads accepting the old
  call shapes (hard cut, pre-1.0, all consumers in-repo).
- No new API function, resource type, or fixed limit; no milestone work.
- `docs/api/` is regenerated, never hand-edited; archived changes and
  historical ADR texts are left as written.
