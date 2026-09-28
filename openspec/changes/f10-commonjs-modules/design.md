# Design

## Context

Today a run has exactly one script: the player reads `main.js` through the
dir/zip provider and evaluates it as a classic global script — `JS_Eval(...,
JS_EVAL_TYPE_GLOBAL)` on desktop (`src/runtime/runtime.c`) and a `new Function`
body with shadowed host globals on web (`src/web/entry.js`). The global
`update`/`render` sugar is picked up after evaluation. There is no module
concept, and neither evaluation context accepts `import`/`export`.

The two runtimes must stay behaviorally identical (ADR 0022), and the script
API must stay synchronous with zero browser/Node dependencies (ADR 0031,
js-runtime). Native ES modules are not an option: the browser's loader is
URL-based and cannot address zip entries, and native browser modules run in
the real global scope, so host globals could not be shadowed. See
`proposal.md` — Why, and the `script-modules` spec for required behavior.

## Goals / Non-Goals

**Goals:**
- Split authored code across files, loaded synchronously from the same dir/zip
  root, with identical semantics on desktop and web.
- Make `main.js` an ordinary module while keeping the existing global
  `update`/`render` scripts working unchanged.
- Let TypeScript author with `import`/`export` and keep full type safety, by
  compiling to a format the engine executes.
- Keep the module runtime small, standard, and dependency-free.

**Non-Goals:** ES-module execution, dynamic `import()`, `import.meta`,
top-level await, npm/Node compatibility (`node_modules`, built-ins), a
browser-native module layer (blob URLs, import maps, Service Workers),
bundling/tree-shaking/minification, and any change to the resource packaging
model.

## Decisions

**D1 — CommonJS is the engine module format.** `require(path)` returns
`module.exports` synchronously; modules are files under the resource root.
This is the only format that simultaneously satisfies `import`-style
authoring, zip packaging, synchronous boot, and the web host-global sandbox:
`require` is just a synchronous function over the existing provider, evaluated
in the same classic scope the engine already controls.
*Rejected: native ES modules on both runtimes* — quickjs could do it, but the
browser loader needs URLs (zip entries have none) and native modules run in
the real global scope, losing the sandbox and async-loading parity.
*Rejected: an engine-owned ESM-subset linker* — it would require the engine to
parse and lower `import`/`export` and to define a partial ESM semantic model;
delegating that transform to a standard transpiler is smaller and safer.
*Rejected: a custom `efx.import(path)` API* — returns `any`/`unknown` to
TypeScript, losing inference at module boundaries, and is not standard
resolution.

**D2 — One shared pure-JS `require` runtime in the prelude.** The module
runtime lives in `src/prelude/prelude.js` (embedded and shipped identically to
both bindings by `tools/gen_prelude.py`) and is driven by the synchronous
`efx.loadText` provider. Desktop quickjs and the web engine therefore execute
byte-identical module code — resolution, caching, cycles, interop, and errors
cannot drift.
*Rejected: native quickjs modules on desktop and a custom loader on web* —
two module engines would diverge on cycles, live bindings, and error timing,
exactly the class ADR 0022 exists to prevent.
*Rejected: a C-side loader duplicated per runtime* — duplicated resolution and
interop logic with no shared source of truth.

**D3 — A restricted, deterministic resolver.** Specifiers are root-relative or
relative to the requiring module (`./`, `../`); resolution tries the exact
path then a deterministic `.js` fallback; `.json` loads as a module. No bare
specifiers, no `node_modules`, no `package.json`, no directory indexes, no
native addons. Paths obey the resource root's escape rules (no `..` escape).
Determinism and a small, auditable surface matter more than Node parity.
*Rejected: full Node resolution* — large, ambiguous, and its `node_modules`
model has no meaning inside a zip and conflicts with the no-dependency rule.
*Rejected: aggressive extension/index guessing* — non-deterministic and hard to
align with `tsc`'s resolver.

**D4 — Pin the TypeScript/Babel CommonJS interop contract.** The runtime
implements `module.exports`/`exports` aliasing, `__esModule`, and the
`__importDefault`/`__importStar`/`__exportStar` helpers so transpiled ESM
output loads correctly, plus circular-require partial exports and a
`require.cache`. The reference implementation is the `commonjs` transform of
`tsc` (and Babel), not an ad-hoc shape.
*Rejected: raw-CJS-only semantics* — would break the transpiled output that is
the whole point of the format.
*Rejected: a custom export shape* — non-standard and would break interop with
transpilers.

**D5 — Entry module plus hook compatibility.** `main.js` is evaluated as a
module. Global `update`/`render` remain load-time sugar; additionally
`module.exports.update`/`module.exports.render` are accepted with the same
ordering (after explicitly registered hooks, in load order). If both forms are
present the hook is registered once, not twice. Modules required by the entry
may register hooks during evaluation.
*Rejected: globals only* — no module-shaped entry, and exported hooks are the
natural ESM/TS result.
*Rejected: exports only* — breaks every existing script and the documented
sugar.

**D6 — Errors carry the module path; `sourceURL` for traces.** A module
evaluation error, a resolution failure, and an unsupported construct all
surface through the existing error/exit contract, with the resolved module
path in the diagnostic. Evaluated module bodies get a `//# sourceURL=<path>`
so stack traces name the file instead of `<anonymous>`, keeping desktop/web
diagnostics equivalent.
*Rejected: bare `new Function` bodies* — traces would be anonymous and
unlocatable, weakening the error contract.

**D7 — The dependency restriction is unchanged and explicit.** No Node
built-ins or Node globals are provided; the web host-global shadowing applies
to module evaluation. npm-package compatibility is a non-goal, stated in the
reference so the guarantee is not traded away for a package that touches
`process`/`Buffer`.
*Rejected: a partial Node shim (`process`, `Buffer`)* — it would make some
packages load but violates "zero host dependencies, even transitively" and
invites packages that then fail unpredictably.

**D8 — TypeScript alignment is documented, not enforced by the engine.**
The recommended compiler settings are `module: commonjs`, `target: es2015+`,
`esModuleInterop: true`, `verbatimModuleSyntax: true`, with a
`moduleResolution` whose emitted specifiers the resolver accepts. `tsc`
type-checks the original ESM source; the engine only executes emitted JS.
Raw-JS authors write `require` directly or run a transpile.
*Rejected: shipping a transpiler in the engine* — duplicates standard tooling
and grows the runtime; *rejected: requiring no build step* — would forfeit the
`import`/type-safety benefit the change exists to provide.

**D9 — Sequencing as an orthogonal milestone (F10).** Predecessors are F1–F2
(dual bindings) and F6a (the provider), both green. F10 is declared orthogonal
to F3–F9 and is an explicit enabler of F8. The roadmap delta also carries the
pending F9 entry so the ladder stays coherent; it must be reconciled with
`f9-input` at archive.

## Risks / Trade-offs

- [`tsc` resolver vs runtime resolver diverge] → pin the compiler settings,
  document the supported/unsupported specifier forms, and reject unsupported
  forms loudly at load time; add a fixture compiled from ESM and run on both
  runtimes.
- [Interop helper drift as `tsc` evolves] → test against committed transpiled
  fixtures rather than hand-written CJS, so a helper change is caught.
- [Anonymous stack traces] → `sourceURL` on every module body (D6), asserted
  by an error test.
- [Circular-require semantics are subtle] → implement the documented partial-
  exports rule and cover it with a dedicated cross-runtime test.
- [Prelude growth and drift] → the prelude stays the single source, checked by
  `tools/gen_prelude.py --check`; the module runtime is plain ES6 with no host
  APIs.
- [Roadmap delta overlaps `f9-input`] → this delta carries F9 so the ladder is
  coherent; reconcile at archive (a task records this).
- [Host entry-source channel] → a host `__efx_main_js` string is treated as the
  entry module source and its `require`s resolve against the mounted root; the
  channel is still consumed before evaluation (ADR 0030).

## Migration Plan

Additive. Existing `main.js` files that define global `update`/`render` and
load assets at top level keep working unchanged; the only difference is that
`main.js` is now wrapped as a module (its `require`/`module`/`exports` become
available). No file format, packaging, or API migration exists. Rollback is
removing the module runtime and restoring the classic entry evaluation.

## Open Questions

- The exact extension-fallback set beyond `.js` (e.g. whether `.mjs` is
  accepted as a synonym) — an implementation detail pinned by the resolver
  tests; no spec change.
- Whether `require.cache`, `require.resolve`, `__filename`, or `__dirname` are
  exposed in v1 — internal surface, decidable during implementation without
  changing the approach.
- Whether the gallery's in-browser TypeScript transpiler emits CommonJS or ESM
  — a gallery-tooling detail; if it emits ESM, the gallery must configure it to
  CommonJS (or the engine accepts an ESM source only after transpilation).
