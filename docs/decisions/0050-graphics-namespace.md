# 0050 — Graphics functions live in the `efx.graphics` sub-namespace

Status: Accepted

## Context

Every domain added after the F2 graphics layer shipped as a
sub-namespace of the single `efx` object — `efx.keyboard`/`efx.mouse`/
`efx.window` (F9), `efx.physics` (F12), `efx.gamepad` (F13), `efx.audio`
(F14) — but the 33 graphics drawing, state, and resource functions kept
their original root-level bindings from F2–F8 and F11. The root
therefore mixed runtime/lifecycle facilities with the largest domain
surface, and the namespace had two different organization stories
depending on when a function was added. Change
`openspec/changes/efx-graphics-namespace/` holds the full process
record.

## Decision

Graphics drawing, state, and resource functions SHALL be exposed as the
`efx.graphics` sub-namespace, not at the `efx` root. The namespace is
assembled in the two bindings and augmented by the shared prelude,
exactly like `efx.audio`/`efx.physics`: the desktop binding creates the
object and registers its native entries (`GRAPHICS_FUNCS` in
`src/runtime/runtime.c`), the web binding declares the nested
`graphics: { … }` member of the concatenated `api` literal
(`src/web/js/`), and `src/prelude/prelude.js` installs its members onto
the same object in place. The root of `efx` holds only
runtime/lifecycle facilities and domain sub-namespaces: `log`, `quit`,
`args`, `registerUpdateHook`, `registerRenderHook`, `whiteTexture`,
`loadText` (the resource facility the CommonJS loader also uses), the
`efx.mat4`/`efx.vec3`/`efx.quat` helpers, and the domain namespaces
(`keyboard`/`mouse`/`window`/`physics`/`gamepad`/`audio`/`graphics`).

The move is a hard cut: no root alias, no deprecation shim, and no
behavior change — names, signatures, semantics, defaults, and error
message text (which stays bare-function-named, e.g. `drawQuad: …`)
are byte-identical across the move so the cross-runtime error catalog
and every golden image stay valid.

Future API additions follow the same organization rule: a new domain
gets a sub-namespace; anything that is graphics (drawing, camera/light
state, post processing, graphics resource creation/loading) goes under
`efx.graphics`.

## Consequences

Scripts must use `efx.graphics.drawQuad`-style paths; `efx.drawQuad` is
a compile-time error in the gallery type document (`EfxGraphics`
interface) and a runtime `undefined` call otherwise. All in-repo
consumers (tests, golden scenes, curated samples, examples) were
re-pathed in the same change, so nothing outside the repo observes the
break. Future changes adding graphics functions extend `EfxGraphics`
and the three registration layers in lockstep — a membership guard test
(`graphics_ns_js` in `tests/unit/api_tests.c`) pins the exact 33-member
shape on both runtimes and fails if a root alias returns or a layer
drifts. Docs conventions: `docs/js-api.md` states the organization
rule, and the generated reference renders the sub-namespace page from
`gallery/src/api/efx.d.ts`.

## Rejected alternatives

- **Build `efx.graphics` purely in the prelude** by re-wrapping the
  flat natives — lost because it would add a JS frame to 16 hot
  per-frame draw calls (against the hot-path rule of ADR 0049's
  layering) and diverge from the established audio/physics assembly
  pattern.
- **Keep root bindings and alias `efx.graphics` to them** — lost
  because it changes nothing observable, contradicts the "instead of
  the root" goal, and leaves two live paths to rot docs, tests, and the
  error catalog.
- **Namespace-qualify error message text** (`efx.graphics.drawQuad:
  …`) — lost because it would break the byte-identical cross-runtime
  error catalog contract for zero script-visible benefit.
