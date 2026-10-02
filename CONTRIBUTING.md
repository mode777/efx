# Contributing to EmotionFX

This document is for people building, testing, and changing EmotionFX itself.
If you just want to build a game with the engine, start with
[`README.md`](README.md) instead.

- [Getting the source](#getting-the-source)
- [Building from source](#building-from-source)
- [Running a source build](#running-a-source-build)
- [Testing](#testing)
- [Verifying before CI](#verifying-before-ci)
- [Continuous integration](#continuous-integration)
- [Repository layout](#repository-layout)
- [Documentation and process](#documentation-and-process)
- [Vendored dependencies](#vendored-dependencies)
- [Notes](#notes)

## Getting the source

```sh
git clone https://github.com/mode777/emotion-fx.git
cd emotion-fx
```

## Building from source

Requirements: CMake ≥ 3.21 and a C11 toolchain.

| Target | Commands |
|---|---|
| Linux | `sudo apt install libx11-dev libxi-dev libxcursor-dev libgl1-mesa-dev libasound2-dev` then `cmake -B build && cmake --build build` |
| Windows (VS 2022) | `cmake -B build && cmake --build build --config Release` |
| macOS (Xcode toolchain) | `cmake -B build && cmake --build build` |
| Emscripten | `emcmake cmake -B build-em && cmake --build build-em` |

The output is a single `player` binary (or `player.exe` on Windows). The
Emscripten target produces the `player_web` bundle (`player_web.html` +
`.js` + `.wasm` + `.data`).

For local iteration without a display, the headless build configures the
unit-test targets only and needs no X11:

```sh
cmake -B build -DEFX_HEADLESS=ON
cmake --build build
```

`EFX_BUILD_GOLDEN_TESTS` defaults to `OFF`; golden-image tests need a display
(ADR 0020). If a build directory was configured with it `ON` and you have no
display, exclude them with `ctest -E golden`.

## Running a source build

```sh
build/player examples/hello          # window + frame loop
build/player --script tests/scripts/s_quit3.js   # headless; exits 3
build/player --repl examples/hello   # interactive console against a resource root
```

The run modes are documented in `AGENTS.md` and `openspec/specs/player-runtime`
(ADR 0007): windowed (`player <resource-root>`), headless
`--script <file> [args…]`, and interactive `--repl [<root>]`, plus golden
capture (`--capture-frame N --capture-output file`, ADR 0020).

Window behavior (a window opens, hooks run per frame, clean exit on close) is
verified manually per desktop platform — CI runners have no real display. The
checklist: launch `build/player examples/hello`, confirm a window opens with
frame logs on stdout and a clean exit 0 on close after the 60-frame auto-quit.

## Testing

The suite is ctest-based: headless smoke tests (`--script` mode), headless
display-list + JS-API unit tests (mock GPU sink, no window needed), and —
where a GPU/display exists — golden-image capture tests:

```sh
cmake -B build -DEFX_BUILD_GOLDEN_TESTS=ON
cmake --build build
ctest --test-dir build -C Release --output-on-failure
```

Linux CI renders under `xvfb-run` with `LIBGL_ALWAYS_SOFTWARE=1`. The
Emscripten job runs its goldens through pinned headless Chrome with
SwiftShader (`tools/run_web_goldens.mjs`).

**Regenerating goldens** — only when intended output changed:

```sh
cmake -B build -DEFX_BUILD_GOLDEN_TESTS=ON && cmake --build build
./build/player --capture-frame 2 --capture-output tests/goldens/<scene>/golden.png tests/goldens/<scene>
```

Review the regenerated `golden.png` carefully before committing: goldens are
the reference, so a diff here is a deliberate rendering change. CI fails if the
toolchain or a code change alters output without a committed regen.

## Verifying before CI

A Linux verification server (Xvfb + llvmpipe, pinned emsdk and
chrome-headless-shell) runs the two golden-bearing jobs the gate runs on
`ubuntu-latest`: the native ctest suite including all golden scenes, and the
Emscripten golden suite. Verify there **before** dispatching the gate:

```sh
# commit, push the branch, then:
python3 tools/verify_remote.py all <branch>
# only if green:
gh workflow run ci.yml --ref <branch>
```

Credentials come from the `SSH_HOST` / `SSH_USER` / `SSH_PASSWORD` environment
variables only — never commit them or the server's identity. Details and known
quirks (including capturing a new golden scene server-side): see
[`docs/verification-server.md`](docs/verification-server.md).

## Continuous integration

CI is the four-target gate (Linux, Windows, macOS, Emscripten). It is **not**
run on every push: the workflow triggers on version tags (`v*`) and on manual
dispatch only (ADR 0023). The smoke suite, golden-image checks, and web
comparison harness all run inside those triggered runs.

```sh
gh workflow run ci.yml            # start the full gate on the current ref
gh run list --workflow ci.yml     # list runs and their status
```

Iteration order for changes: run the Linux pipeline first and fix anything it
finds; only if Linux passes run Windows; only if Windows passes run macOS.
The full matrix still gates every milestone (ADR 0020) — the order is about
how changes are iterated, not about which targets count.

Every completed run publishes downloadable archives under the run's
**Artifacts**: the native player for Linux, Windows, and macOS, the Emscripten
web bundle, and the curated-samples pack
(`emotion-fx-<version>-samples.zip`). A run triggered by a `v*` tag
additionally creates a GitHub Release for that tag with the same archives
attached. Manual runs use a `dev-<sha>` version token in the archive names;
tag runs use the tag name.

The public sample gallery deploys separately, via
`.github/workflows/pages.yml`, on pushes to `main` and on manual dispatch —
not by the gate workflow. A manual gate run on any ref therefore contains no
deployment job and can be green.

## Repository layout

```
src/            C11 core: one static library (platform, runtime, api, player,
                render, physics, input, audio, resource, math, web, prelude)
                plus a thin main.c (ADR 0003)
vendor/         vendored pinned dependencies (sokol, quickjs-ng, stb, miniz,
                cgltf, dr_libs, minigamepad)
shaders/        engine-owned canned GLSL, compiled with pinned sokol-shdc (ADR 0021)
tests/          ctest smoke/unit suites + committed PNG goldens under tests/goldens/
examples/       sample resource roots (hello, hooks, browser)
gallery/        public sample-gallery site + the API type document
tools/          build/verification/generation scripts
docs/           guidelines, ADRs, verification notes, generated API reference
openspec/       OpenSpec specs and change artifacts
```

`src/prelude/prelude.js` (the engine-bundled pure-JS layer) is embedded via
`tools/gen_prelude.py`; the committed `src/prelude/prelude.h` must be
regenerated after every `prelude.js` edit, and the Linux gate job fails on
drift (`gen_prelude.py --check`).

## Documentation and process

- [`AGENTS.md`](AGENTS.md) — working conventions, the current state and
  codebase map, and the operational rules (CI, verification, releases).
- [`vision.md`](vision.md) — product goals; the source of truth for intent.
- [`openspec/`](openspec/) — required behavior (`openspec/specs/`) and the
  OpenSpec SDD flow for changes (`openspec/changes/`; propose → apply →
  archive).
- [`docs/decisions/`](docs/decisions/) — architecture decision records (ADRs):
  the durable *why* behind cross-cutting invariants.
- [`docs/js-api.md`](docs/js-api.md) — script-facing API design guidelines.
- [`docs/api/`](docs/api/) — the per-symbol API reference. **Auto-generated;
  do not edit by hand.** It is rendered from
  [`gallery/src/api/efx.d.ts`](gallery/src/api/efx.d.ts) by
  `npm --prefix gallery run docs:markdown`; `docs:check` fails on drift.

Work flows through the OpenSpec SDD flow rather than ad-hoc coding. When a
change settles a durable architecture decision, add a numbered ADR under
`docs/decisions/` and index it in `docs/decisions/README.md`.

## Vendored dependencies

Dependencies are vendored and pinned under `vendor/`; see
[`vendor/README.md`](vendor/README.md). Builds are fully offline.

## Notes

- sokol's Linux backend needs X11/GL dev packages at build time; no display is
  needed for headless runs and tests.
- Audio builds on Linux additionally need `libasound2-dev`.
- The Emscripten toolchain is pinned to an exact emsdk version (3.1.64) for
  golden-image determinism (ADR 0020).
