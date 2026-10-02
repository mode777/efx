# Proposal

## Why

ADR 0050 moved the 33 graphics functions into `efx.graphics` and set the
organization rule "root = runtime/lifecycle facilities + domain
sub-namespaces", but the root still mixes those facilities with domain
helpers: the pure-JS math helpers (`mat4`/`vec3`/`quat`), the resource
loader `loadText`, and the engine-owned `whiteTexture` sit flat at the
root. Two gaps remain alongside: `args()` is a function where a read-only
property reads better, and there is no shared named-color vocabulary, so
every demo repeats RGBA literals. Consolidating the remaining root
facilities into `efx.math`, `efx.io`, and `efx.color` (and moving
`whiteTexture` into `efx.graphics`) finishes the namespace organization
ADR 0050 started.

## What Changes

- **New `efx.math` sub-namespace.** `efx.mat4`, `efx.vec3`, and `efx.quat`
  move to `efx.math.mat4` / `efx.math.vec3` / `efx.math.quat` with
  unchanged names, signatures, semantics, and return shapes.
  **BREAKING — hard cut, no root aliases.**
- **`whiteTexture` moves into `efx.graphics`.** `efx.whiteTexture` becomes
  `efx.graphics.whiteTexture`; it stays the same engine-owned 1×1 opaque
  white `Texture` (identity-stable, `destroy()` throws). **BREAKING.**
- **New `efx.io` sub-namespace.** `efx.loadText` moves to
  `efx.io.loadText` (unchanged behavior). `efx.io.loadData(path)` is added:
  it reads a resource as raw bytes and returns a fresh `Uint8Array` copy,
  with the same synchronous read, path rules, and error contract as
  `loadText`. **BREAKING** for `loadText`.
- **New `efx.color` namespace** of frozen named `Color` constants: the CSS
  basic 16 — `aqua`, `black`, `blue`, `fuchsia`, `gray`, `green`, `lime`,
  `maroon`, `navy`, `olive`, `purple`, `red`, `silver`, `teal`, `white`,
  `yellow` — plus `transparent` (`[0, 0, 0, 0]`). CSS half/three-quarter
  levels (`#808080`, `#C0C0C0`, `#008000`, …) are expressed as the readable
  `0.5` / `0.75` normalized values. Each constant is a plain `[r, g, b, a]`
  array frozen with `Object.freeze`; the namespace has no functions. The
  curated samples and examples use the constants wherever a literal matches.
- **`args()` becomes the read-only property `efx.args`.** Reading it returns
  a fresh `string[]` of the host `--script <file> [args…]` tail (empty array
  when none); mutating the result never affects the engine. **BREAKING** for
  `efx.args()` callers.
- **Three lockstep registration layers.** `efx.io` is created by the
  bindings (like `efx.graphics`); `efx.math` and `efx.color` are pure JS in
  the shared prelude. The native table (`src/runtime/runtime.c`), the web
  `api` assembly (`src/web/js/*.js`), and `src/prelude/prelude.js`
  (with regenerated `prelude.h`) all move in lockstep.
- **Docs/types.** `gallery/src/api/efx.d.ts` (new `EfxMath`, `EfxIo`,
  `EfxColor` interfaces; `args` property; re-pathed TSDoc), the gallery type
  test, regenerated `docs/api/`, and `docs/js-api.md` (organization rule +
  vision-traceability table). New ADR `docs/decisions/0051` amending 0050.

## Capabilities

### New Capabilities

- None — this reorganizes the existing script surface and adds one loader
  inside an existing domain; no new capability.

### Modified Capabilities

- `js-api`: **structural.** MODIFY "Single global API namespace" (root holds
  only lifecycle/runtime facilities; `math`/`io`/`color` join the
  sub-namespaces; `args` is a read-only property); MODIFY "Graphics namespace
  API" (`whiteTexture` joins `efx.graphics`, now 34 members); ADD "Math
  namespace API", "IO namespace API", and "Color namespace API"; MODIFY
  "Resource loading and texture composition" (`efx.io.loadText`/`loadData`);
  MODIFY "Gallery type document accuracy" and "Normative API reference
  document" paths.
- `3d-core`: MODIFY "Script math layer" to the `efx.math.mat4` /
  `efx.math.vec3` / `efx.math.quat` paths (behavior unchanged).
- `2d-layer`: MODIFY the texture requirement to `efx.graphics.whiteTexture`.
- `resource-loading`: MODIFY "Text loading" to name `efx.io.loadText`; ADD
  "Binary loading" for `efx.io.loadData` (raw bytes as `Uint8Array`, escape
  and missing-resource errors).
- `repl`: MODIFY "Optional resource root and interactive loading" to the
  `efx.io.*` loader paths.
- `js-runtime`: MODIFY "Native bridge binding contract" to the
  `efx.graphics.whiteTexture` path for the engine-owned resource object.

## Impact

- **Bindings (three lockstep layers):** `src/runtime/runtime.c` (remove
  `args`/`whiteTexture`/`loadText` from `EFX_FUNCS`; `args` becomes a
  cgetset; `whiteTexture` joins `GRAPHICS_FUNCS`; new `IO_FUNCS` +
  `efx_api`-style `io` object), `src/api/api.c` (`efx_js_args` getter) and
  `src/api/api_resource.c` (`efx_js_loadData` via `JS_NewUint8ArrayCopy`),
  `src/web/js/*.js` (`api.io`, `api.graphics.whiteTexture`, `args` getter,
  new `_efx_bridge_load_data` in `src/web/bridge_resource.c`), and
  `src/prelude/prelude.js` (`efx.math`, `efx.color`; internal
  `efx.loadText`/math call sites re-pathed; regenerate `prelude.h`).
- **Error text:** loaders keep the existing bare-name messages
  (`loadText requires a path string`, `resource not found`); `loadData`
  mirrors them, so the cross-runtime error catalog stays byte-identical apart
  from the added `loadData` cases and re-pathed trigger lines.
- **Types & generated docs:** `gallery/src/api/efx.d.ts`,
  `efx.type-test.ts`, regenerated `docs/api/`.
- **Guidelines & ADR:** `docs/js-api.md` (Overview "One namespace" rule,
  color/resource tables, vision-traceability table) and a new ADR
  `docs/decisions/0051` (next free number, confirmed at apply time) amending
  ADR 0050; indexed in `docs/decisions/README.md`. `README.md` mentions
  re-pathed.
- **Tests:** `tests/unit/api_tests.c` (membership guard + `whiteTexture` /
  `loadText` / math call sites), the portable script suite
  (`tests/scripts/s_args.js`, `s_6a_*.js`, `s_error_catalog.js` +
  `.expected.txt`, math/resource cases), all golden-scene `main.js` files
  using `efx.whiteTexture`, the web fixtures, and `tests/CMakeLists.txt`
  (paths only; no new case kind).
- **Samples/examples:** all curated `gallery/samples/curated/*/main.js`
  (color constants where literals match; `whiteTexture` re-path) and
  `examples/browser/main.js`.
- **Verification:** no new test kind; the existing gate applies (Linux
  server pre-filter via `tools/verify_remote.py all`, then the four-target
  gate Linux → Windows → macOS, Emscripten suite included). Golden pixels
  must be bit-identical — a re-path plus constant substitution cannot change
  any rendered frame.
- **Dependencies:** none added or removed.

## Non-goals

- No behavior change to the moved functions: `efx.math.*`, `efx.io.loadText`,
  and `efx.graphics.whiteTexture` keep their exact signatures, defaults,
  validation, and error messages.
- No deprecated root aliases, deprecation warnings, or compatibility shims
  (hard cut, pre-1.0, all consumers in-repo). `efx.args()` no longer exists
  as a call form.
- `efx.color` is constants only — no color constructors, parsers, hex
  strings, palette objects, or named-alpha variants; `efx.io.loadData` is a
  raw-byte reader only (no decoding, no image/mesh/audio interpretation).
- `log`, `quit`, `registerUpdateHook`, `registerRenderHook`, and the
  existing `keyboard`/`mouse`/`window`/`physics`/`gamepad`/`audio`
  namespaces are untouched; the `efx.graphics` member set only gains
  `whiteTexture`.
- No new resource class, limit change, or native-backed class; the
  fixed-limits table and resource classification table are unchanged.
- No milestone work: every F1–F14 roadmap item is done; this is a
  post-roadmap API reorganization applying ADR 0050's organization rule.
- `docs/api/` is regenerated, never hand-edited; archived change folders and
  historical ADR texts are left as written.
