# 0053 — Required inputs are positional; an options bag holds only optional configuration

Status: Accepted (2026-10-05, change `required-args-positional`)

## Context

The script-facing API had no single argument rule. `docs/js-api.md` stated a
hot-path heuristic ("hot immediate-mode calls take scalar arguments first;
configuration beyond ~3 values goes in a trailing option object"), which is
not a required/optional rule, and ten functions buried required inputs as
fields of their option bag (`createBody({ shape })`,
`createRenderTarget({ width, height })`, `setCamera3D({ pos, target, fov })`,
`createImageData`, `createFont`, `drawBillboard`, `createParticleSystem`,
`createCharacter`, `raycast`, `createMeshData`). The draw family also ordered
arguments inconsistently: `drawSprites(texture, sprites)` led with the thing
drawn while `drawQuad(x, y, texture, …)` did not, and `drawBillboard` hid its
texture in the bag. Every future API addition had to guess which convention
applied. The full process record is
`openspec/changes/required-args-positional/`.

## Decision

One rule governs every public function: **required inputs are positional
arguments and all optional inputs are fields of one trailing options bag**. A
call with no optional inputs omits the bag. Every bag field is optional and a
bag never carries a required input. A lone optional input MAY stay positional
(`efx.log(msg?)`, `efx.quit(code?)`). Required fields are permitted inside a
**record/value** argument (a shape, a light descriptor, a pose sample, a
source rectangle, or a batch element) because it is itself a required
positional value, not the function's bag. Argument order is the subject (the
thing drawn, created, or queried) first, then required resources/selectors,
then required scalars (2D `x`, `y`) or vectors (3D position), then the bag.

Concretely the ten bag-buried functions become `createImageData(width, height,
pixels, opts?)`, `createRenderTarget(width, height)`, `setCamera3D(pos,
target, fov, opts?)`, `createFont(fontData, size, opts?)`,
`drawBillboard(texture, pos, opts?)`, `createParticleSystem(texture, max,
lifetime, opts?)`, `createBody(shape, opts?)`, `createCharacter(radius,
height, opts?)`, `raycast(origin, direction, maxDistance, opts?)`, and
`createMeshData(surfaces, materials?)`; `drawQuad` becomes `(texture, x, y,
opts?)`; the `createMeshData` single-surface shorthand is removed. This is a
hard cut with no aliases or compatibility shims.

`docs/js-api.md` states the rule, the `gallery/src/api/efx.d.ts` declaration
expresses it (a required input cannot be omitted from a bag), and the shared
prelude (ADR 0049) validates the remaining optional bags once for every
runtime. Behavior — defaults, validation outcomes, error classes and messages
— is unchanged; only the call shape changes.

## Consequences

- New API functions have one mechanical rule to follow; a caller can see
  requiredness in the signature and the `?` on a bag means exactly "the whole
  bag may be omitted".
- `log`/`quit` and existing single-field bags (`setRenderScale(scale, opts?)`,
  `loadMeshData(path, opts?)`) are explicitly permitted to keep a lone
  optional positional or bagged; the allowance is not forced either way.
- The cold-function changes live in `src/prelude/prelude.js` (natives already
  receive scalars); the hot `drawQuad`/`drawBillboard` reorders update both
  native validators (desktop `src/api/*.c`, web `src/web/js/*.js`), keeping the
  cross-runtime error catalog byte-identical.
- Applying the rule exposed a latent leak in the desktop mesh-data wire
  helpers (`src/api/api_3d.c`): `wire_f32`/`wire_i32`/`wire_u32` returned NULL
  on a `null` argument without clearing the pending quickjs exception, which
  the now-nullable `blocks`/`maps` path reached; the helpers now free the
  exception.
- Golden frames stay pixel-identical: the change is a call shape, not a
  behavior change, so no golden re-baseline is needed.

## Rejected alternatives

- **Keep the hot-path/config-volume heuristic.** It does not say what is
  required vs optional and produced the ten bag-buried functions; ambiguity
  is the problem being removed.
- **Make everything one options bag.** Maximally uniform but breaks the
  hot-path scalar-first rule and `drawQuad`/`drawSprites` ergonomics, and
  hides requiredness in bags again.
- **Apply the rule recursively into record fields.** Would force positional
  unpacking of shapes, light descriptors, pose samples, and batch elements,
  and is ambiguous for unions; records keep their required fields.
- **Keep `createMeshData`'s batch/shorthand union.** The shorthand's
  convenience does not justify the union's complexity under a mandatory rule;
  a single-surface mesh passes a one-element list.
- **Two `createMeshData` overloads keyed on the first argument.** Requires
  runtime type-sniffing and preserves the union; rejected for clarity.
