# AGENTS.md

Guidance for agents working in this repo. The product source of truth is
`vision.md`; read it before proposing anything. Work flows through the
OpenSpec SDD flow — the `opsx-*` / `openspec-*` commands and skills
(propose → apply → archive) — rather than ad-hoc coding.

## Current state

- F6a (resource loading) is **implemented** — a pure-C directory/zip provider
  (`src/resource/`, vendored miniz), synchronous `loadText` / `loadImage` /
  `loadTexture` (PNG/JPEG via the vendored `stb_image`), `player <dir|zip>`
  and `--script <file> [--root <dir|zip>]`, and a web boot that fetches one
  host-provided zip (`__efx_assets` / `?assets=`) and mounts it before
  `main.js` so the script API stays synchronous (ADR 0031). Change
  `f6a-resource-loading`. The four-target gate is **green** (ci run
  36338597814: native suites incl. all goldens on Linux/Windows/macOS,
  Emscripten ctest + web goldens + browser harness + cross-runtime compare).
  F6b (glTF static import) is **implemented** — `loadMeshData(path, opts?)`
  built on vendored cgltf (`vendor/cgltf/`), primitive→surface mapping with
  accessor normalization, the pinned PBR→Phong conversion, per-texture
  samplers on `createTexture`, and the glTF profile pinned in ADR 0032
  (change `f6b-gltf-import`, archived at
  `openspec/changes/archive/2026-09-27-f6b-gltf-import`); its four-target gate
  is **green** (ci run 36344464419: native suites incl. all goldens on
  Linux/Windows/macOS, Emscripten ctest + web goldens + cross-runtime
  compare). F6c (rig import) is **implemented** — `loadMeshData` imports
  `JOINTS_0`/`WEIGHTS_0` into per-surface `joints`/`weights` and bundles the
  skin (joint hierarchy + inverse bind matrices) and every `animations[]`
  clip into the `MeshData` as an opaque rig payload carried onto the `Mesh`
  by `createMesh` (LINEAR/STEP exact, CUBICSPLINE→LINEAR); `createMeshData`
  accepts `joints`/`weights`; no script rig API (change `f6c-rig-import`,
  ADR 0033). Its four-target gate is **green** (ci run 36347575565: native
  suites incl. all goldens on Linux/Windows/macOS, Emscripten ctest + web
  goldens + cross-runtime compare). F6d (interactive console) is
  **implemented** — `player --repl [<root>]` opens the normal window/frame
  loop and evaluates stdin lines in the persistent context (optional root
  runs its `main.js`, `.help`/`.exit` are host commands, errors recover,
  EOF/`.exit` exit 0, `efx.quit(n)` exits n; no new script API; change
  `f6d-repl`, ADR 0007 amended). Its four-target gate is **green** (ci run
  36367478373: native suites incl. all goldens + the REPL ctest cases on
  Linux/Windows/macOS, Emscripten ctest incl. the web-unavailable case +
  web goldens + cross-runtime compare).
- F5 (render targets + post FX) is **done** — the four-target gate is
  green (ci run 36313950553: native suites incl. all forty goldens on
  Linux/Windows/macOS, Emscripten ctest + cross-runtime compare + web
  goldens; the change `f5b-post-fx` is archived at
  `openspec/changes/archive/2026-09-27-f5b-post-fx`, specs synced:
  `openspec/specs/post-fx` plus a delta to `js-api`). F5b delivers the
  declarative post-effect chain `setPostEffects` (≤ 8 entries,
  eager-atomic validation, value-snapshotted; v1 effects `colorFilter`,
  `blur`, `bloom`, each with per-entry `mix`) and `setRenderScale` (scene
  resolution vs surface, nearest/linear blit) — a no-chain fast path that
  stays byte-identical, an engine-owned implicit scene target +
  ping-pong temporaries, and an engine-owned non-script-visible effect
  registry (ADR 0029). F5a (render targets) is **done** — four-target
  gate green (ci run 36309953607: native suites incl. all thirty goldens
  on Linux/Windows/macOS, Emscripten ctest + web goldens); the change is
  archived at `openspec/changes/archive/2026-09-27-f5a-render-targets`
  (specs synced: `openspec/specs/render-targets`, plus deltas to
  `2d-layer`, `3d-core`, `lighting`, `js-api`). F5a delivers
  `createRenderTarget` / `beginRenderTarget` / `endRenderTarget`, a live
  RenderTarget accepted wherever a live Texture is (drawQuad, material
  maps, alphaMask — no alias object, ADR 0028), display-list
  segmentation with value-snapshotted clear-per-begin, the active target
  driving the default 2D frame and 3D aspect, and the Texture lifecycle
  (deferred release, map retention) extended to targets. F4 (lighting +
  Phong, split F4a/F4b)
  is **done**: F4b's four-target gate
  is green (ci run 36284454599: native suites incl. all twenty-six
  goldens on Linux/Windows/macOS, Emscripten ctest + cross-runtime
  compare + web goldens). F4b delivers per-channel Phong maps
  (`ambient`/`diffuse`/`specular`/`emissive` `map`), a material-level
  binary `alphaMask` (discard when sampled alpha < 0.5), `uv` consumption,
  and the retained bound-map texture lifetime — all through the same
  single uniform-driven mesh shader (five always-bound samplers with a
  white-texture fallback, no permutations; ADR 0027). F4a delivers the
  fixed light bank (`setLight` / `setDirectionalLight`), per-surface
  Phong materials (`setMeshSurfaceMaterial` + `createMeshData`'s
  `materials` array; no global material state — ADR 0024), world-space
  lit `drawMesh` (ADR 0026), and the CPU lighting reference. F4a's
  four-target gate is green (ci run 36271736775: native suites incl. all
  nineteen goldens on Linux/Windows/macOS, Emscripten ctest + cross-runtime
  compare + web goldens). F3's four-target gate is also
  green (ci run 36122872839: native suites incl. all
  twelve goldens on Linux/Windows/macOS, Emscripten ctest + web goldens)
  after the D3D11/Metal clip-depth fix recorded in ADR 0025. F3 delivers
  multi-surface meshes
  (Godot-style, ADR 0024), `setCamera3D`, depth-tested `drawMesh`, the GLM
  wrapper (`src/math`, ADR 0005), the shared pure-JS prelude (mat4/vec3/
  quat + makeCube/makePlane/makeSphere), and per-surface material bindings
  as the F4 contract (no global setMaterial). The F3 change is archived at
  `openspec/changes/archive/2026-09-25-f3-3d-core` (specs synced:
  `openspec/specs/3d-core`, `js-api`). The F2 follow-ups are
  archived: `f2a-sokol-shdc` (ADR 0021) and `f2b-web-native-runtime`
  (ADR 0022); the F2 change is archived at
  `openspec/changes/archive/2026-09-22-f2-2d-layer`.
- `src/` is a single core static library (`platform`, `runtime`, `api`,
  `player`, `render`) plus a thin `main.c` (ADR 0003). Sokol and
  quickjs-ng are vendored pinned snapshots under `vendor/`
  (`vendor/README.md`, ADR 0006); stb is vendored for golden-image I/O.
- The `efx` player binary has two run modes (ADR 0007): windowed
  (`player <resource-root>`, runs `main.js`'s `update`/`render` hooks)
  and headless (`player --script <file> [args…]`, exit-code contract),
  plus a capture mode for golden images (`--capture-frame N
  --capture-output file`, ADR 0020).
- `src/` gains two modules in F3: `src/math/` (GLM behind a plain C API,
  ADR 0005 — the only C++ TUs) and `src/prelude/` (the engine-bundled
  pure-JS layer — mat4/vec3/quat, procedural primitives — embedded from
  one source via `tools/gen_prelude.py` and evaluated by both the desktop
  runtime and the web bridge; `src/prelude/prelude.h` is committed and
  the Linux gate job fails on drift via `gen_prelude.py --check`, so
  regenerate after every `prelude.js` edit). Local headless iteration:
  `cmake -B build -DEFX_HEADLESS=ON` builds the unit-test targets only
  (no X11 needed); display-required builds run on the verification
  server.
- The script-facing API: F1's `efx.log`, `efx.quit`, `efx.args`, and
  lifecycle `efx.registerUpdateHook` / `efx.registerRenderHook` (stacking,
  `dt`, unsubscribe; global `update`/`render` remain load-time sugar), plus
  F2's 2D layer — `setCamera2D` (virtual frame), `drawQuad`, `setBlendMode`,
  `setClearColor`, `createImageData`, `createTexture`, `whiteTexture` —
  and F3's 3D core — `setCamera3D`, multi-surface `createMeshData` /
  `createMesh` / `drawMesh`, `efx.mat4`/`efx.vec3`/`efx.quat`,
  `makeCube`/`makePlane`/`makeSphere` — plus F4's lighting and materials —
  `setLight`, `setDirectionalLight`, `setMeshSurfaceMaterial`, and
  `createMeshData`'s `materials` array (F4a), whose channels take optional
  per-channel `map` textures and a material-level `alphaMask` (F4b) —
  cataloged in `docs/js-api.md` (F1/F2/F3/F4 entries are current behavior;
  materials bind per surface — ADR 0024 — there is no global setMaterial).
- `gallery/` is the public sample gallery (Vite + TypeScript + Svelte)
  deployed to GitHub Pages: a left sample list, an iframe-per-run engine
  host, and a Monaco (CDN) editor with the API type document
  (`gallery/src/api/efx.d.ts`). Samples are the committed golden scenes
  plus a curated showcase set; the catalog is generated from
  `tests/goldens/` by `gallery/scripts/gen-catalog.mjs`. A curated
  sample may ship a committed CC0 asset pack (a zip beside its manifest,
  copied into the site by `gen-catalog.mjs` and mounted as the resource
  root); see `gallery/samples/curated/CREDITS.md` for provenance. Build
  with `npm --prefix gallery ci && npm --prefix gallery run build` → `gallery/dist/`
  (copy the Emscripten player in first, `gallery/scripts/prepare-player.mjs`).
  See ADR 0030 for the host↔engine embedding contract.
- Verification: ctest runs smoke + headless display-list/JS-API unit tests
  everywhere (on Emscripten the smoke suite runs the same portable scripts
  through the native bridge with the host JS engine as the runtime, plus
  `tools/run_web_compare.mjs` diffs desktop vs web output); golden-image
  tests (the committed golden scenes under `tests/goldens/`; the public
  sample gallery in `gallery/` generates its catalog from them, so a new
  golden scene appears in the gallery without a manual edit)
  run where a display exists — Linux CI under `xvfb-run` + llvmpipe,
  Emscripten in pinned headless Chrome (ADR 0020). Local builds without a display configure with
  `-DEFX_BUILD_GOLDEN_TESTS=OFF` (the default); if a local build dir was
  configured with `ON`, exclude them (`ctest -E golden`) — goldens fail
  without a display.
- **CI runs on tags and manually, never per push (ADR 0023).**
  `.github/workflows/ci.yml` triggers only on `v*` tags and
  `workflow_dispatch` (`gh workflow run ci.yml`); ordinary branch pushes
  and pull requests do not start it. Every run publishes four downloadable
  archives (native player for Linux/Windows/macOS, Emscripten web bundle)
  as workflow artifacts, and a tag run attaches the same archives to that
  tag's GitHub Release. Use a manual run to prove the gate.
- **Pages deploys separately.** The public sample gallery is built and
  deployed by `.github/workflows/pages.yml` on pushes to `main` and on
  manual dispatch — not by the gate workflow. The workflow builds the
  Emscripten web player, bundles it into the `gallery/` site, and deploys
  the gallery's static output. A manual gate run on any ref therefore
  contains no deployment job and can be green.
- **CI verification order (all future changes): run the Linux pipeline
  first and fix anything it finds; only if Linux passes run the Windows
  pipeline; only if Windows passes run the macOS pipeline.** Linux is the
  fastest, cheapest signal (llvmpipe, matches the canonical goldens);
  Windows and macOS are slower per-roundtrip and verified in that order.
  The full matrix still gates every milestone (ADR 0020) — the order is
  about how changes are iterated, not about which targets count.
- **Verify on the SSH verification server BEFORE dispatching the gate.**
  A Linux server with Xvfb + llvmpipe, pinned emsdk 3.1.64 and pinned
  chrome-headless-shell 131 runs the exact two golden-bearing jobs the
  gate runs on ubuntu-latest (native ctest incl. all golden scenes, and
  the Emscripten golden suite). Flow: commit → push branch →
  `python3 tools/verify_remote.py all <branch>` → only if green dispatch
  `gh workflow run ci.yml --ref <branch>`. If the server verification
  fails, fix and re-verify — do not start a GitHub Actions run yet.
  This is a pre-filter for the Linux signal; the Linux→Windows→macOS
  order and the four-target gate still apply as above. Credentials come
  from the `SSH_HOST` / `SSH_USER` / `SSH_PASSWORD` env vars only — never
  commit them or the server's identity. Details and quirks:
  `docs/verification-server.md`. Known quirks: adding a golden scene
  requires a manual server-side capture before verification (llvmpipe
  only; recipe in that doc), and `ubuntu-latest` moves to Ubuntu 26 on
  2026-10-19, which may bump llvmpipe and require a golden re-baseline
  per ADR 0020.
- **Agents may commit and push to run the gate.** Because CI never fires on
  an ordinary push (ADR 0023), an agent verifying a change MAY create a
  branch, commit, and push for the sole purpose of dispatching
  `gh workflow run ci.yml --ref <branch>` — no separate commit/push request
  is needed for feature verification. Keep commits scoped to the change under
  verification and never sweep in unrelated working-tree changes.
- **After a successful apply, merge to `main` and push.** Once a change's
  verification (the pre-CI server suites and the four-target gate) is green,
  an agent SHALL merge its branch into `main` and push, without waiting for a
  separate merge request. This keeps `main` current and triggers the Pages
  deployment (`pages.yml` runs on pushes to `main`); archive the change
  afterwards and push that too.
- `package.json` exists only to install the OpenSpec CLI. The
  `openspec` binary is not on PATH: run `npm install` once, then invoke
  commands as `npx openspec <command>` from the repo root (e.g.
  `npx openspec status --change <name>`, `npx openspec validate --strict`).
  Known CLI noise: every command prints `Rules for 'design' must be an
  array of strings, ignoring this artifact's rules` even though
  `openspec/config.yaml` is well-formed — a CLI-side parse issue
  (f2c apply notes); honor the design rules by reading the config
  directly instead of chasing the warning.

## Stack

- C11 core (ADR 0001); rendering via **Sokol** — fixed-function consumer
  API, no script-visible shaders ever (internals use Sokol's programmable
  pipeline with engine-owned canned shaders, ADR 0015);
  **quickjs-ng** embedded as the desktop ES6 runtime (ADR 0002); on
  Emscripten the page's native JS engine drives the core through the
  `src/web/` bridge — no quickjs in the wasm (ADR 0022).
- Build system is CMake; targets: Windows, Linux, macOS, Emscripten;
  output is a single binary "player" for a resource folder/zip with a
  `main.js` entry (godot `res://`-style resource root).
- Math: **GLM**, integrated in F3 behind a plain C wrapper (ADR 0005).

## Roadmap

`vision.md` decomposes into a fixed ladder of milestones — the normative
spec is `openspec/specs/feature-roadmap`. The order is fixed: a milestone
must not start before its predecessor's verification gate passes on all
four targets, and every feature proposal must name the milestone it
implements.

| # | Milestone | Scope (one line) | Verification gate | Status |
|---|-----------|------------------|-------------------|--------|
| F1 | Player skeleton | CMake + vendored Sokol/QuickJS, window, resource root, `main.js` hooks, `--script` run mode | Builds on Win/Linux/macOS/Emscripten; script smoke test crosses the JS/C boundary and exits 0 on each | done |
| F2 | 2D layer | `drawQuad`, ortho camera, texture slots, blending modes, display list (record → playback); golden-image harness is a first-class deliverable | Golden-image pixel-diff within tolerance + display-list unit tests, all four targets | done — full four-target CI matrix green; `f2a` (shdc) + `f2b` (web runtime) archived, ADR 0021/0022 |
| F3 | 3D core | Camera, multi-surface mesh resources (ADR 0024), `drawMesh` with depth test, GLM math wrapper, vertex colors, procedural primitives, pure-JS math layer | Golden images + math unit tests | done — four-target gate green (run 36122872839); ADR 0024/0025 |
| F4 | Lighting + Phong (F4a/F4b) | 4 point + 1 directional light, 4-channel Phong on solids/vertex colors (F4a); per-channel maps + alpha masks (F4b); F4 lighting shaders reuse the sokol-shdc pipeline (strategy settled in F2, ADR 0021) | Golden images + lighting unit tests against a CPU reference implementation | done — F4a gate green (run 36271736775, ADR 0026); F4b gate green (run 36284454599, ADR 0027) |
| F5 | Render targets + post FX | F5a: RTT, texture-coerced sampling, segmentation (ADR 0028); F5b: fullscreen passes, declarative effect chain, `mix`, render scale (ADR 0029) | Golden images | done — F5a gate green (run 36309953607, archived 2026-09-27); F5b gate green (run 36313950553, archived 2026-09-27) |
| F6 | Resource packaging | Zip resource root, glTF 2.0 asset import — meshes, images, skins, animation clips (profile decided here), interactive REPL | Script tests load assets from a zip; REPL exercised via piped stdin | in progress — F6a (resource root + text/image loading) done, gate green (run 36338597814); F6b (glTF static import) done, gate green (run 36344464419, ADR 0032); F6c (rig import) done, gate green (run 36347575565, ADR 0033); F6d (interactive console) done, gate green (run 36367478373) |
| F7 | Skinning + animation | CPU skinning into a mesh slot, skeleton/animation import, play/pause/blend | FK joint-transform tests vs CPU reference + golden images | planned |
| F8 | High-level JS + text | `drawModel`, `drawText` (font atlas built on quads), demo resource pack | Golden images; demo pack runs end-to-end on all four targets | planned |

Deferred cross-cutting decisions settle inside specific milestones, not
before: golden-image tolerance + CI determinism (incl. emsdk pinning) in
F2 (done: tolerance/determinism + canned-shader strategy), glTF import profile in
F6. F1's deferred set (toolchain, quickjs flavor, math library) is
settled — see `docs/decisions/`.

## Non-negotiable design constraints (easy to get wrong)

- **Fixed-function pipeline only** — no shader-shaped features on the
  consumer API, ever; the internal renderer uses Sokol's programmable
  pipeline with engine-owned canned shaders (ADR 0015).
- Fixed limits: 4 point lights + 1 directional light, 1 camera.
- Immediate-mode *API*, but rendering goes through a re-orderable display list
  — do not map API calls 1:1 to draw calls.
- **Sokol does not normalize the clip depth range or attachment formats**
  (ADR 0025). Camera math is GL-convention; `src/platform/pipeline.c`
  folds `row2 = 0.5·row2 + 0.5·row3` into the MVP on `origin_top_left`
  backends (D3D11/Metal) — all four columns, never in shaders. Engine-
  created attachments must declare the env-default pixel formats. "Only
  GL renders correctly" plus half-missing meshes means check this first.
- JS API layering: low/mid-level in C/C++ (`drawQuad`, `drawMesh`,
  `setMaterial`…), high-level conveniences in pure JS (`drawModel`,
  `drawText`…).
- JS code must have **zero browser/Node dependencies, not even transitively**.
- Memory rules: manage resources in JS where possible; unavoidable unmanaged
  resources are exposed as GC-finalized opaque classes with explicit
  `destroy()` (textures, meshes, … — ADR 0011, discipline ADR 0012) or as
  fixed pre-allocated banks (lights), to avoid leaks in a GC'd language.
- Script-facing API changes require a `js-api` spec delta and a matching
  `docs/js-api.md` update in the same change (see `docs/js-api.md`), plus a
  matching update to the type document `gallery/src/api/efx.d.ts`.

## Documentation

- `vision.md` — product goals; the source of truth for intent.
- `docs/js-api.md` — the script-facing API catalog; updated in the same
  change as any API delta.
- `gallery/src/api/efx.d.ts` — the living TypeScript declaration of the
  public `efx` API, loaded into the gallery editor; it grows with the API
  and is updated in the same change as any API delta (like
  `docs/js-api.md`).
- `docs/decisions/` — architecture decision records (ADRs): the durable
  *why* behind cross-cutting invariants (language, runtime, module
  walls, binding pattern, vendoring, run modes, CI).
- `openspec/specs/` — required behavior; `openspec/changes/` — full
  design/process records per change.

Dividing rule: `openspec/specs/` pin required behavior, `docs/` hold
invariants and rationale, this file points rather than restates. When
work settles a durable architecture decision — a trade-off future
changes must respect — document it as a short ADR in `docs/decisions/`
(new numbered file + a row in its index). Full design/process records
stay in `openspec/changes/`; the ADR extracts only what outlives the
change.

## Reference implementations

Use these when designing, don't reinvent: sokol-samples (rendering patterns),
rayjs (QuickJS integration + stripping QuickJS for cross-platform).

## Not yet decided

Golden-image tolerance and CI determinism are settled (F2, ADR 0020),
as is the canned-shader strategy (settled early in F2 via
`f2a-sokol-shdc`, ADR 0021 — canned shaders are single-source GLSL in
`shaders/*.glsl`, compiled with pinned sokol-shdc). The glTF import
profile (F6) remains open — settle it via an OpenSpec proposal, not by
silently picking defaults. The Roadmap section assigns each deferred
decision a latest-settling milestone. The glTF 2.0 import format itself is pinned
in the roadmap; only the profile remains open.
