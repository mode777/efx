# Design

## Context

See `proposal.md` — Why. The relevant current state:

- `efx.drawMesh` is a C-implemented function on the desktop binding
  (`src/api/api.c`, registered in `src/runtime/runtime.c` with arity hint 1)
  and a hand-written wrapper on the web binding (`src/web/entry.js`), which
  calls `src/web/bridge.c`'s `efx_bridge_draw_mesh(handle, transformPtr,
  colorPtr)`. The bridge signature already separates the mesh from the
  transform/color pointers, so only the JS-facing argument parsing changes.
- The procedural primitives live in the engine-bundled pure-JS prelude
  (`src/prelude/prelude.js`), embedded into `src/prelude/prelude.h` by
  `tools/gen_prelude.py`; the Linux gate fails on drift. Each primitive
  builds arrays and returns `efx.createMeshData({ positions, … })`.
- `efx.createMeshData` already accepts a parallel `materials` array and
  snapshots/binds it; the primitive material must reuse that path, not
  duplicate binding or validation.
- The gallery type document `gallery/src/api/efx.d.ts` is checked by
  `tsc`/`svelte-check` via `gallery/src/api/efx.type-test.ts`.

## Goals / Non-Goals

**Goals:**

- Make the required mesh argument visible and positional on both bindings
  with identical semantics (ADR 0022 parity).
- Add the primitive `material` option without new binding/validation logic.
- Make `efx.d.ts` type every valid call shape for the touched functions.
- Keep rendering output byte-identical (goldens unchanged).

**Non-Goals:**

- No renderer, display-list, shader, or material-model change.
- No compatibility shim for the old `drawMesh({ mesh })` bag.
- No general audit of `efx.d.ts` beyond the touched shapes.

## Decisions

**D1 — `drawMesh(mesh, opts?)`; `mesh` is `argv[0]`, the bag is `argv[1]`.**
The desktop binding reads the mesh from `argv[0]` and requires a live Mesh
before touching the bag; the bag, when present, must be an object and its
known fields become `{ transform, color }` (`mesh` is removed from the
known set, so it now throws `TypeError` as unknown). The runtime arity hint
becomes 2. The web wrapper mirrors this exactly with `arguments.length`.
Rejected: keeping the bag and merely documenting that `mesh` is required
(the requirement stays invisible); accepting both forms (an ambiguous call
shape that defeats the point of the break).

**D2 — Primitive `material` is an option field, not a positional.**
`__efxPrimOpts`'s allowed-field list gains `material`; the primitive reads
`opts.material` (default `undefined`), then passes
`materials: material === undefined ? undefined : [material]` into its
existing `createMeshData` call. `null` means the engine default (matching
`createMeshData`). This reuses the existing snapshot/binding/validation and
avoids a placeholder argument. Rejected: a positional second argument
(`makeCube(undefined, mat)` is awkward); hand-rolling surface binding in
the prelude (duplicates native logic and risks drift).

**D3 — Type the two `createMeshData` forms as an exclusive union.**
`type CreateMeshDataOptions = MeshDataBatch | MeshDataShorthand`, where
`MeshDataBatch = { surfaces: MeshSurfaceData[]; materials?: (Material |
null)[]; positions?: never; … }` and `MeshDataShorthand = MeshSurfaceData &
{ materials?: …; surfaces?: never }`. `MeshSurfaceData` keeps `positions`
required (correct for a surface and for the shorthand); the batch bag no
longer inherits that requirement. The `never` discriminators are required
because a plain union's excess-property check considers the *union* of all
members' known properties, so `{ surfaces, positions }` would otherwise
type-check; declaring the other form's fields as `never` makes a mixed
object literal fail both members. Rejected: a plain union (does not reject
mixing — found during apply); `Partial<MeshSurfaceData> & { surfaces? }` (a
permissive bag that no longer rejects a call with neither form); function
overloads (heavier, and the runtime distinguishes by presence, which a
union expresses more directly).

**D4 — Only the JS-facing wrappers change; `bridge.c` is untouched.**
`efx_bridge_draw_mesh` already takes the handle plus optional transform and
color pointers, so the reshape is confined to `efx_js_drawMesh`
(`src/api/api.c`), its registration, and the `entry.js` wrapper. Rejected:
threading the mesh through the options-pointer protocol (needless churn).

**D5 — No ADR.**
ADR 0024 already records the resource-first-arg convention and per-surface
material binding; this change applies them to `drawMesh` and adds a
convenience that reuses them. The break is documented in the change record,
matching the f2c `drawQuad` precedent.

## Risks / Trade-offs

- **Every existing 3D call site breaks at once** → rewrite them
  mechanically in one pass; the golden suite must reproduce the committed
  PNGs unchanged, which proves the reshape is behavior-preserving, and the
  portable smoke/unit scripts cover both bindings.
- **Desktop/web binding drift** → the two wrappers must reject the same
  inputs with the same error classes; the validation scripts run through
  both runtimes, and the type-test pins the call shape.
- **`prelude.h` drift after editing `prelude.js`** → regenerate with
  `tools/gen_prelude.py`; the gate's `--check` catches a missed
  regeneration.
- **The `never`-discriminated union rejects mixed object literals, but a
  variable typed as one member with an extra field can still slip through
  the compiler** → the runtime rejects mixed forms regardless; the type-test
  exercises the literal cases the docs show.
- **Stale `drawMesh({…})` examples in historical ADRs** → ADRs are
  historical records and are not rewritten; the current reference and
  provisional F7/F8 sections in `docs/js-api.md` are updated so forward
  guidance is correct.
