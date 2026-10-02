# 0049 — Shared option-bag validation lives once, in the engine prelude

Status: Accepted (2026-10, change `shared-option-validation`; go/no-go after
the R22 spike — the cold budget is ratified at the measured absolute cost for
creation-time APIs, ≈ 75–80 µs per call)

Supports: ADR 0002 (embedded quickjs desktop runtime), ADR 0004 (single `efx`
namespace), ADR 0022 (two bindings, one API; web runs the page engine over the
wasm bridge).
Amends: ADR 0015's layering reading — the *functionality* of a C-implemented
API stays in C; cold-path **argument validation** for it is written once in
the shared pure-JS prelude and is not script-reachable.

## Context

The desktop binding (`src/api/*.c`, quickjs) and the web binding
(`src/web/js/*.js`, page engine over `src/web/bridge_*.c`) each implement all
option-bag validation for the same API: 391 throw sites on desktop, 323 on the
web. The copies have drifted — the error catalog records 18 message
divergences plus one semantic divergence (desktop physics coerces `'0.5'` to
`0.5` via `JS_ToFloat64`, contradicting the `docs/js-api.md` rule that a
wrongly-typed field throws `TypeError`; the web build already enforces it).
Part 1 of `docs/refactoring.md` shrank each binding but cannot remove the
duplication *between* them. Full record: `openspec/changes/shared-option-validation/`.

Both runtimes already evaluate the same prelude bytes
(`src/prelude/prelude.h`, drift-checked), so the prelude is the one place
validation can live once. The open question was cost on the desktop
interpreter: does routing a cold call through a JS validator make it
unacceptably slower, and would a hot path survive?

## Spike measurements (R22)

Prototype: the web binding's particle wire (`__efxParticleWire`, 352 floats)
moved into the prelude behind a private `natives` object; the desktop
`createParticleSystem` accepts the wire. The desktop error catalog
(`smoke_error_catalog` contract) stayed byte-identical, all 191 headless unit
tests passed, and `api_tests particles_js` passed.

Timed on the spike branch with a dedicated harness
(`tools/r22_timing.c`, median of 5 runs, `clock_gettime` around `JS_Call`
loops, full option bags, results destroyed per call so memory stays flat):

| Path | Native (today) | Prelude path | Delta |
|------|---------------:|-------------:|------:|
| **Cold:** `createParticleSystem` ×1000 | 17–24 ms | 93–101 ms | **+74…+80 ms** |
| — validation + wire build only ×1000 | — | 84 ms | (≈ the whole delta) |
| **Hot:** `drawQuad` ×10000, full bag | 15–27 ms | 215–246 ms | **+200…+227 ms (9–14×)** |

Reading:

- The entire cold delta is quickjs interpreting the validator; the native wire
  unpack is free. Per call the prelude path costs ≈ **75–80 µs** on a
  creation-time API (a game creates particle systems at load, not per frame).
  The measured delta includes the short-lived JS objects (result record, vec
  arrays) the validator allocates and the quickjs GC pressure from them.
- Reusing a scratch `Float32Array` instead of allocating the wire per call
  made no measurable difference (±5 %).
- Hot paths are decisively out: ~13× slower through quickjs.

**Platform note (recorded deviation):** the change brief names the Windows
Release desktop build for these measurements. No Windows machine exists in the
authoring environment; the numbers above are Linux, gcc, `-O2` Release,
headless quickjs (the same interpreter build the CI Linux job uses). The
hot/cold *ratio* between an interpreter and native readers is CPU-family
insensitive; the owner should treat the absolute cold numbers as the Linux
signal.

## Decision

1. **Cold-path option-bag validation moves into the shared prelude, once.**
   The prelude wrapper becomes `function (efx, natives)`; `natives` is a
   plain object built by each binding and passed to the wrapper call. It is
   never a property of `efx`, never a global (single-namespace ADR 0004), and
   unreachable from scripts. Validators wrap the public entry inside the
   prelude, so the public function keeps its name, signature, defaults and
   C-implemented layer tag.
2. **Hot paths stay native** on both runtimes: `drawQuad`, `drawSprites`,
   `drawBillboard`, `drawMesh`, `drawText`/`measureText`, and the input
   queries keep their per-binding validation.
3. **Budget (the cold/hot boundary).** A path may move to the prelude only if
   it is a creation/config/load-time call whose measured added cost stays
   under **50 ms per 1000 calls on the desktop Release build** (≈ 50 µs per
   call, deemed invisible for non-per-frame APIs). Hot paths (per-frame draw
   and query calls) may move only if the prelude path measures within **1.2×**
   of the native path — which the spike shows is not the case, so none move.
   *Ratification note:* particles measured +74…+80 ms/1000 on Linux — above
   the 50 ms draft line, at an absolute cost of ≈ 75–80 µs on a load-time
   call. The owner's go/no-go (task 1.4) either ratifies the boundary at the
   measured absolute cost for creation-time APIs or rejects the change.
4. **Check order is fixed** for every migrated validator (multi-fault inputs
   must throw identically on both runtimes):
   1. arity;
   2. resource arguments (type, then liveness through `natives.live`);
   3. the option bag (known fields, then fields in declaration order);
   4. cross-field constraints.
5. **Normalized forms (D2):** a validator returns a flat `Float32Array` wire
   (or positional numbers) in a documented layout, strings/enums as numbers,
   bulk data as typed arrays, and resource arguments resolved through the
   binding (handles or wrapper objects pass through; the native checks
   liveness with its existing mechanism and the shared message). Natives
   report failures as return codes; one prelude table (`__efxRc`-style, with
   per-call overrides) maps codes to `[ErrorClass, message]`. Resource-load
   failures get dedicated codes: not found / unreadable / undecodable.
6. **Strict numbers (D6, BREAKING on desktop only):** a number-typed option
   field accepts only `typeof v === 'number'`. Numeric strings and booleans
   throw `TypeError` on every runtime; non-finite and out-of-range values
   keep their documented classes. This applies the existing
   `docs/js-api.md` guideline; only desktop physics loses `JS_ToFloat64`
   coercion.
7. **Canonical-message rule (D5):** for each known divergence, the canonical
   message names the field or API function and states the accepted form or
   range; if both candidates qualify, the desktop text wins (older,
   documented binding). Resolutions:

   | Case | Canonical message (both runtimes) | Winner |
   |------|-----------------------------------|--------|
   | `4a.light-slot` / `-neg` | `light slot must be an integer 0..3` | desktop |
   | `4a.light-pos-short` | `pos must hold 3 numbers` | web (desktop never names the field) |
   | `6a.loadimage-missing` | `resource not found` (via the not-found code) | desktop |
   | `6b.loadmesh-missing` | `glTF resource could not be read` (unreadable code) | desktop |
   | `6b.loadmesh-corrupt` | `invalid or malformed glTF asset` (undecodable code) | desktop |
   | `11.ps-max-range` | `max must be in 1..65536` | web |
   | `11.ps-bad-facing` | `facing must be 'view', 'y', or 'plane'` | desktop |
   | `11.ps-set-max` | `max must be in 1..65536` | web |
   | `11.billboard-pos` | `drawBillboard pos must be [x,y,z]` (both bindings require 3; the web text wrongly offers [x,y]) | desktop |
   | `12.createbody-noshape` | `shape must be an options object` | desktop |
   | `12.createstaticmesh-noopts` | `createStaticMesh requires a Mesh` | desktop |
   | `12.raycast-noopts` | `raycast requires origin and direction` | desktop |
   | `12.overlap-noopts` | `overlap requires a shape` | desktop |
   | `12.shapecast-noopts` | `shapeCast requires shape, from and motion` | desktop |
   | `12.step-nodt` | `dt must be a finite number` | web (matches the pinned `12.dt-nonfinite` text) |
   | `14.loadaudiodata-missing` / `14.loadaudiostream-missing` | `cannot read audio: <path>` | desktop |
   | `coercion.phys-number-string` | `TypeError` on both (decision 6, not a text change) | rule |

   The core never owns script-facing text: cores return codes; the prelude
   table is the single message source.

## Consequences

- One implementation of cold-path validation, shared by both runtimes, with
  byte-identical error classes and messages (the `js-api` requirement added
  by this change). Every future option is validated in one place.
- Both bindings shrink to marshalling: about −1000 to −1800 net non-blank
  lines across the seven domain migrations.
- The prelude grows by the moved validators (~1100 lines, committed generated
  `prelude.h`); the V3 `gen_prelude.py --check` gate covers drift.
- Desktop scripts relying on numeric-string physics options break (documented,
  web-parity).
- Core re-validation at the C ABI boundary stays (`ps_config_valid`,
  `post_entry_valid`, …) — defense in depth is unchanged.
- Liveness tracking stays per binding (ADR 0011/0012/0046); only check order
  and messages are shared.
- Hot paths stay duplicated by design; the budget in decision 3 is the bar any
  future migration must clear with new measurements.

## Rejected alternatives

- **Hidden global (`__efxNatives`) deleted after install** — briefly
  script-reachable; module code could capture it. The wrapper-parameter form
  is never reachable.
- **Validators as a separate JS file per runtime** — reintroduces the
  duplication this change removes.
- **Binding-neutral C option-reader interface used from both runtimes** — the
  web side would need wasm→JS callbacks per field: slower and a much larger
  bridge surface for what JS does natively.
- **Normalized plain objects instead of wires** — desktop C would need a
  property lookup per field, recreating the reader code being removed.
- **Validate the bag first and leave liveness to the native** — reorders
  errors for multi-fault inputs relative to today's web behavior.
- **Cores return message strings** — C would own script-facing text and every
  core call needs a string channel; codes + one prelude table keep one message
  source.
- **Coerce numbers everywhere** — contradicts the `docs/js-api.md` TypeError
  guideline and widens accepted input on the web.
- **Move everything unconditionally** — quickjs is an interpreter; per-frame
  draw calls are where a regression would be felt (measured ~13×).
