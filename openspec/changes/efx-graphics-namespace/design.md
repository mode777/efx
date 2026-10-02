# Design

## Context

The 33 graphics functions are registered in three lockstep layers: the
native QuickJS table `EFX_FUNCS` + `install_efx_api()` in
`src/runtime/runtime.c` (which also builds the private `natives` wire
object), the concatenated `var api = {…}` literal spanning
`src/web/js/{render2d,resource,text,target_post,particles,render3d}.js`
(plus the web `natives` twin in `src/web/js/audio.js`), and the pure-JS
install targets in `src/prelude/prelude.js` (embedded via
`tools/gen_prelude.py` → `prelude.h`). Every later domain already ships
as a sub-namespace object created by the binding and augmented by the
prelude: `efx.keyboard/mouse/window/gamepad` (`src/api/api_input.c`),
`efx.audio` (`src/api/api_audio.c`), `efx.physics`
(`src/api/api_physics.c`); their web twins are `api.<ns> = {…}`
literals. All in-repo callers use plain `efx.fn(...)` calls — there is
no destructuring or method extraction anywhere, so every consumer
migration is a mechanical re-path. Motivation: see proposal.md — Why.
Behavior contract: see the spec deltas (`js-api` ADDED "Graphics
namespace API" + MODIFIED requirements, and the nine domain deltas).

## Goals / Non-Goals

- Goals: one `efx.graphics` object with exactly the 33 members on both
  runtimes; zero root remnants; byte-identical rendered frames and
  error output; migrated tests, samples, docs, and types in the same
  change.
- Non-Goals: any behavior, signature, default, validation, or
  error-text change; aliases or deprecation shims; touching the other
  namespaces or the root facilities (`log`, `quit`, `args`, hooks,
  `whiteTexture`, `loadText`, `mat4`/`vec3`/`quat`).
- Unmanaged-resource exposure is unchanged by this move:
  dynamic-count resources stay GC-finalized opaque classes with
  explicit `destroy()` (ADR 0011, discipline ADR 0012); the light bank
  stays the one pre-allocated slot bank. Only the JS access path
  changes; class identity, lifecycle, and the fixed-limits table are
  untouched.

## Decisions

### D1 — Assemble the namespace in the bindings, augment from the prelude (copy the audio/physics pattern)

Both bindings create an empty `graphics` object on `efx` and register
the natively-implemented members on it; the prelude then installs its
members onto `efx.graphics` in place.

- Desktop: `install_efx_api()` creates `graphics = JS_NewObject()`,
  registers a new `GRAPHICS_FUNCS` table via
  `JS_SetPropertyFunctionList`, and attaches it with
  `JS_SetPropertyStr(ctx, efx, "graphics", graphics)` — the exact
  shape of `efx_api_register_input`/`efx_api_register_audio`. The 16
  native entries move out of `EFX_FUNCS` into `GRAPHICS_FUNCS`;
  C implementations (`src/api/api_*.c`) and their declarations are
  unchanged.
- Web: the concatenated `api` literal gains a nested
  `graphics: { … }` object (opened in `render2d.js` where the literal
  opens); each domain file contributes its moved entries to it,
  mirroring today's structure one level deeper. The web `natives` twin
  is untouched (it is not script-visible).
- Prelude: `__efxPreludeInstall` assigns its 17 members to
  `efx.graphics.<name>` instead of `efx.<name>` (guard
  `efx.graphics = efx.graphics || {}` for ordering robustness), and
  regenerating `prelude.h` is part of the change.

Layer split that drives the work (33 total):

- Native-registered (16): `setClearColor`, `drawQuad`, `setBlendMode`,
  `createMesh`, `drawMesh`, `poseMesh`, `setMeshSurfaceMaterial`,
  `beginRenderTarget`, `endRenderTarget`, `setRenderScale`,
  `loadFontData`, `drawText`, `measureText`, `drawBillboard`,
  `drawSprites`, `drawParticles`.
- Prelude-wrapped natives (13): `setLight`, `setDirectionalLight`,
  `setCamera2D`, `setCamera3D`, `createImageData`, `createTexture`,
  `createRenderTarget`, `loadImage`, `loadMeshData`, `createMeshData`,
  `createFont`, `setPostEffects`, `createParticleSystem`.
- Pure JS (4): `makeCube`, `makePlane`, `makeSphere`, `makeCapsule`.

*Alternatives considered:* (a) Build `efx.graphics` purely in the
prelude as a fresh object re-exposing the flat natives — rejected: it
must re-wrap 16 hot natives per runtime, adding a JS frame to per-frame
draw calls and contradicting the hot-path rule (native validation, no
extra indirection, docs/js-api.md Overview), and it diverges from how
`efx.audio`/`efx.physics` are already assembled. (b) Keep root
functions and define `efx.graphics` as an alias — rejected: the spec
requires the root members gone; an alias changes nothing observable and
would fail the "no root aliases" scenario.

### D2 — Hard cut, no compatibility aliases

Root members are deleted, not deprecated. All consumers are in-repo
(tests, goldens, curated samples, examples) and migrate in the same
change. *Alternative:* keep deprecated root aliases for one release —
rejected: there is no external script corpus to protect (pre-1.0,
single-vault API), aliases would need a deprecation-warning path that
does not exist, and a second live path would rot docs, tests, and the
error catalog.

### D3 — Error message text stays byte-identical

Engine error strings embed bare function names (e.g. `drawQuad: …` in
`src/api/api_2d.c`, the `where` labels in `src/web/js/core.js`). They
stay exactly as they are — no `efx.` or `efx.graphics.` prefix — so the
cross-runtime error catalog (`tests/scripts/s_error_catalog.js` +
`.expected.txt`) and the "Identical argument errors on every runtime"
requirement hold without churn. *Alternative:* namespace-qualify
messages — rejected: breaks byte-identical cross-runtime error output
for zero script-visible benefit. Audit caveat: if any message turns out
to embed a root-qualified name (`efx.<fn>`), that one message is
re-pathed and the expected fixture updated in the same change (apply
time task).

### D4 — Type document: extracted `EfxGraphics` interface, root members deleted

`gallery/src/api/efx.d.ts` gains `interface EfxGraphics { … }` holding
the 33 declarations (TSDoc and `@example` blocks re-pathed); `interface
Efx` loses the 33 members and gains `readonly graphics: EfxGraphics`.
`efx.type-test.ts` is re-pathed and keeps rejecting invalid shapes.
`docs/api/` is regenerated (`npm --prefix gallery run docs:markdown`)
and `docs:check` must pass. *Alternative:* keep the flat declarations
and add a type-level alias — rejected: the declaration must fail to
compile on root-level use, or the type document would not "accurately
describe the valid call shapes" (js-api requirement).

### D5 — Internal prelude call sites route through the new public path

The pure-JS primitives call `efx.createMeshData` in four places; they
call `efx.graphics.createMeshData` after the move. *Alternative:*
capture the function reference once at install time — rejected: a
needless second style for calling the public API from the `[JS]` layer;
the two-layer rule is about building on public API, and the direct path
is what the primitives already do.

## Risks / Trade-offs

- [Missed root-qualified call in a rarely-run script] → End-of-change
  audit: word-bounded repo grep for all 33 names as `efx.<name>` must
  return zero outside `openspec/changes/archive/`, historical ADR text,
  and this change folder; both runtime suites run the whole corpus.
- [Membership drift between the three layers] → Extend the api tests
  with an enumeration check: `Object.keys`-style listing of
  `efx.graphics` matches the 33-name list, no moved name exists at the
  root; the existing cross-runtime compare covers the web side.
- [Error-catalog fixture churn] → D3 keeps messages stable; the catalog
  runs unchanged on both runtimes and must stay byte-identical.
- [docs/api drift breaking the Pages build] → Regenerate `docs/api/`
  in the same change and run `npm --prefix gallery run docs:check`.
- [Golden suite suggests drift] → A pure re-path cannot change pixels;
  any golden diff indicates an accidental behavior change — treat as a
  bug, never re-baseline.
- [Curated samples rebuilt] → The 15 sample `main.js` files are
  re-pathed; the gallery rebuild (`prepare-player.mjs` + build) is part
  of verification, and Pages deploys on merge to `main` as usual.

## Migration Plan

One atomic in-repo change, no external migration (no scripts exist
outside the repo):

1. Engine: `runtime.c` (`GRAPHICS_FUNCS` + attach), `src/web/js/*.js`
   (`api.graphics` literal members), `prelude.js` install targets +
   the four internal call sites; regenerate `prelude.h`.
2. Consumers: `tests/unit/api_tests.c`, `tests/scripts/*.js` (+ the
   golden `main.js` files, web fixtures, `physics_smoke.js`), the 15
   curated samples, `examples/browser/main.js`.
3. Types & docs: `efx.d.ts` + `efx.type-test.ts`, regenerate
   `docs/api/`, update `docs/js-api.md` (Overview "One namespace" rule
   gains the sub-namespace organization; re-path examples and the
   vision-traceability table), `README.md` mentions, `vision.md`
   mention check.
4. ADR: `docs/decisions/0050-graphics-namespace.md` (next free number,
   confirmed at apply time) recording the namespace-organization rule
   (root = lifecycle/runtime facilities + domain sub-namespaces;
   graphics joins audio/physics/input/gamepad), indexed in
   `docs/decisions/README.md`.
5. Verification per AGENTS.md: commit → push branch →
   `python3 tools/verify_remote.py all <branch>` (native ctest incl.
   all goldens + Emscripten suite) → only if green dispatch
   `gh workflow run ci.yml --ref <branch>` (Linux → Windows → macOS);
   merge to `main` after the gate is green.

Rollback: `git revert` of the change series; no data, resource, or
on-disk format changes exist.

## Open Questions

None — the ADR number is resolved at apply time (next free number in
`docs/decisions/`).
