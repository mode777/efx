# Design

## Context

See `proposal.md` — Why. The relevant current state:

- The script API is implemented once per platform but validated in two places
  (ADR 0049): cold creation/configuration functions validate and normalize in
  the shared pure-ES6 prelude (`src/prelude/prelude.js`) and reach
  marshal-only natives; hot per-frame draw/query calls keep native validation
  on **both** bindings (`src/api/*.c` for quickjs desktop and
  `src/web/js/*.js` for the Emscripten page engine). The native C functions
  already receive scalar arguments from the prelude wrappers, so a cold
  signature change is confined to the prelude and the declaration.
- `docs/js-api.md` currently states a hot-path/config-volume heuristic, not a
  required/optional rule; ten functions bury required inputs in their option
  bags and the draw family orders arguments inconsistently.
- `gallery/src/api/efx.d.ts` is the single source of truth for the generated
  reference; `docs/api/` is regenerated from it and guarded against drift.

## Goals / Non-Goals

**Goals:**

- One unambiguous, documented argument convention that every future API
  addition follows.
- Required inputs visible in the signature (positional) for the ten functions
  that hide them; optional configuration confined to a trailing all-optional
  bag.
- Consistent argument order across the draw family and creators.
- No behavioral change: identical defaults, validation outcomes, error
  classes/messages, and golden frames.

**Non-Goals:**

- No new function, resource type, limit, or behavior.
- No compatibility shims or deprecated aliases (hard cut, pre-1.0, all
  consumers in-repo).
- No uniform scalar/vector convention between 2D and 3D draw APIs; they stay
  intentionally different (`drawMesh` transform remains optional in its bag).

## Decisions

### Decision: the convention applies to a function's own parameters, not to record fields

The rule governs the function's parameter list: a parameter is either a
**required positional value** or the single trailing **all-optional options
bag**. A required structured argument (a `shape`, a light descriptor, a
`PoseSample`, a `SourceRect`, a batch element) is a *record/value* and may
keep its own required fields, because it is a required positional argument
rather than the function's bag. This is what keeps `setLight(slot, light)`
and `setDirectionalLight(light)` valid while `createBody({ shape, … })` is
split into `createBody(shape, opts?)`.

*Alternatives considered:* applying the rule recursively into every nested
object would force positional unpacking of shapes and pose samples — clearly
worse and ambiguous for unions. Rejected.

### Decision: a lone optional input MAY stay positional

If a function has exactly one optional input, it may be passed positionally
rather than bagged. This preserves `log(msg?)` / `quit(code?)` and leaves the
existing single-field bags (`setRenderScale(scale, opts?)`,
`loadMeshData(path, opts?)`) as bags. It is an allowance, not a mandate, so
existing spellings are not churned. For the new splits the lone optional is
positional where one remains (`createMeshData(surfaces, materials?)`) and a
bag where the original had one reserved for future options
(`createImageData(width, height, pixels, opts?)`).

*Alternative considered:* forcing every lone optional to positional for
maximal uniformity would rename `setRenderScale`/`loadMeshData` call shapes
for no behavioral gain. Rejected as churn.

### Decision: ordering rule — subject → required resources/selectors → required scalars/vectors → bag

The thing being drawn/created/queried leads; required resources and selectors
follow; then required scalars (2D `x`, `y`) or vectors (3D position); then the
bag. This makes `drawQuad(texture, x, y, opts?)` and
`drawBillboard(texture, pos, opts?)` match the already-correct
`drawSprites(texture, sprites)`, `drawMesh(mesh, opts?)`, and
`drawParticles(system)`. `drawText(text, font, x, y, opts?)` is already
consistent (subject → required resource → position) and is left unchanged.
`drawMesh`'s transform stays optional in its bag.

*Alternative considered:* moving `drawText`'s `font` after `x, y` to make the
"thing then scalars" reading literal; rejected because it reads worse and the
resource-before-position reading is the stated rule.

### Decision: `createMeshData` drops the single-surface shorthand

`createMeshData(surfaces, materials?)` replaces the `MeshDataBatch |
MeshDataShorthand` union. The shorthand's convenience does not justify the
union's complexity once the rule is mandatory; a single-surface mesh passes a
one-element list. `materials` is a lone optional and stays positional.

*Alternatives considered:* two overloads keyed on the first argument's shape
(batch array vs flat positions) require runtime type-sniffing and preserve the
union; a separate `createMeshBatch` verb adds surface. Rejected.

### Decision: cold signature changes live in the prelude; hot reorders touch both native bindings

The nine cold functions (`createImageData`, `createRenderTarget`,
`setCamera3D`, `createFont`, `createParticleSystem`, `createBody`,
`createCharacter`, `raycast`, `createMeshData`) move their required values
from bag reads to positional parameters inside their existing prelude
wrappers; the native marshalling is unchanged. `drawQuad` and `drawBillboard`
are hot and natively validated, so their reorder updates `src/api/api_2d.c`,
`src/api/api_particles.c`, `src/runtime/runtime.c` (arg counts), and
`src/web/js/particles.js` in lockstep, and the ADR 0049 hot-path carve-out
list is re-confirmed.

### Decision: migrate compiler-first, behavior-last

Update `gallery/src/api/efx.d.ts` and the type test first; TypeScript then
flags every stale call site in the in-repo corpus. Then update the prelude,
the hot native validators, the callers, and regenerate `docs/api/`. Golden
frames must remain byte-identical because only the call shape changes.

## Risks / Trade-offs

- **Large caller churn (~56 files)** → the `.d.ts` change makes stale call
  shapes compile errors, so the compiler enumerates the work; a behavior-free
  change means failures are mechanical.
- **Two hot native validators can drift** → keep the desktop C and web JS
  reorders in the same commit and rely on the cross-runtime error catalog
  (`smoke_error_catalog`) to catch message/behavior divergence.
- **Golden frames could move if an argument is mis-threaded** → goldens stay
  bit-identical by design; any pixel diff is a bug, not a re-baseline.
- **`createMeshData` shorthand removal is the most disruptive ergonomic
  change** → it is called out in the proposal; `makeCube`/`makePlane`/
  `makeSphere`/`makeCapsule` still cover the common primitives.
- **Docs/declaration drift** → `docs:check` fails on stale `docs/api/`;
  regenerate in the same change.

## Migration Plan

Hard cut with no aliases or shims. Sequence: (1) declaration + type test;
(2) prelude wrappers and regenerated `prelude.h`; (3) hot native validators
and runtime arg counts; (4) all in-repo callers (tests, goldens, fixtures,
samples, examples); (5) regenerate `docs/api/` and rewrite the
`docs/js-api.md` convention section; (6) write and index the ADR; (7) verify
on the SSH server (`tools/verify_remote.py all`) then dispatch the four-target
gate. Rollback is a revert of the branch; there is no partial-compatibility
state to unwind.

## Apply-time discoveries

- **Desktop mesh-data wire leak (fixed during apply).** Making `createMeshData`
  always pass `blocks`/`maps` (now `null` when no materials are supplied) hit a
  latent bug in `src/api/api_3d.c`: `wire_f32`/`wire_i32`/`wire_u32` returned
  NULL on a non-typed-array argument without clearing the pending quickjs
  exception, tripping `JS_FreeRuntime`'s `list_empty(&rt->gc_obj_list)`
  assertion in the headless catalog. The helpers now free the exception; the
  error catalog messages are unchanged. This is a bug fix, not a new durable
  decision — recorded in ADR 0053's Consequences.

## Open Questions

None — the argument-order and lone-optional decisions are settled and
recorded in the specs.
