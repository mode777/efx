# Proposal

## Why

Every script today is a single `main.js` evaluated as a classic global script.
There is no way to split authored code across files, and the two runtimes
cannot use ES modules: quickjs evaluates `main.js` with `JS_EVAL_TYPE_GLOBAL`
and the web build evaluates it through `new Function` (ADR 0022), so static
`import`/`export` are syntax errors on both. Native ES modules cannot close
that gap either — the browser module loader is URL-based and cannot reach
entries inside a fetched zip, and native browser modules run in the real
global scope, which would break the "no host dependencies, enforced by
construction" contract. This change adds a synchronous, provider-backed
**CommonJS** module system as the engine's module format and resolution, so
authored code can be split into files, `main.js` becomes an ordinary module,
and TypeScript can compile `import`/`export` down to `require` while keeping
full type safety.

This is a new **orthogonal milestone, F10 (script modules — CommonJS)**: its
only dependencies are the dual script bindings (F1–F2) and the dir/zip
resource provider (F6a), both green, and it is independent of F3–F9. It is an
explicit enabler of F8's pure-JS high-level layer.

## What Changes

- **Milestone F10 — script modules (CommonJS)** is added to the roadmap as an
  orthogonal milestone (predecessor gate: F1–F2 + F6a; independent of F3–F9),
  verified by script-level module tests on all four targets.
- **CommonJS becomes the engine's module format.** A module is a file under
  the resource root; `require(path)` resolves synchronously through the
  existing dir/zip provider (ADR 0031), reads the module, evaluates it in a
  wrapper scope exposing `require`, `module`, `exports`, and caches its
  `module.exports`. No new dependency, no new file layer.
- **`main.js` is evaluated as a CommonJS module** (an implicit entry module)
  on both runtimes. The global `update`/`render` load-time sugar keeps
  working, and the entry module's `module.exports.update`/`.render` are
  accepted as the module-shaped equivalent.
- **One shared module runtime.** The `require` runtime lives in the
  engine-bundled pure-JS layer (`src/prelude/`) and is driven by the
  synchronous `efx.loadText` provider, so desktop quickjs and the web engine
  run byte-identical module semantics (ADR 0022 parity).
- **A restricted, documented resolver.** Root-relative and `./`/`../`
  specifiers with explicit `.js`/`.json` extensions (plus a deterministic
  extension fallback); no `node_modules`, no bare specifiers, no directory
  indexes. Paths obey the resource root's escape rules.
- **Interop and semantics pinned:** `module.exports`/`exports` alias,
  `__esModule` interop (`__importDefault`/`__importStar`/`__exportStar`),
  circular-require partial exports, a `require.cache`, and `.json` modules.
- **The dependency restriction is unchanged and explicit.** Node built-ins
  (`fs`, `path`, `process`, `Buffer`, …) are not provided and host globals
  stay shadowed on web. npm-package compatibility is a **non-goal**; CommonJS
  is an authoring/tooling target, not a Node environment.
- **TypeScript alignment is documented:** `module: commonjs`, `target:
  es2015+`, `esModuleInterop: true`, `verbatimModuleSyntax: true`, so `tsc`
  type-checks the original ESM source and emits the `require` form the engine
  executes.
- **The web host entry-source channel is preserved:** a host-supplied
  `__efx_main_js` string is treated as the entry module source, and its
  `require`s resolve against the mounted root.

## Capabilities

### New Capabilities
- `script-modules`: the engine's CommonJS module format and resolution — the
  synchronous provider-backed `require` runtime, module caching and cycles,
  `module.exports`/`exports` semantics and `__esModule` interop, JSON
  modules, the restricted resolver and its path rules, the unsupported-syntax
  set, and the entry-module and hook-registration model.

### Modified Capabilities
- `feature-roadmap`: adds F10 (script modules — CommonJS) as an orthogonal
  tenth milestone with an explicit F1–F2 + F6a predecessor gate and a
  script-test verification strategy.
- `js-runtime`: the entry script is evaluated as a CommonJS module (not a
  bare global script) with a shared `require` runtime on both bindings;
  module resolution failures and exceptions surface through the existing
  error/exit contract; the no-host-dependency rule is preserved.
- `js-api`: clarifies that `require`/`module`/`exports` are module-scoped
  authoring facilities, not members of the `efx` namespace or free globals;
  extends the normative reference's milestone range to F1–F10 and documents
  the module model and the module-shaped `update`/`render` exports.

## Impact

- Code: new module runtime in `src/prelude/prelude.js` (embedded, shared by
  both bindings via `tools/gen_prelude.py`); `src/runtime/runtime.c` gains the
  module-aware entry evaluation and a desktop `require` seam; `src/player/
  player.c` and `src/player/repl.c` route `main.js` through the module loader;
  `src/web/entry.js` routes the entry source and host `__efx_main_js` through
  the same loader. No new C module and no new dependency — the provider
  (F6a) already reads the files.
- API/docs: `docs/js-api.md` gains the module-model section and the F10
  milestone tag; `gallery/src/api/efx.d.ts` gains module authoring types
  (`require`/`module.exports`) as needed; `AGENTS.md` roadmap and
  current-state sections gain F10; **new ADR `docs/decisions/0037`** (CommonJS
  is the engine module format; synchronous provider-backed resolution; the
  no-Node-compatibility non-goal); `vision.md` packaging/consumer-API sections
  name the module format.
- Dependencies: none new. No parser, no bundler, and no third-party module
  library is vendored — `require` is a small pure-JS runtime over the existing
  provider.
- Verification: new portable module smoke scripts run through both runtimes
  (ctest desktop + Emscripten, `tools/run_web_compare.mjs` cross-runtime
  diff), covering relative resolution, cache identity, cycles, `__esModule`
  interop, JSON modules, missing-module errors, escape rejection, and
  hook registration from a module. No golden-image test is added (modules
  have no visual surface).

## Non-goals

- **npm/Node compatibility.** Node built-ins, `node_modules`, bare
  specifiers, `package.json` resolution, and native addons are not provided;
  host globals stay shadowed. The no-browser/Node-dependency rule is
  unchanged.
- **ES module execution.** Static `import`/`export`, dynamic `import()`, and
  `import.meta` are not executed by the engine; ESM is a source format that
  TypeScript (or another transpiler) lowers to CommonJS before the engine
  sees it.
- **Top-level await** (not expressible in CommonJS) and asynchronous module
  loading.
- **A browser-native module layer** (blob URLs, import maps, Service Workers)
  — rejected because it cannot reach zip entries and would lose the
  host-global sandbox.
- **Tree-shaking, bundling, or minification** — a future tooling concern, not
  an engine concern.
- **Changing the resource packaging model** — modules are ordinary resources
  in the existing dir/zip root.
