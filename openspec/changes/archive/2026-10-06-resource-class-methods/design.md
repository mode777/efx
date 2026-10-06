# Design

## Context

See `proposal.md` — Why. Three operations currently live as free functions in
`efx.graphics` while their subject is a native-backed class instance:
`efx.graphics.poseMesh(mesh, pose)`,
`efx.graphics.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)`, and
`efx.graphics.measureText(text, font, opts?)`.

The class infrastructure needed to make them methods already exists on both
bindings:

- **Desktop (quickjs).** `src/api/api.c` builds a per-class prototype in
  `efx_api_init` from `CLASS_SPECS[]` (`mesh_proto_funcs`,
  `font_proto_funcs`) and attaches `destroy()`. Prototype functions receive
  the receiver as `this_val`.
- **Web (page JS engine).** `src/web/js/core.js` builds classes with
  `__efxResourceClass(name, spec)`; `spec.methods` are installed on
  `Ctor.prototype` and wrapped so the receiver is liveness-checked
  (`live(this)`) before the body runs. `EfxMesh` and `EfxFont` are defined
  there; their operation bodies currently live in the `api.graphics` literal
  in `src/web/js/render3d.js` and `src/web/js/text.js`.
- **Registration.** Desktop natives are listed in `GRAPHICS_FUNCS` in
  `src/runtime/runtime.c`; web members are keys in the `api.graphics` object
  literal. Both must drop the three names in lockstep.

Existing precedent: `ParticleSystem`, `Audio`, `Body`, and `Character` already
expose operations as methods, and their methods are validated by the class
liveness guard. The error catalog (`tests/scripts/s_error_catalog.js` +
`.expected.txt`) pins every cross-runtime error string, and goldens pin every
rendered frame.

## Goals / Non-Goals

**Goals:**

- Make the three operations methods on the class they act on, with the subject
  implicit as the receiver, and remove the free functions (hard cut).
- Keep observable behavior — layout/pose/material results, defaults, error
  classes, and cross-runtime error-message identity — intact.
- Establish and document the durable rule so future additions follow it.

**Non-Goals:**

- Converting any other `efx.graphics` function to a method (`drawText`,
  `drawMesh`, `drawQuad`, … stay namespace functions).
- Introducing a new resource class, query property, or limit.
- Changing the underlying C layout/pose/material implementations.
- Re-baselining any golden image (a surface move must be pixel-identical).

## Decisions

### D1. Where each method is registered (three lockstep layers)

| Operation | Desktop prototype | Web class spec |
|---|---|---|
| `Font.measure(text, opts?)` | `font_proto_funcs` in `src/api/api.c` | `methods` on `EfxFont` in `src/web/js/core.js` |
| `Mesh.pose(pose)` | `mesh_proto_funcs` in `src/api/api.c` | `methods` on `EfxMesh` in `src/web/js/core.js` |
| `Mesh.setSurfaceMaterial(surfaceIndex, mat)` | `mesh_proto_funcs` in `src/api/api.c` | `methods` on `EfxMesh` in `src/web/js/core.js` |

The C implementations (`efx_js_poseMesh` in `src/api/api_3d.c`,
`efx_js_setMeshSurfaceMaterial` in `src/api/api_lighting.c`,
`efx_js_measureText` in `src/api/api_text.c`) resolve the subject from
`this_val` instead of `argv[0]` and shift the remaining argument indices by
one. The web bodies move from the `api.graphics` literal
(`src/web/js/render3d.js`, `src/web/js/text.js`) into the class `methods`
specs; the generic wrapper supplies the `live(this)` guard, so the bodies no
longer re-resolve the subject. The bridge functions
(`_efx_bridge_pose_mesh`, `_efx_bridge_mesh_set_material`,
`_efx_bridge_text_measure`) are unchanged — they already take the native
handle.

The three names are removed from `GRAPHICS_FUNCS` (`src/runtime/runtime.c`)
and from the web `api.graphics` literal. `src/api/api.h` keeps the entry-point
declarations (renamed or unchanged) so the prototype function list can
reference them.

*Alternatives considered:* keeping the C functions with the subject in
`argv[0]` and adding thin prototype wrappers that re-inject `this` — rejected
as pointless indirection and a second place for the receiver check to drift;
moving the logic into the prelude — rejected because these are hot/mid-level
paths that keep native validation (ADR 0049).

### D2. Error-message policy

Two classes of message change:

- **Receiver errors** now come from the class liveness guard, not an
  argument check: `expected a Mesh` / `expected a Font` (wrong type) and
  `using a destroyed resource` (dead). These already exist verbatim in both
  bindings (`live_opaque` on desktop, `__efxResourceClass.live` on web), so
  they stay byte-identical across runtimes.
- **Operation-token messages** that named the removed free-function argument
  form are updated to the method form in both bindings, e.g.
  `measureText requires a live Font` → the receiver guard,
  `measureText requires (text, font, opts?)` → `measure requires (text, opts?)`,
  `poseMesh requires (mesh, pose)` → `pose requires a pose`,
  `poseMesh requires a Mesh with a rig` → `pose requires a Mesh with a rig`,
  `setMeshSurfaceMaterial requires (mesh, surfaceIndex, mat)` →
  `setSurfaceMaterial requires (surfaceIndex, mat)`. Every other validation
  message (material fields, surface index, clip lookup, weights, layout) is
  byte-identical.

The error catalog trigger script and `.expected.txt` are updated in the same
change; the desktop/web compare must stay byte-identical. *Alternative
considered:* keep the historical text verbatim — rejected because a method
error that names a removed argument form (`(mesh, pose)`) is misleading, and
the catalog is explicitly a maintained artifact.

### D3. Method argument convention

The subject is implicit, so each method carries only its own inputs and
follows ADR 0053's required-positional/optional-bag rule: `mesh.pose(pose)`
(one required record/array), `mesh.setSurfaceMaterial(surfaceIndex, mat)`
(two required positionals), `font.measure(text, opts?)` (required `text`,
optional trailing bag). No bag carries a required input. This is stated as a
design rule in `docs/js-api.md` and pinned by the "Resource operation methods"
requirement.

### D4. Type document and generated reference

`gallery/src/api/efx.d.ts` declares `measure` on `EfxFont`, `pose` and
`setSurfaceMaterial` on `EfxMesh`, and removes the three members from
`EfxGraphics`. `gallery/src/api/efx.type-test.ts` gains valid method calls and
`@ts-expect-error` cases for the removed `efx.graphics.*` forms. `docs/api/` is
regenerated with `npm --prefix gallery run docs:markdown` (never hand-edited);
`docs:check` guards drift.

### D5. ADR

A new ADR `docs/decisions/0055-resource-operation-methods.md` records the rule:
an operation whose subject is a native-backed class instance is a method on
that class; `efx.graphics` holds constructors/factories and stateless
operations and does not re-take a class instance as a free-function argument.
*Alternative considered:* no ADR (pure refactor) — rejected because this is a
reusable API-shape rule (like ADR 0053) that future additions must follow and
that `docs/js-api.md` now states.

### D6. Test and sample migration

All in-repo callers migrate mechanically to the method form: `tests/unit/
api_tests.c`, the portable script suite (`tests/scripts/s_4a_validation.js`,
`s_4b_validation.js`, `s_5a_validation.js`, `s_7_skin_pose.js`,
`s_error_catalog.js`), the web fixtures
(`tests/fixtures/web/skin_probe/main.js`,
`tests/fixtures/web/text_root/main.js`), the affected golden `main.js` files,
the curated samples, and `examples/browser/main.js`. The `tests/CMakeLists.txt`
scene list is unchanged (paths only). No new test kind is added.

## Risks / Trade-offs

- **Cross-runtime message divergence** → the receiver guard is already
  identical on both bindings; operation-token messages are edited together and
  the error catalog's desktop/web compare catches any drift. Keep the catalog
  case names meaningful (the non-Font argument case becomes a receiver/bad-text
  case).
- **Golden drift from a "pure move"** → the C layout/pose/material cores are
  untouched; run the golden suite and require bit-identical frames (no
  re-baseline). A diff here means a refactor bug, not an accepted change.
- **Native/web prototype asymmetry** → both register the same method names on
  the same classes and share the liveness guard semantics; the type document
  and the portable script suite are the cross-binding contract.
- **Stale references in guidelines/ADR texts** → `docs/js-api.md` and the
  vision-traceability table are re-pathed; archived change folders and
  historical ADR texts are intentionally left as written.
- **`prelude.h` drift** → the three operations are native/web, not prelude
  wrappers, so no prelude edit is expected; if one is needed, regenerate with
  `python3 tools/gen_prelude.py` and keep `--check` clean.

## Migration Plan

1. **Native** — move the three entry points onto `mesh_proto_funcs`/
   `font_proto_funcs`, read the subject from `this_val`, update operation-token
   messages, and remove the names from `GRAPHICS_FUNCS`.
2. **Web** — move the bodies into `EfxMesh`/`EfxFont` `methods`, drop them
   from the `api.graphics` literal, and mirror the messages.
3. **Types and docs** — update `efx.d.ts` + `efx.type-test.ts`, regenerate
   `docs/api/`, and update `docs/js-api.md`.
4. **ADR** — write `0055-resource-operation-methods.md` and index it.
5. **Migrate callers** — tests, fixtures, goldens, samples, examples.
6. **Verify** — `python3 tools/verify_remote.py all <branch>` (Linux server
   pre-filter), then dispatch the four-target gate; goldens must be
   pixel-identical and the error catalog byte-identical.

Rollback is a branch revert; the change is self-contained and adds no
dependency or persisted state.

## Open Questions

None — the material choices (method names, subject-as-receiver, error policy,
hard cut) are settled above and in the specs.
