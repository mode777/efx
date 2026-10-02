# 0051 — Remaining root facilities move into sub-namespaces; `args` is a property

Status: Accepted

## Context

ADR 0050 moved the 33 graphics functions into `efx.graphics` and stated
the organization rule "the root holds only runtime/lifecycle facilities
and domain sub-namespaces", but its decision text still listed
`whiteTexture`, `loadText`, and the `efx.mat4`/`efx.vec3`/`efx.quat`
helpers as root members. The root therefore kept three domain helpers
flat and had no vocabulary for named colors, while `args` was a function
even though the host arguments are fixed at startup. Change
`openspec/changes/efx-namespace-consolidation/` holds the full process
record.

## Decision

The remaining non-lifecycle root facilities SHALL move into
sub-namespaces, completing the ADR 0050 rule:

- `efx.mat4`/`efx.vec3`/`efx.quat` become `efx.math.mat4` /
  `efx.math.vec3` / `efx.math.quat` (pure JS in
  `src/prelude/prelude.js`).
- `efx.whiteTexture` becomes `efx.graphics.whiteTexture` (the existing
  native cgetset moves into `GRAPHICS_FUNCS`).
- `efx.loadText` becomes `efx.io.loadText`, and `efx.io.loadData(path)`
  is added: it reads raw bytes and returns a fresh `Uint8Array` copy
  (`JS_NewUint8ArrayCopy` on desktop; a `_efx_bridge_load_data` bridge
  call with a caller length cell on web). `efx.io` is binding-created
  like `efx.graphics`.
- `efx.color` is added as a **constants namespace**: the CSS basic 16
  (`aqua`, `black`, `blue`, `fuchsia`, `gray`, `green`, `lime`,
  `maroon`, `navy`, `olive`, `purple`, `red`, `silver`, `teal`,
  `white`, `yellow`) plus `transparent`, each a `[r, g, b, a]` array
  frozen with `Object.freeze`; the 128/192 sRGB levels are expressed as
  `0.5`/`0.75`. A constants namespace contains no functions.
- `efx.args()` becomes the read-only property `efx.args`, a fresh
  `string[]` on every access.

The `efx` root therefore holds only `log`, `quit`, the `args` property,
`registerUpdateHook`, `registerRenderHook`, and the domain
sub-namespaces (`graphics`/`math`/`io`/`color`/`keyboard`/`mouse`/
`window`/`physics`/`gamepad`/`audio`). Every move is a hard cut with no
root alias and no deprecation shim; moved behavior, defaults, and error
message text stay byte-identical.

## Consequences

Scripts use `efx.math.*`, `efx.io.*`, `efx.color.*`, and
`efx.graphics.whiteTexture`; the old root paths are compile-time errors
in the gallery type document and runtime `undefined` calls otherwise.
All in-repo consumers were re-pathed in the same change. The membership
guard (`graphics_ns_js` in `tests/unit/api_tests.c`) pins the exact
shape of every sub-namespace and asserts no moved name remains at the
root; `docs/js-api.md` and the generated `docs/api/` reference carry the
new paths. Future constant-only domains follow the `efx.color` pattern:
frozen plain data, no functions. `efx.io.loadData` is a raw-byte reader
only — no decoding or asset interpretation.

## Rejected alternatives

- **No ADR; treat this as pure cleanup** — lost because ADR 0050's
  decision text explicitly named these symbols as root members, so the
  change amends a durable, cross-cutting decision and must be recorded.
- **Return an `ArrayBuffer` from `loadData`** — lost because
  `Uint8Array` is the binary form already used for pixel buffers and the
  most convenient for scripts; a view into the native buffer was
  rejected for lifetime/ownership hazards.
- **Exact `128/255`/`192/255` color levels** — lost because the
  normalized values are ugly and no demo benefits; `0.5`/`0.75` are the
  readable, conventional choice.
- **A color constructor/hex parser instead of constants** — lost
  because it is out of scope and adds validation surface; the request
  was named constants only.
- **Keep `args()` as a function** — lost because the arguments are fixed
  at startup and a read-only property is the simpler, safer shape.
