# 0055 — Operations on a native-backed resource are methods on its class

Status: Accepted (2026-10-06, change `resource-class-methods`)

## Context

ADR 0053 made the **subject** the first argument of every public function.
For three operations the subject is a native-backed class instance, yet they
lived as free functions in `efx.graphics` and re-took that instance as their
first argument: `efx.graphics.poseMesh(mesh, pose)`,
`efx.graphics.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)`, and
`efx.graphics.measureText(text, font, opts?)`. Every other operation on a
class was already a method (`ParticleSystem.emit`/`set`, `Audio.stop`/`pause`,
`Body.applyImpulse`, `Character.moveAndSlide`, `destroy()`), so the three were
the only class operations that repeated their subject as an argument and the
`Mesh`/`Font` method surface was incomplete. The full process record is
`openspec/changes/resource-class-methods/`.

## Decision

An operation whose **subject is a native-backed class instance** is a **method
on that class's prototype**, with the subject implicit as the receiver
(`this`). Concretely: `Font.measure(text, opts?)`, `Mesh.pose(pose)`, and
`Mesh.setSurfaceMaterial(surfaceIndex, mat)`. The corresponding free functions
`efx.graphics.measureText`, `efx.graphics.poseMesh`, and
`efx.graphics.setMeshSurfaceMaterial` are removed — a hard cut with no
aliases, matching ADR 0050/0051/0053. `efx.graphics` holds constructors,
factories, and stateless operations, and does not re-take a class instance as
a free-function argument.

The methods follow ADR 0053's convention with the subject implicit: required
inputs stay positional and optional inputs stay in one trailing bag
(`mesh.pose(pose)`, `mesh.setSurfaceMaterial(surfaceIndex, mat)`,
`font.measure(text, opts?)`). They are registered on the existing class
prototypes in lockstep: `CLASS_SPECS[]` / `*_proto_funcs` in `src/api/api.c`
on desktop, and the `methods` field of `__efxResourceClass` in
`src/web/js/core.js` on web. The C entry points resolve the subject from
`this_val`; the bridge functions are unchanged. Receiver liveness/type errors
use the class guard (`expected a Mesh` / `expected a Font` /
`using a destroyed resource`) identically on both runtimes; a message that
named the removed argument form is renamed to the method form and the
cross-runtime error catalog is updated in the same change.

## Consequences

- Class operations now have one shape, `subject.method(args)`; a caller never
  repeats the subject, and the `Mesh`/`Font` method surface is complete.
- `docs/js-api.md` states the rule; `gallery/src/api/efx.d.ts` declares the
  methods and drops the free functions; the generated `docs/api/` reference is
  regenerated from it.
- Behavior — layout/pose/material results, defaults, error classes, and
  cross-runtime error-message identity — is unchanged; only the access path
  changes. Golden frames stay pixel-identical (no re-baseline).
- Future additions: if an operation's subject is a native-backed class, add a
  method rather than a namespace function. The argument-order rule (ADR 0053)
  now reads "the receiver is the subject, then required resources/selectors,
  then scalars/vectors, then the bag".

## Rejected alternatives

- **Keep the free functions with the subject first.** Consistent with ADR
  0053's positional order, but inconsistent with every other class operation
  and it repeats the subject; the class-method form is the established model.
- **Thin prototype wrappers that re-inject `this` into the old functions.**
  Pointless indirection and a second place for the receiver check to drift
  from the argument check.
- **Move the operation logic into the shared prelude (ADR 0049).** These are
  hot/mid-level paths that keep their native validation; the prelude is for
  cold-path option-bag validation.
- **Keep deprecated free-function aliases.** Pre-1.0 with all consumers
  in-repo; aliases would leave two shapes for one operation and contradict the
  hard-cut precedent (ADR 0050/0051/0053).
