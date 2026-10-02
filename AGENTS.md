# AGENTS.md

Guidance for agents working in this repo. The product source of truth is
`vision.md`; read it before proposing anything. Work flows through the
OpenSpec SDD flow — the `opsx-*` / `openspec-*` commands and skills
(propose → apply → archive) — rather than ad-hoc coding.

## Current state

**Summary.** Every roadmap milestone F1–F14 is implemented and passed its
verification gate on all four targets (Windows, Linux, macOS, Emscripten).
Status per milestone: the Roadmap table below and the normative spec
`openspec/specs/feature-roadmap`. Durable decisions: `docs/decisions/README.md`.
Per-change design/process records and gate run ids:
`openspec/changes/archive/<date>-<change>/`. The script-facing API:
the generated reference `docs/api/` (source `gallery/src/api/efx.d.ts`).

**Milestones** — one line each; the archived change folder (under
`openspec/changes/archive/`) followed by its ADR(s):

- F1 player skeleton — `2026-09-19-f1-player-skeleton`
- F2 2D layer — `2026-09-22-f2-2d-layer`; follow-ups
  `2026-09-22-f2a-sokol-shdc` (ADR 0021) and
  `2026-09-22-f2b-web-native-runtime` (ADR 0022)
- F3 3D core — `2026-09-25-f3-3d-core` (ADR 0024, ADR 0025)
- F4a/F4b lighting + maps — `2026-09-26-f4a-lighting-phong` (ADR 0026),
  `2026-09-27-f4b-maps-alpha-masks` (ADR 0027)
- F5a/F5b render targets + post FX — `2026-09-27-f5a-render-targets`
  (ADR 0028), `2026-09-27-f5b-post-fx` (ADR 0029)
- F6a–F6e resource packaging — `2026-09-27-f6a-resource-loading` (ADR 0031),
  `2026-09-27-f6b-gltf-import` (ADR 0032), `2026-09-27-f6c-rig-import`
  (ADR 0033), `2026-09-28-f6d-repl` (ADR 0007 amended),
  `2026-09-28-f6e-texture-creation-options` (ADR 0034)
- F7 skinning + animation — `2026-09-28-f7-skinning-animation` (ADR 0035)
- F8a font + text — `2026-09-28-f8a-font-typesetting` (ADR 0038; the former
  F8b slice is retired, superseded by ADR 0024)
- F9 input — `2026-09-28-f9-input` (ADR 0036); iframe-focus follow-up
  `openspec/changes/archive/2026-10-02-web-keyboard-focus` (ADR 0043)
- F10 CommonJS modules — `2026-09-28-f10-commonjs-modules` (ADR 0037)
- F11 particles + billboards — `2026-09-29-f11-particles-billboards`
  (ADR 0039)
- F12 physics — `2026-09-29-f12-collision-physics` (ADR 0040); hardening
  `2026-09-30-physics-tunneling` (ADR 0045); live-body ownership ADR 0046
- F13 gamepad — `2026-09-29-gamepad-input` (ADR 0041)
- F14 audio — `2026-09-30-f14-audio` (ADR 0042); source-model revision
  `2026-09-30-audio-source-model` (ADR 0047)
- Gallery — `2026-09-27-web-gallery` (ADR 0030); curated sample directories
  `openspec/changes/archive/2026-10-02-curated-sample-dirs` (ADR 0044)
- Post-roadmap API reorganization — the 33 graphics drawing/state/resource
  functions live under `efx.graphics`, not the `efx` root
  (`efx-graphics-namespace`, ADR 0050; hard cut, no root aliases, error
  text and golden pixels byte-identical)
- Namespace consolidation — `mat4`/`vec3`/`quat` live under `efx.math`,
  loaders under `efx.io` (with `loadData` returning a `Uint8Array` copy),
  `whiteTexture` under `efx.graphics`, frozen color constants under
  `efx.color`, and `args` is a read-only property
  (`efx-namespace-consolidation`, ADR 0051; hard cut, no root aliases)
- Initialization ordering — the entry script is evaluated after the rendering
  surface exists in surface-bearing run modes (`entry-after-gpu-init`, ADR 0016
  amended); surface-less modes (`--script`, web Node) create CPU-only resources
  through the one creation path, the deferred pre-GPU upload queue is gone, and
  `efx.graphics.whiteTexture` works in every run mode
  (`collapse-pre-gpu-queue`, ADR 0052)
- Rebrand — the project ships as **EFX** (was EmotionFX, which collided with
  an existing game middleware); the repo slug is `efx`, artifact names are
  `efx-<version>-…`, and the CMake project is `efx`, with no script-API or
  behavior change (`rebrand-to-efx`; no ADR)

**Codebase map:**

- `src/` is a single core static library (`platform`, `runtime`, `api`,
  `player`, `render`, `physics`, `input`, `audio`, `resource`, `math`,
  `web`, `prelude`) plus a thin `main.c` (ADR 0003).
- Vendored pinned snapshots under `vendor/` (`vendor/README.md`,
  ADR 0006): Sokol, quickjs-ng, stb, miniz, cgltf, dr_libs, minigamepad.
- The `efx` player run modes (ADR 0007): windowed
  (`player <resource-root>`, runs `main.js`'s `update`/`render` hooks),
  headless (`--script <file> [args…]`, exit-code contract), interactive
  console (`--repl [<root>]`), and golden capture
  (`--capture-frame N --capture-output file`, ADR 0020).
- `src/prelude/prelude.js` (the engine-bundled pure-JS layer) is embedded
  via `tools/gen_prelude.py`; `src/prelude/prelude.h` is committed and the
  Linux gate job fails on drift (`gen_prelude.py --check`) — regenerate
  after every `prelude.js` edit.
- Local headless iteration: `cmake -B build -DEFX_HEADLESS=ON` builds the
  unit-test targets only (no X11 needed); display-required builds run on
  the verification server and CI.
- Golden-image tests need a display (ADR 0020): local builds keep the
  default `-DEFX_BUILD_GOLDEN_TESTS=OFF`; if a build dir was configured
  `ON`, exclude them (`ctest -E golden`). Emscripten goldens run in pinned
  headless Chrome; `tools/run_web_compare.mjs` diffs desktop vs web output.
- Gallery: `npm --prefix gallery ci && npm --prefix gallery run build` →
  `gallery/dist/` (copy the Emscripten player in first via
  `gallery/scripts/prepare-player.mjs`). The catalog is generated from
  `tests/goldens/` + `gallery/samples/curated/` by
  `gallery/scripts/gen-catalog.mjs`; a curated sample is a
  self-contained resource-root directory (ADR 0044); the host↔engine
  embedding contract is ADR 0030.

**Operational rules:**

- **CI runs on tags and manually, never per push (ADR 0023).**
  `.github/workflows/ci.yml` triggers only on `v*` tags and
  `workflow_dispatch` (`gh workflow run ci.yml`); ordinary branch pushes
  and pull requests do not start it. Every run publishes downloadable
  archives (native player for Linux/Windows/macOS, Emscripten web bundle,
  and the curated-samples pack `efx-<version>-samples.zip`) as
  workflow artifacts, and a tag run attaches the same archives to that
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
implements. F9 (input), F10 (script modules — CommonJS), F11 (particles +
billboards), F12 (collision + character + impulse dynamics), F13 (gamepad
input), and F14 (audio playback) are the **orthogonal** milestones: F9's only
predecessor is F2, F10's are F1–F2 plus the F6a dir/zip resource provider,
F11's are F2 + F3 (+ F6a for file textures), F12's are F3 + F6a/F6b, F13's
only predecessor is F9, and F14's are F1–F2 plus F6a, so they may land
independently of the remaining F3–F8 milestones.

| # | Milestone | Scope (one line) | Verification gate | Status |
|---|-----------|------------------|-------------------|--------|
| F1 | Player skeleton | CMake + vendored Sokol/QuickJS, window, resource root, `main.js` hooks, `--script` run mode | Builds on Win/Linux/macOS/Emscripten; script smoke test crosses the JS/C boundary and exits 0 on each | done |
| F2 | 2D layer | `drawQuad`, ortho camera, texture slots, blending modes, display list (record → playback); golden-image harness is a first-class deliverable | Golden-image pixel-diff within tolerance + display-list unit tests, all four targets | done — full four-target CI matrix green; `f2a` (shdc) + `f2b` (web runtime) archived, ADR 0021/0022 |
| F3 | 3D core | Camera, multi-surface mesh resources (ADR 0024), `drawMesh` with depth test, GLM math wrapper, vertex colors, procedural primitives, pure-JS math layer | Golden images + math unit tests | done — four-target gate green (run 36122872839); ADR 0024/0025 |
| F4 | Lighting + Phong (F4a/F4b) | 4 point + 1 directional light, 4-channel Phong on solids/vertex colors (F4a); per-channel maps + alpha masks (F4b); F4 lighting shaders reuse the sokol-shdc pipeline (strategy settled in F2, ADR 0021) | Golden images + lighting unit tests against a CPU reference implementation | done — F4a gate green (run 36271736775, ADR 0026); F4b gate green (run 36284454599, ADR 0027) |
| F5 | Render targets + post FX | F5a: RTT, texture-coerced sampling, segmentation (ADR 0028); F5b: fullscreen passes, declarative effect chain, `mix`, render scale (ADR 0029) | Golden images | done — F5a gate green (run 36309953607, archived 2026-09-27); F5b gate green (run 36313950553, archived 2026-09-27) |
| F6 | Resource packaging | Zip resource root, glTF 2.0 asset import — meshes, images, skins, animation clips (profile decided here), interactive REPL | Script tests load assets from a zip; REPL exercised via piped stdin | done — F6a (resource root + text/image loading) gate green (run 36338597814); F6b (glTF static import) gate green (run 36344464419, ADR 0032); F6c (rig import) gate green (run 36347575565, ADR 0033); F6d (interactive console) gate green (run 36367478373); F6e (texture creation options) gate green (run 36392688547) |
| F7 | Skinning + animation | CPU skinning into a mesh slot, skeleton/animation import, script-driven posing | FK joint-transform tests vs CPU reference + golden images | done — `poseMesh` + `skinned` draw option, CPU-reference unit tests, `skin_pose` golden, CC0 Fox gallery sample (ADR 0035); four-target gate green (run 36443794987) |
| F8 | High-level JS + text | `loadFontData`/`createFont`/`drawText`/`measureText` (fixed C-baked atlas, wrap + alignment, baked outline/shadow) | Text golden images + headless layout/measure unit tests | done — F8a implemented (change `f8a-font-typesetting`, ADR 0038); F8b (`drawModel` + demo resource pack) retired as obsolete — superseded by F3's multi-surface meshes (ADR 0024) |
| F9 | Input (keyboard + mouse) | **Orthogonal** (predecessor F2; independent of F3–F8): pure-C frame-staged input core, `efx.keyboard`/`efx.mouse`/`efx.window` query + event API, surface-pixel coordinates, test-only injection seam | Non-visual: headless unit tests over the C core + a script-level simulation harness, all four targets (no golden image) | implemented — change `f9-input`, ADR 0036; four-target gate green (run 36448429521) |
| F10 | Script modules (CommonJS) | **Orthogonal** (predecessors F1–F2 + F6a; independent of F3–F9): synchronous provider-backed `require` in the shared pure-JS prelude, restricted resolver, module caching/cycles, `__esModule` interop, JSON modules, `main.js` as a module, TypeScript `import`→CommonJS authoring | Script-level module tests on all four targets (ctest + Emscripten ctest + cross-runtime compare; no golden image) | implemented — change `f10-commonjs-modules`, ADR 0037; four-target gate green (run 36463101573) |
| F11 | Particles + billboards | **Orthogonal** (predecessors F2 + F3, F6a for file textures; independent of F4/F5/F7/F8): CPU-simulated engine-owned particle systems (`createParticleSystem`/`emit`/`drawParticles`, 3D world or 2D screen, `facing` `view`/`y`/`plane`), world-space `drawBillboard`, batched 2D `drawSprites`, a depth-test/no-write billboard pipeline, and curated gallery showcases | Golden image + headless simulation/billboard unit tests + portable script case + curated showcases, all four targets | implemented — change `f11-particles-billboards`, ADR 0039; four-target gate green (run 36563480277) |
| F12 | Collision + character + impulse dynamics | **Orthogonal** (predecessors F3 + F6a/F6b; independent of F4/F5/F7/F8): a bespoke dependency-free C11 core (`src/physics/`) — sphere/box/capsule/triangle-mesh colliders, one script-stepped world, linear-only sequential-impulse dynamics, sensors, `Body`/`Character` native-backed classes, the `moveAndSlide` capsule controller, and the `raycast`/`overlap`/`shapeCast` queries — plus a curated gallery showcase | Headless `efx_physics_tests` (narrowphase, invariants, scenarios, determinism, stress) + portable script case through both runtimes + cross-runtime compare (no golden image) | implemented — change `f12-collision-physics`, ADR 0040; `step(dt)` sub-stepping hardening in `physics-tunneling`, ADR 0045 |
| F13 | Gamepad input | **Orthogonal** (predecessor F9; independent of F3–F8 and F10–F12): a vendored pinned minigamepad poll backend confined to the platform layer, a pure-C fixed pad bank (`src/input/gamepad.c`) with frame-begin polling and F9-style one-frame edges, a portable SDL-mapping evaluator (GUID selection, half-axis/inversion/hat handling), canonical ranges + trigger threshold, a raw fallback for unmapped pads, and the `efx.gamepad` namespace (no new resource type) | Headless `efx_input_tests` gamepad cases + `efx_api_tests gamepad_js` over the pure-C model/evaluator with synthetic descriptors, a portable script case through both runtimes, and a cross-runtime compare (no golden image) | implemented — change `gamepad-input`, ADR 0041; four-target gate green (run 36597602782) |
| F14 | Audio playback | **Orthogonal** (predecessors F1–F2 + F6a; independent of F3–F13): a vendored `sokol_audio` (push mode) + `dr_libs` decoder stack confined to the platform layer, a dependency-free pure-C source/voice-bank core in `src/audio/` (static `AudioData` + streamed `AudioStream`, a fixed 32-voice playback bank, a 4-stream cap, WAV/MP3, linear-interpolation resampling/pitch, decoded-PCM only), and the `efx.audio` namespace (`loadAudioData`/`loadAudioStream`/`playAudio` with `volume`/`resume`, native-backed `AudioData`/`AudioStream`/`Audio`; no-device soft-fail, web autoplay unlock); the music/effect split was replaced by the two source kinds in `audio-source-model`, ADR 0047 | Headless `efx_audio_tests` over the pure-C core (embedded WAV/MP3 fixtures) + `efx_api_tests audio_js`, a portable script case through both runtimes, and a cross-runtime compare (no golden image) | implemented — change `f14-audio`, ADR 0042, revised by `audio-source-model`, ADR 0047 |

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
  `setMeshSurfaceMaterial`, `drawText`…), high-level conveniences in pure JS
  (`makeCube`/`makePlane`/`makeSphere`…). Cold-path option-bag validation is
  written once in the shared prelude behind a private `natives` object —
  natives stay marshal-only, hot draw/query paths keep native validation
  (ADR 0049).
- JS code must have **zero browser/Node dependencies, not even transitively**.
- Memory rules: manage resources in JS where possible; unavoidable unmanaged
  resources are exposed as GC-finalized opaque classes with explicit
  `destroy()` (textures, meshes, … — ADR 0011, discipline ADR 0012) or as
  fixed pre-allocated banks (lights), to avoid leaks in a GC'd language.
- Script-facing API changes require a `js-api` spec delta, a matching update
  to the type document `gallery/src/api/efx.d.ts`, and regenerating the
  committed reference `docs/api/` from it (`npm --prefix gallery run
  docs:markdown`); `docs/js-api.md` holds the API design guidelines and is
  updated when a design rule changes.

## Documentation

> **`docs/api/` is auto-generated — never edit its files by hand.** It is
> rendered from `gallery/src/api/efx.d.ts` by
> `npm --prefix gallery run docs:markdown`. To change the reference, edit the
> declaration and regenerate; `docs:check` fails the build on drift.

- `README.md` — end-user onboarding (what the engine is, how to get it, first
  script); `CONTRIBUTING.md` — the from-source build, test/golden harness,
  verification server, and CI gate for human contributors.
- `vision.md` — product goals; the source of truth for intent.
- `docs/js-api.md` — the script-facing API **design guidelines** (conventions,
  layering, resource model, limits, and the process for adding API); the
  per-symbol reference is generated, not hand-written here.
- `docs/api/` — the committed Markdown rendering of the per-symbol API
  reference. **Auto-generated; do not edit by hand.** Generated from
  `gallery/src/api/efx.d.ts` by `npm --prefix gallery run docs:markdown`
  (`docs:check` fails on drift). The same source is built to HTML and
  published at `/api` on the gallery site.
- `gallery/src/api/efx.d.ts` — the living TypeScript declaration of the
  public `efx` API, loaded into the gallery editor and the single source of
  truth for the generated reference `docs/api/`; it grows with the API and is
  updated in the same change as any API delta, then `docs/api/` is
  regenerated from it.
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
`shaders/*.glsl`, compiled with pinned sokol-shdc), and the glTF import
profile (settled in F6b, ADR 0032). The Roadmap section assigns each
deferred decision a latest-settling milestone.
