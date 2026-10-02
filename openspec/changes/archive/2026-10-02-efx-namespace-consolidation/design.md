# Design

## Context

See `proposal.md` — Why, and the spec deltas for the behavior contract.
ADR 0050 already moved the 33 graphics functions into `efx.graphics` and set
the organization rule; this change finishes it. The current registration is
three lockstep layers: the native QuickJS table `EFX_FUNCS` + `install_efx_api()`
in `src/runtime/runtime.c` (which also builds the private `natives` wire
object), the concatenated `var api = {…}` literal spanning
`src/web/js/{render2d,resource,text,target_post,particles,render3d}.js` plus
per-domain assignments (`api.audio = …`), and the pure-JS install targets in
`src/prelude/prelude.js` (embedded via `tools/gen_prelude.py` → `prelude.h`).

Today: `args` is a native `JS_CFUNC_DEF`; `whiteTexture` is a root cgetset;
`loadText` is a root native; `mat4`/`vec3`/`quat` are prelude-assigned root
objects; there is no binary reader and no color vocabulary. `efx.io` is a new
binding-created namespace in the `efx.graphics` mold; `efx.math` and
`efx.color` are pure JS in the prelude. All in-repo callers use plain
`efx.<name>(…)` calls, so consumer migration is mechanical.

## Goals / Non-Goals

**Goals:**

- One `efx.math` (`mat4`/`vec3`/`quat`), one `efx.io` (`loadText`/`loadData`),
  and one `efx.color` (17 frozen constants) on both runtimes; `whiteTexture`
  lives only in `efx.graphics`; `efx.args` is a read-only property.
- Zero root remnants for the moved names; byte-identical rendered frames and
  byte-identical error output for the moved loaders; migrated tests, samples,
  docs, and types in the same change.
- `efx.io.loadData` returns a fresh `Uint8Array` copy; `efx.color` constants
  are frozen plain arrays.

**Non-Goals:**

- Any behavior change to `efx.math.*`, `efx.io.loadText`, or
  `efx.graphics.whiteTexture`; aliases or deprecation shims; color
  constructors/parsers/helpers; binary decoding; touching the lifecycle
  members or the `keyboard`/`mouse`/`window`/`physics`/`gamepad`/`audio`
  namespaces.
- Unmanaged-resource exposure is unchanged by this move: dynamic-count
  resources stay GC-finalized opaque classes with explicit `destroy()` (ADR
  0011, discipline ADR 0012); the light bank stays the one pre-allocated slot
  bank. Only the JS access path changes; class identity, lifecycle, and the
  fixed-limits table are untouched. `efx.color` adds plain JS-managed data and
  `efx.io` adds no resource type.

## Decisions

### D1 — Which namespace is binding-created vs prelude-defined

`efx.io` is created by the bindings (native `loadText`/`loadData`) and
attached like `efx.graphics`; the prelude installs nothing on it. `efx.math`
and `efx.color` are installed by the shared prelude as plain JS data/objects
(the move of the existing math objects, plus new frozen constant arrays).

*Alternatives considered:* (a) build `efx.io` purely in the prelude by
re-wrapping flat natives — rejected: it adds a JS frame to every resource read
and diverges from how `efx.graphics` is assembled. (b) build `efx.math`/
`efx.color` in the native bindings — rejected: they need no native facility
and would have to be duplicated across quickjs and the web bridge, breaking
the shared-prelude rule (ADR 0049).

### D2 — `whiteTexture` becomes a member of `GRAPHICS_FUNCS`

The existing `efx_js_whiteTexture` cgetset registration moves from `EFX_FUNCS`
into `GRAPHICS_FUNCS`; the C getter and its identity-stable host cache are
unchanged. On web the `Object.defineProperty(api, 'whiteTexture', …)` moves
from `api` to `api.graphics`. *Alternative:* keep the root property and define
`efx.graphics.whiteTexture` as an alias — rejected: the spec requires the root
name gone (no aliases).

### D3 — `args` is a read-only getter returning a fresh array

Desktop: `JS_CGETSET_DEF("args", efx_js_args, NULL)`; `efx_js_args` changes to
the getter signature and still builds the array from host state. Web: replace
the literal `args: function () {…}` with
`Object.defineProperty(api, 'args', { get: … })` (after the `api` literal is
assembled). Each read returns a new `string[]`, so a script cannot corrupt
engine state and the engine never exposes its internal argument storage.
*Alternative:* a single frozen array created at startup — rejected by the
chosen behavior (fresh copy) and because a stable array would still expose a
mutable `string[]` unless frozen; freezing alone does not stop a script from
observing identity-based coupling.

### D4 — `loadData` returns a fresh `Uint8Array`

Desktop: `efx_js_loadData` reads with `efx_resource_read` and returns
`JS_NewUint8ArrayCopy(ctx, bytes, size)` (the engine frees its buffer
immediately). Web: a new `_efx_bridge_load_data(path, lenPtr)` allocates the
bytes, writes the length into a JS-allocated 4-byte cell, and returns the
pointer; the JS wrapper reads `HEAPU8`/`HEAPU32`, copies into a new
`Uint8Array`, then frees both allocations. This keeps the copy owned by the JS
engine and the resource buffer lifetime entirely on the native side.

*Alternatives:* return an `ArrayBuffer` — rejected: `Uint8Array` is the binary
form already used for pixel buffers and the most convenient for scripts;
return a view into the native buffer — rejected: ownership/lifetime hazard;
return a plain `number[]` — rejected: inefficient and inconsistent with the
typed-array convention.

### D5 — Color palette: CSS basic 16 plus `transparent`, frozen, rounded levels

`efx.color` holds exactly the CSS basic-16 names plus `transparent`, each a
`[r, g, b, a]` array frozen with `Object.freeze`. The 128/192 sRGB levels are
expressed as `0.5`/`0.75` for readability (`gray` `[0.5,0.5,0.5,1]`, `silver`
`[0.75,0.75,0.75,1]`, `green` `[0,0.5,0,1]` distinct from `lime`
`[0,1,0,1]`). No functions.

*Alternatives:* exact `128/255`/`192/255` values — rejected: ugly constants
that no demo benefits from; the CSS extended set — rejected: larger than
needed for a demos vocabulary; a color constructor/hex parser — rejected:
out of scope and adds validation surface.

### D6 — Demos use the constants; golden scenes are not rewritten

The curated samples (`gallery/samples/curated/*/main.js`) and
`examples/browser/main.js` substitute a constant wherever a literal exactly
matches one (for example `[1,1,1,1]` → `efx.color.white`); custom palette
literals stay as literals. Golden-scene scripts under `tests/goldens/` are
re-pathed for `whiteTexture` but not restyled with color constants — they are
test fixtures, not demos, and the substitution would add churn with no
pixel effect.

*Alternative:* rewrite every literal in every fixture — rejected: test churn
and risk with zero demonstration value.

### D7 — Error text stays byte-identical for the moved loaders

`loadText` keeps `loadText requires a path string` (TypeError) and the
existing resource error texts (`resource not found`, `resource root could not
be opened`, `invalid resource path`, `resource read failed`, `out of memory`);
`loadData` mirrors them with `loadData requires a path string`. No message
embeds a namespace prefix, so the cross-runtime error catalog
(`tests/scripts/s_error_catalog.js` + `.expected.txt`) changes only by the
re-pathed trigger lines and the added `loadData` cases.

*Alternative:* namespace-qualify messages — rejected: breaks byte-identical
cross-runtime error output for no script-visible benefit.

### D8 — Type document: extracted interfaces, root members deleted, `args` property

`gallery/src/api/efx.d.ts` gains `interface EfxMath`, `interface EfxIo`, and
`interface EfxColor` (with the constant tuple literals); `interface Efx` loses
`args()`/`whiteTexture`/`loadText`/`mat4`/`vec3`/`quat` and gains
`readonly args: string[]`, `readonly math: EfxMath`, `readonly io: EfxIo`,
`readonly color: EfxColor`, and `graphics.whiteTexture`; TSDoc examples are
re-pathed. `efx.type-test.ts` proves the new forms type-check and the removed
root forms fail. `docs/api/` is regenerated and `docs:check` must pass.

*Alternative:* keep flat declarations plus a type-level alias — rejected: the
declaration must fail to compile on root-level use, or it would not
"accurately describe the valid call shapes" (js-api requirement).

### D9 — New ADR 0051 amending ADR 0050

ADR 0050 fixed the organization rule and explicitly listed `whiteTexture`,
`loadText`, and the math helpers as root members — this change contradicts
that list. A short new ADR (`docs/decisions/0051-api-namespace-followup.md`,
number confirmed at apply time) records the completed rule (root = lifecycle/
runtime facilities + domain sub-namespaces; `math`/`io`/`color` join
`graphics`/input/physics/gamepad/audio), that a constants namespace is frozen
plain data with no functions, that `args` is a read-only property, and that
`loadData` returns a `Uint8Array` copy. ADR 0050's index row is left as
written (the new ADR links forward).

*Alternative:* no ADR — rejected: the change amends a durable, cross-cutting
decision that ADR 0050 stated explicitly, and repo practice records follow-ups
as new numbered ADRs (`audio-source-model`, `physics-tunneling`).

### D10 — Internal prelude call sites route through the new public path

The CommonJS loader calls `efx.loadText` in three places
(`src/prelude/prelude.js`); they become `efx.io.loadText`. The math primitives
do not call the public math API (they use the internal `__efx*` helpers), so
only the install object moves to `efx.math`. *Alternative:* capture the loader
reference once at install time — rejected: a needless second style for calling
the public API from the `[JS]` layer.

## Risks / Trade-offs

- [Missed root-qualified call in a rarely-run script] → End-of-change audit:
  word-bounded repo grep for `efx.whiteTexture`, `efx.loadText`, `efx.mat4`,
  `efx.vec3`, `efx.quat`, and `efx.args(` must return zero outside
  `openspec/changes/archive/`, historical ADR text, and this change folder;
  both runtime suites run the whole corpus.
- [Membership drift between the three layers] → Extend the api-tests
  enumeration guard: `efx.math` is exactly `{mat4,vec3,quat}`, `efx.io` is
  exactly `{loadText,loadData}`, `efx.color` is exactly the 17 constants and
  every value is a frozen 4-tuple, `efx.graphics` is the 33 functions plus
  `whiteTexture`, `efx.args` is a non-function, and none of the moved names
  exists at the root; the cross-runtime compare covers the web side.
- [Color constants drift from the CSS values or are mutated] → A unit case
  asserts each of the 17 tuples and `Object.isFrozen`; `efx.color.white` must
  behave byte-identically to `[1,1,1,1]` in a golden scene.
- [Error-catalog fixture churn] → D7 keeps messages stable; the catalog runs
  unchanged on both runtimes and must stay byte-identical apart from the
  re-pathed triggers and the new `loadData` cases.
- [Web `loadData` length/ownership bug] → The bridge writes the length to a
  caller-provided cell and the wrapper copies before freeing; a portable
  script case loads a known binary fixture and compares length and bytes on
  both runtimes.
- [docs/api drift breaking the Pages build] → Regenerate `docs/api/` in the
  same change and run `npm --prefix gallery run docs:check`.
- [Golden suite suggests drift] → A re-path plus constant substitution cannot
  change pixels; any golden diff indicates an accidental behavior change —
  treat as a bug, never re-baseline.
- [Pre-existing malformed `js-api` main spec] → `openspec/specs/js-api/spec.md`
  currently carries stray `## ADDED Requirements` / `## MODIFIED Requirements`
  headers from the not-yet-archived `efx-graphics-namespace` sync; `openspec
  validate` reports that archive would refuse a delta against it. Reconcile
  that main spec (or archive `efx-graphics-namespace` first) before archiving
  this change; the change's own delta is valid.

## Migration Plan

One atomic in-repo change, no external migration (no scripts exist outside the
repo):

1. Engine: `runtime.c` (`args` cgetset, `whiteTexture` into `GRAPHICS_FUNCS`,
   new `IO_FUNCS` + attach), `src/api/api.c` (`efx_js_args` getter),
   `src/api/api_resource.c` (`efx_js_loadData`), `src/web/js/*.js`
   (`api.io`, `api.graphics.whiteTexture`, `args` getter) and
   `src/web/bridge_resource.c` (`_efx_bridge_load_data`); `prelude.js`
   (`efx.math`, `efx.color`, `efx.io.loadText` call sites); regenerate
   `prelude.h`.
2. Consumers: `tests/unit/api_tests.c` (membership guard + call sites),
   `tests/scripts/*.js` (`s_args.js`, `s_6a_*.js`, `s_error_catalog.*`, math
   and resource cases), the golden `main.js` files using `whiteTexture`, the
   web fixtures, and `tests/CMakeLists.txt` (paths only; add a portable
   `loadData` case).
3. Types & docs: `efx.d.ts` + `efx.type-test.ts`, regenerate `docs/api/`,
   update `docs/js-api.md` (Overview organization rule, color/resource tables,
   vision-traceability table), `README.md` mentions.
4. ADR: `docs/decisions/0051-*.md` (next free number, confirmed at apply
   time), indexed in `docs/decisions/README.md`.
5. Verification per AGENTS.md: commit → push branch →
   `python3 tools/verify_remote.py all <branch>` (native ctest incl. all
   goldens + Emscripten suite) → only if green dispatch
   `gh workflow run ci.yml --ref <branch>` (Linux → Windows → macOS); merge to
   `main` after the gate is green.

Rollback: `git revert` of the change series; no data, resource, or on-disk
format changes exist.

## Open Questions

None — the ADR number and the exact next-free color/ADR numbering are resolved
at apply time; the palette, `loadData` type, and `args` semantics were decided
before writing this design.
