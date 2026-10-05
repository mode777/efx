# AGENTS.md

Guidance for agents working in this repo. **EFX** is a fixed-function C11
renderer (Sokol; ADR 0001/0015) with an embedded ES6 scripting layer
(quickjs-ng on desktop, ADR 0002; the page's JS engine on the web via the
`src/web/` bridge, ADR 0022), GLM math behind a plain C wrapper (ADR 0005).
CMake builds Windows/Linux/macOS/Emscripten into a single `efx` player that
runs a resource root (folder/zip) with a `main.js` entry (ADR 0007). Product
intent: `vision.md` — read it before proposing anything.

Work flows through the OpenSpec SDD flow — the `opsx-*` / `openspec-*`
commands and skills (propose → apply → archive) — rather than ad-hoc
coding. Every proposal names the capability spec(s) under `openspec/specs/`
it modifies or creates, or sets `skip_specs: true` for docs/tooling changes.

> Status lives in the systems, not in this file: per-change records in
> `openspec/changes/archive/`, durable decisions in `docs/decisions/README.md`
> (ADR index; its Status column marks supersessions), and the completed
> milestone ladder in `openspec/specs/feature-roadmap` — all of F1–F14 are
> implemented and gated on all four targets.

## Local scratch

Put throwaway artifacts (build logs, diffs, capture staging, one-off scripts)
in the repo-local, gitignored **`.tmp/`** directory. It lives on the persistent
workspace volume, so it survives restarts; `/tmp` is ephemeral and
`/tmp/opencode` is not writable in the agent container. Only `.tmp/.gitignore`
is tracked (it ignores the rest), so a fresh clone has the directory.

## Change lifecycle

1. **Propose** — create an OpenSpec change (`npx openspec new change`, then
   proposal → specs/design → tasks per its schema) on a branch.
2. **Apply** — implement the tasks; keep commits scoped to the change and
   never sweep in unrelated working-tree changes.
3. **Verify on the SSH server first** — commit → push branch →
   `python3 tools/verify_remote.py all <branch>`. It runs the exact two
   golden-bearing jobs the gate runs on ubuntu-latest. Fix and re-verify
   until green — do not dispatch the gate before that. Credentials come
   only from `SSH_HOST` / `SSH_USER` / `SSH_PASSWORD`; never commit them
   or the server's identity. Quirks (golden-scene capture recipe, pinned
   emsdk/chrome): `docs/verification-server.md`.
4. **Dispatch the gate** — `gh workflow run ci.yml --ref <branch>` and
   iterate in order: Linux → fix anything → Windows → macOS. Linux is the
   fastest, cheapest signal (llvmpipe matches the canonical goldens); the
   full four-target matrix still gates every change (ADR 0020). CI fires on
   `v*` tags and manual dispatch only, never per push (ADR 0023) — agents
   may therefore commit and push a branch solely to dispatch the gate.
   Every run publishes archives; tag runs attach them to the release.
5. **Merge on green** — merge the branch into `main` and push without
   waiting for a separate request, using `git merge --no-ff` with a
   `Merge <change>: <summary> (ADR NNNN)` message (the repo's history shape).
   This triggers the Pages deployment: the
   sample gallery deploys via `pages.yml` on pushes to `main`, separately
   from the gate.
6. **Archive** — archive the change and push. An ADR the change promised is
   written and indexed in `docs/decisions/` before archiving.

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
  `setLight`, `drawText`…), high-level conveniences in pure JS
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
  committed reference `docs/api/` from it; `docs/js-api.md` holds the API
  design guidelines and is updated when a design rule changes.

## Repo map

- `src/` — one core static library (`platform`, `runtime`, `api`, `player`,
  `render`, `physics`, `input`, `audio`, `resource`, `math`, `web`,
  `prelude`) plus a thin `main.c` (ADR 0003).
- `src/web/js/*.js` are `--post-js` fragments concatenated in `CMakeLists.txt`
  order into one shared scope, not modules: `core.js` holds the helpers and
  the native-backed classes (`__efxResourceClass`, whose `methods`/`getters`
  define their operations), the other files are object-literal fragments.
- `vendor/` — pinned source snapshots (Sokol, quickjs-ng, stb, miniz, cgltf,
  dr_libs, minigamepad — `vendor/README.md`, ADR 0006).
- Run modes (ADR 0007): windowed resource root (`main.js` `update`/`render`
  hooks), headless `--script` (exit-code contract), `--repl` console,
  golden capture (`--capture-frame`, ADR 0020).
- `src/prelude/prelude.js` is embedded via `tools/gen_prelude.py`;
  `src/prelude/prelude.h` is committed and the Linux gate job fails on drift
  (`gen_prelude.py --check`) — regenerate after every prelude edit.
- Local headless iteration: `cmake -B build -DEFX_HEADLESS=ON` (unit tests
  only, no X11). Golden tests need a display (ADR 0020): local builds keep
  `-DEFX_BUILD_GOLDEN_TESTS=OFF`; on an ON build dir exclude with
  `ctest -E golden`. Emscripten goldens run in pinned headless Chrome;
  `tools/run_web_compare.mjs` diffs desktop vs web output.
- Gallery: `npm --prefix gallery ci && npm --prefix gallery run build`; the
  type test is checked by `npm --prefix gallery run check` (svelte-check over
  `src/**/*.ts`, which includes `gallery/src/api/efx.type-test.ts`), not
  `tsc`. Copy
  the Emscripten player in via `gallery/scripts/prepare-player.mjs`; the
  catalog is generated from `tests/goldens/` + `gallery/samples/curated/` by
  `gallery/scripts/gen-catalog.mjs` (embedding contract ADR 0030, sample
  layout ADR 0044).
- `docs/api/` is **auto-generated — never edit its files by hand**: rendered
  from `gallery/src/api/efx.d.ts` by `npm --prefix gallery run docs:markdown`
  (`docs:check` fails on drift). `docs/js-api.md` holds the API design
  guidelines; `CONTRIBUTING.md` is the human build/test/CI guide.
- `npx openspec <command>` (binary not on PATH; `npm install` once). Known
  CLI noise: every command prints `Rules for 'design' must be an array of
  strings` — a CLI-side parse issue; honor design rules by reading
  `openspec/config.yaml` directly. Strict validation is
  `npx openspec validate "<change>" --type change --strict` (there is no
  `--change` flag on `validate`; `status`/`instructions` do take `--change`).
  A `## MODIFIED Requirements` block must reproduce every existing scenario
  name verbatim (and keep the requirement header text) or strict validation
  rejects it as dropping scenarios — rename via `RENAMED`, never in place.
- Reference implementations: sokol-samples (rendering patterns), rayjs
  (QuickJS integration + stripping for cross-platform).

## Open risks (dated — prune when stale)

- **2026-10-19** — `ubuntu-latest` moves to Ubuntu 26, which may bump
  llvmpipe and require a golden-image re-baseline per ADR 0020.

## Maintenance rule

**AGENTS.md records invariants and pointers, never per-change history.**
If a fact can be derived from the archive, the ADR index, or the roadmap
spec, it does not belong in this file. When a change lands, update those
systems — not this file.
