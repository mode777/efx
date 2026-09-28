# 0037 — CommonJS is the engine module format, resolved synchronously from the resource root

Status: Accepted (2026-09, change `f10-commonjs-modules`)

Supports: vision.md (scripts split across files; TypeScript authoring with
`import`); ADR 0004 (single `efx` namespace); ADR 0016 (loading `main.js` is
the implicit init); ADR 0022 (quickjs desktop-only; the browser engine drives
the core behind the sandbox); ADR 0030 (host entry-source channel); ADR 0031
(the dir/zip provider and synchronous resource API).

## Context

A run has exactly one script, `main.js`, evaluated as a classic global script —
`JS_EVAL_TYPE_GLOBAL` on desktop quickjs and a `new Function` body with
shadowed host globals on web. Neither context accepts static `import`/`export`,
and the two runtimes must stay behaviorally identical (ADR 0022) while keeping
the script API synchronous and free of browser/Node dependencies (ADR 0031).
Native ES modules cannot serve: the browser module loader is URL-based and
cannot address entries inside a fetched zip, and native browser modules execute
in the real global scope, so the host-global shadowing that enforces the
dependency rule would be lost. A module format is needed that supports
`import`-style authoring, zip packaging, synchronous boot, and the web sandbox
at once. See `openspec/changes/f10-commonjs-modules/` for the full record.

## Decision

- **CommonJS is the engine's module format and resolution.** Every script file
  under the resource root is a module; `require(path)` resolves and evaluates
  synchronously through the dir/zip provider (ADR 0031) and returns
  `module.exports`. `main.js` is the entry module.
- **One shared runtime in the engine-bundled pure-JS layer**
  (`src/prelude/prelude.js`, shipped identically to both bindings by
  `tools/gen_prelude.py`, ADR 0022) implements resolution, caching,
  circular-require partial exports, `module.exports`/`exports`, the
  `__esModule` interop contract (`__importDefault`/`__importStar`/
  `__exportStar`), and JSON modules. Desktop quickjs and the web engine run
  byte-identical module code.
- **A restricted, deterministic resolver:** root-relative or relative to the
  requiring module, exact path then a deterministic `.js` fallback, `.json`
  modules, resource-root escape rules. No bare specifiers, no `node_modules`,
  no `package.json`, no directory indexes, no native addons.
- **ESM is a source format, not an execution format.** TypeScript compiles
  `import`/`export` to CommonJS (`module: commonjs`, `target: es2015+`,
  `esModuleInterop: true`, `verbatimModuleSyntax: true`) before packaging; the
  engine never executes `import`/`export`, dynamic `import()`, or
  `import.meta`.
- **The dependency restriction is unchanged.** No Node built-ins or Node
  globals are provided, host globals stay shadowed on web, and npm-package
  compatibility is an explicit non-goal.
- **Entry hooks:** global `update`/`render` remain load-time sugar; the entry
  module's `module.exports.update`/`render` are accepted with the same
  ordering, and a hook present in both forms registers once.

## Consequences

- Authored code can be split across files, loaded from a directory or a zip
  with identical behavior on all four targets, and TypeScript keeps full type
  safety because `tsc` type-checks the original ESM source.
- The engine owns a small, standard module runtime instead of a bespoke ESM
  linker; the syntax transform is delegated to standard tooling.
- Module resolution is intentionally smaller than Node's: code that relies on
  `node_modules`, bare specifiers, directory indexes, or Node built-ins will
  not run, and this is a stated contract rather than an accident.
- The recommended TypeScript settings and the unsupported-syntax set are part
  of the compatibility contract; the reference document must state them, and a
  divergence between `tsc`'s resolver and the engine's resolver is the main
  compatibility risk to guard with tests.
- `require`/`module`/`exports` are module-scoped facilities, never members of
  the `efx` namespace or free globals (ADR 0004 stays intact).

## Rejected alternatives

- **Native ES modules on both runtimes** — quickjs could do it, but the
  browser loader cannot reach zip entries and native modules run unsandboxed
  and asynchronously, breaking ADR 0022 parity and the no-host-dependency
  guarantee.
- **An engine-owned ESM-subset linker** — the engine would parse and lower
  `import`/`export` and define a partial ESM semantic model; delegating the
  transform to a standard transpiler and implementing the standard CommonJS
  runtime is smaller and safer.
- **A custom `efx.import(path)` API** — returns `any`/`unknown` to TypeScript,
  losing inference at module boundaries, and is not standard resolution.
- **Full Node resolution** — `node_modules`/`package.json`/extension guessing
  have no meaning inside a zip, are non-deterministic, and conflict with the
  zero-dependency rule.
- **A partial Node shim (`process`, `Buffer`, built-ins)** — would make some
  npm packages load while violating "zero host dependencies, even
  transitively" and failing unpredictably later.
- **A browser-native module layer (blob URLs, import maps, Service Workers)** —
  cannot address zip entries and/or loses the host-global sandbox; async
  loading would diverge from the desktop runtime.
