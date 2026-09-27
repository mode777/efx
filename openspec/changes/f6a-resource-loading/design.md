# Design

## Context

See `proposal.md` for motivation. Current state that shapes the approach:

- **Module walls (ADR 0003):** `src/render` is pure C with a GPU sink; the
  quickjs binding lives in `src/api` (`api.c`), and the web binding in
  `src/web/bridge.c` (`EM_JS`/`EMSCRIPTEN_KEEPALIVE` wrappers). Any new
  loader must sit below both bindings and above `render`, with no quickjs or
  sokol includes.
- **Boot ordering (ADR 0016/0022/0030):** the browser evaluates `main.js` from
  `postRun` synchronously, reading it from MEMFS; the node player reads it via
  `NODERAWFS`. The host `__efx_main_js` channel is consumed and deleted before
  evaluation. The web golden/harness drivers serve assets over a local HTTP
  server from the build directory.
- **Resource taxonomy (ADR 0011/0012/0017):** `ImageData` and `Texture` are
  already native-backed GC-finalized classes with `destroy()` and read-only
  size properties; the display-list runs frame-end GC.
- **Vendoring (ADR 0006):** pinned in-repo snapshots; `stb_image` is already
  vendored and linked into `capture.c`; the build never touches the network.

## Goals / Non-Goals

**Goals:**

- One pure-C provider that reads a directory or zip root identically on all
  four targets, and through which `main.js` itself is read.
- A synchronous `load*` layer whose only async moment is a single web boot-time
  zip fetch, before script evaluation.
- Promote the already-vendored image decoder and add the smallest viable zip
  reader, keeping the dependency delta to one new vendored library.

**Non-Goals:**

- glTF parsing (F6b adds `cgltf`), skins/animations (F6c), REPL (F6d).
- A script-visible async API, per-file remote fetch, or a manifest loader.
- Changing the GPU sink, display list, or resource taxonomy.

## Decisions

### D1 — A new `src/resource/` module, stdio-based, no platform ifdefs

The provider exposes an opaque root and `read(path, &size)`; the implementation
uses `FILE*` for both the root directory and the mounted zip. Emscripten maps
`stdio` onto MEMFS, so the *same* implementation serves desktop and web — the
only difference is how the bytes got into the filesystem before boot.

- **Rejected — per-platform providers:** duplicated logic and divergent path
  handling for no benefit given MEMFS.
- **Rejected — reading through Emscripten `FS` from JS only:** leaves the
  desktop binding without the provider and splits the read path in two.

### D2 — Zip reads via a vendored single-file miniz

`miniz` (MIT) provides `mz_zip_reader` in one amalgamated C file with no
external zlib, satisfying the pinned-snapshot policy and Emscripten builds.

- **Rejected — libzip / minizip-ng:** both need zlib and a CMake build,
  violating the single-snapshot vendoring fit.
- **Rejected — rolling our own inflate:** needless surface for a solved problem.

### D3 — Image decoding via the already-vendored `stb_image`

`stb_image` (v2.30) is already evaluated and vendored; F6a promotes it into
`efx_core` with the pre-existing 4-channel (RGBA8) request so decoded pixels
drop straight into `ImageData`. JPEG's missing alpha becomes opaque.

- **Rejected — libpng + libjpeg-turbo:** two more dependencies for formats stb
  already covers, contradicting the lightweight goal.

### D4 — Fetch once at boot; keep the script API synchronous

On the web, the asset zip is fetched and written into MEMFS **before** the
entry script is evaluated; after that, every `load*` is a synchronous C call.
This is the whole reason to fetch an archive rather than streaming individual
files: the API contract (already promised as synchronous in
`docs/js-api.md`) stays identical across targets.

- **Rejected — an async `load*` API:** would leak Promises into game scripts
  and diverge from the desktop contract.
- **Rejected — synchronous XHR:** deprecated, blocks the main thread, and is
  unavailable in workers.

### D5 — Host asset channel: `globalThis.__efx_assets`, `?assets=` fallback

The embedding page sets a URL before boot; `entry.js` consumes and deletes it
before evaluation, exactly like `__efx_main_js` (ADR 0030). A `?assets=` query
parameter is a driver fallback for the local harness servers. `main.js` stays
host-provided and independent of the asset archive.

- **Rejected — a script-visible `efx.load*Async`:** violates the no-browser-
  dependency and sync-contract goals.
- **Rejected — a build-time preload only:** cannot express a runtime-selected
  asset set, which is the point of fetching.

### D6 — Root state is threaded; `main.js` reads through the provider

`player.c` opens the root (dir or zip) and the entry script is read through the
provider on desktop; the web bridge exposes a root setter and a read path used
by `entry.js`. The `--script` root is the script's directory.

- **Rejected — resolving relative to CWD:** breaks when the player is launched
  from elsewhere and has no meaning for a zip root.
- **Rejected — an explicit `--root` flag on `--script`:** extra ceremony for
  the common case (tests keep assets next to the script).

### D7 — Resource exposure to scripts is unchanged in kind

`loadImage` returns the existing native-backed `ImageData` class; `loadTexture`
returns the existing `Texture` class; both keep deterministic `destroy()`,
idempotence, use-after-destroy throws, and frame-end GC accounting (ADR
0011/0012). `loadText` returns a plain GC-managed string. No new resource type
is introduced, so the `js-api` taxonomy is untouched.

### D8 — Both bindings wrap one C surface

`api.c` (quickjs) and `bridge.c` (web) each marshal JS values to the same
`src/resource/` calls, mirroring the existing render wrappers. This is the
intended duplication of the two-binding architecture; the provider logic
exists once.

### D9 — Boot blocks; no script-visible loading hook

The web boot is synchronous from the script's perspective: the fetch and mount
complete before `main.js` evaluation and before the frame loop starts, and no
loading hook or async resource API is exposed. A loading UI, if ever wanted,
belongs to the embedding page (which controls the asset URL), not the engine.

- **Rejected — a script-visible `efx.onLoading` hook:** reintroduces async into
  the script contract and duplicates what the host page already controls.

### D10 — `--script` accepts an explicit `--root` override

Script mode defaults to the script's directory but accepts `--root
<directory|archive>` to point loads at another root (e.g. a zip fixture),
which keeps test assets decoupled from script placement without a second
launch mode.

- **Rejected — directory-only default with no override:** forces test fixtures
  to sit next to scripts, and gives no way to exercise a zip root from a
  script test.

## Risks / Trade-offs

- **[Emscripten may not await an async `postRun`]** → Make `__efxBoot` an async
  function invoked fire-and-forget from a synchronous `postRun`; the runtime
  stays alive (`EXIT_RUNTIME=0`) until the loop starts. Prove this with a spike
  before the rest of F6a; fall back to `onRuntimeInitialized`/`noInitialRun`
  handshaking if needed.
- **[Zip buffered twice (JS `ArrayBuffer` + MEMFS)]** → Free the JS buffer
  after `FS.writeFile`; assets are PS2-era sized. Measured in the boot spike.
- **[Fetch failure / CORS / 404]** → Surface through the existing console
  error + non-zero exit contract before evaluating `main.js`; add a harness
  case.
- **[Provider path traversal]** → Normalize and reject parent-segment escapes;
  zip reads are exact-entry lookups.
- **[`vendor/stb` compiled into `efx_core` under `-Werror`]** → Keep the
  implementation TU isolated and/or use suppressible warnings, as `capture.c`
  already must.
- **[Async boot changes web lifecycle timing]** → Node harness and override
  test already assert from process exit; keep the URL path browser-only so the
  goldens, compare, and override paths are untouched.
- **[Golden re-baseline if output shifts]** → F6a adds a new golden only;
  existing goldens are byte-identical (no rendering-path change).

## Spike result (task 1.1)

An Emscripten spike on the verification server confirmed the async-boot
mechanic: a `postRun` callback that kicks off a `Promise` and starts the sokol
loop from its `.then` (after the synchronous call stack unwinds) builds and
runs to completion under Node (`-sEXIT_RUNTIME=0`), exit 0:
`main → postRun-exit → async-resolved → start-loop-called → frames → loop-done`.
So `postRun` need not be awaited; `__efxBoot` can be an async function invoked
fire-and-forget, with the loop started after the fetch/mount resolves. The
browser-specific confirmation (pinned headless Chrome, rAF/BeginFrame) is
folded into the task 4.2 harness case, which already must exercise a fetched
zip; no fallback (`onRuntimeInitialized`/`noInitialRun`) is required.

## Migration Plan

1. Spike async boot on Emscripten with a throwaway fetched blob; confirm the
   frame loop starts after the promise resolves. Gate the rest on the result.2. Vendor miniz, promote stb into `efx_core`, add `src/resource/` with the
   directory backend; wire `loadText`/`loadImage` through both bindings and
   add ctest coverage reading next to the script.
3. Add the zip backend and the `player <root.zip>` launch path; extend the
   desktop smoke tests with a zip fixture.
4. Add the web boot mount and host channel; add a harness case that fetches a
   fixture zip over the local server, then a golden that samples a decoded PNG.
5. Update `docs/js-api.md`, `gallery/src/api/efx.d.ts`, the new ADR
   (`docs/decisions/0031-*`), and the ADR 0016/0030 amendments.

Rollback: the feature is additive; reverting the change leaves F5 behavior
intact, since no existing render path or golden changes.
