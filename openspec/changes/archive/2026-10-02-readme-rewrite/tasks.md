# Tasks

## 1. Reconnaissance

- [x] 1.1 Resolve the published sample-gallery URL from the repository's Pages configuration (`gh api repos/mode777/emotion-fx/pages`) and record it for the README's "try it in the browser" link; verify the URL responds (or fall back to the repository Pages index) and that no hostname is guessed — resolved from API `html_url`: `https://mode777.github.io/emotion-fx/` (HTTP 200)
- [x] 1.2 Inventory the current README's sections and label each as end-user or maintainer, so nothing is dropped; verify every section maps to either the new README outline or `CONTRIBUTING.md` — end-user: intro, player/runtime + resource root + `main.js` + `--script` + `efx` namespace, "JavaScript API", "Running"; maintainer: "What F2 delivers" status list, "Repository layout", "Building", "Testing / the F2 gate", "Continuous integration", "Notes" (vendoring/sokol/emsdk)

## 2. Create `CONTRIBUTING.md`

- [x] 2.1 Create `CONTRIBUTING.md` and move the maintainer content into it: the from-source build matrix (Linux/Windows/macOS/Emscripten), headless `-DEFX_HEADLESS=ON` builds, the ctest smoke/golden harness, golden regeneration, the verification server workflow, the four-target CI gate and ADRs; verify none of it remains in the README — created, with the build matrix, headless build, testing/goldens, verification server, CI, and notes sections
- [x] 2.2 Add the contributor orientation to `CONTRIBUTING.md`: repository layout/codebase map, vendored dependencies, and pointers to `AGENTS.md`, `openspec/`, and `docs/` (including the auto-generated `docs/api/`); verify each linked path exists — layout, vendoring, and documentation/process sections added; all referenced paths verified to exist

## 3. Rewrite `README.md`

- [x] 3.1 Rewrite `README.md` around the end-user outline (D3): title/tagline, what EmotionFX is, capabilities, get it (release downloads + browser gallery link from 1.1), quick start (`main.js` `update`/`render`), resource-root + CommonJS module model, an `efx` API tour, run modes (windowed / `--script` / `--repl`), platform support, and a short "Building from source / Contributing" pointer to `CONTRIBUTING.md`; verify no F1/F2 status line, milestone ladder, or maintainer build/CI instruction remains — rewritten; gallery link uses the resolved URL; no status/milestone or build/CI text remains
- [x] 3.2 Ensure the README's quick-start snippet matches the real entry contract and mirrors `examples/hello/main.js`; verify the snippet is syntactically runnable and the documented run command (`player <resource-root>`) matches `AGENTS.md` — quick start uses the same `update`/`render` + 60-frame quit shape as `examples/hello`; both code snippets pass `node --check`; run command matches `AGENTS.md`
- [x] 3.3 Point deeper API exploration at existing, non-duplicated sources: the generated reference `docs/api/`, the guidelines `docs/js-api.md`, the sample gallery, `examples/`, and `gallery/samples/curated/`; verify the README contains no per-symbol API catalog or status/milestone claims — "Learn more" links to each; the API section names namespaces, not per-symbol signatures

## 4. Agent/tooling documentation

- [x] 4.1 Add a one-line pointer to `CONTRIBUTING.md` in the `AGENTS.md` Documentation section so the README/CONTRIBUTING split is discoverable; verify the section still points rather than restates and no invariant text changed — added a `README.md`/`CONTRIBUTING.md` bullet to the Documentation section; no other text changed

## 5. Verification

- [x] 5.1 Verify all relative links in `README.md` and `CONTRIBUTING.md` resolve to existing paths in the working tree, and the gallery link matches the URL resolved in 1.1 — a link-extraction check reports all local links OK for both files; README gallery links are `https://mode777.github.io/emotion-fx/` (and `/api/`), matching the resolved Pages `html_url`
- [x] 5.2 Verify no maintainer-only instructions (ctest, golden regeneration, CI dispatch, verification server) remain in `README.md`, and no end-user onboarding content was lost from `CONTRIBUTING.md` — token scan for `ctest`/`--capture`/`verify_remote`/`gh workflow`/`cmake`/`emcmake`/`libx11`/`EFX_HEADLESS` finds none in the README; every section from the 1.2 inventory is preserved (maintainer content moved to `CONTRIBUTING.md`, end-user content rewritten in the README)
- [x] 5.3 Run the documented quick start against a locally built player (e.g. `build/player examples/hello`) and confirm it logs frames and exits 0, proving the README's first-run instructions are accurate; the four-target gate is unaffected (no code, build, test, or workflow changes) and is deliberately not run for this change — run on the verification server (host has no display): `xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 ./build/player examples/hello` printed the frame logs and exited 0
