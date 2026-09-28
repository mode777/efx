# Tasks

## 1. Module runtime (shared pure JS)

- [ ] 1.1 Implement the CommonJS runtime in `src/prelude/prelude.js` (`require`, `module`, `exports`, module cache, resolved-path keying, cycle handling) and verify it with a headless prelude unit test that requires a two-module graph and asserts export identity
- [ ] 1.2 Implement the restricted resolver (root-relative + `./`/`../`, exact path then deterministic `.js` fallback, `.json` modules, escape rejection, unsupported-specifier rejection) and verify each rule with resolver unit tests
- [ ] 1.3 Implement `__esModule` interop (`__importDefault`/`__importStar`/`__exportStar`) against committed TypeScript-`commonjs` transpiled fixtures and verify the fixtures load with correct default/named/star bindings
- [ ] 1.4 Implement module error reporting with the resolved module path in the diagnostic and a `//# sourceURL=<path>` on evaluated bodies, and verify a throwing module and a missing module produce path-naming diagnostics
- [ ] 1.5 Regenerate `src/prelude/prelude.h` via `tools/gen_prelude.py` and verify `python3 tools/gen_prelude.py --check` passes (no drift)

## 2. Desktop binding (quickjs)

- [ ] 2.1 Route `main.js` through the shared module runtime in `src/runtime/runtime.c` (entry evaluated as a module) and verify the existing smoke suite still passes with global `update`/`render`
- [ ] 2.2 Register module-shaped entry hooks (`module.exports.update`/`render`) with the same ordering as the global sugar and no double registration, and verify with a module-entry script test
- [ ] 2.3 Route `--script` and the REPL `main.js` load (`src/player/player.c`, `src/player/repl.c`) through the module runtime and verify `--script` and `--repl` ctest cases still pass

## 3. Web binding (browser engine)

- [ ] 3.1 Route the entry source and the host `__efx_main_js` channel through the shared module runtime in `src/web/entry.js`, keeping the host-global shadowing and the consumed-before-evaluation rule, and verify the browser harness runs a multi-module entry
- [ ] 3.2 Verify module resolution uses the mounted zip root on web (no synchronous-fetch escape) and that a missing module surfaces the failure exit code

## 4. Tests and cross-runtime parity

- [ ] 4.1 Add portable module smoke scripts (relative/parent resolution, cache identity, cycle, interop fixtures, JSON module, missing module, escape rejection, unsupported syntax) and verify the desktop ctest smoke suite runs them
- [ ] 4.2 Wire the module smoke scripts into the Emscripten ctest suite and verify `tools/run_web_compare.mjs` reports identical desktop/web output
- [ ] 4.3 Verify the no-Node/no-host-dependency rule for modules: a module requiring `fs` and a web module referencing `window`/`process` both fail rather than silently receiving them

## 5. Documentation

- [ ] 5.1 Write ADR `docs/decisions/0037` (CommonJS is the engine module format; synchronous provider-backed resolution; no-Node-compatibility non-goal) per `TEMPLATE.md` and add its row to `docs/decisions/README.md`
- [ ] 5.2 Update `vision.md` (Packaging / Consumer API Design) to name the CommonJS module format, synchronous `require` resolution from the dir/zip root, and the TypeScript `import`→CommonJS authoring path
- [ ] 5.3 Update `docs/js-api.md` with the module-model section (format, resolver, caching/cycles, JSON, entry hooks, unsupported forms, no-Node rule) and the F10 tag; update `gallery/src/api/efx.d.ts` with the module authoring types
- [ ] 5.4 Update `AGENTS.md` roadmap table and current-state section with F10 and its gate, and reconcile the `feature-roadmap` delta with the pending `f9-input` change

## 6. Verification

- [ ] 6.1 Run the Linux signal first (server verification per `docs/verification-server.md`: `python3 tools/verify_remote.py all <branch>`) and fix any failure before dispatching the gate
- [ ] 6.2 Dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) and verify the native ctest suites (incl. module smoke tests), Emscripten ctest, and the cross-runtime comparison are green on Linux, Windows, and macOS; no golden-image test is added
